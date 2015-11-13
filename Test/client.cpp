#include <boost/test/unit_test.hpp> 
#include "TcpConnection.h"
#include "TcpSession.h"
#include "TcpServer.h"
#include "SessionManager.h"
#include <iostream>
#include <boost/timer.hpp>
#include "define.h"
using namespace msgpack::rpc;

BOOST_AUTO_TEST_CASE(begin)
{
	//boost::asio::io_service ios;
	//Dispatcher disp;
	//TcpConnection conn(ios);
	//TcpSession ses(ios, nullptr);
	//TcpServer server(ios, 8070);

	//std::cout << "Dispatcher:" << sizeof(disp) << std::endl;
	//std::cout << "TcpConnection:" << sizeof(ios) << std::endl;
	//std::cout << "TcpSession:" << sizeof(ses) << std::endl;
	//std::cout << "TcpServer:" << sizeof(server) << std::endl;

	BufferManager::instance();

	std::cout << "enter repeat times: ";
	std::cin >> count;
	std::cout << std::endl << std::endl;
}

//BOOST_AUTO_TEST_CASE(connect_close)
//{
//	std::cout << "BGN connect_close" << std::endl;
//	boost::asio::io_service client_io;
//	auto pWork = std::make_shared<boost::asio::io_service::work>(client_io);// *clinet_thread exit without work
//	boost::thread clinet_thread([&client_io]() { client_io.run(); });
//
//	{
//		boost::timer t;
//		for (int i = 0; i < count; i++)
//		{
//			auto session = std::make_shared<msgpack::rpc::TcpSession>(client_io, nullptr);
//			session->asyncConnect(boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), PORT));
//		}
//		std::cout << t.elapsed() << std::endl;
//	}
//
//	{
//		boost::timer t;
//		auto session = std::make_shared<msgpack::rpc::TcpSession>(client_io, nullptr);
//		for (int i = 0; i < count; i++)
//		{
//			session->asyncConnect(boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), PORT));
//		}
//		std::cout << t.elapsed() << std::endl;
//	}
//
//	pWork.reset();
//	clinet_thread.join();
//	std::cout << "END connect_close" << std::endl << std::endl;
//}

//BOOST_AUTO_TEST_CASE(OnewayCall)
//{
//	std::cout << "BGN OnewayCall" << std::endl;
//
//	boost::asio::io_service client_io;
//	auto pWork = std::make_shared<boost::asio::io_service::work>(client_io);
//	boost::thread clinet_thread([&client_io]() { client_io.run(); });
//
//	try
//	{
//		boost::timer t;
//		int i1 = 1, i3 = 3;
//		for (int i = 0; i < count; i++)
//		{
//			auto session = std::make_shared<msgpack::rpc::TcpSession>(client_io, nullptr);
//			session->asyncConnect(boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), PORT));
//			auto fut = session->call("add", i1, i3);
//			BOOST_CHECK_EQUAL(fut.get().as<int>(), i1 + i3);
//		}
//		std::cout << "sync call ok: " << t.elapsed() << std::endl;
//	}
//	catch (const std::exception& e)
//	{
//		std::cerr << "call failed: " << e.what() << std::endl;
//	}
//	catch (...)
//	{
//		std::cerr << "call failed: " << std::endl;
//	}
//
//	try
//	{
//		int i11 = 11, i33 = 33;
//		auto on_result = [=](boost::shared_future<msgpack::object> fut)
//		{
//			BOOST_CHECK_EQUAL(fut.get().as<int>(), i11 + i33);
//		};
//
//		auto session = std::make_shared<msgpack::rpc::TcpSession>(client_io, nullptr);
//		session->asyncConnect(boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), PORT));
//		session->call(on_result, "add", i11, i33);
//
//		boost::timer t;
//		int i1 = 1, i3 = 3;
//		for (int i = 0; i < count; i++)
//			BOOST_CHECK_EQUAL(session->call("add", i1, i3).get().as<int>(), i1 + i3);
//		session->waitforFinish();
//		std::cout << "async call ok: " << t.elapsed() << std::endl;
//	}
//	catch (const std::exception& e)
//	{
//		std::cerr << "call failed: " << e.what() << std::endl;
//	}
//
//	pWork.reset();
//	clinet_thread.join();
//	std::cout << "END OnewayCall" << std::endl << std::endl;
//}

BOOST_AUTO_TEST_CASE(TwowayCall)
{
	std::cout << "BGN TwowayCall" << std::endl;

	boost::asio::io_service client_io;
	auto pWork = std::make_shared<boost::asio::io_service::work>(client_io);
	boost::thread clinet_thread([&client_io]() { client_io.run(); });

	try
	{
		std::shared_ptr<msgpack::rpc::Dispatcher> dispatcher = std::make_shared<msgpack::rpc::Dispatcher>();
		dispatcher->add_handler("add", &clientadd);
		auto session = std::make_shared<msgpack::rpc::TcpSession>(client_io, dispatcher);
		session->asyncConnect(boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), PORT));

		int i1 = 1, i3 = 3;
		auto on_result = [i1, i3](boost::shared_future<msgpack::object> fut)
		{
			BOOST_CHECK_EQUAL(fut.get().as<int>(), i1 + i3);
		};

		boost::timer t;
		for (int i = 0; i < count; i++)
		{
			session->call(on_result, "twowayAdd", i1, i3);

			int i11 = 11, i33 = 33;
			BOOST_CHECK_EQUAL(session->call("twowayAdd", i11, i33).get().as<int>(), i11 + i33);
		}
		session->waitforFinish();
		std::cout << t.elapsed() << std::endl;
	}
	catch (const std::exception& e)
	{
		std::cerr << "call failed: " << e.what() << std::endl;
	}

	pWork.reset();
	clinet_thread.join();
	std::cout << "END TwowayCall" << std::endl << std::endl;
}

BOOST_AUTO_TEST_CASE(end)
{
	std::cout << "enter something to exit test: ";
	std::string str;
	std::cin >> str;
}