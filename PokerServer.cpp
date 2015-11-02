// PokerServer.cpp : 定义控制台应用程序的入口点。 //

#include "TcpConnection.h"
#include "TcpSession.h"
#include "TcpServer.h"
#include "SessionManager.h"

class Cont
{
public:
	typedef std::function<void(boost::future<msgpack::object>)> ThenFunc;
	ThenFunc _func;

	Cont()
	{
		_func = std::bind(&Cont::finish, this, std::placeholders::_1);
	}

	void finish(boost::future<msgpack::object> result)
	{
		try
		{
			int ret;
			result.get().convert(&ret);
			std::cout << "server: add, 1, 2 = " << ret << std::endl;
		}
		catch (const std::exception& e)
		{
			std::cout << "server: exception =" << e.what() << std::endl;
		}
	}
};

int clientadd(int a, int b)
{
	std::cout << "client: handle add, " << a << " + " << b << std::endl;
	return a + b;
}

boost::shared_future<void> thenFuture;

int serveradd(int a, int b)
{
	//std::cout << "server: handle add, " << a << " + " << b << std::endl;
	//auto sessionPool = msgpack::rpc::SessionManager::instance()->getSessionPool();
	//for (auto session : sessionPool)
	//{
	//	if (session->isConnected())
	//		thenFuture = session->call("add", 2, 2).then([&](boost::future<msgpack::object> result)
	//	{
	//		try
	//		{
	//			int ret;
	//			result.get().convert(&ret);
	//			std::cout << "server: add, 2, 2 = " << ret << std::endl;
	//		}
	//		catch (const std::exception& e)
	//		{
	//			std::cout << "server: exception =" << e.what() << std::endl;
	//		}
	//	});
	//}
	return a + b;
}

int main()
{
	std::cout << "start server..." << std::endl;
	boost::asio::io_service server_io;
	msgpack::rpc::TcpServer server(server_io, 8070);

	std::shared_ptr<msgpack::rpc::Dispatcher> dispatcher = std::make_shared<msgpack::rpc::Dispatcher>();
	dispatcher->add_handler("add", &serveradd);

	server.setDispatcher(dispatcher);
	server.start();

	server_io.run();
	return 0;
}