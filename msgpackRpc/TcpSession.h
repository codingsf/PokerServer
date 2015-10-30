#pragma once
#include "TcpConnection.h"
#include "Dispatcher.h"
#include <memory>	// enable_shared_from_this 
#include <boost/thread/future.hpp>

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
	boost::future<msgpack::object> asyncCall(const std::string& method, TArgs... args);

private:
	template<typename TArg>
	boost::future<msgpack::object> asyncSend(const MsgRequest<std::string, TArg>& msgreq);

	void processMsg(const object& msg, std::shared_ptr<TcpConnection> TcpConnection);

	boost::asio::io_service& _ioService;
	RequestFactory _reqFactory;

	std::shared_ptr<TcpConnection> _connection;
	std::map<uint32_t, std::shared_ptr<boost::promise<msgpack::object>>> _mapRequest;	// 要有加有删

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
inline boost::future<msgpack::object> TcpSession::asyncCall(const std::string& method, TArgs... args)
{
	auto request = _reqFactory.create(method, args...);
	return asyncSend(request);
}

template<typename TArg>
inline boost::future<msgpack::object> TcpSession::asyncSend(const MsgRequest<std::string, TArg>& msgreq)
{
	auto sbuf = std::make_shared<msgpack::sbuffer>();
	msgpack::pack(*sbuf, msgreq);

	auto prom = std::make_shared<boost::promise<msgpack::object>>();
	_mapRequest.insert(std::make_pair(msgreq.msgid, prom));

	_connection->asyncWrite(sbuf);
	return prom->get_future();;
}

typedef std::shared_ptr<TcpSession> SessionPtr;

} }