#include <boost/test/unit_test.hpp> 
#include "TcpConnection.h"
#include "TcpSession.h"
#include "TcpServer.h"
#include "SessionManager.h"
#include <iostream>
#include <boost/timer.hpp>
#include "main.h"
using namespace msgpack::rpc;
using namespace boost::asio::ip;
namespace utf = boost::unit_test;

int count = 1;

BOOST_AUTO_TEST_CASE(begin)
{
	BufferManager::instance();

	std::cout << "enter repeat times: ";
	std::cin >> count;
	std::cout << std::endl << std::endl;
}

BOOST_AUTO_TEST_CASE(tc_connect_close, *utf::enable_if<enable_connect_close>())
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
			session->asyncConnect(peer);
		}
		std::cout << t.elapsed() << std::endl;
	}

	{
		boost::timer t;
		auto session = std::make_shared<msgpack::rpc::TcpSession>(client_io, nullptr);
		for (int i = 0; i < count; i++)
		{
			session->asyncConnect(peer);
		}
		std::cout << t.elapsed() << std::endl;
	}

	pWork.reset();
	clinet_thread.join();
	std::cout << "END connect_close" << std::endl << std::endl;
}

BOOST_AUTO_TEST_CASE(tc_sync_call, *utf::enable_if<enable_sync_call>())
{
	std::cout << "BGN ͬ��add" << std::endl;

	boost::asio::io_service client_io;
	boost::asio::io_service::work work(client_io);
	boost::thread clinet_thread([&client_io]() { client_io.run(); });
	try
	{
		boost::timer t;
		auto ses = std::make_shared<msgpack::rpc::TcpSession>(client_io, nullptr);
		ses->asyncConnect(peer).get();
		for (int i = 0; i < count; i++)
		{
			auto fut = ses->call("add", i, i);
			BOOST_CHECK_EQUAL(fut.get().first.as<int>(), i + i);
		}
		std::cout << "ͬһ����:	" << t.elapsed() << "��" << std::endl;

		t.restart();
		for (int i = 0; i < count; i++)
		{
			try
			{
				auto ses = std::make_shared<msgpack::rpc::TcpSession>(client_io, nullptr);
				ses->asyncConnect(peer).get();
				auto fut = ses->call("add", i, i);
				BOOST_CHECK_EQUAL(fut.get().first.as<int>(), i + i);
			}
			catch (const boost::exception& e) { std::cerr << "�����쳣��" << *boost::get_error_info<err_no>(e) << "	" << *boost::get_error_info<err_str>(e) << std::endl; }
		}
		std::cout << "ÿ������:	" << t.elapsed() << "��" << std::endl;
	}
	catch (const boost::exception& e){std::cerr << "�������쳣��" << diagnostic_information(e) << std::endl;}
	catch (const std::exception& e){std::cerr << "�������쳣��" << e.what() << std::endl;}
	catch (...){std::cerr << "������δ֪�쳣" << std::endl;}

	client_io.stop();
	clinet_thread.join();
	std::cout << "END ͬ��add" << std::endl << std::endl;
}

void OnResult(SessionPtr pSes, int i, boost::shared_future<ObjectZone> fut)
{
	try
	{
		BOOST_CHECK_EQUAL(fut.get().first.as<int>(), i + i);
		if (pSes)
			pSes->close();
		done++;
	}
	catch (const boost::exception& e)
	{
		std::cerr << "�쳣�����" << *boost::get_error_info<err_no>(e) << "	" << *boost::get_error_info<err_str>(e) << std::endl;
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
		std::cout << "ͬһ����:	" << t.elapsed() << "��		�ɹ�: " << done << "��" << std::endl;
	}
	catch (const boost::exception& e) { std::cerr << "�������쳣��" << diagnostic_information(e) << std::endl; }
	catch (const std::exception& e) { std::cerr << "�������쳣��" << e.what() << std::endl; }
	catch (...) { std::cerr << "������δ֪�쳣" << std::endl; }

	client_io.stop();
	clinet_thread.join();
}

void oneConnOneCall(const std::string& func)
{
	boost::asio::io_service client_io;
	boost::asio::io_service::work work(client_io);
	boost::thread clinet_thread1([&client_io]() { client_io.run(); });
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
		std::cout << "ÿ������:	" << t.elapsed() << "��		�ɹ�: " << done << "��" << std::endl;
	}
	catch (const boost::exception& e) { std::cerr << "�������쳣��" << diagnostic_information(e) << std::endl; }
	catch (const std::exception& e) { std::cerr << "�������쳣��" << e.what() << std::endl; }
	catch (...) { std::cerr << "������δ֪�쳣" << std::endl; }

	client_io.stop();
	clinet_thread1.join();
}

BOOST_AUTO_TEST_CASE(tc_async_oneway, *utf::enable_if<enable_async_oneway>())
{
	std::cout << "BGN �첽add" << std::endl;
	oneConnManyCall("add");
	oneConnOneCall("add");
	std::cout << "END �첽add" << std::endl << std::endl;
}

BOOST_AUTO_TEST_CASE(tc_async_twoway, *utf::enable_if<enable_async_twoway>())
{
	std::cout << "BGN �첽twowayAdd" << std::endl;
	oneConnManyCall("twowayAdd");
	oneConnOneCall("twowayAdd");
	std::cout << "END �첽twowayAdd" << std::endl << std::endl;
}
