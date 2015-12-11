#include "Dispatcher.h"
#include "TcpSession.h"

namespace msgpack {
namespace rpc {

std::shared_ptr<msgpack::sbuffer> Dispatcher::processCall(uint32_t msgid, msgpack::object method, msgpack::object args)
{
	std::string methodName;
	method.convert(&methodName);

	auto found = m_handlerMap.find(methodName);
	if (found == m_handlerMap.end())
	{
		BOOST_THROW_EXCEPTION(
			FunctionNotFoundException() <<
			err_no(error_no_function) <<
			err_str(std::string("error_no_function: ") + methodName));
	}
	else
	{
		Procedure proc = found->second;
		return proc(msgid, args);
	}
}

void Dispatcher::dispatch(const object &objMsg, msgpack::zone&& zone, std::shared_ptr<TcpSession> session)
{
	setCurrentTcpSession(session);
	std::shared_ptr<TcpConnection> connection = session->getConnection();
	MsgRequest<msgpack::object, msgpack::object> req;
	objMsg.convert(&req);
	try
	{
#ifdef _DEBUG
		if (req.method.as<std::string>() == "shutdown_send")
		{
			connection->getSocket().shutdown(boost::asio::socket_base::shutdown_send);
			return;
		}
		else if (req.method.as<std::string>() == "shutdown_receive")
		{
			connection->getSocket().shutdown(boost::asio::socket_base::shutdown_receive);
			return;
		}
		else if (req.method.as<std::string>() == "shutdown_both")
		{
			connection->getSocket().shutdown(boost::asio::socket_base::shutdown_both);
			return;
		}
#endif
		std::shared_ptr<msgpack::sbuffer> bufPtr = processCall(req.msgid, req.method, req.param);
		connection->asyncWrite(bufPtr);
		return;
	}
	catch (boost::exception& e)
	{
		auto no = boost::get_error_info<err_no>(e);
		auto str = boost::get_error_info<err_str>(e);

		MsgResponse<std::tuple<int, std::string>, bool> rsp(
			std::make_tuple(no ? *no : 0, str ? *str : ""),
			true,
			req.msgid);

		auto bufPtr = std::make_shared<msgpack::sbuffer>();
		msgpack::pack(*bufPtr, rsp);
		connection->asyncWrite(bufPtr);

		std::cerr << diagnostic_information(e) << std::endl;
	}
	catch (std::exception& e)
	{
		MsgResponse<std::tuple<int, std::string>, bool> rsp(
			std::make_tuple(0, e.what()),
			true,
			req.msgid);

		auto bufPtr = std::make_shared<msgpack::sbuffer>();
		msgpack::pack(*bufPtr, rsp);
		connection->asyncWrite(bufPtr);

		std::cerr << e.what() << std::endl;
	}
}

} }