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
	std::cout <<"server: handle add, " << a << " + " << b << std::endl;
	auto sessionPool = msgpack::rpc::SessionManager::instance()->getSessionPool();
 	for (auto session : sessionPool)
	{
		if (session->isConnected())
			thenFuture = session->call("add", 2, 2).then([&](boost::future<msgpack::object> result)
		{
			try
			{
				int ret;
				result.get().convert(&ret);
				std::cout << "server: add, 2, 2 = " << ret << std::endl;
			}
			catch (const std::exception& e)
			{
				std::cout << "server: exception =" << e.what() << std::endl;
			}
		});
	}
	return a + b;
}

int main()
{
	const static int PORT = 8070;

	// server
	boost::asio::io_service server_io;
	msgpack::rpc::TcpServer server(server_io, PORT);

	std::shared_ptr<msgpack::rpc::Dispatcher> dispatcher = std::make_shared<msgpack::rpc::Dispatcher>();
	dispatcher->add_handler("add", &serveradd);
	dispatcher->add_handler("mul", [](float a, float b)->float { return a*b; });

	server.setDispatcher(dispatcher);
	server.start();	
	std::thread server_thread([&server_io]() { server_io.run(); });

	// client
	boost::asio::io_service client_io;	
	boost::asio::io_service::work work(client_io);	// avoid stop client_io when client closed
	std::shared_ptr<msgpack::rpc::Dispatcher> disp = std::make_shared<msgpack::rpc::Dispatcher>();
	disp->add_handler("add", &clientadd);

	auto session = std::make_shared<msgpack::rpc::TcpSession>(client_io, dispatcher);
	session->setDispatcher(disp);
	session->asyncConnect(boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), PORT));
	boost::thread clinet_thread([&client_io]() { client_io.run(); });

	// sync call
	std::cout << "client: add, 1, 2 = " << session->call("add", 1, 2).get().as<int>() << std::endl;

	// close
	session->close();
	// reconnect
	session->asyncConnect(boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), PORT));

	// async call
	auto fut = session->call("add", 3, 4).then([&](boost::future<msgpack::object> result)
	{
		std::cout << "client: add, 3, 4 = " << result.get().as<int>() << std::endl;

		// stop asio
		client_io.stop();
	});

	clinet_thread.join();

	server_io.stop();
	server_thread.join();
	return 0;
}