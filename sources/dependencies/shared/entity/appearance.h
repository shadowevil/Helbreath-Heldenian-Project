#pragma once

#include "packet/packet_common.h"
#include <cstdint>
#include <cstring>

namespace hb::shared::entity {

HB_PACK_BEGIN

// EntityAppearance: NPC appearance using what was appr2.
// NPCs don't have equipment; appr2 encodes sub-type and special frame.
struct HB_PACKED EntityAppearance
{
	uint8_t sub_type;        // (appr2 & 0xFF00) >> 8  (crop type, crusade ownership)
	uint8_t special_frame;   // appr2 & 0x00FF          (NPC special animation frame)

	bool HasSpecialState() const { return sub_type != 0 || special_frame != 0; }
	void clear() { sub_type = 0; special_frame = 0; }
};

// PlayerAppearance: Individual fields that replace sAppr1-4 + iApprColor.
// Used in packet structs (must be packed) and in CClient/CTile storage.
struct HB_PACKED PlayerAppearance
{
	// Body (was sAppr1)
	uint8_t underwear_type;  // sAppr1 & 0x000F
	uint8_t hair_color;      // (sAppr1 & 0x00F0) >> 4
	uint8_t hair_style;      // (sAppr1 & 0x0F00) >> 8
	uint8_t skin_color;      // (sAppr1 & 0xF000) >> 12

	// State
	bool is_walking;         // combat mode flag

	// Effects (was sAppr4)
	uint8_t weapon_glare;    // sAppr4 & 0x0003
	uint8_t shield_glare;    // (sAppr4 & 0x000C) >> 2
	uint8_t effect_type;     // (sAppr4 & 0x00F0) >> 4
	bool hide_armor;         // (sAppr4 & 0x0080) >> 7

	// Equipment flags (computed at broadcast time)
	bool is_skirt;           // true when equipped pants has appearance_value == 1

	// Colors (was iApprColor)
	uint8_t weapon_color;    // (iApprColor >> 28) & 0xF
	uint8_t shield_color;    // (iApprColor >> 24) & 0xF
	uint8_t armor_color;     // (iApprColor >> 20) & 0xF
	uint8_t mantle_color;    // (iApprColor >> 16) & 0xF
	uint8_t arm_color;       // (iApprColor >> 12) & 0xF
	uint8_t pants_color;     // (iApprColor >> 8) & 0xF
	uint8_t boots_color;     // (iApprColor >> 4) & 0xF
	uint8_t helm_color;      // iApprColor & 0xF

	// NPC sub-type/special frame (always 0 for players, populated from EntityAppearance for NPCs)
	uint8_t sub_type;        // (was appr2 upper byte for NPCs) crop type, crusade ownership
	uint8_t special_frame;   // (was appr2 lower byte for NPCs) NPC special animation frame

	// Equipment item_id + display_id pairs (populated at broadcast time, not stored in CClient)
	// item_id: server item config ID (used in-game to look up config cache → display_id → render)
	// display_id: sprite rendering key (used on char select when config cache unavailable)
	int16_t helm_item_id = 0;      int16_t helm_display_id = -1;
	int16_t armor_item_id = 0;     int16_t armor_display_id = -1;
	int16_t arm_item_id = 0;       int16_t arm_display_id = -1;
	int16_t pants_item_id = 0;     int16_t pants_display_id = -1;
	int16_t boots_item_id = 0;     int16_t boots_display_id = -1;
	int16_t weapon_item_id = 0;    int16_t weapon_display_id = -1;
	int16_t shield_item_id = 0;    int16_t shield_display_id = -1;
	int16_t mantle_item_id = 0;    int16_t mantle_display_id = -1;

	bool HasNpcSpecialState() const { return sub_type != 0 || special_frame != 0; }

	void clear() { std::memset(this, 0, sizeof(*this)); }

	// Populate NPC fields from an EntityAppearance (used client-side after receiving NPC packets)
	void SetFromNpcAppearance(const EntityAppearance& npc)
	{
		clear();
		sub_type = npc.sub_type;
		special_frame = npc.special_frame;
	}
};

HB_PACK_END

} // namespace hb::shared::entity
