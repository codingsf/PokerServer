// PokerServer.cpp : 定义控制台应用程序的入口点。 //

#include "TcpConnection.h"
#include "TcpSession.h"
#include "TcpServer.h"
#include "SessionManager.h"

int mul(int a, int b)
{
	std::cout << "server: handle mul, " << a << " * " << b << std::endl;
	auto sessionPool = msgpack::rpc::SessionManager::instance()->getSessionPool();
	for (auto session : sessionPool)
	{
		if (session->isConnected())
		{
			auto fut = session->call("add", 2, 2);
			auto then = fut.then(
				[session](boost::future<msgpack::object> result)
				{
					try
					{
						std::cout << "server: 2 + 2 = " << result.get().as<int>() << std::endl;
						// session->delFuture(); 要在些函数返回时futThen才会is_ready
					}
					catch (const std::exception& e)
					{
						std::cout << "server: add exception: " << e.what() << std::endl;
					}
				});
			session->saveFuture(std::move(then));
		}
	}
	return a * b;
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
	dispatcher->add_handler("mul", &mul);

	server.setDispatcher(dispatcher);
	server.start();

	server_io.run();
	return 0;
}