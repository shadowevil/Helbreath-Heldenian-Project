#include "PlayerRenderer.h"
#include "Game.h"
#include "Screen_OnGame.h"
#include "FloatingTextManager.h"
#include "EquipmentIndices.h"
#include "RenderHelpers.h"
#include "CommonTypes.h"
#include <algorithm>
using namespace hb::shared::direction;
using hb::shared::item::EquipPos;

hb::shared::sprite::BoundRect CPlayerRenderer::draw_stop(int indexX, int indexY, int sX, int sY, bool trans, uint32_t time)
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

	// Frame halving for stop animation
	state.m_frame = state.m_frame / 2;

	// Calculate equipment indices — walking uses pose 1, standing uses pose 0
	bool walking = state.m_appearance.is_walking;
	int bodyPose = walking ? 1 : 0;
	EquipmentIndices eq = EquipmentIndices::CalcPlayer(state, bodyPose);
	eq.calc_colors(state);

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

		// draw all equipment layers in correct z-order
		RenderHelpers::draw_player_layers(m_game, eq, state, sX, sY, inv, mantle_draw_order, 8, admin_invis);

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
		// trans mode: only draw name
		RenderHelpers::draw_name(*m_screen, state, sX, sY);
	}

	// Chat message (always)
	RenderHelpers::update_chat(m_game, state, sX, sY, indexX, indexY);

	// Abaddon effects (always)
	RenderHelpers::draw_abaddon_effects(m_game, state, sX, sY);

	// GM mode (always)
	RenderHelpers::draw_gm_effect(m_game, state, sX, sY);

	// AFK indicator (local player only)
	RenderHelpers::draw_afk_effect(m_game, state, sX, sY, time);

	return m_game.m_sprite[eq.m_body_index + (state.m_dir - 1)]->GetBoundRect();
}

hb::shared::sprite::BoundRect CPlayerRenderer::draw_move(int indexX, int indexY, int sX, int sY, bool trans, uint32_t time)
{
	hb::shared::sprite::BoundRect invalidRect = {0, -1, 0, 0};
	auto& state = m_game.m_entity_state;
	bool inv = false;
	bool admin_invis = false;

	// Invisibility check
	if (RenderHelpers::check_invisibility(m_game, state, inv, admin_invis))
		return invalidRect;

	// Calculate equipment indices — walking uses pose 3, standing uses pose 2
	bool walking = state.m_appearance.is_walking;
	int bodyPose = walking ? 3 : 2;
	EquipmentIndices eq = EquipmentIndices::CalcPlayer(state, bodyPose);
	eq.calc_colors(state);

	// Motion offset
	int dx = static_cast<int>(m_game.m_map_data->m_data[state.m_data_x][state.m_data_y].m_motion.m_current_offset_x);
	int dy = static_cast<int>(m_game.m_map_data->m_data[state.m_data_x][state.m_data_y].m_motion.m_current_offset_y);
	int fix_x = sX + dx;
	int fix_y = sY + dy;

	// Players (types 1-6) never get frame halving in OnMove

	// Crusade FOE indicator
	if (m_screen->m_is_crusade_mode)
		m_screen->draw_object_foe(fix_x, fix_y, state.m_frame);

	// Effect auras
	RenderHelpers::draw_effect_auras(m_game, state, fix_x, fix_y);

	if (!trans)
	{
		m_game.check_active_aura(fix_x, fix_y, time, state.m_owner_type);

		// draw all equipment layers in correct z-order
		RenderHelpers::draw_player_layers(m_game, eq, state, fix_x, fix_y, inv, mantle_draw_order, 8, admin_invis);

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

	// AFK indicator (local player only)
	RenderHelpers::draw_afk_effect(m_game, state, fix_x, fix_y, time);

	return m_game.m_sprite[eq.m_body_index + (state.m_dir - 1)]->GetBoundRect();
}

hb::shared::sprite::BoundRect CPlayerRenderer::draw_run(int indexX, int indexY, int sX, int sY, bool trans, uint32_t time)
{
	hb::shared::sprite::BoundRect invalidRect = {0, -1, 0, 0};
	auto& state = m_game.m_entity_state;
	bool inv = false;
	bool admin_invis = false;

	// Invisibility check
	if (RenderHelpers::check_invisibility(m_game, state, inv, admin_invis))
		return invalidRect;

	// bodyPose=4 for running (weapon/shield use same pose in new system)
	EquipmentIndices eq = EquipmentIndices::CalcPlayer(state, 4);
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

		// draw all equipment layers — uses OnRun mantle order
		RenderHelpers::draw_player_layers(m_game, eq, state, fix_x, fix_y, inv, mantle_draw_order_running, 8, admin_invis);

		// Berserk glow
		RenderHelpers::draw_berserk_glow(m_game, eq, state, fix_x, fix_y);

		// Angel + aura
		m_screen->draw_angel(40 + (state.m_dir - 1), fix_x + 20, fix_y - 20, state.m_frame % 4, time);
		m_game.check_active_aura2(fix_x, fix_y, time, state.m_owner_type);

		// Haste trail effect
		if (state.m_status.haste)
		{
			int bodyDirIndex = eq.m_body_index + (state.m_dir - 1);
			for (int i = 1; i <= 5; i++)
			{
				int tx = fix_x, ty = fix_y;
				switch (state.m_dir) {
				case 1: ty += i * 5; break;
				case 2: tx -= i * 5; ty += i * 5; break;
				case 3: tx -= i * 5; break;
				case 4: tx -= i * 5; ty -= i * 5; break;
				case 5: ty -= i * 5; break;
				case 6: tx += i * 5; ty -= i * 5; break;
				case 7: tx += i * 5; break;
				case 8: tx += i * 5; ty += i * 5; break;
				}
				m_game.m_sprite[bodyDirIndex]->draw(tx, ty, state.m_frame,
					hb::shared::sprite::DrawParams::tinted_alpha(126, 192, 242, 0.7f));
			}
		}
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

	// AFK indicator (local player only)
	RenderHelpers::draw_afk_effect(m_game, state, fix_x, fix_y, time);

	return m_game.m_sprite[eq.m_body_index + (state.m_dir - 1)]->GetBoundRect();
}

hb::shared::sprite::BoundRect CPlayerRenderer::draw_attack(int indexX, int indexY, int sX, int sY, bool trans, uint32_t time)
{
	hb::shared::sprite::BoundRect invalidRect = {0, -1, 0, 0};
	auto& state = m_game.m_entity_state;
	bool inv = false;
	bool admin_invis = false;

	// Invisibility check
	if (RenderHelpers::check_invisibility(m_game, state, inv, admin_invis))
		return invalidRect;

	// Attack poses depend on walking state and weapon type
	// Pose 6 = melee attack (all swords, axes, hammers, wands — both 1H and 2H)
	// Pose 7 = bow attack (archery weapons only)
	EquipmentIndices eq;
	if (state.m_appearance.is_walking)
	{
		constexpr int archery_skill = 6;
		bool is_bow = false;
		if (state.m_appearance.weapon_item_id > 0) {
			CItem* cfg = m_game.get_item_config(state.m_appearance.weapon_item_id);
			if (cfg) is_bow = (cfg->m_related_skill == archery_skill);
		}
		int pose = is_bow ? 7 : 6;
		eq = EquipmentIndices::CalcPlayer(state, pose);
	}
	else
	{
		// Standing attack: bodyPose=5, no weapon/shield drawn
		eq = EquipmentIndices::CalcPlayer(state, 5, false, false);
	}
	eq.calc_colors(state);

	// Crusade FOE indicator
	if (m_screen->m_is_crusade_mode)
		m_screen->draw_object_foe(sX, sY, state.m_frame);

	// Effect auras
	RenderHelpers::draw_effect_auras(m_game, state, sX, sY);

	if (!trans)
	{
		m_game.check_active_aura(sX, sY, time, state.m_owner_type);

		// draw all equipment layers
		RenderHelpers::draw_player_layers(m_game, eq, state, sX, sY, inv, mantle_draw_order, 8, admin_invis);

		// Attack-specific: weapon swing trail at frame 3 (melee only, not bows)
		if (eq.m_weapon_index != -1 && state.m_frame == 3 && state.m_appearance.is_walking)
		{
			constexpr int archery_skill = 6;
			bool is_bow = false;
			if (state.m_appearance.weapon_item_id > 0) {
				CItem* cfg = m_game.get_item_config(state.m_appearance.weapon_item_id);
				if (cfg) is_bow = (cfg->m_related_skill == archery_skill);
			}
			if (!is_bow)
			{
				int trailFrame = (state.m_dir - 1) * 8 + (state.m_frame - 1);
				m_game.m_equip_sprites[eq.m_weapon_index]->draw(sX, sY, trailFrame,
					hb::shared::sprite::DrawParams::tinted_alpha(126, 192, 242, 0.7f));
			}
		}

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

	// Abaddon effects (always) — players can't be Abaddon, but keep for consistency
	RenderHelpers::draw_abaddon_effects(m_game, state, sX, sY);

	// GM mode (always)
	RenderHelpers::draw_gm_effect(m_game, state, sX, sY);

	// AFK indicator (local player only)
	RenderHelpers::draw_afk_effect(m_game, state, sX, sY, time);

	return m_game.m_sprite[eq.m_body_index + (state.m_dir - 1)]->GetBoundRect();
}

hb::shared::sprite::BoundRect CPlayerRenderer::draw_attack_move(int indexX, int indexY, int sX, int sY, bool trans, uint32_t time)
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

	// Same poses as draw_attack — pose 6 melee, pose 7 bow
	EquipmentIndices eq;
	if (state.m_appearance.is_walking)
	{
		constexpr int archery_skill = 6;
		bool is_bow = false;
		if (state.m_appearance.weapon_item_id > 0) {
			CItem* cfg = m_game.get_item_config(state.m_appearance.weapon_item_id);
			if (cfg) is_bow = (cfg->m_related_skill == archery_skill);
		}
		int pose = is_bow ? 7 : 6;
		eq = EquipmentIndices::CalcPlayer(state, pose);
	}
	else
	{
		eq = EquipmentIndices::CalcPlayer(state, 5, false, false);
	}
	eq.calc_colors(state);

	// Frame-based motion offset
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

		// draw all equipment layers
		RenderHelpers::draw_player_layers(m_game, eq, state, sX + dx, sY + dy, inv, mantle_draw_order, 8, admin_invis);

		// Attack-specific: weapon swing trail at frame 3
		if (eq.m_weapon_index != -1 && state.m_frame == 3)
		{
			int trailFrame = (state.m_dir - 1) * 8 + (state.m_frame - 1);
			m_game.m_equip_sprites[eq.m_weapon_index]->draw(sX + dx, sY + dy, trailFrame,
				hb::shared::sprite::DrawParams::tinted_alpha(126, 192, 242, 0.7f));
		}

		// Berserk glow
		RenderHelpers::draw_berserk_glow(m_game, eq, state, sX + dx, sY + dy);

		// Angel + aura
		m_screen->draw_angel(8 + (state.m_dir - 1), sX + dx + 20, sY + dy - 20, state.m_frame % 8, time);
		m_game.check_active_aura2(sX + dx, sY + dy, time, state.m_owner_type);

		// Dash ghost effect
		if (dash_draw)
		{
			int ghostDirFrame = (state.m_dir - 1) * 8 + state.m_frame;
			m_game.m_sprite[eq.m_body_index + (state.m_dir - 1)]->draw(sX + dsx, sY + dsy, state.m_frame,
				hb::shared::sprite::DrawParams::tinted_alpha(126, 192, 242, 0.7f));
			if (eq.m_weapon_index != -1)
				m_game.m_equip_sprites[eq.m_weapon_index]->draw(sX + dsx, sY + dsy, ghostDirFrame,
					hb::shared::sprite::DrawParams::tinted_alpha(126, 192, 242, 0.7f));
			if (eq.m_shield_index != -1)
				m_game.m_equip_sprites[eq.m_shield_index]->draw(sX + dsx, sY + dsy, ghostDirFrame,
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

	// AFK indicator (local player only)
	RenderHelpers::draw_afk_effect(m_game, state, sX + dx, sY + dy, time);

	return m_game.m_sprite[eq.m_body_index + (state.m_dir - 1)]->GetBoundRect();
}

hb::shared::sprite::BoundRect CPlayerRenderer::draw_magic(int indexX, int indexY, int sX, int sY, bool trans, uint32_t time)
{
	hb::shared::sprite::BoundRect invalidRect = {0, -1, 0, 0};
	auto& state = m_game.m_entity_state;
	bool inv = false;
	bool admin_invis = false;

	// Magic has special invisibility handling — updates chat position for invisible enemies
	if (hb::shared::owner::is_always_invisible(state.m_owner_type)) inv = true;
	if (state.m_status.invisibility)
	{
		if (state.m_object_id == m_game.m_player->m_player_object_id)
			inv = true;
		else
		{
			// update chat position even for invisible enemies
			if (state.m_chat_index != 0)
			{
				if (m_game.get_floating_text().is_occupied(state.m_chat_index))
				{
					m_game.get_floating_text().update_position(state.m_chat_index, static_cast<short>(sX), static_cast<short>(sY));
				}
				else
				{
					m_game.m_map_data->clear_chat_msg(indexX, indexY);
				}
			}
			return invalidRect;
		}
	}

	// bodyPose=8, no weapon/shield
	EquipmentIndices eq = EquipmentIndices::CalcPlayer(state, 8, false, false);
	eq.calc_colors(state);

	// Crusade FOE indicator
	if (m_screen->m_is_crusade_mode)
		m_screen->draw_object_foe(sX, sY, state.m_frame);

	// Effect auras
	RenderHelpers::draw_effect_auras(m_game, state, sX, sY);

	if (!trans)
	{
		m_game.check_active_aura(sX, sY, time, state.m_owner_type);

		// Spell casting ground effect (effect5 sprite 7)
		if (m_game.m_effect_sprites[30])
			m_game.m_effect_sprites[30]->draw(sX, sY + 8, state.m_frame % 16, hb::shared::sprite::DrawParams::additive());

		// draw all equipment layers — equipFrameMul=16 for magic
		RenderHelpers::draw_player_layers(m_game, eq, state, sX, sY, inv, mantle_draw_order, 16, admin_invis);

		// Berserk glow
		RenderHelpers::draw_berserk_glow(m_game, eq, state, sX, sY);

		// Angel + aura — OnMagic uses 32+dir-1, frame%16
		m_screen->draw_angel(32 + (state.m_dir - 1), sX + 20, sY - 20, state.m_frame % 16, time);
		m_game.check_active_aura2(sX, sY, time, state.m_owner_type);
	}
	else if (state.m_name[0] != '\0')
	{
		RenderHelpers::draw_name(*m_screen, state, sX, sY);
	}

	// Chat message (always)
	RenderHelpers::update_chat(m_game, state, sX, sY, indexX, indexY);

	// AFK indicator (local player only)
	RenderHelpers::draw_afk_effect(m_game, state, sX, sY, time);

	return m_game.m_sprite[eq.m_body_index + (state.m_dir - 1)]->GetBoundRect();
}

hb::shared::sprite::BoundRect CPlayerRenderer::draw_get_item(int indexX, int indexY, int sX, int sY, bool trans, uint32_t time)
{
	hb::shared::sprite::BoundRect invalidRect = {0, -1, 0, 0};
	auto& state = m_game.m_entity_state;
	bool inv = false;
	bool admin_invis = false;

	// Invisibility check
	if (RenderHelpers::check_invisibility(m_game, state, inv, admin_invis))
		return invalidRect;

	// bodyPose=9, no weapon/shield
	EquipmentIndices eq = EquipmentIndices::CalcPlayer(state, 9, false, false);
	eq.calc_colors(state);

	// Crusade FOE indicator
	if (m_screen->m_is_crusade_mode)
		m_screen->draw_object_foe(sX, sY, state.m_frame);

	// Effect auras
	RenderHelpers::draw_effect_auras(m_game, state, sX, sY);

	if (!trans)
	{
		m_game.check_active_aura(sX, sY, time, state.m_owner_type);

		// draw all equipment layers — equipFrameMul=4 for get item
		RenderHelpers::draw_player_layers(m_game, eq, state, sX, sY, inv, mantle_draw_order, 4, admin_invis);

		// Berserk glow
		RenderHelpers::draw_berserk_glow(m_game, eq, state, sX, sY);

		// Angel + aura — OnGetItem uses 40+dir-1, frame%4
		m_screen->draw_angel(40 + (state.m_dir - 1), sX + 20, sY - 20, state.m_frame % 4, time);
		m_game.check_active_aura2(sX, sY, time, state.m_owner_type);
	}
	else if (state.m_name[0] != '\0')
	{
		RenderHelpers::draw_name(*m_screen, state, sX, sY);
	}

	// Chat message (always)
	RenderHelpers::update_chat(m_game, state, sX, sY, indexX, indexY);

	// AFK indicator (local player only)
	RenderHelpers::draw_afk_effect(m_game, state, sX, sY, time);

	return m_game.m_sprite[eq.m_body_index + (state.m_dir - 1)]->GetBoundRect();
}

hb::shared::sprite::BoundRect CPlayerRenderer::draw_damage(int indexX, int indexY, int sX, int sY, bool trans, uint32_t time)
{
	hb::shared::sprite::BoundRect invalidRect = {0, -1, 0, 0};
	auto& state = m_game.m_entity_state;
	bool inv = false;
	bool admin_invis = false;

	// Invisibility check
	if (RenderHelpers::check_invisibility(m_game, state, inv, admin_invis))
		return invalidRect;

	// Two-state: frame<4 = idle, frame>=4 = damage recoil
	EquipmentIndices eq;
	int equipFrameMul;
	char frame = state.m_frame;

	if (frame < 4)
	{
		// Idle state — walking or standing, with weapon/shield
		bool walking = state.m_appearance.is_walking;
		int bodyPose = walking ? 1 : 0;
		eq = EquipmentIndices::CalcPlayer(state, bodyPose);
		equipFrameMul = 8;
	}
	else
	{
		// Damage recoil state — pose 10
		frame -= 4;
		state.m_frame = frame;
		eq = EquipmentIndices::CalcPlayer(state, 10);
		equipFrameMul = 4;
	}
	eq.calc_colors(state);

	// Crusade FOE indicator
	if (m_screen->m_is_crusade_mode)
		m_screen->draw_object_foe(sX, sY, frame);

	// Effect auras
	RenderHelpers::draw_effect_auras(m_game, state, sX, sY);

	if (!trans)
	{
		m_game.check_active_aura(sX, sY, time, state.m_owner_type);

		// draw all equipment layers
		RenderHelpers::draw_player_layers(m_game, eq, state, sX, sY, inv, mantle_draw_order, equipFrameMul);

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

	// AFK indicator (local player only)
	RenderHelpers::draw_afk_effect(m_game, state, sX, sY, time);

	return m_game.m_sprite[eq.m_body_index + (state.m_dir - 1)]->GetBoundRect();
}

hb::shared::sprite::BoundRect CPlayerRenderer::draw_damage_move(int indexX, int indexY, int sX, int sY, bool trans, uint32_t time)
{
	hb::shared::sprite::BoundRect invalidRect = {0, -1, 0, 0};
	auto& state = m_game.m_entity_state;
	bool inv = false;
	bool admin_invis = false;

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

	// bodyPose=10 for damage knockback
	EquipmentIndices eq = EquipmentIndices::CalcPlayer(state, 10);
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

		// draw all equipment layers — equipFrameMul=4 for damage move
		RenderHelpers::draw_player_layers(m_game, eq, state, fix_x, fix_y, inv, mantle_draw_order, 4, admin_invis);

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

	// AFK indicator (local player only)
	RenderHelpers::draw_afk_effect(m_game, state, fix_x, fix_y, time);

	return m_game.m_sprite[eq.m_body_index + (state.m_dir - 1)]->GetBoundRect();
}

hb::shared::sprite::BoundRect CPlayerRenderer::draw_dying(int indexX, int indexY, int sX, int sY, bool trans, uint32_t time)
{
	hb::shared::sprite::BoundRect invalidRect = {0, -1, 0, 0};
	auto& state = m_game.m_entity_state;

	// No invisibility check for dying

	// save original frame for angel drawing
	int originalFrame = state.m_frame;
	char frame = state.m_frame;

	// Two-state: frame<6 = standing idle, frame>=6 = dying animation
	EquipmentIndices eq;
	if (frame < 6)
	{
		// Standing idle — bodyPose=0, no weapon/shield
		eq = EquipmentIndices::CalcPlayer(state, 0, false, false);
	}
	else
	{
		// Dying animation — bodyPose=11, no weapon/shield
		frame -= 6;
		state.m_frame = frame;
		eq = EquipmentIndices::CalcPlayer(state, 11, false, false);
	}
	eq.calc_colors(state);

	// Crusade FOE indicator
	if (m_screen->m_is_crusade_mode)
		m_screen->draw_object_foe(sX, sY, frame);

	// Effect auras
	RenderHelpers::draw_effect_auras(m_game, state, sX, sY);

	if (!trans)
	{
		// draw all equipment layers — equipFrameMul=8
		RenderHelpers::draw_player_layers(m_game, eq, state, sX, sY, false, mantle_draw_order);

		// Berserk glow
		RenderHelpers::draw_berserk_glow(m_game, eq, state, sX, sY);

		// Angel + aura — OnDying uses 24+dir-1, ORIGINAL frame (not adjusted)
		m_screen->draw_angel(24 + (state.m_dir - 1), sX + 20, sY - 20, originalFrame, time);
		m_game.check_active_aura2(sX, sY, time, state.m_owner_type);
	}
	else if (state.m_name[0] != '\0')
	{
		RenderHelpers::draw_name(*m_screen, state, sX, sY);
	}

	// Chat message (always)
	RenderHelpers::update_chat(m_game, state, sX, sY, indexX, indexY);

	// AFK indicator (local player only)
	RenderHelpers::draw_afk_effect(m_game, state, sX, sY, time);

	return m_game.m_sprite[eq.m_body_index + (state.m_dir - 1)]->GetBoundRect();
}

hb::shared::sprite::BoundRect CPlayerRenderer::draw_dead(int indexX, int indexY, int sX, int sY, bool trans, uint32_t time)
{
	hb::shared::sprite::BoundRect invalidRect = {0, -1, 0, 0};
	auto& state = m_game.m_entity_state;

	// bodyPose=11, frame=7 fixed for dead players
	EquipmentIndices eq = EquipmentIndices::CalcPlayer(state, 11, false, false);
	eq.calc_colors(state);
	int frame = 7;

	if (!trans)
	{
		if (state.m_frame == -1)
		{
			// Full corpse with equipment — just-died state
			state.m_frame = 7;
			RenderHelpers::draw_player_layers(m_game, eq, state, sX, sY, false, mantle_draw_order);
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

	return m_game.m_sprite[eq.m_body_index + (state.m_dir - 1)]->GetBoundRect();
}
