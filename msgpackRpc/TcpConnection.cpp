#include "TcpConnection.h"

namespace msgpack {
namespace rpc {

using boost::asio::ip::tcp;

TcpConnection::TcpConnection(boost::asio::io_service& io_service) :
	_socket(io_service),
	_buf(MSG_BUF_LENGTH),
	_connectionStatus(connection_none)
{
}

TcpConnection::TcpConnection(tcp::socket socket):
	_socket(std::move(socket)),
	_buf(MSG_BUF_LENGTH),
	_connectionStatus(connection_none)
{
	if (_socket.is_open())
		_connectionStatus = connection_connected;
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
		handleConnectError(error);
	else
	{
		_connectionStatus = connection_connected;
		beginReadSome();
	}
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
					_processMsg(upk, shared_from_this());
					beginReadSome();
				}
				catch (unpack_error& error)
				{
					// 简单处理，上一步asyncWrite还没完成socket就close了，应该专门在session类里加一发送异常消息的call
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

void TcpConnection::beginReadSome()
{
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
							throw std::runtime_error("消息头不满4字节");
						uint32_t length = ntohl(*((uint32_t*)(_buf.data() + offset)));	// 下一条消息长度
						if (length > MAX_MSG_LENGTH)
							throw std::runtime_error("消息超长");
						offset += sizeof(uint32_t);		// 下一条消息长度的地址

						if (bytesRead - offset < length)// buf收到的字节数 < 消息长度
						{
							auto bufPtr = BufferManager::instance()->getBuffer();
							std::memcpy(bufPtr->data(), _buf.data() + offset, bytesRead - offset);
							continueRead(bufPtr, bytesRead - offset, length - (bytesRead - offset));
							return;
						}
						else
						{
							msgpack::unpacked upk = msgpack::unpack(_buf.data(), bytesRead, offset);
							_processMsg(upk, shared_from_this());
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
	_sendLenth = htonl(msg->size());

	std::vector<boost::asio::const_buffer> bufs;
	bufs.push_back(boost::asio::buffer(&_sendLenth, sizeof(_sendLenth)));
	bufs.push_back(boost::asio::buffer(msg->data(), msg->size()));

	auto self = shared_from_this();
	_socket.async_write_some(bufs,
		[this, self, msg](const boost::system::error_code& error, size_t bytesWrite)
		{
			if (error)
				handleWriteError(error, bytesWrite);
		});
}

void TcpConnection::close()
{
	_connectionStatus = connection_none;
	
	boost::system::error_code ec;
	_socket.close(ec);
}

void TcpConnection::handleNetError(const boost::system::error_code& error)
{
	_connectionStatus = connection_error;

	if (_connectionHandler)
		_connectionHandler(connection_error);
	if (_netErrorHandler)
		_netErrorHandler(error);
}

void TcpConnection::handleConnectError(const boost::system::error_code& error)
{
	handleNetError(error);
}

void TcpConnection::handleReadError(const boost::system::error_code& error, size_t bytesRead)
{
	handleNetError(error);
}

void TcpConnection::handleWriteError(const boost::system::error_code& error, size_t bytesWrite)
{
	handleNetError(error);
}

} }