#pragma once

#include "control.h"
#include <string>

namespace cc {

class label : public control
{
public:
	label(int id, rect local_bounds, const char* text = "");

	void set_text(const char* text);
	void set_text(const std::string& text);
	const std::string& text() const;

	bool is_focusable() const override { return false; }

private:
	std::string m_text;
};

} // namespace cc
