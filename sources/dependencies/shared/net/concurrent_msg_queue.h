// concurrent_msg_queue.h: Thread-safe message queue for server
//
// Replaces the non-thread-safe CMsg* ring buffer (m_pMsgQuene[100000])
// with a mutex-protected deque. Supports push from I/O threads and
// pop from the game thread.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include <mutex>
#include <deque>
#include <vector>
#include <cstdint>
#include <cstring>

namespace hb::shared::net {

struct QueuedMsg {
	char cFrom;
	std::vector<char> data;
	size_t size;
	int index;
	char key;

	QueuedMsg() : cFrom(0), size(0), index(0), key(0) {}
	QueuedMsg(char from, const char* src, size_t len, int idx, char k)
		: cFrom(from), size(len), index(idx), key(k)
	{
		if (src && len > 0) {
			data.assign(src, src + len);
		}
	}
};

class ConcurrentMsgQueue {
public:
	bool push(char cFrom, const char* data, size_t size, int index, char key)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		if (m_queue.size() >= MAX_QUEUE_SIZE) return false;
		m_queue.emplace_back(cFrom, data, size, index, key);
		return true;
	}

	bool pop(char* pFrom, char* data, size_t* size, int* index, char* key)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		if (m_queue.empty()) return false;

		auto& msg = m_queue.front();
		*pFrom = msg.cFrom;
		if (!msg.data.empty()) {
			std::memcpy(data, msg.data.data(), msg.size);
		}
		*size = msg.size;
		*index = msg.index;
		*key = msg.key;

		m_queue.pop_front();
		return true;
	}

	size_t size() const
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		return m_queue.size();
	}

private:
	mutable std::mutex m_mutex;
	std::deque<QueuedMsg> m_queue;
	static constexpr size_t MAX_QUEUE_SIZE = 100000;
};

// Thread-safe queue for accepted sockets
template<typename T>
class ConcurrentQueue {
public:
	void push(T item)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_queue.push_back(std::move(item));
	}

	bool pop(T& out)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		if (m_queue.empty()) return false;
		out = std::move(m_queue.front());
		m_queue.pop_front();
		return true;
	}

	bool empty() const
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		return m_queue.empty();
	}

	size_t size() const
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		return m_queue.size();
	}

private:
	mutable std::mutex m_mutex;
	std::deque<T> m_queue;
};

// Error event from I/O threads
struct SocketErrorEvent {
	int socket_index;
	int error_code;
};

} // namespace hb::shared::net
