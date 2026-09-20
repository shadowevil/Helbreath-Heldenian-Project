#pragma once

#include "EquipmentIndices.h"

class CGame;
class Screen_OnGame;
class CEntityRenderState;
namespace hb::shared::sprite { class SpriteCollection; }

// Drawing order arrays — indexed by direction (1-8), element [0] unused.
// weapon_draw_order: 1 = draw weapon BEFORE body, 0 = draw weapon AFTER body
// mantle_draw_order: 0 = behind body, 1 = in front, 2 = over armor
// mantle_draw_order_running: same semantics but for running animation
inline constexpr char weapon_draw_order[]          = { 0, 1, 0, 0, 0, 0, 0, 1, 1 };
inline constexpr char mantle_draw_order[]           = { 0, 1, 1, 1, 0, 0, 0, 2, 2 };
inline constexpr char mantle_draw_order_running[]   = { 0, 1, 1, 1, 1, 1, 1, 1, 1 };

namespace RenderHelpers
{
	// draw a single equipment layer (undies, armor, pants, boots, arm, helm, mantle)
	// with optional color tint. Does nothing if spriteIndex == -1.
	// sprites: the SpriteCollection to draw from (m_sprite for cosmetics, m_equip_sprites for equipment)
	// frame: usually (dir-1)*equipFrameMul + animFrame
	void draw_equip_layer(CGame& game, hb::shared::sprite::SpriteCollection& sprites, int spriteIndex, int sX, int sY, int frame,
	                    bool inv, int colorIndex);

	// draw weapon sprite with color tint + dk_glare overlay.
	// sprites: the SpriteCollection containing the weapon sprite (m_equip_sprites)
	// weaponFrame: (dir-1)*equipFrameMul + animFrame (direction now in frame, not index)
	void draw_weapon(CGame& game, hb::shared::sprite::SpriteCollection& sprites,
	                const EquipmentIndices& eq, int sX, int sY,
	                int weaponFrame, bool inv);

	// draw shield sprite with color tint + glare overlay.
	// sprites: the SpriteCollection containing the shield sprite (m_equip_sprites)
	// frame: (dir-1)*equipFrameMul + animFrame
	void draw_shield(CGame& game, hb::shared::sprite::SpriteCollection& sprites,
	                const EquipmentIndices& eq, int sX, int sY,
	                int frame, bool inv);

	// draw body shadow. Checks mob type skip list and detail level.
	// Does nothing for shadow-exempt mob types or when detail level is 0.
	void draw_shadow(CGame& game, int body_dir_index, int sX, int sY, int frame,
	                bool inv, short owner_type);

	// draw body sprite with frozen tint, invisibility alpha, or Abaddon handling.
	// Also captures the body bounding rect into game.m_body_rect.
	// admin_invis: true = render with red-tinted 50% alpha (admin invisible to higher-level GMs)
	void draw_body(CGame& game, int body_dir_index, int sX, int sY, int frame,
	              bool inv, short owner_type, bool frozen, bool admin_invis = false);

	// draw the full player equipment stack in correct z-order.
	// This is the main consolidation function that replaces ~200 lines per DrawObject function.
	// Draws: shadow, weapon/body/equipment/shield in correct order based on _cDrawingOrder[dir].
	// mantleOrder: pointer to _cMantleDrawingOrder or _cMantleDrawingOrderOnRun
	// equipFrameMul: frame multiplier for equipment layers (default 8, OnMagic uses 16, OnGetItem/OnDamage use 4)
	void draw_player_layers(CGame& game, const EquipmentIndices& eq,
	                      const CEntityRenderState& state, int sX, int sY,
	                      bool inv, const char* mantleOrder, int equipFrameMul = 8,
	                      bool admin_invis = false);

	// draw the full NPC body (just body sprite, no equipment).
	// Same shadow/body/frozen logic as players but simpler.
	void draw_npc_layers(CGame& game, const EquipmentIndices& eq,
	                   const CEntityRenderState& state, int sX, int sY,
	                   bool inv);

	// Check invisibility state. Returns true if entity should NOT be drawn at all.
	// Sets inv to true if entity should be drawn with transparency (friendly invisible, always-invisible).
	// Sets admin_invis to true if entity is admin invisible (red-tinted transparency).
	bool check_invisibility(CGame& game, const CEntityRenderState& state, bool& inv, bool& admin_invis);

	// Apply single-direction monster overrides (Air Elemental always dir 1, Gate dir 3 or 5).
	void apply_direction_override(CEntityRenderState& state);

	// draw entity name (player or NPC).
	void draw_name(Screen_OnGame& screen, const CEntityRenderState& state, int sX, int sY);

	// update chat message position or clear expired chat.
	// indexX/indexY needed for chat message clearing when message expires.
	void update_chat(CGame& game, const CEntityRenderState& state,
	                int sX, int sY, int indexX, int indexY);

	// draw NPC ground light effect (shopkeepers, guards, etc.)
	void draw_npc_light(CGame& game, short owner_type, int sX, int sY);

	// draw special ability effect auras (attack/protect effects).
	void draw_effect_auras(CGame& game, const CEntityRenderState& state, int sX, int sY);

	// draw berserk glow overlay on body sprite.
	void draw_berserk_glow(CGame& game, const EquipmentIndices& eq, const CEntityRenderState& state,
	                     int sX, int sY);

	// draw Abaddon-specific particle effects.
	void draw_abaddon_effects(CGame& game, const CEntityRenderState& state, int sX, int sY);

	// draw GM mode crown effect.
	void draw_gm_effect(CGame& game, const CEntityRenderState& state, int sX, int sY);

	// draw AFK indicator sprite above local player's head.
	void draw_afk_effect(CGame& game, const CEntityRenderState& state, int sX, int sY, uint32_t time);
}
