#pragma once
#include <mutex>
#include "TcpSession.h"
namespace msgpack {
namespace rpc {
		
class SessionManager
{
public:
	/// Create instance
	static SessionManager* instance();

	/// Add the specified session to the manager and start it.
	void start(SessionPtr session);

	/// Stop the specified connection.
	void stop(SessionPtr session);

	/// Stop all session.
	void stopAll();

	/// Get session pool
	std::set<SessionPtr>& getSessionPool();

private:
	SessionManager();
	~SessionManager();
	SessionManager(const SessionManager&) = delete;
	SessionManager& operator=(const SessionManager&) = delete;

	std::mutex _mutex;
	std::set<SessionPtr> _sessionPool;
};

inline std::set<SessionPtr>& SessionManager::getSessionPool()
{
	return _sessionPool;
}

} }