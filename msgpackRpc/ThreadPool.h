#pragma once
#include <memory>

namespace msgpack {
namespace rpc {

class ThreadPool
{
public:
	ThreadPool();
	~ThreadPool();
};

class TcpSession;
std::shared_ptr<TcpSession> getCurrentTcpSession();
void setCurrentTcpSession(std::shared_ptr<TcpSession> pSession);

} }