#pragma once

namespace hb::shared::balance
{
	constexpr int angelic_bonus          = 16;
	constexpr int swing_str_divisor      = 13;
	constexpr int swing_frames           = 8;
	constexpr int base_frame_time        = 78;
	constexpr int delay_per_frame        = 12;
	constexpr int run_frame_time         = 39;
	constexpr int weight_units_per_stone = 1000;
	constexpr int equip_str_threshold    = 11 * weight_units_per_stone;
}
