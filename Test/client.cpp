#include <boost/test/included/unit_test.hpp>
#include "TcpConnection.h"
#include "TcpSession.h"
#include "TcpServer.h"
#include "SessionManager.h"
#include <iostream>


int clientadd(int a, int b)
{
	std::cout << "client: handle add, " << a << " + " << b << std::endl;
	return a + b;
}

BOOST_AUTO_TEST_CASE(client)
{
	const static int PORT = 8070;

	// client
	boost::asio::io_service client_io;
	boost::asio::io_service::work work(client_io);
	boost::thread clinet_thread([&client_io]() { client_io.run(); });

	std::shared_ptr<msgpack::rpc::Dispatcher> disp = std::make_shared<msgpack::rpc::Dispatcher>();
	disp->add_handler("add", &clientadd);


	auto session = std::make_shared<msgpack::rpc::TcpSession>(client_io, disp);
	session->asyncConnect(boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), PORT));

	try
	{
		std::cout << "client: add, 1, 2 = " << session->call("add", 1, 2).get().as<int>() << std::endl;
		session->close();
	}
	catch (const std::exception& e)
	{
		std::cerr << "call failed: " << e.what() << std::endl;
	}

	char line[256];
	if (std::cin.getline(line, 256))
	{
		//auto session = std::make_shared<msgpack::rpc::TcpSession>(client_io, disp);
		//session->asyncConnect(boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), PORT));
		//std::cout << "client: add, 1, 2 = " << session->call("add", 1, 2).get().as<int>() << std::endl;
		client_io.stop();
	}
	clinet_thread.join();
}
