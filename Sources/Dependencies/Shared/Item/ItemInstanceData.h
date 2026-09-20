#pragma once
#include <algorithm>
#include <cstdint>

namespace hb::shared::item {

// All dynamic/mutable item fields — the per-instance data layered on top of base config.
// item_id is NOT included — it's a lookup key that travels separately
// (CItem::m_id_num, CTile::m_item_id, function parameters, etc.).
// Field names match CItem::copy_attributes_to / load_attributes_from templates
// for the 6 attribute fields (custom_made through enchant_bonus).
struct item_instance_data
{
	uint64_t count = 0;
	int16_t touch_effect_type = 0;
	int16_t touch_effect_value1 = 0;
	int16_t touch_effect_value2 = 0;
	int16_t touch_effect_value3 = 0;
	int16_t special_effect_value1 = 0;
	int16_t special_effect_value2 = 0;
	int16_t special_effect_value3 = 0;
	uint16_t cur_durability = 0;
	int8_t item_color = 0;
	uint8_t custom_made = 0;
	uint8_t prefix_type = 0;
	uint8_t prefix_value = 0;
	uint8_t secondary_type = 0;
	uint8_t secondary_value = 0;
	uint8_t enchant_bonus = 0;

	void clear() { *this = {}; }
	bool has_attributes() const { return custom_made || prefix_type || secondary_type || enchant_bonus; }

	// Populate from any packet type that has matching field names
	// (e.g. PacketEventGroundItem). Keeps this header dependency-free.
	// item_id is NOT set — caller reads pkt.item_id separately.
	template<typename Packet>
	static item_instance_data from_ground_item_packet(const Packet& pkt)
	{
		item_instance_data d;
		d.count = static_cast<uint64_t>(std::max<int16_t>(pkt.count, 0));
		d.item_color = static_cast<int8_t>(pkt.item_color);
		d.touch_effect_type = pkt.touch_effect_type;
		d.touch_effect_value1 = pkt.touch_effect_value1;
		d.touch_effect_value2 = pkt.touch_effect_value2;
		d.touch_effect_value3 = pkt.touch_effect_value3;
		d.special_effect_value1 = pkt.special_effect_value1;
		d.special_effect_value2 = pkt.special_effect_value2;
		d.special_effect_value3 = pkt.special_effect_value3;
		d.cur_durability = pkt.cur_durability;
		d.custom_made = pkt.custom_made;
		d.prefix_type = pkt.prefix_type;
		d.prefix_value = pkt.prefix_value;
		d.secondary_type = pkt.secondary_type;
		d.secondary_value = pkt.secondary_value;
		d.enchant_bonus = pkt.enchant_bonus;
		return d;
	}
};

}
