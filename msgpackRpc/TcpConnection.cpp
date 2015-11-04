#include "TcpConnection.h"

namespace msgpack {
namespace rpc {

using boost::asio::ip::tcp;

TcpConnection::TcpConnection(boost::asio::io_service& io_service) :
	_socket(io_service),
	_offset(0),
	_buf(512)
{
}

TcpConnection::TcpConnection(tcp::socket socket):
	_socket(std::move(socket)),
	_offset(0),
	_buf(512)
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

void TcpConnection::continueRead(uint32_t bytesMore)
{
	if (bytesMore > _buf.capacity() - _buf.size())
		_buf.reserve(MSG_BUF_LENGTH * (bytesMore / MSG_BUF_LENGTH + 1));

	auto weak = std::weak_ptr<TcpConnection>(shared_from_this());
	auto handler = [weak](const boost::system::error_code& error, size_t bytesRead)
	{
		auto shared = weak.lock();
		if (shared)
			shared->handleContRead(error, bytesRead);
	};

	boost::asio::async_read(_socket,
		boost::asio::buffer(_buf.data() + _offset, bytesMore),
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

	uint32_t length = ntohl(*((uint32_t*)(_buf.data() + _offset)));	// ��һ����Ϣ����
	_offset += sizeof(uint32_t); //������Ϣ����ʼ��
	msgpack::unpacked upk = msgpack::unpack(_buf.data() + _offset, _buf.size() - _offset, _offset);
	_msgHandler(upk, shared_from_this());

	if (_buf.capacity() > MSG_BUF_LENGTH * 2)
		_buf.resize(MSG_BUF_LENGTH);
	asyncReadSome();
}

void TcpConnection::asyncReadSome()
{
	auto weak = std::weak_ptr<TcpConnection>(shared_from_this());
	auto handler = [weak](const boost::system::error_code& error, size_t bytesRead)
	{
		auto shared = weak.lock();
		if (shared)
			shared->handleReadSome(error, bytesRead);
	};

	_socket.async_read_some(boost::asio::buffer(_buf), handler);
}

void TcpConnection::handleReadSome(const boost::system::error_code& error, size_t bytesRead)
{
	if (error)
	{
		_netErrorHandler(error);
		setConnectionStatus(connection_none);
		return;
	}

	// �µ�һ�����������ݣ�ͷ4�ֽڱ�ʾ����
	if (bytesRead < sizeof(uint32_t))
	{
		asyncReadSome();
		return;
	}

	try
	{
		uint32_t _offset = 0, len = 0;
		std::vector<std::pair<char*, uint32_t>> items;
		do
		{
			char* pchar = _buf.data() + _offset;
			len = ntohl(*((uint32_t*)pchar));	// ��һ����Ϣ����
			pchar += sizeof(uint32_t);
			_offset += sizeof(uint32_t);
			items.push_back(std::make_pair(pchar, len));
		}
		while (_offset + len < bytesRead);

		for (auto& item : items)
		{
			if (item.second > MAX_MSG_LENGTH)
				throw std::runtime_error("��Ϣ����");

			if (_buf.size() - _offset < length)	// buf�յ����ֽ��� < ��Ϣ����
			{
				continueRead(111);
				return;
			}
			else
			{
				msgpack::unpacked upk = msgpack::unpack(_buf.data() + _offset, _buf.size() - _offset, _offset);
				_msgHandler(upk, shared_from_this());
			}
		}

		asyncReadSome();
	}
	catch (unpack_error& error)
	{
		// �򵥴�����һ��asyncWrite��û���socket��close�ˣ�Ӧ��ר����session�����һ�����쳣��Ϣ��call
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