#pragma once

#include "log_level.h"
#include <format>
#include <string>
#include <string_view>

namespace hb::logger {

namespace detail {
	void write(int channel, int level, std::string_view message);
}

void initialize(std::string_view log_directory);
void shutdown();

template<auto C = 0, typename... Args>
void error(std::format_string<Args...> fmt, Args&&... args)
{
	auto msg = std::format(fmt, std::forward<Args>(args)...);
	detail::write(static_cast<int>(C), level::error, msg);
}

template<auto C = 0, typename... Args>
void warn(std::format_string<Args...> fmt, Args&&... args)
{
	auto msg = std::format(fmt, std::forward<Args>(args)...);
	detail::write(static_cast<int>(C), level::warn, msg);
}

template<auto C = 0, typename... Args>
void log(std::format_string<Args...> fmt, Args&&... args)
{
	auto msg = std::format(fmt, std::forward<Args>(args)...);
	detail::write(static_cast<int>(C), level::log, msg);
}

template<auto C = 0, typename... Args>
void debug(std::format_string<Args...> fmt, Args&&... args)
{
	auto msg = std::format(fmt, std::forward<Args>(args)...);
	detail::write(static_cast<int>(C), level::debug, msg);
}

} // namespace hb::logger
