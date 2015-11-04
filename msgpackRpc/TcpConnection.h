#pragma once
#include <boost/pool/pool_alloc.hpp>
#include "Asio.h"
#include <boost/array.hpp>

namespace msgpack {
namespace rpc {

static uint32_t MSG_BUF_LENGTH = 512;
static uint32_t MAX_MSG_LENGTH = 32 * 1024;

struct func_call_error : public std::runtime_error
{
	func_call_error(const std::string &msg) : std::runtime_error(msg) { }
};

class client_error : public std::runtime_error
{
public:
	client_error(const std::string &msg) : std::runtime_error(msg) { }
};


inline std::shared_ptr<msgpack::sbuffer> error_notify(const std::string &msg)
{
	// notify type
	MsgNotify<std::string, std::string> notify(
		// method
		"error_notify",
		// params
		msg
		);
	// result
	auto sbuf = std::make_shared<msgpack::sbuffer>();
	msgpack::pack(*sbuf, notify);
	return sbuf;
}

enum ConnectionStatus
{
	connection_none,
	connection_connecting,
	connection_connected,
	connection_error,
};

typedef std::function<void(boost::system::error_code error)> NetErrorHandler;
typedef std::function<void(ConnectionStatus)> ConnectionHandler;

class TcpConnection : public std::enable_shared_from_this<TcpConnection>
{
public:
	typedef std::function<void(const object &, std::shared_ptr<TcpConnection>)> MsgHandler;

	TcpConnection(boost::asio::io_service& io_service);
	TcpConnection(boost::asio::ip::tcp::socket);

	virtual ~TcpConnection();

	void asyncConnect(const boost::asio::ip::tcp::endpoint& endpoint);
	void handleConnect(const boost::system::error_code& error);

	void asyncReadSome();

	void asyncWrite(std::shared_ptr<msgpack::sbuffer> msg);

	void asyncRead();

	void close();

	ConnectionStatus getConnectionStatus() const;

	void setMsgHandler(MsgHandler&& handler);
	void setConnectionHandler(const ConnectionHandler& handler);
	void setNetErrorHandler(const NetErrorHandler& handler);// 应该传引用吗？

private:
	void setConnectionStatus(ConnectionStatus status);

	boost::asio::ip::tcp::socket _socket;

	ConnectionStatus _connectionStatus;

	MsgHandler _msgHandler;
	ConnectionHandler _connectionHandler;
	NetErrorHandler _netErrorHandler;
	unpacker _unpacker;

	uint32_t _msgLenth;
	std::vector<char, boost::fast_pool_allocator<char> > _msgBody;
};

inline void TcpConnection::setMsgHandler(MsgHandler&& handler)
{
	_msgHandler = handler;
}

inline void TcpConnection::setConnectionHandler(const ConnectionHandler& handler)
{
	_connectionHandler = handler;
}

inline void TcpConnection::setNetErrorHandler(const NetErrorHandler& handler)
{
	_netErrorHandler = handler;
}

} }