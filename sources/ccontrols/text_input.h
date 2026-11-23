#pragma once

#include "control.h"
#include <string>
#include <string_view>
#include <cstdint>

namespace cc {

class text_input : public control
{
public:
	text_input(int id, rect local_bounds, int max_length);

	bool is_focusable() const override { return true; }

	// ===== Text buffer =====
	const std::string& text() const;
	std::string& text();
	int max_length() const;

	// ===== Hidden (password) mode =====
	void set_hidden(bool hidden);
	bool is_hidden() const;

	// ===== Display =====
	std::string display_text() const;

	// ===== Character filter =====
	void set_character_filter(std::string_view allowed_chars);

	// ===== Character handling =====
	void handle_char(uint32_t codepoint);
	void handle_backspace();
	void handle_delete();

	// ===== Cursor movement =====
	void move_cursor_left(bool extend_selection);
	void move_cursor_right(bool extend_selection);
	void move_to_start(bool extend_selection);
	void move_to_end(bool extend_selection);

	// ===== Selection =====
	void select_all();
	bool has_selection() const;
	int selection_start() const;
	int selection_end() const;
	std::string selected_text() const;
	void clear_selection();
	void delete_selection();

	// ===== Clipboard (pure data, no OS calls) =====
	std::string copy() const;
	std::string cut();
	void paste(const std::string& text);

	// ===== Queries for rendering =====
	int cursor_pos() const;
	bool is_cursor_visible(uint32_t time_ms) const;

	// ===== Blink reset =====
	void update_blink(uint32_t time_ms);

	// ===== Lifecycle =====
	void on_focus_gained() override;
	void on_focus_lost() override;

protected:
	std::string m_text;
	std::string m_allowed_chars;
	int m_max_length;
	bool m_hidden = false;
	int m_cursor_pos = 0;
	int m_selection_anchor = -1;
	uint32_t m_last_activity_time = 0;
	uint32_t m_current_time_ms = 0;

private:
	int prev_char_boundary(int pos) const;
	int next_char_boundary(int pos) const;
	void mark_activity(uint32_t time_ms = 0);
};

} // namespace cc
