#include "label.h"

namespace cc {

label::label(int id, rect local_bounds, const char* text)
	: control(id, local_bounds)
	, m_text(text ? text : "")
{
}

void label::set_text(const char* text)
{
	m_text = text ? text : "";
}

void label::set_text(const std::string& text)
{
	m_text = text;
}

const std::string& label::text() const
{
	return m_text;
}

} // namespace cc
