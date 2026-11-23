#include "toggle_button.h"

namespace cc {

toggle_button::toggle_button(int id, rect local_bounds, rect minus_rect, rect plus_rect)
	: control(id, local_bounds)
	, m_minus_rect(minus_rect)
	, m_plus_rect(plus_rect)
{
}

void toggle_button::set_on_change(value_change_callback cb) { m_on_change = std::move(cb); }

void toggle_button::set_click_sound(sound_callback cb)
{
	m_click_sound = std::move(cb);
	m_has_sound_override = true;
}

bool toggle_button::has_click_sound_override() const { return m_has_sound_override; }
const sound_callback& toggle_button::click_sound() const { return m_click_sound; }

bool toggle_button::is_minus_hovered() const { return m_minus_hovered; }
bool toggle_button::is_plus_hovered() const { return m_plus_hovered; }
bool toggle_button::is_minus_held() const { return m_minus_held; }
bool toggle_button::is_plus_held() const { return m_plus_held; }
bool toggle_button::minus_clicked() const { return m_minus_clicked; }
bool toggle_button::plus_clicked() const { return m_plus_clicked; }

void toggle_button::fire_change(int delta)
{
	if (m_on_change)
		m_on_change(id(), delta);
}

void toggle_button::update(const input_state& input, const input_state& prev_input)
{
	control::update(input, prev_input);

	if (!is_effectively_visible() || !is_effectively_enabled())
	{
		m_minus_hovered = false;
		m_plus_hovered = false;
		m_minus_held = false;
		m_plus_held = false;
		m_minus_clicked = false;
		m_plus_clicked = false;
		return;
	}

	auto sb = screen_bounds();

	rect minus_screen = {sb.x + m_minus_rect.x, sb.y + m_minus_rect.y,
	                     m_minus_rect.w, m_minus_rect.h};
	rect plus_screen = {sb.x + m_plus_rect.x, sb.y + m_plus_rect.y,
	                    m_plus_rect.w, m_plus_rect.h};

	m_minus_hovered = rect_contains(minus_screen, input.mouse_x, input.mouse_y);
	m_plus_hovered = rect_contains(plus_screen, input.mouse_x, input.mouse_y);
	m_minus_held = m_minus_hovered && input.mouse_left_down;
	m_plus_held = m_plus_hovered && input.mouse_left_down;

	bool mouse_released = !input.mouse_left_down && prev_input.mouse_left_down;
	m_minus_clicked = m_minus_was_held && mouse_released && m_minus_hovered;
	m_plus_clicked = m_plus_was_held && mouse_released && m_plus_hovered;

	m_minus_was_held = m_minus_held;
	m_plus_was_held = m_plus_held;
}

} // namespace cc
