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
auto peer = tcp::endpoint(address::from_string("127.0.0.1"), PORT);
std::mutex _mutex;

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
//	auto peer = tcp::endpoint(address::from_string("127.0.0.1"), PORT);
//	{
//		boost::timer t;
//		for (int i = 0; i < count; i++)
//		{
//			auto session = std::make_shared<msgpack::rpc::TcpSession>(client_io, nullptr);
//			session->asyncConnect(peer);
//		}
//		std::cout << t.elapsed() << std::endl;
//	}
//
//	{
//		boost::timer t;
//		auto session = std::make_shared<msgpack::rpc::TcpSession>(client_io, nullptr);
//		for (int i = 0; i < count; i++)
//		{
//			session->asyncConnect(peer);
//		}
//		std::cout << t.elapsed() << std::endl;
//	}
//
//	pWork.reset();
//	clinet_thread.join();
//	std::cout << "END connect_close" << std::endl << std::endl;
//}

//BOOST_AUTO_TEST_CASE(SyncCall)
//{
//	std::cout << "BGN 同步调用" << std::endl;
//
//	boost::asio::io_service client_io;
//	boost::asio::io_service::work work(client_io);
//	boost::thread clinet_thread([&client_io]() { client_io.run(); });
//	auto peer = tcp::endpoint(address::from_string("127.0.0.1"), PORT);
//	try
//	{
//		boost::timer t;
//		auto ses = std::make_shared<msgpack::rpc::TcpSession>(client_io, nullptr);
//		ses->asyncConnect(peer).get();
//		for (int i = 0; i < count; i++)
//		{
//			auto fut = ses->call("add", i, i);
//			BOOST_CHECK_EQUAL(fut.get().as<int>(), i + i);
//		}
//		std::cout << "同步，同一连接: " << t.elapsed() << std::endl;
//
//		t.restart();
//		for (int i = 0; i < count; i++)
//		{
//			try
//			{
//				auto ses = std::make_shared<msgpack::rpc::TcpSession>(client_io, nullptr);
//				ses->asyncConnect(peer).get();
//				auto fut = ses->call("add", i, i);
//				BOOST_CHECK_EQUAL(fut.get().as<int>(), i + i);
//			}
//			catch (const boost::exception& e) { std::cerr << "调用异常：" << *boost::get_error_info<err_no>(e) << "	" << *boost::get_error_info<err_str>(e) << std::endl; }
//		}
//		std::cout << "同步，每次连接: " << t.elapsed() << std::endl;
//	}
//	catch (const boost::exception& e){std::cerr << "主流程异常：" << diagnostic_information(e);}
//	catch (const std::exception& e){std::cerr << "主流程异常：" << e.what();}
//	catch (...){std::cerr << "主流程未知异常";}
//
//	client_io.stop();
//	clinet_thread.join();
//	std::cout << "END 同步调用" << std::endl << std::endl;
//}

//std::atomic<int> done = 0;
int done = 0;
void OnResult(SessionPtr pSes, int i, boost::shared_future<msgpack::object> fut)
{
	try
	{
		BOOST_CHECK_EQUAL(fut.get().as<int>(), i + i);
		if (pSes)
			pSes->close();
		done++;
	}
	catch (const boost::exception& e)
	{
		std::cerr << "异常结果：" << *boost::get_error_info<err_no>(e) << "	" << *boost::get_error_info<err_str>(e) << std::endl;
	}
}

void oneConnManyCall(const std::string& func)
{
	boost::asio::io_service client_io;
	boost::asio::io_service::work work(client_io);
	boost::thread clinet_thread([&client_io]() { client_io.run(); });

	std::shared_ptr<msgpack::rpc::Dispatcher> dispatcher = std::make_shared<msgpack::rpc::Dispatcher>();
	dispatcher->add_handler("add", &clientadd);

	try
	{
		done = 0;
		boost::timer t;
		auto ses = std::make_shared<msgpack::rpc::TcpSession>(client_io, dispatcher);
		if (ses->connect(peer))
		{
			for (int i = 0; i < count; i++)
				ses->call(std::bind(OnResult, nullptr, i, std::placeholders::_1), func, i, i);
			ses->waitforFinish();
		}
		std::cout << "异步，同一连接，成功" << done << "次，用时" << t.elapsed() << std::endl;
	}
	catch (const boost::exception& e) { std::cerr << "主流程异常：" << diagnostic_information(e); }
	catch (const std::exception& e) { std::cerr << "主流程异常：" << e.what(); }
	catch (...) { std::cerr << "主流程未知异常"; }

	client_io.stop();
	clinet_thread.join();
}

void oneConnOneCall(const std::string& func)
{

	boost::asio::io_service client_io;
	boost::asio::io_service::work work(client_io);
	boost::thread clinet_thread1([&client_io]() { client_io.run(); });
	boost::thread clinet_thread2([&client_io]() { client_io.run(); });
	auto peer = tcp::endpoint(address::from_string("127.0.0.1"), PORT);
	std::shared_ptr<msgpack::rpc::Dispatcher> dispatcher = std::make_shared<msgpack::rpc::Dispatcher>();
	dispatcher->add_handler("add", &clientadd);

	try
	{
		done = 0;
		boost::timer t;
		std::vector<SessionPtr> vec;
		for (int i = 0; i < count; i++)
		{
			SessionPtr ses = std::make_shared<msgpack::rpc::TcpSession>(client_io, dispatcher);
			if (ses->connect(peer))
				ses->call(std::bind(OnResult, nullptr, i, std::placeholders::_1), func, i, i);
			vec.push_back(ses);
		}
		for (auto item : vec)
			item->waitforFinish();
		std::cout << "异步，每次连接，成功" << done << "次，用时" << t.elapsed() << std::endl;
	}
	catch (const boost::exception& e) { std::cerr << "主流程异常：" << diagnostic_information(e); }
	catch (const std::exception& e) { std::cerr << "主流程异常：" << e.what(); }
	catch (...) { std::cerr << "主流程未知异常"; }

	client_io.stop();
	clinet_thread1.join();
	clinet_thread1.join();
}

//BOOST_AUTO_TEST_CASE(AsyncCall)
//{
//	std::cout << "BGN 异步调用" << std::endl;
//	oneConnManyCall("add");
//	oneConnOneCall("add");
//	std::cout << "END 异步调用" << std::endl << std::endl;
//}

//BOOST_AUTO_TEST_CASE(TwowayCall)
//{
//	std::cout << "BGN 双向调用" << std::endl;
//	oneConnManyCall("twowayAdd");
//	oneConnOneCall("twowayAdd");
//	std::cout << "END 双向调用" << std::endl;
//}

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

//void OnSetUuid(boost::shared_future<msgpack::object> fut)
//{
//	try
//	{
//		BOOST_CHECK(fut.get().is_nil());
//	}
//	catch (const boost::exception& e)
//	{
//		std::cerr << "异常结果：" << *boost::get_error_info<err_no>(e) << "	" << *boost::get_error_info<err_str>(e) << std::endl;
//	}
//}

void OnGetUuid(SessionPtr pSes, std::string str, boost::shared_future<msgpack::object> fut)
{
	try
	{
		BOOST_CHECK_EQUAL(fut.get().as<std::string>(), str);
		if (pSes)
			pSes->close();
		done++;
	}
	catch (const boost::exception& e)
	{
		std::cerr << "异常结果：" << *boost::get_error_info<err_no>(e) << "	" << *boost::get_error_info<err_str>(e) << std::endl;
	}
}

BOOST_AUTO_TEST_CASE(uuid)
{
	std::cout << "BGN uuid" << std::endl;

	boost::asio::io_service client_io;
	boost::asio::io_service::work work(client_io);
	boost::thread clinet_thread([&client_io]() { client_io.run(); });
	auto peer = tcp::endpoint(address::from_string("127.0.0.1"), PORT);

	try
	{
		boost::timer t;
		std::vector<std::string> vecUuid;
		boost::uuids::random_generator gen;
		for (int i = 0; i < count; i++)
		{
			boost::uuids::uuid u = gen();		
			vecUuid.push_back(boost::to_string(u));
		}
		std::cout << "uuid，生成，用时" << t.elapsed() << std::endl;

		done = 0;
		t.restart();
		std::vector<SessionPtr> vecSession;
		for (int i = 0; i < count; i++)
		{
			std::string uuid = vecUuid[i];
			SessionPtr ses = std::make_shared<msgpack::rpc::TcpSession>(client_io, nullptr);
			vecSession.push_back(ses);

			auto OnConnect = [uuid, ses](ConnectionStatus status)
							{
								auto OnSet = [uuid, ses](boost::shared_future<msgpack::object> fut)
								{
									try
									{
										BOOST_CHECK(fut.get().is_nil());
										ses->call(std::bind(OnGetUuid, ses, uuid, std::placeholders::_1), "getUuid");
									}
									catch (const boost::exception& e)
									{
										std::cerr << "异常结果：" << *boost::get_error_info<err_no>(e) << "	" << *boost::get_error_info<err_str>(e) << std::endl;
									}
								};
								if (status == connection_connected)
									ses->call(OnSet, "setUuid", uuid);
							};

			ses->asyncConnect(peer, OnConnect);
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10000));
		for (auto item : vecSession)
			item->waitforFinish();
		std::cout << "uuid，成功" << done << "次，用时" << t.elapsed() << std::endl;
	}
	catch (const std::exception& e)
	{
		std::cerr << "call failed: " << e.what() << std::endl;
	}

	client_io.stop();
	clinet_thread.join();
	std::cout << "END uuid" << std::endl << std::endl;
}

BOOST_AUTO_TEST_CASE(end)
{
	std::cout << "enter something to exit test: ";
	std::string str;
	std::cin >> str;
}