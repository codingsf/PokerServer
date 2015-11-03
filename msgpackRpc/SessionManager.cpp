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
	static std::once_flag instanceFlag;
	static SessionManager* pInstance;

	std::call_once(instanceFlag, []()
	{
		static SessionManager instance;
		pInstance = &instance;
	});
	return pInstance;
}

void SessionManager::start(SessionPtr session)
{
	std::unique_lock<std::mutex> lck(_mutex);
	_sessionPool.insert(session);
	//c->start();
}

void SessionManager::stop(SessionPtr session)
{
	std::unique_lock<std::mutex> lck(_mutex);
	_sessionPool.erase(session);
	//c->stop();
}

void SessionManager::stopAll()
{
	std::unique_lock<std::mutex> lck(_mutex);
	for (auto session : _sessionPool)
		session->stop();
	_sessionPool.clear();
}

} }