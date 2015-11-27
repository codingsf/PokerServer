#pragma once
#include <boost/pool/pool_alloc.hpp>
#include "Asio.h"
#include <boost/array.hpp>
#include "BufferManager.h"
#include "Exception.h"
#include <atomic> 

namespace msgpack {
namespace rpc {

static const uint32_t MSG_BUF_LENGTH = 512;

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
		// args
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

typedef std::function<void(const boost::system::error_code& error, boost::exception_ptr pExcept)> NetErrorHandler;
typedef std::function<void(ConnectionStatus)> ConnectionHandler;

class TcpConnection : public std::enable_shared_from_this<TcpConnection>
{
public:
	typedef std::function<void(msgpack::unpacked)> ProcessMsg;

	TcpConnection(boost::asio::io_service& io_service);
	TcpConnection(boost::asio::ip::tcp::socket);

	virtual ~TcpConnection();

	void asyncConnect(const boost::asio::ip::tcp::endpoint& endpoint);
	void handleConnect(const boost::system::error_code& error);

	void beginReadSome();	/// 一步或二步读
	void continueRead(std::shared_ptr<ArrayBuffer> bufPtr, uint32_t bytesRead, uint32_t bytesMore);

	void asyncWrite(std::shared_ptr<msgpack::sbuffer> msg);
	uint32_t pendingWrites();

	void close();
	boost::asio::ip::tcp::socket& getSocket();
	boost::promise<bool>& getConnectPromise();

	ConnectionStatus getConnectionStatus() const;

	void setProcessMsgHandler(ProcessMsg&& handler);
	void setConnectionHandler(ConnectionHandler&& handler);
	void setNetErrorHandler(NetErrorHandler&& handler);	// 传右值

private:
	void handleNetError(const boost::system::error_code& error, boost::exception_ptr pExcept);
	void handleConnectError(const boost::system::error_code& error);
	void handleReadError(const boost::system::error_code& error, size_t bytesRead);
	void handleWriteError(const boost::system::error_code& error, size_t bytesWrite);

	boost::asio::ip::tcp::socket _socket;
	boost::asio::ip::tcp::endpoint _peerAddr;

	ConnectionStatus _connectionStatus;
	boost::promise<bool> _promConn;

	std::atomic<uint32_t> _pendingWrite{0};

	ProcessMsg _processMsg;
	ConnectionHandler _connectionHandler;
	NetErrorHandler _netErrorHandler;

	//std::array<char, MSG_BUF_LENGTH> _buf;
	//std::vector<char, boost::pool_allocator<char> > _buf;		// 内存泄漏
	//std::vector<char, boost::fast_pool_allocator<char> > _buf;// 内存泄漏
	std::vector<char> _buf;
};

inline uint32_t TcpConnection::pendingWrites()
{
	return _pendingWrite;
}

inline boost::asio::ip::tcp::socket& TcpConnection::getSocket()
{
	return _socket;
}

inline boost::promise<bool>& TcpConnection::getConnectPromise()
{
	return _promConn;
}

inline void TcpConnection::setProcessMsgHandler(ProcessMsg&& handler)
{
	_processMsg = handler;
}

inline void TcpConnection::setConnectionHandler(ConnectionHandler&& handler)
{
	_connectionHandler = handler;
}

inline void TcpConnection::setNetErrorHandler(NetErrorHandler&& handler)
{
	_netErrorHandler = handler;
}

} }