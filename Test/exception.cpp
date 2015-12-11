#include <boost/test/unit_test.hpp> 
#include "TcpConnection.h"
#include "TcpSession.h"
#include "TcpServer.h"
#include "SessionManager.h"
#include <iostream>
#include <boost/timer.hpp>
#include "main.h"

BOOST_AUTO_TEST_SUITE(ts_exception, *boost::unit_test::enable_if<enable_exception>())

BOOST_AUTO_TEST_CASE(net_exception)
{
	std::cout << "BGN net_exception" << std::endl;

	boost::asio::io_service client_io;
	boost::asio::io_service::work work(client_io);
	boost::thread clinet_thread([&client_io]() { client_io.run(); });

	auto ses = std::make_shared<msgpack::rpc::TcpSession>(client_io, nullptr);
	ses->asyncConnect(tcp::endpoint(address::from_string("127.0.0.1"), 8077)).get();

	try	// call on valid socket
	{
		ses->call(std::string("add")).get();
	}
	catch (const boost::exception& e)
	{
		std::cout << "net_exception:" << *boost::get_error_info<err_no>(e) << "	" << *boost::get_error_info<err_str>(e) << std::endl;
	}

	client_io.stop();
	clinet_thread.join();
	std::cout << "END net_exception" << std::endl << std::endl;
}

BOOST_AUTO_TEST_CASE(server_exception)
{
	std::cout << "BGN server_exception" << std::endl;

	boost::asio::io_service client_io;
	boost::asio::io_service::work work(client_io);
	boost::thread clinet_thread([&client_io]() { client_io.run(); });

	auto ses = std::make_shared<msgpack::rpc::TcpSession>(client_io, nullptr);
	ses->asyncConnect(peer).get();

	try	// no_function
	{
		ses->call(std::string("no_function")).get();
		std::cout << "no_function error" << std::endl;
	}
	catch (const boost::exception& e)
	{
		if (msgpack::rpc::error_no_function == *boost::get_error_info<err_no>(e))
			std::cout << "no_function ok" << std::endl;
		else
			std::cout << "no_function error:" << *boost::get_error_info<err_no>(e) << "	" << *boost::get_error_info<err_str>(e) << std::endl;
	}

	try	// params_too_many
	{
		ses->call(std::string("add"), 1, 2, 3).get();
		std::cout << "params_too_many error" << std::endl;
	}
	catch (const boost::exception& e)
	{
		if (msgpack::rpc::error_params_too_many == *boost::get_error_info<err_no>(e))
			std::cout << "params_too_many ok" << std::endl;
		else
			std::cout << "params_too_many error:" << *boost::get_error_info<err_no>(e) << "	" << *boost::get_error_info<err_str>(e) << std::endl;
	}

	try	// params_not_enough
	{
		ses->call(std::string("add"), 1).get();
		std::cout << "params_not_enough error" << std::endl;
	}
	catch (const boost::exception& e)
	{
		if (msgpack::rpc::error_params_not_enough == *boost::get_error_info<err_no>(e))
			std::cout << "params_not_enough ok" << std::endl;
		else
			std::cout << "params_not_enough error:" << *boost::get_error_info<err_no>(e) << "	" << *boost::get_error_info<err_str>(e) << std::endl;
	}

	try	// invalid argument type
	{
		ses->call(std::string("add"), "a", "b").get();
		std::cout << "invalid argument type error" << std::endl;
	}
	catch (const boost::exception& e)
	{
		if (msgpack::rpc::error_params_convert == *boost::get_error_info<err_no>(e))
			std::cout << "invalid argument type ok" << std::endl;
		else
			std::cout << "invalid argument type error:" << *boost::get_error_info<err_no>(e) << "	" << *boost::get_error_info<err_str>(e) << std::endl;
	}

	client_io.stop();
	clinet_thread.join();
	std::cout << "END server_exception" << std::endl << std::endl;
}

BOOST_AUTO_TEST_SUITE_END()