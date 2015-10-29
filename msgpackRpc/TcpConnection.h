#pragma once

#include "Asio.h"

namespace msgpack {
namespace rpc {

struct func_call_error : public std::runtime_error
{
	func_call_error(const std::string &msg) : std::runtime_error(msg) { }
};


class AsyncCallCtx
{
public:
	enum STATUS_TYPE
	{
		STATUS_WAIT,
		STATUS_RECEIVED,
		STATUS_ERROR,
	};
private:
	STATUS_TYPE m_status;
	ServerSideError m_error_code;
	std::string m_error_msg;
	::msgpack::object m_result;
	std::string m_request;
	boost::mutex m_mutex;
	boost::condition_variable_any m_cond;

	std::function<void(AsyncCallCtx*)> m_callback;
public:
	AsyncCallCtx(const std::string &s, std::function<void(AsyncCallCtx*)> callback)
		: m_status(STATUS_WAIT), m_request(s), m_error_code(success), m_callback(callback)
	{
	}

	void setResult(const ::msgpack::object &result);
	void setError(const ::msgpack::object &error);

	bool isError() const { return m_status == STATUS_ERROR; }
	ServerSideError getErrorCode() const;


	// blocking
	AsyncCallCtx& sync()
	{
		boost::mutex::scoped_lock lock(m_mutex);
		if (m_status == STATUS_WAIT) {
			m_cond.wait(m_mutex);
		}
		return *this;
	}

	const ::msgpack::object &get_result()const
	{
		if (m_status == STATUS_RECEIVED) {
			return m_result;
		}
		else {
			throw func_call_error("not ready");
		}
	}

	template<typename R>
	R& convert(R *value)const
	{
		if (m_status == STATUS_RECEIVED) {
			m_result.convert(value);
			return *value;
		}
		else {
			throw func_call_error("not ready");
		}
	}

	std::string string() const;

private:
	void notify()
	{
		if (m_callback) {
			m_callback(this);
		}
		m_cond.notify_all();
	}
};
typedef std::function<void(AsyncCallCtx*)> OnAsyncCall;
inline std::ostream &operator<<(std::ostream &os, const AsyncCallCtx &request)
{
	os << request.string();
	return os;
}


class client_error : public std::runtime_error
{
public:
	client_error(const std::string &msg) : std::runtime_error(msg) { }
};


inline std::shared_ptr<msgpack::sbuffer> error_notify(const std::string &msg)
{
	// notify type
	msgpack::rpc::MsgNotify<std::string, std::string> notify(
		// method
		"error_notify",
		// params
		msg
		);
	// result
	auto sbuf = std::make_shared<msgpack::sbuffer>();
	msgpack::pack(*sbuf, notify);
	return sbuf;
}

enum ConnectionStatus
{
	connection_none,
	connection_connecting,
	connection_connected,
	connection_error,
};
typedef std::function<void(ConnectionStatus)> connection_callback_t;


class TcpConnection : public std::enable_shared_from_this<TcpConnection>
{
public:
	typedef std::function<void(const object &, std::shared_ptr<TcpConnection>)> on_read_t;

	TcpConnection(boost::asio::io_service& io_service, on_read_t on_read = on_read_t(),
					connection_callback_t connection_callback = connection_callback_t(), error_handler_t error_handler = error_handler_t());

	virtual ~TcpConnection();

	void asyncConnect(const boost::asio::ip::tcp::endpoint& endpoint);

	void asyncRead();

	void asyncWrite(std::shared_ptr<msgpack::sbuffer> msg);

	void start();

	void close();

	boost::asio::ip::tcp::socket& getSocket();

	ConnectionStatus get_connection_status() const;

private:
	void set_connection_status(ConnectionStatus status);

	boost::asio::io_service& _ioService;
	boost::asio::ip::tcp::socket _socket;

	ConnectionStatus m_connection_status;
	connection_callback_t m_connection_callback;

	error_handler_t m_error_handler;

	on_read_t _onRead;
	unpacker _unpacker;
};

} }