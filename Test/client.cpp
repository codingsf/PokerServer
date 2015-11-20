#include <boost/test/unit_test.hpp> 
#include "TcpConnection.h"
#include "TcpSession.h"
#include "TcpServer.h"
#include "SessionManager.h"
#include <iostream>
#include <boost/timer.hpp>
#include "define.h"
using namespace msgpack::rpc;
using namespace boost::asio::ip;
int count = 1;
std::mutex ioMutex;

BOOST_AUTO_TEST_CASE(begin)
{
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
//			session->asyncConnect(tcp::endpoint(address::from_string("127.0.0.1"), PORT));
//		}
//		std::cout << t.elapsed() << std::endl;
//	}
//
//	{
//		boost::timer t;
//		auto session = std::make_shared<msgpack::rpc::TcpSession>(client_io, nullptr);
//		for (int i = 0; i < count; i++)
//		{
//			session->asyncConnect(tcp::endpoint(address::from_string("127.0.0.1"), PORT));
//		}
//		std::cout << t.elapsed() << std::endl;
//	}
//
//	pWork.reset();
//	clinet_thread.join();
//	std::cout << "END connect_close" << std::endl << std::endl;
//}

BOOST_AUTO_TEST_CASE(SyncCall)
{
	std::cout << "BGN 同步调用" << std::endl;

	boost::asio::io_service client_io;
	boost::asio::io_service::work work(client_io);
	boost::thread clinet_thread([&client_io]() { client_io.run(); });

	try
	{
		boost::timer t;
		auto ses = std::make_shared<msgpack::rpc::TcpSession>(client_io, nullptr);
		ses->asyncConnect(tcp::endpoint(address::from_string("127.0.0.1"), PORT));
		for (int i = 0; i < count; i++)
		{
			try
			{
				auto fut = ses->call("add", i, i);
				BOOST_CHECK_EQUAL(fut.get().as<int>(), i + i);
			}
			catch (const boost::exception& e) { std::cerr << "调用异常：" << *boost::get_error_info<err_no>(e) << *boost::get_error_info<err_str>(e); }
		}
		std::cout << "同步，同一连接: " << t.elapsed() << std::endl;

		t.restart();
		for (int i = 0; i < count; i++)
		{
			try
			{
				auto ses = std::make_shared<msgpack::rpc::TcpSession>(client_io, nullptr);
				ses->asyncConnect(tcp::endpoint(address::from_string("127.0.0.1"), PORT));
				auto fut = ses->call("add", i, i);
				BOOST_CHECK_EQUAL(fut.get().as<int>(), i + i);
			}
			catch (const boost::exception& e) { std::cerr << "调用异常：" << *boost::get_error_info<err_no>(e) << *boost::get_error_info<err_str>(e); }
		}
		std::cout << "同步，每次连接: " << t.elapsed() << std::endl;
	}
	catch (const boost::exception& e){std::cerr << "主流程异常：" << diagnostic_information(e);}
	catch (const std::exception& e){std::cerr << "主流程异常：" << e.what();}
	catch (...){std::cerr << "主流程未知异常";}

	client_io.stop();
	clinet_thread.join();
	std::cout << "END 同步调用" << std::endl << std::endl;
}

int done = 0;
void OnResult(int i, boost::shared_future<msgpack::object> fut)
{
	try
	{
		BOOST_CHECK_EQUAL(fut.get().as<int>(), i + i);
		done++;
	}
	catch (const boost::exception& e)
	{
		auto no = boost::get_error_info<err_no>(e);
		auto str = boost::get_error_info<err_str>(e);
		std::unique_lock<std::mutex> lck(ioMutex);
		std::cerr << "异常结果：" << (no ? *no : 0) << "	" << (str ? *str : "") << std::endl;
	}
}

BOOST_AUTO_TEST_CASE(AsyncCall)
{
	std::cout << "BGN 异步调用" << std::endl;
	using std::placeholders::_1;

	boost::asio::io_service client_io;
	boost::asio::io_service::work work(client_io);
	boost::thread clinet_thread([&client_io]() { client_io.run(); });

	try
	{
		done = 0;
		boost::timer t;
		auto session4 = std::make_shared<msgpack::rpc::TcpSession>(client_io, nullptr);
		session4->asyncConnect(tcp::endpoint(address::from_string("127.0.0.1"), PORT));
		for (int i = 0; i < count; i++)
			session4->call(std::bind(OnResult, i, _1), "add", i, i);
		session4->waitforFinish();
		ioMutex.lock();
		std::cout << "异步，同一连接，成功" << done << "次，用时" << t.elapsed() << std::endl;
		ioMutex.unlock();

		done = 0;
		t.restart();
		std::vector<SessionPtr> vec;
		for (int i = 0; i < count; i++)
		{
			SessionPtr ses = std::make_shared<msgpack::rpc::TcpSession>(client_io, nullptr);
			ses->asyncConnect(tcp::endpoint(address::from_string("127.0.0.1"), PORT));
			vec.push_back(ses);
		}
		for (int i = 0; i < count; i++)
			vec[i]->call(std::bind(OnResult, i, _1), "add", i, i);
		for (auto item : vec)
			item->waitforFinish();
		vec.clear();
		ioMutex.lock();
		std::cout << "异步，每次连接，成功" << done << "次，用时" << t.elapsed() << std::endl;
		ioMutex.unlock();
	}
	catch (const boost::exception& e) { std::cerr << "主流程异常：" << diagnostic_information(e); }
	catch (const std::exception& e) { std::cerr << "主流程异常：" << e.what(); }
	catch (...) { std::cerr << "主流程未知异常"; }

	client_io.stop();
	clinet_thread.join();
	std::cout << "END 异步调用" << std::endl << std::endl;
}

BOOST_AUTO_TEST_CASE(TwowayCall)
{
	std::cout << "BGN TwowayCall" << std::endl;

	boost::asio::io_service client_io;
	boost::asio::io_service::work work(client_io);
	boost::thread clinet_thread([&client_io]() { client_io.run(); });

	try
	{
		std::shared_ptr<msgpack::rpc::Dispatcher> dispatcher = std::make_shared<msgpack::rpc::Dispatcher>();
		dispatcher->add_handler("add", &clientadd);
		auto session = std::make_shared<msgpack::rpc::TcpSession>(client_io, dispatcher);
		session->asyncConnect(tcp::endpoint(address::from_string("127.0.0.1"), PORT));

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

	client_io.stop();
	clinet_thread.join();
	std::cout << "END TwowayCall" << std::endl << std::endl;
}

BOOST_AUTO_TEST_CASE(end)
{
	std::cout << "enter something to exit test: ";
	std::string str;
	std::cin >> str;
}