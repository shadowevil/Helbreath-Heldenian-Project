// common_types.cpp: Implementation of common_types.h
//
//////////////////////////////////////////////////////////////////////

#include "common_types.h"

// Static member initialization for GameClock
std::chrono::steady_clock::time_point GameClock::s_startTime;
bool GameClock::s_initialized = false;
