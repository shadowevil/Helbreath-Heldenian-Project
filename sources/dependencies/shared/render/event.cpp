// event.cpp: Factory method implementations for event struct
//////////////////////////////////////////////////////////////////////

#include "event.h"

namespace hb::shared::render {

event event::make_closed()
{
	event e{};
	e.id = event_id::closed;
	return e;
}

event event::make_resized(int w, int h)
{
	event e{};
	e.id = event_id::resized;
	e.resize.width = w;
	e.resize.height = h;
	return e;
}

event event::make_focus_gained()
{
	event e{};
	e.id = event_id::focus_gained;
	return e;
}

event event::make_focus_lost()
{
	event e{};
	e.id = event_id::focus_lost;
	return e;
}

} // namespace hb::shared::render
