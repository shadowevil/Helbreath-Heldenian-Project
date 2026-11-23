#pragma once

#include <cstdint>

class CEntityRenderState;

// Consolidated equipment sprite index calculation.
// Replaces the 42+ duplicated blocks of equipment index computation
// across all DrawObject_On* functions. Each function computes a bodyPose
// (and optionally separate weapon/shield poses) then calls CalcPlayer.
struct EquipmentIndices
{
	// Sprite indices (-1 = not drawn)
	int m_body_index;
	int m_undies_index;
	int m_hair_index;
	int m_body_armor_index;
	int m_arm_armor_index;
	int m_pants_index;
	int m_boots_index;
	int m_weapon_index;
	int m_shield_index;
	int m_mantle_index;
	int m_helm_index;

	// Equipment colors (0 = no tint, >0 = palette index)
	int m_weapon_color;
	int m_shield_color;
	int m_armor_color;
	int m_mantle_color;
	int m_arm_color;
	int m_pants_color;
	int m_boots_color;
	int m_helm_color;

	// Weapon item ID (for item config lookups — dk_glare, two-hand detection, etc.)
	int16_t m_weapon_item_id;

	// Glare effects
	int m_weapon_glare;
	int m_shield_glare;

	// Female skirt flag (pants type 1 on female character)
	int m_skirt_draw;

	// Calculate all equipment indices for a player character.
	// bodyPose: animation pose (0-11) for body, cosmetics, and equipment layers.
	//   Body: 500 + (ownerType-1)*8*15 + (bodyPose*8) — from m_sprite
	//   Cosmetics (undies/hair): old base + type*15 + bodyPose — from m_sprite
	//   Equipment: equip_sprite::index(female, display_id, bodyPose) — from m_equip_sprites
	// drawWeapon/drawShield: false = set index to -1 (e.g. magic casting)
	static EquipmentIndices CalcPlayer(const CEntityRenderState& state, int bodyPose, bool drawWeapon = true, bool drawShield = true);

	// Calculate body index for NPC/mob. All equipment indices set to -1.
	// npcPose: animation pose for the mob sprite
	//   Body: DEF_SPRID_MOB + (ownerType-10)*8*7 + (npcPose*8)
	// If HasNpcSpecialState(), caller should override body index and frame after calling this.
	static EquipmentIndices CalcNpc(const CEntityRenderState& state, int npcPose);

	// Fill color fields from entity appearance, respecting detail level.
	// When detail level is 0, all colors are set to 0 (no tinting).
	// Also sets glare values (note: glare fields are swapped in original code).
	void calc_colors(const CEntityRenderState& state);
};
