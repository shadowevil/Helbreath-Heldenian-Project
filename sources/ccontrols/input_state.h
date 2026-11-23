#pragma once

#include <cstdint>

namespace cc {

struct input_state
{
	int mouse_x = 0;
	int mouse_y = 0;
	bool mouse_left_down = false;
	bool mouse_right_down = false;
	bool key_tab = false;
	bool key_shift = false;
	bool key_enter = false;
	bool key_escape = false;
	bool key_up = false;
	bool key_down = false;
	bool key_left = false;
	bool key_right = false;
	bool key_space = false;

	// Text input keys
	bool key_ctrl = false;
	bool key_home = false;
	bool key_end = false;
	bool key_delete = false;
	bool key_backspace = false;
	bool key_a = false;
	bool key_c = false;
	bool key_x = false;
	bool key_v = false;

	// Typed characters this frame (Unicode codepoints)
	static constexpr int max_typed_chars = 16;
	uint32_t typed_chars[max_typed_chars] = {};
	int typed_char_count = 0;
};

} // namespace cc
