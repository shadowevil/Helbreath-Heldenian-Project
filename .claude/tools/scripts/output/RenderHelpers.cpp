#include "RenderHelpers.h"
#include "Game.h"
#include "Screen_OnGame.h"
#include "FloatingTextManager.h"
#include "CommonTypes.h"
#include "ConfigManager.h"
#include <algorithm>
using namespace hb::shared::direction;

// -----------------------------------------------------------------------
// Drawing order arrays (moved from Game.cpp file scope)
// Indexed by direction (1-8). Element [0] is unused.
// -----------------------------------------------------------------------

namespace RenderHelpers
{

// -----------------------------------------------------------------------
// Helper: should this mob type skip shadow rendering?
// -----------------------------------------------------------------------
static bool ShouldSkipShadow(short owner_type)
{
	switch (owner_type) {
	case hb::shared::owner::Slime:
	case hb::shared::owner::EnergySphere:
	case hb::shared::owner::TigerWorm:
	case hb::shared::owner::Catapult:
	case hb::shared::owner::CannibalPlant:
	case hb::shared::owner::IceGolem:
	case hb::shared::owner::Abaddon:
	case hb::shared::owner::Gate:
		return true;
	default:
		return false;
	}
}

// -----------------------------------------------------------------------
void draw_equip_layer(hb::shared::sprite::SpriteCollection& sprites, int spriteIndex, int sX, int sY, int frame,
                    bool inv, int colorIndex)
{
	if (spriteIndex == -1) return;

	if (inv)
	{
		sprites[spriteIndex]->draw(sX, sY, frame, hb::shared::sprite::DrawParams::alpha_blend(0.25f));
	}
	else if (colorIndex == 0)
	{
		sprites[spriteIndex]->draw(sX, sY, frame);
	}
	else
	{
		auto c = GameColors::Items[colorIndex];
		sprites[spriteIndex]->draw(sX, sY, frame,
			hb::shared::sprite::DrawParams::tint(c.r, c.g, c.b));
	}
}

// -----------------------------------------------------------------------
void draw_weapon(CGame& game, hb::shared::sprite::SpriteCollection& sprites,
                const EquipmentIndices& eq, int sX, int sY,
                int weaponFrame, bool inv)
{
	if (eq.m_weapon_index == -1) return;

	if (inv)
	{
		sprites[eq.m_weapon_index]->draw(sX, sY, weaponFrame, hb::shared::sprite::DrawParams::alpha_blend(0.25f));
	}
	else if (eq.m_weapon_color == 0)
	{
		sprites[eq.m_weapon_index]->draw(sX, sY, weaponFrame);
	}
	else
	{
		auto c = GameColors::Weapons[eq.m_weapon_color];
		sprites[eq.m_weapon_index]->draw(sX, sY, weaponFrame,
			hb::shared::sprite::DrawParams::tint(c.r, c.g, c.b));
	}

	// DK set glare — single-pass shader rendering matching DDraw PutTransSpriteRGB.
	// Shader adds a flat color offset to every pixel before additive blending:
	//   dest += clamp(src + (r, g, b))
	int weaponGlare = eq.m_weapon_glare;
	if (auto* on_game = GameModeManager::get_active_screen_as<Screen_OnGame>())
		on_game->dk_glare(eq.m_weapon_color, eq.m_weapon_item_id, &weaponGlare);
	if (weaponGlare != 0)
	{
		int f = game.on_game()->m_draw_flag;
		switch (weaponGlare) {
		case 1: sprites[eq.m_weapon_index]->draw(sX, sY, weaponFrame, hb::shared::sprite::DrawParams::additive_tinted(f, 0, 0)); break;
		case 2: sprites[eq.m_weapon_index]->draw(sX, sY, weaponFrame, hb::shared::sprite::DrawParams::additive_tinted(0, f, 0)); break;
		case 3: sprites[eq.m_weapon_index]->draw(sX, sY, weaponFrame, hb::shared::sprite::DrawParams::additive_tinted(0, 0, f)); break;
		}
	}
}

// -----------------------------------------------------------------------
void draw_shield(CGame& game, hb::shared::sprite::SpriteCollection& sprites,
                const EquipmentIndices& eq, int sX, int sY,
                int frame, bool inv)
{
	if (eq.m_shield_index == -1) return;

	if (inv)
	{
		sprites[eq.m_shield_index]->draw(sX, sY, frame, hb::shared::sprite::DrawParams::alpha_blend(0.25f));
	}
	else if (eq.m_shield_color == 0)
	{
		sprites[eq.m_shield_index]->draw(sX, sY, frame);
	}
	else
	{
		auto c = GameColors::Items[eq.m_shield_color];
		sprites[eq.m_shield_index]->draw(sX, sY, frame,
			hb::shared::sprite::DrawParams::tint(c.r, c.g, c.b));
	}

	// Shield glare — single-pass shader matching DDraw PutTransSpriteRGB
	if (eq.m_shield_glare != 0)
	{
		int sf = game.on_game()->m_draw_flag;
		switch (eq.m_shield_glare) {
		case 1:
			// GM sprite (m_effect_sprites[45]) is only drawn by draw_gm_effect when gm_mode is true
			// fallthrough to case 2 for green offset shield glare
		case 2: sprites[eq.m_shield_index]->draw(sX, sY, frame, hb::shared::sprite::DrawParams::additive_tinted(0, sf, 0)); break;
		case 3: sprites[eq.m_shield_index]->draw(sX, sY, frame, hb::shared::sprite::DrawParams::additive_tinted(0, 0, sf)); break;
		}
	}
}

// -----------------------------------------------------------------------
void draw_shadow(CGame& game, int body_dir_index, int sX, int sY, int frame,
                bool inv, short owner_type)
{
	if (ShouldSkipShadow(owner_type)) return;
	if (config_manager::get().get_detail_level() == 0) return;
	if (inv) return;

	game.m_sprite[body_dir_index]->draw(sX, sY, frame, hb::shared::sprite::DrawParams::shadow());
}

// -----------------------------------------------------------------------
void draw_body(CGame& game, int body_dir_index, int sX, int sY, int frame,
              bool inv, short owner_type, bool frozen, bool admin_invis)
{
	if (owner_type == hb::shared::owner::Abaddon)
	{
		game.m_sprite[body_dir_index]->draw(sX, sY, frame, hb::shared::sprite::DrawParams::alpha_blend(0.5f));
	}
	else if (admin_invis)
	{
		game.m_sprite[body_dir_index]->draw(sX, sY, frame,
			hb::shared::sprite::DrawParams::tinted_alpha(255, 132, 132, 0.5f));
	}
	else if (inv)
	{
		game.m_sprite[body_dir_index]->draw(sX, sY, frame, hb::shared::sprite::DrawParams::alpha_blend(0.5f));
	}
	else if (frozen)
	{
		game.m_sprite[body_dir_index]->draw(sX, sY, frame,
			hb::shared::sprite::DrawParams::tint(94, 160, 208));
	}
	else
	{
		game.m_sprite[body_dir_index]->draw(sX, sY, frame);
	}

	// Capture body bounding rect
	auto br = game.m_sprite[body_dir_index]->GetBoundRect();
	game.m_body_rect = hb::shared::geometry::GameRectangle(br.left, br.top, br.right - br.left, br.bottom - br.top);
}

// -----------------------------------------------------------------------
// Internal: draw the non-weapon/shield equipment layers in correct order
// -----------------------------------------------------------------------
static void DrawEquipmentStack(CGame& game, const EquipmentIndices& eq,
                               const CEntityRenderState& state, int sX, int sY,
                               bool inv, const char* mantleOrder, int equipFrameMul = 8)
{
	int dir = state.m_dir;
	int frame = state.m_frame;
	int dirFrame = (dir - 1) * equipFrameMul + frame;

	// Equipment draws from m_equip_sprites, cosmetics from m_sprite
	auto& equip = game.m_equip_sprites;
	auto& cosmetic = game.m_sprite;

	// Mantle behind body (order 0)
	if (eq.m_mantle_index != -1 && mantleOrder[dir] == 0)
		draw_equip_layer(equip, eq.m_mantle_index, sX, sY, dirFrame, inv, eq.m_mantle_color);

	// Undies (cosmetic — from m_sprite)
	draw_equip_layer(cosmetic, eq.m_undies_index, sX, sY, dirFrame, inv, 0);

	// Hair (cosmetic — from m_sprite, only if no helm)
	if (eq.m_hair_index != -1 && eq.m_helm_index == -1)
	{
		if (inv)
		{
			cosmetic[eq.m_hair_index]->draw(sX, sY, dirFrame, hb::shared::sprite::DrawParams::alpha_blend(0.25f));
		}
		else
		{
			const auto& hc = GameColors::Hair[state.m_appearance.hair_color];
			cosmetic[eq.m_hair_index]->draw(sX, sY, dirFrame, hb::shared::sprite::DrawParams::tint(hc.r, hc.g, hc.b));
		}
	}

	// Boots before pants if wearing skirt
	if (eq.m_skirt_draw == 1)
		draw_equip_layer(equip, eq.m_boots_index, sX, sY, dirFrame, inv, eq.m_boots_color);

	// Pants
	draw_equip_layer(equip, eq.m_pants_index, sX, sY, dirFrame, inv, eq.m_pants_color);

	// Arm armor
	draw_equip_layer(equip, eq.m_arm_armor_index, sX, sY, dirFrame, inv, eq.m_arm_color);

	// Boots after pants if not wearing skirt
	if (eq.m_skirt_draw == 0)
		draw_equip_layer(equip, eq.m_boots_index, sX, sY, dirFrame, inv, eq.m_boots_color);

	// Body armor
	draw_equip_layer(equip, eq.m_body_armor_index, sX, sY, dirFrame, inv, eq.m_armor_color);

	// Helm
	draw_equip_layer(equip, eq.m_helm_index, sX, sY, dirFrame, inv, eq.m_helm_color);

	// Mantle over armor (order 2)
	if (eq.m_mantle_index != -1 && mantleOrder[dir] == 2)
		draw_equip_layer(equip, eq.m_mantle_index, sX, sY, dirFrame, inv, eq.m_mantle_color);

	// Shield + glare (from m_equip_sprites)
	draw_shield(game, equip, eq, sX, sY, dirFrame, inv);

	// Mantle in front (order 1)
	if (eq.m_mantle_index != -1 && mantleOrder[dir] == 1)
		draw_equip_layer(equip, eq.m_mantle_index, sX, sY, dirFrame, inv, eq.m_mantle_color);
}

// -----------------------------------------------------------------------
void draw_player_layers(CGame& game, const EquipmentIndices& eq,
                      const CEntityRenderState& state, int sX, int sY,
                      bool inv, const char* mantleOrder, int equipFrameMul,
                      bool admin_invis)
{
	int dir = state.m_dir;
	int frame = state.m_frame;
	int bodyDirIndex = eq.m_body_index + (dir - 1);
	int weaponDirFrame = (dir - 1) * equipFrameMul + frame;

	if (weapon_draw_order[dir] == 1)
	{
		// Weapon before body (from m_equip_sprites, direction now in frame)
		draw_weapon(game, game.m_equip_sprites, eq, sX, sY, weaponDirFrame, inv);
		draw_shadow(game, bodyDirIndex, sX, sY, frame, inv, state.m_owner_type);

		// Energy sphere light
		if (state.m_owner_type == hb::shared::owner::EnergySphere)
			game.m_effect_sprites[0]->draw(sX, sY, 1, hb::shared::sprite::DrawParams::alpha_blend(0.5f));

		draw_body(game, bodyDirIndex, sX, sY, frame, inv, state.m_owner_type, state.m_status.frozen, admin_invis);
		DrawEquipmentStack(game, eq, state, sX, sY, inv, mantleOrder, equipFrameMul);
	}
	else
	{
		// Body before weapon
		draw_shadow(game, bodyDirIndex, sX, sY, frame, inv, state.m_owner_type);

		if (state.m_owner_type == hb::shared::owner::EnergySphere)
			game.m_effect_sprites[0]->draw(sX, sY, 1, hb::shared::sprite::DrawParams::alpha_blend(0.5f));

		draw_body(game, bodyDirIndex, sX, sY, frame, inv, state.m_owner_type, state.m_status.frozen, admin_invis);
		DrawEquipmentStack(game, eq, state, sX, sY, inv, mantleOrder, equipFrameMul);
		draw_weapon(game, game.m_equip_sprites, eq, sX, sY, weaponDirFrame, inv);
	}
}

// -----------------------------------------------------------------------
void draw_npc_layers(CGame& game, const EquipmentIndices& eq,
                   const CEntityRenderState& state, int sX, int sY,
                   bool inv)
{
	int dir = state.m_dir;
	int frame = state.m_frame;
	int bodyDirIndex = eq.m_body_index + (dir - 1);

	draw_shadow(game, bodyDirIndex, sX, sY, frame, inv, state.m_owner_type);

	if (state.m_owner_type == hb::shared::owner::EnergySphere)
		game.m_effect_sprites[0]->draw(sX, sY, 1, hb::shared::sprite::DrawParams::alpha_blend(0.5f));

	draw_body(game, bodyDirIndex, sX, sY, frame, inv, state.m_owner_type, state.m_status.frozen);
}

// -----------------------------------------------------------------------
bool check_invisibility(CGame& game, const CEntityRenderState& state, bool& inv, bool& admin_invis)
{
	admin_invis = false;

	if (hb::shared::owner::is_always_invisible(state.m_owner_type))
		inv = true;

	if (state.m_status.invisibility)
	{
		// Admin invisibility: invisibility + gm_mode combo means admin invis
		// Always draw with red-tinted transparency for any viewer that receives this packet
		if (state.m_status.gm_mode)
		{
			inv = true;
			admin_invis = true;
			return false; // draw with admin invis transparency
		}

		if (state.m_object_id == game.m_player->m_player_object_id)
			inv = true;
		else if (IsFriendly(state.m_status.relationship))
			inv = true;
		else
			return true; // Don't draw at all
	}
	return false; // draw (possibly with inv transparency)
}

// -----------------------------------------------------------------------
void apply_direction_override(CEntityRenderState& state)
{
	switch (state.m_owner_type) {
	case hb::shared::owner::AirElemental:
		state.m_dir = direction::north;
		break;
	case hb::shared::owner::Gate:
		if (state.m_dir <= direction::east) state.m_dir = direction::east;
		else state.m_dir = direction::south;
		break;
	}
}

// -----------------------------------------------------------------------
void draw_name(Screen_OnGame& screen, const CEntityRenderState& state, int sX, int sY)
{
	if (state.m_name[0] != '\0')
	{
		if (state.is_player())
			screen.draw_object_name(sX, sY, state.m_name.data(), state.m_status, state.m_object_id);
		else
			screen.draw_npc_name(sX, sY, state.m_owner_type, state.m_status, state.m_npc_config_id);
	}
}

// -----------------------------------------------------------------------
void update_chat(CGame& game, const CEntityRenderState& state,
                int sX, int sY, int indexX, int indexY)
{
	if (state.m_chat_index == 0) return;

	if (game.get_floating_text().is_valid(state.m_chat_index, state.m_object_id))
	{
		game.get_floating_text().update_position(state.m_chat_index, static_cast<short>(sX), static_cast<short>(sY));
	}
	else
	{
		game.m_map_data->clear_chat_msg(indexX, indexY);
	}
}

// -----------------------------------------------------------------------
void draw_npc_light(CGame& game, short owner_type, int sX, int sY)
{
	switch (owner_type) {
	case hb::shared::owner::ShopKeeper:
	case hb::shared::owner::Gandalf:
	case hb::shared::owner::Howard:
	case hb::shared::owner::Tom:
	case hb::shared::owner::William:
	case hb::shared::owner::Kennedy:
	case hb::shared::owner::Catapult:
	case hb::shared::owner::HBT:
	case hb::shared::owner::Gail:
		game.m_effect_sprites[0]->draw(sX, sY, 1, hb::shared::sprite::DrawParams::alpha_blend(0.5f));
		break;
	}
}

// -----------------------------------------------------------------------
void draw_effect_auras(CGame& game, const CEntityRenderState& state, int sX, int sY)
{
	if (state.m_effect_type == 0) return;
	switch (state.m_effect_type) {
	case 1: game.m_effect_sprites[26]->draw(sX, sY, state.m_effect_frame, hb::shared::sprite::DrawParams::alpha_blend(0.5f)); break;
	case 2: game.m_effect_sprites[27]->draw(sX, sY, state.m_effect_frame, hb::shared::sprite::DrawParams::alpha_blend(0.5f)); break;
	}
}

// -----------------------------------------------------------------------
void draw_berserk_glow(CGame& game, const EquipmentIndices& eq, const CEntityRenderState& state,
                     int sX, int sY)
{
	if (!state.m_status.berserk) return;
	int bodyDirIndex = eq.m_body_index + (state.m_dir - 1);
	game.m_sprite[bodyDirIndex]->draw(sX, sY, state.m_frame,
		hb::shared::sprite::DrawParams::additive_colored(GameColors::BerserkGlow.r, GameColors::BerserkGlow.g, GameColors::BerserkGlow.b, 0.7f));
}

// -----------------------------------------------------------------------
void draw_abaddon_effects(CGame& game, const CEntityRenderState& state, int sX, int sY)
{
	if (state.m_owner_type != hb::shared::owner::Abaddon) return;

	int randFrame = state.m_frame % 12;
	game.m_effect_sprites[154]->draw(sX - 50, sY - 50, randFrame, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
	game.m_effect_sprites[155]->draw(sX - 20, sY - 80, randFrame, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
	game.m_effect_sprites[156]->draw(sX + 70, sY - 50, randFrame, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
	game.m_effect_sprites[157]->draw(sX - 30, sY, randFrame, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
	game.m_effect_sprites[158]->draw(sX - 60, sY + 90, randFrame, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
	game.m_effect_sprites[159]->draw(sX + 65, sY + 85, randFrame, hb::shared::sprite::DrawParams::alpha_blend(0.7f));

	int ef = state.m_effect_frame;
	switch (state.m_dir) {
	case 1:
		game.m_effect_sprites[153]->draw(sX, sY + 108, ef % 28, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
		game.m_effect_sprites[164]->draw(sX - 50, sY + 10, ef % 15, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
		break;
	case 2:
		game.m_effect_sprites[153]->draw(sX, sY + 95, ef % 28, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
		game.m_effect_sprites[164]->draw(sX - 70, sY + 10, ef % 15, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
		break;
	case 3:
		game.m_effect_sprites[153]->draw(sX, sY + 105, ef % 28, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
		game.m_effect_sprites[164]->draw(sX - 90, sY + 10, ef % 15, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
		break;
	case 4:
		game.m_effect_sprites[153]->draw(sX - 35, sY + 100, ef % 28, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
		game.m_effect_sprites[164]->draw(sX - 80, sY + 10, ef % 15, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
		break;
	case 5:
		game.m_effect_sprites[153]->draw(sX, sY + 95, ef % 28, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
		game.m_effect_sprites[164]->draw(sX - 65, sY - 5, ef % 15, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
		break;
	case 6:
		game.m_effect_sprites[153]->draw(sX + 45, sY + 95, ef % 28, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
		game.m_effect_sprites[164]->draw(sX - 31, sY + 10, ef % 15, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
		break;
	case 7:
		game.m_effect_sprites[153]->draw(sX + 40, sY + 110, ef % 28, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
		game.m_effect_sprites[164]->draw(sX - 30, sY + 10, ef % 15, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
		break;
	case 8:
		game.m_effect_sprites[153]->draw(sX + 20, sY + 110, ef % 28, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
		game.m_effect_sprites[164]->draw(sX - 20, sY + 16, ef % 15, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
		break;
	}
}

// -----------------------------------------------------------------------
void draw_gm_effect(CGame& game, const CEntityRenderState& state, int sX, int sY)
{
	if (state.m_status.gm_mode && state.is_player())
		game.m_effect_sprites[45]->draw(sX - 13, sY - 34, 0, hb::shared::sprite::DrawParams::additive(1.0f));
}

// -----------------------------------------------------------------------
void draw_afk_effect(CGame& game, const CEntityRenderState& state, int sX, int sY, uint32_t time)
{
	if (!state.is_player()) return;
	if (!state.m_status.afk) return;

	// effect9.pak sprite 19 = m_effect_sprites[66 + 19 = 85], 17 frames (0-16)
	constexpr int AFK_SPRITE_INDEX = 85;
	constexpr int AFK_MAX_FRAMES = 17;

	// Note: static frame counter is shared across all AFK entities, so they animate in sync.
	// Per-entity state would require storing frame/time in CEntityRenderState.
	static int s_iAfkFrame = 0;
	static uint32_t s_dwNextFrameTime = 0;

	if (time >= s_dwNextFrameTime)
	{
		s_iAfkFrame = (s_iAfkFrame + 1) % AFK_MAX_FRAMES;
		s_dwNextFrameTime = time + 100 + (rand() % 101); // 100-200ms
	}

	game.m_effect_sprites[AFK_SPRITE_INDEX]->draw(sX + 56, sY+32, s_iAfkFrame, hb::shared::sprite::DrawParams::alpha_blend(0.8f));
}

} // namespace RenderHelpers
