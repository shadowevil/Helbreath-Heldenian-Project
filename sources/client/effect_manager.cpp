// effect_manager.cpp: Visual effects system implementation (Orchestrator)
//
//////////////////////////////////////////////////////////////////////

#include "effect_manager.h"
#include "game_constants.h"
#include "game.h"
#include "sprite.h"
#include "effect.h"
#include "global_def.h"
#include "misc.h"
#include "config_manager.h"

// External global from game.cpp

effect_manager::effect_manager(CGame* game)
	: m_game(game)
	, m_effect_sprites(nullptr)
{
	for (int i = 0; i < game_limits::max_effects; i++)
		m_effect_list[i] = nullptr;
}

// Listo
effect_manager::~effect_manager()
{
	clear_all_effects();
}

// Listo
void effect_manager::set_effect_sprites(hb::shared::sprite::SpriteCollection& effectSpr)
{
	m_effect_sprites = &effectSpr;
}

// Listo
void effect_manager::clear_all_effects()
{
	for (int i = 0; i < game_limits::max_effects; i++)
	{
		if (m_effect_list[i] != nullptr)
		{
			delete m_effect_list[i];
			m_effect_list[i] = nullptr;
		}
	}
}

// Public API: Orchestrator methods that delegate to private implementation methods
// Implementation methods are defined in separate .cpp files:
//   - effect_add.cpp
//   - effect_update.cpp
//   - effect_draw.cpp
//   - effect_lights.cpp

void effect_manager::add_effect(EffectType type, int sX, int sY, int dX, int dY, char start_frame, int v1)
{
	add_effect_impl(type, sX, sY, dX, dY, start_frame, v1);
}

void effect_manager::update()
{
	update_effects_impl();
}

void effect_manager::draw_effects()
{
	draw_effects_impl();
}

void effect_manager::draw_effect_lights()
{
	draw_effect_lights_impl();
}

