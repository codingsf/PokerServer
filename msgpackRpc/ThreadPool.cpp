#include "ThreadPool.h"

namespace msgpack {
namespace rpc {

thread_local std::shared_ptr<TcpSession> currentSessionPtr;


ThreadPool::ThreadPool()
{
}


ThreadPool::~ThreadPool()
{
}

std::shared_ptr<TcpSession> getCurrentTcpSession()
{
	return currentSessionPtr;
}

void setCurrentTcpSession(std::shared_ptr<TcpSession> pSession)
{
	currentSessionPtr = pSession;
}

} }