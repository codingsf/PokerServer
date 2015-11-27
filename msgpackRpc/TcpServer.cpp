#pragma once
#include "TcpServer.h"
#include "TcpSession.h"
#include "SessionManager.h"

namespace msgpack {
namespace rpc {

using boost::asio::io_service;
using boost::asio::ip::tcp;

TcpServer::TcpServer(short port):
	_socket(_ioService),
	_acceptor(_ioService, tcp::endpoint(tcp::v4(), port)),
	_dispatcher(nullptr)
{
} 

TcpServer::TcpServer(const tcp::endpoint& endpoint):
	_socket(_ioService),
	_acceptor(_ioService, endpoint),
	_dispatcher(nullptr)
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

	for (std::size_t i = 0; i < 3; ++i)
		_threads.push_back(std::thread([this]() {_ioService.run(); }));
}

void TcpServer::stop()
{
	_acceptor.close();

	for (auto& thread : _threads)
		thread.join();
}

void TcpServer::startAccept()
{
	auto pSession = std::make_shared<TcpSession>(_ioService, _dispatcher);

	_acceptor.async_accept(_socket, [this, pSession](const boost::system::error_code& error)
	{
		if (error)
		{
			// log.error
		}
		else
		{
			boost::system::error_code ec;
			tcp::endpoint peer = _socket.remote_endpoint(ec);
			//if (!ec)
			//	std::cout << "accepted: " << peer.address().to_string() << std::endl;

			SessionManager::instance()->start(pSession);
			pSession->begin(std::move(_socket));
		}

		startAccept();
	});
}

} }