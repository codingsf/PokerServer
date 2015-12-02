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
				BOOST_CHECK_EQUAL(fut.get().first.as<int>(), a + b);
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
				std::cerr << "δ֪�쳣" << std::endl;
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
	std::cout << "exit? ";
	std::cin >> s;
	server.stop();
}