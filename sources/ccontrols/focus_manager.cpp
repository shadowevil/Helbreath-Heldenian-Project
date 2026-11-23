#include "focus_manager.h"

namespace cc {

void focus_manager::collect_focusable(control* root)
{
	if (root->is_focusable() && root->is_effectively_visible() && root->is_effectively_enabled())
		m_controls.push_back(root);

	for (auto& child : root->children())
		collect_focusable(child.get());
}

void focus_manager::build(control* root, const std::vector<int>& explicit_order)
{
	// Clear old focus
	if (m_focus_index >= 0 && m_focus_index < static_cast<int>(m_controls.size()))
	{
		m_controls[m_focus_index]->set_focused(false);
		m_controls[m_focus_index]->on_focus_lost();
	}

	m_controls.clear();
	m_focus_index = -1;

	if (explicit_order.empty())
	{
		collect_focusable(root);
	}
	else
	{
		for (int id : explicit_order)
		{
			auto* ctrl = root->find(id);
			if (ctrl && ctrl->is_focusable())
				m_controls.push_back(ctrl);
		}
	}
}

void focus_manager::apply_focus(int new_index)
{
	if (m_focus_index >= 0 && m_focus_index < static_cast<int>(m_controls.size()))
	{
		m_controls[m_focus_index]->set_focused(false);
		m_controls[m_focus_index]->on_focus_lost();
	}

	m_focus_index = new_index;

	if (m_focus_index >= 0 && m_focus_index < static_cast<int>(m_controls.size()))
	{
		m_controls[m_focus_index]->set_focused(true);
		m_controls[m_focus_index]->on_focus_gained();
	}
}

void focus_manager::update(const input_state& input, const input_state& prev_input,
                           bool hover_focus, uint32_t time_ms,
                           uint32_t repeat_initial_ms, uint32_t repeat_interval_ms)
{
	if (m_controls.empty()) return;

	// Edge detection
	bool tab_pressed = input.key_tab && !prev_input.key_tab;
	bool up_pressed = input.key_up && !prev_input.key_up;
	bool down_pressed = input.key_down && !prev_input.key_down;
	bool left_pressed = m_horizontal_nav && input.key_left && !prev_input.key_left;
	bool right_pressed = m_horizontal_nav && input.key_right && !prev_input.key_right;
	bool mouse_clicked = input.mouse_left_down && !prev_input.mouse_left_down;

	// Navigation delta from key presses
	int nav_delta = 0;

	if (tab_pressed)
	{
		nav_delta = input.key_shift ? -1 : 1;
		m_repeat_direction = nav_delta;
		m_repeat_next_ms = time_ms + repeat_initial_ms;
	}
	else if (up_pressed || left_pressed)
	{
		nav_delta = -1;
		m_repeat_direction = -1;
		m_repeat_next_ms = time_ms + repeat_initial_ms;
	}
	else if (down_pressed || right_pressed)
	{
		nav_delta = 1;
		m_repeat_direction = 1;
		m_repeat_next_ms = time_ms + repeat_initial_ms;
	}
	// Key repeat while held
	else if (m_repeat_direction != 0)
	{
		bool key_still_held = false;
		if (input.key_tab) key_still_held = true;
		else if (m_repeat_direction == -1 && input.key_up) key_still_held = true;
		else if (m_repeat_direction == 1 && input.key_down) key_still_held = true;
		if (m_horizontal_nav)
		{
			if (m_repeat_direction == -1 && input.key_left) key_still_held = true;
			else if (m_repeat_direction == 1 && input.key_right) key_still_held = true;
		}

		if (key_still_held && time_ms >= m_repeat_next_ms)
		{
			nav_delta = m_repeat_direction;
			m_repeat_next_ms = time_ms + repeat_interval_ms;
		}
		else if (!key_still_held)
		{
			m_repeat_direction = 0;
		}
	}

	if (nav_delta != 0)
		advance(nav_delta);

	// Click to focus
	if (mouse_clicked)
	{
		for (int i = 0; i < static_cast<int>(m_controls.size()); ++i)
		{
			auto* ctrl = m_controls[i];
			if (ctrl->is_effectively_visible() && ctrl->is_effectively_enabled())
			{
				auto sb = ctrl->screen_bounds();
				if (rect_contains(sb, input.mouse_x, input.mouse_y))
				{
					if (m_focus_index != i)
						apply_focus(i);
					break;
				}
			}
		}
	}

	// Hover to focus
	if (hover_focus && !mouse_clicked)
	{
		for (int i = 0; i < static_cast<int>(m_controls.size()); ++i)
		{
			auto* ctrl = m_controls[i];
			if (ctrl->is_effectively_visible() && ctrl->is_effectively_enabled()
			    && ctrl->is_hovered())
			{
				if (m_focus_index != i)
					apply_focus(i);
				break;
			}
		}
	}
}

control* focus_manager::focused() const
{
	if (m_focus_index >= 0 && m_focus_index < static_cast<int>(m_controls.size()))
		return m_controls[m_focus_index];
	return nullptr;
}

int focus_manager::focused_id() const
{
	auto* f = focused();
	return f ? f->id() : -1;
}

void focus_manager::set_focus(int control_id)
{
	for (int i = 0; i < static_cast<int>(m_controls.size()); ++i)
	{
		if (m_controls[i]->id() == control_id)
		{
			apply_focus(i);
			return;
		}
	}
}

void focus_manager::advance(int delta)
{
	if (m_controls.empty()) return;

	int new_index;
	if (m_focus_index < 0)
	{
		new_index = delta > 0 ? 0 : static_cast<int>(m_controls.size()) - 1;
	}
	else
	{
		new_index = (m_focus_index + delta + static_cast<int>(m_controls.size()))
		            % static_cast<int>(m_controls.size());
	}

	apply_focus(new_index);
}

void focus_manager::set_horizontal_navigation(bool enabled)
{
	m_horizontal_nav = enabled;
}

} // namespace cc
