#pragma once
#include "TcpClient.h"
#include "TcpSession.h"

namespace msgpack {
namespace rpc {

using boost::asio::io_service;
using boost::asio::ip::tcp;

TcpClient::TcpClient(io_service &ios): 
	_ioService(ios)
{
	_dispatcher = std::make_shared<Dispatcher>();
} 

TcpClient::~TcpClient()
{
}

void TcpClient::asyncConnect(const boost::asio::ip::tcp::endpoint &endpoint)
{
	_session = std::make_shared<TcpSession>(_ioService);
	_session->setDispatcher(_dispatcher);
	_session->asyncConnect(endpoint);
}

void TcpClient::close()
{
	_session->close();
}


} }