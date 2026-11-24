#pragma once

#include "input_state.h"
#include "game_geometry.h"

namespace hb::client {

// The mouse in world-target units (UI units -> window -> world rect), unclamped: also the units of
// anything drawn inside push/pop_world_rect_view (the HUD). Equals the UI mouse when the rects coincide.
hb::shared::geometry::GamePoint mouse_in_world_units();

// mouse_in_world_units() clamped to the logical area:
// for world picking (tiles, objects, edge scroll). Equals the UI mouse when the rects coincide.
hb::shared::geometry::GamePoint world_mouse();

// Fills a cc::input_state from the engine input system.
// Includes mouse, keyboard, modifier keys, text input keys, and typed characters.
// Call once per frame in on_update() before m_controls.update().
void fill_input_state(cc::input_state& input);

} // namespace hb::client
