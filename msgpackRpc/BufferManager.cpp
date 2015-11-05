#include "BufferManager.h"

namespace msgpack {
namespace rpc {

BufferManager::BufferManager()
{
}

BufferManager::~BufferManager()
{
	_bufferPool.clear();
}

BufferManager* BufferManager::instance()
{
	static std::once_flag instanceFlag;
	static BufferManager* pInstance;

	std::call_once(instanceFlag, []()
	{
		static BufferManager instance;

		for (uint32_t i = 0; i < instance._minCount; ++i)
			instance._bufferPool.insert(std::make_shared<ArrayBuffer>());

		instance._genCount = instance._minCount;
		pInstance = &instance;
	});
	return pInstance;
}

std::shared_ptr<ArrayBuffer> BufferManager::getBuffer()
{
	std::unique_lock<std::mutex> lck(_mutex);
	auto iter = _bufferPool.begin();
	if (iter != _bufferPool.end())
	{
		auto ptr = *iter;
		_bufferPool.erase(iter);
		return ptr;
	}
	else
	{
		if (_genCount < _maxCount)
			for (uint32_t i = 0; i < _minCount - 1; ++i)
				_bufferPool.insert(std::make_shared<ArrayBuffer>());

		auto ptr = std::make_shared<ArrayBuffer>();
		_genCount += _minCount;
		return ptr;
	}
}

void BufferManager::freeBuffer(std::shared_ptr<ArrayBuffer> pbuf)
{
	std::unique_lock<std::mutex> lck(_mutex);
	_bufferPool.insert(pbuf);
}

} }