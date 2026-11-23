// animation_state.h: Clean animation controller replacing scattered CTile fields
//
// Manages frame advancement, timing, looping, and completion detection
// for entity animations. Caller computes final frame time with modifiers.
//////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include "direction_helpers.h"

using hb::shared::direction::direction;

struct animation_state
{
	// --- Config (set when action changes) ---
	int8_t  m_action        = 0;    // hb::shared::action::Type::stop, MOVE, RUN, ATTACK, etc.
	direction m_dir         = direction{};  // Facing direction (1-8)
	int16_t m_max_frame      = 0;    // Total frame count
	int16_t m_frame_time     = 0;    // Milliseconds per frame (final, after modifiers)
	bool    m_loop          = true; // STOP/MOVE/RUN loop; others play once

	// --- Playback State ---
	int8_t   m_current_frame  = 0;
	int8_t   m_previous_frame = -1;  // -1 forces frame_changed() true initially
	uint32_t m_last_frame_time = 0;  // 0 = not started yet
	bool     m_finished       = false;

	// --- Lifecycle ---
	void reset();
	void set_action(int8_t action, direction dir,
	               int16_t maxFrame, int16_t frameTime, bool loop,
	               int8_t startFrame = 0);
	void set_direction(direction dir);

	// --- Per-frame update. Returns true if frame changed ---
	bool update(uint32_t current_time);

	// --- Queries ---
	bool is_finished() const    { return m_finished; }
	bool frame_changed() const  { return m_current_frame != m_previous_frame; }
	bool just_changed_to(int8_t frame) const {
		return m_current_frame == frame && m_previous_frame != frame;
	}
};
