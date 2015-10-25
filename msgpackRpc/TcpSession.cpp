#include "TcpSession.h"
#include <utility>

namespace msgpack {
namespace rpc {
namespace asio {

TcpSession::TcpSession(boost::asio::io_service &io_service, connection_callback_t connection_callback, error_handler_t error_handler) :
	_ioService(io_service),
	m_connection_callback(connection_callback),
	m_error_handler(error_handler)
{
	auto on_read = [this](const object &msg, std::shared_ptr<TcpConnection> connection)
	{
		receive(msg, connection);
	};
	_connection = std::make_shared<TcpConnection>(_ioService, on_read, m_connection_callback);
}

void TcpSession::receive(const object &msg, std::shared_ptr<TcpConnection> TcpConnection)
{
	::msgpack::rpc::MsgRpc rpc;
	msg.convert(&rpc);
	switch (rpc.type) {
	case ::msgpack::rpc::MSG_TYPE_REQUEST:
		_dispatcher->dispatch(msg, TcpConnection);
		break;

	case ::msgpack::rpc::MSG_TYPE_RESPONSE:
	{
		::msgpack::rpc::MsgResponse<object, object> res;
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

	case ::msgpack::rpc::MSG_TYPE_NOTIFY:
	{
		::msgpack::rpc::MsgNotify<object, object> req;
		msg.convert(&req);
	}
	break;

	default:
		throw client_error("rpc type error");
	}
}



} } }