#pragma once

namespace hb::logger {

namespace level {
	constexpr int error = 0;
	constexpr int warn  = 1;
	constexpr int log   = 2;
	constexpr int debug = 3;
}

constexpr const char* level_name(int lvl)
{
	switch (lvl)
	{
	case level::error: return "ERROR";
	case level::warn:  return "WARN";
	case level::log:   return "LOG";
	case level::debug: return "DEBUG";
	default:           return "???";
	}
}

} // namespace hb::logger
