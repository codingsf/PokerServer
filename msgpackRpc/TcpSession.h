#pragma once
#include "TcpConnection.h"
#include "Dispatcher.h"
#include <memory>	// enable_shared_from_this 
#include <boost/thread/future.hpp>

namespace msgpack {
namespace rpc {

typedef std::function<void(boost::shared_future<msgpack::object>& )> ResultCallback;
struct CallPromise
{
	boost::promise<msgpack::object> _prom;
	boost::shared_future<msgpack::object> _future;
	ResultCallback _callback;

	CallPromise()
	{
		_future = _prom.get_future().share();
	}

	CallPromise(ResultCallback&& callback) : _callback(std::move(callback))
	{
		_future = _prom.get_future().share();
	}
};

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
	TcpSession(boost::asio::io_service& ios, std::shared_ptr<Dispatcher> disp = nullptr);
	virtual ~TcpSession();

	void setDispatcher(std::shared_ptr<Dispatcher> disp);

	void init();
	void begin(boost::asio::ip::tcp::socket&& socket);
	void asyncConnect(const boost::asio::ip::tcp::endpoint& endpoint);

	void stop();
	void close();

	bool isConnected();
	void netErrorHandler(boost::system::error_code& error);

	void waitforFinish();

	// Async call
	template<typename... TArgs>
	boost::shared_future<msgpack::object> call(const std::string& method, TArgs... args);

	template<typename... TArgs>
	void call(ResultCallback&& callback, const std::string& method, TArgs... args);

private:
	void processMsg(msgpack::unpacked upk, std::shared_ptr<TcpConnection> TcpConnection);

	boost::asio::io_service& _ioService;
	boost::asio::ip::tcp::endpoint _peerAddr;
	std::shared_ptr<TcpConnection> _connection;
	ConnectionHandler _connectionCallback;

	std::unordered_map<uint32_t, CallPromise> _mapRequest;
	RequestFactory _reqFactory;

	std::shared_ptr<Dispatcher> _dispatcher;
};

inline void TcpSession::waitforFinish()
{
	std::chrono::milliseconds span(1000);
	for (auto& apair : _mapRequest)
		apair.second._future.wait();
	// 还要完善，加上没有完成的dispatcher处理的判断
}

// inline defination
template<typename... TArgs>
inline MsgRequest<std::string, std::tuple<TArgs...>> RequestFactory::create(const std::string& method, const TArgs... args)
{
	return MsgRequest<std::string, std::tuple<TArgs...>>(method, std::tuple<TArgs...>(args...), nextMsgid());
}

template<typename... TArgs>
inline boost::shared_future<msgpack::object> TcpSession::call(const std::string& method, TArgs... args)
{
	auto msgreq = _reqFactory.create(method, args...);
	auto sbuf = std::make_shared<msgpack::sbuffer>();
	msgpack::pack(*sbuf, msgreq);

	auto ret = _mapRequest.emplace(msgreq.msgid, CallPromise());
	_connection->asyncWrite(sbuf);

	return ret.first->second._future;
}

template<typename... TArgs>
inline void TcpSession::call(ResultCallback&& callback, const std::string& method, TArgs... args)
{
	auto msgreq = _reqFactory.create(method, args...);
	auto sbuf = std::make_shared<msgpack::sbuffer>();
	msgpack::pack(*sbuf, msgreq);

	_mapRequest.emplace(msgreq.msgid, CallPromise(std::move(callback)));
	_connection->asyncWrite(sbuf);
}

typedef std::shared_ptr<TcpSession> SessionPtr;

} }