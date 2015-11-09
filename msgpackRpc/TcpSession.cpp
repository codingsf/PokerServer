#include "TcpSession.h"
#include <functional>	// std::bind
#include "SessionManager.h"
#include "Exception.h"

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
}

TcpSession::~TcpSession()
{
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
	objMsg.convert(&rpc);
	switch (rpc.type)
	{
	case MSG_TYPE_REQUEST:
		if (!_dispatcher)
			throw DispatcherNotFoundException() << err_str("DispatcherNotFound");
		else
			_dispatcher->dispatch(objMsg, TcpConnection);
		break;

	case MSG_TYPE_RESPONSE:
	{
		MsgResponse<object, object> res;
		objMsg.convert(&res);
		auto found = _mapRequest.find(res.msgid);
		if (found == _mapRequest.end())
		{
			throw RequestNotFoundException() << err_no(res.msgid) << err_str("RequestNotFound");
		}
		else
		{
			auto prom = found->second;
			_mapRequest.erase(found);
			if (res.error.type == msgpack::type::NIL)
			{
				prom->set_value(res.result);
			}
			else if (res.error.type == msgpack::type::BOOLEAN)
			{
				bool isError;
				res.error.convert(&isError);
				if (isError)
				{
					std::tuple<int, std::string> tup = res.result.as<std::tuple<int, std::string>>();
					msgerror err(std::get<1>(tup), (ServerSideError)std::get<0>(tup));
					prom->set_exception(boost::copy_exception(err));	// catch (const msgpack::rpc::msgerror& e) 还有问题
					//prom->set_exception(boost::copy_exception(std::runtime_error(std::get<1>(tup))));	// catch (const std::exception& e)
				}
				else
					prom->set_value(res.result);
			}
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

} }