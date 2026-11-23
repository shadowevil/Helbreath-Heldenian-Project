// game_events.h: Game-defined event IDs (user range: 1000+)
//
// Engine events (0-999) are defined in event.h.
// Game code defines its own discrete events here.
//////////////////////////////////////////////////////////////////////

#pragma once

#include "event.h"

namespace game_event_id {

constexpr uint32_t timer_tick   = hb::shared::render::event_id::user + 0;
// future: network_data, quest_update, etc.

} // namespace game_event_id
