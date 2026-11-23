// spell_aoe.h: Spell area-of-effect tile calculator for targeting overlay
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>

struct spell_aoe_tile
{
	int m_x, m_y;
};

// Parameters extracted from CMagic for AoE calculation
struct spell_aoe_params
{
	int m_magic_type;      // DEF_MAGICTYPE_*
	int m_aoe_radius_x;     // Server m_value_2 — area spell X radius
	int m_aoe_radius_y;     // Server m_value_3 — area spell Y radius
	int m_dynamic_pattern; // Server m_sValue11 — 1=wall, 2=field
	int m_dynamic_radius;  // Server m_sValue12 — wall length / field radius
};

namespace SpellAoE
{
	// Calculates all affected tiles for a spell cast from (casterX,casterY)
	// targeting (targetX,targetY). Returns number of tiles written to outTiles.
	int calculate_tiles(const spell_aoe_params& params,
		int casterX, int casterY,
		int targetX, int targetY,
		spell_aoe_tile* outTiles, int outMax);
}
