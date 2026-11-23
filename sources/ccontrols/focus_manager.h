#pragma once

#include "control.h"
#include "input_state.h"
#include <vector>
#include <cstdint>

namespace cc {

class focus_manager
{
public:
	void build(control* root, const std::vector<int>& explicit_order = {});

	void update(const input_state& input, const input_state& prev_input,
	            bool hover_focus, uint32_t time_ms,
	            uint32_t repeat_initial_ms, uint32_t repeat_interval_ms);

	control* focused() const;
	int focused_id() const;

	void set_focus(int control_id);
	void advance(int delta = 1);

	void set_horizontal_navigation(bool enabled);

private:
	void collect_focusable(control* root);
	void apply_focus(int new_index);

	std::vector<control*> m_controls;
	int m_focus_index = -1;

	uint32_t m_repeat_next_ms = 0;
	int m_repeat_direction = 0;
	bool m_horizontal_nav = false;
};

} // namespace cc
