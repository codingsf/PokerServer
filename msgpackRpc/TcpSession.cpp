#include "TcpSession.h"
#include <functional>	// std::bind

namespace msgpack {
namespace rpc {

using boost::asio::ip::tcp;

uint32_t RequestFactory::nextMsgid()
{
	return _nextMsgid++;
}

TcpSession::TcpSession(boost::asio::io_service &io_service, connection_callback_t connection_callback, error_handler_t error_handler) :
	_ioService(io_service),
	m_connection_callback(connection_callback),
	m_error_handler(error_handler)
{
	_connection = std::make_shared<TcpConnection>(_ioService,
		std::bind(&TcpSession::receive, shared_from_this(), std::placeholders::_1, std::placeholders::_2),
		m_connection_callback);
}

void TcpSession::start()
{
	_connection->start();
}

tcp::socket& TcpSession::getSocket()
{
	return _connection->getSocket();
}

void TcpSession::setDispatcher(std::shared_ptr<msgpack::rpc::dispatcher> disp)
{
	_dispatcher = disp;
}

void TcpSession::asyncConnect(const boost::asio::ip::tcp::endpoint &endpoint)
{

}

void TcpSession::stop()
{
	_connection.reset();
}

void TcpSession::close()
{
	_connection->close();
}

bool TcpSession::is_connect()
{
	return _connection->get_connection_status() == connection_connected;
}

void TcpSession::receive(const object &msg, std::shared_ptr<TcpConnection> TcpConnection)
{
	MsgRpc rpc;
	msg.convert(&rpc);
	switch (rpc.type) {
	case MSG_TYPE_REQUEST:
		_dispatcher->dispatch(msg, TcpConnection);
		break;

	case MSG_TYPE_RESPONSE:
	{
		MsgResponse<object, object> res;
		msg.convert(&res);
		auto found = _mapRequest.find(res.msgid);
		if (found != _mapRequest.end()) {
			if (res.error.type == msgpack::type::NIL) {
				found->second->setResult(res.result);
			}
			else if (res.error.type == msgpack::type::BOOLEAN) {
				bool isError;
				res.error.convert(&isError);
				if (isError) {
					found->second->setError(res.result);
				}
				else {
					found->second->setResult(res.result);
				}
			}
			// _mapRequest.erase(found);
		}
		else {
			throw client_error("no request for response");
		}
	}
	break;

	case MSG_TYPE_NOTIFY:
	{
		MsgNotify<object, object> req;
		msg.convert(&req);
	}
	break;

	default:
		throw client_error("rpc type error");
	}
}

} }