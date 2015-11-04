#include <boost/test/included/unit_test.hpp>
#include "TcpConnection.h"
#include "TcpSession.h"
#include "TcpServer.h"
#include "SessionManager.h"
#include <iostream>
#include <boost/timer.hpp>

int count = 1;
const static int PORT = 8070;

int clientadd(int a, int b)
{
	std::cout << "client: handle add, " << a << " + " << b << std::endl;
	return a + b;
}

BOOST_AUTO_TEST_CASE(begin)
{
	std::cout << "enter repeat times: ";
	std::cin >> count;
	std::cout << std::endl << std::endl;
}

BOOST_AUTO_TEST_CASE(connect_close)
{
	std::cout << "BGN connect_close" << std::endl;
	boost::asio::io_service client_io;
	auto pWork = std::make_shared<boost::asio::io_service::work>(client_io);// *clinet_thread exit without work
	boost::thread clinet_thread([&client_io]() { client_io.run(); });

	{
		boost::timer t;
		for (int i = 0; i < count; i++)
		{
			auto session = std::make_shared<msgpack::rpc::TcpSession>(client_io, nullptr);
			session->asyncConnect(boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), PORT));
			session->close();	// session->_connection引用记数（asyncRead中加一的），会减一
		}
		std::cout << t.elapsed() << std::endl;
	}

	{
		boost::timer t;
		auto session = std::make_shared<msgpack::rpc::TcpSession>(client_io, nullptr);
		for (int i = 0; i < count; i++)
		{
			session->asyncConnect(boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), PORT));
			session->close();
		}
		std::cout << t.elapsed() << std::endl;
	}

	pWork.reset();
	clinet_thread.join();
	std::cout << "END connect_close" << std::endl; << std::endl;
}

BOOST_AUTO_TEST_CASE(syncCall)
{
	std::cout << "BGN syncCall" << std::endl;

	boost::asio::io_service client_io;
	auto pWork = std::make_shared<boost::asio::io_service::work>(client_io);
	boost::thread clinet_thread([&client_io]() { client_io.run(); });

	try
	{
		auto session = std::make_shared<msgpack::rpc::TcpSession>(client_io, nullptr);
		session->asyncConnect(boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), PORT));

		boost::timer t;
		for (int i = 0; i < count; i++)
			BOOST_CHECK_EQUAL(session->call("add", 1, 2).get().as<int>(), 3);
		std::cout << t.elapsed() << std::endl;
		session->close();
	}
	catch (const std::exception& e)
	{
		std::cerr << "call failed: " << e.what() << std::endl;
	}

	pWork.reset();
	clinet_thread.join();
	std::cout << "END syncCall" << std::endl;
}

//BOOST_AUTO_TEST_CASE(serverRequest)
//{
//	std::cout << "BGN syncCall" << std::endl;
//
//	boost::asio::io_service client_io;
//	auto pWork = std::make_shared<boost::asio::io_service::work>(client_io);
//	boost::thread clinet_thread([&client_io]() { client_io.run(); });
//
//	std::shared_ptr<msgpack::rpc::Dispatcher> disp = std::make_shared<msgpack::rpc::Dispatcher>();
//	disp->add_handler("add", &clientadd);
//
//	try
//	{
//		auto session = std::make_shared<msgpack::rpc::TcpSession>(client_io, disp);
//		session->asyncConnect(boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), PORT));
//
//		boost::timer t;
//		for (int i = 0; i < count; i++)
//		{
//			BOOST_CHECK_EQUAL(session->call("add", 1, 2).get().as<int>(), 3);
//			if (i >= 5491)
//				std::cout << i << std::endl;
//		}
//		std::cout << t.elapsed() << std::endl;
//
//		session->close();
//	}
//	catch (const std::exception& e)
//	{
//		std::cerr << "call failed: " << e.what() << std::endl;
//	}
//
//	pWork.reset();
//	clinet_thread.join();
//	std::cout << "END syncCall" << std::endl;
//}

BOOST_AUTO_TEST_CASE(end)
{
	std::cout << "enter something to exit test: ";
	std::string str;
	std::cin >> str;
}