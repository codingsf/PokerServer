#include "TcpSession.h"
#include <functional>	// std::bind
#include "SessionManager.h"
#include "Exception.h"
#include <boost/lexical_cast.hpp>

namespace msgpack {
namespace rpc {

using boost::asio::ip::tcp;
using std::placeholders::_1;
using std::placeholders::_2;

uint32_t RequestFactory::nextMsgid()
{
	return _nextMsgid++;
}

TcpSession::TcpSession(boost::asio::io_service& ios, std::shared_ptr<Dispatcher> disp):
	_ioService(ios),
	_dispatcher(disp)
{
	if (!disp)
		_dispatcher = std::make_shared<Dispatcher>();
}

TcpSession::~TcpSession()
{
	close();
	_reqThenFutures.clear();
}

void TcpSession::setDispatcher(std::shared_ptr<Dispatcher> disp)
{
	_dispatcher = disp;
}

void TcpSession::init()
{
	auto weak = std::weak_ptr<TcpSession>(shared_from_this());

	auto msgHandler = [weak](msgpack::unpacked upk, std::shared_ptr<TcpConnection> TcpConnection)
	{
		auto shared = weak.lock();
		if (shared)
			shared->processMsg(upk, TcpConnection);
	};
	_connection->setProcessMsgHandler(msgHandler);

	auto netErrorHandler = [weak](boost::system::error_code& error)
	{
		auto shared = weak.lock();
		if (shared)
			shared->netErrorHandler(error);
	};
	_connection->setNetErrorHandler(netErrorHandler);
	_connection->setConnectionHandler(_connectionCallback);
}

void TcpSession::begin(tcp::socket socket)
{
	_connection = std::make_shared<TcpConnection>(std::move(socket));

	init();
	_connection->beginReadSome();
}

void TcpSession::asyncConnect(const boost::asio::ip::tcp::endpoint& endpoint)
{
	_connection = std::make_shared<TcpConnection>(_ioService);

	init();
	_connection->asyncConnect(endpoint);
}

void TcpSession::stop()
{
	_connection.reset();
}

void TcpSession::close()
{
	if (_connection)
		_connection->close();
}

bool TcpSession::isConnected()
{
	return _connection->getConnectionStatus() == connection_connected;
}

void TcpSession::netErrorHandler(boost::system::error_code& error)
{
	for (auto& mapReq : _mapRequest)
	{
		mapReq.second->set_exception(boost::copy_exception(std::runtime_error(error.message())));
	}
	SessionManager::instance()->stop(shared_from_this());
}

void TcpSession::processMsg(msgpack::unpacked upk, std::shared_ptr<TcpConnection> TcpConnection)
{
	object objMsg(upk.get());
	MsgRpc rpc;
	convertObject(objMsg, rpc);

	switch (rpc.type)
	{
	case MSG_TYPE_REQUEST:
		_dispatcher->dispatch(objMsg, std::move(*(upk.zone())), TcpConnection);
		break;

	case MSG_TYPE_RESPONSE:
	{
		MsgResponse<object, object> rsp;
		convertObject(objMsg, rsp);
		auto found = _mapRequest.find(rsp.msgid);
		if (found == _mapRequest.end())
		{
			throw RequestNotFoundException() <<
				err_no(error_RequestNotFound) <<
				err_str(std::string("RequestNotFound, msgid = ") + boost::lexical_cast<std::string>(rsp.msgid));
		}
		else
		{
			auto prom = found->second;
			_mapRequest.erase(found);
			if (rsp.error.type == msgpack::type::NIL)
			{
				prom->set_value(rsp.result);
			}
			else if (rsp.error.type == msgpack::type::BOOLEAN)
			{
				bool isError;
				convertObject(rsp.error, isError);
				if (isError)
				{
					std::tuple<int, std::string> tup;
					convertObject(rsp.result, tup);
					prom->set_exception(boost::copy_exception(CallReturnException() <<
						err_no(std::get<0>(tup)) <<
						err_str(std::get<1>(tup))));
				}
				else
					prom->set_value(rsp.result);
			}
			else
				prom->set_exception(std::runtime_error("MsgResponse.error type wrong"));
		}
	}
	break;

	case MSG_TYPE_NOTIFY:
	{
		MsgNotify<object, object> req;
		objMsg.convert(&req);
	}
	break;

	default:
		throw MessageException() << err_str("objMsg type not found");
	}
}

void TcpSession::delFuture()
{
	auto it = _reqThenFutures.begin();
	if (it == _reqThenFutures.end())
		return;

	do
	{
		if (it->is_ready())
		{
			_reqThenFutures.erase(it);
			it = _reqThenFutures.begin();
		}
		else
			it++;
	} while (it != _reqThenFutures.end());
}

void TcpSession::delSharedFuture()
{
	auto it = _reqSharedFuture.begin();
	if (it == _reqSharedFuture.end())
		return;

	do
	{
		if (it->is_ready())
		{
			_reqSharedFuture.erase(it);
			it = _reqSharedFuture.begin();
		}
		else
			it++;
	} while (it != _reqSharedFuture.end());
}

} }