#include "TcpConnection.h"

namespace msgpack {
namespace rpc {

using boost::asio::ip::tcp;

std::string AsyncCallCtx::string() const
{
	std::stringstream ss;
	ss << m_request << " = ";
	switch (m_status)
	{
	case AsyncCallCtx::STATUS_WAIT:
		ss << "?";
		break;
	case AsyncCallCtx::STATUS_RECEIVED:
		ss << m_result;
		break;
	case AsyncCallCtx::STATUS_ERROR:
		ss << "!";
		break;
	default:
		ss << "!?";
		break;
	}

	return ss.str();
}
 void AsyncCallCtx::setResult(const ::msgpack::object &result)
{
	if (m_status != STATUS_WAIT) {
		throw func_call_error("already finishded");
	}
	boost::mutex::scoped_lock lock(m_mutex);
	m_result = result;
	m_status = STATUS_RECEIVED;
	notify();
}

void AsyncCallCtx::setError(const ::msgpack::object &error)
{
	if (m_status != STATUS_WAIT) {
		throw func_call_error("already finishded");
	}
	boost::mutex::scoped_lock lock(m_mutex);
	typedef std::tuple<int, std::string> CodeWithMsg;
	CodeWithMsg codeWithMsg;
	error.convert(&codeWithMsg);
	m_status = STATUS_ERROR;
	m_error_code = static_cast<ServerSideError>(std::get<0>(codeWithMsg));
	m_error_msg = std::get<1>(codeWithMsg);
	notify();
}
 ServerSideError AsyncCallCtx::getErrorCode() const
{
	if (m_status != STATUS_ERROR)
		throw func_call_error("no error !");
	return m_error_code;
}

TcpConnection::TcpConnection(boost::asio::io_service& io_service):
	_socket(io_service),
	_unpacker()
{
}

TcpConnection::TcpConnection(tcp::socket socket):
	_socket(std::move(socket)),
	_unpacker()
{
}

TcpConnection::~TcpConnection()
{
}

ConnectionStatus TcpConnection::getConnectionStatus() const
{
	return _connectionStatus;
}

void TcpConnection::asyncConnect(const boost::asio::ip::tcp::endpoint &endpoint)
{
	setConnectionStatus(connection_connecting);
	auto self = shared_from_this();
	_socket.async_connect(endpoint, [this, self](const boost::system::error_code &error)
	{
		if (error)
		{
			if (_netErrorHandler)
				_netErrorHandler(error);
			setConnectionStatus(connection_error);
		}
		else
		{
			startRead();
		}
	});
}

void TcpConnection::asyncRead()
{
	auto self = shared_from_this();
	_socket.async_read_some(boost::asio::buffer(_unpacker.buffer(), _unpacker.buffer_capacity()),
		[this, self](const boost::system::error_code &error, size_t bytes_transferred)
		{
			if (error)
			{
				if (_netErrorHandler)
					_netErrorHandler(error);
				setConnectionStatus(connection_none);
				return;
			}
			else
			{
				_unpacker.buffer_consumed(bytes_transferred);
				try
				{
					unpacked result;
					while (_unpacker.next(&result))
					{
						_msgHandler(result.get(), self);	// result.get()引用_unpacker的buffer，注意引用的有效性
					}
				}
				catch (unpack_error &error)
				{
					auto msg = error_notify(error.what());
					asyncWrite(msg);
					// no more read
					// todo: close after write
					return;
				}
				catch (...)
				{
					auto msg = error_notify("unknown error");
					asyncWrite(msg);
					// no more read
					// todo: close after write
					return;
				}

				// read loop
				//if (pac->message_size() > 100)
				//	*pac = unpacker();	// 释放不断增长的buffer			// pac->buffer不会释放，收到的数据append在buffer中
				asyncRead();
			}
		});
}

void TcpConnection::asyncWrite(std::shared_ptr<msgpack::sbuffer> msg)
{
	auto self = shared_from_this();
	_socket.async_write_some(boost::asio::buffer(msg->data(), msg->size()), 		
		[this, self, msg](const boost::system::error_code& error, size_t bytes_transferred)
		{
			if (error)
			{
				if (_netErrorHandler)
					_netErrorHandler(error);
				setConnectionStatus(connection_error);
			}
		});
}

void TcpConnection::startRead()
{
	setConnectionStatus(connection_connected);
	asyncRead();
}

void TcpConnection::close()
{
	setConnectionStatus(connection_none);
}

void TcpConnection::setConnectionStatus(ConnectionStatus status)
{
	if (_connectionStatus == status)
		return;

	if (status == connection_none || status == connection_error)
	{
		boost::system::error_code ec;
		_socket.shutdown(boost::asio::socket_base::shutdown_both, ec);
		if (!ec)
			_socket.close(ec);
	}

	_connectionStatus = status;
	if (_connectionHandler)
		_connectionHandler(status);
}

} }