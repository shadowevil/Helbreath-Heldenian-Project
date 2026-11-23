// io_service_pool.cpp: Shared io_context + optional thread pool
//
//////////////////////////////////////////////////////////////////////

#include "io_service_pool.h"
#include <cstdio>
#include "log.h"

namespace hb::shared::net {

IOServicePool::IOServicePool(int threadCount)
	: m_ioContext()
	, m_workGuard(asio::make_work_guard(m_ioContext))
	, m_threadCount(threadCount)
{
}

IOServicePool::~IOServicePool()
{
	stop();
}

void IOServicePool::start()
{
	if (m_threadCount <= 0) return;
	if (!m_threads.empty()) return; // Already started

	for(int i = 0; i < m_threadCount; i++) {
		m_threads.emplace_back([this, i]() {
			m_ioContext.run();
			hb::logger::debug("[IOServicePool] I/O thread {} exited", i);
		});
	}
}

void IOServicePool::stop()
{
	// Release work guard so run() can return when idle
	m_workGuard.reset();

	// stop accepting new handlers
	m_ioContext.stop();

	// Join all threads
	for (auto& t : m_threads) {
		if (t.joinable()) {
			t.join();
		}
	}
	m_threads.clear();
}

} // namespace hb::shared::net
