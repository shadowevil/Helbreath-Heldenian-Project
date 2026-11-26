#pragma once

#include "button.h"

#include <functional>

namespace cc {

using check_change_callback = std::function<void(int control_id, bool checked)>;

// A two-state box: a click, Enter or Space toggles it and reports the new state.
// Clicks, focus, the click sound and tooltips work as for cc::button; on_click is not used.
class check_box : public button
{
public:
	check_box(int id, rect local_bounds, bool checked = false);

	bool is_checked() const;
	void set_checked(bool checked);   // fires no callback

	void set_on_change(check_change_callback cb);

protected:
	void fire_click() override;

private:
	check_change_callback m_on_change;
	bool m_checked;
};

} // namespace cc
