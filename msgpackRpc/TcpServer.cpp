#pragma once
#include "TcpServer.h"
#include "TcpSession.h"
#include "SessionManager.h"

namespace msgpack {
namespace rpc {

using boost::asio::io_service;
using boost::asio::ip::tcp;

TcpServer::TcpServer(io_service& ios, short port):
	_ioService(ios),
	_socket(ios),
	_acceptor(ios, tcp::endpoint(tcp::v4(), port))
{
} 

TcpServer::TcpServer(io_service& ios, const tcp::endpoint& endpoint):
	_ioService(ios),
	_socket(ios),
	_acceptor(ios, endpoint)
{
}

TcpServer::~TcpServer()
{
}

void TcpServer::setDispatcher(std::shared_ptr<Dispatcher> disp)
{
	_dispatcher = disp;
}

void TcpServer::start()
{
	startAccept();
}

void TcpServer::stop()
{
	_acceptor.close();
}

void TcpServer::startAccept()
{
	auto pSession = std::make_shared<TcpSession>(_ioService, _dispatcher ? _dispatcher : std::make_shared<Dispatcher>());

	_acceptor.async_accept(_socket, [this, pSession](const boost::system::error_code& error)
	{
		if (error)
		{
			// log.error
		}
		else
		{
			SessionManager::instance()->start(pSession);
			pSession->begin(std::move(_socket));
		}

		startAccept();
	});
}

} }