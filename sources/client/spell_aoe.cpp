// spell_aoe.cpp: Spell area-of-effect tile calculator
//
// Calculates which tiles are affected by a spell based on server logic.
// Used by the debug spell targeting overlay.
//
//////////////////////////////////////////////////////////////////////

#include "spell_aoe.h"
#include "magic.h"
#include "game_geometry.h"
#include <cstdlib>

// Helper: add tile if not already present and under limit
static int add_unique(spell_aoe_tile* tiles, int count, int maxCount, int x, int y)
{
	if (count >= maxCount) return count;
	for (int i = 0; i < count; i++) {
		if (tiles[i].m_x == x && tiles[i].m_y == y) return count;
	}
	tiles[count].m_x = x;
	tiles[count].m_y = y;
	return count + 1;
}

// Helper: add rectangular area from (cx-rx, cy-ry) to (cx+rx, cy+ry)
static int add_area(spell_aoe_tile* tiles, int count, int maxCount,
	int cx, int cy, int rx, int ry)
{
	for (int iy = cy - ry; iy <= cy + ry; iy++)
		for (int ix = cx - rx; ix <= cx + rx; ix++)
			count = add_unique(tiles, count, maxCount, ix, iy);
	return count;
}

// Server's get_next_move_dir — direction from (sX,sY) to (dX,dY)
// Returns direction 1-8 (compass), 0 if same position
static int get_move_dir(int sX, int sY, int dX, int dY)
{
	int absX = sX - dX;
	int absY = sY - dY;
	int ret = 0;

	if (absX == 0 && absY == 0) return 0;
	if (absX == 0) { return (absY > 0) ? 1 : 5; }
	if (absY == 0) { return (absX > 0) ? 7 : 3; }
	if (absX > 0 && absY > 0) return 8;
	if (absX < 0 && absY > 0) return 2;
	if (absX > 0 && absY < 0) return 6;
	if (absX < 0 && absY < 0) return 4;
	return ret;
}

// get perpendicular wall offset from direction (same as server switch)
static void get_wall_offset(int dir, int& rx, int& ry)
{
	switch (dir) {
	case 1:  rx = 1;  ry = 0;  break;
	case 2:  rx = 1;  ry = 1;  break;
	case 3:  rx = 0;  ry = 1;  break;
	case 4:  rx = -1; ry = 1;  break;
	case 5:  rx = 1;  ry = 0;  break;
	case 6:  rx = -1; ry = -1; break;
	case 7:  rx = 0;  ry = -1; break;
	case 8:  rx = 1;  ry = -1; break;
	default: rx = 0;  ry = 0;  break;
	}
}

int SpellAoE::calculate_tiles(const spell_aoe_params& params,
	int casterX, int casterY,
	int targetX, int targetY,
	spell_aoe_tile* outTiles, int outMax)
{
	int count = 0;
	int radiusX = params.m_aoe_radius_x;
	int radiusY = params.m_aoe_radius_y;

	switch (params.m_magic_type)
	{
	// =================================================================
	// Linear spells: Bresenham line (steps 2-9) with cross pattern,
	// plus a rectangular area at the destination point
	// =================================================================
	case hb::shared::magic::DamageLinear:
	case hb::shared::magic::IceLinear:
	case hb::shared::magic::DamageLinearSpDown:
	{
		// Line trace with cross pattern at each step
		for (int i = 2; i < 10; i++) {
			int err = 0;
			int tX, tY;
			hb::shared::geometry::GetPoint2(casterX, casterY, targetX, targetY, &tX, &tY, &err, i);

			// Cross pattern: center + 4 cardinal neighbors
			count = add_unique(outTiles, count, outMax, tX, tY);
			count = add_unique(outTiles, count, outMax, tX - 1, tY);
			count = add_unique(outTiles, count, outMax, tX + 1, tY);
			count = add_unique(outTiles, count, outMax, tX, tY - 1);
			count = add_unique(outTiles, count, outMax, tX, tY + 1);

			if ((std::abs(tX - targetX) <= 1) && (std::abs(tY - targetY) <= 1)) break;
		}

		// Area at destination (server uses m_value_2 for X, m_value_3 for Y)
		if (radiusX > 0 || radiusY > 0) {
			count = add_area(outTiles, count, outMax, targetX, targetY, radiusX, radiusY);
		}

		// Target tile itself (direct damage)
		count = add_unique(outTiles, count, outMax, targetX, targetY);
		break;
	}

	// =================================================================
	// Area spells with center damage: rectangular AoE + center hit
	// Server: center tile gets m_value_4/5/6, area gets m_sValue7/8/9
	// =================================================================
	case hb::shared::magic::DamageArea:
	case hb::shared::magic::SpDownArea:
	case hb::shared::magic::SpUpArea:
	case hb::shared::magic::Tremor:
	case hb::shared::magic::Ice:
	{
		count = add_unique(outTiles, count, outMax, targetX, targetY);
		if (radiusX > 0 || radiusY > 0) {
			count = add_area(outTiles, count, outMax, targetX, targetY, radiusX, radiusY);
		}
		break;
	}

	// =================================================================
	// Area spells WITHOUT center damage (no-spot)
	// =================================================================
	case hb::shared::magic::DamageAreaNoSpot:
	case hb::shared::magic::DamageAreaNoSpotSpDown:
	case hb::shared::magic::DamageAreaArmorBreak:
	{
		if (radiusX > 0 || radiusY > 0) {
			count = add_area(outTiles, count, outMax, targetX, targetY, radiusX, radiusY);
		} else {
			// Fallback: at least show the target tile
			count = add_unique(outTiles, count, outMax, targetX, targetY);
		}
		break;
	}

	// =================================================================
	// CREATE_DYNAMIC: Firewall, Firefield, Spikefield, Poison Cloud
	// Pattern depends on m_sValue11: 1=wall, 2=field
	// =================================================================
	case hb::shared::magic::CreateDynamic:
	{
		int dynRadius = params.m_dynamic_radius;
		if (dynRadius <= 0) dynRadius = 1;

		switch (params.m_dynamic_pattern)
		{
		case 1:
		{
			// Wall type: perpendicular to caster→target direction
			int dir = get_move_dir(casterX, casterY, targetX, targetY);
			int rx, ry;
			get_wall_offset(dir, rx, ry);

			// Center tile
			count = add_unique(outTiles, count, outMax, targetX, targetY);

			// Both sides of the wall
			for (int i = 1; i <= dynRadius; i++) {
				count = add_unique(outTiles, count, outMax, targetX + i * rx, targetY + i * ry);
				count = add_unique(outTiles, count, outMax, targetX - i * rx, targetY - i * ry);
			}
			break;
		}
		case 2:
		{
			// Field type: square area centered on target
			count = add_area(outTiles, count, outMax, targetX, targetY, dynRadius, dynRadius);
			break;
		}
		default:
			// Single tile (e.g., Ice Storm)
			count = add_unique(outTiles, count, outMax, targetX, targetY);
			break;
		}
		break;
	}

	// =================================================================
	// Confuse (Mass Illusion etc.) — uses AoE radius, affects area
	// =================================================================
	case hb::shared::magic::Confuse:
	{
		if (radiusX > 0 || radiusY > 0) {
			count = add_area(outTiles, count, outMax, targetX, targetY, radiusX, radiusY);
		} else {
			count = add_unique(outTiles, count, outMax, targetX, targetY);
		}
		break;
	}

	// =================================================================
	// Single target spells — just the target tile
	// =================================================================
	case hb::shared::magic::DamageSpot:
	case hb::shared::magic::HpUpSpot:
	case hb::shared::magic::SpDownSpot:
	case hb::shared::magic::SpUpSpot:
	case hb::shared::magic::HoldObject:
	case hb::shared::magic::Possession:
	case hb::shared::magic::Poison:
	case hb::shared::magic::Inhibition:
	case hb::shared::magic::Cancellation:
	case hb::shared::magic::Berserk:
	case hb::shared::magic::Protect:
	case hb::shared::magic::Polymorph:
	case hb::shared::magic::Resurrection:
	case hb::shared::magic::Scan:
	case hb::shared::magic::Invisibility:
	case hb::shared::magic::Haste:
		count = add_unique(outTiles, count, outMax, targetX, targetY);
		break;

	// Self/utility spells — show target tile as a simple indicator
	case hb::shared::magic::Teleport:
	case hb::shared::magic::Summon:
	case hb::shared::magic::Create:
		count = add_unique(outTiles, count, outMax, targetX, targetY);
		break;

	default:
		break;
	}

	return count;
}
