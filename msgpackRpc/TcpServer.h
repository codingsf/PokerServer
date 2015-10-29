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

	TcpServer(boost::asio::io_service& ios, short port);
	TcpServer(boost::asio::io_service& ios, const boost::asio::ip::tcp::endpoint& endpoint);
	virtual ~TcpServer();

	void start();
	void stop();

	void setDispatcher(std::shared_ptr<msgpack::rpc::asio::dispatcher> disp);

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

	boost::asio::io_service& _ioService;
	boost::asio::ip::tcp::acceptor _acceptor;
	std::shared_ptr<msgpack::rpc::asio::dispatcher> _dispatcher;
};

}}}
