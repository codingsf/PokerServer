#pragma once
#include <memory>
#include <boost/asio.hpp>
#include "Dispatcher.h"

namespace msgpack {
namespace rpc {

class TcpServer
{
public:
	TcpServer(boost::asio::io_service& ios, short port);
	TcpServer(boost::asio::io_service& ios, const boost::asio::ip::tcp::endpoint& endpoint);
	virtual ~TcpServer();

	void start();
	void stop();

	void setDispatcher(std::shared_ptr<msgpack::rpc::dispatcher> disp);

private:
	void startAccept();

	boost::asio::io_service& _ioService;
	boost::asio::ip::tcp::socket _socket;
	boost::asio::ip::tcp::acceptor _acceptor;
	std::shared_ptr<msgpack::rpc::dispatcher> _dispatcher;
};

} }