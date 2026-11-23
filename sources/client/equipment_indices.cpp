#include "equipment_indices.h"
#include "entity_render_state.h"
#include "sprite_id.h"
#include "config_manager.h"
using namespace hb::client::sprite_id;

EquipmentIndices EquipmentIndices::CalcPlayer(const CEntityRenderState& state, int bodyPose, bool drawWeapon, bool drawShield)
{
	EquipmentIndices eq = {};
	const auto& appr = state.m_appearance;
	bool female = state.is_female();

	// Body index (still from m_sprite — not per-item)
	eq.m_body_index = 500 + (state.m_owner_type - 1) * 8 * 15 + (bodyPose * 8);

	// Cosmetics — still from m_sprite with old base IDs
	int UNDIES = female ? UndiesW : UndiesM;
	int HAIR   = female ? HairW  : HairM;
	eq.m_undies_index = UNDIES + appr.underwear_type * 15 + bodyPose;
	eq.m_hair_index   = HAIR + appr.hair_style * 15 + bodyPose;

	// Equipment — from m_equip_sprites via equip_sprite::index()
	// Check item_id > 0 for "is equipped" (display_id=0 is valid after memset)
	if (!appr.hide_armor && appr.armor_item_id > 0)
		eq.m_body_armor_index = equip_sprite::index(female, appr.armor_display_id, bodyPose);
	else
		eq.m_body_armor_index = -1;

	eq.m_arm_armor_index = (appr.arm_item_id > 0)    ? equip_sprite::index(female, appr.arm_display_id, bodyPose)    : -1;
	eq.m_pants_index     = (appr.pants_item_id > 0)   ? equip_sprite::index(female, appr.pants_display_id, bodyPose)  : -1;
	eq.m_boots_index     = (appr.boots_item_id > 0)   ? equip_sprite::index(female, appr.boots_display_id, bodyPose)  : -1;
	eq.m_mantle_index    = (appr.mantle_item_id > 0)  ? equip_sprite::index(female, appr.mantle_display_id, bodyPose) : -1;
	eq.m_helm_index      = (appr.helm_item_id > 0)    ? equip_sprite::index(female, appr.helm_display_id, bodyPose)   : -1;

	// Weapon — same unified formula, direction now in frame (not index)
	if (drawWeapon && appr.weapon_item_id > 0)
	{
		eq.m_weapon_index = equip_sprite::index(female, appr.weapon_display_id, bodyPose);
		eq.m_weapon_item_id = appr.weapon_item_id;
	}
	else
	{
		eq.m_weapon_index = -1;
		eq.m_weapon_item_id = 0;
	}

	// Shield — same unified formula
	if (drawShield && appr.shield_item_id > 0)
		eq.m_shield_index = equip_sprite::index(female, appr.shield_display_id, bodyPose);
	else
		eq.m_shield_index = -1;

	// Female skirt check — from is_skirt flag computed at broadcast time
	eq.m_skirt_draw = (female && appr.is_skirt) ? 1 : 0;

	// Colors and glare initialized to 0 by = {} above
	return eq;
}

EquipmentIndices EquipmentIndices::CalcNpc(const CEntityRenderState& state, int npcPose)
{
	EquipmentIndices eq = {};

	eq.m_body_index      = Mob + (state.m_owner_type - 10) * 8 * 7 + (npcPose * 8);
	eq.m_undies_index    = -1;
	eq.m_hair_index      = -1;
	eq.m_body_armor_index = -1;
	eq.m_arm_armor_index  = -1;
	eq.m_pants_index     = -1;
	eq.m_boots_index     = -1;
	eq.m_weapon_index    = -1;
	eq.m_shield_index    = -1;
	eq.m_mantle_index    = -1;
	eq.m_helm_index      = -1;
	eq.m_skirt_draw      = 0;

	// Colors and glare initialized to 0 by = {} above
	return eq;
}

void EquipmentIndices::calc_colors(const CEntityRenderState& state)
{
	if (config_manager::get().get_detail_level() == 0)
	{
		m_weapon_color = 0;
		m_shield_color = 0;
		m_armor_color  = 0;
		m_mantle_color = 0;
		m_arm_color    = 0;
		m_pants_color  = 0;
		m_boots_color  = 0;
		m_helm_color   = 0;
	}
	else
	{
		m_weapon_color = state.m_appearance.weapon_color;
		m_shield_color = state.m_appearance.shield_color;
		m_armor_color  = state.m_appearance.armor_color;
		m_mantle_color = state.m_appearance.mantle_color;
		m_arm_color    = state.m_appearance.arm_color;
		m_pants_color  = state.m_appearance.pants_color;
		m_boots_color  = state.m_appearance.boots_color;
		m_helm_color   = state.m_appearance.helm_color;
	}

	m_weapon_glare = state.m_appearance.weapon_glare;
	m_shield_glare = state.m_appearance.shield_glare;
}
