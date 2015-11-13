// PokerServer.cpp : 定义控制台应用程序的入口点。 //

#include "TcpConnection.h"
#include "TcpSession.h"
#include "TcpServer.h"
#include "SessionManager.h"

int twowayAdd(int a, int b)
{
	std::cout << "handle twowayAdd, " << a << " + " << b << std::endl;
	auto sessionPool = msgpack::rpc::SessionManager::instance()->getSessionPool();
	for (auto session : sessionPool)
	{
		if (session->isConnected())
		{
			auto on_result = [](boost::shared_future<msgpack::object> fut)
			{
				try
				{
					std::cout << "call: 2 + 4 = " << fut.get().as<int>() << std::endl;
				}
				catch (const boost::exception& e)
				{
					std::cerr << diagnostic_information(e);
				}
			};
			session->call(on_result, "add", 2, 4);
			//session->call(on_result, "shutdown_send");
		}
	}
	return a + b;
}

int serveradd(int a, int b)
{
	//std::cout << "server: handle add, " << a << " + " << b << std::endl;
	return a + b;
}

int main()
{
	std::cout << "start server..." << std::endl;
	boost::asio::io_service server_io;
	msgpack::rpc::TcpServer server(server_io, 8070);

	std::shared_ptr<msgpack::rpc::Dispatcher> dispatcher = std::make_shared<msgpack::rpc::Dispatcher>();
	dispatcher->add_handler("add", &serveradd);
	dispatcher->add_handler("twowayAdd", &twowayAdd);

	server.setDispatcher(dispatcher);
	server.start();

	server_io.run();
	return 0;
}