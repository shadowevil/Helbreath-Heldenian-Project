#pragma once

#include "PacketHeaders.h"
#include "NetConstants.h"
#include "Appearance.h"
#include "PlayerStatusData.h"

#include <cstdint>

namespace hb {
namespace net {
	HB_PACK_BEGIN
	struct HB_PACKED PacketResponseInitPlayer {
		PacketHeader header;
	};

	struct HB_PACKED PacketResponseCivilRight {
		PacketHeader header;
		char location[hb::shared::limits::MapNameLen];
	};

	struct HB_PACKED PacketResponseRetrieveItem {
		PacketHeader header;
		int8_t bank_index;
		int8_t item_index;
	};

	struct HB_PACKED PacketResponseNoticementHeader {
		PacketHeader header;
	};

	struct HB_PACKED PacketResponseNoticementText {
		PacketHeader header;
		char text[1];
	};

	struct HB_PACKED PacketResponseMotionHeader {
		PacketHeader header;
	};

	struct HB_PACKED PacketResponsePanningHeader {
		PacketHeader header;
		int16_t x;
		int16_t y;
		uint8_t dir;
	};

	struct HB_PACKED PacketResponseChargedTeleport {
		PacketHeader header;
		int16_t reason;
	};

	struct HB_PACKED PacketResponseTeleportListHeader {
		PacketHeader header;
		int32_t count;
	};

	struct HB_PACKED PacketResponseTeleportListEntry {
		int32_t index;
		char map_name[hb::shared::limits::MapNameLen];
		int32_t x;
		int32_t y;
		int32_t cost;
	};

	struct HB_PACKED PacketResponseItemListHeader {
		PacketHeader header;
		uint8_t item_count;
	};

	struct HB_PACKED PacketResponseItemListEntry {
		char name[hb::shared::limits::ItemNameLen];
		uint64_t count;
		uint8_t item_type;
		uint8_t equip_pos;
		uint8_t is_equipped;
		int16_t level_limit;
		uint8_t gender_limit;
		uint16_t cur_durability;
		uint16_t weight;
		uint8_t item_color;
		uint8_t spec_value2;
		uint8_t custom_made;
		uint8_t prefix_type;
		uint8_t prefix_value;
		uint8_t secondary_type;
		uint8_t secondary_value;
		uint8_t enchant_bonus;
		int16_t item_id;           // Item ID for config lookup
		uint16_t max_durability;     // Maximum durability
	};

	struct HB_PACKED PacketResponseBankItemListHeader {
		uint16_t bank_item_count;
	};

	struct HB_PACKED PacketResponseBankItemEntry {
		char name[hb::shared::limits::ItemNameLen];
		uint64_t count;
		uint8_t item_type;
		uint8_t equip_pos;
		int16_t level_limit;
		uint8_t gender_limit;
		uint16_t cur_durability;
		uint16_t weight;
		uint8_t item_color;
		uint8_t spec_value2;
		uint8_t custom_made;
		uint8_t prefix_type;
		uint8_t prefix_value;
		uint8_t secondary_type;
		uint8_t secondary_value;
		uint8_t enchant_bonus;
		int16_t item_id;           // Item ID for config lookup
		uint16_t max_durability;     // Maximum durability
	};

	struct HB_PACKED PacketResponseMasteryData {
		char magic_mastery[hb::shared::limits::MaxMagicType];
		uint8_t skill_mastery[hb::shared::limits::MaxSkillType];
	};

	struct HB_PACKED PacketResponseDynamicObject {
		PacketHeader header;
		int16_t x;
		int16_t y;
		int16_t v1;
		int16_t v2;
		int16_t v3;
	};

	struct HB_PACKED PacketResponsePlayerCharacterContents {
		PacketHeader header;
		int32_t hp;
		int32_t mp;
		int32_t sp;
		int32_t ac;
		int32_t thac0;
		int32_t level;
		int32_t str;
		int32_t intel;
		int32_t vit;
		int32_t dex;
		int32_t mag;
		int32_t chr;
		uint16_t lu_point;
		uint8_t lu_unused[5];
		uint32_t exp;
		int32_t enemy_kills;
		int32_t pk_count;
		uint32_t reward_gold;
		char location[hb::shared::limits::MapNameLen];
		uint8_t super_attack_left;
		int16_t max_stats;
		int32_t max_level;
		int16_t max_bank_items;
	};

	struct HB_PACKED PacketResponseInitDataHeader {
		PacketHeader header;
		int16_t player_object_id;
		int16_t pivot_x;
		int16_t pivot_y;
		int16_t player_type;
		hb::shared::entity::PlayerAppearance appearance;
		hb::shared::entity::PlayerStatus status;
		char map_name[hb::shared::limits::MapNameLen];
		char cur_location[hb::shared::limits::MapNameLen];
		uint8_t sprite_alpha;
		uint8_t weather_status;
		int32_t contribution;
		uint8_t observer_mode;
		int32_t rating;
		int32_t hp;
		uint8_t discount;
	};
	HB_PACK_END
}
}
