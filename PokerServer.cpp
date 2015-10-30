// PokerServer.cpp : 定义控制台应用程序的入口点。 //

#include "TcpConnection.h"
#include "TcpSession.h"
#include "TcpServer.h"
#include "SessionManager.h"

 void on_result(msgpack::rpc::AsyncCallCtx* result)
{
	int ret;
	result->convert(&ret);
	std::cout << "on_result =" << ret << std::endl;
}

int serveradd(int a, int b)
{
	std::cout <<"handle add, " << a << " + " << b << std::endl;
	//auto sessionPool = msgpack::rpc::SessionManager::instance()->getSessionPool();
 //	for (auto session : sessionPool)
	//{
	//	if (session->isConnected())
	//		auto callFuture = session->call("clientadd", 2, 2).then([&](boost::future<msgpack::object> result)
	//	{
	//		int ret;
	//		result.get().convert(&ret);
	//		std::cout << "on_result =" << ret << std::endl;
	//	});
	//}
	return a + b;
}

int main()
{
	const static int PORT = 8070;

	// server
	//boost::asio::io_service server_io;
	//msgpack::rpc::TcpServer server(server_io, PORT);

	//std::shared_ptr<msgpack::rpc::Dispatcher> dispatcher = std::make_shared<msgpack::rpc::Dispatcher>();
	//dispatcher->add_handler("add", &serveradd);
	//dispatcher->add_handler("mul", [](float a, float b)->float { return a*b; });

	//server.setDispatcher(dispatcher);
	//server.start();	
	//std::thread server_thread([&server_io]() { server_io.run(); });

	//// client
	//boost::asio::io_service client_io;	
	//boost::asio::io_service::work work(client_io);	// avoid stop client_io when client closed

	//msgpack::rpc::TcpSession client(client_io, dispatcher);
	//client.asyncConnect(boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), PORT));
	//boost::thread clinet_thread([&client_io]() { client_io.run(); });

	// sync call
	//auto obj = client.call("add", 1, 2).get();
	//std::cout << "add, 1, 2 = " << client.call("add", 1, 2).get().as<int>() << std::endl;

	// close
	//client.close();
	//// reconnect
	//client.asyncConnect(boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), PORT));

	// async call
	//auto fut = client.call("add", 3, 4).then([&](boost::future<msgpack::object> result)
	//{
	//	std::cout << "add, 3, 4 = " << result.get().as<int>() << std::endl;
	//});

	// stop asio
	//client_io.stop();
	//clinet_thread.join();

	//server_io.stop();
	//server_thread.join();
	return 0;
}