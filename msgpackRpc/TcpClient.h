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

	TcpClient(boost::asio::io_service& io_service);
	virtual ~TcpClient();

	void close();
	void asyncConnect(const boost::asio::ip::tcp::endpoint& endpoint);

	/// register a function without return
	template<typename... Args>
	void registerFunc(const std::string& method, void(*handler)(Args... args));

	/// register a function with return type R
	template<typename R, typename... Args>
	void registerFunc(const std::string& method, R(*handler)(Args... args));

	/// asyncCall without callback
	template<typename... Args>
	std::shared_ptr<AsyncCallCtx> asyncCall(const std::string& method, Args... args);

	/// asyncCall with callback
	template<typename... Args>
	std::shared_ptr<AsyncCallCtx> asyncCall(OnAsyncCall callback, const std::string& method, Args... args);

	/// syncCall without return
	template<typename... Args>
	void syncCall(const std::string& method, Args... args);

	/// syncCall with return type R
	template<typename R, typename... Args>
	R& syncCall(R* value, const std::string& method, Args... args);

private:
	boost::asio::io_service& _ioService;

	std::shared_ptr<msgpack::rpc::asio::dispatcher> _dispatcher;
};

inline void TcpClient::close()
{
	_session->close();
}

template<typename... Args>
inline void TcpClient::registerFunc(const std::string& method, void(*handler)(Args... args))
{
	_dispatcher->add_handler(method, handler);
}

template<typename R, typename... Args>
inline void TcpClient::registerFunc(const std::string& method, R(*handler)(Args... args))
{
	_dispatcher->add_handler(method, handler);
}

template<typename... Args>
inline std::shared_ptr<AsyncCallCtx> TcpClient::asyncCall(const std::string& method, Args... args)
{
	return _session->asyncCall(method, args...);
}

template<typename... Args>
inline std::shared_ptr<AsyncCallCtx> TcpClient::asyncCall(OnAsyncCall callback, const std::string& method, Args... args)
{
	return _session->asyncCall(callback, method, args...);
}

template<typename... Args>
inline void TcpClient::syncCall(const std::string& method, Args... args)
{
	return _session->syncCall(method, args...);
}

template<typename R, typename... Args>
inline R& TcpClient::syncCall(R *value, const std::string& method, Args... args)
{
	return _session->syncCall(value, method, args...);
}

} } } // namespace msgpack::rpc
