#pragma once

#include "packet/packet_common.h"
#include "entity_relationship.h"
#include <cstdint>
#include <cstring>

// Compile-time guarantee that bool is 1 byte (true on all major compilers).
// This ensures bool fields in packed structs have the same wire size as uint8_t.
static_assert(sizeof(bool) == 1, "bool must be 1 byte for packed struct compatibility");

namespace hb::shared::entity {

// EntityStatus: Base status fields shared by both NPCs and Players.
// Replaces the old packed int32_t status bitmask.
HB_PACK_BEGIN
struct HB_PACKED EntityStatus
{
	// Multi-value fields (0-15 range)
	uint8_t attack_delay;
	uint8_t angel_percent;

	// Boolean flags (shared NPC+Player)
	bool invisibility;
	bool berserk;
	bool frozen;
	bool poisoned;

	// Combat effect flags (apply to both NPCs and players)
	bool inhibition_casting;
	bool illusion;
	bool hero;
	bool defense_shield;
	bool magic_protection;
	bool protection_from_arrow;

	// Per-viewer NPC relationship (set by server before sending, not persisted)
	EntityRelationship relationship;

	void clear() { std::memset(this, 0, sizeof(*this)); }
};

// PlayerStatus: Full status for players. Contains all EntityStatus fields plus player-only fields.
// Uses composition (not inheritance) to remain standard_layout for packet embedding.
// CTile and EntityRenderState store PlayerStatus (the superset).
struct HB_PACKED PlayerStatus
{
	// === EntityStatus fields (must match EntityStatus layout) ===
	uint8_t attack_delay;
	uint8_t angel_percent;
	bool invisibility;
	bool berserk;
	bool frozen;
	bool poisoned;
	bool inhibition_casting;
	bool illusion;
	bool hero;
	bool defense_shield;
	bool magic_protection;
	bool protection_from_arrow;
	EntityRelationship relationship;

	// === Player-only fields ===
	// Angel type flags
	bool angel_str;
	bool angel_dex;
	bool angel_int;
	bool angel_mag;

	// Player effect flags
	bool slate_exp;
	bool haste;
	bool gm_mode;
	bool illusion_movement;
	bool slate_invincible;
	bool slate_mana;

	// Faction/identity flags
	bool hunter;
	bool aresden;
	bool citizen;
	bool pk;

	// AFK state (server-controlled)
	bool afk;

	bool HasAngelType() const
	{
		return angel_str || angel_dex || angel_int || angel_mag;
	}

	void clear() { std::memset(this, 0, sizeof(*this)); }

	// Assign from EntityStatus (NPC -> tile storage). Clears player-only fields.
	void SetFromEntityStatus(const EntityStatus& npc)
	{
		clear();
		attack_delay = npc.attack_delay;
		angel_percent = npc.angel_percent;
		invisibility = npc.invisibility;
		berserk = npc.berserk;
		frozen = npc.frozen;
		poisoned = npc.poisoned;
		inhibition_casting = npc.inhibition_casting;
		illusion = npc.illusion;
		hero = npc.hero;
		defense_shield = npc.defense_shield;
		magic_protection = npc.magic_protection;
		protection_from_arrow = npc.protection_from_arrow;
		relationship = npc.relationship;
	}
};
HB_PACK_END

} // namespace hb::shared::entity
