#pragma once
#include <memory>
#include <boost/asio.hpp>
#include "TcpConnection.h"

namespace msgpack {
namespace rpc {

class TcpSession;
class Dispatcher;
class TcpClient
{
public:
	TcpClient(boost::asio::io_service& io_service);
	virtual ~TcpClient();

	void close();
	void setDispatcher(std::shared_ptr<Dispatcher> disp);
	void asyncConnect(const boost::asio::ip::tcp::endpoint& endpoint);

	/// asyncCall without callback
	template<typename... TArgs>
	std::shared_ptr<AsyncCallCtx> asyncCall(const std::string& method, TArgs... args);

	/// asyncCall with callback
	template<typename... TArgs>
	std::shared_ptr<AsyncCallCtx> asyncCall(OnAsyncCall callback, const std::string& method, TArgs... args);

	/// syncCall without return
	template<typename... TArgs>
	void syncCall(const std::string& method, TArgs... args);

	/// syncCall with return type R
	template<typename R, typename... TArgs>
	R& syncCall(R* value, const std::string& method, TArgs... args);

private:
	boost::asio::io_service& _ioService;

	std::shared_ptr<TcpSession> _session;

	std::shared_ptr<Dispatcher> _dispatcher;
};

template<typename... TArgs>
inline std::shared_ptr<AsyncCallCtx> TcpClient::asyncCall(const std::string& method, TArgs... args)
{
	return _session->asyncCall(method, args...);
}

template<typename... TArgs>
inline std::shared_ptr<AsyncCallCtx> TcpClient::asyncCall(OnAsyncCall callback, const std::string& method, TArgs... args)
{
	return _session->asyncCall(callback, method, args...);
}

template<typename... TArgs>
inline void TcpClient::syncCall(const std::string& method, TArgs... args)
{
	return _session->syncCall(method, args...);
}

template<typename R, typename... TArgs>
inline R& TcpClient::syncCall(R *value, const std::string& method, TArgs... args)
{
	return _session->syncCall(value, method, args...);
}

} } // namespace msgpack::rpc