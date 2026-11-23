// event.h: Discrete event type for application lifecycle and game events
//
// Two complementary systems handle different concerns:
//   IInput (pull/polled state) — keyboard and mouse state, queried by game code
//   event (push/discrete) — things that happen TO the application (close, resize, focus, timer, etc.)
//
// Input (keyboard, mouse) does NOT flow through events — the application base
// class routes IWindowEventHandler input callbacks directly into IInput.
//////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>

namespace hb::shared::render {

// Event ID ranges:
//   0-999    Engine-reserved (window lifecycle, etc.)
//   1000+    Game-defined (timer_tick, network, quest, etc.)
namespace event_id {

	// Window lifecycle (0-99)
	constexpr uint32_t closed       = 0;
	constexpr uint32_t resized      = 1;
	constexpr uint32_t focus_gained = 2;
	constexpr uint32_t focus_lost   = 3;

	// Game-defined events start here
	constexpr uint32_t user         = 1000;

} // namespace event_id

struct event
{
	uint32_t id = 0;

	// Payload — use the field matching the event ID
	union
	{
		struct { int width; int height; }   resize;
		struct { uint32_t elapsed_ms; }     timer;
		uintptr_t                           user_data;  // game-defined payload
	};

	// Convenience factories for engine events
	static event make_closed();
	static event make_resized(int w, int h);
	static event make_focus_gained();
	static event make_focus_lost();
};

} // namespace hb::shared::render
