#include "control_collection.h"
#include "button.h"
#include "textbox.h"
#include "text_input.h"
#include "toggle_button.h"

namespace cc {

control_collection::control_collection()
	: m_root(0, {0, 0, 0, 0})
{
}

void control_collection::set_screen_size(int w, int h)
{
	m_root.set_local_bounds({0, 0, w, h});
}

panel* control_collection::root() { return &m_root; }

void control_collection::clear()
{
	// Rebuild focus manager first — defocuses current control while children are still alive,
	// then sets m_focus_index to -1 so it won't access them after they're destroyed.
	m_focus.build(&m_root, {});

	// Now safe to destroy all children
	m_root.m_children.clear();

	// Reset repeat/edge/tooltip state
	m_prev_input = {};
	m_toggle_repeat_dir = 0;
	m_toggle_repeat_next_ms = 0;
	m_text_repeat_key = 0;
	m_text_repeat_next_ms = 0;
	m_hover_control_id = -1;
	m_hover_start_ms = 0;
	m_active_tooltip = nullptr;
	m_enter_pressed = false;
	m_escape_pressed = false;
	m_mouse_released = false;
}

control* control_collection::find(int id) { return m_root.find(id); }

void control_collection::set_focus_order(std::initializer_list<int> control_ids)
{
	m_focus.build(&m_root, std::vector<int>(control_ids));
}

int control_collection::focused_id() const { return m_focus.focused_id(); }

void control_collection::set_focus(int control_id)
{
	m_focus.set_focus(control_id);
}

void control_collection::set_hover_focus(bool enabled) { m_hover_focus = enabled; }
void control_collection::set_enter_advances_focus(bool enabled) { m_enter_advances_focus = enabled; }
void control_collection::set_default_button(int button_id) { m_default_button_id = button_id; }
void control_collection::set_focus_nav_horizontal(bool enabled) { m_focus.set_horizontal_navigation(enabled); }

void control_collection::set_click_sound(sound_callback cb)
{
	m_default_click_sound = std::move(cb);
}

void control_collection::set_tooltip_delay_ms(uint32_t ms) { m_tooltip_delay_ms = ms; }

void control_collection::set_key_repeat(uint32_t initial_delay_ms, uint32_t repeat_interval_ms)
{
	m_key_repeat_initial_ms = initial_delay_ms;
	m_key_repeat_interval_ms = repeat_interval_ms;
}

void control_collection::set_clipboard_provider(clipboard_get_fn get, clipboard_set_fn set)
{
	m_clipboard_get = std::move(get);
	m_clipboard_set = std::move(set);
}

void control_collection::update(const input_state& input, uint32_t time_ms)
{
	// Edge detection
	m_mouse_released = !input.mouse_left_down && m_prev_input.mouse_left_down;
	bool enter_edge = input.key_enter && !m_prev_input.key_enter;
	bool escape_edge = input.key_escape && !m_prev_input.key_escape;

	// 1. Update tree states (hover, held, pressed)
	m_root.update(input, m_prev_input);

	// 2. Focus management
	m_focus.update(input, m_prev_input, m_hover_focus, time_ms,
	               m_key_repeat_initial_ms, m_key_repeat_interval_ms);

	// 3. Text input handling (chars, keys, clipboard)
	handle_text_input_keys(input, time_ms);

	// 4. Button click events (mouse-up while was-held)
	if (m_mouse_released)
		handle_button_clicks();

	// 5. Enter/Space key handling
	m_enter_pressed = false;
	bool space_edge = input.key_space && !m_prev_input.key_space;

	if (enter_edge)
	{
		auto* f = m_focus.focused();
		if (dynamic_cast<text_input*>(f))
		{
			if (m_enter_advances_focus)
			{
				m_focus.advance(1);
				int after = m_focus.focused_id();

				// Auto-fire if focus landed on the default button
				if (m_default_button_id >= 0 && after == m_default_button_id)
				{
					auto* def_btn = dynamic_cast<button*>(m_root.find(m_default_button_id));
					if (def_btn)
					{
						play_click_sound(def_btn);
						def_btn->fire_click();
					}
				}
			}
			else if (m_default_button_id >= 0)
			{
				// No advance — fire default button directly from textbox
				auto* def_btn = dynamic_cast<button*>(m_root.find(m_default_button_id));
				if (def_btn)
				{
					play_click_sound(def_btn);
					def_btn->fire_click();
				}
			}
			// Otherwise silently consumed — not reported to screen
		}
		else if (auto* btn = dynamic_cast<button*>(f))
		{
			play_click_sound(btn);
			btn->fire_click();
		}
		else
		{
			// Enter pressed but not consumed by textbox or button
			m_enter_pressed = true;
		}
	}

	// Space activates focused button
	if (space_edge)
	{
		auto* btn = dynamic_cast<button*>(m_focus.focused());
		if (btn)
		{
			play_click_sound(btn);
			btn->fire_click();
		}
	}

	// 6. Toggle button keyboard events
	handle_toggle_keys(input, time_ms);

	// 7. Tooltip delay
	update_tooltip(time_ms);

	// 8. Escape
	m_escape_pressed = escape_edge;

	// 9. Store previous input
	m_prev_input = input;
}

void control_collection::render() const
{
	m_root.render();
}

const char* control_collection::active_tooltip() const { return m_active_tooltip; }
bool control_collection::enter_pressed() const { return m_enter_pressed; }
bool control_collection::escape_pressed() const { return m_escape_pressed; }

void control_collection::debug_walk(std::function<void(const control& c)> fn) const
{
	m_root.walk([&fn](const control& c) {
		if (c.is_effectively_visible())
			fn(c);
	});
}

void control_collection::set_debug_draw(bool enabled) { m_debug_draw = enabled; }
bool control_collection::debug_draw_enabled() const { return m_debug_draw; }

void control_collection::discard_pending_input(const input_state& input)
{
	m_prev_input = input;
}

void control_collection::play_click_sound(control* ctrl)
{
	// Check button override
	if (auto* btn = dynamic_cast<button*>(ctrl))
	{
		if (btn->has_click_sound_override())
		{
			if (btn->click_sound()) btn->click_sound()();
			return;
		}
	}
	// Check toggle override
	if (auto* tog = dynamic_cast<toggle_button*>(ctrl))
	{
		if (tog->has_click_sound_override())
		{
			if (tog->click_sound()) tog->click_sound()();
			return;
		}
	}
	// Fall back to collection default
	if (m_default_click_sound)
		m_default_click_sound();
}

void control_collection::handle_button_clicks()
{
	m_root.walk([this](control& c) {
		auto* btn = dynamic_cast<button*>(&c);
		if (btn && btn->was_clicked())
		{
			play_click_sound(btn);
			btn->fire_click();
		}
	});
}

void control_collection::handle_enter_key()
{
	// Handled inline in update()
}

void control_collection::handle_toggle_keys(const input_state& input, uint32_t time_ms)
{
	auto* focused = m_focus.focused();
	auto* toggle = dynamic_cast<toggle_button*>(focused);

	if (!toggle)
	{
		m_toggle_repeat_dir = 0;

		// Also handle mouse clicks on toggle buttons
		m_root.walk([this](control& c) {
			auto* tog = dynamic_cast<toggle_button*>(&c);
			if (!tog) return;
			if (tog->minus_clicked())
			{
				play_click_sound(tog);
				tog->fire_change(-1);
			}
			if (tog->plus_clicked())
			{
				play_click_sound(tog);
				tog->fire_change(+1);
			}
		});
		return;
	}

	// Handle mouse clicks on the focused toggle
	if (toggle->minus_clicked())
	{
		play_click_sound(toggle);
		toggle->fire_change(-1);
	}
	if (toggle->plus_clicked())
	{
		play_click_sound(toggle);
		toggle->fire_change(+1);
	}

	// Handle mouse clicks on OTHER toggles
	m_root.walk([this, toggle](control& c) {
		auto* tog = dynamic_cast<toggle_button*>(&c);
		if (!tog || tog == toggle) return;
		if (tog->minus_clicked())
		{
			play_click_sound(tog);
			tog->fire_change(-1);
		}
		if (tog->plus_clicked())
		{
			play_click_sound(tog);
			tog->fire_change(+1);
		}
	});

	// Keyboard arrows on focused toggle
	bool left = input.key_left;
	bool right = input.key_right;
	bool left_pressed = left && !m_prev_input.key_left;
	bool right_pressed = right && !m_prev_input.key_right;

	int toggle_delta = 0;

	if (left_pressed)
	{
		toggle_delta = -1;
		m_toggle_repeat_dir = -1;
		m_toggle_repeat_next_ms = time_ms + m_key_repeat_initial_ms;
	}
	else if (right_pressed)
	{
		toggle_delta = 1;
		m_toggle_repeat_dir = 1;
		m_toggle_repeat_next_ms = time_ms + m_key_repeat_initial_ms;
	}
	else if (m_toggle_repeat_dir != 0)
	{
		bool still_held = (m_toggle_repeat_dir == -1 && left)
		                  || (m_toggle_repeat_dir == 1 && right);

		if (still_held && time_ms >= m_toggle_repeat_next_ms)
		{
			toggle_delta = m_toggle_repeat_dir;
			m_toggle_repeat_next_ms = time_ms + m_key_repeat_interval_ms;
		}
		else if (!still_held)
		{
			m_toggle_repeat_dir = 0;
		}
	}

	if (toggle_delta != 0)
	{
		play_click_sound(toggle);
		toggle->fire_change(toggle_delta);
	}
}

void control_collection::handle_text_input_keys(const input_state& input, uint32_t time_ms)
{
	auto* focused = m_focus.focused();
	auto* ti = dynamic_cast<text_input*>(focused);

	if (!ti)
	{
		m_text_repeat_key = 0;
		return;
	}

	// Update blink timer
	ti->update_blink(time_ms);

	// Route typed characters
	for (int i = 0; i < input.typed_char_count; i++)
		ti->handle_char(input.typed_chars[i]);

	// Edge detection helpers
	bool backspace_edge = input.key_backspace && !m_prev_input.key_backspace;
	bool delete_edge = input.key_delete && !m_prev_input.key_delete;
	bool left_edge = input.key_left && !m_prev_input.key_left;
	bool right_edge = input.key_right && !m_prev_input.key_right;
	bool home_edge = input.key_home && !m_prev_input.key_home;
	bool end_edge = input.key_end && !m_prev_input.key_end;
	bool ctrl_a_edge = input.key_ctrl && input.key_a && !m_prev_input.key_a;
	bool ctrl_c_edge = input.key_ctrl && input.key_c && !m_prev_input.key_c;
	bool ctrl_x_edge = input.key_ctrl && input.key_x && !m_prev_input.key_x;
	bool ctrl_v_edge = input.key_ctrl && input.key_v && !m_prev_input.key_v;

	// Key repeat tracking for backspace, delete, arrows
	auto do_repeat = [&](int key_id, bool edge, bool held) -> bool {
		if (edge)
		{
			m_text_repeat_key = key_id;
			m_text_repeat_next_ms = time_ms + m_key_repeat_initial_ms;
			return true;
		}
		if (m_text_repeat_key == key_id && held)
		{
			if (time_ms >= m_text_repeat_next_ms)
			{
				m_text_repeat_next_ms = time_ms + m_key_repeat_interval_ms;
				return true;
			}
			return false;
		}
		if (m_text_repeat_key == key_id && !held)
			m_text_repeat_key = 0;
		return false;
	};

	// Backspace (with repeat)
	if (do_repeat(1, backspace_edge, input.key_backspace))
		ti->handle_backspace();

	// Delete (with repeat)
	if (do_repeat(2, delete_edge, input.key_delete))
		ti->handle_delete();

	// Left/Right arrows (with repeat) — only when not Ctrl-modified
	if (!input.key_ctrl)
	{
		if (do_repeat(3, left_edge, input.key_left))
			ti->move_cursor_left(input.key_shift);

		if (do_repeat(4, right_edge, input.key_right))
			ti->move_cursor_right(input.key_shift);
	}

	// Home/End (no repeat)
	if (home_edge)
		ti->move_to_start(input.key_shift);
	if (end_edge)
		ti->move_to_end(input.key_shift);

	// Ctrl+A: select all
	if (ctrl_a_edge)
		ti->select_all();

	// Ctrl+C: copy
	if (ctrl_c_edge && m_clipboard_set)
	{
		std::string copied = ti->copy();
		if (!copied.empty())
			m_clipboard_set(copied);
	}

	// Ctrl+X: cut
	if (ctrl_x_edge && m_clipboard_set)
	{
		std::string cut_text = ti->cut();
		if (!cut_text.empty())
			m_clipboard_set(cut_text);
	}

	// Ctrl+V: paste
	if (ctrl_v_edge && m_clipboard_get)
	{
		std::string pasted = m_clipboard_get();
		if (!pasted.empty())
			ti->paste(pasted);
	}
}

void control_collection::update_tooltip(uint32_t time_ms)
{
	m_active_tooltip = nullptr;

	// Find the hovered control with a tooltip
	const control* hovered_with_tooltip = nullptr;
	m_root.walk([&hovered_with_tooltip](const control& c) {
		if (c.is_hovered() && c.tooltip() && c.is_effectively_visible())
			hovered_with_tooltip = &c;
	});

	if (!hovered_with_tooltip)
	{
		m_hover_control_id = -1;
		m_hover_start_ms = 0;
		return;
	}

	int hovered_id = hovered_with_tooltip->id();

	if (hovered_id != m_hover_control_id)
	{
		// Started hovering a new control
		m_hover_control_id = hovered_id;
		m_hover_start_ms = time_ms;
		return;
	}

	// Same control — check if delay has elapsed
	if (time_ms - m_hover_start_ms >= m_tooltip_delay_ms)
		m_active_tooltip = hovered_with_tooltip->tooltip();
}

} // namespace cc
