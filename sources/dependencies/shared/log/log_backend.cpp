#include "log_backend.h"
#include "log_level.h"
#include <chrono>
#include <ctime>
#include <format>
#include <iostream>
#include <string>

namespace hb::logger {

static const char* level_ansi(int level)
{
	switch (level)
	{
	case level::error: return "\033[1;31m"; // bright red
	case level::warn:  return "\033[1;33m"; // bright yellow
	case level::debug: return "\033[0;90m"; // dim gray
	default:           return "";
	}
}

void log_backend::init(const config& cfg)
{
	std::lock_guard lock(m_mutex);
	m_directory = cfg.directory;
	m_filenames = cfg.filenames;
	m_channel_count = cfg.channel_count;
	m_channel_namer = cfg.channel_namer;

	if (!m_directory.empty())
		std::filesystem::create_directories(m_directory);

	for (int i = 0; i < m_channel_count && i < max_channels; ++i)
	{
		if (m_filenames[i].empty()) continue;
		auto path = m_directory / m_filenames[i];
		m_files[i].open(path, std::ios::app);
	}
	m_initialized = true;
}

void log_backend::write(int channel, int level, std::string_view message)
{
	std::lock_guard lock(m_mutex);
	if (!m_initialized) return;

	auto now = std::chrono::system_clock::now();
	auto time_t_now = std::chrono::system_clock::to_time_t(now);
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
		now.time_since_epoch()) % 1000;
	struct tm tm_buf;
#ifdef _WIN32
	localtime_s(&tm_buf, &time_t_now);
#else
	localtime_r(&time_t_now, &tm_buf);
#endif

	auto timestamp = std::format("{:02d}:{:02d}:{:02d}.{:03d}",
		tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec, static_cast<int>(ms.count()));

	std::string tag;
	if (channel == 0 || !m_channel_namer)
		tag = std::format("[{}]", level_name(level));
	else
		tag = std::format("[{}] [{}]", level_name(level), m_channel_namer(channel));

	std::string ts_str = std::format("[{}]", timestamp);
	std::string line = std::format("{} {} {}", ts_str, tag, message);

	// Build colored line: timestamp=muted, tag=level color, message=default
	const char* tag_ansi = level_ansi(level);
	std::string colored;
	if (tag_ansi[0] != '\0')
		colored = std::format("\033[0;90m{}\033[0m {}{}\033[0m {}", ts_str, tag_ansi, tag, message);
	else
		colored = std::format("\033[0;90m{}\033[0m {} {}", ts_str, tag, message);

	write_console(channel, colored);

	if (channel >= 0 && channel < m_channel_count && m_files[channel].is_open())
	{
		m_files[channel] << line << '\n';
		m_files[channel].flush();
	}

	if (channel != 0 && m_files[0].is_open())
	{
		m_files[0] << line << '\n';
		m_files[0].flush();
	}
}

void log_backend::write_console(int channel, std::string_view colored_line)
{
	(void)channel;
	std::cout << colored_line << '\n';
}

void log_backend::close()
{
	std::lock_guard lock(m_mutex);
	for (auto& file : m_files)
	{
		if (file.is_open())
		{
			file.flush();
			file.close();
		}
	}
	m_initialized = false;
}

} // namespace hb::logger
