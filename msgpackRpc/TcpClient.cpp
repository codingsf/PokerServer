#pragma once
#include "TcpClient.h"

namespace msgpack {
namespace rpc {
namespace asio {


using boost::asio::io_service;
using boost::asio::ip::tcp;

TcpClient::TcpClient(io_service &ios): 
	_ioService(ios)
{
	_dispatcher = std::make_shared<msgpack::rpc::asio::dispatcher>();
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

} } }
