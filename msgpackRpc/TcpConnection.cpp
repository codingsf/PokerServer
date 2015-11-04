#include "TcpConnection.h"

namespace msgpack {
namespace rpc {

using boost::asio::ip::tcp;

TcpConnection::TcpConnection(boost::asio::io_service& io_service) :
	_socket(io_service),
	_unpacker(),
	_msgLenth(0),
	_msgBody(512)
{
}

TcpConnection::TcpConnection(tcp::socket socket):
	_socket(std::move(socket)),
	_unpacker()
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

void TcpConnection::asyncReadSome()
{
	std::array<boost::asio::mutable_buffer, 2> bufs =
	{
		boost::asio::buffer(&_msgLenth, sizeof(_msgLenth)),
		boost::asio::buffer(_msgBody)
	};

	auto self = shared_from_this();
	_socket.async_read_some(bufs,
		[this, self](const boost::system::error_code& error, size_t bytesRead)
		{
			if (error)
			{
				_netErrorHandler(error);
				setConnectionStatus(connection_none);
				return;
			}

			try
			{
				_msgLenth = ntohl(_msgLenth);
				if (_msgLenth > MAX_MSG_LENGTH)
					throw std::runtime_error("消息超长");

				if (_msgLenth < bytesRead - sizeof(_msgLenth))
				{
					if (_msgLenth > _msgBody.capacity())
						_msgBody.reserve(MSG_BUF_LENGTH * (_msgLenth / MSG_BUF_LENGTH + 1));
					boost::asio::async_read(_socket, boost::asio::buffer(_msgBody.data() + _msgLenth, _msgBody.size() - _msgLenth),
						[this](const boost::system::error_code& error, std::size_t /*length*/)
					{
						if (error)
						{
							_netErrorHandler(error);
							setConnectionStatus(connection_none);
							return;
						}

						// _unpacker
						// _msgHandler

						_msgBody.resize(MSG_BUF_LENGTH);
						asyncReadSome();
					});
				}

				_unpacker.buffer_consumed(bytesRead);
				unpacked result;
				while (_unpacker.next(&result))
				{
					_msgHandler(result.get(), self);	// result.get()引用_unpacker的buffer，注意引用的有效性
				}

				// read loop
				if (_unpacker.buffer_capacity() < 128)
					_unpacker = unpacker();	// 收到的数据append在buffer中, 而pac->buffer不会释放
				asyncReadSome();
			}
			catch (unpack_error& error)
			{
				asyncWrite(error_notify(error.what()));
			}
			catch (...)
			{
				asyncWrite(error_notify("unknown error"));
			}

			// 简单处理，上一步asyncWrite还没完成socket就close了，应该专门在session类里加一发送异常消息的call
			_socket.get_io_service().post(boost::bind(&TcpConnection::close, this));
			return;
	});
}

void TcpConnection::asyncWrite(std::shared_ptr<msgpack::sbuffer> msg)
{
	auto head = std::make_shared<uint32_t>();
	*head = htonl(msg->size());

	std::vector<boost::asio::const_buffer> bufs;
	bufs.push_back(boost::asio::buffer(head.get(), sizeof(uint32_t)));
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