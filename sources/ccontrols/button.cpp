#include "button.h"

namespace cc {

button::button(int id, rect local_bounds)
	: control(id, local_bounds)
{
}

void button::set_on_click(button_callback cb) { m_on_click = std::move(cb); }

void button::set_click_sound(sound_callback cb)
{
	m_click_sound = std::move(cb);
	m_has_sound_override = true;
}

bool button::has_click_sound_override() const { return m_has_sound_override; }
const sound_callback& button::click_sound() const { return m_click_sound; }

void button::set_confirm_mode(bool confirm) { m_confirm_mode = confirm; }

bool button::is_confirm_pending() const { return m_confirm_pending; }

bool button::was_clicked() const { return m_was_clicked; }

void button::fire_click()
{
	if (m_on_click)
		m_on_click(id());
}

void button::update(const input_state& input, const input_state& prev_input)
{
	control::update(input, prev_input);

	bool mouse_released = !input.mouse_left_down && prev_input.mouse_left_down;

	// Click = was held last frame, mouse released this frame, still hovered
	m_was_clicked = m_was_held_prev && mouse_released && is_hovered()
	                && is_effectively_enabled();

	// Confirm mode: first click focuses, second click (while focused) activates
	if (m_confirm_mode && m_was_clicked && !is_focused())
	{
		m_was_clicked = false;
		m_confirm_pending = true;
	}

	m_was_held_prev = is_held();
}

} // namespace cc
