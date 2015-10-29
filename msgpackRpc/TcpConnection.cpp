#include "TcpConnection.h"

namespace msgpack {
namespace rpc {

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

TcpConnection::TcpConnection(boost::asio::io_service& io_service, on_read_t on_read,
							connection_callback_t connection_callback, error_handler_t error_handler):
	_ioService(io_service),
	_onRead(on_read),
	m_connection_callback(connection_callback),
	m_error_handler(error_handler),
	_unpacker(),
	_socket(io_service),
	m_connection_status(connection_none)
{
}

TcpConnection::~TcpConnection()
{
}

boost::asio::ip::tcp::socket& TcpConnection::getSocket()
{
	return _socket;
}

ConnectionStatus msgpack::rpc::TcpConnection::get_connection_status() const
{
	return m_connection_status;
}

void TcpConnection::asyncConnect(const boost::asio::ip::tcp::endpoint &endpoint)
{
	set_connection_status(connection_connecting);
	auto self = shared_from_this();
	_socket.async_connect(endpoint, [this, self](const boost::system::error_code &error)
	{
		if (error)
		{
			if (m_error_handler)
				m_error_handler(error);
			set_connection_status(connection_error);
		}
		else
		{
			set_connection_status(connection_connected);
			asyncRead();
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
				if (m_error_handler)
					m_error_handler(error);
				set_connection_status(connection_none);
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
						if (_onRead)
							_onRead(result.get(), self);	// result.get()����_unpacker��buffer��ע�����õ���Ч��
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
				//	*pac = unpacker();	// �ͷŲ���������buffer			// pac->buffer�����ͷţ��յ�������append��buffer��
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
				if (m_error_handler)
					m_error_handler(error);
				set_connection_status(connection_error);
			}
		});
}

void TcpConnection::start()
{
	set_connection_status(connection_connected);
	asyncRead();
}

void msgpack::rpc::TcpConnection::close()
{
	set_connection_status(connection_none);
}

void TcpConnection::set_connection_status(ConnectionStatus status)
{
	if (m_connection_status == status)
		return;

	if (status == connection_none || status == connection_error)
	{
		boost::system::error_code ec;
		_socket.shutdown(boost::asio::socket_base::shutdown_both, ec);
		if (!ec)
			_socket.close(ec);
	}

	m_connection_status = status;
	if (m_connection_callback)
		m_connection_callback(status);
}

} }