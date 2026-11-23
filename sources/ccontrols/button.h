#pragma once

#include "control.h"

namespace cc {

using button_callback = std::function<void(int button_id)>;

class button : public control
{
public:
	button(int id, rect local_bounds);

	bool is_focusable() const override { return true; }

	void set_on_click(button_callback cb);

	void set_click_sound(sound_callback cb);
	bool has_click_sound_override() const;
	const sound_callback& click_sound() const;

	void set_confirm_mode(bool confirm);
	bool is_confirm_pending() const;

	bool was_clicked() const;

	void update(const input_state& input, const input_state& prev_input) override;

private:
	friend class control_collection;
	void fire_click();

	button_callback m_on_click;
	sound_callback m_click_sound;
	bool m_has_sound_override = false;
	bool m_confirm_mode = false;
	bool m_confirm_pending = false;
	bool m_was_clicked = false;
	bool m_was_held_prev = false;
};

} // namespace cc
