#pragma once

#include <cstdint>

// 8-directional movement offset helpers.
// Directions: 1=N, 2=NE, 3=E, 4=SE, 5=S, 6=SW, 7=W, 8=NW (0=none)

namespace hb::shared::direction {

	enum direction : int8_t
	{
		north     = 1,
		northeast = 2,
		east      = 3,
		southeast = 4,
		south     = 5,
		southwest = 6,
		west      = 7,
		northwest = 8,
	};
	constexpr int8_t direction_count = 8;

	// Lookup tables indexed by direction (0 unused, 1-8 valid)
	constexpr int OffsetX[] = { 0,  0, 1, 1, 1, 0, -1, -1, -1 };
	constexpr int OffsetY[] = { 0, -1, -1, 0, 1, 1,  1,  0, -1 };

	// Apply directional offset to coordinates.
	inline void ApplyOffset(direction dir, int& x, int& y)
	{
		if (dir >= 1 && dir <= 8)
		{
			x += OffsetX[dir];
			y += OffsetY[dir];
		}
	}

	inline void ApplyOffset(direction dir, short& x, short& y)
	{
		if (dir >= 1 && dir <= 8)
		{
			x += static_cast<short>(OffsetX[dir]);
			y += static_cast<short>(OffsetY[dir]);
		}
	}

	inline direction& operator++(direction& d)
	{
		d = static_cast<direction>(static_cast<int8_t>(d) + 1);
		return d;
	}

	inline direction operator++(direction& d, int)
	{
		direction old = d;
		++d;
		return old;
	}

} // namespace hb::shared::direction
