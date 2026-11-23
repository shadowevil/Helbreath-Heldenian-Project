// io_service_pool.h: Shared io_context + optional thread pool
//
// Server uses threadCount=4 for async I/O on multiple threads.
// Client uses threadCount=0 for manual poll mode (same as today).
//
//////////////////////////////////////////////////////////////////////

#pragma once

// ASIO standalone configuration (must be before asio.hpp)
#define ASIO_STANDALONE
#define ASIO_NO_DEPRECATED
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601  // Windows 7+
#endif

#include "ASIO/asio.hpp"

#include <vector>
#include <thread>

namespace hb::shared::net {

class IOServicePool {
public:
	explicit IOServicePool(int threadCount);
	~IOServicePool();

	// Non-copyable
	IOServicePool(const IOServicePool&) = delete;
	IOServicePool& operator=(const IOServicePool&) = delete;

	asio::io_context& get_context() { return m_ioContext; }

	// start background threads (no-op if threadCount == 0)
	void start();

	// stop context, join all threads
	void stop();

private:
	asio::io_context m_ioContext;
	asio::executor_work_guard<asio::io_context::executor_type> m_workGuard;
	std::vector<std::thread> m_threads;
	int m_threadCount;
};

} // namespace hb::shared::net
