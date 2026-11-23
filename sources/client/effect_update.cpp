// effect_update.cpp: UpdateEffects implementation
//
//////////////////////////////////////////////////////////////////////

#include "effect_manager.h"
#include "game_constants.h"
#include "game.h"
#include "sprite.h"
#include "effect.h"
#include "global_def.h"
#include "misc.h"
#include "audio_manager.h"


void effect_manager::update_effects_impl()
{
	int i, x;
	uint32_t time;

	short abs_x, abs_y, dist;
	direction dir;
	long lPan;
	time = m_game->m_cur_time;
	time += m_game->m_map_data->m_frame_adjust_time;
	for (i = 0; i < game_limits::max_effects; i++)
		if (m_effect_list[i] != 0) {
			if ((time - m_effect_list[i]->m_time) > m_effect_list[i]->m_frame_time)
			{
				m_effect_list[i]->m_time = time;
				m_effect_list[i]->m_frame++;

				m_effect_list[i]->m_move_x2 = m_effect_list[i]->m_move_x;
				m_effect_list[i]->m_move_y2 = m_effect_list[i]->m_move_y;
				switch (m_effect_list[i]->m_type) {
				case EffectType::NORMAL_HIT: // coup normal
					if (m_effect_list[i]->m_frame == 1)
					{
						for (int j = 1; j <= m_effect_list[i]->m_value1; j++) add_effect_impl(EffectType::BURST_SMALL_GRENADE, m_effect_list[i]->m_move_x + 15 - (rand() % 30), m_effect_list[i]->m_move_y + 15 - (rand() % 30), 0, 0, -1 * (rand() % 2), 0);
					}
					if (m_effect_list[i]->m_frame >= m_effect_list[i]->m_max_frame)
					{
						delete m_effect_list[i];
						m_effect_list[i] = 0;
					}
					break;

				case EffectType::ARROW_FLYING:	// (Arrow missing target ?)
					CMisc::get_point(m_effect_list[i]->m_move_x, m_effect_list[i]->m_move_y,
						m_effect_list[i]->m_dest_x * 32, m_effect_list[i]->m_dest_y * 32 - 40,
						&m_effect_list[i]->m_move_x, &m_effect_list[i]->m_move_y,
						&m_effect_list[i]->m_error, 70);
					if ((abs(m_effect_list[i]->m_move_x - m_effect_list[i]->m_dest_x * 32) <= 2)
						&& (abs(m_effect_list[i]->m_move_y - (m_effect_list[i]->m_dest_y * 32 - 40)) <= 2))
					{
						delete m_effect_list[i];
						m_effect_list[i] = 0;
					}
					break;

				case EffectType::GOLD_DROP: // Gold Drop ,33,69,70
				case EffectType::IMPACT_EFFECT: //
				case EffectType::LIGHT_EFFECT_1:
				case EffectType::LIGHT_EFFECT_2:
					if (m_effect_list[i]->m_frame > m_effect_list[i]->m_max_frame)
					{
						delete m_effect_list[i];
						m_effect_list[i] = 0;
					}
					break;

				case EffectType::FIREBALL_EXPLOSION:
				case EffectType::MASS_FIRE_STRIKE_CALLER1:
				case EffectType::MASS_FIRE_STRIKE_CALLER3: // Fire Explosion
				case EffectType::SALMON_BURST_IMPACT:
					if (m_effect_list[i]->m_frame == 1)
					{
						add_effect_impl(EffectType::BURST_LARGE, m_effect_list[i]->m_move_x + 5 - (rand() % 10), m_effect_list[i]->m_move_y + 5 - (rand() % 10), 0, 0, -1 * (rand() % 2), 0);
						add_effect_impl(EffectType::BURST_LARGE, m_effect_list[i]->m_move_x + 5 - (rand() % 10), m_effect_list[i]->m_move_y + 5 - (rand() % 10), 0, 0, -1 * (rand() % 2), 0);
						add_effect_impl(EffectType::BURST_LARGE, m_effect_list[i]->m_move_x + 5 - (rand() % 10), m_effect_list[i]->m_move_y + 5 - (rand() % 10), 0, 0, -1 * (rand() % 2), 0);
						add_effect_impl(EffectType::BURST_LARGE, m_effect_list[i]->m_move_x + 5 - (rand() % 10), m_effect_list[i]->m_move_y + 5 - (rand() % 10), 0, 0, -1 * (rand() % 2), 0);
						add_effect_impl(EffectType::BURST_LARGE, m_effect_list[i]->m_move_x + 5 - (rand() % 10), m_effect_list[i]->m_move_y + 5 - (rand() % 10), 0, 0, -1 * (rand() % 2), 0);
					}
					if (m_effect_list[i]->m_frame == 7)
					{
						add_effect_impl(EffectType::RED_CLOUD_PARTICLES, m_effect_list[i]->m_move_x + 5 - (rand() % 10), m_effect_list[i]->m_move_y + 5 - (rand() % 10), 0, 0, 0, 0);
						add_effect_impl(EffectType::RED_CLOUD_PARTICLES, m_effect_list[i]->m_move_x + 5 - (rand() % 10), m_effect_list[i]->m_move_y + 5 - (rand() % 10), 0, 0, 0, 0);
						add_effect_impl(EffectType::RED_CLOUD_PARTICLES, m_effect_list[i]->m_move_x + 5 - (rand() % 10), m_effect_list[i]->m_move_y + 5 - (rand() % 10), 0, 0, 0, 0);
					}
					if (m_effect_list[i]->m_frame > m_effect_list[i]->m_max_frame)
					{
						delete m_effect_list[i];
						m_effect_list[i] = 0;
					}
					break;

				case EffectType::ENERGY_BOLT_EXPLOSION: // Lightning Bolt Burst
					if (m_effect_list[i]->m_frame == 1)
					{
						add_effect_impl(EffectType::BURST_MEDIUM, m_effect_list[i]->m_move_x + 5 - (rand() % 10), m_effect_list[i]->m_move_y + 5 - (rand() % 10), 0, 0, -1 * (rand() % 2));
						add_effect_impl(EffectType::BURST_MEDIUM, m_effect_list[i]->m_move_x + 5 - (rand() % 10), m_effect_list[i]->m_move_y + 5 - (rand() % 10), 0, 0, -1 * (rand() % 2));
						add_effect_impl(EffectType::BURST_MEDIUM, m_effect_list[i]->m_move_x + 5 - (rand() % 10), m_effect_list[i]->m_move_y + 5 - (rand() % 10), 0, 0, -1 * (rand() % 2));
						add_effect_impl(EffectType::BURST_MEDIUM, m_effect_list[i]->m_move_x + 5 - (rand() % 10), m_effect_list[i]->m_move_y + 5 - (rand() % 10), 0, 0, -1 * (rand() % 2));
						add_effect_impl(EffectType::BURST_MEDIUM, m_effect_list[i]->m_move_x + 5 - (rand() % 10), m_effect_list[i]->m_move_y + 5 - (rand() % 10), 0, 0, -1 * (rand() % 2));
					}
					if (m_effect_list[i]->m_frame >= m_effect_list[i]->m_max_frame)
					{
						delete m_effect_list[i];
						m_effect_list[i] = 0;
					}
					break;

				case EffectType::MAGIC_MISSILE_EXPLOSION: // Magic Missile Burst
					if (m_effect_list[i]->m_frame == 1)
					{
						add_effect_impl(EffectType::BURST_MEDIUM, m_effect_list[i]->m_move_x + 5 - (rand() % 10), m_effect_list[i]->m_move_y + 5 - (rand() % 10), 0, 0, -1 * (rand() % 2));
						add_effect_impl(EffectType::BURST_MEDIUM, m_effect_list[i]->m_move_x + 5 - (rand() % 10), m_effect_list[i]->m_move_y + 5 - (rand() % 10), 0, 0, -1 * (rand() % 2));
						add_effect_impl(EffectType::BURST_MEDIUM, m_effect_list[i]->m_move_x + 5 - (rand() % 10), m_effect_list[i]->m_move_y + 5 - (rand() % 10), 0, 0, -1 * (rand() % 2));
					}
					if (m_effect_list[i]->m_frame >= m_effect_list[i]->m_max_frame)
					{
						delete m_effect_list[i];
						m_effect_list[i] = 0;
					}
					break;

				case EffectType::BURST_MEDIUM:  // Burst Type 2
				case EffectType::BURST_SMALL_GRENADE: // Burst Type 3
					m_effect_list[i]->m_move_x += m_effect_list[i]->m_render_x;
					m_effect_list[i]->m_move_y += m_effect_list[i]->m_render_y;
					m_effect_list[i]->m_render_y++;
					if (m_effect_list[i]->m_frame > m_effect_list[i]->m_max_frame)
					{
						delete m_effect_list[i];
						m_effect_list[i] = 0;
					}
					break;

				case EffectType::LIGHTNING_ARROW_EXPLOSION: // Lightning Arrow Burst
					if (m_effect_list[i]->m_frame == 1)
					{
						add_effect_impl(EffectType::BURST_MEDIUM, m_effect_list[i]->m_move_x + 20 - (rand() % 40), m_effect_list[i]->m_move_y + 20 - (rand() % 40), 0, 0, -1 * (rand() % 2));
						add_effect_impl(EffectType::BURST_MEDIUM, m_effect_list[i]->m_move_x + 20 - (rand() % 40), m_effect_list[i]->m_move_y + 20 - (rand() % 40), 0, 0, -1 * (rand() % 2));
						add_effect_impl(EffectType::BURST_MEDIUM, m_effect_list[i]->m_move_x + 20 - (rand() % 40), m_effect_list[i]->m_move_y + 20 - (rand() % 40), 0, 0, -1 * (rand() % 2));
						add_effect_impl(EffectType::BURST_MEDIUM, m_effect_list[i]->m_move_x + 20 - (rand() % 40), m_effect_list[i]->m_move_y + 20 - (rand() % 40), 0, 0, -1 * (rand() % 2));
						add_effect_impl(EffectType::BURST_MEDIUM, m_effect_list[i]->m_move_x + 20 - (rand() % 40), m_effect_list[i]->m_move_y + 20 - (rand() % 40), 0, 0, -1 * (rand() % 2));
						add_effect_impl(EffectType::BURST_MEDIUM, m_effect_list[i]->m_move_x + 20 - (rand() % 40), m_effect_list[i]->m_move_y + 20 - (rand() % 40), 0, 0, -1 * (rand() % 2));
						add_effect_impl(EffectType::BURST_MEDIUM, m_effect_list[i]->m_move_x + 20 - (rand() % 40), m_effect_list[i]->m_move_y + 20 - (rand() % 40), 0, 0, -1 * (rand() % 2));
						add_effect_impl(EffectType::BURST_MEDIUM, m_effect_list[i]->m_move_x + 20 - (rand() % 40), m_effect_list[i]->m_move_y + 20 - (rand() % 40), 0, 0, -1 * (rand() % 2));
						add_effect_impl(EffectType::BURST_MEDIUM, m_effect_list[i]->m_move_x + 20 - (rand() % 40), m_effect_list[i]->m_move_y + 20 - (rand() % 40), 0, 0, -1 * (rand() % 2));
						add_effect_impl(EffectType::BURST_MEDIUM, m_effect_list[i]->m_move_x + 20 - (rand() % 40), m_effect_list[i]->m_move_y + 20 - (rand() % 40), 0, 0, -1 * (rand() % 2));
						add_effect_impl(EffectType::BURST_MEDIUM, m_effect_list[i]->m_move_x + 20 - (rand() % 40), m_effect_list[i]->m_move_y + 20 - (rand() % 40), 0, 0, -1 * (rand() % 2));
					}
					if (m_effect_list[i]->m_frame >= m_effect_list[i]->m_max_frame)
					{
						delete m_effect_list[i];
						m_effect_list[i] = 0;
					}
					break;

				case EffectType::BURST_LARGE: // Burst Type 4
					m_effect_list[i]->m_move_x += m_effect_list[i]->m_render_x;
					m_effect_list[i]->m_move_y += m_effect_list[i]->m_render_y;
					if (m_effect_list[i]->m_frame > m_effect_list[i]->m_max_frame)
					{
						delete m_effect_list[i];
						m_effect_list[i] = 0;
					}
					break;

				case EffectType::BUBBLES_DRUNK: // Bulles druncncity
					if (m_effect_list[i]->m_frame < 15)
					{
						if ((rand() % 2) == 0)
							m_effect_list[i]->m_move_x++;
						else m_effect_list[i]->m_move_x--;
						m_effect_list[i]->m_move_y--;
					}
					if (m_effect_list[i]->m_frame > m_effect_list[i]->m_max_frame)
					{
						delete m_effect_list[i];
						m_effect_list[i] = 0;
					}
					break;

				case EffectType::PROJECTILE_GENERIC: //
					CMisc::get_point(m_effect_list[i]->m_move_x, m_effect_list[i]->m_move_y,
						m_effect_list[i]->m_dest_x, m_effect_list[i]->m_dest_y,
						&m_effect_list[i]->m_move_x, &m_effect_list[i]->m_move_y,
						&m_effect_list[i]->m_error, 40);
					add_effect_impl(EffectType::BURST_SMALL, m_effect_list[i]->m_move_x + (rand() % 20) - 10, m_effect_list[i]->m_move_y + (rand() % 20) - 10, 0, 0, -1 * (rand() % 4));
					if ((abs(m_effect_list[i]->m_move_x - m_effect_list[i]->m_dest_x) <= 2)
						&& (abs(m_effect_list[i]->m_move_y - (m_effect_list[i]->m_dest_y)) <= 2))
					{
						add_effect_impl(EffectType::IMPACT_BURST, m_effect_list[i]->m_dest_x, m_effect_list[i]->m_dest_y, 0, 0, 0, 0); // testcode 0111 18
						add_effect_impl(EffectType::BURST_MEDIUM, m_effect_list[i]->m_move_x + 20 - (rand() % 40), m_effect_list[i]->m_move_y + 20 - (rand() % 40), 0, 0, -1 * (rand() % 2));
						add_effect_impl(EffectType::BURST_MEDIUM, m_effect_list[i]->m_move_x + 20 - (rand() % 40), m_effect_list[i]->m_move_y + 20 - (rand() % 40), 0, 0, -1 * (rand() % 2));
						add_effect_impl(EffectType::BURST_MEDIUM, m_effect_list[i]->m_move_x + 20 - (rand() % 40), m_effect_list[i]->m_move_y + 20 - (rand() % 40), 0, 0, -1 * (rand() % 2));
						add_effect_impl(EffectType::BURST_MEDIUM, m_effect_list[i]->m_move_x + 20 - (rand() % 40), m_effect_list[i]->m_move_y + 20 - (rand() % 40), 0, 0, -1 * (rand() % 2));
						add_effect_impl(EffectType::BURST_MEDIUM, m_effect_list[i]->m_move_x + 20 - (rand() % 40), m_effect_list[i]->m_move_y + 20 - (rand() % 40), 0, 0, -1 * (rand() % 2));
						delete m_effect_list[i];
						m_effect_list[i] = 0;
					}
					break;

				case EffectType::ICE_STORM: // Ice-Storm
					dir = CMisc::get_next_move_dir(m_effect_list[i]->m_move_x, m_effect_list[i]->m_move_y, m_effect_list[i]->m_move_x3, m_effect_list[i]->m_move_y3);
					switch (dir) {
					case direction::north:
						m_effect_list[i]->m_render_y -= 2;
						break;
					case direction::northeast:
						m_effect_list[i]->m_render_y -= 2;
						m_effect_list[i]->m_render_x += 2;
						break;
					case direction::east:
						m_effect_list[i]->m_render_x += 2;
						break;
					case direction::southeast:
						m_effect_list[i]->m_render_x += 2;
						m_effect_list[i]->m_render_y += 2;
						break;
					case direction::south:
						m_effect_list[i]->m_render_y += 2;
						break;
					case direction::southwest:
						m_effect_list[i]->m_render_x -= 2;
						m_effect_list[i]->m_render_y += 2;
						break;
					case direction::west:
						m_effect_list[i]->m_render_x -= 2;
						break;
					case direction::northwest:
						m_effect_list[i]->m_render_x -= 2;
						m_effect_list[i]->m_render_y -= 2;
						break;
					}
					if (m_effect_list[i]->m_render_x < -10) m_effect_list[i]->m_render_x = -10;
					if (m_effect_list[i]->m_render_x > 10) m_effect_list[i]->m_render_x = 10;
					if (m_effect_list[i]->m_render_y < -10) m_effect_list[i]->m_render_y = -10;
					if (m_effect_list[i]->m_render_y > 10) m_effect_list[i]->m_render_y = 10;
					m_effect_list[i]->m_move_x += m_effect_list[i]->m_render_x;
					m_effect_list[i]->m_move_y += m_effect_list[i]->m_render_y;
					m_effect_list[i]->m_move_y3--;
					if (m_effect_list[i]->m_frame > 10)
					{
						m_effect_list[i]->m_frame = 0;
						if (abs(m_effect_list[i]->m_y - m_effect_list[i]->m_move_y3) > 100)
						{
							delete m_effect_list[i];
							m_effect_list[i] = 0;
						}
					}
					break;

				case EffectType::CRITICAL_STRIKE_1: // Critical strike with a weapon
				case EffectType::CRITICAL_STRIKE_2:
				case EffectType::CRITICAL_STRIKE_3:
				case EffectType::CRITICAL_STRIKE_4:
				case EffectType::CRITICAL_STRIKE_5:
				case EffectType::CRITICAL_STRIKE_6:
				case EffectType::CRITICAL_STRIKE_7:
				case EffectType::CRITICAL_STRIKE_8: // Critical strike with a weapon
					CMisc::get_point(m_effect_list[i]->m_move_x, m_effect_list[i]->m_move_y,
						m_effect_list[i]->m_dest_x * 32, m_effect_list[i]->m_dest_y * 32 - 40,
						&m_effect_list[i]->m_move_x, &m_effect_list[i]->m_move_y,
						&m_effect_list[i]->m_error, 50);
					add_effect_impl(EffectType::BURST_SMALL, m_effect_list[i]->m_move_x + 10 - (rand() % 20), m_effect_list[i]->m_move_y + 10 - (rand() % 20), 0, 0, 0, 0);
					add_effect_impl(EffectType::BURST_SMALL, m_effect_list[i]->m_move_x + 10 - (rand() % 20), m_effect_list[i]->m_move_y + 10 - (rand() % 20), 0, 0, 0, 0);
					add_effect_impl(EffectType::BURST_SMALL, m_effect_list[i]->m_move_x + 10 - (rand() % 20), m_effect_list[i]->m_move_y + 10 - (rand() % 20), 0, 0, 0, 0);
					add_effect_impl(EffectType::BURST_SMALL, m_effect_list[i]->m_move_x + 10 - (rand() % 20), m_effect_list[i]->m_move_y + 10 - (rand() % 20), 0, 0, 0, 0);
					add_effect_impl(EffectType::BURST_SMALL, m_effect_list[i]->m_move_x + 10 - (rand() % 20), m_effect_list[i]->m_move_y + 10 - (rand() % 20), 0, 0, 0, 0);
					if ((abs(m_effect_list[i]->m_move_x - m_effect_list[i]->m_dest_x * 32) <= 2) &&
						(abs(m_effect_list[i]->m_move_y - (m_effect_list[i]->m_dest_y * 32 - 40)) <= 2))
					{
						delete m_effect_list[i];
						m_effect_list[i] = 0;
					}
					break;

				case EffectType::BLOODY_SHOCK_STRIKE: //
					CMisc::get_point(m_effect_list[i]->m_move_x, m_effect_list[i]->m_move_y,
						m_effect_list[i]->m_dest_x, m_effect_list[i]->m_dest_y,
						&m_effect_list[i]->m_move_x, &m_effect_list[i]->m_move_y,
						&m_effect_list[i]->m_error, 50);
					add_effect_impl(EffectType::IMPACT_EFFECT, m_effect_list[i]->m_move_x + (rand() % 30) - 15, m_effect_list[i]->m_move_y + (rand() % 30) - 15, 0, 0, -1 * (rand() % 4));
					if ((abs(m_effect_list[i]->m_move_x - m_effect_list[i]->m_dest_x) <= 2) &&
						(abs(m_effect_list[i]->m_move_y - (m_effect_list[i]->m_dest_y)) <= 2))
					{
						add_effect_impl(EffectType::IMPACT_EFFECT, m_effect_list[i]->m_dest_x, m_effect_list[i]->m_dest_y, 0, 0, 0, 0); //7
						delete m_effect_list[i];
						m_effect_list[i] = 0;
					}
					break;


				case EffectType::CHILL_WIND_IMPACT:
				case EffectType::MASS_CHILL_WIND:
					if (m_effect_list[i]->m_frame == 9)
					{
						add_effect_impl(EffectType::SPARKLE_SMALL, m_effect_list[i]->m_move_x + ((rand() % 100) - 50), m_effect_list[i]->m_move_y + ((rand() % 70) - 35), 0, 0, 0, 0);
						add_effect_impl(EffectType::SPARKLE_SMALL, m_effect_list[i]->m_move_x + ((rand() % 100) - 50), m_effect_list[i]->m_move_y + ((rand() % 70) - 35), 0, 0, 0, 0);
						add_effect_impl(EffectType::SPARKLE_SMALL, m_effect_list[i]->m_move_x + ((rand() % 100) - 50), m_effect_list[i]->m_move_y + ((rand() % 70) - 35), 0, 0, 0, 0);
						add_effect_impl(EffectType::SPARKLE_SMALL, m_effect_list[i]->m_move_x + ((rand() % 100) - 50), m_effect_list[i]->m_move_y + ((rand() % 70) - 35), 0, 0, 0, 0);
						add_effect_impl(EffectType::SPARKLE_SMALL, m_effect_list[i]->m_move_x + ((rand() % 100) - 50), m_effect_list[i]->m_move_y + ((rand() % 70) - 35), 0, 0, 0, 0);
					}
					if (m_effect_list[i]->m_frame > m_effect_list[i]->m_max_frame)
					{
						delete m_effect_list[i];
						m_effect_list[i] = 0;
					}
					break;

				case EffectType::ICE_STRIKE_VARIANT_1: //Large Type 1, 2, 3, 4
				case EffectType::ICE_STRIKE_VARIANT_2:
				case EffectType::ICE_STRIKE_VARIANT_3:
				case EffectType::ICE_STRIKE_VARIANT_4:
				case EffectType::ICE_STRIKE_VARIANT_5: // Small Type 1, 2
				case EffectType::ICE_STRIKE_VARIANT_6:
					if (m_effect_list[i]->m_frame >= 7)
					{
						m_effect_list[i]->m_move_x--;
						m_effect_list[i]->m_move_y += m_effect_list[i]->m_value1;
						m_effect_list[i]->m_value1++;
					}

					if (m_effect_list[i]->m_frame > m_effect_list[i]->m_max_frame)
					{
						if ((m_effect_list[i]->m_type != EffectType::ICE_STRIKE_VARIANT_5) && (m_effect_list[i]->m_type != EffectType::ICE_STRIKE_VARIANT_6))
						{
							add_effect_impl(EffectType::SMOKE_DUST, m_effect_list[i]->m_move_x, m_effect_list[i]->m_move_y, 0, 0, 0, 0);
							add_effect_impl(EffectType::FOOTPRINT, m_effect_list[i]->m_move_x + ((rand() % 20) - 10), m_effect_list[i]->m_move_y + ((rand() % 20) - 10), 0, 0, 0, 0);
							add_effect_impl(EffectType::FOOTPRINT, m_effect_list[i]->m_move_x + ((rand() % 20) - 10), m_effect_list[i]->m_move_y + ((rand() % 20) - 10), 0, 0, 0, 0);
							add_effect_impl(EffectType::FOOTPRINT, m_effect_list[i]->m_move_x + ((rand() % 20) - 10), m_effect_list[i]->m_move_y + ((rand() % 20) - 10), 0, 0, 0, 0);
							add_effect_impl(EffectType::SPARKLE_SMALL, m_effect_list[i]->m_move_x + ((rand() % 20) - 10), m_effect_list[i]->m_move_y + ((rand() % 20) - 10), 0, 0, 0, 0);
							add_effect_impl(EffectType::SPARKLE_SMALL, m_effect_list[i]->m_move_x + ((rand() % 20) - 10), m_effect_list[i]->m_move_y + ((rand() % 20) - 10), 0, 0, 0, 0);
						}
						delete m_effect_list[i];
						m_effect_list[i] = 0;
					}
					break;

				case EffectType::BLIZZARD_VARIANT_1: // Blizzard
				case EffectType::BLIZZARD_VARIANT_2:
				case EffectType::BLIZZARD_VARIANT_3:
					if (m_effect_list[i]->m_frame >= 7)
					{
						m_effect_list[i]->m_move_x--;
						m_effect_list[i]->m_move_y += m_effect_list[i]->m_value1;
						m_effect_list[i]->m_value1 += 4;
					}
					if (m_effect_list[i]->m_frame > m_effect_list[i]->m_max_frame)
					{
						if (m_effect_list[i]->m_type == EffectType::BLIZZARD_VARIANT_3)
							add_effect_impl(EffectType::BLIZZARD_IMPACT, m_effect_list[i]->m_move_x, m_effect_list[i]->m_move_y, 0, 0, 0, 0);
						else add_effect_impl(EffectType::SMOKE_DUST, m_effect_list[i]->m_move_x, m_effect_list[i]->m_move_y, 0, 0, 0, 0);
						add_effect_impl(EffectType::FOOTPRINT, m_effect_list[i]->m_move_x + ((rand() % 20) - 10), m_effect_list[i]->m_move_y + ((rand() % 20) - 10), 0, 0, 0, 0);
						add_effect_impl(EffectType::FOOTPRINT, m_effect_list[i]->m_move_x + ((rand() % 20) - 10), m_effect_list[i]->m_move_y + ((rand() % 20) - 10), 0, 0, 0, 0);
						add_effect_impl(EffectType::FOOTPRINT, m_effect_list[i]->m_move_x + ((rand() % 20) - 10), m_effect_list[i]->m_move_y + ((rand() % 20) - 10), 0, 0, 0, 0);

						add_effect_impl(EffectType::SPARKLE_SMALL, m_effect_list[i]->m_move_x + ((rand() % 20) - 10), m_effect_list[i]->m_move_y + ((rand() % 20) - 10), 0, 0, 0, 0);
						add_effect_impl(EffectType::SPARKLE_SMALL, m_effect_list[i]->m_move_x + ((rand() % 20) - 10), m_effect_list[i]->m_move_y + ((rand() % 20) - 10), 0, 0, 0, 0);
						delete m_effect_list[i];
						m_effect_list[i] = 0;
					}
					break;

				case EffectType::METEOR_FLYING: //
				case EffectType::METEOR_STRIKE_DESCENDING: // Meteor-Strike
					if (m_effect_list[i]->m_frame > m_effect_list[i]->m_max_frame)
					{
						add_effect_impl(EffectType::FIRE_AURA_GROUND, m_effect_list[i]->m_move_x, m_effect_list[i]->m_move_y, 0, 0, 0, 0);
						add_effect_impl(EffectType::FIRE_EXPLOSION_CRUSADE, m_effect_list[i]->m_move_x, m_effect_list[i]->m_move_y, 0, 0, 0, 0);
						add_effect_impl(EffectType::BURST_LARGE, m_effect_list[i]->m_move_x + 5 - (rand() % 10), m_effect_list[i]->m_move_y + 5 - (rand() % 10), 0, 0, -1 * (rand() % 2), 0);
						add_effect_impl(EffectType::BURST_LARGE, m_effect_list[i]->m_move_x + 5 - (rand() % 10), m_effect_list[i]->m_move_y + 5 - (rand() % 10), 0, 0, -1 * (rand() % 2), 0);
						add_effect_impl(EffectType::BURST_LARGE, m_effect_list[i]->m_move_x + 5 - (rand() % 10), m_effect_list[i]->m_move_y + 5 - (rand() % 10), 0, 0, -1 * (rand() % 2), 0);
						add_effect_impl(EffectType::BURST_LARGE, m_effect_list[i]->m_move_x + 5 - (rand() % 10), m_effect_list[i]->m_move_y + 5 - (rand() % 10), 0, 0, -1 * (rand() % 2), 0);
						add_effect_impl(EffectType::BURST_LARGE, m_effect_list[i]->m_move_x + 5 - (rand() % 10), m_effect_list[i]->m_move_y + 5 - (rand() % 10), 0, 0, -1 * (rand() % 2), 0);
						delete m_effect_list[i];
						m_effect_list[i] = 0;
					}
					else if (m_effect_list[i]->m_frame >= 0)
					{
						m_effect_list[i]->m_move_x -= 30;
						m_effect_list[i]->m_move_y += 46;
						add_effect_impl(EffectType::METEOR_IMPACT, m_effect_list[i]->m_move_x, m_effect_list[i]->m_move_y, 0, 0, 0, 0);
					}
					break;

				case EffectType::METEOR_IMPACT:
					if (m_effect_list[i]->m_frame > m_effect_list[i]->m_max_frame)
					{
						delete m_effect_list[i];
						m_effect_list[i] = 0;
					}
					else if (m_effect_list[i]->m_frame >= 0)
					{
						m_effect_list[i]->m_move_x += (rand() % 3) - 1;
						m_effect_list[i]->m_move_y += (rand() % 3) - 1;
					}
					break;

				case EffectType::MS_CRUSADE_CASTING: // Building fire after MS (crusade) 65 & 67
					if (m_effect_list[i]->m_frame > m_effect_list[i]->m_max_frame)
					{
						delete m_effect_list[i];
						m_effect_list[i] = 0;
					}
					else if (m_effect_list[i]->m_frame >= 0)
					{
						m_effect_list[i]->m_move_x += (rand() % 3) - 1;
						m_effect_list[i]->m_move_y -= 4 + (rand() % 2);
					}
					break;

				case EffectType::MS_CRUSADE_EXPLOSION:
				case EffectType::EXPLOSION_FIRE_APOCALYPSE:
				case EffectType::CRACK_OBLIQUE:
				case EffectType::CRACK_HORIZONTAL:
				case EffectType::STEAMS_SMOKE:
					if (m_effect_list[i]->m_frame > m_effect_list[i]->m_max_frame)
					{
						delete m_effect_list[i];
						m_effect_list[i] = 0;
					}
					break;

				case EffectType::WORM_BITE:
					if (m_effect_list[i]->m_frame > m_effect_list[i]->m_max_frame)
					{
						delete m_effect_list[i];
						m_effect_list[i] = 0;
					}
					else if (m_effect_list[i]->m_frame == 11)
					{
						m_game->set_camera_shaking_effect(m_effect_list[i]->m_value1, 2);
					}
					break;

				case EffectType::BLIZZARD_PROJECTILE:
					CMisc::get_point(m_effect_list[i]->m_move_x, m_effect_list[i]->m_move_y,
						m_effect_list[i]->m_dest_x, m_effect_list[i]->m_dest_y,
						&m_effect_list[i]->m_move_x, &m_effect_list[i]->m_move_y,
						&m_effect_list[i]->m_error, 50);
					add_effect_impl(EffectType::BLIZZARD_VARIANT_2, m_effect_list[i]->m_move_x + (rand() % 30) - 15, m_effect_list[i]->m_move_y + (rand() % 30) - 15, 0, 0, 0, 0);
					add_effect_impl(EffectType::SPARKLE_SMALL, m_effect_list[i]->m_move_x + ((rand() % 20) - 10), m_effect_list[i]->m_move_y + ((rand() % 20) - 10), 0, 0, 0, 0);
					if ((abs(m_effect_list[i]->m_move_x - m_effect_list[i]->m_dest_x) <= 2) &&
						(abs(m_effect_list[i]->m_move_y - (m_effect_list[i]->m_dest_y)) <= 2))
					{
						add_effect_impl(EffectType::BLIZZARD_VARIANT_3, m_effect_list[i]->m_move_x/* + (rand() % 30) - 15*/, m_effect_list[i]->m_move_y/* + (rand() % 30) - 15*/, 0, 0, 0, 0);
						delete m_effect_list[i];
						m_effect_list[i] = 0;
					}
					break;

				case EffectType::STORM_BLADE: // Snoopy: Added StromBlade
					CMisc::get_point(m_effect_list[i]->m_move_x
						, m_effect_list[i]->m_move_y
						, m_effect_list[i]->m_dest_x * 32
						, m_effect_list[i]->m_dest_y * 32
						, &m_effect_list[i]->m_move_x
						, &m_effect_list[i]->m_move_y
						, &m_effect_list[i]->m_error
						, 10);
					if (m_effect_list[i]->m_frame > m_effect_list[i]->m_max_frame)
					{
						delete m_effect_list[i];
						m_effect_list[i] = 0;
					}
					break;

				case EffectType::MAGIC_MISSILE_FLYING: // Magic Missile
					CMisc::get_point(m_effect_list[i]->m_move_x, m_effect_list[i]->m_move_y,
						m_effect_list[i]->m_dest_x * 32, m_effect_list[i]->m_dest_y * 32/* - 40*/,
						&m_effect_list[i]->m_move_x, &m_effect_list[i]->m_move_y,
						&m_effect_list[i]->m_error, 50);
					add_effect_impl(EffectType::BURST_SMALL, m_effect_list[i]->m_move_x + (rand() % 20) - 10, m_effect_list[i]->m_move_y + (rand() % 20) - 10, 0, 0, -1 * (rand() % 4));

					if ((abs(m_effect_list[i]->m_move_x - m_effect_list[i]->m_dest_x * 32) <= 2) &&
						(abs(m_effect_list[i]->m_move_y - (m_effect_list[i]->m_dest_y * 32/* - 40*/)) <= 2))
					{
						add_effect_impl(EffectType::MAGIC_MISSILE_EXPLOSION, m_effect_list[i]->m_dest_x * 32, m_effect_list[i]->m_dest_y * 32, 0, 0, 0, 0);
						delete m_effect_list[i];
						m_effect_list[i] = 0;
					}
					break;

				case EffectType::ENERGY_BOLT_FLYING: // Enegy-Bolt
					CMisc::get_point(m_effect_list[i]->m_move_x, m_effect_list[i]->m_move_y,
						m_effect_list[i]->m_dest_x * 32, m_effect_list[i]->m_dest_y * 32/* - 40*/,
						&m_effect_list[i]->m_move_x, &m_effect_list[i]->m_move_y,
						&m_effect_list[i]->m_error, 50);
					add_effect_impl(EffectType::BURST_SMALL, m_effect_list[i]->m_move_x + (rand() % 20) - 10, m_effect_list[i]->m_move_y + (rand() % 20) - 10, 0, 0, -1 * (rand() % 4));
					add_effect_impl(EffectType::BURST_SMALL, m_effect_list[i]->m_move_x + (rand() % 20) - 10, m_effect_list[i]->m_move_y + (rand() % 20) - 10, 0, 0, -1 * (rand() % 4));
					if ((abs(m_effect_list[i]->m_move_x - m_effect_list[i]->m_dest_x * 32) <= 2)
						&& (abs(m_effect_list[i]->m_move_y - m_effect_list[i]->m_dest_y * 32) <= 2))
					{
						add_effect_impl(EffectType::ENERGY_BOLT_EXPLOSION, m_effect_list[i]->m_dest_x * 32, m_effect_list[i]->m_dest_y * 32, 0, 0, 0, 0); // 6 testcode 0111
						delete m_effect_list[i];
						m_effect_list[i] = 0;
					}
					break;

				case EffectType::FIRE_BALL_FLYING: // Fire Ball
					CMisc::get_point(m_effect_list[i]->m_move_x, m_effect_list[i]->m_move_y,
						m_effect_list[i]->m_dest_x * 32, m_effect_list[i]->m_dest_y * 32/* - 40*/,
						&m_effect_list[i]->m_move_x, &m_effect_list[i]->m_move_y,
						&m_effect_list[i]->m_error, 50);
					if ((abs(m_effect_list[i]->m_move_x - m_effect_list[i]->m_dest_x * 32) <= 2)
						&& (abs(m_effect_list[i]->m_move_y - m_effect_list[i]->m_dest_y * 32) <= 2))
					{
						add_effect_impl(EffectType::FIREBALL_EXPLOSION, m_effect_list[i]->m_dest_x * 32, m_effect_list[i]->m_dest_y * 32, 0, 0, 0, 0);
						delete m_effect_list[i];
						m_effect_list[i] = 0;
					}
					break;

				case EffectType::FIRE_STRIKE_FLYING: // Fire Strike
					CMisc::get_point(m_effect_list[i]->m_move_x, m_effect_list[i]->m_move_y,
						m_effect_list[i]->m_dest_x * 32, m_effect_list[i]->m_dest_y * 32/* - 40*/,
						&m_effect_list[i]->m_move_x, &m_effect_list[i]->m_move_y,
						&m_effect_list[i]->m_error, 50);
					if ((abs(m_effect_list[i]->m_move_x - m_effect_list[i]->m_dest_x * 32) <= 2)
						&& (abs(m_effect_list[i]->m_move_y - m_effect_list[i]->m_dest_y * 32) <= 2))
					{
						add_effect_impl(EffectType::FIREBALL_EXPLOSION, m_effect_list[i]->m_dest_x * 32, m_effect_list[i]->m_dest_y * 32, 0, 0, 0, 0);
						add_effect_impl(EffectType::FIREBALL_EXPLOSION, m_effect_list[i]->m_dest_x * 32 - 30, m_effect_list[i]->m_dest_y * 32 - 15, 0, 0, -7, 0);
						add_effect_impl(EffectType::FIREBALL_EXPLOSION, m_effect_list[i]->m_dest_x * 32 + 35, m_effect_list[i]->m_dest_y * 32 - 30, 0, 0, -5, 0);
						add_effect_impl(EffectType::FIREBALL_EXPLOSION, m_effect_list[i]->m_dest_x * 32 + 20, m_effect_list[i]->m_dest_y * 32 + 30, 0, 0, -3, 0);
						delete m_effect_list[i];
						m_effect_list[i] = 0;
					}
					break;

				case EffectType::LIGHTNING_ARROW_FLYING: // Lightning Arrow
					CMisc::get_point(m_effect_list[i]->m_move_x, m_effect_list[i]->m_move_y,
						m_effect_list[i]->m_dest_x * 32, m_effect_list[i]->m_dest_y * 32/* - 40*/,
						&m_effect_list[i]->m_move_x, &m_effect_list[i]->m_move_y,
						&m_effect_list[i]->m_error, 50);
					add_effect_impl(EffectType::BURST_SMALL, m_effect_list[i]->m_move_x + (rand() % 20) - 10, m_effect_list[i]->m_move_y + (rand() % 20) - 10, 0, 0, -1 * (rand() % 4));
					add_effect_impl(EffectType::BURST_SMALL, m_effect_list[i]->m_move_x + (rand() % 20) - 10, m_effect_list[i]->m_move_y + (rand() % 20) - 10, 0, 0, -1 * (rand() % 4));
					add_effect_impl(EffectType::BURST_SMALL, m_effect_list[i]->m_move_x + (rand() % 20) - 10, m_effect_list[i]->m_move_y + (rand() % 20) - 10, 0, 0, -1 * (rand() % 4));
					if ((abs(m_effect_list[i]->m_move_x - m_effect_list[i]->m_dest_x * 32) <= 2)
						&& (abs(m_effect_list[i]->m_move_y - m_effect_list[i]->m_dest_y * 32) <= 2))
					{
						add_effect_impl(EffectType::LIGHTNING_ARROW_EXPLOSION, m_effect_list[i]->m_dest_x * 32, m_effect_list[i]->m_dest_y * 32, 0, 0, 0, 0);
						delete m_effect_list[i];
						m_effect_list[i] = 0;
					}
					break;

				case EffectType::LIGHTNING: // Lightning
				case EffectType::LIGHTNING_BOLT: // Lightning-Bolt
					if (m_effect_list[i]->m_frame > m_effect_list[i]->m_max_frame)
					{
						add_effect_impl(EffectType::LIGHTNING_ARROW_EXPLOSION, m_effect_list[i]->m_dest_x * 32, m_effect_list[i]->m_dest_y * 32, 0, 0, 0, 0);
						delete m_effect_list[i];
						m_effect_list[i] = 0;
					}
					else
					{
						m_effect_list[i]->m_render_x = 5 - (rand() % 10);
						m_effect_list[i]->m_render_y = 5 - (rand() % 10);
					}
					break;

				case EffectType::CHILL_WIND: // Chill-Wind
					add_effect_impl(EffectType::CHILL_WIND_IMPACT, m_effect_list[i]->m_dest_x * 32, m_effect_list[i]->m_dest_y * 32, 0, 0, 0, 0);
					add_effect_impl(EffectType::CHILL_WIND_IMPACT, m_effect_list[i]->m_dest_x * 32 - 30, m_effect_list[i]->m_dest_y * 32 - 15, 0, 0, -10, 0);
					add_effect_impl(EffectType::CHILL_WIND_IMPACT, m_effect_list[i]->m_dest_x * 32 + 35, m_effect_list[i]->m_dest_y * 32 - 30, 0, 0, -6, 0);
					add_effect_impl(EffectType::CHILL_WIND_IMPACT, m_effect_list[i]->m_dest_x * 32 + 20, m_effect_list[i]->m_dest_y * 32 + 30, 0, 0, -3, 0);
					delete m_effect_list[i];
					m_effect_list[i] = 0;
					break;

				case EffectType::TRIPLE_ENERGY_BOLT:  // Triple-Energy-Bolt
					add_effect_impl(EffectType::ENERGY_BOLT_FLYING, m_effect_list[i]->m_x, m_effect_list[i]->m_y,
						m_effect_list[i]->m_dest_x - 1, m_effect_list[i]->m_dest_y - 1, 0);
					add_effect_impl(EffectType::ENERGY_BOLT_FLYING, m_effect_list[i]->m_x, m_effect_list[i]->m_y,
						m_effect_list[i]->m_dest_x + 1, m_effect_list[i]->m_dest_y - 1, 0);
					add_effect_impl(EffectType::ENERGY_BOLT_FLYING, m_effect_list[i]->m_x, m_effect_list[i]->m_y,
						m_effect_list[i]->m_dest_x + 1, m_effect_list[i]->m_dest_y + 1, 0);
					add_effect_impl(EffectType::BURST_SMALL, m_effect_list[i]->m_move_x + (rand() % 20) - 10, m_effect_list[i]->m_move_y + (rand() % 20) - 10, 0, 0, -1 * (rand() % 4));
					lPan = -(((m_game->m_Camera.get_x() / 32) + VIEW_CENTER_TILE_X()) - m_effect_list[i]->m_dest_x) * LOGICAL_WIDTH();
					audio_manager::get().play_game_sound(sound_type::effect, 1, dist, lPan);
					add_effect_impl(EffectType::MAGIC_MISSILE_EXPLOSION, m_effect_list[i]->m_dest_x * 32, m_effect_list[i]->m_dest_y * 32, 0, 0, 0, 0);
					delete m_effect_list[i];
					m_effect_list[i] = 0;
					break;

				case EffectType::MASS_LIGHTNING_ARROW: // Mass-Lightning-Arrow
					if (m_effect_list[i]->m_frame > m_effect_list[i]->m_max_frame)
					{
						delete m_effect_list[i];
						m_effect_list[i] = 0;
					}
					else
					{
						add_effect_impl(EffectType::LIGHTNING_ARROW_FLYING, m_effect_list[i]->m_x, m_effect_list[i]->m_y,
							m_effect_list[i]->m_dest_x, m_effect_list[i]->m_dest_y, 0);
						abs_x = abs(((m_game->m_Camera.get_x() / 32) + VIEW_CENTER_TILE_X()) - m_effect_list[i]->m_dest_x);
						abs_y = abs(((m_game->m_Camera.get_y() / 32) + VIEW_CENTER_TILE_Y()) - m_effect_list[i]->m_dest_y);
						if (abs_x > abs_y) dist = abs_x;
						else dist = abs_y;
						lPan = -(((m_game->m_Camera.get_x() / 32) + VIEW_CENTER_TILE_X()) - m_effect_list[i]->m_dest_x) * LOGICAL_WIDTH();
						audio_manager::get().play_game_sound(sound_type::effect, 1, dist, lPan);
					}
					break;

				case EffectType::ICE_STRIKE: // Ice-Strike
					add_effect_impl(EffectType::ICE_STRIKE_VARIANT_1, m_effect_list[i]->m_dest_x * 32, m_effect_list[i]->m_dest_y * 32, 0, 0, 0, 0);
					for (x = 0; x < 14; x++)
					{
						{
						constexpr EffectType ice_variants[] = { EffectType::ICE_STRIKE_VARIANT_1, EffectType::ICE_STRIKE_VARIANT_2, EffectType::ICE_STRIKE_VARIANT_3 };
						add_effect_impl(ice_variants[rand() % 3], m_effect_list[i]->m_dest_x * 32 + (rand() % 100) - 50 + 10, m_effect_list[i]->m_dest_y * 32 + (rand() % 90) - 45, 0, 0, -1 * x - 1);
					}
					}
					for (x = 0; x < 6; x++)
					{
						add_effect_impl(static_cast<EffectType>(45 + (rand() % 2)), m_effect_list[i]->m_dest_x * 32 + (rand() % 100) - 50 + 10, m_effect_list[i]->m_dest_y * 32 + (rand() % 90) - 45, 0, 0, -1 * x - 1 - 10);
					}
					delete m_effect_list[i];
					m_effect_list[i] = 0;
					break;

				case EffectType::ENERGY_STRIKE: // Energy-Strike
					if (m_effect_list[i]->m_frame > m_effect_list[i]->m_max_frame)
					{
						delete m_effect_list[i];
						m_effect_list[i] = 0;
					}
					else
					{
						add_effect_impl(EffectType::PROJECTILE_GENERIC, m_effect_list[i]->m_x, m_effect_list[i]->m_y,
							m_effect_list[i]->m_dest_x * 32 + 50 - (rand() % 100), m_effect_list[i]->m_dest_y * 32 + 50 - (rand() % 100), 0);
						abs_x = abs(((m_game->m_Camera.get_x() / 32) + VIEW_CENTER_TILE_X()) - m_effect_list[i]->m_dest_x);
						abs_y = abs(((m_game->m_Camera.get_y() / 32) + VIEW_CENTER_TILE_Y()) - m_effect_list[i]->m_dest_y);
						if (abs_x > abs_y) dist = abs_x;
						else dist = abs_y;
						lPan = -(((m_game->m_Camera.get_x() / 32) + VIEW_CENTER_TILE_X()) - m_effect_list[i]->m_dest_x);
						audio_manager::get().play_game_sound(sound_type::effect, 1, dist, lPan);
					}
					break;

				case EffectType::MASS_FIRE_STRIKE_FLYING: // Mass-Fire-Strike
					CMisc::get_point(m_effect_list[i]->m_move_x, m_effect_list[i]->m_move_y,
						m_effect_list[i]->m_dest_x * 32, m_effect_list[i]->m_dest_y * 32/* - 40*/,
						&m_effect_list[i]->m_move_x, &m_effect_list[i]->m_move_y,
						&m_effect_list[i]->m_error, 50);
					if ((abs(m_effect_list[i]->m_move_x - m_effect_list[i]->m_dest_x * 32) <= 2)
						&& (abs(m_effect_list[i]->m_move_y - m_effect_list[i]->m_dest_y * 32) <= 2))
					{
						add_effect_impl(EffectType::MASS_FIRE_STRIKE_CALLER1, m_effect_list[i]->m_dest_x * 32, m_effect_list[i]->m_dest_y * 32, 0, 0, 0, 0);
						add_effect_impl(EffectType::MASS_FIRE_STRIKE_CALLER3, m_effect_list[i]->m_dest_x * 32 - 30, m_effect_list[i]->m_dest_y * 32 - 15, 0, 0, -7, 0);
						add_effect_impl(EffectType::MASS_FIRE_STRIKE_CALLER3, m_effect_list[i]->m_dest_x * 32 + 35, m_effect_list[i]->m_dest_y * 32 - 30, 0, 0, -5, 0);
						add_effect_impl(EffectType::MASS_FIRE_STRIKE_CALLER3, m_effect_list[i]->m_dest_x * 32 + 20, m_effect_list[i]->m_dest_y * 32 + 30, 0, 0, -3, 0);
						delete m_effect_list[i];
						m_effect_list[i] = 0;
					}
					break;

				case EffectType::MASS_CHILL_WIND_SPELL: // Mass-Chill-Wind Chill-Wind
					add_effect_impl(EffectType::MASS_CHILL_WIND, m_effect_list[i]->m_dest_x * 32, m_effect_list[i]->m_dest_y * 32, 0, 0, 0, 0);
					add_effect_impl(EffectType::MASS_CHILL_WIND, m_effect_list[i]->m_dest_x * 32 - 30, m_effect_list[i]->m_dest_y * 32 - 15, 0, 0, -10, 0);
					add_effect_impl(EffectType::MASS_CHILL_WIND, m_effect_list[i]->m_dest_x * 32 + 35, m_effect_list[i]->m_dest_y * 32 - 30, 0, 0, -6, 0);
					add_effect_impl(EffectType::MASS_CHILL_WIND, m_effect_list[i]->m_dest_x * 32 + 20, m_effect_list[i]->m_dest_y * 32 + 30, 0, 0, -3, 0);
					add_effect_impl(EffectType::MASS_CHILL_WIND, m_effect_list[i]->m_dest_x * 32 + (rand() % 100) - 50, m_effect_list[i]->m_dest_y * 32 + (rand() % 70) - 35, 0, 0, -1 * (rand() % 10));
					add_effect_impl(EffectType::MASS_CHILL_WIND, m_effect_list[i]->m_dest_x * 32 + (rand() % 100) - 50, m_effect_list[i]->m_dest_y * 32 + (rand() % 70) - 35, 0, 0, -1 * (rand() % 10));
					add_effect_impl(EffectType::MASS_CHILL_WIND, m_effect_list[i]->m_dest_x * 32 + (rand() % 100) - 50, m_effect_list[i]->m_dest_y * 32 + (rand() % 70) - 35, 0, 0, -1 * (rand() % 10));
					add_effect_impl(EffectType::MASS_CHILL_WIND, m_effect_list[i]->m_dest_x * 32 + (rand() % 100) - 50, m_effect_list[i]->m_dest_y * 32 + (rand() % 70) - 35, 0, 0, -1 * (rand() % 10));
					delete m_effect_list[i];
					m_effect_list[i] = 0;
					break;

				case EffectType::WORM_BITE_MASS: // worm-bite
					if (m_effect_list[i]->m_frame > m_effect_list[i]->m_max_frame)
					{
						add_effect_impl(EffectType::WORM_BITE, m_effect_list[i]->m_dest_x * 32, m_effect_list[i]->m_dest_y * 32, 0, 0, 0, 0); // testcode 0111 18
						delete m_effect_list[i];
						m_effect_list[i] = 0;
					}
					break;

				case EffectType::BLOODY_SHOCK_WAVE: // Bloody-Shock-Wave
					if (m_effect_list[i]->m_frame > m_effect_list[i]->m_max_frame)
					{
						delete m_effect_list[i];
						m_effect_list[i] = 0;
					}
					else if ((m_effect_list[i]->m_frame % 2) == 0)
					{
						add_effect_impl(EffectType::BLOODY_SHOCK_STRIKE, m_effect_list[i]->m_x, m_effect_list[i]->m_y,
							m_effect_list[i]->m_dest_x * 32 + 30 - (rand() % 60), m_effect_list[i]->m_dest_y * 32 + 30 - (rand() % 60), 0);
						abs_x = abs(((m_game->m_Camera.get_x() / 32) + VIEW_CENTER_TILE_X()) - m_effect_list[i]->m_dest_x);
						abs_y = abs(((m_game->m_Camera.get_y() / 32) + VIEW_CENTER_TILE_Y()) - m_effect_list[i]->m_dest_y);
						if (abs_x > abs_y) dist = abs_x;
						else dist = abs_y;
						lPan = -(((m_game->m_Camera.get_x() / 32) + VIEW_CENTER_TILE_X()) - m_effect_list[i]->m_dest_x);
						audio_manager::get().play_game_sound(sound_type::effect, 1, dist, lPan);
					}
					break;

				case EffectType::MASS_ICE_STRIKE: // Mass-Ice-Strike
					add_effect_impl(EffectType::ICE_STRIKE_VARIANT_4, m_effect_list[i]->m_dest_x * 32, m_effect_list[i]->m_dest_y * 32, 0, 0, 0, 0);
					add_effect_impl(EffectType::ICE_STRIKE_VARIANT_4, m_effect_list[i]->m_dest_x * 32 + (rand() % 110) - 55 + 10, m_effect_list[i]->m_dest_y * 32 + (rand() % 100) - 50, 0, 0, -1 * (rand() % 3));
					add_effect_impl(EffectType::ICE_STRIKE_VARIANT_4, m_effect_list[i]->m_dest_x * 32 + (rand() % 110) - 55 + 10, m_effect_list[i]->m_dest_y * 32 + (rand() % 100) - 50, 0, 0, -1 * (rand() % 3));
					add_effect_impl(EffectType::ICE_STRIKE_VARIANT_4, m_effect_list[i]->m_dest_x * 32 + (rand() % 110) - 55 + 10, m_effect_list[i]->m_dest_y * 32 + (rand() % 100) - 50, 0, 0, -1 * (rand() % 3));
					add_effect_impl(EffectType::ICE_STRIKE_VARIANT_4, m_effect_list[i]->m_dest_x * 32 + (rand() % 110) - 55 + 10, m_effect_list[i]->m_dest_y * 32 + (rand() % 100) - 50, 0, 0, -1 * (rand() % 3));
					add_effect_impl(EffectType::ICE_STRIKE_VARIANT_4, m_effect_list[i]->m_dest_x * 32 + (rand() % 110) - 55 + 10, m_effect_list[i]->m_dest_y * 32 + (rand() % 100) - 50, 0, 0, -1 * (rand() % 3));
					add_effect_impl(EffectType::ICE_STRIKE_VARIANT_4, m_effect_list[i]->m_dest_x * 32 + (rand() % 110) - 55 + 10, m_effect_list[i]->m_dest_y * 32 + (rand() % 100) - 50, 0, 0, -1 * (rand() % 3));
					add_effect_impl(EffectType::ICE_STRIKE_VARIANT_4, m_effect_list[i]->m_dest_x * 32 + (rand() % 110) - 55 + 10, m_effect_list[i]->m_dest_y * 32 + (rand() % 100) - 50, 0, 0, -1 * (rand() % 3));
					add_effect_impl(EffectType::ICE_STRIKE_VARIANT_4, m_effect_list[i]->m_dest_x * 32 + (rand() % 110) - 55 + 10, m_effect_list[i]->m_dest_y * 32 + (rand() % 100) - 50, 0, 0, -1 * (rand() % 3));
					for (x = 0; x < 16; x++)
					{
						add_effect_impl(EffectType::ICE_STRIKE_VARIANT_4, m_effect_list[i]->m_dest_x * 32 + (rand() % 110) - 55 + 10, m_effect_list[i]->m_dest_y * 32 + (rand() % 100) - 50, 0, 0, -1 * x - 1, 0);
					}
					for (x = 0; x < 8; x++)
					{
						add_effect_impl(static_cast<EffectType>(45 + (rand() % 2)), m_effect_list[i]->m_dest_x * 32 + (rand() % 100) - 50 + 10, m_effect_list[i]->m_dest_y * 32 + (rand() % 90) - 45, 0, 0, -1 * x - 1 - 10);
					}
					delete m_effect_list[i];
					m_effect_list[i] = 0;
					break;

				case EffectType::LIGHTNING_STRIKE: // Lightning-Strike
					if (m_effect_list[i]->m_frame > m_effect_list[i]->m_max_frame)
					{
						delete m_effect_list[i];
						m_effect_list[i] = 0;
					}
					else
					{
						add_effect_impl(EffectType::LIGHTNING_BOLT, m_effect_list[i]->m_x, m_effect_list[i]->m_y,
							m_effect_list[i]->m_dest_x + (rand() % 3) - 1, m_effect_list[i]->m_dest_y + (rand() % 3) - 1, 0);
						abs_x = abs(((m_game->m_Camera.get_x() / 32) + VIEW_CENTER_TILE_X()) - m_effect_list[i]->m_dest_x);
						abs_y = abs(((m_game->m_Camera.get_y() / 32) + VIEW_CENTER_TILE_Y()) - m_effect_list[i]->m_dest_y);
						if (abs_x > abs_y) dist = abs_x;
						else dist = abs_y;
						lPan = -(((m_game->m_Camera.get_x() / 32) + VIEW_CENTER_TILE_X()) - m_effect_list[i]->m_dest_x);
						audio_manager::get().play_game_sound(sound_type::effect, 1, dist, lPan);
					}
					break;

				case EffectType::MASS_MAGIC_MISSILE_FLYING: // Mass-Magic-Missile
					CMisc::get_point(m_effect_list[i]->m_move_x
						, m_effect_list[i]->m_move_y
						, m_effect_list[i]->m_dest_x * 32
						, m_effect_list[i]->m_dest_y * 32
						, &m_effect_list[i]->m_move_x
						, &m_effect_list[i]->m_move_y
						, &m_effect_list[i]->m_error
						, 50);
					add_effect_impl(EffectType::BURST_SMALL, m_effect_list[i]->m_move_x + (rand() % 20) - 10, m_effect_list[i]->m_move_y + (rand() % 20) - 10, 0, 0, -1 * (rand() % 4));
					if ((abs(m_effect_list[i]->m_move_x - m_effect_list[i]->m_dest_x * 32) <= 2)
						&& (abs(m_effect_list[i]->m_move_y - m_effect_list[i]->m_dest_y * 32) <= 2))
					{	// JLE 0043132A
						delete m_effect_list[i];
						m_effect_list[i] = 0;
					}
					else
					{
						add_effect_impl(EffectType::MASS_MAGIC_MISSILE_AURA1, m_effect_list[i]->m_dest_x * 32 + 22, m_effect_list[i]->m_dest_y * 32 - 15, 0, 0, -7, 1);
						add_effect_impl(EffectType::MASS_MAGIC_MISSILE_AURA2, m_effect_list[i]->m_dest_x * 32 - 22, m_effect_list[i]->m_dest_y * 32 - 7, 0, 0, -7, 1);
						add_effect_impl(EffectType::MASS_MAGIC_MISSILE_AURA2, m_effect_list[i]->m_dest_x * 32 + 30, m_effect_list[i]->m_dest_y * 32 - 22, 0, 0, -5, 1);
						add_effect_impl(EffectType::MASS_MAGIC_MISSILE_AURA2, m_effect_list[i]->m_dest_x * 32 + 12, m_effect_list[i]->m_dest_y * 32 + 22, 0, 0, -3, 1);
					}
					break;

				case EffectType::BLIZZARD: // Blizzard
					if (m_effect_list[i]->m_frame > m_effect_list[i]->m_max_frame)
					{
						delete m_effect_list[i];
						m_effect_list[i] = 0;
					}
					else
					{
						add_effect_impl(EffectType::BLIZZARD_PROJECTILE, m_effect_list[i]->m_x, m_effect_list[i]->m_y,
							m_effect_list[i]->m_dest_x * 32 + (rand() % 120) - 60, m_effect_list[i]->m_dest_y * 32 + (rand() % 120) - 60, 0);
						abs_x = abs(((m_game->m_Camera.get_x() / 32) + VIEW_CENTER_TILE_X()) - m_effect_list[i]->m_dest_x);
						abs_y = abs(((m_game->m_Camera.get_y() / 32) + VIEW_CENTER_TILE_Y()) - m_effect_list[i]->m_dest_y);
						if (abs_x > abs_y) dist = abs_x;
						else dist = abs_y;
						lPan = -(((m_game->m_Camera.get_x() / 32) + VIEW_CENTER_TILE_X()) - m_effect_list[i]->m_dest_x);
						audio_manager::get().play_game_sound(sound_type::effect, 1, dist, lPan);
					}
					break;

				case EffectType::EARTH_SHOCK_WAVE: // Earth-Shock-Wave
					CMisc::get_point(m_effect_list[i]->m_move_x
						, m_effect_list[i]->m_move_y
						, m_effect_list[i]->m_dest_x * 32
						, m_effect_list[i]->m_dest_y * 32
						, &m_effect_list[i]->m_move_x
						, &m_effect_list[i]->m_move_y
						, &m_effect_list[i]->m_error
						, 40);
					add_effect_impl(EffectType::EARTH_SHOCK_WAVE_PARTICLE, m_effect_list[i]->m_move_x + (rand() % 30) - 15, m_effect_list[i]->m_move_y + (rand() % 30) - 15, 0, 0, 0, 1);
					add_effect_impl(EffectType::EARTH_SHOCK_WAVE_PARTICLE, m_effect_list[i]->m_move_x + (rand() % 20) - 10, m_effect_list[i]->m_move_y + (rand() % 20) - 10, 0, 0, 0, 0);
					if (m_effect_list[i]->m_frame >= m_effect_list[i]->m_max_frame)
					{
						delete m_effect_list[i];
						m_effect_list[i] = 0;
					}
					else
					{
						abs_x = abs(((m_game->m_Camera.get_x() / 32) + VIEW_CENTER_TILE_X()) - m_effect_list[i]->m_dest_x);
						abs_y = abs(((m_game->m_Camera.get_y() / 32) + VIEW_CENTER_TILE_Y()) - m_effect_list[i]->m_dest_y);
						if (abs_x > abs_y) dist = abs_x - 10;
						else dist = abs_y - 10;
						lPan = -(((m_game->m_Camera.get_x() / 32) + VIEW_CENTER_TILE_X()) - m_effect_list[i]->m_dest_x);
						audio_manager::get().play_game_sound(sound_type::effect, 1, dist, lPan);
					}
					break;

				case EffectType::SHOTSTAR_FALL_1:
					if (m_effect_list[i]->m_frame >= m_effect_list[i]->m_max_frame)
					{
						delete m_effect_list[i];
						m_effect_list[i] = 0;
					}
					else
					{
						add_effect_impl(EffectType::EXPLOSION_FIRE_APOCALYPSE, m_effect_list[i]->m_x + 40, m_effect_list[i]->m_y + 120, 0, 0, 0, 0);
						add_effect_impl(EffectType::CRACK_OBLIQUE, m_effect_list[i]->m_x - 10, m_effect_list[i]->m_y + 70, 0, 0, 0, 0);
						add_effect_impl(EffectType::CRACK_HORIZONTAL, m_effect_list[i]->m_x - 10, m_effect_list[i]->m_y + 75, 0, 0, 0, 0);
						add_effect_impl(EffectType::STEAMS_SMOKE, m_effect_list[i]->m_x - 7, m_effect_list[i]->m_y + 27, 0, 0, 0, 0);
						add_effect_impl(EffectType::SHOTSTAR_FALL_2, (rand() % (LOGICAL_WIDTH() / 4)) + LOGICAL_WIDTH() / 2, (rand() % (LOGICAL_HEIGHT() / 4)) + LOGICAL_HEIGHT() / 2, 0, 0, 0, 1);
						add_effect_impl(EffectType::SHOTSTAR_FALL_3, (rand() % (LOGICAL_WIDTH() / 4)) + LOGICAL_WIDTH() / 2, (rand() % (LOGICAL_HEIGHT() / 4)) + LOGICAL_HEIGHT() / 2, 0, 0, 0, 1);
						delete m_effect_list[i];
						m_effect_list[i] = 0;
					}
					break;

				case EffectType::SHOTSTAR_FALL_2:
					if (m_effect_list[i]->m_frame >= m_effect_list[i]->m_max_frame)
					{
						delete m_effect_list[i];
						m_effect_list[i] = 0;
					}
					else
					{
						add_effect_impl(EffectType::EXPLOSION_FIRE_APOCALYPSE, m_effect_list[i]->m_x + 110, m_effect_list[i]->m_y + 120, 0, 0, 0, 0);
						add_effect_impl(EffectType::CRACK_OBLIQUE, m_effect_list[i]->m_x - 10, m_effect_list[i]->m_y + 70, 0, 0, 0, 0);
						add_effect_impl(EffectType::CRACK_HORIZONTAL, m_effect_list[i]->m_x - 10, m_effect_list[i]->m_y + 75, 0, 0, 0, 0);
						add_effect_impl(EffectType::SHOTSTAR_FALL_3, (rand() % (LOGICAL_WIDTH() / 4)) + LOGICAL_WIDTH() / 2, (rand() % (LOGICAL_HEIGHT() / 4)) + LOGICAL_HEIGHT() / 2, 0, 0, 0, 1);
						delete m_effect_list[i];
						m_effect_list[i] = 0;
					}
					break;

				case EffectType::SHOTSTAR_FALL_3:
					if (m_effect_list[i]->m_frame >= m_effect_list[i]->m_max_frame)
					{
						delete m_effect_list[i];
						m_effect_list[i] = 0;
					}
					else
					{
						add_effect_impl(EffectType::EXPLOSION_FIRE_APOCALYPSE, m_effect_list[i]->m_x + 65, m_effect_list[i]->m_y + 120, 0, 0, 0, 0);
						add_effect_impl(EffectType::CRACK_OBLIQUE, m_effect_list[i]->m_x - 10, m_effect_list[i]->m_y + 70, 0, 0, 0, 0);
						add_effect_impl(EffectType::CRACK_HORIZONTAL, m_effect_list[i]->m_x - 10, m_effect_list[i]->m_y + 75, 0, 0, 0, 0);
						add_effect_impl(EffectType::STEAMS_SMOKE, m_effect_list[i]->m_x - 7, m_effect_list[i]->m_y + 27, 0, 0, 0, 0);
						delete m_effect_list[i];
						m_effect_list[i] = 0;
					}
					break;

				case EffectType::GATE_ROUND: // Gate round
					CMisc::get_point(m_effect_list[i]->m_move_x
						, m_effect_list[i]->m_move_y
						, m_effect_list[i]->m_dest_x * 32
						, m_effect_list[i]->m_dest_y * 32 - 40
						, &m_effect_list[i]->m_move_x
						, &m_effect_list[i]->m_move_y
						, &m_effect_list[i]->m_error
						, 10);
					if ((abs(m_effect_list[i]->m_move_x - m_effect_list[i]->m_dest_x * 32) <= 2)
						&& (abs(m_effect_list[i]->m_move_y - (m_effect_list[i]->m_dest_y * 32 - 40)) <= 2))
					{
						delete m_effect_list[i];
						m_effect_list[i] = 0;
					}
					break;

				case EffectType::SALMON_BURST: // Salmon burst (effect11s)
					CMisc::get_point(m_effect_list[i]->m_move_x
						, m_effect_list[i]->m_move_y
						, m_effect_list[i]->m_dest_x * 32
						, m_effect_list[i]->m_dest_y * 32
						, &m_effect_list[i]->m_move_x
						, &m_effect_list[i]->m_move_y
						, &m_effect_list[i]->m_error
						, 50);
					if ((abs(m_effect_list[i]->m_move_x - m_effect_list[i]->m_dest_x * 32) <= 2)
						&& (abs(m_effect_list[i]->m_move_y - (m_effect_list[i]->m_dest_y * 32 - 40)) <= 2))
					{
						delete m_effect_list[i];
						m_effect_list[i] = 0;
					}
					else
					{
						add_effect_impl(EffectType::SALMON_BURST_IMPACT, m_effect_list[i]->m_dest_x * 32, m_effect_list[i]->m_dest_y * 32, 0, 0, 0, 1);
						add_effect_impl(EffectType::SALMON_BURST_IMPACT, m_effect_list[i]->m_dest_x * 32 - 30, m_effect_list[i]->m_dest_y * 32 - 15, 0, 0, -7, 1);
						add_effect_impl(EffectType::SALMON_BURST_IMPACT, m_effect_list[i]->m_dest_x * 32 - 35, m_effect_list[i]->m_dest_y * 32 - 30, 0, 0, -5, 1);
						add_effect_impl(EffectType::SALMON_BURST_IMPACT, m_effect_list[i]->m_dest_x * 32 + 20, m_effect_list[i]->m_dest_y * 32 + 30, 0, 0, -3, 1);
						delete m_effect_list[i];
						m_effect_list[i] = 0;
					}
					break;

				case EffectType::BURST_SMALL:
				case EffectType::FOOTPRINT:
				case EffectType::RED_CLOUD_PARTICLES:
				case EffectType::IMPACT_BURST:
				case EffectType::FOOTPRINT_RAIN:
				case EffectType::MASS_MAGIC_MISSILE_AURA1: //
				case EffectType::MASS_MAGIC_MISSILE_AURA2: //
				case EffectType::SMOKE_DUST:
				case EffectType::SPARKLE_SMALL:
				case EffectType::PROTECTION_RING:
				case EffectType::HOLD_TWIST:
				case EffectType::STAR_TWINKLE:
				case EffectType::UNUSED_55:
				case EffectType::BUFF_EFFECT_LIGHT:
				case EffectType::FIRE_AURA_GROUND:
				case EffectType::FIRE_EXPLOSION_CRUSADE:
				case EffectType::WHITE_HALO:
				case EffectType::MS_FIRE_SMOKE:
				case EffectType::BLIZZARD_IMPACT:
				case EffectType::AURA_EFFECT_1:
				case EffectType::AURA_EFFECT_2:
				case EffectType::ICE_GOLEM_EFFECT_1:
				case EffectType::ICE_GOLEM_EFFECT_2:
				case EffectType::ICE_GOLEM_EFFECT_3:
				case EffectType::EARTH_SHOCK_WAVE_PARTICLE: //
				case EffectType::GATE_APOCALYPSE: //

				case EffectType::HEAL:
				case EffectType::CREATE_FOOD:
				case EffectType::STAMINA_DRAIN:
				case EffectType::RECALL:
				case EffectType::DEFENSE_SHIELD:
				case EffectType::GREAT_HEAL:
				case EffectType::UNUSED_122:
				case EffectType::STAMINA_RECOVERY: // Stamina Rec
				case EffectType::PROTECT_FROM_NM:
				case EffectType::HOLD_PERSON:
				case EffectType::POSSESSION:
				case EffectType::POISON:
				case EffectType::GREAT_STAMINA_RECOVERY: // Gr Stamina Rec
				case EffectType::SUMMON_CREATURE:
				case EffectType::INVISIBILITY:
				case EffectType::PROTECT_FROM_MAGIC:
				case EffectType::DETECT_INVISIBILITY:
				case EffectType::PARALYZE:
				case EffectType::CURE:
				case EffectType::CONFUSE_LANGUAGE:
				case EffectType::GREAT_DEFENSE_SHIELD:
				case EffectType::BERSERK: // Berserk : Cirlcle 6 magic
				case EffectType::POLYMORPH: // Polymorph
				case EffectType::MASS_POISON:
				case EffectType::CONFUSION:
				case EffectType::ABSOLUTE_MAGIC_PROTECTION:
				case EffectType::ARMOR_BREAK:
				case EffectType::MASS_CONFUSION:
				case EffectType::CANCELLATION: //
				case EffectType::ILLUSION_MOVEMENT: //

				case EffectType::ILLUSION:
				case EffectType::INHIBITION_CASTING: //
				case EffectType::MAGIC_DRAIN: // EP's Magic Drain
				case EffectType::MASS_ILLUSION:
				case EffectType::ICE_RAIN_VARIANT_1:
				case EffectType::ICE_RAIN_VARIANT_2:
				case EffectType::RESURRECTION:
				case EffectType::MASS_ILLUSION_MOVEMENT:
				case EffectType::MAGE_HERO_SET: // Mage hero effect
				case EffectType::WAR_HERO_SET: // War hero effect
				case EffectType::MASS_MM_AURA_CASTER: // Snoopy: Moved for new spells: Caster aura for Mass MagicMissile
					if (m_effect_list[i]->m_frame > m_effect_list[i]->m_max_frame)
					{
						delete m_effect_list[i];
						m_effect_list[i] = 0;
					}
					break;
				}
			}
		}

}
