#include <boost/test/unit_test.hpp> 
#include "TcpConnection.h"
#include "TcpSession.h"
#include "TcpServer.h"
#include "SessionManager.h"
#include <iostream>
#include <boost/timer.hpp>
#include "define.h"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
using namespace msgpack::rpc;
using namespace boost::asio::ip;

extern int count;
extern tcp::endpoint peer;// = tcp::endpoint(address::from_string("127.0.0.1"), PORT);

std::vector<std::string> vecUuid;
BOOST_AUTO_TEST_CASE(getUuid)
{
	boost::timer t;
	boost::uuids::random_generator gen;
	for (int i = 0; i < count; i++)
	{
		boost::uuids::uuid u = gen();
		vecUuid.push_back(boost::to_string(u));
	}
	//std::cout << "生成uuid用时:" << t.elapsed() << std::endl;
}

BOOST_AUTO_TEST_CASE(uuidSyncCall)
{
	std::cout << "BGN 同步Uuid" << std::endl;

	boost::asio::io_service client_io;
	boost::asio::io_service::work work(client_io);
	boost::thread clinet_thread([&client_io]() { client_io.run(); });
	auto peer = tcp::endpoint(address::from_string("127.0.0.1"), PORT);
	try
	{
		boost::timer t;
		auto ses = std::make_shared<msgpack::rpc::TcpSession>(client_io, nullptr);
		ses->asyncConnect(peer).get();
		for (int i = 0; i < count; i++)
		{
			auto fut = ses->call(std::string("setUuid"), vecUuid[i]);
			BOOST_CHECK(fut.get().first.is_nil());
			auto fut2 = ses->call(std::string("getUuid"));
			BOOST_CHECK_EQUAL(fut2.get().first.as<std::string>(), vecUuid[i]);
		}
		std::cout << "同一连接:	" << t.elapsed() << "秒" << std::endl;

		t.restart();
		for (int i = 0; i < count; i++)
		{
			try
			{
				auto ses = std::make_shared<msgpack::rpc::TcpSession>(client_io, nullptr);
				ses->asyncConnect(peer).get();
				auto fut = ses->call(std::string("setUuid"), vecUuid[i]);
				BOOST_CHECK(fut.get().first.is_nil());
				auto fut2 = ses->call(std::string("getUuid"));
				BOOST_CHECK_EQUAL(fut2.get().first.as<std::string>(), vecUuid[i]);
			}
			catch (const boost::exception& e) { std::cerr << "调用异常：" << *boost::get_error_info<err_no>(e) << "	" << *boost::get_error_info<err_str>(e) << std::endl; }
		}
		std::cout << "每次连接:	" << t.elapsed() << "秒" << std::endl;
	}
	catch (msgpack::type_error& err) { std::cerr << "主流程异常：" << err.what() << std::endl; }
	catch (const boost::exception& e) { std::cerr << "主流程异常：" << diagnostic_information(e) << std::endl; }
	catch (const std::exception& e) { std::cerr << "主流程异常：" << e.what() << std::endl; }
	catch (...) { std::cerr << "主流程未知异常" << std::endl; }

	client_io.stop();
	clinet_thread.join();
	std::cout << "END 同步Uuid" << std::endl << std::endl;
}

extern int done;
void OnGetUuid(SessionPtr pSes, std::string uuid, boost::shared_future<ObjectZone> fut)
{
	try
	{
		BOOST_CHECK_EQUAL(fut.get().first.as<std::string>(), uuid);
		done++;
	}
	catch (const boost::exception& e)
	{
		std::cerr << "异常结果：" << *boost::get_error_info<err_no>(e) << "	" << *boost::get_error_info<err_str>(e) << std::endl;
	}
	pSes->close();
}

void OnSetUuid(SessionPtr pSes, std::string uuid, boost::shared_future<ObjectZone> fut)
{
	try
	{
		BOOST_CHECK(fut.get().first.is_nil());
		pSes->call(std::bind(OnGetUuid, pSes, uuid, std::placeholders::_1), "getUuid");
	}
	catch (const boost::exception& e)
	{
		pSes->close();
		std::cerr << "异常结果：" << *boost::get_error_info<err_no>(e) << "	" << *boost::get_error_info<err_str>(e) << std::endl;
	}
}

BOOST_AUTO_TEST_CASE(uuidAsyncCall)
{
	std::cout << "BGN 异步Uuid" << std::endl;

	boost::asio::io_service client_io;
	auto peer = tcp::endpoint(address::from_string("127.0.0.1"), PORT);

	try
	{
		done = 0;
		boost::timer t;
		std::vector<SessionPtr> vecSession;
		for (int i = 0; i < count; i++)
		{
			std::string uuid = vecUuid[i];
			SessionPtr ses = std::make_shared<msgpack::rpc::TcpSession>(client_io, nullptr);
			auto weak = std::weak_ptr<TcpSession>(ses);
			vecSession.push_back(ses);

			auto OnConnect = [uuid, weak](ConnectionStatus status)	// 防止TcpSession和TcpConnection相互引用对方的shared_ptr
							{
								auto shared = weak.lock();
								if (!shared)
									return;
								if (status == connection_connected)
									shared->call(std::bind(OnSetUuid, shared, uuid, std::placeholders::_1), "setUuid", uuid);
							};

			ses->asyncConnect(peer, OnConnect);
		}

		client_io.run();
		std::cout << "每次连接:	" << t.elapsed() << "秒		成功: " << done << "次" << std::endl;
	}
	catch (const std::exception& e)
	{
		std::cerr << "call failed: " << e.what() << std::endl;
	}
	std::cout << "END 异步Uuid" << std::endl << std::endl;
}

//BOOST_AUTO_TEST_CASE(end)
//{
//	std::cout << "exit?: ";
//	std::string str;
//	std::cin >> str;
//}