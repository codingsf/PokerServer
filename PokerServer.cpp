// PokerServer.cpp : 定义控制台应用程序的入口点。 //

#include "TcpConnection.h"
#include "TcpSession.h"
#include "TcpServer.h"
#include "SessionManager.h"
#include "TcpClient.h"

 void on_result(msgpack::rpc::AsyncCallCtx* result)
{
	int ret;
	result->convert(&ret);
	std::cout << "on_result =" << ret << std::endl;
}

int serveradd(int a, int b)
{
	std::cout <<"handle add, " << a << " + " << b << std::endl;
	auto sessionPool = msgpack::rpc::SessionManager::instance()->getSessionPool();
 	for (auto session : sessionPool)
	{
		if (session->isConnected())
			auto request2 = session->asyncCall(&on_result,"clientadd", 2, 2);
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

	// avoid stop client_io when client closed
	boost::asio::io_service::work work(client_io);

	msgpack::rpc::TcpClient client(client_io);
	client.asyncConnect(boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), PORT));
	boost::thread clinet_thread([&client_io]() { client_io.run(); });

	// sync request
	int result1;
	std::cout << "add, 1, 2 = " << client.syncCall(&result1, "add", 1, 2) << std::endl;

	// close
	client.close();

	// reconnect
	client.asyncConnect(boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), PORT));

	// request callback
	auto on_result = [](msgpack::rpc::AsyncCallCtx* result)
	{
		int result2;
		std::cout << "add, 3, 4 = " << result->convert(&result2) << std::endl;
	};
	auto result2 = client.asyncCall(on_result, "add", 3, 4);

	// block
	result2->sync();

	// stop asio
	client_io.stop();
	clinet_thread.join();

	server_io.stop();
	server_thread.join();
	return 0;
}