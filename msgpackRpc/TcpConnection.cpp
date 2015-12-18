#include "TcpConnection.h"
#include "boost/format.hpp"

namespace msgpack {
namespace rpc {

using boost::asio::ip::tcp;
const static int SESSION_TIMEOUT = 10;	// 15 second

TcpConnection::TcpConnection(boost::asio::io_service& io_service) :
	_socket(io_service),
	_deadline(io_service),
	_buf(MSG_BUF_LENGTH),
	_connectionStatus(connection_none)
{
}

TcpConnection::TcpConnection(tcp::socket socket):
	_socket(std::move(socket)),
	_deadline(socket.get_io_service()),
	_buf(MSG_BUF_LENGTH),
	_connectionStatus(connection_none)
{
	if (_socket.is_open())
	{
		_connectionStatus = connection_connected;
		boost::system::error_code ec;
		_peerAddr = _socket.remote_endpoint(ec);
	}
}

TcpConnection::~TcpConnection()
{
}

ConnectionStatus TcpConnection::getConnectionStatus() const
{
	return _connectionStatus;
}

void TcpConnection::handleConnect(const boost::system::error_code& error)
{
	if (error)
	{
		_promConn.set_value(false);
		handleConnectError(error);
	}
	else
	{
		_promConn.set_value(true);
		_connectionStatus = connection_connected;

		boost::system::error_code ec;
		_peerAddr = _socket.remote_endpoint(ec);

		_deadline.expires_at(boost::posix_time::pos_infin);
		beginReadSome();
		_deadline.async_wait(
			boost::bind(&TcpConnection::checkTimeout, shared_from_this(), &_deadline));
	}

	if (_connectionHandler)
		_connectionHandler(error ? connection_error : connection_connected);
}

void TcpConnection::asyncConnect(const boost::asio::ip::tcp::endpoint& endpoint)
{
	_connectionStatus = connection_connecting;

	auto weak = std::weak_ptr<TcpConnection>(shared_from_this());
	auto handler = [weak](const boost::system::error_code& error)
	{
		auto shared = weak.lock();
		if (shared)
			shared->handleConnect(error);
	};
	_socket.async_connect(endpoint, handler);
}

void TcpConnection::continueRead(std::shared_ptr<ArrayBuffer> bufPtr, uint32_t bytesRead, uint32_t bytesMore)
{
	auto shared = shared_from_this();
	boost::asio::async_read(_socket,
		boost::asio::buffer(bufPtr->data() + bytesRead, bytesMore),
		[this, shared, bufPtr](const boost::system::error_code& error, size_t bytesRead)
		{
			if (error)
				handleReadError(error, bytesRead);
			else
			{
				try
				{
					msgpack::unpacked upk = msgpack::unpack(bufPtr->data(), bufPtr->size());
					BufferManager::instance()->freeBuffer(bufPtr);
					_processMsg(upk);
					beginReadSome();
				}
				catch (unpack_error& error)
				{
					// 简单处理，上一步asyncWrite还没完成socket就close了，应该专门在session类里加一发送异常消息的call
					asyncWrite(error_notify(error.what()));
					_socket.get_io_service().post(boost::bind(&TcpConnection::close, this));
					return;
				}
				catch (const boost::exception& error)
				{
					auto no = boost::get_error_info<err_no>(error);
					auto str = boost::get_error_info<err_str>(error);
					asyncWrite(error_notify(str ? *str : ""));
					_socket.get_io_service().post(boost::bind(&TcpConnection::close, this));
					return;
				}
				catch (...)
				{
					asyncWrite(error_notify("unknown error"));
					_socket.get_io_service().post(boost::bind(&TcpConnection::close, this));
					return;
				}
			} // else
		}); // lambda
}

void TcpConnection::beginReadSome()
{
	_deadline.expires_from_now(boost::posix_time::seconds(SESSION_TIMEOUT));

	auto shared = shared_from_this();	// 没有读完前防止TcpConnection析构，而使用无效buffer
	boost::asio::async_read(_socket,
		boost::asio::buffer(_buf),
		boost::asio::transfer_at_least(4), 
		[this, shared](const boost::system::error_code& error, size_t bytesRead)
		{
			if (error)
				handleReadError(error, bytesRead);
			else
			{
				try
				{
					uint32_t offset = 0;
					do
					{
						if (bytesRead - offset < 4)
							if (MSG_BUF_LENGTH != bytesRead)
								throw Not4BytesHeadException() <<
									err_no(Not4BytesHead) <<
									err_str((boost::format("Not4BytesHead: %d bytes") % (bytesRead - offset)).str());
							else
							{
								int bytes = bytesRead - offset;
								std::memcpy(_buf.data(), _buf.data() + offset, bytes);
								boost::asio::async_read(_socket, boost::asio::buffer(_buf.data() + bytes, 4 - bytes),
									[this, shared](const boost::system::error_code& error, size_t bytesRead)
									{
										if (error)
											handleReadError(error, bytesRead);
										else
										{
											uint32_t length = ntohl(*((uint32_t*)(_buf.data())));
											boost::asio::async_read(_socket, boost::asio::buffer(_buf.data(), length),
												[this, shared](const boost::system::error_code& error, size_t bytesRead)
											{
												if (error)
													handleReadError(error, bytesRead);
												else
												{
													try
													{
														msgpack::unpacked upk = msgpack::unpack(_buf.data(), bytesRead);
														_processMsg(upk);
														beginReadSome();
													}
													catch (const boost::exception& error)
													{
														auto no = boost::get_error_info<err_no>(error);
														auto str = boost::get_error_info<err_str>(error);
														asyncWrite(error_notify(str ? *str : ""));
														_socket.get_io_service().post(boost::bind(&TcpConnection::close, this));
														return;
													}
													catch (const std::exception& error)
													{
														asyncWrite(error_notify(error.what()));
														_socket.get_io_service().post(boost::bind(&TcpConnection::close, this));
														return;
													}
													catch (...)
													{
														asyncWrite(error_notify("unknown error"));
														_socket.get_io_service().post(boost::bind(&TcpConnection::close, this));
														return;
													}
												}
											});
										}
								});
								return;
							}

						uint32_t length = ntohl(*((uint32_t*)(_buf.data() + offset)));	// 下一条消息长度
						if (offset == 0 && length == 0)
							break;
						if (length > MAX_MSG_LENGTH)
							throw MsgTooLongException() <<
								err_no(MsgTooLong) <<
								err_str((boost::format("MsgTooLong: %d bytes") % length).str());

						offset += sizeof(uint32_t);		// 下一条消息Body的起始地址
						if (bytesRead - offset < length)// buf收到的字节数 < 消息长度
						{
							auto bufPtr = BufferManager::instance()->getBuffer();
							std::memcpy(bufPtr->data(), _buf.data() + offset, bytesRead - offset);
							continueRead(bufPtr, bytesRead - offset, length - (bytesRead - offset));
							return;
						}
						else
						{
							msgpack::unpacked upk = msgpack::unpack(_buf.data(), bytesRead, offset); // ???
							_processMsg(upk);
						}
					} while (offset < bytesRead);

					beginReadSome();
				}
				catch (const unpack_error& error)
				{
					// 简单处理，上一步asyncWrite还没完成socket就close了，应该专门在session类里加一发送异常消息的call
					asyncWrite(error_notify(error.what()));
					_socket.get_io_service().post(boost::bind(&TcpConnection::close, this));
					return;
				}
				catch (const boost::exception& error)
				{
					auto no = boost::get_error_info<err_no>(error);
					auto str = boost::get_error_info<err_str>(error);
					asyncWrite(error_notify(str ? *str : ""));
					_socket.get_io_service().post(boost::bind(&TcpConnection::close, this));
					return;
				}
				catch (const std::exception& error)
				{
					asyncWrite(error_notify(error.what()));
					_socket.get_io_service().post(boost::bind(&TcpConnection::close, this));
					return;
				}
				catch (...)
				{
					asyncWrite(error_notify("unknown error"));
					_socket.get_io_service().post(boost::bind(&TcpConnection::close, this));
					return;
				}
			} // else
		}); // lambda
}

void TcpConnection::asyncWrite(std::shared_ptr<msgpack::sbuffer> msg)
{
	auto len = std::make_shared<uint32_t>(htonl(msg->size()));

	std::vector<boost::asio::const_buffer> bufs;
	bufs.push_back(boost::asio::buffer(len.get(), sizeof(uint32_t)));
	bufs.push_back(boost::asio::buffer(msg->data(), msg->size()));

	_pendingWrite++;
	auto self = shared_from_this();
	boost::asio::async_write(_socket, bufs,
		[this, self, len, msg](const boost::system::error_code& error, size_t bytesWrite)
		{
			_pendingWrite--;
			if (error)
				handleWriteError(error, bytesWrite);
		});
}

void TcpConnection::ping()
{
	auto len = std::make_shared<uint32_t>(0);
	auto self = shared_from_this();

	boost::asio::async_write(_socket,
		boost::asio::buffer(len.get(), sizeof(uint32_t)),
		[this, self, len](const boost::system::error_code& error, size_t bytesWrite)
		{
			if (error)
				handleWriteError(error, bytesWrite);
		});
}

void TcpConnection::checkTimeout(boost::asio::deadline_timer* deadline)
{
	if (!_socket.is_open())
		return;

	if (deadline->expires_at() <= boost::asio::deadline_timer::traits_type::now())
	{
		boost::system::error_code ignored_ec;
		_socket.close(ignored_ec);
		deadline->cancel();
	}
	else
	{
		deadline->async_wait(
			boost::bind(&TcpConnection::checkTimeout, shared_from_this(), deadline));
	}
}

void TcpConnection::close()
{
	_connectionStatus = connection_none;
	
	boost::system::error_code ec;
	_socket.shutdown(boost::asio::socket_base::shutdown_both, ec);
	_socket.close(ec);
}

void TcpConnection::handleNetError(const boost::system::error_code& error, boost::exception_ptr pExcept)
{
	try
	{
		boost::system::error_code ec;
		_socket.close(ec);
		_connectionStatus = connection_error;

		if (_netErrorHandler)
			_netErrorHandler(error, pExcept);
	}
	catch (const boost::exception& error)
	{
		auto no = boost::get_error_info<err_no>(error);
		auto str = boost::get_error_info<err_str>(error);
		// log
		return;
	}
	catch (const std::exception& error)
	{
		// log
		return;
	}
	catch (...)
	{
		// log
		return;
	}
}

void TcpConnection::handleConnectError(const boost::system::error_code& error)
{
	handleNetError(error, boost::copy_exception(
		ConnectionException() <<
		err_no(error.value()) <<
		err_str(_peerAddr.address().to_string() + error.message()) <<
		boost::throw_function(BOOST_THROW_EXCEPTION_CURRENT_FUNCTION)
		));
}

void TcpConnection::handleReadError(const boost::system::error_code& error, size_t bytesRead)
{
	handleNetError(error, boost::copy_exception(
		NetReadException() <<
		err_no(error.value()) <<
		err_str(_peerAddr.address().to_string() + error.message()) <<
		boost::throw_function(BOOST_THROW_EXCEPTION_CURRENT_FUNCTION)
		));
}

void TcpConnection::handleWriteError(const boost::system::error_code& error, size_t bytesWrite)
{
	handleNetError(error, boost::copy_exception(
		NetWriteException() <<
		err_no(error.value()) <<
		err_str(_peerAddr.address().to_string() + error.message()) <<
		boost::throw_function(BOOST_THROW_EXCEPTION_CURRENT_FUNCTION)/* <<
		boost::throw_file(__FILE__) <<
		boost::throw_line((int)__LINE__)*/
		));
}

} }