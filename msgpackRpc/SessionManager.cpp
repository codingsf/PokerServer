#include "SessionManager.h"

namespace msgpack {
namespace rpc {

SessionManager::SessionManager()
{
}

SessionManager::~SessionManager()
{
}

SessionManager* SessionManager::instance()
{
	static std::once_flag instance_flag;
	static SessionManager* m_pInstance;

	std::call_once(instance_flag, []() { m_pInstance = new SessionManager(); });
	return m_pInstance;
}

void SessionManager::start(SessionPtr session)
{
	std::unique_lock<std::mutex> lck(_mtx);
	_sessionPool.insert(session);
	//c->start();
}

void SessionManager::stop(SessionPtr session)
{
	std::unique_lock<std::mutex> lck(_mtx);
	_sessionPool.erase(session);
	//c->stop();
}

void SessionManager::stopAll()
{
	std::unique_lock<std::mutex> lck(_mtx);
	for (auto session : _sessionPool)
		session->stop();
	_sessionPool.clear();
}

} }