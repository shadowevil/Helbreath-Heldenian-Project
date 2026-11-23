// animation_state.cpp: Animation controller implementation
//
//////////////////////////////////////////////////////////////////////

#include "animation_state.h"

using namespace hb::shared::direction;

//=============================================================================
// reset - clear all state back to defaults
//=============================================================================
void animation_state::reset()
{
	m_action        = 0;
	m_dir           = direction{};
	m_max_frame      = 0;
	m_frame_time     = 0;
	m_loop          = true;
	m_current_frame  = 0;
	m_previous_frame = -1;
	m_last_frame_time = 0;
	m_finished       = false;
}

//=============================================================================
// set_action - Configure for a new action (resets playback)
//=============================================================================
void animation_state::set_action(int8_t action, direction dir,
                               int16_t maxFrame, int16_t frameTime, bool loop,
                               int8_t startFrame)
{
	m_action        = action;
	m_dir           = dir;
	m_max_frame      = maxFrame;
	m_frame_time     = frameTime;
	m_loop          = loop;
	m_current_frame  = startFrame;
	m_previous_frame = -1;  // Force frame_changed() true on first check
	m_last_frame_time = 0;  // Will be set on first update()
	m_finished       = false;
}

//=============================================================================
// set_direction - Change facing without resetting animation
//=============================================================================
void animation_state::set_direction(direction dir)
{
	m_dir = dir;
}

//=============================================================================
// update - Advance animation based on elapsed time
//
// Preserves original Helbreath frame-advance behavior:
// - First call sets timestamp, shows first frame for full duration
// - Checks elapsed time against frame_time
// - Frame skip up to 3 on lag (original catchup behavior)
// - Non-looping: finished when past max_frame
// - Looping: wraps to 0
//
// Returns true if frame changed this call
//=============================================================================
bool animation_state::update(uint32_t current_time)
{
	if (m_finished) return false;
	if (m_frame_time <= 0) return false;

	m_previous_frame = m_current_frame;

	// First call: set timestamp, show first frame for full duration
	if (m_last_frame_time == 0)
	{
		m_last_frame_time = current_time;
		return false;
	}

	uint32_t elapsed = current_time - m_last_frame_time;
	if (elapsed <= static_cast<uint32_t>(m_frame_time))
		return false;

	// Frame skip on lag (original behavior: skip up to 3 frames)
	if (elapsed >= static_cast<uint32_t>(m_frame_time + m_frame_time))
	{
		int skip_frame = static_cast<int>(elapsed / static_cast<uint32_t>(m_frame_time));
		if (skip_frame > 3) skip_frame = 3;
		m_current_frame += static_cast<int8_t>(skip_frame);
	}
	else
	{
		m_current_frame++;
	}

	m_last_frame_time = current_time;

	// Check frame boundary
	if (m_current_frame > m_max_frame)
	{
		if (m_loop)
		{
			m_current_frame = 0;
		}
		else
		{
			m_finished = true;
		}
	}

	return m_current_frame != m_previous_frame;
}
