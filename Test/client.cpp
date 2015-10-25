#define BOOST_TEST_MODULE msgpack-asiorpc

#include <boost/test/unit_test.hpp> 
#include "TcpClient.h"


int clientadd(int a, int b)
{
	std::cout << "handle add, " << a << " + " << b << std::endl;

	return a + b;
}


BOOST_AUTO_TEST_CASE(client)
{
	int PORT = 8070;
	boost::asio::io_service client_io;
	boost::asio::io_service::work work(client_io);

	msgpack::rpc::asio::TcpClient client(client_io);
	client.add_handler("clientadd", &clientadd);
	client.asyncConnect(boost::asio::ip::tcp::endpoint(
		boost::asio::ip::address::from_string("127.0.0.1"), PORT));
	boost::thread clinet_thread([&client_io](){ client_io.run(); });

	// sync request
	int result1;
	//std::cout << "add, 1, 2 = " << client.call_sync(&result1, "add", 1, 2) << std::endl;	// server没有add方法，会抛异常
	for (int i = 0; i < 100; i++)
		std::cout << "add, 1, 2 = " << client.syncCall(&result1, "serveradd", 1, 2) << std::endl;

	// close and reconnect
	client.close();
	client.asyncConnect(boost::asio::ip::tcp::endpoint(
	            boost::asio::ip::address::from_string("127.0.0.1"), PORT));

	// request callback
	auto on_result = [](msgpack::rpc::asio::AsyncCallCtx* result)
	{
		int result2;
		std::cout << "add, 3, 4 = " << result->convert(&result2) << std::endl;
	};
	auto result2 = client.asyncCall(on_result, "serveradd", 3, 4);

	// block
	result2->sync();

	// stop asio
	client_io.stop();
	clinet_thread.join();
}
