#pragma once

#ifdef _WIN32
	#include <windows.h>
#else
	#include <termios.h>
#endif

#include <string>
#include <format>

namespace console_color
{
	constexpr int normal  = 0;
	constexpr int error   = 1; // bright red
	constexpr int warning = 2; // bright yellow
	constexpr int success = 3; // bright green
	constexpr int bright  = 4; // bright white
	constexpr int info    = 5; // bright cyan
	constexpr int muted   = 6; // dim gray
}

class ServerConsole
{
public:
	ServerConsole();
	~ServerConsole();
	bool init();
	void write_line(const char* text, int color = console_color::normal);
	void write_line_raw(std::string_view text);
	bool poll_input(char* out_cmd, int maxLen);
	void redraw_prompt();

private:
	void clear_input_line();
	void draw_input_line();

	// Input buffer
	char m_input[256];
	int m_input_len;
	int m_cursor_pos;
	bool m_init;

	// Command history
	static constexpr int max_history = 64;
	std::string m_history[max_history];
	int m_history_count = 0;
	int m_history_write = 0;
	int m_history_browse = 0;
	std::string m_saved_input;
	bool m_browsing_history = false;

	void history_push(const char* cmd, int len);
	void history_up();
	void history_down();
	void set_input(const std::string& text);

#ifdef _WIN32
	HANDLE m_out;
	HANDLE m_in;
	DWORD m_orig_mode;
#else
	struct termios m_orig_termios;
#endif
};

ServerConsole& GetServerConsole();

// ============================================================================
// hb::console — Direct console output (bypasses log system, no timestamps).
// Use for command feedback and UI output.
// ============================================================================

namespace hb::console
{

inline void write(const char* text, int color = console_color::normal)
{
	GetServerConsole().write_line(text, color);
}

inline void write(const std::string& text, int color = console_color::normal)
{
	GetServerConsole().write_line(text.c_str(), color);
}

template <typename... Args>
void write(std::format_string<Args...> fmt, Args&&... args)
{
	auto text = std::format(fmt, std::forward<Args>(args)...);
	GetServerConsole().write_line(text.c_str(), console_color::normal);
}

template <typename... Args>
void error(std::format_string<Args...> fmt, Args&&... args)
{
	auto text = std::format(fmt, std::forward<Args>(args)...);
	GetServerConsole().write_line(text.c_str(), console_color::error);
}

template <typename... Args>
void warn(std::format_string<Args...> fmt, Args&&... args)
{
	auto text = std::format(fmt, std::forward<Args>(args)...);
	GetServerConsole().write_line(text.c_str(), console_color::warning);
}

template <typename... Args>
void success(std::format_string<Args...> fmt, Args&&... args)
{
	auto text = std::format(fmt, std::forward<Args>(args)...);
	GetServerConsole().write_line(text.c_str(), console_color::success);
}

template <typename... Args>
void info(std::format_string<Args...> fmt, Args&&... args)
{
	auto text = std::format(fmt, std::forward<Args>(args)...);
	GetServerConsole().write_line(text.c_str(), console_color::info);
}

} // namespace hb::console
