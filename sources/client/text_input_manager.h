#pragma once

#include "control_collection.h"
#include <string>
#include <string_view>
#include <cstdint>

namespace cc { class textbox; }

class text_input_manager
{
public:
	static text_input_manager& get();

	void start_input(int x, int y, unsigned char max_len, std::string& buffer,
	                 bool hidden = false, std::string_view char_filter = {});
	void end_input();
	void clear_input();
	void update(uint32_t time_ms);
	void render();
	void show_input();

	std::string get_input_string() const;
	bool is_active() const { return m_is_active; }

	void set_chat_background(bool show) { m_show_chat_bg = show; }
	bool shows_chat_background() const { return m_show_chat_bg; }

private:
	text_input_manager();

	cc::control_collection m_controls;
	cc::textbox* m_active_textbox = nullptr;
	std::string* m_buffer = nullptr;
	bool m_is_active = false;
	bool m_show_chat_bg = false;
};
