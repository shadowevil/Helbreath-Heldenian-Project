#include "text_input.h"

namespace cc {

text_input::text_input(int id, rect local_bounds, int max_length)
	: control(id, local_bounds)
	, m_max_length(max_length)
{
}

const std::string& text_input::text() const { return m_text; }
std::string& text_input::text() { return m_text; }
int text_input::max_length() const { return m_max_length; }

void text_input::set_character_filter(std::string_view allowed_chars) { m_allowed_chars = std::string(allowed_chars); }

void text_input::set_hidden(bool hidden) { m_hidden = hidden; }
bool text_input::is_hidden() const { return m_hidden; }

std::string text_input::display_text() const
{
	if (m_hidden)
		return std::string(m_text.size(), '*');
	return m_text;
}

// ===== UTF-8 boundary helpers =====

int text_input::prev_char_boundary(int pos) const
{
	if (pos <= 0) return 0;
	pos--;
	// Skip UTF-8 continuation bytes (10xxxxxx)
	while (pos > 0 && (static_cast<unsigned char>(m_text[pos]) & 0xC0) == 0x80)
		pos--;
	return pos;
}

int text_input::next_char_boundary(int pos) const
{
	int len = static_cast<int>(m_text.size());
	if (pos >= len) return len;
	pos++;
	// Skip UTF-8 continuation bytes (10xxxxxx)
	while (pos < len && (static_cast<unsigned char>(m_text[pos]) & 0xC0) == 0x80)
		pos++;
	return pos;
}

void text_input::mark_activity(uint32_t time_ms)
{
	m_last_activity_time = time_ms > 0 ? time_ms : m_current_time_ms;
}

// ===== Character handling =====

void text_input::handle_char(uint32_t codepoint)
{
	// Ignore control characters
	if (codepoint < 32 || codepoint == 127) return;

	// Character filter — reject if not in allowed set
	if (!m_allowed_chars.empty())
	{
		if (codepoint >= 128 || m_allowed_chars.find(static_cast<char>(codepoint)) == std::string::npos)
			return;
	}

	// Delete selection first if active
	if (has_selection())
		delete_selection();

	// Encode codepoint to UTF-8
	std::string encoded;
	if (codepoint < 0x80)
	{
		encoded += static_cast<char>(codepoint);
	}
	else if (codepoint < 0x800)
	{
		encoded += static_cast<char>(0xC0 | (codepoint >> 6));
		encoded += static_cast<char>(0x80 | (codepoint & 0x3F));
	}
	else if (codepoint < 0x10000)
	{
		encoded += static_cast<char>(0xE0 | (codepoint >> 12));
		encoded += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
		encoded += static_cast<char>(0x80 | (codepoint & 0x3F));
	}
	else
	{
		encoded += static_cast<char>(0xF0 | (codepoint >> 18));
		encoded += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
		encoded += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
		encoded += static_cast<char>(0x80 | (codepoint & 0x3F));
	}

	// Check length limit (in bytes, matching legacy behavior)
	if (m_max_length > 0 && static_cast<int>(m_text.size() + encoded.size()) >= m_max_length)
		return;

	m_text.insert(m_cursor_pos, encoded);
	m_cursor_pos += static_cast<int>(encoded.size());
	mark_activity();
}

void text_input::handle_backspace()
{
	if (has_selection())
	{
		delete_selection();
		return;
	}

	if (m_cursor_pos <= 0) return;

	int prev = prev_char_boundary(m_cursor_pos);
	m_text.erase(prev, m_cursor_pos - prev);
	m_cursor_pos = prev;
	mark_activity();
}

void text_input::handle_delete()
{
	if (has_selection())
	{
		delete_selection();
		return;
	}

	int len = static_cast<int>(m_text.size());
	if (m_cursor_pos >= len) return;

	int next = next_char_boundary(m_cursor_pos);
	m_text.erase(m_cursor_pos, next - m_cursor_pos);
	mark_activity();
}

// ===== Cursor movement =====

void text_input::move_cursor_left(bool extend_selection)
{
	if (!extend_selection && has_selection())
	{
		// Collapse selection to left edge
		m_cursor_pos = selection_start();
		clear_selection();
		mark_activity();
		return;
	}

	if (extend_selection && m_selection_anchor < 0)
		m_selection_anchor = m_cursor_pos;

	m_cursor_pos = prev_char_boundary(m_cursor_pos);

	if (!extend_selection)
		clear_selection();

	mark_activity();
}

void text_input::move_cursor_right(bool extend_selection)
{
	if (!extend_selection && has_selection())
	{
		// Collapse selection to right edge
		m_cursor_pos = selection_end();
		clear_selection();
		mark_activity();
		return;
	}

	if (extend_selection && m_selection_anchor < 0)
		m_selection_anchor = m_cursor_pos;

	m_cursor_pos = next_char_boundary(m_cursor_pos);

	if (!extend_selection)
		clear_selection();

	mark_activity();
}

void text_input::move_to_start(bool extend_selection)
{
	if (extend_selection && m_selection_anchor < 0)
		m_selection_anchor = m_cursor_pos;

	m_cursor_pos = 0;

	if (!extend_selection)
		clear_selection();

	mark_activity();
}

void text_input::move_to_end(bool extend_selection)
{
	if (extend_selection && m_selection_anchor < 0)
		m_selection_anchor = m_cursor_pos;

	m_cursor_pos = static_cast<int>(m_text.size());

	if (!extend_selection)
		clear_selection();

	mark_activity();
}

// ===== Selection =====

void text_input::select_all()
{
	m_selection_anchor = 0;
	m_cursor_pos = static_cast<int>(m_text.size());
	mark_activity();
}

bool text_input::has_selection() const
{
	return m_selection_anchor >= 0 && m_selection_anchor != m_cursor_pos;
}

int text_input::selection_start() const
{
	if (m_selection_anchor < 0) return m_cursor_pos;
	return m_cursor_pos < m_selection_anchor ? m_cursor_pos : m_selection_anchor;
}

int text_input::selection_end() const
{
	if (m_selection_anchor < 0) return m_cursor_pos;
	return m_cursor_pos > m_selection_anchor ? m_cursor_pos : m_selection_anchor;
}

std::string text_input::selected_text() const
{
	if (!has_selection()) return {};
	int start = selection_start();
	int end = selection_end();
	return m_text.substr(start, end - start);
}

void text_input::clear_selection()
{
	m_selection_anchor = -1;
}

void text_input::delete_selection()
{
	if (!has_selection()) return;

	int start = selection_start();
	int end = selection_end();
	m_text.erase(start, end - start);
	m_cursor_pos = start;
	clear_selection();
	mark_activity();
}

// ===== Clipboard =====

std::string text_input::copy() const
{
	return selected_text();
}

std::string text_input::cut()
{
	std::string result = selected_text();
	delete_selection();
	return result;
}

void text_input::paste(const std::string& pasted)
{
	if (has_selection())
		delete_selection();

	// Check length limit
	if (m_max_length > 0)
	{
		int available = m_max_length - 1 - static_cast<int>(m_text.size());
		if (available <= 0) return;

		std::string clipped = pasted.substr(0, available);
		m_text.insert(m_cursor_pos, clipped);
		m_cursor_pos += static_cast<int>(clipped.size());
	}
	else
	{
		m_text.insert(m_cursor_pos, pasted);
		m_cursor_pos += static_cast<int>(pasted.size());
	}
	mark_activity();
}

// ===== Queries for rendering =====

int text_input::cursor_pos() const { return m_cursor_pos; }

bool text_input::is_cursor_visible(uint32_t time_ms) const
{
	// Blink: 500ms on, 500ms off. Reset on activity.
	uint32_t elapsed = time_ms - m_last_activity_time;
	return (elapsed / 500) % 2 == 0;
}

void text_input::update_blink(uint32_t time_ms)
{
	m_current_time_ms = time_ms;
}

// ===== Lifecycle =====

void text_input::on_focus_gained()
{
	// Reset cursor blink and place cursor at end
	m_cursor_pos = static_cast<int>(m_text.size());
	mark_activity();
}

void text_input::on_focus_lost()
{
	clear_selection();
}

} // namespace cc
