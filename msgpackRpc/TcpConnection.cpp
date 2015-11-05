#include "TcpConnection.h"

namespace msgpack {
namespace rpc {

using boost::asio::ip::tcp;

TcpConnection::TcpConnection(boost::asio::io_service& io_service) :
	_socket(io_service),
	_buf(MSG_BUF_LENGTH)
{
}

TcpConnection::TcpConnection(tcp::socket socket):
	_socket(std::move(socket)),
	_buf(MSG_BUF_LENGTH)
{
	if (socket.is_open())
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
	{
		_netErrorHandler(error);
		setConnectionStatus(connection_error);
	}
	else
	{
		_connectionStatus = connection_connected;
		asyncReadSome();
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

void TcpConnection::continueRead(std::shared_ptr<ArrayBuffer> pBuf, uint32_t bytesRead, uint32_t bytesMore)
{
	auto shared = shared_from_this();
	auto handler = [shared](const boost::system::error_code& error, size_t bytesRead)
	{
		shared->handleContRead(error, bytesRead);
	};

	boost::asio::async_read(_socket,
		boost::asio::buffer(_chunk->data() + bytesRead, bytesMore),
		handler);
}

void TcpConnection::handleContRead(const boost::system::error_code& error, size_t bytesRead)
{
	if (error)
	{
		_netErrorHandler(error);
		setConnectionStatus(connection_none);
		return;
	}

	try
	{
		msgpack::unpacked upk = msgpack::unpack(_chunk->data(), _chunk->size());
		BufferManager::instance()->freeBuffer(_chunk);
		_chunk.reset();
		_msgHandler(upk, shared_from_this());
		asyncReadSome();
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
}

void TcpConnection::asyncReadSome()
{
	auto shared = shared_from_this();
	auto handler = [shared](const boost::system::error_code& error, size_t bytesRead)
	{
		shared->handleReadSome(error, bytesRead);
	};

	boost::asio::async_read(_socket, boost::asio::buffer(_buf), boost::asio::transfer_at_least(4), handler);
}

void TcpConnection::handleReadSome(const boost::system::error_code& error, size_t bytesRead)
{
	if (error)
	{
		_netErrorHandler(error);
		setConnectionStatus(connection_none);
		return;
	}

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
				_chunk = BufferManager::instance()->getBuffer();
				std::memcpy(_chunk->data(), _buf.data() + offset, bytesRead - offset);
				continueRead(_chunk, bytesRead - offset, length - (bytesRead - offset));
				return;
			}
			else
			{
				msgpack::unpacked upk = msgpack::unpack(_buf.data(), bytesRead, offset);
				_msgHandler(upk, shared_from_this());
			}
		}
		while (offset < bytesRead);

		asyncReadSome();
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
}

void TcpConnection::asyncWrite(std::shared_ptr<msgpack::sbuffer> msg)
{
	_sendLenth = htonl(msg->size());

	std::vector<boost::asio::const_buffer> bufs;
	bufs.push_back(boost::asio::buffer(&_sendLenth, sizeof(_sendLenth)));
	bufs.push_back(boost::asio::buffer(msg->data(), msg->size()));

	auto self = shared_from_this();
	_socket.async_write_some(bufs,
		[this, self, msg](const boost::system::error_code& error, size_t bytes_transferred)
		{
			if (error)
			{
				setConnectionStatus(connection_error);
				_netErrorHandler(error);
			}
		});
}

void TcpConnection::asyncRead()
{
}

void TcpConnection::close()
{
	_connectionStatus = connection_none;
	
	boost::system::error_code ec;
	_socket.close(ec);
}

void TcpConnection::setConnectionStatus(ConnectionStatus status)
{
	if (_connectionStatus == status)
		return;

	_connectionStatus = status;
	if (_connectionHandler)
		_connectionHandler(status);
}

} }