#pragma once
#include <array>
#include <mutex>
#include <unordered_set>

namespace msgpack {
namespace rpc {

static const uint32_t MAX_MSG_LENGTH = 32 * 1024;
typedef std::array<char, MAX_MSG_LENGTH> ArrayBuffer;

class BufferManager
{
public:
	static BufferManager* instance();

	std::shared_ptr<ArrayBuffer> getBuffer();

	void freeBuffer(std::shared_ptr<ArrayBuffer> pbuf);

private:
	BufferManager();
	~BufferManager();
	BufferManager(const BufferManager&) = delete;
	BufferManager& operator=(const BufferManager&) = delete;

	std::mutex _mutex;
	std::unordered_set<std::shared_ptr<ArrayBuffer>> _bufferPool;

	uint32_t _genCount{ 0 };
	uint32_t _minCount{ 32 * 1 };
	uint32_t _maxCount{ 32 * 1024};
};

} }