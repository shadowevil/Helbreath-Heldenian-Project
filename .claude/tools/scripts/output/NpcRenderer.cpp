#include "NpcRenderer.h"
#include "Game.h"
#include "Screen_OnGame.h"
#include "FloatingTextManager.h"
#include "EquipmentIndices.h"
#include "RenderHelpers.h"
#include "CommonTypes.h"
#include <algorithm>
using namespace hb::client::sprite_id;
using namespace hb::shared::direction;

hb::shared::sprite::BoundRect CNpcRenderer::draw_stop(int indexX, int indexY, int sX, int sY, bool trans, uint32_t time)
{
	hb::shared::sprite::BoundRect invalidRect = {0, -1, 0, 0};
	auto& state = m_game.m_entity_state;
	bool inv = false;
	bool admin_invis = false;

	// Apply motion offset if entity is still interpolating
	sX += static_cast<int>(m_game.m_map_data->m_data[state.m_data_x][state.m_data_y].m_motion.m_current_offset_x);
	sY += static_cast<int>(m_game.m_map_data->m_data[state.m_data_x][state.m_data_y].m_motion.m_current_offset_y);

	// Invisibility check
	if (RenderHelpers::check_invisibility(m_game, state, inv, admin_invis))
		return invalidRect;

	// Single-direction monster override
	RenderHelpers::apply_direction_override(state);

	// NPC body index calculation
	EquipmentIndices eq = EquipmentIndices::CalcNpc(state, 0);
	eq.calc_colors(state);

	// Special frame from NPC appearance
	if (state.m_appearance.HasNpcSpecialState())
	{
		eq.m_body_index = Mob + (state.m_owner_type - 10) * 8 * 7 + (4 * 8);
		state.m_frame = state.m_appearance.special_frame - 1;
	}

	// Crusade FOE indicator
	if (m_screen->m_is_crusade_mode)
		m_screen->draw_object_foe(sX, sY, state.m_frame);

	// NPC ground lights
	RenderHelpers::draw_npc_light(m_game, state.m_owner_type, sX, sY);

	// Effect auras
	RenderHelpers::draw_effect_auras(m_game, state, sX, sY);

	if (!trans)
	{
		m_game.check_active_aura(sX, sY, time, state.m_owner_type);

		// draw NPC body (shadow + body sprite)
		RenderHelpers::draw_npc_layers(m_game, eq, state, sX, sY, inv);

		// Crop effects
		if (state.m_owner_type == hb::shared::owner::Crops)
		{
			switch (state.m_frame) {
			case 0: m_game.m_effect_sprites[84]->draw(sX + 52, sY + 54, (time % 3000) / 120, hb::shared::sprite::DrawParams::alpha_blend(0.5f)); break;
			case 1: m_game.m_effect_sprites[83]->draw(sX + 53, sY + 59, (time % 3000) / 120, hb::shared::sprite::DrawParams::alpha_blend(0.5f)); break;
			case 2: m_game.m_effect_sprites[82]->draw(sX + 53, sY + 65, (time % 3000) / 120, hb::shared::sprite::DrawParams::alpha_blend(0.5f)); break;
			}
		}

		// Berserk glow
		RenderHelpers::draw_berserk_glow(m_game, eq, state, sX, sY);

		// Angel + aura
		m_screen->draw_angel(40 + (state.m_dir - 1), sX + 20, sY - 20, state.m_frame % 4, time);
		m_game.check_active_aura2(sX, sY, time, state.m_owner_type);
	}
	else if (state.m_name[0] != '\0')
	{
		RenderHelpers::draw_name(*m_screen, state, sX, sY);
	}

	// Chat message (always)
	RenderHelpers::update_chat(m_game, state, sX, sY, indexX, indexY);

	// Abaddon effects (always)
	RenderHelpers::draw_abaddon_effects(m_game, state, sX, sY);

	// GM mode (always)
	RenderHelpers::draw_gm_effect(m_game, state, sX, sY);

	return m_game.m_sprite[eq.m_body_index + (state.m_dir - 1)]->GetBoundRect();
}

// Helper: returns true if this NPC type should keep full animation frames (no halving).
// Types NOT in this list get frame /= 2 during move animation.
static bool ShouldKeepFullFrames(short owner_type)
{
	switch (owner_type) {
	case hb::shared::owner::Troll:
	case hb::shared::owner::Ogre:
	case hb::shared::owner::Liche:
	case hb::shared::owner::Demon:
	case hb::shared::owner::Unicorn:
	case hb::shared::owner::WereWolf:
	case hb::shared::owner::LightWarBeetle:
	case hb::shared::owner::GodsHandKnight:
	case hb::shared::owner::GodsHandKnightCK:
	case hb::shared::owner::TempleKnight:
	case hb::shared::owner::BattleGolem:
	case hb::shared::owner::Stalker:
	case hb::shared::owner::HellClaw:
	case hb::shared::owner::TigerWorm:
	case hb::shared::owner::Gargoyle:
	case hb::shared::owner::Beholder:
	case hb::shared::owner::DarkElf:
	case hb::shared::owner::Bunny:
	case hb::shared::owner::Cat:
	case hb::shared::owner::GiantFrog:
	case hb::shared::owner::MountainGiant:
	case hb::shared::owner::Ettin:
	case hb::shared::owner::CannibalPlant:
	case hb::shared::owner::Rudolph:
	case hb::shared::owner::DireBoar:
	case hb::shared::owner::Frost:
	case hb::shared::owner::IceGolem:
	case hb::shared::owner::Wyvern:
	case hb::shared::owner::Dragon:
	case hb::shared::owner::Centaur:
	case hb::shared::owner::ClawTurtle:
	case hb::shared::owner::FireWyvern:
	case hb::shared::owner::GiantCrayfish:
	case hb::shared::owner::GiLizard:
	case hb::shared::owner::GiTree:
	case hb::shared::owner::MasterOrc:
	case hb::shared::owner::Minaus:
	case hb::shared::owner::Nizie:
	case hb::shared::owner::Tentocle:
	case hb::shared::owner::Abaddon:
	case hb::shared::owner::Sorceress:
	case hb::shared::owner::ATK:
	case hb::shared::owner::MasterElf:
	case hb::shared::owner::DSK:
	case hb::shared::owner::HBT:
	case hb::shared::owner::CT:
	case hb::shared::owner::Barbarian:
	case hb::shared::owner::AGC:
	case hb::shared::owner::Gail:
		return true;
	default:
		return false;
	}
}

hb::shared::sprite::BoundRect CNpcRenderer::draw_move(int indexX, int indexY, int sX, int sY, bool trans, uint32_t time)
{
	hb::shared::sprite::BoundRect invalidRect = {0, -1, 0, 0};
	auto& state = m_game.m_entity_state;
	bool inv = false;
	bool admin_invis = false;

	// Invisibility check
	if (RenderHelpers::check_invisibility(m_game, state, inv, admin_invis))
		return invalidRect;

	// NPC body index: HBT uses pose 0, all others use pose 1
	int npcPose = (state.m_owner_type == hb::shared::owner::HBT) ? 0 : 1;
	EquipmentIndices eq = EquipmentIndices::CalcNpc(state, npcPose);
	eq.calc_colors(state);

	// Motion offset
	int dx = static_cast<int>(m_game.m_map_data->m_data[state.m_data_x][state.m_data_y].m_motion.m_current_offset_x);
	int dy = static_cast<int>(m_game.m_map_data->m_data[state.m_data_x][state.m_data_y].m_motion.m_current_offset_y);
	int fix_x = sX + dx;
	int fix_y = sY + dy;

	// Frame halving for non-exempt NPC types
	if (!ShouldKeepFullFrames(state.m_owner_type))
		state.m_frame = state.m_frame / 2;

	// Crusade FOE indicator
	if (m_screen->m_is_crusade_mode)
		m_screen->draw_object_foe(fix_x, fix_y, state.m_frame);

	// Effect auras
	RenderHelpers::draw_effect_auras(m_game, state, fix_x, fix_y);

	// IceGolem particle effects
	if (state.m_owner_type == hb::shared::owner::IceGolem)
	{
		switch (rand() % 3) {
		case 0: m_game.m_effect_sprites[76]->draw(fix_x, fix_y, state.m_frame, hb::shared::sprite::DrawParams::alpha_blend(0.7f)); break;
		case 1: m_game.m_effect_sprites[77]->draw(fix_x, fix_y, state.m_frame, hb::shared::sprite::DrawParams::alpha_blend(0.7f)); break;
		case 2: m_game.m_effect_sprites[78]->draw(fix_x, fix_y, state.m_frame, hb::shared::sprite::DrawParams::alpha_blend(0.7f)); break;
		}
	}

	if (!trans)
	{
		m_game.check_active_aura(fix_x, fix_y, time, state.m_owner_type);

		// draw NPC body (shadow + body sprite)
		RenderHelpers::draw_npc_layers(m_game, eq, state, fix_x, fix_y, inv);

		// Berserk glow
		RenderHelpers::draw_berserk_glow(m_game, eq, state, fix_x, fix_y);

		// Angel + aura
		m_screen->draw_angel(40 + (state.m_dir - 1), fix_x + 20, fix_y - 20, state.m_frame % 4, time);
		m_game.check_active_aura2(fix_x, fix_y, time, state.m_owner_type);
	}
	else if (state.m_name[0] != '\0')
	{
		RenderHelpers::draw_name(*m_screen, state, fix_x, fix_y);
	}

	// Chat message (always)
	RenderHelpers::update_chat(m_game, state, fix_x, fix_y, indexX, indexY);

	// Store motion offsets
	state.m_move_offset_x = dx;
	state.m_move_offset_y = dy;

	// Abaddon effects — surrounding effects use sX/sY (tile-anchored),
	// direction-based effects use fix_x/fix_y (entity-following)
	if (state.m_owner_type == hb::shared::owner::Abaddon)
	{
		int randFrame = state.m_effect_frame % 12;
		m_game.m_effect_sprites[154]->draw(sX - 50, sY - 50, randFrame, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
		m_game.m_effect_sprites[155]->draw(sX - 20, sY - 80, randFrame, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
		m_game.m_effect_sprites[156]->draw(sX + 70, sY - 50, randFrame, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
		m_game.m_effect_sprites[157]->draw(sX - 30, sY, randFrame, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
		m_game.m_effect_sprites[158]->draw(sX - 60, sY + 90, randFrame, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
		m_game.m_effect_sprites[159]->draw(sX + 65, sY + 85, randFrame, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
		int ef = state.m_effect_frame;
		switch (state.m_dir) {
		case 1:
			m_game.m_effect_sprites[153]->draw(fix_x, fix_y + 108, ef % 28, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
			m_game.m_effect_sprites[164]->draw(fix_x - 50, fix_y + 10, ef % 15, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
			break;
		case 2:
			m_game.m_effect_sprites[153]->draw(fix_x, fix_y + 95, ef % 28, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
			m_game.m_effect_sprites[164]->draw(fix_x - 70, fix_y + 10, ef % 15, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
			break;
		case 3:
			m_game.m_effect_sprites[153]->draw(fix_x, fix_y + 105, ef % 28, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
			m_game.m_effect_sprites[164]->draw(fix_x - 90, fix_y + 10, ef % 15, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
			break;
		case 4:
			m_game.m_effect_sprites[153]->draw(fix_x - 35, fix_y + 100, ef % 28, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
			m_game.m_effect_sprites[164]->draw(fix_x - 80, fix_y + 10, ef % 15, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
			break;
		case 5:
			m_game.m_effect_sprites[153]->draw(fix_x, fix_y + 95, ef % 28, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
			m_game.m_effect_sprites[164]->draw(fix_x - 65, fix_y - 5, ef % 15, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
			break;
		case 6:
			m_game.m_effect_sprites[153]->draw(fix_x + 45, fix_y + 95, ef % 28, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
			m_game.m_effect_sprites[164]->draw(fix_x - 31, fix_y + 10, ef % 15, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
			break;
		case 7:
			m_game.m_effect_sprites[153]->draw(fix_x + 40, fix_y + 110, ef % 28, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
			m_game.m_effect_sprites[164]->draw(fix_x - 30, fix_y + 10, ef % 15, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
			break;
		case 8:
			m_game.m_effect_sprites[153]->draw(fix_x + 20, fix_y + 110, ef % 28, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
			m_game.m_effect_sprites[164]->draw(fix_x - 20, fix_y + 16, ef % 15, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
			break;
		}
	}

	// GM mode (always)
	RenderHelpers::draw_gm_effect(m_game, state, fix_x, fix_y);

	return m_game.m_sprite[eq.m_body_index + (state.m_dir - 1)]->GetBoundRect();
}

hb::shared::sprite::BoundRect CNpcRenderer::draw_run(int indexX, int indexY, int sX, int sY, bool trans, uint32_t time)
{
	// NPCs don't normally run, but the function still handles the NPC default case
	hb::shared::sprite::BoundRect invalidRect = {0, -1, 0, 0};
	auto& state = m_game.m_entity_state;
	bool inv = false;
	bool admin_invis = false;

	// Invisibility check
	if (RenderHelpers::check_invisibility(m_game, state, inv, admin_invis))
		return invalidRect;

	// NPC default in OnRun: all equipment -1, no specific body index pose override
	// Original code has no body_index assignment for NPC default in OnRun
	EquipmentIndices eq = EquipmentIndices::CalcNpc(state, 1);
	eq.calc_colors(state);

	// Motion offset
	int dx = static_cast<int>(m_game.m_map_data->m_data[state.m_data_x][state.m_data_y].m_motion.m_current_offset_x);
	int dy = static_cast<int>(m_game.m_map_data->m_data[state.m_data_x][state.m_data_y].m_motion.m_current_offset_y);
	int fix_x = sX + dx;
	int fix_y = sY + dy;

	// Crusade FOE indicator
	if (m_screen->m_is_crusade_mode)
		m_screen->draw_object_foe(fix_x, fix_y, state.m_frame);

	// Effect auras
	RenderHelpers::draw_effect_auras(m_game, state, fix_x, fix_y);

	if (!trans)
	{
		m_game.check_active_aura(fix_x, fix_y, time, state.m_owner_type);

		// draw NPC body
		RenderHelpers::draw_npc_layers(m_game, eq, state, fix_x, fix_y, inv);

		// Berserk glow
		RenderHelpers::draw_berserk_glow(m_game, eq, state, fix_x, fix_y);

		// Angel + aura
		m_screen->draw_angel(40 + (state.m_dir - 1), fix_x + 20, fix_y - 20, state.m_frame % 4, time);
		m_game.check_active_aura2(fix_x, fix_y, time, state.m_owner_type);
	}
	else if (state.m_name[0] != '\0')
	{
		RenderHelpers::draw_name(*m_screen, state, fix_x, fix_y);
	}

	// Chat message (always)
	RenderHelpers::update_chat(m_game, state, fix_x, fix_y, indexX, indexY);

	// Store motion offsets
	state.m_move_offset_x = dx;
	state.m_move_offset_y = dy;

	// GM mode (always)
	RenderHelpers::draw_gm_effect(m_game, state, fix_x, fix_y);

	return m_game.m_sprite[eq.m_body_index + (state.m_dir - 1)]->GetBoundRect();
}

hb::shared::sprite::BoundRect CNpcRenderer::draw_attack(int indexX, int indexY, int sX, int sY, bool trans, uint32_t time)
{
	hb::shared::sprite::BoundRect invalidRect = {0, -1, 0, 0};
	auto& state = m_game.m_entity_state;
	bool inv = false;
	bool admin_invis = false;

	// Invisibility check
	if (RenderHelpers::check_invisibility(m_game, state, inv, admin_invis))
		return invalidRect;

	// NPC attack pose calculation with per-mob-type overrides
	EquipmentIndices eq;
	if (state.m_appearance.HasNpcSpecialState())
	{
		eq = EquipmentIndices::CalcNpc(state, 4);
		state.m_frame = state.m_appearance.special_frame - 1;
	}
	else if (state.m_owner_type == hb::shared::owner::Wyvern || state.m_owner_type == hb::shared::owner::FireWyvern)
		eq = EquipmentIndices::CalcNpc(state, 0);
	else if (state.m_owner_type == hb::shared::owner::HBT || state.m_owner_type == hb::shared::owner::CT || state.m_owner_type == hb::shared::owner::AGC)
		eq = EquipmentIndices::CalcNpc(state, 1);
	else
		eq = EquipmentIndices::CalcNpc(state, 2);
	eq.calc_colors(state);

	// Crusade FOE indicator
	if (m_screen->m_is_crusade_mode)
		m_screen->draw_object_foe(sX, sY, state.m_frame);

	// Effect auras
	RenderHelpers::draw_effect_auras(m_game, state, sX, sY);

	if (!trans)
	{
		m_game.check_active_aura(sX, sY, time, state.m_owner_type);

		// draw NPC body
		RenderHelpers::draw_npc_layers(m_game, eq, state, sX, sY, inv);

		// Berserk glow
		RenderHelpers::draw_berserk_glow(m_game, eq, state, sX, sY);

		// Angel + aura — OnAttack uses (dir-1) angel index, frame % 8
		m_screen->draw_angel(state.m_dir - 1, sX + 20, sY - 20, state.m_frame % 8, time);
		m_game.check_active_aura2(sX, sY, time, state.m_owner_type);
	}
	else if (state.m_name[0] != '\0')
	{
		RenderHelpers::draw_name(*m_screen, state, sX, sY);
	}

	// Chat message (always)
	RenderHelpers::update_chat(m_game, state, sX, sY, indexX, indexY);

	// Abaddon effects (always)
	RenderHelpers::draw_abaddon_effects(m_game, state, sX, sY);

	// GM mode (always)
	RenderHelpers::draw_gm_effect(m_game, state, sX, sY);

	return m_game.m_sprite[eq.m_body_index + (state.m_dir - 1)]->GetBoundRect();
}

hb::shared::sprite::BoundRect CNpcRenderer::draw_attack_move(int indexX, int indexY, int sX, int sY, bool trans, uint32_t time)
{
	hb::shared::sprite::BoundRect invalidRect = {0, -1, 0, 0};
	auto& state = m_game.m_entity_state;
	bool inv = false;
	bool admin_invis = false;

	// Invisibility check
	if (RenderHelpers::check_invisibility(m_game, state, inv, admin_invis))
		return invalidRect;

	// Frame clamping — same as original
	switch (state.m_frame) {
	case 4: case 5: case 6: case 7: case 8: case 9: state.m_frame = 4; break;
	case 10: state.m_frame = 5; break;
	case 11: state.m_frame = 6; break;
	case 12: state.m_frame = 7; break;
	}

	// NPC attack-move uses pose 2
	EquipmentIndices eq = EquipmentIndices::CalcNpc(state, 2);
	eq.calc_colors(state);

	// Frame-based motion offset — same as player
	int dx = 0, dy = 0;
	bool dash_draw = false;
	int dsx = 0, dsy = 0;
	int frame_move_dots = 0;

	if ((state.m_frame >= 1) && (state.m_frame <= 3))
	{
		switch (state.m_frame) {
		case 1: frame_move_dots = 26; break;
		case 2: frame_move_dots = 16; break;
		case 3: frame_move_dots = 0;  break;
		}
		switch (state.m_dir) {
		case 1: dy = frame_move_dots; break;
		case 2: dy = frame_move_dots; dx = -frame_move_dots; break;
		case 3: dx = -frame_move_dots; break;
		case 4: dx = -frame_move_dots; dy = -frame_move_dots; break;
		case 5: dy = -frame_move_dots; break;
		case 6: dy = -frame_move_dots; dx = frame_move_dots; break;
		case 7: dx = frame_move_dots; break;
		case 8: dx = frame_move_dots; dy = frame_move_dots; break;
		}
		switch (state.m_frame) {
		case 1: dy++;    break;
		case 2: dy += 2; break;
		case 3: dy++;    break;
		}
		switch (state.m_frame) {
		case 2: dash_draw = true; frame_move_dots = 26; break;
		case 3: dash_draw = true; frame_move_dots = 16; break;
		}
		switch (state.m_dir) {
		case 1: dsy = frame_move_dots; break;
		case 2: dsy = frame_move_dots; dsx = -frame_move_dots; break;
		case 3: dsx = -frame_move_dots; break;
		case 4: dsx = -frame_move_dots; dsy = -frame_move_dots; break;
		case 5: dsy = -frame_move_dots; break;
		case 6: dsy = -frame_move_dots; dsx = frame_move_dots; break;
		case 7: dsx = frame_move_dots; break;
		case 8: dsx = frame_move_dots; dsy = frame_move_dots; break;
		}
	}
	else if (state.m_frame == 0)
	{
		switch (state.m_dir) {
		case 1: dy = 32; break;
		case 2: dy = 32; dx = -32; break;
		case 3: dx = -32; break;
		case 4: dx = -32; dy = -32; break;
		case 5: dy = -32; break;
		case 6: dy = -32; dx = 32; break;
		case 7: dx = 32; break;
		case 8: dx = 32; dy = 32; break;
		}
	}

	// Crusade FOE indicator
	if (m_screen->m_is_crusade_mode)
		m_screen->draw_object_foe(sX + dx, sY + dy, state.m_frame);

	// Effect auras
	RenderHelpers::draw_effect_auras(m_game, state, sX + dx, sY + dy);

	if (!trans)
	{
		m_game.check_active_aura(sX + dx, sY + dy, time, state.m_owner_type);

		// draw NPC body
		RenderHelpers::draw_npc_layers(m_game, eq, state, sX + dx, sY + dy, inv);

		// Berserk glow
		RenderHelpers::draw_berserk_glow(m_game, eq, state, sX + dx, sY + dy);

		// Angel + aura
		m_screen->draw_angel(8 + (state.m_dir - 1), sX + dx + 20, sY + dy - 20, state.m_frame % 8, time);
		m_game.check_active_aura2(sX + dx, sY + dy, time, state.m_owner_type);

		// Dash ghost effect
		if (dash_draw)
		{
			m_game.m_sprite[eq.m_body_index + (state.m_dir - 1)]->draw(sX + dsx, sY + dsy, state.m_frame,
				hb::shared::sprite::DrawParams::tinted_alpha(126, 192, 242, 0.7f));
		}
	}
	else if (state.m_name[0] != '\0')
	{
		RenderHelpers::draw_name(*m_screen, state, sX + dx, sY + dy);
	}

	// Chat message (always)
	RenderHelpers::update_chat(m_game, state, sX + dx, sY + dy, indexX, indexY);

	// Store motion offsets
	state.m_move_offset_x = dx;
	state.m_move_offset_y = dy;

	// GM mode (always)
	RenderHelpers::draw_gm_effect(m_game, state, sX + dx, sY + dy);

	return m_game.m_sprite[eq.m_body_index + (state.m_dir - 1)]->GetBoundRect();
}

hb::shared::sprite::BoundRect CNpcRenderer::draw_magic(int indexX, int indexY, int sX, int sY, bool trans, uint32_t time)
{
	// NPCs don't have a magic animation in the original code — the original OnMagic
	// only handles player types 1-6. For NPCs, we just draw them stopped.
	return draw_stop(indexX, indexY, sX, sY, trans, time);
}

hb::shared::sprite::BoundRect CNpcRenderer::draw_get_item(int indexX, int indexY, int sX, int sY, bool trans, uint32_t time)
{
	// NPCs don't have a get-item animation in the original code — the original OnGetItem
	// has no body_index set for the NPC default case. Fall back to stop.
	return draw_stop(indexX, indexY, sX, sY, trans, time);
}

hb::shared::sprite::BoundRect CNpcRenderer::draw_damage(int indexX, int indexY, int sX, int sY, bool trans, uint32_t time)
{
	hb::shared::sprite::BoundRect invalidRect = {0, -1, 0, 0};
	auto& state = m_game.m_entity_state;
	bool inv = false;
	bool admin_invis = false;

	// Invisibility check
	if (RenderHelpers::check_invisibility(m_game, state, inv, admin_invis))
		return invalidRect;

	// Two-state NPC damage: complex per-mob overrides
	char frame = state.m_frame;
	int npcPose;

	if (frame < 4)
	{
		// Idle state — per-mob overrides
		if (state.m_appearance.HasNpcSpecialState())
			npcPose = 4; // special frame from NPC appearance
		else if (state.m_owner_type == hb::shared::owner::Wyvern) npcPose = 0;
		else if (state.m_owner_type == hb::shared::owner::McGaffin) npcPose = 0;
		else if (state.m_owner_type == hb::shared::owner::Perry) npcPose = 0;
		else if (state.m_owner_type == hb::shared::owner::Devlin) npcPose = 0;
		else if (state.m_owner_type == hb::shared::owner::FireWyvern) npcPose = 0;
		else if (state.m_owner_type == hb::shared::owner::Abaddon) npcPose = 2;
		else if (state.m_owner_type == hb::shared::owner::HBT) npcPose = 2;
		else if (state.m_owner_type == hb::shared::owner::CT) npcPose = 2;
		else if (state.m_owner_type == hb::shared::owner::AGC) npcPose = 2;
		else if (state.m_owner_type == hb::shared::owner::Gate) npcPose = 0;
		else npcPose = 0;
	}
	else
	{
		frame -= 4;
		// Damage recoil state — per-mob overrides
		if (state.m_appearance.HasNpcSpecialState())
			npcPose = 4;
		else if (state.m_owner_type == hb::shared::owner::Wyvern) npcPose = 0;
		else if (state.m_owner_type == hb::shared::owner::McGaffin) npcPose = 0;
		else if (state.m_owner_type == hb::shared::owner::Perry) npcPose = 0;
		else if (state.m_owner_type == hb::shared::owner::Devlin) npcPose = 0;
		else if (state.m_owner_type == hb::shared::owner::FireWyvern) npcPose = 0;
		else if (state.m_owner_type == hb::shared::owner::Abaddon) npcPose = 2;
		else if (state.m_owner_type == hb::shared::owner::HBT) npcPose = 2;
		else if (state.m_owner_type == hb::shared::owner::CT) npcPose = 2;
		else if (state.m_owner_type == hb::shared::owner::AGC) npcPose = 2;
		else if (state.m_owner_type == hb::shared::owner::Gate) npcPose = 1;
		else npcPose = 3;
	}

	EquipmentIndices eq = EquipmentIndices::CalcNpc(state, npcPose);
	eq.calc_colors(state);

	// Apply NPC special frame override
	if (state.m_appearance.HasNpcSpecialState())
		frame = state.m_appearance.special_frame - 1;

	state.m_frame = frame;

	// Crusade FOE indicator
	if (m_screen->m_is_crusade_mode)
		m_screen->draw_object_foe(sX, sY, frame);

	// Effect auras
	RenderHelpers::draw_effect_auras(m_game, state, sX, sY);

	if (!trans)
	{
		m_game.check_active_aura(sX, sY, time, state.m_owner_type);

		// draw NPC body
		RenderHelpers::draw_npc_layers(m_game, eq, state, sX, sY, inv);

		// Berserk glow
		RenderHelpers::draw_berserk_glow(m_game, eq, state, sX, sY);

		// Angel + aura — OnDamage uses 16+dir-1, frame%4
		m_screen->draw_angel(16 + (state.m_dir - 1), sX + 20, sY - 20, frame % 4, time);
		m_game.check_active_aura2(sX, sY, time, state.m_owner_type);
	}
	else if (state.m_name[0] != '\0')
	{
		RenderHelpers::draw_name(*m_screen, state, sX, sY);
	}

	// Chat message (always)
	RenderHelpers::update_chat(m_game, state, sX, sY, indexX, indexY);

	// Abaddon effects (always)
	RenderHelpers::draw_abaddon_effects(m_game, state, sX, sY);

	// GM mode (always)
	RenderHelpers::draw_gm_effect(m_game, state, sX, sY);

	return m_game.m_sprite[eq.m_body_index + (state.m_dir - 1)]->GetBoundRect();
}

hb::shared::sprite::BoundRect CNpcRenderer::draw_damage_move(int indexX, int indexY, int sX, int sY, bool trans, uint32_t time)
{
	hb::shared::sprite::BoundRect invalidRect = {0, -1, 0, 0};
	auto& state = m_game.m_entity_state;
	bool inv = false;
	bool admin_invis = false;

	// Early return for McGaffin/Perry/Devlin/Abaddon
	if (state.m_owner_type == hb::shared::owner::McGaffin ||
		state.m_owner_type == hb::shared::owner::Perry ||
		state.m_owner_type == hb::shared::owner::Devlin ||
		state.m_owner_type == hb::shared::owner::Abaddon)
		return invalidRect;

	// Invisibility check
	if (RenderHelpers::check_invisibility(m_game, state, inv, admin_invis))
		return invalidRect;

	// Direction inversion (knockback is opposite direction)
	switch (state.m_dir) {
	case direction::north:     state.m_dir = direction::south;     break;
	case direction::northeast: state.m_dir = direction::southwest; break;
	case direction::east:      state.m_dir = direction::west;      break;
	case direction::southeast: state.m_dir = direction::northwest; break;
	case direction::south:     state.m_dir = direction::north;     break;
	case direction::southwest: state.m_dir = direction::northeast; break;
	case direction::west:      state.m_dir = direction::east;      break;
	case direction::northwest: state.m_dir = direction::southeast; break;
	}

	// Per-mob pose overrides
	int npcPose;
	if (state.m_owner_type == hb::shared::owner::Wyvern) npcPose = 0;
	else if (state.m_owner_type == hb::shared::owner::FireWyvern) npcPose = 0;
	else if (state.m_owner_type == hb::shared::owner::HBT) npcPose = 2;
	else if (state.m_owner_type == hb::shared::owner::CT) npcPose = 2;
	else if (state.m_owner_type == hb::shared::owner::AGC) npcPose = 2;
	else npcPose = 3;

	EquipmentIndices eq = EquipmentIndices::CalcNpc(state, npcPose);
	eq.calc_colors(state);

	// Motion offset
	int dx = static_cast<int>(m_game.m_map_data->m_data[state.m_data_x][state.m_data_y].m_motion.m_current_offset_x);
	int dy = static_cast<int>(m_game.m_map_data->m_data[state.m_data_x][state.m_data_y].m_motion.m_current_offset_y);
	int fix_x = sX + dx;
	int fix_y = sY + dy;

	int frame = state.m_frame;

	// Crusade FOE indicator
	if (m_screen->m_is_crusade_mode)
		m_screen->draw_object_foe(fix_x, fix_y, frame);

	// Effect auras
	RenderHelpers::draw_effect_auras(m_game, state, fix_x, fix_y);

	if (!trans)
	{
		m_game.check_active_aura(fix_x, fix_y, time, state.m_owner_type);

		// draw NPC body
		RenderHelpers::draw_npc_layers(m_game, eq, state, fix_x, fix_y, inv);

		// Berserk glow
		RenderHelpers::draw_berserk_glow(m_game, eq, state, fix_x, fix_y);

		// Angel + aura — OnDamageMove uses 16+dir-1, frame%4
		m_screen->draw_angel(16 + (state.m_dir - 1), fix_x + 20, fix_y - 20, frame % 4, time);
		m_game.check_active_aura2(fix_x, fix_y, time, state.m_owner_type);
	}
	else if (state.m_name[0] != '\0')
	{
		RenderHelpers::draw_name(*m_screen, state, fix_x, fix_y);
	}

	// Chat message (always)
	RenderHelpers::update_chat(m_game, state, fix_x, fix_y, indexX, indexY);

	// Store motion offsets
	state.m_move_offset_x = dx;
	state.m_move_offset_y = dy;

	// GM mode (always)
	RenderHelpers::draw_gm_effect(m_game, state, fix_x, fix_y);

	return m_game.m_sprite[eq.m_body_index + (state.m_dir - 1)]->GetBoundRect();
}

hb::shared::sprite::BoundRect CNpcRenderer::draw_dying(int indexX, int indexY, int sX, int sY, bool trans, uint32_t time)
{
	hb::shared::sprite::BoundRect invalidRect = {0, -1, 0, 0};
	auto& state = m_game.m_entity_state;

	// No invisibility check for dying

	int originalFrame = state.m_frame;
	char frame = state.m_frame;

	// NPC dying: two-state with per-mob overrides
	int npcPose;

	if (frame < 4)
	{
		if (state.m_appearance.HasNpcSpecialState())
			npcPose = 4;
		else if (state.m_owner_type == hb::shared::owner::Wyvern) npcPose = 2;
		else if (state.m_owner_type == hb::shared::owner::FireWyvern) npcPose = 2;
		else if (state.m_owner_type == hb::shared::owner::Abaddon) npcPose = 3;
		else if (state.m_owner_type == hb::shared::owner::HBT) npcPose = 3;
		else if (state.m_owner_type == hb::shared::owner::CT) npcPose = 3;
		else if (state.m_owner_type == hb::shared::owner::AGC) npcPose = 3;
		else if (state.m_owner_type == hb::shared::owner::Gate) npcPose = 2;
		else npcPose = 0;

		// Guard tower types: no special state → frame=0
		switch (state.m_owner_type) {
		case hb::shared::owner::ArrowGuardTower:
		case hb::shared::owner::CannonGuardTower:
		case hb::shared::owner::ManaCollector:
		case hb::shared::owner::Detector:
		case hb::shared::owner::EnergyShield:
		case hb::shared::owner::GrandMagicGenerator:
		case hb::shared::owner::ManaStone:
			if (!state.m_appearance.HasNpcSpecialState()) frame = 0;
			break;
		case hb::shared::owner::Catapult: frame = 0; break;
		}
	}
	else
	{
		switch (state.m_owner_type) {
		case hb::shared::owner::Catapult: frame = 0; break;
		default: frame -= 4; break;
		}

		if (state.m_appearance.HasNpcSpecialState())
			npcPose = 4;
		else if (state.m_owner_type == hb::shared::owner::Wyvern) npcPose = 2;
		else if (state.m_owner_type == hb::shared::owner::FireWyvern) npcPose = 2;
		else if (state.m_owner_type == hb::shared::owner::Abaddon) npcPose = 3;
		else if (state.m_owner_type == hb::shared::owner::HBT) npcPose = 3;
		else if (state.m_owner_type == hb::shared::owner::CT) npcPose = 3;
		else if (state.m_owner_type == hb::shared::owner::AGC) npcPose = 3;
		else if (state.m_owner_type == hb::shared::owner::Gate) npcPose = 2;
		else npcPose = 4;
	}

	EquipmentIndices eq = EquipmentIndices::CalcNpc(state, npcPose);
	eq.calc_colors(state);

	// Apply NPC special frame override
	if (state.m_appearance.HasNpcSpecialState())
		frame = state.m_appearance.special_frame - 1;

	state.m_frame = frame;

	// Crusade FOE indicator
	if (m_screen->m_is_crusade_mode)
		m_screen->draw_object_foe(sX, sY, frame);

	// Effect auras
	RenderHelpers::draw_effect_auras(m_game, state, sX, sY);

	if (!trans)
	{
		// Shadow — includes Wyvern/FireWyvern in skip list for dying
		RenderHelpers::draw_shadow(m_game, eq.m_body_index + (state.m_dir - 1), sX, sY, frame, false, state.m_owner_type);

		// Abaddon death effects
		if (state.m_owner_type == hb::shared::owner::Abaddon)
		{
			m_game.m_effect_sprites[152]->draw(sX - 80, sY - 15, state.m_effect_frame % 27, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
			m_game.m_effect_sprites[152]->draw(sX, sY - 15, state.m_effect_frame % 27, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
			m_game.m_effect_sprites[152]->draw(sX - 40, sY, state.m_effect_frame % 27, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
			m_game.m_effect_sprites[163]->draw(sX - 90, sY - 80, state.m_effect_frame % 12, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
			m_game.m_effect_sprites[160]->draw(sX - 60, sY - 50, state.m_effect_frame % 12, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
			m_game.m_effect_sprites[161]->draw(sX - 30, sY - 20, state.m_effect_frame % 12, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
			m_game.m_effect_sprites[162]->draw(sX, sY - 100, state.m_effect_frame % 12, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
			m_game.m_effect_sprites[163]->draw(sX + 30, sY - 30, state.m_effect_frame % 12, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
			m_game.m_effect_sprites[162]->draw(sX + 60, sY - 90, state.m_effect_frame % 12, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
			m_game.m_effect_sprites[163]->draw(sX + 90, sY - 50, state.m_effect_frame % 12, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
			switch (state.m_dir) {
			case 1: m_game.m_effect_sprites[140]->draw(sX, sY, frame, hb::shared::sprite::DrawParams::alpha_blend(0.7f)); break;
			case 2: m_game.m_effect_sprites[141]->draw(sX, sY, frame, hb::shared::sprite::DrawParams::alpha_blend(0.7f)); break;
			case 3: m_game.m_effect_sprites[142]->draw(sX, sY, frame, hb::shared::sprite::DrawParams::alpha_blend(0.7f)); break;
			case 4: m_game.m_effect_sprites[143]->draw(sX, sY, frame, hb::shared::sprite::DrawParams::alpha_blend(0.7f)); break;
			case 5: m_game.m_effect_sprites[144]->draw(sX, sY, frame, hb::shared::sprite::DrawParams::alpha_blend(0.7f)); break;
			case 6: m_game.m_effect_sprites[145]->draw(sX, sY, frame, hb::shared::sprite::DrawParams::alpha_blend(0.7f)); break;
			case 7: m_game.m_effect_sprites[146]->draw(sX, sY, frame, hb::shared::sprite::DrawParams::alpha_blend(0.7f)); break;
			case 8: m_game.m_effect_sprites[147]->draw(sX, sY, frame, hb::shared::sprite::DrawParams::alpha_blend(0.7f)); break;
			}
		}
		else if (state.m_owner_type == hb::shared::owner::Wyvern)
		{
			m_game.m_sprite[eq.m_body_index + (state.m_dir - 1)]->draw(sX, sY, frame, hb::shared::sprite::DrawParams::alpha_blend(0.5f));
		}
		else if (state.m_owner_type == hb::shared::owner::FireWyvern)
		{
			m_game.m_sprite[33]->draw(sX, sY, frame, hb::shared::sprite::DrawParams::alpha_blend(0.5f));
			switch (state.m_dir) {
			case 1: m_game.m_effect_sprites[141]->draw(sX, sY, frame + 8, hb::shared::sprite::DrawParams::alpha_blend(0.7f)); break;
			case 2: m_game.m_effect_sprites[142]->draw(sX, sY, frame + 8, hb::shared::sprite::DrawParams::alpha_blend(0.7f)); break;
			case 3: m_game.m_effect_sprites[143]->draw(sX, sY, frame + 8, hb::shared::sprite::DrawParams::alpha_blend(0.7f)); break;
			case 4: m_game.m_effect_sprites[144]->draw(sX, sY, frame + 8, hb::shared::sprite::DrawParams::alpha_blend(0.7f)); break;
			case 5: m_game.m_effect_sprites[145]->draw(sX, sY, frame + 8, hb::shared::sprite::DrawParams::alpha_blend(0.7f)); break;
			case 6: m_game.m_effect_sprites[146]->draw(sX, sY, frame + 8, hb::shared::sprite::DrawParams::alpha_blend(0.7f)); break;
			case 7: m_game.m_effect_sprites[147]->draw(sX, sY, frame + 8, hb::shared::sprite::DrawParams::alpha_blend(0.7f)); break;
			case 8: m_game.m_effect_sprites[141]->draw(sX, sY, frame + 8, hb::shared::sprite::DrawParams::alpha_blend(0.7f)); break;
			}
		}
		else
		{
			// Normal NPC body draw
			RenderHelpers::draw_body(m_game, eq.m_body_index + (state.m_dir - 1), sX, sY, frame, false, state.m_owner_type, state.m_status.frozen);
		}

		{ auto br = m_game.m_sprite[eq.m_body_index + (state.m_dir - 1)]->GetBoundRect();
		  m_game.m_body_rect = hb::shared::geometry::GameRectangle(br.left, br.top, br.right - br.left, br.bottom - br.top); }

		// Berserk glow
		RenderHelpers::draw_berserk_glow(m_game, eq, state, sX, sY);

		// Angel + aura — OnDying uses 24+dir-1, ORIGINAL frame
		m_screen->draw_angel(24 + (state.m_dir - 1), sX + 20, sY - 20, originalFrame, time);
		m_game.check_active_aura2(sX, sY, time, state.m_owner_type);
	}
	else if (state.m_name[0] != '\0')
	{
		RenderHelpers::draw_name(*m_screen, state, sX, sY);
	}

	// Chat message (always)
	RenderHelpers::update_chat(m_game, state, sX, sY, indexX, indexY);

	return m_game.m_sprite[eq.m_body_index + (state.m_dir - 1)]->GetBoundRect();
}

hb::shared::sprite::BoundRect CNpcRenderer::draw_dead(int indexX, int indexY, int sX, int sY, bool trans, uint32_t time)
{
	hb::shared::sprite::BoundRect invalidRect = {0, -1, 0, 0};
	auto& state = m_game.m_entity_state;

	// Wyvern: early return — no corpse
	if (state.m_owner_type == hb::shared::owner::Wyvern) return invalidRect;

	// Per-mob frame and pose table
	int frame;
	int npcPose;

	switch (state.m_owner_type) {
	case hb::shared::owner::Troll:
	case hb::shared::owner::Ogre:
	case hb::shared::owner::Liche:
	case hb::shared::owner::Demon:
	case hb::shared::owner::Frost:
		frame = 5;
		npcPose = 4;
		break;

	case hb::shared::owner::Unicorn:
	case hb::shared::owner::WereWolf:
	case hb::shared::owner::LightWarBeetle:
	case hb::shared::owner::GodsHandKnight:
	case hb::shared::owner::GodsHandKnightCK:
	case hb::shared::owner::TempleKnight:
	case hb::shared::owner::BattleGolem:
	case hb::shared::owner::Stalker:
	case hb::shared::owner::HellClaw:
	case hb::shared::owner::TigerWorm:
	case hb::shared::owner::Beholder:
	case hb::shared::owner::DarkElf:
	case hb::shared::owner::Bunny:
	case hb::shared::owner::Cat:
	case hb::shared::owner::GiantFrog:
	case hb::shared::owner::MountainGiant:
	case hb::shared::owner::Ettin:
	case hb::shared::owner::CannibalPlant:
	case hb::shared::owner::Rudolph:
	case hb::shared::owner::DireBoar:
	case hb::shared::owner::Crops:
	case hb::shared::owner::IceGolem:
	case hb::shared::owner::Dragon:
	case hb::shared::owner::Centaur:
	case hb::shared::owner::ClawTurtle:
	case hb::shared::owner::GiantCrayfish:
	case hb::shared::owner::GiLizard:
	case hb::shared::owner::GiTree:
	case hb::shared::owner::MasterOrc:
	case hb::shared::owner::Minaus:
	case hb::shared::owner::Nizie:
	case hb::shared::owner::Tentocle:
	case hb::shared::owner::Sorceress:
	case hb::shared::owner::ATK:
	case hb::shared::owner::MasterElf:
	case hb::shared::owner::DSK:
	case hb::shared::owner::Barbarian:
		frame = 7;
		npcPose = 4;
		break;

	case hb::shared::owner::HBT:
	case hb::shared::owner::CT:
	case hb::shared::owner::AGC:
		frame = 7;
		npcPose = 3;
		break;

	case hb::shared::owner::Wyvern:
		frame = 15;
		npcPose = 2;
		break;

	case hb::shared::owner::FireWyvern:
		frame = 7;
		npcPose = 2;
		trans = true;
		break;

	case hb::shared::owner::Abaddon:
		frame = 0;
		npcPose = 3;
		trans = true;
		break;

	case hb::shared::owner::Catapult:
		frame = 0;
		npcPose = 4;
		break;

	case hb::shared::owner::Gargoyle:
		frame = 11;
		npcPose = 4;
		break;

	case hb::shared::owner::Gate:
		frame = 5;
		npcPose = 2;
		break;

	default:
		frame = 3;
		npcPose = 4;
		break;
	}

	EquipmentIndices eq = EquipmentIndices::CalcNpc(state, npcPose);
	eq.calc_colors(state);

	if (!trans)
	{
		if (state.m_frame == -1)
		{
			// Full corpse draw — just-died state
			state.m_frame = frame;
			RenderHelpers::draw_body(m_game, eq.m_body_index + (state.m_dir - 1), sX, sY, frame, false, state.m_owner_type, state.m_status.frozen);
		}
		else if (state.m_status.berserk)
		{
			// Berserk corpse fade — smoothly reaches full transparency by frame 10
			int remaining = (std::max)(0, 10 - state.m_frame);
			int r = 202 * remaining / 10;
			int gb = 182 * remaining / 10;
			float alpha = 0.7f * remaining / 10.0f;
			m_game.m_sprite[eq.m_body_index + (state.m_dir - 1)]->draw(sX, sY, frame,
				hb::shared::sprite::DrawParams::tinted_alpha(r, gb, gb, alpha));
		}
		else
		{
			// Normal corpse fade — smoothly reaches full transparency by frame 10
			int remaining = (std::max)(0, 10 - state.m_frame);
			int fade = 192 * remaining / 10;
			float alpha = 0.7f * remaining / 10.0f;
			m_game.m_sprite[eq.m_body_index + (state.m_dir - 1)]->draw(sX, sY, frame,
				hb::shared::sprite::DrawParams::tinted_alpha(fade, fade, fade, alpha));
		}
	}
	else if (state.m_name[0] != '\0')
	{
		RenderHelpers::draw_name(*m_screen, state, sX, sY);
	}

	// Chat message — uses clear_dead_chat_msg for dead entities
	if (state.m_chat_index != 0)
	{
		if (m_game.get_floating_text().is_valid(state.m_chat_index, state.m_object_id))
		{
			m_game.get_floating_text().update_position(state.m_chat_index, static_cast<short>(sX), static_cast<short>(sY));
		}
		else
		{
			m_game.m_map_data->clear_dead_chat_msg(indexX, indexY);
		}
	}

	// Abaddon corpse effects
	if (state.m_owner_type == hb::shared::owner::Abaddon)
	{
		m_screen->abaddon_corpse(sX, sY);
	}
	else if (state.m_owner_type == hb::shared::owner::FireWyvern)
	{
		m_game.m_effect_sprites[35]->draw(sX + 20, sY - 15, rand() % 10, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
	}

	return m_game.m_sprite[eq.m_body_index + (state.m_dir - 1)]->GetBoundRect();
}
