#pragma once

#include <msgpack.hpp>

namespace msgpack {
namespace rpc {

static const uint8_t MSG_TYPE_NONE = 0x00;
static const uint8_t MSG_TYPE_REQUEST = 0x01;
static const uint8_t MSG_TYPE_RESPONSE = 0x02;
static const uint8_t MSG_TYPE_NOTIFY = 0x03;

struct MsgRpc
{
	MsgRpc() { }

	bool is_request()  const { return type == MSG_TYPE_REQUEST; }
	bool is_response() const { return type == MSG_TYPE_RESPONSE; }
	bool is_notify()   const { return type == MSG_TYPE_NOTIFY; }

	uint8_t type{ MSG_TYPE_NONE };
	MSGPACK_DEFINE(type);
};

template <typename TMethod, typename TParam>
struct MsgRequest
{
	MsgRequest() { }
	MsgRequest(TMethod method, TParam param, uint32_t msgid) :
		msgid(msgid),
		method(method),
		param(param) { }

	uint8_t type{ MSG_TYPE_REQUEST };
	uint32_t msgid{ 0 };
	TMethod method;
	TParam  param;
	MSGPACK_DEFINE(type, msgid, method, param);
};

template <typename TResult, typename TError>
struct MsgResponse
{
	MsgResponse() { }
	MsgResponse(TResult result, TError error, uint32_t msgid) :
		msgid(msgid),
		error(error),
		result(result) { }

	uint8_t type{ MSG_TYPE_RESPONSE };
	uint32_t msgid{ 0 };
	TError  error;
	TResult result;
	MSGPACK_DEFINE(type, msgid, error, result);
};

template <typename TMethod, typename TParam>
struct MsgNotify
{
	MsgNotify() { }
	MsgNotify(TMethod method, TParam param) :
		method(method),
		param(param) { }

	uint8_t type{ MSG_TYPE_NOTIFY };
	TMethod method;
	TParam  param;
	MSGPACK_DEFINE(type, method, param);
};

} } // namespace msgpack::rpc