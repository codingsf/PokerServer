#pragma once
#include "TcpServer.h"
#include "TcpSession.h"

namespace msgpack {
namespace rpc {

using boost::asio::io_service;
using boost::asio::ip::tcp;

TcpServer::TcpServer(io_service& ios, short port):
	_ioService(ios),
	_acceptor(ios, tcp::endpoint(tcp::v4(), port))
{
} 

TcpServer::TcpServer(io_service& ios, const tcp::endpoint& endpoint):
	_ioService(ios),
	_acceptor(ios, endpoint)
{
}

TcpServer::~TcpServer()
{
}

void TcpServer::setDispatcher(std::shared_ptr<msgpack::rpc::dispatcher> disp)
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
	std::shared_ptr<TcpSession> pSession = std::make_shared<TcpSession>(_ioService);
	pSession->setDispatcher(_dispatcher);

	_acceptor.async_accept(pSession->getSocket(),
		[this, pSession](const boost::system::error_code& error)
	{
		if (!error)
		{
			pSession->start();
		}
		else
		{
			// log.error
		}

		startAccept();
	});
}

} }