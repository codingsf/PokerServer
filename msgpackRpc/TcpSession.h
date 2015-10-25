#pragma once
#include "TcpConnection.h"
#include "Dispatcher.h"

namespace msgpack {
namespace rpc {
namespace asio {

class RequestFactory
{
	uint32_t _nextMsgid;
public:
	RequestFactory() : _nextMsgid(1) {}

	uint32_t nextMsgid() { return _nextMsgid++; }

	template<typename... A1>
	::msgpack::rpc::MsgRequest<std::string, std::tuple<A1...>>
		create(const std::string &method, A1... a1)
	{
		uint32_t msgid = nextMsgid();
		typedef std::tuple<A1...> Parameter;
		return ::msgpack::rpc::MsgRequest<std::string, Parameter>(
			method, Parameter(a1...), msgid);
	}
};


class TcpServer;
class TcpSession
{
	friend TcpServer;

	boost::asio::io_service &_ioService;
    RequestFactory _reqFactory;

    std::shared_ptr<TcpConnection> _connection;
    std::map<uint32_t, std::shared_ptr<AsyncCallCtx>> _mapRequest;	// 要有加有删

    connection_callback_t m_connection_callback;
    error_handler_t m_error_handler;
	std::shared_ptr<msgpack::rpc::asio::dispatcher> _dispatcher;

public:
	void start() { _connection->start(); }
	boost::asio::ip::tcp::socket & getSocket() { return _connection->getSocket(); }
	void setDispatcher(std::shared_ptr<msgpack::rpc::asio::dispatcher> disp) { _dispatcher = disp; }

	template<typename R, typename A1, typename A2>
	void add_handler(const std::string &method, R(*handler)(A1, A2))
	{
		_dispatcher->add_handler(method, handler);
	}

	TcpSession(boost::asio::io_service &io_service,
					connection_callback_t connection_callback = connection_callback_t(),
					error_handler_t error_handler = error_handler_t());


    void asyncConnect(const boost::asio::ip::tcp::endpoint &endpoint)
    {
        _connection->asyncConnect(endpoint);
    }

    void close()
    {
        _connection->close();
    }

    bool is_connect()
    {
        return _connection->get_connection_status()==connection_connected;
    }

	// asyncCall with callback
	template<typename... A1>
    std::shared_ptr<AsyncCallCtx> asyncCall(OnAsyncCall callback, const std::string &method, A1... a1)
    {
		auto request = _reqFactory.create(method, a1...);	// auto = msgpack::rpc::MsgRequest<std::string, std::tuple<A1...>>
		return asyncSend(request, callback);
	}

	// asyncCall without callback
	template<typename... A1>
	std::shared_ptr<AsyncCallCtx> asyncCall(const std::string &method, A1... a1)
    {
		auto request = _reqFactory.create(method, a1...);
		return asyncSend(request);
	}

	// syncCall
	template<typename R, typename... A1> 	R &syncCall(R *value, const std::string &method, A1... a1)
	{
		auto request = _reqFactory.create(method, a1...);
		auto call = asyncSend(request);
		call->sync().convert(value);
		return *value;
	}

private:
	template<typename Parameter>
	std::shared_ptr<AsyncCallCtx> asyncSend(const ::msgpack::rpc::MsgRequest<std::string, Parameter> &msgreq, OnAsyncCall callback = OnAsyncCall())
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

	void receive(const object &msg, std::shared_ptr<TcpConnection> TcpConnection);
};

} } }
