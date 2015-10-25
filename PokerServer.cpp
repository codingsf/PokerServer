// PokerServer.cpp : 定义控制台应用程序的入口点。 //

//#include "TcpConnection.h"
//#include "dispatcher.h"
#include "TcpConnection.h"
 #include "TcpSession.h"
 #include "TcpServer.h"

 void on_result(msgpack::rpc::asio::AsyncCallCtx* result)
{
	int ret;
	result->convert(&ret);
	std::cout << "on_result =" << ret << std::endl;
}

int serveradd(int a, int b)
{
	std::cout <<"handle add, " << a << " + " << b << std::endl;
 	for (auto session : msgpack::rpc::asio::TcpServer::_sessions)
	{
		if (session->is_connect())
		auto request2 = session->asyncCall(&on_result,"clientadd", 2, 2);
	}
	return a + b;
}

int main()
{
	boost::asio::io_service server_io;
	msgpack::rpc::asio::TcpServer server(server_io, 8070);
	server.add_handler("serveradd", &serveradd);
	server.start();server_io.run();
	return 0;
}