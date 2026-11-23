#pragma once

namespace hb::shared
{
	namespace character_class
	{
		enum character_class : int
		{
			all      = 0, // selector: give item to every class
			warrior  = 1,
			mage     = 2,
			master   = 3,
		};

		constexpr int max = 4;

		constexpr bool is_valid_player_class(int c)
		{
			return c == warrior || c == mage || c == master;
		}
	}
}
