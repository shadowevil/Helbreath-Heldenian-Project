#include "check_box.h"

namespace cc {

check_box::check_box(int id, rect local_bounds, bool checked)
	: button(id, local_bounds)
	, m_checked(checked)
{
}

bool check_box::is_checked() const { return m_checked; }
void check_box::set_checked(bool checked) { m_checked = checked; }
void check_box::set_on_change(check_change_callback cb) { m_on_change = std::move(cb); }

void check_box::fire_click()
{
	if (!is_effectively_enabled())
		return;
	m_checked = !m_checked;
	// Runs inside the collection's tree walk, as a button's on_click does: defer any rebuild of the controls
	if (m_on_change)
		m_on_change(id(), m_checked);
}

} // namespace cc
