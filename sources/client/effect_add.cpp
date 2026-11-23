// effect_add.cpp: add_effect implementation
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
#include "audio_manager.h"


void effect_manager::add_effect_impl(EffectType type, int sX, int sY, int dX, int dY, char start_frame, int v1)
{
	int i;
	short abs_x, abs_y, dist;
	long lPan;
	if (config_manager::get().get_detail_level() == 0) // Detail Level Low
	{
		switch (type) {
		case EffectType::BURST_SMALL:
		case EffectType::BURST_MEDIUM:
		case EffectType::BURST_SMALL_GRENADE:
		case EffectType::BURST_LARGE:
		case EffectType::FOOTPRINT:
		case EffectType::RED_CLOUD_PARTICLES:
			return;
		}
	}

	int x = LOGICAL_WIDTH() / 2;
	int y = LOGICAL_HEIGHT() / 2;
	int fixx = VIEW_CENTER_TILE_X();
	int fixy = VIEW_CENTER_TILE_Y();
	abs_x = abs(((m_game->m_Camera.get_x() / 32) + fixx) - dX);
	abs_y = abs(((m_game->m_Camera.get_y() / 32) + fixy) - dY);
	if (abs_x > abs_y) dist = abs_x; else dist = abs_y;
	short fixdist = dist;
	int fixpan = LOGICAL_WIDTH();

	for (i = 0; i < game_limits::max_effects; i++)
		if (m_effect_list[i] == 0)
		{
			m_effect_list[i] = new class CEffect;
			m_effect_list[i]->m_type = type;
			m_effect_list[i]->m_x = sX;
			m_effect_list[i]->m_y = sY;
			m_effect_list[i]->m_dest_x = dX;
			m_effect_list[i]->m_dest_y = dY;
			m_effect_list[i]->m_value1 = v1;
			m_effect_list[i]->m_frame = start_frame;
			m_effect_list[i]->m_time = m_game->m_cur_time;

			switch (type) {
			case EffectType::NORMAL_HIT: // coup normal
				m_effect_list[i]->m_move_x = sX * 32;
				m_effect_list[i]->m_move_y = sY * 32 - entity_visual::attacker_height[v1];
				m_effect_list[i]->m_error = 0;
				m_effect_list[i]->m_max_frame = 2;
				m_effect_list[i]->m_frame_time = 10;
				break;

			case EffectType::ARROW_FLYING:	// Arrow Flying
				m_effect_list[i]->m_move_x = sX * 32;
				m_effect_list[i]->m_move_y = sY * 32 - entity_visual::attacker_height[v1];
				m_effect_list[i]->m_error = 0;
				m_effect_list[i]->m_max_frame = 0;
				m_effect_list[i]->m_frame_time = 10;
				m_effect_list[i]->m_dir = CMisc::calc_direction(sX, sY, dX, dY);
				audio_manager::get().play_game_sound(sound_type::character, 4, dist);
				break;

			case EffectType::GOLD_DROP: // Gold
				m_effect_list[i]->m_move_x = sX * 32;
				m_effect_list[i]->m_move_y = sY * 32;
				m_effect_list[i]->m_max_frame = 3;
				m_effect_list[i]->m_frame_time = 100;
				abs_x = abs(((m_game->m_Camera.get_x() / 32) + fixx) - sX);
				abs_y = abs(((m_game->m_Camera.get_y() / 32) + fixy) - sY);
				if (abs_x > abs_y) dist = abs_x;
				else dist = abs_y;
				lPan = -(((m_game->m_Camera.get_x() / 32) + fixx) - sX) * fixpan;
				audio_manager::get().play_game_sound(sound_type::effect, 12, dist, lPan);
				break;

			case EffectType::FIREBALL_EXPLOSION: // FireBall Fire Explosion
				m_effect_list[i]->m_move_x = sX;
				m_effect_list[i]->m_move_y = sY;
				m_effect_list[i]->m_max_frame = 11;
				m_effect_list[i]->m_frame_time = 10;
				abs_x = abs(x - (sX - m_game->m_Camera.get_x()));
				abs_y = abs(y - (sY - m_game->m_Camera.get_y()));
				if (abs_x > abs_y) dist = abs_x;
				else dist = abs_y;
				dist = dist / 32;
				lPan = -(((m_game->m_Camera.get_x() / 32) + fixx) - sX) * fixpan;
				audio_manager::get().play_game_sound(sound_type::effect, 4, dist, lPan);
				m_game->set_camera_shaking_effect(dist);
				break;

			case EffectType::ENERGY_BOLT_EXPLOSION:	 // Energy Bolt
			case EffectType::LIGHTNING_ARROW_EXPLOSION: // Lightning Arrow
				m_effect_list[i]->m_move_x = sX;
				m_effect_list[i]->m_move_y = sY;
				m_effect_list[i]->m_max_frame = 14;
				m_effect_list[i]->m_frame_time = 10;
				abs_x = abs(x - (sX - m_game->m_Camera.get_x()));
				abs_y = abs(y - (sY - m_game->m_Camera.get_y()));
				if (abs_x > abs_y) dist = abs_x;
				else dist = abs_y;
				dist = dist / 32;
				lPan = -(((m_game->m_Camera.get_x() / 32) + fixx) - sX) * fixpan;
				audio_manager::get().play_game_sound(sound_type::effect, 2, dist, lPan);
				m_game->set_camera_shaking_effect(dist);
				break;

			case EffectType::MAGIC_MISSILE_EXPLOSION: // Magic Missile Explosion
				m_effect_list[i]->m_move_x = sX;
				m_effect_list[i]->m_move_y = sY;
				m_effect_list[i]->m_max_frame = 5;
				m_effect_list[i]->m_frame_time = 50;
				abs_x = abs(x - (sX - m_game->m_Camera.get_x()));
				abs_y = abs(y - (sY - m_game->m_Camera.get_y()));
				if (abs_x > abs_y) dist = abs_x;
				else dist = abs_y;
				dist = dist / 32;
				lPan = -(((m_game->m_Camera.get_x() / 32) + fixx) - sX) * fixpan;
				audio_manager::get().play_game_sound(sound_type::effect, 3, dist, lPan);
				break;

			case EffectType::BURST_SMALL: // Burst
				m_effect_list[i]->m_move_x = sX;
				m_effect_list[i]->m_move_y = sY;
				m_effect_list[i]->m_max_frame = 4;
				m_effect_list[i]->m_frame_time = 30;
				break;

			case EffectType::BURST_MEDIUM: // Burst
				m_effect_list[i]->m_move_x = sX;
				m_effect_list[i]->m_move_y = sY;
				m_effect_list[i]->m_render_x = 6 - (rand() % 12);
				m_effect_list[i]->m_render_y = -8 - (rand() % 6);
				m_effect_list[i]->m_max_frame = 14;
				m_effect_list[i]->m_frame_time = 30;
				break;

			case EffectType::BURST_SMALL_GRENADE:
				m_effect_list[i]->m_move_x = sX;
				m_effect_list[i]->m_move_y = sY;
				m_effect_list[i]->m_render_x = 6 - (rand() % 12);
				m_effect_list[i]->m_render_y = -2 - (rand() % 4);
				m_effect_list[i]->m_max_frame = 8;
				m_effect_list[i]->m_frame_time = 30;
				break;

			case EffectType::BURST_LARGE: // Burst
				m_effect_list[i]->m_move_x = sX;
				m_effect_list[i]->m_move_y = sY;
				m_effect_list[i]->m_render_x = 8 - (rand() % 16);
				m_effect_list[i]->m_render_y = 4 - (rand() % 12);
				m_effect_list[i]->m_max_frame = 10;
				m_effect_list[i]->m_frame_time = 30;
				break;

			case EffectType::BUBBLES_DRUNK: // Bulles druncncity
				m_effect_list[i]->m_move_x = sX;
				m_effect_list[i]->m_move_y = sY;
				m_effect_list[i]->m_max_frame = 18;
				m_effect_list[i]->m_frame_time = 20;
				break;

			case EffectType::FOOTPRINT: // Traces of pas ou Tremor (pas en low detail)
				m_effect_list[i]->m_move_x = sX;
			if (m_effect_list[i]->m_value1 > 0) // Case if hit by an arrow
			{
				m_effect_list[i]->m_move_y = sY - (entity_visual::attacker_height[m_effect_list[i]->m_value1] / 4 + rand() % (entity_visual::attacker_height[m_effect_list[i]->m_value1] / 2));
				m_effect_list[i]->m_move_x = sX + (rand() % 5) - 2;
			}
			else m_effect_list[i]->m_move_y = sY;
				m_effect_list[i]->m_max_frame = 4;
				m_effect_list[i]->m_frame_time = 100;
				m_effect_list[i]->m_value1 = v1;
				break;

			case EffectType::RED_CLOUD_PARTICLES: //
				m_effect_list[i]->m_move_x = sX;
				m_effect_list[i]->m_move_y = sY;
				m_effect_list[i]->m_max_frame = 16;
				m_effect_list[i]->m_frame_time = 80;
				break;

			case EffectType::PROJECTILE_GENERIC: //
				m_effect_list[i]->m_move_x = sX * 32;
				m_effect_list[i]->m_move_y = sY * 32 - 40;
				m_effect_list[i]->m_error = 0;
				m_effect_list[i]->m_max_frame = 0;
				m_effect_list[i]->m_frame_time = 20;
				break;

			case EffectType::ICE_STORM:
				m_effect_list[i]->m_move_x = sX + (rand() % 20) - 40;
				m_effect_list[i]->m_move_y = sY + (rand() % 20) - 40;
				m_effect_list[i]->m_render_x = 8 - (rand() % 16);
				m_effect_list[i]->m_render_y = 4 - (rand() % 12);
				m_effect_list[i]->m_move_x3 = sX;
				m_effect_list[i]->m_move_y3 = sY;
				m_effect_list[i]->m_value1 = 0;
				m_effect_list[i]->m_frame_time = 20;
				break;

			case EffectType::IMPACT_BURST:
				m_effect_list[i]->m_move_x = sX;
				m_effect_list[i]->m_move_y = sY;
				m_effect_list[i]->m_max_frame = 10;
				m_effect_list[i]->m_frame_time = 50;
				abs_x = abs(x - (sX - m_game->m_Camera.get_x()));
				abs_y = abs(y - (sY - m_game->m_Camera.get_y()));
				if (abs_x > abs_y) dist = abs_x;
				else dist = abs_y;
				dist = dist / 32;
				m_game->set_camera_shaking_effect(dist);
				break;

			case EffectType::CRITICAL_STRIKE_1:
			case EffectType::CRITICAL_STRIKE_2:
			case EffectType::CRITICAL_STRIKE_3:
			case EffectType::CRITICAL_STRIKE_4:
			case EffectType::CRITICAL_STRIKE_5:
			case EffectType::CRITICAL_STRIKE_6:
			case EffectType::CRITICAL_STRIKE_7:
			case EffectType::CRITICAL_STRIKE_8: // Critical strike with a weapon
				m_effect_list[i]->m_move_x = sX * 32;
				m_effect_list[i]->m_move_y = sY * 32 - 40;
				m_effect_list[i]->m_error = 0;
				m_effect_list[i]->m_max_frame = 0;
				m_effect_list[i]->m_frame_time = 10;
				m_effect_list[i]->m_dir = CMisc::calc_direction(sX, sY, dX, dY);
				break;

			case EffectType::MASS_FIRE_STRIKE_CALLER1: // Mass-Fire-Strike (called 1 time)
				m_effect_list[i]->m_move_x = sX;
				m_effect_list[i]->m_move_y = sY;
				m_effect_list[i]->m_max_frame = 9;
				m_effect_list[i]->m_frame_time = 40;
				abs_x = abs(x - (sX - m_game->m_Camera.get_x()));
				abs_y = abs(y - (sY - m_game->m_Camera.get_y()));
				if (abs_x > abs_y) dist = abs_x;
				else dist = abs_y;
				dist = dist / 32;
				lPan = -(((m_game->m_Camera.get_x() / 32) + fixx) - sX) * fixpan;
				audio_manager::get().play_game_sound(sound_type::effect, 4, dist, lPan);
				m_game->set_camera_shaking_effect(dist * 2);
				break;

			case EffectType::MASS_FIRE_STRIKE_CALLER3: // Mass-Fire-Strike (called 3 times)
			case EffectType::SALMON_BURST_IMPACT: //
				m_effect_list[i]->m_move_x = sX;
				m_effect_list[i]->m_move_y = sY;
				m_effect_list[i]->m_max_frame = 8;
				m_effect_list[i]->m_frame_time = 40;
				abs_x = abs(x - (sX - m_game->m_Camera.get_x()));
				abs_y = abs(y - (sY - m_game->m_Camera.get_y()));
				if (abs_x > abs_y) dist = abs_x;
				else dist = abs_y;
				dist = dist / 32;
				lPan = -(((m_game->m_Camera.get_x() / 32) + fixx) - sX) * fixpan;
				audio_manager::get().play_game_sound(sound_type::effect, 4, dist, lPan);
				m_game->set_camera_shaking_effect(dist);
				break;

			case EffectType::FOOTPRINT_RAIN: //
				m_effect_list[i]->m_move_x = sX;
				m_effect_list[i]->m_move_y = sY;
				m_effect_list[i]->m_error = 0;
				m_effect_list[i]->m_max_frame = 4;
				m_effect_list[i]->m_frame_time = 100;
				break;

			case EffectType::IMPACT_EFFECT: //
				m_effect_list[i]->m_move_x = sX;
				m_effect_list[i]->m_move_y = sY;
				m_effect_list[i]->m_max_frame = 16;
				m_effect_list[i]->m_frame_time = 10;
				break;

			case EffectType::BLOODY_SHOCK_STRIKE: //
				m_effect_list[i]->m_move_x = sX * 32;
				m_effect_list[i]->m_move_y = sY * 32 - 40;
				m_effect_list[i]->m_error = 0;
				m_effect_list[i]->m_max_frame = 0;
				m_effect_list[i]->m_frame_time = 20;
				m_game->set_camera_shaking_effect(dist);
				break;

			case EffectType::MASS_MAGIC_MISSILE_AURA1: // Snoopy: Added for Mass Magic-Missile
				m_effect_list[i]->m_move_x = sX;
				m_effect_list[i]->m_move_y = sY;
				m_effect_list[i]->m_max_frame = 18;
				m_effect_list[i]->m_frame_time = 40;
				abs_x = abs(x - (sX - m_game->m_Camera.get_x()));
				abs_y = abs(y - (sY - m_game->m_Camera.get_y()));
				if (abs_x > abs_y) dist = abs_x;
				else dist = abs_y;
				dist = dist / 32;
				lPan = -(((m_game->m_Camera.get_x() / 32) + fixx) - sX) * fixpan;
				audio_manager::get().play_game_sound(sound_type::effect, 4, dist, lPan);
				m_game->set_camera_shaking_effect(dist * 2);
				break;

			case EffectType::MASS_MAGIC_MISSILE_AURA2: // Snoopy: Added for Mass Magic-Missile
				m_effect_list[i]->m_move_x = sX;
				m_effect_list[i]->m_move_y = sY;
				m_effect_list[i]->m_max_frame = 15;
				m_effect_list[i]->m_frame_time = 40;
				abs_x = abs(x - (sX - m_game->m_Camera.get_x()));
				abs_y = abs(y - (sY - m_game->m_Camera.get_y()));
				if (abs_x > abs_y) dist = abs_x;
				else dist = abs_y;
				dist = dist / 32;
				lPan = -(((m_game->m_Camera.get_x() / 32) + fixx) - sX) * fixpan;
				audio_manager::get().play_game_sound(sound_type::effect, 4, dist, lPan);
				m_game->set_camera_shaking_effect(dist);
				break;

			case EffectType::CHILL_WIND_IMPACT: //
				m_effect_list[i]->m_move_x = sX;
				m_effect_list[i]->m_move_y = sY;
				m_effect_list[i]->m_max_frame = 15;
				m_effect_list[i]->m_frame_time = 30;
				abs_x = abs(x - (sX - m_game->m_Camera.get_x()));
				abs_y = abs(y - (sY - m_game->m_Camera.get_y()));
				if (abs_x > abs_y) dist = abs_x;
				else dist = abs_y;
				dist = dist / 32;
				lPan = ((sX - m_game->m_Camera.get_x()) - x) * 30;
				audio_manager::get().play_game_sound(sound_type::effect, 45, dist, lPan);
				break;

			case EffectType::ICE_STRIKE_VARIANT_1: // Large Type 1, 2, 3, 4
			case EffectType::ICE_STRIKE_VARIANT_2:
			case EffectType::ICE_STRIKE_VARIANT_3:
			case EffectType::ICE_STRIKE_VARIANT_4:
			case EffectType::ICE_STRIKE_VARIANT_5: // Small Type 1, 2
			case EffectType::ICE_STRIKE_VARIANT_6:
				m_effect_list[i]->m_move_x = sX;
				m_effect_list[i]->m_move_y = sY - 220;
				m_effect_list[i]->m_max_frame = 14;
				m_effect_list[i]->m_frame_time = 20;
				m_effect_list[i]->m_value1 = 20;
				abs_x = abs(x - (sX - m_game->m_Camera.get_x()));
				abs_y = abs(y - (sY - m_game->m_Camera.get_y()));
				if (abs_x > abs_y) dist = abs_x;
				else dist = abs_y;
				dist = dist / 32;
				lPan = ((sX - m_game->m_Camera.get_x()) - x) * 30;
				audio_manager::get().play_game_sound(sound_type::effect, 46, dist, lPan);
				break;

			case EffectType::BLIZZARD_VARIANT_1: // Blizzard
			case EffectType::BLIZZARD_VARIANT_2: // Blizzard
			case EffectType::BLIZZARD_VARIANT_3: // Blizzard
				m_effect_list[i]->m_move_x = sX;
				m_effect_list[i]->m_move_y = sY - 220;
				m_effect_list[i]->m_max_frame = 12;
				m_effect_list[i]->m_frame_time = 20;
				m_effect_list[i]->m_value1 = 20;
				abs_x = abs(x - (sX - m_game->m_Camera.get_x()));
				abs_y = abs(y - (sY - m_game->m_Camera.get_y()));
				if (abs_x > abs_y) dist = abs_x;
				else dist = abs_y;
				dist = dist / 32;
				lPan = ((sX - m_game->m_Camera.get_x()) - x) * 30;
				audio_manager::get().play_game_sound(sound_type::effect, 46, dist, lPan);
				break;

			case EffectType::SMOKE_DUST: //
				m_effect_list[i]->m_move_x = sX;
				m_effect_list[i]->m_move_y = sY;
				m_effect_list[i]->m_max_frame = 12;
				m_effect_list[i]->m_frame_time = 50;
				abs_x = abs(x - (sX - m_game->m_Camera.get_x()));
				abs_y = abs(y - (sY - m_game->m_Camera.get_y()));
				if (abs_x > abs_y) dist = abs_x;
				else dist = abs_y;
				dist = dist / 32;
				lPan = ((sX - m_game->m_Camera.get_x()) - x) * 30;
				if ((rand() % 4) == 1) m_game->set_camera_shaking_effect(dist);
				audio_manager::get().play_game_sound(sound_type::effect, 47, dist, lPan);
				break;

			case EffectType::SPARKLE_SMALL:
				m_effect_list[i]->m_move_x = sX;
				m_effect_list[i]->m_move_y = sY;
				m_effect_list[i]->m_max_frame = 9;
				m_effect_list[i]->m_frame_time = 80;
				break;

			case EffectType::PROTECTION_RING: // Protect ring
				m_effect_list[i]->m_move_x = sX;
				m_effect_list[i]->m_move_y = sY;
				m_effect_list[i]->m_max_frame = 15;
				m_effect_list[i]->m_frame_time = 80;
				abs_x = abs(x - (sX - m_game->m_Camera.get_x()));
				abs_y = abs(y - (sY - m_game->m_Camera.get_y()));
				if (abs_x > abs_y) dist = abs_x;
				else dist = abs_y;
				dist = dist / 32;
				lPan = ((sX - m_game->m_Camera.get_x()) - x) * 30;
				audio_manager::get().play_game_sound(sound_type::effect, 5, dist, lPan);
				break;

			case EffectType::HOLD_TWIST: // Hold twist
				m_effect_list[i]->m_move_x = sX;
				m_effect_list[i]->m_move_y = sY;
				m_effect_list[i]->m_max_frame = 15;
				m_effect_list[i]->m_frame_time = 80;
				abs_x = abs(x - (sX - m_game->m_Camera.get_x()));
				abs_y = abs(y - (sY - m_game->m_Camera.get_y()));
				if (abs_x > abs_y) dist = abs_x;
				else dist = abs_y;
				dist = dist / 32;
				lPan = ((sX - m_game->m_Camera.get_x()) - x) * 30;
				audio_manager::get().play_game_sound(sound_type::effect, 5, dist, lPan);
				break;

			case EffectType::STAR_TWINKLE: // star twingkling (effect armes brillantes)
			case EffectType::UNUSED_55: // Unused
				m_effect_list[i]->m_move_x = sX;
				m_effect_list[i]->m_move_y = sY;
				m_effect_list[i]->m_max_frame = 10;
				m_effect_list[i]->m_frame_time = 15;
				break;

			case EffectType::MASS_CHILL_WIND: //  Mass-Chill-Wind
				m_effect_list[i]->m_move_x = sX;
				m_effect_list[i]->m_move_y = sY;
				m_effect_list[i]->m_max_frame = 14;
				m_effect_list[i]->m_frame_time = 30;
				abs_x = abs(x - (sX - m_game->m_Camera.get_x()));
				abs_y = abs(y - (sY - m_game->m_Camera.get_y()));
				if (abs_x > abs_y) dist = abs_x;
				else dist = abs_y;
				dist = dist / 32;
				lPan = ((sX - m_game->m_Camera.get_x()) - x) * 30;
				audio_manager::get().play_game_sound(sound_type::effect, 45, dist, lPan);
				break;

			case EffectType::BUFF_EFFECT_LIGHT: //
				m_effect_list[i]->m_move_x = sX;
				m_effect_list[i]->m_move_y = sY;
				m_effect_list[i]->m_max_frame = 16;
				m_effect_list[i]->m_frame_time = 80;
				break;

			case EffectType::METEOR_FLYING: //
				m_effect_list[i]->m_move_x = sX + 300;
				m_effect_list[i]->m_move_y = sY - 460;
				m_effect_list[i]->m_max_frame = 10;
				m_effect_list[i]->m_frame_time = 50;
				break;

			case EffectType::FIRE_AURA_GROUND: //
				m_effect_list[i]->m_move_x = sX;
				m_effect_list[i]->m_move_y = sY;
				m_effect_list[i]->m_max_frame = 16;
				m_effect_list[i]->m_frame_time = 10;
				abs_x = abs(x - (sX - m_game->m_Camera.get_x()));
				abs_y = abs(y - (sY - m_game->m_Camera.get_y()));
				if (abs_x > abs_y) dist = abs_x;
				else dist = abs_y;
				dist = dist / 32;
				lPan = -(((m_game->m_Camera.get_x() / 32) + fixx) - sX) * fixpan;
				audio_manager::get().play_game_sound(sound_type::effect, 4, dist, lPan);
				m_game->set_camera_shaking_effect(dist, 2);
				break;

			case EffectType::METEOR_IMPACT: //
				m_effect_list[i]->m_move_x = sX;
				m_effect_list[i]->m_move_y = sY;
				m_effect_list[i]->m_max_frame = 6;
				m_effect_list[i]->m_frame_time = 100;
				break;

			case EffectType::FIRE_EXPLOSION_CRUSADE: //
				m_effect_list[i]->m_move_x = sX;
				m_effect_list[i]->m_move_y = sY;
				m_effect_list[i]->m_max_frame = 16;
				m_effect_list[i]->m_frame_time = 20;
				break;

			case EffectType::WHITE_HALO: //
				m_effect_list[i]->m_move_x = sX;
				m_effect_list[i]->m_move_y = sY;
				m_effect_list[i]->m_max_frame = 15;
				m_effect_list[i]->m_frame_time = 20;
				break;

			case EffectType::MS_CRUSADE_CASTING: // Crusade's MS
				m_effect_list[i]->m_move_x = sX;
				m_effect_list[i]->m_move_y = sY;
				m_effect_list[i]->m_max_frame = 30;
				m_effect_list[i]->m_frame_time = 80;
				break;

			case EffectType::MS_CRUSADE_EXPLOSION: // Crusade MS explosion
				m_effect_list[i]->m_move_x = sX;
				m_effect_list[i]->m_move_y = sY;
				m_effect_list[i]->m_max_frame = 14;
				m_effect_list[i]->m_frame_time = 30;
				abs_x = abs(x - (sX - m_game->m_Camera.get_x()));
				abs_y = abs(y - (sY - m_game->m_Camera.get_y()));
				if (abs_x > abs_y) dist = abs_x;
				else dist = abs_y;
				dist = dist / 32;
				lPan = -(((m_game->m_Camera.get_x() / 32) + fixx) - sX) * fixpan;
				audio_manager::get().play_game_sound(sound_type::effect, 4, dist, lPan);
				m_game->set_camera_shaking_effect(dist, 2);
				break;

			case EffectType::MS_FIRE_SMOKE: // Crusade's MS fire + smoke ?
				m_effect_list[i]->m_move_x = sX;
				m_effect_list[i]->m_move_y = sY;
				m_effect_list[i]->m_max_frame = 27;
				m_effect_list[i]->m_frame_time = 10;
				break;

			case EffectType::WORM_BITE: // worm-bite
				m_effect_list[i]->m_move_x = sX;
				m_effect_list[i]->m_move_y = sY;
				m_effect_list[i]->m_max_frame = 17;
				m_effect_list[i]->m_frame_time = 30;
				abs_x = abs(x - (sX - m_game->m_Camera.get_x()));
				abs_y = abs(y - (sY - m_game->m_Camera.get_y()));
				if (abs_x > abs_y) dist = abs_x;
				else dist = abs_y;
				dist = dist / 32;
				lPan = -(((m_game->m_Camera.get_x() / 32) + fixx) - sX) * fixpan;
				audio_manager::get().play_game_sound(sound_type::effect, 4, dist, lPan);
				m_effect_list[i]->m_value1 = dist;
				break;

			case EffectType::LIGHT_EFFECT_1: // identique au cas 70
				m_effect_list[i]->m_move_x = sX;
				m_effect_list[i]->m_move_y = sY;
				m_effect_list[i]->m_max_frame = 11;
				m_effect_list[i]->m_frame_time = 30;
				abs_x = abs(x - (sX - m_game->m_Camera.get_x()));
				abs_y = abs(y - (sY - m_game->m_Camera.get_y()));
				if (abs_x > abs_y) dist = abs_x;
				else dist = abs_y;
				dist = dist / 32;
				lPan = -(((m_game->m_Camera.get_x() / 32) + fixx) - sX) * fixpan;
				audio_manager::get().play_game_sound(sound_type::effect, 42, dist, lPan);
				break;

			case EffectType::LIGHT_EFFECT_2: // identtique au cas 69
				m_effect_list[i]->m_move_x = sX;
				m_effect_list[i]->m_move_y = sY;
				m_effect_list[i]->m_max_frame = 11;
				m_effect_list[i]->m_frame_time = 30;
				abs_x = abs(x - (sX - m_game->m_Camera.get_x()));
				abs_y = abs(y - (sY - m_game->m_Camera.get_y()));
				if (abs_x > abs_y) dist = abs_x;
				else dist = abs_y;
				dist = dist / 32;
				lPan = -(((m_game->m_Camera.get_x() / 32) + fixx) - sX) * fixpan;
				audio_manager::get().play_game_sound(sound_type::effect, 42, dist, lPan);
				break;

			case EffectType::BLIZZARD_PROJECTILE: //
				m_effect_list[i]->m_move_x = sX * 32;
				m_effect_list[i]->m_move_y = sY * 32;
				m_effect_list[i]->m_error = 0;
				m_effect_list[i]->m_max_frame = 0;
				m_effect_list[i]->m_frame_time = 20;
				m_game->set_camera_shaking_effect(dist);
				break;

			case EffectType::BLIZZARD_IMPACT: // Blizzard
				m_effect_list[i]->m_move_x = sX;
				m_effect_list[i]->m_move_y = sY;
				m_effect_list[i]->m_max_frame = 15;
				m_effect_list[i]->m_frame_time = 20;
				abs_x = abs(x - (sX - m_game->m_Camera.get_x()));
				abs_y = abs(y - (sY - m_game->m_Camera.get_y()));
				if (abs_x > abs_y) dist = abs_x;
				else dist = abs_y;
				dist = dist / 32;
				lPan = ((sX - m_game->m_Camera.get_x()) - x) * 30;
				if ((rand() % 4) == 1) m_game->set_camera_shaking_effect(dist);
				audio_manager::get().play_game_sound(sound_type::effect, 47, dist, lPan);
				break;

			case EffectType::AURA_EFFECT_1:
				m_effect_list[i]->m_move_x = sX;
				m_effect_list[i]->m_move_y = sY;
				m_effect_list[i]->m_max_frame = 15;
				m_effect_list[i]->m_frame_time = 60;
				break;

			case EffectType::AURA_EFFECT_2:
				m_effect_list[i]->m_move_x = sX;
				m_effect_list[i]->m_move_y = sY;
				m_effect_list[i]->m_max_frame = 19;
				m_effect_list[i]->m_frame_time = 40;
				break;

			case EffectType::ICE_GOLEM_EFFECT_1: //ice golem
				m_effect_list[i]->m_move_x = sX;
				m_effect_list[i]->m_move_y = sY;
				m_effect_list[i]->m_dest_x = dX;
				m_effect_list[i]->m_dest_y = dY;
				m_effect_list[i]->m_max_frame = 16;
				m_effect_list[i]->m_frame_time = 40;
				break;

			case EffectType::ICE_GOLEM_EFFECT_2: //ice golem
				m_effect_list[i]->m_move_x = sX;
				m_effect_list[i]->m_move_y = sY;
				m_effect_list[i]->m_dest_x = dX;
				m_effect_list[i]->m_dest_y = dY;
				m_effect_list[i]->m_max_frame = 16;
				m_effect_list[i]->m_frame_time = 40;
				break;

			case EffectType::ICE_GOLEM_EFFECT_3: //ice golem
				m_effect_list[i]->m_move_x = sX;
				m_effect_list[i]->m_move_y = sY;
				m_effect_list[i]->m_dest_x = dX;
				m_effect_list[i]->m_dest_y = dY;
				m_effect_list[i]->m_max_frame = 16;
				m_effect_list[i]->m_frame_time = 40;
				break;

			case EffectType::EARTH_SHOCK_WAVE_PARTICLE: // Snoopy: Added, implemented last in v351
				m_effect_list[i]->m_move_x = sX;
				m_effect_list[i]->m_move_y = sY;
				m_effect_list[i]->m_value1 = 20;
				m_effect_list[i]->m_max_frame = 30;
				m_effect_list[i]->m_frame_time = 25;
				abs_x = abs(x - (sX - m_game->m_Camera.get_x()));
				abs_y = abs(y - (sY - m_game->m_Camera.get_y()));
				if (abs_x > abs_y) dist = abs_x;
				else dist = abs_y;
				dist = dist / 32;
				m_game->set_camera_shaking_effect(dist);
				break;

			case EffectType::STORM_BLADE: //  Snoopy: Added (StormBlade)
				m_effect_list[i]->m_move_x = sX * 32;
				m_effect_list[i]->m_move_y = sY * 32;
				m_effect_list[i]->m_error = 0;
				m_effect_list[i]->m_max_frame = 27;
				m_effect_list[i]->m_frame_time = 40;
				break;

			case EffectType::GATE_APOCALYPSE: //  Snoopy: Added (Gate Apocalypse)
				m_effect_list[i]->m_max_frame = 30;
				m_effect_list[i]->m_frame_time = 40;
				break;

			case EffectType::MAGIC_MISSILE_FLYING: // MagicMissile is Flying
				m_effect_list[i]->m_move_x = sX * 32;
				m_effect_list[i]->m_move_y = sY * 32 - 40;
				m_effect_list[i]->m_error = 0;
				m_effect_list[i]->m_max_frame = 0;
				m_effect_list[i]->m_frame_time = 20;
				lPan = -(((m_game->m_Camera.get_x() / 32) + fixx) - sX) * fixpan;
				audio_manager::get().play_game_sound(sound_type::effect, 1, dist, lPan);
				break;

			case EffectType::HEAL: // Heal
			case EffectType::STAMINA_DRAIN: // Staminar-Drain
			case EffectType::GREAT_HEAL: // Great Heal
			case EffectType::STAMINA_RECOVERY: // Staminar-Recovery
			case EffectType::GREAT_STAMINA_RECOVERY: // Great-Staminar-Recovery
				m_effect_list[i]->m_max_frame = 14;
				m_effect_list[i]->m_frame_time = 80;
				lPan = -(((m_game->m_Camera.get_x() / 32) + fixx) - sX) * fixpan;
				audio_manager::get().play_game_sound(sound_type::effect, 5, dist, lPan);
				break;

			case EffectType::CREATE_FOOD: // CreateFood
			case EffectType::UNUSED_122: // Recall
			case EffectType::POSSESSION: // Possession
			case EffectType::POISON: // Poison
			case EffectType::DETECT_INVISIBILITY: // DetectInvi
			case EffectType::CURE: // Cure
			case EffectType::CONFUSE_LANGUAGE: // Confuse language
			case EffectType::POLYMORPH: // Polymorph
			case EffectType::MASS_POISON: // Mass-Poison
			case EffectType::CONFUSION: // Confusion
			case EffectType::MASS_CONFUSION: // Mass-Confusion
				m_effect_list[i]->m_max_frame = 13;
				m_effect_list[i]->m_frame_time = 120;
				lPan = -(((m_game->m_Camera.get_x() / 32) + fixx) - sX) * fixpan;
				audio_manager::get().play_game_sound(sound_type::effect, 5, dist, lPan);
				break;

			case EffectType::ENERGY_BOLT_FLYING: // Energy-Bolt
				m_effect_list[i]->m_move_x = sX * 32;
				m_effect_list[i]->m_move_y = sY * 32 - 40;
				m_effect_list[i]->m_error = 0;
				m_effect_list[i]->m_max_frame = 0;
				m_effect_list[i]->m_frame_time = 20;
				lPan = -(((m_game->m_Camera.get_x() / 32) + fixx) - sX) * fixpan;
				audio_manager::get().play_game_sound(sound_type::effect, 1, dist, lPan);
				break;

			case EffectType::RECALL: // Recall
			case EffectType::SUMMON_CREATURE: // Summon
			case EffectType::INVISIBILITY: // Invi
			case EffectType::HASTE: // Haste
				m_effect_list[i]->m_max_frame = 12;
				m_effect_list[i]->m_frame_time = 80;
				lPan = -(((m_game->m_Camera.get_x() / 32) + fixx) - sX) * fixpan;
				audio_manager::get().play_game_sound(sound_type::effect, 5, dist, lPan);
				break;

			case EffectType::DEFENSE_SHIELD: // Defense-Shield
			case EffectType::GREAT_DEFENSE_SHIELD: // Great-Defense-Shield
				m_effect_list[i]->m_max_frame = 12;
				m_effect_list[i]->m_frame_time = 120;
				lPan = -(((m_game->m_Camera.get_x() / 32) + fixx) - sX) * fixpan;
				audio_manager::get().play_game_sound(sound_type::effect, 5, dist, lPan);
				break;

			case EffectType::CELEBRATING_LIGHT: // Celebrating Light
				add_effect_impl(static_cast<EffectType>(69 + (rand() % 2)), dX * 32 + 20 - (rand() % 40), dY * 32 + 20 - (rand() % 40), 0, 0, -12, 0);
				add_effect_impl(static_cast<EffectType>(69 + (rand() % 2)), dX * 32 + 20 - (rand() % 40), dY * 32 + 20 - (rand() % 40), 0, 0, -9, 0);
				add_effect_impl(static_cast<EffectType>(69 + (rand() % 2)), dX * 32 + 20 - (rand() % 40), dY * 32 + 20 - (rand() % 40), 0, 0, -6, 0);
				add_effect_impl(static_cast<EffectType>(69 + (rand() % 2)), dX * 32 + 20 - (rand() % 40), dY * 32 + 20 - (rand() % 40), 0, 0, -3, 0);
				add_effect_impl(static_cast<EffectType>(69 + (rand() % 2)), dX * 32 + 20 - (rand() % 40), dY * 32 + 20 - (rand() % 40), 0, 0, 0, 0);
				break;

			case EffectType::FIRE_BALL_FLYING: // Fire Ball
				m_effect_list[i]->m_move_x = sX * 32;
				m_effect_list[i]->m_move_y = sY * 32 - 40;
				m_effect_list[i]->m_error = 0;
				m_effect_list[i]->m_max_frame = 0;
				m_effect_list[i]->m_frame_time = 20;
				m_effect_list[i]->m_dir = CMisc::calc_direction(sX, sY, dX, dY);
				lPan = -(((m_game->m_Camera.get_x() / 32) + fixx) - sX) * fixpan;
				audio_manager::get().play_game_sound(sound_type::effect, 1, dist, lPan);
				break;

			case EffectType::PROTECT_FROM_NM: // Protect form N.M
			case EffectType::PROTECT_FROM_MAGIC: // Protection from Magic
				add_effect_impl( EffectType::PROTECTION_RING, dX * 32, dY * 32, 0, 0, 0, 0);
				break;

			case EffectType::HOLD_PERSON: // Hold Person
			case EffectType::PARALYZE: // Paralyze
				add_effect_impl( EffectType::HOLD_TWIST, dX * 32, dY * 32, 0, 0, 0, 0);
				break;

			case EffectType::FIRE_STRIKE_FLYING: // Fire Strike
			case EffectType::LIGHTNING_ARROW_FLYING: // Lightning Arrow
				m_effect_list[i]->m_move_x = sX * 32;
				m_effect_list[i]->m_move_y = sY * 32 - 40;
				m_effect_list[i]->m_error = 0;
				m_effect_list[i]->m_max_frame = 0;
				m_effect_list[i]->m_frame_time = 20;
				m_effect_list[i]->m_dir = CMisc::calc_direction(sX, sY, dX, dY);
				lPan = -(((m_game->m_Camera.get_x() / 32) + fixx) - sX) * fixpan;
				audio_manager::get().play_game_sound(sound_type::effect, 1, dist, lPan);
				break;

			case EffectType::TREMOR: // Tremor.
				lPan = -(((m_game->m_Camera.get_x() / 32) + fixx) - sX) * fixpan;
				audio_manager::get().play_game_sound(sound_type::effect, 4, dist, lPan);
				m_game->set_camera_shaking_effect(dist, 2);
				add_effect_impl( EffectType::FOOTPRINT, dX * 32 + (rand() % 120) - 60, dY * 32 + (rand() % 80) - 40, 0, 0, 0, 0);
				add_effect_impl( EffectType::FOOTPRINT, dX * 32 + (rand() % 120) - 60, dY * 32 + (rand() % 80) - 40, 0, 0, 0, 0);
				add_effect_impl( EffectType::FOOTPRINT, dX * 32 + (rand() % 120) - 60, dY * 32 + (rand() % 80) - 40, 0, 0, 0, 0);
				add_effect_impl( EffectType::FOOTPRINT, dX * 32 + (rand() % 120) - 60, dY * 32 + (rand() % 80) - 40, 0, 0, 0, 0);
				add_effect_impl( EffectType::FOOTPRINT, dX * 32 + (rand() % 120) - 60, dY * 32 + (rand() % 80) - 40, 0, 0, 0, 0);
				add_effect_impl( EffectType::FOOTPRINT, dX * 32 + (rand() % 120) - 60, dY * 32 + (rand() % 80) - 40, 0, 0, 0, 0);
				add_effect_impl( EffectType::FOOTPRINT, dX * 32 + (rand() % 120) - 60, dY * 32 + (rand() % 80) - 40, 0, 0, 0, 0);

				add_effect_impl( EffectType::FOOTPRINT, dX * 32 + (rand() % 120) - 60, dY * 32 + (rand() % 80) - 40, 0, 0, 0, 0);
				add_effect_impl( EffectType::FOOTPRINT, dX * 32 + (rand() % 120) - 60, dY * 32 + (rand() % 80) - 40, 0, 0, 0, 0);
				add_effect_impl( EffectType::FOOTPRINT, dX * 32 + (rand() % 120) - 60, dY * 32 + (rand() % 80) - 40, 0, 0, 0, 0);
				add_effect_impl( EffectType::FOOTPRINT, dX * 32 + (rand() % 120) - 60, dY * 32 + (rand() % 80) - 40, 0, 0, 0, 0);
				add_effect_impl( EffectType::FOOTPRINT, dX * 32 + (rand() % 120) - 60, dY * 32 + (rand() % 80) - 40, 0, 0, 0, 0);
				add_effect_impl( EffectType::FOOTPRINT, dX * 32 + (rand() % 120) - 60, dY * 32 + (rand() % 80) - 40, 0, 0, 0, 0);
				add_effect_impl( EffectType::FOOTPRINT, dX * 32 + (rand() % 120) - 60, dY * 32 + (rand() % 80) - 40, 0, 0, 0, 0);
				m_effect_list[i]->m_max_frame = 2;
				m_effect_list[i]->m_frame_time = 10;
				break;

			case EffectType::LIGHTNING: // Lightning
				m_effect_list[i]->m_move_x = sX * 32;
				m_effect_list[i]->m_move_y = sY * 32 - 50;
				m_effect_list[i]->m_error = 0;
				m_effect_list[i]->m_render_x = 5 - (rand() % 10);
				m_effect_list[i]->m_render_y = 5 - (rand() % 10);
				m_effect_list[i]->m_max_frame = 7;
				m_effect_list[i]->m_frame_time = 10;
				lPan = -(((m_game->m_Camera.get_x() / 32) + fixx) - sX) * fixpan;
				audio_manager::get().play_game_sound(sound_type::effect, 40, dist, lPan);
				break;

			case EffectType::CHILL_WIND: // ChillWind
				m_effect_list[i]->m_max_frame = 2;
				m_effect_list[i]->m_frame_time = 10;
				break;

			case EffectType::TRIPLE_ENERGY_BOLT: // Triple-Energy-Bolt
				m_effect_list[i]->m_max_frame = 0;
				m_effect_list[i]->m_frame_time = 20;
				break;

			case EffectType::BERSERK: // Berserk : Cirlcle 6 magic
			case EffectType::ILLUSION_MOVEMENT: // Illusion-Movement
			case EffectType::ILLUSION: // Illusion
			case EffectType::INHIBITION_CASTING: // Inhibition-Casting
			case EffectType::MASS_ILLUSION: // Mass-Illusion
			case EffectType::MASS_ILLUSION_MOVEMENT: // Mass-Illusion-Movement
				m_effect_list[i]->m_max_frame = 11;
				m_effect_list[i]->m_frame_time = 100;
				lPan = -(((m_game->m_Camera.get_x() / 32) + fixx) - sX) * fixpan;
				audio_manager::get().play_game_sound(sound_type::effect, 5, dist, lPan);
				break;

			case EffectType::LIGHTNING_BOLT: // LightningBolt
				m_effect_list[i]->m_move_x = sX * 32;
				m_effect_list[i]->m_move_y = sY * 32 - 50;
				m_effect_list[i]->m_error = 0;
				m_effect_list[i]->m_render_x = 5 - (rand() % 10);
				m_effect_list[i]->m_render_y = 5 - (rand() % 10);
				m_effect_list[i]->m_max_frame = 10;
				m_effect_list[i]->m_frame_time = 10;
				lPan = -(((m_game->m_Camera.get_x() / 32) + fixx) - sX) * fixpan;
				audio_manager::get().play_game_sound(sound_type::effect, 40, dist, lPan);
				break;

			case EffectType::MASS_LIGHTNING_ARROW: // Mass-Ligtning-Arrow
				m_effect_list[i]->m_max_frame = 3;
				m_effect_list[i]->m_frame_time = 130;
				break;

			case EffectType::ICE_STRIKE: // Ice-Strike
				m_effect_list[i]->m_max_frame = 2;
				m_effect_list[i]->m_frame_time = 10;
				break;

			case EffectType::ENERGY_STRIKE: // Energy-Strike
				m_effect_list[i]->m_max_frame = 7;
				m_effect_list[i]->m_frame_time = 80;
				break;

			case EffectType::MASS_FIRE_STRIKE_FLYING: // Mass-Fire-Strike
			case EffectType::SALMON_BURST: //
				m_effect_list[i]->m_move_x = sX * 32;
				m_effect_list[i]->m_move_y = sY * 32 - 40;
				m_effect_list[i]->m_error = 0;
				m_effect_list[i]->m_max_frame = 0;
				m_effect_list[i]->m_frame_time = 20;
				m_effect_list[i]->m_dir = CMisc::calc_direction(sX, sY, dX, dY);
				lPan = -(((m_game->m_Camera.get_x() / 32) + fixx) - sX) * fixpan;
				audio_manager::get().play_game_sound(sound_type::effect, 1, dist, lPan);
				break;

			case EffectType::MASS_CHILL_WIND_SPELL: // Mass-Chill-Wind
				m_effect_list[i]->m_max_frame = 2;
				m_effect_list[i]->m_frame_time = 10;
				break;

			case EffectType::WORM_BITE_MASS: // worm-bite
				lPan = -(((m_game->m_Camera.get_x() / 32) + fixx) - sX) * fixpan;
				audio_manager::get().play_game_sound(sound_type::effect, 4, dist, lPan);
				add_effect_impl( EffectType::FOOTPRINT, dX * 32 + (rand() % 120) - 60, dY * 32 + (rand() % 80) - 40, 0, 0, 0, 0);
				add_effect_impl( EffectType::FOOTPRINT, dX * 32 + (rand() % 120) - 60, dY * 32 + (rand() % 80) - 40, 0, 0, 0, 0);
				add_effect_impl( EffectType::FOOTPRINT, dX * 32 + (rand() % 120) - 60, dY * 32 + (rand() % 80) - 40, 0, 0, 0, 0);
				add_effect_impl( EffectType::FOOTPRINT, dX * 32 + (rand() % 120) - 60, dY * 32 + (rand() % 80) - 40, 0, 0, 0, 0);
				add_effect_impl( EffectType::FOOTPRINT, dX * 32 + (rand() % 120) - 60, dY * 32 + (rand() % 80) - 40, 0, 0, 0, 0);
				add_effect_impl( EffectType::FOOTPRINT, dX * 32 + (rand() % 120) - 60, dY * 32 + (rand() % 80) - 40, 0, 0, 0, 0);
				add_effect_impl( EffectType::FOOTPRINT, dX * 32 + (rand() % 120) - 60, dY * 32 + (rand() % 80) - 40, 0, 0, 0, 0);

				add_effect_impl( EffectType::FOOTPRINT, dX * 32 + (rand() % 120) - 60, dY * 32 + (rand() % 80) - 40, 0, 0, 0, 0);
				add_effect_impl( EffectType::FOOTPRINT, dX * 32 + (rand() % 120) - 60, dY * 32 + (rand() % 80) - 40, 0, 0, 0, 0);
				add_effect_impl( EffectType::FOOTPRINT, dX * 32 + (rand() % 120) - 60, dY * 32 + (rand() % 80) - 40, 0, 0, 0, 0);
				add_effect_impl( EffectType::FOOTPRINT, dX * 32 + (rand() % 120) - 60, dY * 32 + (rand() % 80) - 40, 0, 0, 0, 0);
				add_effect_impl( EffectType::FOOTPRINT, dX * 32 + (rand() % 120) - 60, dY * 32 + (rand() % 80) - 40, 0, 0, 0, 0);
				add_effect_impl( EffectType::FOOTPRINT, dX * 32 + (rand() % 120) - 60, dY * 32 + (rand() % 80) - 40, 0, 0, 0, 0);
				add_effect_impl( EffectType::FOOTPRINT, dX * 32 + (rand() % 120) - 60, dY * 32 + (rand() % 80) - 40, 0, 0, 0, 0);
				m_effect_list[i]->m_max_frame = 1;
				m_effect_list[i]->m_frame_time = 10;
				break;

			case EffectType::ABSOLUTE_MAGIC_PROTECTION: // Absolute-Magic-Protection
				m_effect_list[i]->m_max_frame = 21;
				m_effect_list[i]->m_frame_time = 70;
				lPan = -(((m_game->m_Camera.get_x() / 32) + fixx) - sX) * fixpan;
				audio_manager::get().play_game_sound(sound_type::effect, 5, dist, lPan);
				break;

			case EffectType::ARMOR_BREAK: // Armor Break
				m_effect_list[i]->m_max_frame = 13;
				m_effect_list[i]->m_frame_time = 80;
				lPan = -(((m_game->m_Camera.get_x() / 32) + fixx) - sX) * fixpan;
				audio_manager::get().play_game_sound(sound_type::effect, 5, dist, lPan);
				break;

			case EffectType::BLOODY_SHOCK_WAVE: // Bloody-Shock-Wave
				m_effect_list[i]->m_max_frame = 7;
				m_effect_list[i]->m_frame_time = 80;
				break;

			case EffectType::MASS_ICE_STRIKE: // Mass-Ice-Strike
				m_effect_list[i]->m_max_frame = 2;
				m_effect_list[i]->m_frame_time = 10;
				break;

			case EffectType::LIGHTNING_STRIKE: // Lightning-Strike
				m_effect_list[i]->m_max_frame = 5;
				m_effect_list[i]->m_frame_time = 120;
				break;

			case EffectType::CANCELLATION: // Snoopy: Added Cancellation
				m_effect_list[i]->m_max_frame = 23;
				m_effect_list[i]->m_frame_time = 60;
				dist = dist / 32;
				lPan = -(((m_game->m_Camera.get_x() / 32) + fixx) - sX) * fixpan;
				audio_manager::get().play_game_sound(sound_type::effect, 5, dist, lPan);
				break;

			case EffectType::METEOR_STRIKE_DESCENDING: // MS
				m_effect_list[i]->m_move_x = dX * 32 + 300;
				m_effect_list[i]->m_move_y = dY * 32 - 460;
				m_effect_list[i]->m_max_frame = 10;
				m_effect_list[i]->m_frame_time = 25;
				break;

			case EffectType::MASS_MAGIC_MISSILE_FLYING: // Snoopy: Added Mass-Magic-Missile
				m_effect_list[i]->m_move_x = sX * 32;
				m_effect_list[i]->m_move_y = sY * 32 - 40;
				m_effect_list[i]->m_error = 0;
				m_effect_list[i]->m_max_frame = 0;
				m_effect_list[i]->m_frame_time = 20;
				lPan = -(((m_game->m_Camera.get_x() / 32) + fixx) - sX) * fixpan;
				audio_manager::get().play_game_sound(sound_type::effect, 1, dist, lPan);
				break;

			case EffectType::MASS_MM_AURA_CASTER: // Snoopy: Moved for new spells: Caster aura for Mass MagicMissile
				//case 184: // Effect on caster for MassMM
				m_effect_list[i]->m_max_frame = 29;
				m_effect_list[i]->m_frame_time = 80;
				m_effect_list[i]->m_move_x = sX;
				m_effect_list[i]->m_move_y = sY;
				break;

			case EffectType::BLIZZARD: // Blizzard
				m_effect_list[i]->m_max_frame = 7;
				m_effect_list[i]->m_frame_time = 80;
				break;

				//case 192: // Hero set Effect
			case EffectType::MAGE_HERO_SET: // Hero set Effect
				m_effect_list[i]->m_max_frame = 30;
				m_effect_list[i]->m_frame_time = 40;
				break;

				//case 193: // Hero set Effect
			case EffectType::WAR_HERO_SET: // Hero set Effect
				m_effect_list[i]->m_max_frame = 19;
				m_effect_list[i]->m_frame_time = 18;
				break;

			case EffectType::RESURRECTION: // Resurrection
				m_effect_list[i]->m_max_frame = 30;
				m_effect_list[i]->m_frame_time = 40;
				break;

			case EffectType::EARTH_SHOCK_WAVE: // Snoopy: Added Earth-Shock-Wave
				m_effect_list[i]->m_move_x = sX * 32;
				m_effect_list[i]->m_move_y = sY * 32;
				m_effect_list[i]->m_error = 0;
				m_effect_list[i]->m_max_frame = 30;
				m_effect_list[i]->m_frame_time = 25;
				m_game->set_camera_shaking_effect(dist);
				break;
			case EffectType::SHOTSTAR_FALL_1: //
			case EffectType::SHOTSTAR_FALL_2: //
			case EffectType::SHOTSTAR_FALL_3: //
				m_effect_list[i]->m_move_x = sX;
				m_effect_list[i]->m_move_y = sY;
				m_effect_list[i]->m_max_frame = 15;
				m_effect_list[i]->m_frame_time = 25;
				break;

			case EffectType::EXPLOSION_FIRE_APOCALYPSE: //
				m_effect_list[i]->m_move_x = sX;
				m_effect_list[i]->m_move_y = sY;
				m_effect_list[i]->m_max_frame = 18;
				m_effect_list[i]->m_frame_time = 70;
				break;

			case EffectType::CRACK_OBLIQUE: //
			case EffectType::CRACK_HORIZONTAL: //
				m_effect_list[i]->m_move_x = sX;
				m_effect_list[i]->m_move_y = sY;
				m_effect_list[i]->m_max_frame = 12;
				m_effect_list[i]->m_frame_time = 70;
				break;

			case EffectType::STEAMS_SMOKE: //
				m_effect_list[i]->m_move_x = sX;
				m_effect_list[i]->m_move_y = sY;
				m_effect_list[i]->m_max_frame = 3;
				m_effect_list[i]->m_frame_time = 70;
				break;

			case EffectType::GATE_ROUND: //
				m_effect_list[i]->m_move_x = sX * 32;
				m_effect_list[i]->m_move_y = sY * 32 - 40;
				m_effect_list[i]->m_error = 0;
				m_effect_list[i]->m_max_frame = 0;
				m_effect_list[i]->m_frame_time = 10;
				break;

			default:
				break;
			}
			m_effect_list[i]->m_move_x2 = m_effect_list[i]->m_move_x;
			m_effect_list[i]->m_move_y2 = m_effect_list[i]->m_move_y;
			return;
		}
}
