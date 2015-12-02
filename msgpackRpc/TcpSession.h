#pragma once
#include "TcpConnection.h"
#include "Dispatcher.h"
#include <memory>	// enable_shared_from_this 
#include <boost/thread/future.hpp>

namespace msgpack {
namespace rpc {

typedef std::pair<msgpack::object, msgpack::zone> ObjectZone;
typedef std::function<void(boost::shared_future<ObjectZone>)> ResultCallback;
struct CallPromise
{
	boost::promise<ObjectZone> _prom;
	boost::shared_future<ObjectZone> _future;
	ResultCallback _callback;	// 内部通过future.get时可能会抛出异常，所以内部要用catch。（或调用它的地方要catch）

	CallPromise()
	{
		_future = _prom.get_future().share();
	}

	CallPromise(ResultCallback&& callback) : _callback(std::move(callback))
	{
		_future = _prom.get_future().share();
	}

	CallPromise(CallPromise&& call) :
		_prom(std::move(call._prom)),
		_future(call._future),
		_callback(std::move(call._callback))
		{
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
	bool connect(const boost::asio::ip::tcp::endpoint& endpoint, int timeout = 3);

	void asyncConnect(const boost::asio::ip::tcp::endpoint& endpoint, ConnectionHandler&& callback);
	boost::future<bool> asyncConnect(const boost::asio::ip::tcp::endpoint& endpoint);

	void stop();
	void close();

	bool isConnected();
	ConnectionStatus getConnectionStatus() const;
	void netErrorHandler(const boost::system::error_code& error, boost::exception_ptr pExcept);

	void waitforFinish();

	// Async call
	template<typename... TArgs>
	boost::shared_future<ObjectZone> call(const std::string& method, TArgs... args);

	template<typename... TArgs>
	void call(ResultCallback&& callback, const std::string& method, TArgs... args);

	std::string _uuid;
private:
	void processMsg(msgpack::unpacked upk);

	boost::asio::io_service& _ioService;
	std::shared_ptr<TcpConnection> _connection;
	ConnectionHandler _connectionCallback;

	std::mutex _reqMutex;
	std::atomic<uint32_t> _reqNextMsgid{1};
	std::unordered_map<uint32_t, CallPromise> _reqPromiseMap;

	std::shared_ptr<Dispatcher> _dispatcher;
};

// inline defination
template<typename... TArgs>
inline MsgRequest<std::string, std::tuple<TArgs...>> RequestFactory::create(const std::string& method, const TArgs... args)
{
	return MsgRequest<std::string, std::tuple<TArgs...>>(method, std::tuple<TArgs...>(args...), nextMsgid());
}

template<typename... TArgs>
inline boost::shared_future<ObjectZone> TcpSession::call(const std::string& method, TArgs... args)
{
	auto msgreq = MsgRequest<std::string, std::tuple<TArgs...>>(method, std::tuple<TArgs...>(args...), _reqNextMsgid++);

	auto sbuf = std::make_shared<msgpack::sbuffer>();
	msgpack::pack(*sbuf, msgreq);

	std::unique_lock<std::mutex> lck(_reqMutex);
	auto ret = _reqPromiseMap.emplace(msgreq.msgid, CallPromise());
	// ***千万注意不要在这里unlock, 因为下面还要用到_reqPromiseMap的迭代器ret

	_connection->asyncWrite(sbuf);
	return ret.first->second._future;
}

template<typename... TArgs>
inline void TcpSession::call(ResultCallback&& callback, const std::string& method, TArgs... args)
{
	auto msgreq = MsgRequest<std::string, std::tuple<TArgs...>>(method, std::tuple<TArgs...>(args...), _reqNextMsgid++);

	auto sbuf = std::make_shared<msgpack::sbuffer>();
	msgpack::pack(*sbuf, msgreq);

	std::unique_lock<std::mutex> lck(_reqMutex);
	_reqPromiseMap.emplace(msgreq.msgid, CallPromise(std::move(callback)));
	lck.unlock();

	_connection->asyncWrite(sbuf);
}

typedef std::shared_ptr<TcpSession> SessionPtr;

} }