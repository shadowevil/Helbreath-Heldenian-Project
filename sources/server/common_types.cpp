// common_types.cpp: GameClock static storage
//////////////////////////////////////////////////////////////////////

#include "common_types.h"

std::chrono::steady_clock::time_point GameClock::s_startTime;
bool GameClock::s_initialized = false;
