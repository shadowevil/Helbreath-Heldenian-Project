#include "server_console.h"
#include <cstring>
#include <cstdio>

#ifndef _WIN32
	#include <unistd.h>
	#include <sys/select.h>
#endif

static ServerConsole s_console;

ServerConsole& GetServerConsole()
{
	return s_console;
}

// --- ANSI color table ---

static const char* GetAnsiColor(int color)
{
	switch (color)
	{
	case console_color::error:   return "\033[1;31m";
	case console_color::warning: return "\033[1;33m";
	case console_color::success: return "\033[1;32m";
	case console_color::bright:  return "\033[1;37m";
	case console_color::info:    return "\033[1;36m";
	case console_color::muted:   return "\033[0;90m";
	default:                     return "";
	}
}

// --- Constructor / Destructor ---

ServerConsole::ServerConsole()
	: m_input_len(0)
	, m_cursor_pos(0)
	, m_init(false)
#ifdef _WIN32
	, m_out(INVALID_HANDLE_VALUE)
	, m_in(INVALID_HANDLE_VALUE)
	, m_orig_mode(0)
#else
	, m_orig_termios{}
#endif
{
	std::memset(m_input, 0, sizeof(m_input));
}

ServerConsole::~ServerConsole()
{
	if (!m_init) return;
#ifdef _WIN32
	SetConsoleMode(m_in, m_orig_mode);
#else
	tcsetattr(STDIN_FILENO, TCSANOW, &m_orig_termios);
#endif
}

// --- init (platform-split) ---

bool ServerConsole::init()
{
#ifdef _WIN32
	m_out = GetStdHandle(STD_OUTPUT_HANDLE);
	m_in = GetStdHandle(STD_INPUT_HANDLE);
	if (m_out == INVALID_HANDLE_VALUE || m_in == INVALID_HANDLE_VALUE)
		return false;

	// Save original input mode and set raw key-event mode
	if (!GetConsoleMode(m_in, &m_orig_mode))
		return false;
	if (!SetConsoleMode(m_in, ENABLE_WINDOW_INPUT))
		return false;

	// Enable ANSI escape sequence processing on output
	DWORD outMode = 0;
	if (GetConsoleMode(m_out, &outMode))
		SetConsoleMode(m_out, outMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#else
	// Set terminal to raw mode (no echo, no canonical buffering)
	if (tcgetattr(STDIN_FILENO, &m_orig_termios) < 0)
		return false;
	struct termios raw = m_orig_termios;
	raw.c_lflag &= ~(ECHO | ICANON);
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 0;
	if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) < 0)
		return false;
#endif

	m_init = true;
	return true;
}

// --- Output (ANSI, shared across platforms) ---

void ServerConsole::clear_input_line()
{
	if (!m_init) return;
	std::fputs("\r\033[K", stdout);
	std::fflush(stdout);
}

void ServerConsole::draw_input_line()
{
	if (!m_init) return;
	// Clear line, draw green "> " prompt, input text in bright white, reset
	std::fputs("\r\033[K\033[1;32m> \033[1;37m", stdout);
	if (m_input_len > 0)
		std::fwrite(m_input, 1, m_input_len, stdout);
	std::fputs("\033[0m", stdout);
	// Move cursor to correct column (1-based): prompt ">" + space = 2 chars + cursor offset
	std::fprintf(stdout, "\033[%dG", 3 + m_cursor_pos);
	std::fflush(stdout);
}

void ServerConsole::write_line(const char* text, int color)
{
	if (!m_init) {
		std::printf("%s\n", text);
		return;
	}

	clear_input_line();

	const char* ansi = GetAnsiColor(color);
	if (ansi[0] != '\0')
		std::fputs(ansi, stdout);
	std::fputs(text, stdout);
	if (ansi[0] != '\0')
		std::fputs("\033[0m", stdout);
	std::fputc('\n', stdout);

	draw_input_line();
}

void ServerConsole::write_line_raw(std::string_view text)
{
	if (!m_init) {
		std::fwrite(text.data(), 1, text.size(), stdout);
		std::fputc('\n', stdout);
		return;
	}

	clear_input_line();
	std::fwrite(text.data(), 1, text.size(), stdout);
	std::fputs("\033[0m", stdout);
	std::fputc('\n', stdout);
	draw_input_line();
}

void ServerConsole::redraw_prompt()
{
	if (!m_init) return;
	draw_input_line();
}

// --- Command history ---

void ServerConsole::history_push(const char* cmd, int len)
{
	std::string entry(cmd, len);

	// Don't push duplicates of the most recent entry
	if (m_history_count > 0)
	{
		int last = (m_history_write - 1 + max_history) % max_history;
		if (m_history[last] == entry)
		{
			m_history_browse = m_history_count;
			m_browsing_history = false;
			return;
		}
	}

	m_history[m_history_write] = std::move(entry);
	m_history_write = (m_history_write + 1) % max_history;
	if (m_history_count < max_history)
		m_history_count++;
	m_history_browse = m_history_count;
	m_browsing_history = false;
}

void ServerConsole::set_input(const std::string& text)
{
	int len = static_cast<int>(text.size());
	if (len >= (int)sizeof(m_input))
		len = (int)sizeof(m_input) - 1;
	std::memcpy(m_input, text.c_str(), len);
	m_input[len] = '\0';
	m_input_len = len;
	m_cursor_pos = len;
	draw_input_line();
}

void ServerConsole::history_up()
{
	if (m_history_count == 0) return;

	if (!m_browsing_history)
	{
		// Save current input before starting to browse
		m_saved_input.assign(m_input, m_input_len);
		m_browsing_history = true;
	}

	if (m_history_browse > 0)
	{
		m_history_browse--;
		// Convert browse index to ring buffer index
		int ring_start = (m_history_count < max_history) ? 0 : m_history_write;
		int idx = (ring_start + m_history_browse) % max_history;
		set_input(m_history[idx]);
	}
}

void ServerConsole::history_down()
{
	if (!m_browsing_history) return;

	m_history_browse++;
	if (m_history_browse >= m_history_count)
	{
		// Back to current input
		m_history_browse = m_history_count;
		m_browsing_history = false;
		set_input(m_saved_input);
	}
	else
	{
		int ring_start = (m_history_count < max_history) ? 0 : m_history_write;
		int idx = (ring_start + m_history_browse) % max_history;
		set_input(m_history[idx]);
	}
}

// =========================================================================
// Input polling — platform-split
// =========================================================================

#ifdef _WIN32

bool ServerConsole::poll_input(char* out_cmd, int maxLen)
{
	if (!m_init) return false;

	DWORD numEvents = 0;
	GetNumberOfConsoleInputEvents(m_in, &numEvents);
	if (numEvents == 0) return false;

	INPUT_RECORD records[32];
	DWORD eventsRead = 0;

	while (numEvents > 0) {
		DWORD toRead = (numEvents > 32) ? 32 : numEvents;
		if (!ReadConsoleInput(m_in, records, toRead, &eventsRead))
			break;
		numEvents -= eventsRead;

		for (DWORD i = 0; i < eventsRead; i++) {
			if (records[i].EventType != KEY_EVENT) continue;
			if (!records[i].Event.KeyEvent.bKeyDown) continue;

			KEY_EVENT_RECORD& key = records[i].Event.KeyEvent;
			WORD vk = key.wVirtualKeyCode;
			char ch = key.uChar.AsciiChar;

			if (vk == VK_RETURN) {
				if (m_input_len > 0) {
					int copyLen = (m_input_len < maxLen - 1) ? m_input_len : (maxLen - 1);
					std::memcpy(out_cmd, m_input, copyLen);
					out_cmd[copyLen] = '\0';
					history_push(m_input, m_input_len);
					std::memset(m_input, 0, sizeof(m_input));
					m_input_len = 0;
					m_cursor_pos = 0;
					draw_input_line();
					return true;
				}
				std::memset(m_input, 0, sizeof(m_input));
				m_input_len = 0;
				m_cursor_pos = 0;
				draw_input_line();
				continue;
			}

			if (vk == VK_BACK) {
				if (m_cursor_pos > 0) {
					std::memmove(&m_input[m_cursor_pos - 1], &m_input[m_cursor_pos], m_input_len - m_cursor_pos);
					m_input_len--;
					m_cursor_pos--;
					m_input[m_input_len] = '\0';
					draw_input_line();
				}
				continue;
			}

			if (vk == VK_DELETE) {
				if (m_cursor_pos < m_input_len) {
					std::memmove(&m_input[m_cursor_pos], &m_input[m_cursor_pos + 1], m_input_len - m_cursor_pos - 1);
					m_input_len--;
					m_input[m_input_len] = '\0';
					draw_input_line();
				}
				continue;
			}

			if (vk == VK_LEFT)  { if (m_cursor_pos > 0) { m_cursor_pos--; draw_input_line(); } continue; }
			if (vk == VK_RIGHT) { if (m_cursor_pos < m_input_len) { m_cursor_pos++; draw_input_line(); } continue; }
			if (vk == VK_HOME)  { m_cursor_pos = 0; draw_input_line(); continue; }
			if (vk == VK_END)   { m_cursor_pos = m_input_len; draw_input_line(); continue; }
			if (vk == VK_UP)    { history_up(); continue; }
			if (vk == VK_DOWN)  { history_down(); continue; }

			if (ch >= 32 && ch < 127 && m_input_len < (int)(sizeof(m_input) - 1)) {
				std::memmove(&m_input[m_cursor_pos + 1], &m_input[m_cursor_pos], m_input_len - m_cursor_pos);
				m_input[m_cursor_pos] = ch;
				m_input_len++;
				m_cursor_pos++;
				m_input[m_input_len] = '\0';
				draw_input_line();
				continue;
			}
		}
	}

	return false;
}

#else // POSIX

bool ServerConsole::poll_input(char* out_cmd, int maxLen)
{
	if (!m_init) return false;

	fd_set fds;
	struct timeval tv;

	for (;;) {
		FD_ZERO(&fds);
		FD_SET(STDIN_FILENO, &fds);
		tv = {0, 0};
		if (select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv) <= 0)
			break;

		char ch = 0;
		if (read(STDIN_FILENO, &ch, 1) != 1)
			break;

		// Escape sequences (arrow keys, delete, home, end)
		if (ch == 27) {
			FD_ZERO(&fds);
			FD_SET(STDIN_FILENO, &fds);
			tv = {0, 50000}; // 50ms timeout for rest of escape sequence
			if (select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv) > 0) {
				char seq1 = 0;
				if (read(STDIN_FILENO, &seq1, 1) == 1 && seq1 == '[') {
					char seq2 = 0;
					if (read(STDIN_FILENO, &seq2, 1) == 1) {
						switch (seq2) {
						case 'A': history_up(); break;
						case 'B': history_down(); break;
						case 'D': if (m_cursor_pos > 0) { m_cursor_pos--; draw_input_line(); } break;
						case 'C': if (m_cursor_pos < m_input_len) { m_cursor_pos++; draw_input_line(); } break;
						case 'H': m_cursor_pos = 0; draw_input_line(); break;
						case 'F': m_cursor_pos = m_input_len; draw_input_line(); break;
						case '3': // Delete key: ESC[3~
						{
							char tilde = 0;
							[[maybe_unused]] auto n = read(STDIN_FILENO, &tilde, 1);
							if (m_cursor_pos < m_input_len) {
								std::memmove(&m_input[m_cursor_pos], &m_input[m_cursor_pos + 1], m_input_len - m_cursor_pos - 1);
								m_input_len--;
								m_input[m_input_len] = '\0';
								draw_input_line();
							}
						}
						break;
						}
					}
				}
			}
			continue;
		}

		// Enter
		if (ch == '\n' || ch == '\r') {
			if (m_input_len > 0) {
				int copyLen = (m_input_len < maxLen - 1) ? m_input_len : (maxLen - 1);
				std::memcpy(out_cmd, m_input, copyLen);
				out_cmd[copyLen] = '\0';
				history_push(m_input, m_input_len);
				std::memset(m_input, 0, sizeof(m_input));
				m_input_len = 0;
				m_cursor_pos = 0;
				draw_input_line();
				return true;
			}
			std::memset(m_input, 0, sizeof(m_input));
			m_input_len = 0;
			m_cursor_pos = 0;
			draw_input_line();
			continue;
		}

		// Backspace (127 on most terminals, 8 on some)
		if (ch == 127 || ch == 8) {
			if (m_cursor_pos > 0) {
				std::memmove(&m_input[m_cursor_pos - 1], &m_input[m_cursor_pos], m_input_len - m_cursor_pos);
				m_input_len--;
				m_cursor_pos--;
				m_input[m_input_len] = '\0';
				draw_input_line();
			}
			continue;
		}

		// Printable characters
		if (ch >= 32 && ch < 127 && m_input_len < (int)(sizeof(m_input) - 1)) {
			std::memmove(&m_input[m_cursor_pos + 1], &m_input[m_cursor_pos], m_input_len - m_cursor_pos);
			m_input[m_cursor_pos] = ch;
			m_input_len++;
			m_cursor_pos++;
			m_input[m_input_len] = '\0';
			draw_input_line();
		}
	}

	return false;
}

#endif
