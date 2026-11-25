// bake_debounce.h: When to rebake scale-dependent textures during a live window resize
//
// Textures baked at the window's UI scale (copyright lines, title glow, login labels) are redone when the scale
// changes. A drag-resize changes it every frame, so a rebake waits until the scale has held for a few frames;
// the old textures keep drawing at their baked size until then.
//////////////////////////////////////////////////////////////////////

#pragma once

#include <cmath>

namespace hb::client
{
	class bake_debounce
	{
	public:
		static constexpr int stable_frames = 8;

		// Call once per frame with the scale the textures were baked for (0 = never baked) and the scale now.
		// True when they should be rebaked: at once for the first bake, else once the new scale has held
		// stable_frames frames.
		bool update(float baked, float now)
		{
			if (baked == 0.0f)
				return true;
			if (std::fabs(now - baked) <= tolerance)
			{
				m_pending = baked;
				m_frames = 0;
				return false;
			}
			if (std::fabs(now - m_pending) > tolerance)
			{
				m_pending = now;
				m_frames = 0;
				return false;
			}
			return ++m_frames >= stable_frames;
		}

	private:
		static constexpr float tolerance = 0.001f;

		float m_pending = 0.0f;
		int m_frames = 0;
	};
}
