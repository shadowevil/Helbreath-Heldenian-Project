#pragma once

#include "control.h"

namespace cc {

using value_change_callback = std::function<void(int control_id, int delta)>;

class toggle_button : public control
{
public:
	toggle_button(int id, rect local_bounds, rect minus_rect, rect plus_rect);

	bool is_focusable() const override { return true; }

	void set_on_change(value_change_callback cb);

	void set_click_sound(sound_callback cb);
	bool has_click_sound_override() const;
	const sound_callback& click_sound() const;

	bool is_minus_hovered() const;
	bool is_plus_hovered() const;
	bool is_minus_held() const;
	bool is_plus_held() const;

	bool minus_clicked() const;
	bool plus_clicked() const;

	void update(const input_state& input, const input_state& prev_input) override;

private:
	friend class control_collection;
	void fire_change(int delta);

	rect m_minus_rect;
	rect m_plus_rect;
	value_change_callback m_on_change;
	sound_callback m_click_sound;
	bool m_has_sound_override = false;
	bool m_minus_hovered = false;
	bool m_plus_hovered = false;
	bool m_minus_held = false;
	bool m_plus_held = false;
	bool m_minus_clicked = false;
	bool m_plus_clicked = false;
	bool m_minus_was_held = false;
	bool m_plus_was_held = false;
};

} // namespace cc
