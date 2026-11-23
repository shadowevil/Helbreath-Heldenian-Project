#pragma once

#include "control.h"

namespace cc {

class panel : public control
{
public:
	panel(int id, rect local_bounds);

	bool is_focusable() const override { return false; }
};

} // namespace cc
