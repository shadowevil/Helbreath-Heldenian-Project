#pragma once

#include "input_state.h"

namespace hb::client {

// Fills a cc::input_state from the engine input system.
// Includes mouse, keyboard, modifier keys, text input keys, and typed characters.
// Call once per frame in on_update() before m_controls.update().
void fill_input_state(cc::input_state& input);

} // namespace hb::client
