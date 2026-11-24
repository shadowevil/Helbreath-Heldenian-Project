#pragma once

// Debug-only observer slot on the client logger (HH-0041): the agent journal registers one function here to see
// every line the client logs, so warnings and errors reach its client.warnings / client.errors channels.
// The whole file is inside the testing guard, so Release has no slot and no call in client_log.cpp.
#include "claude_testing.h"

#ifdef DEF_CLAUDE_TESTING

#include <string_view>

namespace hb::logger
{
	// Runs on whichever thread logged, after the backend wrote the line and released its lock. It must not log
	// and must not throw. `level` is one of hb::logger::level.
	using observer_fn = void (*)(void* context, int level, std::string_view message);

	// One slot; a later call replaces the earlier one. Thread-safe against concurrent logging.
	void set_observer(observer_fn fn, void* context);
}

#endif
