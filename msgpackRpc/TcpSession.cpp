#include "TcpSession.h"
#include <functional>	// std::bind
#include "SessionManager.h"
#include "Exception.h"
#include "boost/format.hpp"

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

void TcpSession::begin(tcp::socket&& socket)
{
	boost::system::error_code ec;
	_peerAddr = socket.remote_endpoint(ec);

	_connection = std::make_shared<TcpConnection>(std::move(socket));

	init();
	_connection->beginReadSome();
}

void TcpSession::asyncConnect(const tcp::endpoint& endpoint)
{
	close();

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
		mapReq.second._prom.set_exception(boost::copy_exception(
			NetException() <<
			err_no(error.value()) <<
			err_str(_peerAddr.address().to_string() + error.message()) <<
			boost::throw_function(BOOST_THROW_EXCEPTION_CURRENT_FUNCTION) <<
			boost::throw_file(__FILE__) <<
			boost::throw_line((int)__LINE__)
			));
		if (_callback)
			mapReq.second._callback(mapReq.second._future);
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
				err_str((boost::format("RequestNotFound, msgid = %d") % rsp.msgid).str());
		}
		else
		{
			auto& prom = found->second._prom;
			if (rsp.error.type == msgpack::type::NIL)
				prom.set_value(rsp.result);
			else if (rsp.error.type == msgpack::type::BOOLEAN)
			{
				bool isError;
				convertObject(rsp.error, isError);
				if (isError)
				{
					std::tuple<int, std::string> tup;
					convertObject(rsp.result, tup);
					prom.set_exception(boost::copy_exception(CallReturnException() <<
						err_no(std::get<0>(tup)) <<
						err_str(std::get<1>(tup))));
				}
				else
					prom.set_value(rsp.result);
			}
			else
				prom.set_exception(std::runtime_error("MsgResponse.error type wrong"));

			if (found->second._callback)
				found->second._callback(found->second._future);	// ResultCallback(future<object>&)
			_mapRequest.erase(found);
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

} }