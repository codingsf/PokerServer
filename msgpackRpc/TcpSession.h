#pragma once
#include "TcpConnection.h"
#include "Dispatcher.h"
#include <memory>	// enable_shared_from_this 

namespace msgpack {
namespace rpc {

class RequestFactory
{
public:
	template<typename TMethod, typename... TArgs>
	MsgRequest<TMethod, std::tuple<TArgs...>> create(const TMethod& method, const TArgs... args);

private:
	uint32_t nextMsgid();
	uint32_t _nextMsgid = {1};
};

class TcpSession : public std::enable_shared_from_this<TcpSession>
{
public:
	TcpSession(boost::asio::io_service& ios, ConnectionHandler connectionHandler = ConnectionHandler());

	boost::asio::ip::tcp::socket& getSocket();
	void setDispatcher(std::shared_ptr<Dispatcher> disp);

	void begin(boost::asio::ip::tcp::socket socket);
	void asyncConnect(const boost::asio::ip::tcp::endpoint& endpoint);

	void stop();
	void close();

	bool isConnected();
	void netErrorHandler(boost::system::error_code &error);

	// asyncCall
	template<typename TMethod, typename... TArgs>
	std::shared_ptr<AsyncCallCtx> asyncCall(const TMethod& method, TArgs... args);

	template<typename TMethod, typename... TArgs>
	std::shared_ptr<AsyncCallCtx> asyncCall(OnAsyncCall callback, const TMethod& method, TArgs... args);

	// syncCall
	template<typename TMethod, typename... TArgs>
	void syncCall(const TMethod& method, TArgs... args);

	template<typename R, typename TMethod, typename... TArgs>
	R& syncCall(R* value, const TMethod& method, TArgs... args);

private:
	template<typename TMethod, typename TArg>
	std::shared_ptr<AsyncCallCtx> asyncSend(const MsgRequest<TMethod, TArg>& msgreq, OnAsyncCall callback = OnAsyncCall());

	void processMsg(const object& msg, std::shared_ptr<TcpConnection> TcpConnection);

	boost::asio::io_service& _ioService;
	RequestFactory _reqFactory;

	std::shared_ptr<TcpConnection> _connection;
	std::map<uint32_t, std::shared_ptr<AsyncCallCtx>> _mapRequest;	// 要有加有删

	ConnectionHandler _connectionCallback;
	std::shared_ptr<Dispatcher> _dispatcher;
};

/// inline defination
template<typename TMethod, typename... TArgs>
inline MsgRequest<TMethod, std::tuple<TArgs...>> RequestFactory::create(const TMethod& method, const TArgs... args)
{
	return MsgRequest(method, std::tuple<TArgs...>(args...), nextMsgid());
}

template<typename TMethod, typename... TArgs>
inline std::shared_ptr<AsyncCallCtx> TcpSession::asyncCall(const TMethod& method, TArgs... args)
{
	auto request = _reqFactory.create(method, args...);
	return asyncSend(request);
}

template<typename TMethod, typename... TArgs>
inline std::shared_ptr<AsyncCallCtx> TcpSession::asyncCall(OnAsyncCall callback, const TMethod& method, TArgs... args)
{
	auto request = _reqFactory.create(method, args...);
	return asyncSend(request, callback);
}

template<typename TMethod, typename... TArgs>
inline void TcpSession::syncCall(const TMethod& method, TArgs... args)
{
	auto request = _reqFactory.create(method, args...);
	auto call = TcpSession::asyncSend(request);
	call->sync();
}

template<typename R, typename TMethod, typename... TArgs>
inline R& TcpSession::syncCall(R *value, const TMethod& method, TArgs... args)
{
	auto request = _reqFactory.create(method, args...);
	auto call = TcpSession::asyncSend(request);
	call->sync().convert(value);
	return *value;
}

template<typename TMethod, typename TArg>
inline std::shared_ptr<AsyncCallCtx> TcpSession::asyncSend(const MsgRequest<TMethod, TArg>& msgreq, OnAsyncCall callback)
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