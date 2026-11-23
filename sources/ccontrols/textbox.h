#pragma once

#include "text_input.h"
#include <string>
#include <cstdint>

namespace cc {

using validate_callback = std::function<bool(const std::string& text)>;

class textbox : public text_input
{
public:
	textbox(int id, rect local_bounds, int max_length);

	void set_validator(validate_callback cb);
	bool is_valid() const;

	void update(const input_state& input, const input_state& prev_input) override;
	void on_focus_lost() override;

private:
	validate_callback m_validator;
	mutable bool m_valid = true;
};

} // namespace cc
