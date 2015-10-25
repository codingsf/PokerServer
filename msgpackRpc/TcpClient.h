#pragma once
#include "TcpSession.h"

namespace msgpack {
namespace rpc {
namespace asio {

class TcpSession;
class TcpClient
{
public:
	std::shared_ptr<TcpSession> _session;

	TcpClient(boost::asio::io_service &io_service);
	virtual ~TcpClient();

	void close() { _session->close(); }
	void asyncConnect(const boost::asio::ip::tcp::endpoint &endpoint);


	template<typename R, typename A1, typename A2>
	void add_handler(const std::string &method, R(*handler)(A1, A2))
	{
		_dispatcher->add_handler(method, handler);
	}

	// asyncCall with callback
	template<typename... A1>
	std::shared_ptr<AsyncCallCtx> asyncCall(OnAsyncCall callback, const std::string &method, A1... a1)
	{
		return _session->asyncCall(callback, method, a1...);
	}

	// asyncCall without callback
	template<typename... A1>
	std::shared_ptr<AsyncCallCtx> asyncCall(const std::string &method, A1... a1)
	{
		return _session->asyncCall(method, a1...);
	}

	// syncCall
	template<typename R, typename... A1> 	R &syncCall(R *value, const std::string &method, A1... a1)
	{
		return _session->syncCall(value, method, a1...);
	} 
private:


	boost::asio::io_service &_ioService;

	std::shared_ptr<msgpack::rpc::asio::dispatcher> _dispatcher;
};

}}}
