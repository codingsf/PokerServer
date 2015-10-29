#pragma once
#include "TcpConnection.h"
#include "Dispatcher.h"
#include <memory>	// enable_shared_from_this 

namespace msgpack {
namespace rpc {

class RequestFactory
{
public:
	template<typename... TArgs>
	MsgRequest<std::string, std::tuple<TArgs...>> create(const std::string& method, const TArgs... args);

private:
	uint32_t nextMsgid();
	uint32_t _nextMsgid = {1};
};

class TcpSession : public std::enable_shared_from_this<TcpSession>
{
public:
	TcpSession(boost::asio::io_service& ios, std::shared_ptr<Dispatcher> disp);

	void setDispatcher(std::shared_ptr<Dispatcher> disp);

	void begin(boost::asio::ip::tcp::socket socket);
	void asyncConnect(const boost::asio::ip::tcp::endpoint& endpoint);

	void stop();
	void close();

	bool isConnected();
	void netErrorHandler(boost::system::error_code &error);

	// asyncCall
	template<typename... TArgs>
	std::shared_ptr<AsyncCallCtx> asyncCall(const std::string& method, TArgs... args);

	template<typename... TArgs>
	std::shared_ptr<AsyncCallCtx> asyncCall(OnAsyncCall callback, const std::string& method, TArgs... args);

	// syncCall
	template<typename... TArgs>
	void syncCall(const std::string& method, TArgs... args);

	template<typename R, typename... TArgs>
	R& syncCall(R* value, const std::string& method, TArgs... args);

private:
	template<typename TArg>
	std::shared_ptr<AsyncCallCtx> asyncSend(const MsgRequest<std::string, TArg>& msgreq, OnAsyncCall callback = OnAsyncCall());

	void processMsg(const object& msg, std::shared_ptr<TcpConnection> TcpConnection);

	boost::asio::io_service& _ioService;
	RequestFactory _reqFactory;

	std::shared_ptr<TcpConnection> _connection;
	std::map<uint32_t, std::shared_ptr<AsyncCallCtx>> _mapRequest;	// 要有加有删

	ConnectionHandler _connectionCallback;
	std::shared_ptr<Dispatcher> _dispatcher;
};

// inline defination
template<typename... TArgs>
inline MsgRequest<std::string, std::tuple<TArgs...>> RequestFactory::create(const std::string& method, const TArgs... args)
{
	return MsgRequest<std::string, std::tuple<TArgs...>>(method, std::tuple<TArgs...>(args...), nextMsgid());
}

template<typename... TArgs>
inline std::shared_ptr<AsyncCallCtx> TcpSession::asyncCall(const std::string& method, TArgs... args)
{
	auto request = _reqFactory.create(method, args...);
	return asyncSend(request);
}

template<typename... TArgs>
inline std::shared_ptr<AsyncCallCtx> TcpSession::asyncCall(OnAsyncCall callback, const std::string& method, TArgs... args)
{
	auto request = _reqFactory.create(method, args...);
	return asyncSend(request, callback);
}

template<typename... TArgs>
inline void TcpSession::syncCall(const std::string& method, TArgs... args)
{
	auto request = _reqFactory.create(method, args...);
	auto call = TcpSession::asyncSend(request);
	call->sync();
}

template<typename R, typename... TArgs>
inline R& TcpSession::syncCall(R *value, const std::string& method, TArgs... args)
{
	auto request = _reqFactory.create(method, args...);
	auto call = TcpSession::asyncSend(request);
	call->sync().convert(value);
	return *value;
}

template<typename TArg>
inline std::shared_ptr<AsyncCallCtx> TcpSession::asyncSend(const MsgRequest<std::string, TArg>& msgreq, OnAsyncCall callback)
{
	auto sbuf = std::make_shared<msgpack::sbuffer>();
	::msgpack::pack(*sbuf, msgreq);

	std::stringstream ss;
	ss << msgreq.method << msgreq.param;
	auto req = std::make_shared<AsyncCallCtx>(ss.str(), callback);
	_mapRequest.insert(std::make_pair(msgreq.msgid, req));

	_connection->asyncWrite(sbuf);

	return req;
}

typedef std::shared_ptr<TcpSession> SessionPtr;

} }