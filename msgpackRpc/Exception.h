#pragma once
#include <exception>
#include <boost/exception/all.hpp>
#include <string>
typedef boost::error_info<struct tag_int_info, int> IntInfo;
typedef boost::error_info<struct tag_str_info, std::string> StrInfo;

struct BaseException : virtual std::exception, virtual boost::exception { };

struct NetException : virtual BaseException { };
struct MessageException : virtual BaseException { };
struct FunctionException : virtual BaseException { };
struct RpcException : virtual BaseException { };

struct InvalidAddressException : virtual NetException { };
struct InvalidSocketException : virtual NetException { };
struct ConnectionException : virtual NetException { };

struct Msg4BytesHeadException : virtual MessageException { };
struct MsgTooLongException : virtual MessageException { };
struct MsgParseException : virtual MessageException { };
struct MsgArgsException : virtual MessageException { };

struct RequestNotFoundException : virtual FunctionException { };
struct DispatcherNotFoundException : virtual FunctionException { };
struct FunctionNotFoundException : virtual FunctionException { };
struct ArgsNotArrayException : virtual FunctionException { };
struct ArgsTooManyException : virtual FunctionException { };
struct ArgsNotEnoughException : virtual FunctionException { };
struct ArgsConvertException : virtual FunctionException { };
