#pragma once
#include <cstdint>

// Object ID range constants — replaces magic 10000/30000 comparisons.
// The server assigns object IDs in distinct ranges for players, NPCs,
// and nearby-offset (motion event) IDs.

namespace hb::shared::object_id {

	constexpr uint16_t PlayerMax = 10000;    // IDs 0..9999 are players
	constexpr uint16_t NpcMin = 10000;       // IDs 10000..29999 are NPCs
	constexpr uint16_t NpcMax = 30000;
	constexpr uint16_t NearbyOffset = 30000; // Motion events add this offset

	inline bool is_player_id(uint16_t id) { return id < PlayerMax; }
	inline bool IsNpcID(uint16_t id) { return id >= NpcMin && id < NpcMax; }
	inline bool IsNearbyOffset(uint16_t id) { return id >= NearbyOffset; }
	inline uint16_t ToRealID(uint16_t id) { return id - NearbyOffset; }
	inline uint16_t ToNpcIndex(uint16_t id) { return id - NpcMin; }

} // namespace hb::shared::object_id
