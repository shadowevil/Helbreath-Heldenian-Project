#include "textbox.h"

namespace cc {

textbox::textbox(int id, rect local_bounds, int max_length)
	: text_input(id, local_bounds, max_length)
{
}

void textbox::set_validator(validate_callback cb) { m_validator = std::move(cb); }

bool textbox::is_valid() const { return m_valid; }

void textbox::update(const input_state& input, const input_state& prev_input)
{
	control::update(input, prev_input);

	// Re-validate each frame (text may change via text_input methods)
	if (m_validator)
		m_valid = m_validator(m_text);
}

void textbox::on_focus_lost()
{
	text_input::on_focus_lost();

	// Run final validation on focus loss
	if (m_validator)
		m_valid = m_validator(m_text);
}

} // namespace cc
