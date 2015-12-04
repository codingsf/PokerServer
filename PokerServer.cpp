#define BOOST_TEST_MODULE ServerTest
#include <boost/test/unit_test.hpp> 

#include "TcpConnection.h"
#include "TcpSession.h"
#include "TcpServer.h"
#include "SessionManager.h"
#include "ThreadPool.h"

int twowayAdd(int a, int b)
{
	auto session = msgpack::rpc::getCurrentTcpSession();
	if (session)
	{
		auto on_result = [a, b](boost::shared_future<msgpack::rpc::ObjectZone> fut)
		{
			try
			{
				int i = fut.get().first.as<int>();
				if (i != a + b)
					std::cerr << i << " != " << a << " + " << b << std::endl;

				BOOST_CHECK_EQUAL(i, a + b);	// 多线程下BOOST_CHECK_EQUAL的实现可能会出异常
			}
			catch (const boost::exception& e)
			{
				std::cerr << diagnostic_information(e);
			}
			catch (const std::exception& e)
			{
				std::cerr << e.what() << std::endl;
			}
			catch (...)
			{
				std::cerr << "twowayAdd未知异常" << std::endl;
			}
		};
		session->call(on_result, "add", a, b);
		//session->call(on_result, "shutdown_send");
	}
	return a + b;
}

void setUuid(std::string uuid)
{
	auto session = msgpack::rpc::getCurrentTcpSession();
	if (session)
	{
		session->_uuid = uuid;
	}
}

std::string getUuid()
{
	auto session = msgpack::rpc::getCurrentTcpSession();
	if (session)
		return session->_uuid;
	else
		return "";
}

BOOST_AUTO_TEST_CASE(begin)
{
	std::cout << "start server..." << std::endl;
	msgpack::rpc::TcpServer server(8070);

	std::shared_ptr<msgpack::rpc::Dispatcher> dispatcher = std::make_shared<msgpack::rpc::Dispatcher>();
	dispatcher->add_handler("echo", [](std::string str) { return str; });
	dispatcher->add_handler("add", [](int a, int b) { return a + b; });
	dispatcher->add_handler("twowayAdd", &twowayAdd);
	dispatcher->add_handler("setUuid", &setUuid);
	dispatcher->add_handler("getUuid", &getUuid);

	server.setDispatcher(dispatcher);
	server.start();

	std::string s;
	std::cout << "exit? " << std::endl;
	std::cin >> s;
	server.stop();
}