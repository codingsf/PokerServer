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
	virtual ~TcpSession();

	void setDispatcher(std::shared_ptr<Dispatcher> disp);

	void init();
	void begin(boost::asio::ip::tcp::socket socket);
	void asyncConnect(const boost::asio::ip::tcp::endpoint& endpoint);

	void stop();
	void close();

	bool isConnected();
	void netErrorHandler(boost::system::error_code& error);

	// Async call
	template<typename... TArgs>
	boost::future<msgpack::object> call(const std::string& method, TArgs... args);

private:
	void processMsg(msgpack::unpacked upk, std::shared_ptr<TcpConnection> TcpConnection);

	boost::asio::io_service& _ioService;
	std::shared_ptr<TcpConnection> _connection;
	ConnectionHandler _connectionCallback;

	RequestFactory _reqFactory;
	std::unordered_map<uint32_t, std::shared_ptr<boost::promise<msgpack::object>>> _mapRequest;	// 要有加有删

	std::shared_ptr<Dispatcher> _dispatcher;
};

// inline defination
template<typename... TArgs>
inline MsgRequest<std::string, std::tuple<TArgs...>> RequestFactory::create(const std::string& method, const TArgs... args)
{
	return MsgRequest<std::string, std::tuple<TArgs...>>(method, std::tuple<TArgs...>(args...), nextMsgid());
}

template<typename... TArgs>
inline boost::future<msgpack::object> TcpSession::call(const std::string& method, TArgs... args)
{
	auto msgreq = _reqFactory.create(method, args...);
	auto sbuf = std::make_shared<msgpack::sbuffer>();
	msgpack::pack(*sbuf, msgreq);

	auto prom = std::make_shared<boost::promise<msgpack::object>>();
	_mapRequest.emplace(msgreq.msgid, prom);

	_connection->asyncWrite(sbuf);
	return prom->get_future();;
}

typedef std::shared_ptr<TcpSession> SessionPtr;

} }