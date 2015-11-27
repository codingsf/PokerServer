#include "TcpSession.h"
#include <functional>	// std::bind
#include "SessionManager.h"
#include "Exception.h"
#include "boost/format.hpp"
#include "ThreadPool.h"

namespace msgpack {
namespace rpc {

using boost::asio::ip::tcp;
using std::placeholders::_1;
using std::placeholders::_2;

const static int SESSION_TIMEOUT = 5;	// 15 second

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

	auto msgHandler = [weak](msgpack::unpacked upk)
	{
		auto shared = weak.lock();
		if (shared)
			shared->processMsg(upk);
	};
	_connection->setProcessMsgHandler(msgHandler);

	auto netErrorHandler = [weak](const boost::system::error_code& error, boost::exception_ptr pExcept)
	{
		auto shared = weak.lock();
		if (shared)
			shared->netErrorHandler(error, pExcept);
	};
	_connection->setNetErrorHandler(netErrorHandler);
	//_connection->setConnectionHandler(_connectionCallback);
}

void TcpSession::begin(tcp::socket&& socket)
{
	close();

	_connection = std::make_shared<TcpConnection>(std::move(socket));

	init();
	_connection->beginReadSome();
}

boost::future<bool> TcpSession::asyncConnect(const tcp::endpoint& endpoint)
{
	close();

	_connection = std::make_shared<TcpConnection>(_ioService);

	init();
	_connection->asyncConnect(endpoint);
	return _connection->getConnectPromise().get_future();
}

void TcpSession::asyncConnect(const boost::asio::ip::tcp::endpoint& endpoint, ConnectionHandler&& callback)
{
	close();

	_connection = std::make_shared<TcpConnection>(_ioService);
	_connection->setConnectionHandler(std::move(callback));

	init();
	_connection->asyncConnect(endpoint);
}

bool TcpSession::connect(const boost::asio::ip::tcp::endpoint& endpoint, int timeout)
{
	auto fut = asyncConnect(endpoint);
	if (fut.wait_for(boost::chrono::seconds(timeout)) == boost::future_status::ready && fut.get())
		return true;
	else
		return false;
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

void TcpSession::netErrorHandler(const boost::system::error_code& error, boost::exception_ptr pExcept)
{
	std::unique_lock<std::mutex> lck(_mutex);
	for (auto& mapReq : _mapRequest)
	{
		try
		{
			if (mapReq.second._future.is_ready())	// ���_future is_ready����_prom.set_exception�����쳣
				continue;
			mapReq.second._prom.set_exception(pExcept);
			if (mapReq.second._callback)
				mapReq.second._callback(mapReq.second._future);
		}
		catch (const boost::exception& e)
		{
			std::cerr << diagnostic_information(e);
		}
	}
	_mapRequest.clear();
	SessionManager::instance()->stop(shared_from_this());
}

void TcpSession::waitforFinish()
{
	while (_mapRequest.size())
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	while (_connection->pendingWrites())
	{
		std::cerr << "pending:" << _connection->pendingWrites() << std::endl;
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		std::cerr << "pending:" << _connection->pendingWrites() << std::endl;
	}

	close();
}

void TcpSession::processMsg(msgpack::unpacked upk)
{
	object objMsg(upk.get());
	MsgRpc rpc;
	convertObject(objMsg, rpc);

	switch (rpc.type)
	{
	case MSG_TYPE_REQUEST:
		setCurrentTcpSession(shared_from_this());
		_dispatcher->dispatch(objMsg, std::move(*(upk.zone())), _connection);
		break;

	case MSG_TYPE_RESPONSE:
	{
		MsgResponse<object, object> rsp;
		convertObject(objMsg, rsp);
		std::unique_lock<std::mutex> lck(_mutex);
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
					prom.set_exception(boost::copy_exception(ReturnErrorException() <<
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

		std::cerr << "Server error notify: " << req.param.as<std::string>() << std::endl;
	}
	break;

	default:
		throw MessageException() << err_str("objMsg type not found");
	}
}

} }