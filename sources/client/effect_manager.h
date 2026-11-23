// effect_manager.h: Visual effects system manager
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include "sprite_collection.h"
#include "effect_type.h"
#include "game_constants.h"

// Forward declarations only - avoid including heavy headers
class CGame;
class CEffect;

class effect_manager
{
public:
	explicit effect_manager(CGame* game);
	~effect_manager();

	// Initialization
	void set_effect_sprites(hb::shared::sprite::SpriteCollection& effectSpr);

	// Main API
	void add_effect(EffectType type, int sX, int sY, int dX, int dY, char start_frame, int v1 = 1);
	void update();   // Updates effect frames, removes finished effects
	
	// render methods (separate for correct draw order: lights before objects, effects after)
	void draw_effects();     // draw particle effects (call after draw_objects)
	void draw_effect_lights(); // draw lighting effects (call before draw_objects)

	// Cleanup
	void clear_all_effects();

private:
	// Private implementation methods (defined in separate .cpp files)
	void add_effect_impl(EffectType type, int sX, int sY, int dX, int dY, char start_frame, int v1 = 0);
	void update_effects_impl();
	void draw_effects_impl();
	void draw_effect_lights_impl();

	CGame* m_game;
	CEffect* m_effect_list[game_limits::max_effects];
	hb::shared::sprite::SpriteCollection* m_effect_sprites;  // Reference, not owned
};
