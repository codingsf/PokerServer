#pragma once
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4819)
#endif
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#include <memory>
#include <functional>
#include "Protocol.h"
#include "TupleUtil.h"

namespace msgpack {
namespace rpc {

enum ServerSideError
{
    success = 100,
	error_no_function,
    error_params_not_array,
    error_params_too_many,
    error_params_not_enough,
    error_params_convert,
    error_not_implemented,
    error_self_pointer_is_null,
};

typedef std::function<void(boost::system::error_code error)> error_handler_t;



class msgerror: public std::runtime_error
{
	ServerSideError m_code;

public:
    msgerror(const std::string &msg, ServerSideError code):std::runtime_error(msg), m_code(code)
    {
    }

    std::shared_ptr<msgpack::sbuffer> to_msg(uint32_t msgid)const
    {
        // error type
        MsgResponse<std::tuple<int, std::string>, bool> msgres(
                std::make_tuple(m_code, what()),
                true,
                msgid);
        // result
        auto sbuf=std::make_shared<msgpack::sbuffer>();
        msgpack::pack(*sbuf, msgres);
        return sbuf;
    }

};

} }
