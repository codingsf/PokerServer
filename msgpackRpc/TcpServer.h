#pragma once
#include "TcpSession.h"

namespace msgpack {
namespace rpc {
namespace asio {

class TcpSession;
class TcpServer
{
public:
	static std::set<std::shared_ptr<TcpSession>> _sessions;

	TcpServer(boost::asio::io_service &io_service, short port);
	TcpServer(boost::asio::io_service &io_service, const boost::asio::ip::tcp::endpoint& endpoint);
	virtual ~TcpServer();

	void start();
	void stop();

	template<typename R, typename A1, typename A2>
	void add_handler(const std::string &method, R(*handler)(A1, A2))
	{
		_dispatcher->add_handler(method, handler);
	}

	// asyncCall with callback
	template<typename... A1>
	std::shared_ptr<AsyncCallCtx> asyncCall(std::shared_ptr<TcpSession> pSession, OnAsyncCall callback, const std::string &method, A1... a1)
	{
		return pSession->asyncCall(callback, method, a1...)
	}

	// asyncCall without callback
	template<typename... A1>
	std::shared_ptr<AsyncCallCtx> asyncCall(std::shared_ptr<TcpSession> pSession, const std::string &method, A1... a1)
	{
		return pSession->asyncCall(method, a1...);
	}

private:
	void startAccept();

	boost::asio::io_service &_ioService;
	boost::asio::ip::tcp::acceptor _acceptor;
	std::shared_ptr<msgpack::rpc::asio::dispatcher> _dispatcher;
};

}}}
