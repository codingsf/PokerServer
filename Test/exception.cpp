#include <boost/test/unit_test.hpp> 
#include "TcpConnection.h"
#include "TcpSession.h"
#include "TcpServer.h"
#include "SessionManager.h"
#include <iostream>
#include <boost/timer.hpp>
#include "define.h"


BOOST_AUTO_TEST_CASE(BgnException)
{
	std::cout << "BGN Exception" << std::endl;
	std::cout << "enter repeat times: ";
	std::cin >> count;
}

BOOST_AUTO_TEST_CASE(Exception0)
{
	boost::asio::io_service server_io;
	msgpack::rpc::TcpServer server(server_io, PORT);
	server.start();
	boost::thread server_thread([&server_io]() { server_io.run(); });
	boost::future<void> then;
	try
	{
		boost::asio::io_service client_io;
		auto pWork = std::make_shared<boost::asio::io_service::work>(client_io);
		boost::thread clinet_thread([&client_io]() { client_io.run(); });

		auto session = std::make_shared<msgpack::rpc::TcpSession>(client_io, nullptr);
		session->asyncConnect(boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), PORT));
		for (int i = 0; i < count; i++)
		{
			then = session->call("add", 1, 2).then([&](boost::future<msgpack::object> objResult)
			{
				try
				{
					std::cout << "client: 1 + 2 = " << objResult.get().as<int>() << std::endl;
				}
				catch (const boost::exception& e)
				{
					std::cerr << diagnostic_information(e);
				}
			});
		}
		std::this_thread::sleep_for(std::chrono::seconds(10));	// 主线程执行太快，另一执行then的lamda方法还没返回，导致boost::future<void> then的is_ready=false
			//then.wait();
		pWork.reset();
				session->close();
		clinet_thread.join();
		clinet_thread.get_id();
		// join会等待。所以session不会析构，session里的_reqThenFutures不会析构
	}
	catch (const boost::exception& e)
	{
		std::cerr << "boost::exception" << std::endl;
		std::cerr << diagnostic_information(e) << std::endl;
	}
	catch (const std::exception& e)
	{
		std::cerr << "std::exception" << std::endl;
		std::cerr << e.what() << std::endl;
	}
	server_io.stop();
	server_thread.join();
}

//BOOST_AUTO_TEST_CASE(Exception1)
//{
//	boost::asio::io_service server_io;
//	msgpack::rpc::TcpServer server(server_io, PORT);
//	server.start();
//	boost::thread server_thread([&server_io]() { server_io.run(); });
//
//	boost::asio::io_service client_io;
//	auto pWork = std::make_shared<boost::asio::io_service::work>(client_io);
//	boost::thread clinet_thread([&client_io]() { client_io.run(); });
//	try
//	{
//		boost::future<void> then;
//		auto session = std::make_shared<msgpack::rpc::TcpSession>(client_io, nullptr);
//		session->asyncConnect(boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), PORT));
//		for (int i = 0; i < count; i++)
//		{
//			then = session->call("add", 1, 2).then([&](boost::future<msgpack::object> objResult)
//			{
//				try
//				{
//					std::cout << "client: 1 + 2 = " << objResult.get().as<int>() << std::endl;
//				}
//				catch (const boost::exception& e)
//				{
//					std::cerr << diagnostic_information(e);
//				}
//			});
//			session->saveFuture(std::move(then));
//		}
//	}
//	catch (const boost::exception& e)
//	{
//		std::cerr << diagnostic_information(e);
//	}
//	// client有泄漏。then还没返回，而此时session已经析构，session里的_reqThenFutures要析构，会处于阻塞状态
//	pWork.reset();
//	clinet_thread.join();
//	server_io.stop();
//	server_thread.join();
//}

//BOOST_AUTO_TEST_CASE(Exception2)
//{
//	boost::asio::io_service server_io;
//	msgpack::rpc::TcpServer server(server_io, 8070);
//
//	std::shared_ptr<msgpack::rpc::Dispatcher> dispatcher = std::make_shared<msgpack::rpc::Dispatcher>();
//	dispatcher->add_handler("add", &serveradd);
//	//dispatcher->add_handler("mul", &mul);
//	//server.setDispatcher(dispatcher);
//	server.start();
//	boost::thread server_thread([&server_io]() { server_io.run(); });
//
//	boost::asio::io_service client_io;
//	auto pWork = std::make_shared<boost::asio::io_service::work>(client_io);
//	boost::thread clinet_thread([&client_io]() { client_io.run(); });
//
//	try
//	{
//		auto session = std::make_shared<msgpack::rpc::TcpSession>(client_io, nullptr);
//		session->asyncConnect(boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), PORT));
//
//		session->call("add", 1, 2);
//		BOOST_CHECK_EQUAL(session->call("add", 1, 2).get().as<int>(), 2);
//		BOOST_CHECK_EQUAL(session->call("add", 1, 2).get().as<int>(), 2);
//	}
//	catch (const std::exception& e)
//	{
//		std::cerr << "call failed: " << e.what() << std::endl;
//	}
//
//	pWork.reset();
//	clinet_thread.join();
//	server_io.stop();
//	server_thread.join();
//}

BOOST_AUTO_TEST_CASE(EndException)
{
	std::cout << "End Exception" << std::endl;
}
