// effect_draw.cpp: draw_effects implementation
//
//////////////////////////////////////////////////////////////////////

#include "effect_manager.h"
#include "game.h"
#include "sprite.h"
#include "effect.h"
#include "global_def.h"
#include "misc.h"
#include "weather_manager.h"

void effect_manager::draw_effects_impl()
{
	int i, dX, dY, dvalue, tX, tY, rX, rY, x2, y2, x3, y3, x4, y4, x5, y5, err;
	char  temp_frame;
	uint32_t time = m_game->m_cur_time;
	for (i = 0; i < game_limits::max_effects; i++)
		if ((m_effect_list[i] != 0) && (m_effect_list[i]->m_frame >= 0))
		{
			switch (m_effect_list[i]->m_type) {
			case EffectType::NORMAL_HIT: // Normal hit
				if (m_effect_list[i]->m_frame < 0) break;
				dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
				(*m_effect_sprites)[8]->draw(dX, dY, m_effect_list[i]->m_frame, hb::shared::sprite::DrawParams::additive_no_color_key());
				break;

			case EffectType::ARROW_FLYING: // Arrow flying
				dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
				temp_frame = (m_effect_list[i]->m_dir - 1) * 2;
				if (temp_frame < 0) break;
				(*m_effect_sprites)[7]->draw(dX, dY, temp_frame);
				break;

			case EffectType::GOLD_DROP: // gold
				temp_frame = m_effect_list[i]->m_frame;
				dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
				(*m_effect_sprites)[1]->draw(dX, dY - 40, temp_frame);
				break;

			case EffectType::FIREBALL_EXPLOSION: // FireBall Fire Explosion
				temp_frame = m_effect_list[i]->m_frame;
				if (temp_frame < 0) break;
				dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
				dvalue = (temp_frame - 8) * (-5);
				if (temp_frame < 7)
					(*m_effect_sprites)[3]->draw(dX, dY, temp_frame, hb::shared::sprite::DrawParams::additive_no_color_key());
				else (*m_effect_sprites)[3]->draw(dX, dY, temp_frame, hb::shared::sprite::DrawParams::additive_tinted(dvalue, dvalue, dvalue));
				break;

			case EffectType::ENERGY_BOLT_EXPLOSION:	 // Energy Bolt
			case EffectType::LIGHTNING_ARROW_EXPLOSION: // Lightning Arrow
				temp_frame = m_effect_list[i]->m_frame;
				if (temp_frame < 0) break;
				dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
				dvalue = (temp_frame - 7) * (-6);
				if (temp_frame < 6)
					(*m_effect_sprites)[6]->draw(dX, dY, temp_frame, hb::shared::sprite::DrawParams::additive_no_color_key());
				else (*m_effect_sprites)[6]->draw(dX, dY, temp_frame, hb::shared::sprite::DrawParams::additive_tinted(dvalue, dvalue, dvalue)); // RGB2
				break;

			case EffectType::MAGIC_MISSILE_EXPLOSION: // Magic Missile Explosion
				temp_frame = m_effect_list[i]->m_frame;
				dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
				dvalue = (temp_frame - 4) * (-3);
				if (temp_frame < 4)
					(*m_effect_sprites)[6]->draw(dX, dY, temp_frame, hb::shared::sprite::DrawParams::additive_no_color_key());
				else (*m_effect_sprites)[6]->draw(dX, dY, temp_frame, hb::shared::sprite::DrawParams::additive_tinted(dvalue, dvalue, dvalue)); // RGB2
				break;

			case EffectType::BURST_SMALL: // Burst
				temp_frame = m_effect_list[i]->m_frame;
				temp_frame = 4 - temp_frame;
				if (temp_frame < 0) break;
				dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
				(*m_effect_sprites)[11]->draw(dX, dY, temp_frame, hb::shared::sprite::DrawParams::additive_no_color_key());
				break;

			case EffectType::BURST_MEDIUM: // Burst
				temp_frame = (rand() % 5);
				dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
				(*m_effect_sprites)[11]->draw(dX, dY, temp_frame, hb::shared::sprite::DrawParams::additive_no_color_key());
				break;

			case EffectType::BURST_SMALL_GRENADE: // pt grenat
				temp_frame = (rand() % 5) + 5;
				dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
				(*m_effect_sprites)[11]->draw(dX, dY, temp_frame, hb::shared::sprite::DrawParams::average());
				break;

			case EffectType::BURST_LARGE: // Burst
				temp_frame = (rand() % 6) + 10;
				dvalue = (m_effect_list[i]->m_frame - 4) * (-3);
				dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
				if (temp_frame < 4)
					(*m_effect_sprites)[11]->draw(dX, dY, temp_frame, hb::shared::sprite::DrawParams::additive_no_color_key());
				else //(*m_effect_sprites)[11]->draw(dX, dY, temp_frame, hb::shared::sprite::DrawParams::additive_tinted(dvalue, dvalue, dvalue)); // RGB2
				//
					(*m_effect_sprites)[11]->draw(dX, dY, temp_frame, hb::shared::sprite::DrawParams::additive(0.5f));
				break;

			case EffectType::BUBBLES_DRUNK:
				temp_frame = m_effect_list[i]->m_frame;
				if (temp_frame < 0) break;
				dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
				if (temp_frame < 13)
				{
					(*m_effect_sprites)[11]->draw(dX, dY, 25 + (temp_frame / 5), hb::shared::sprite::DrawParams::additive_no_color_key());
				}
				else
				{
					(*m_effect_sprites)[11]->draw(dX, dY, (8 + temp_frame), hb::shared::sprite::DrawParams::additive_no_color_key());
				}
				break;

			case EffectType::FOOTPRINT: // Traces of pas (terrain sec)
				if (m_effect_list[i]->m_frame < 0) break;
				dX = m_effect_list[i]->m_move_x - m_game->m_Camera.get_x();
				dY = m_effect_list[i]->m_move_y - m_game->m_Camera.get_y();
				(*m_effect_sprites)[11]->draw(dX, dY, (28 + m_effect_list[i]->m_frame), hb::shared::sprite::DrawParams::additive_no_color_key(0.5f));
				break;

			case EffectType::RED_CLOUD_PARTICLES: // petits nuages rouges
				temp_frame = m_effect_list[i]->m_frame;
				if (temp_frame < 0) break;
				dX = m_effect_list[i]->m_move_x - m_game->m_Camera.get_x();
				dY = m_effect_list[i]->m_move_y - m_game->m_Camera.get_y();
				(*m_effect_sprites)[11]->draw(dX, dY, (33 + temp_frame), hb::shared::sprite::DrawParams::additive_no_color_key(0.5f));
				break;

			case EffectType::PROJECTILE_GENERIC: //
				dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
				(*m_effect_sprites)[0]->draw(dX, dY, 0, hb::shared::sprite::DrawParams::additive_no_color_key());
				break;

			case EffectType::ICE_STORM: //test
				dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
				temp_frame = 39 + (rand() % 3) * 3 + (rand() % 3);
				(*m_effect_sprites)[11]->draw(dX, dY, temp_frame, hb::shared::sprite::DrawParams::additive_no_color_key());
				dX = (m_effect_list[i]->m_move_x2) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_move_y2) - m_game->m_Camera.get_y();
				(*m_effect_sprites)[11]->draw(dX, dY, temp_frame, hb::shared::sprite::DrawParams::additive_no_color_key(0.5f));
				break;

			case EffectType::IMPACT_BURST: //
				dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
				temp_frame = m_effect_list[i]->m_frame;
				if (temp_frame < 0) break;
				(*m_effect_sprites)[18]->draw(dX, dY, temp_frame, hb::shared::sprite::DrawParams::additive_no_color_key(0.7f));
				break;

			case EffectType::CRITICAL_STRIKE_1: // critical hit
			case EffectType::CRITICAL_STRIKE_2:
			case EffectType::CRITICAL_STRIKE_3:
			case EffectType::CRITICAL_STRIKE_4:
			case EffectType::CRITICAL_STRIKE_5:
			case EffectType::CRITICAL_STRIKE_6:
			case EffectType::CRITICAL_STRIKE_7:
			case EffectType::CRITICAL_STRIKE_8: // Critical strike with a weapon
				dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
				(*m_effect_sprites)[8]->draw(dX, dY, 1, hb::shared::sprite::DrawParams::additive_no_color_key());
				break;

			case EffectType::MASS_FIRE_STRIKE_CALLER1: // Mass-Fire-Strike
				dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
				temp_frame = m_effect_list[i]->m_frame;
				if (temp_frame < 0) break;
				(*m_effect_sprites)[14]->draw(dX, dY, temp_frame, hb::shared::sprite::DrawParams::additive_no_color_key());
				break;

			case EffectType::MASS_FIRE_STRIKE_CALLER3: // Mass-Fire-Strike
				dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
				temp_frame = m_effect_list[i]->m_frame;
				if (temp_frame < 0) break;
				(*m_effect_sprites)[15]->draw(dX, dY, temp_frame, hb::shared::sprite::DrawParams::additive_no_color_key());
				break;

			case EffectType::FOOTPRINT_RAIN: // Trace of pas  (raining weather)
				dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
				temp_frame = m_effect_list[i]->m_frame + 20;
				if (temp_frame < 0) break;
				(*m_effect_sprites)[11]->draw(dX, dY, temp_frame, hb::shared::sprite::DrawParams::additive_no_color_key());
				break;

			case EffectType::IMPACT_EFFECT: //
				dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
				temp_frame = m_effect_list[i]->m_frame;
				if (temp_frame < 0) break;
				(*m_effect_sprites)[19]->draw(dX, dY, temp_frame, hb::shared::sprite::DrawParams::additive_no_color_key());
				break;

			case EffectType::BLOODY_SHOCK_STRIKE: // absent (220 et 351)
				break;

			case EffectType::MASS_MAGIC_MISSILE_AURA1: // Snoopy: Added if (m_effect_list[i]->m_frame < 0) break;
				dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
				temp_frame = m_effect_list[i]->m_frame;
				(*m_effect_sprites)[6]->draw(dX - 30, dY - 18, temp_frame, hb::shared::sprite::DrawParams::additive_no_color_key());
				break;

			case EffectType::MASS_MAGIC_MISSILE_AURA2: // Snoopy: Added if (m_effect_list[i]->m_frame < 0) break;
				dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
				temp_frame = m_effect_list[i]->m_frame;
				(*m_effect_sprites)[97]->draw(dX, dY, temp_frame, hb::shared::sprite::DrawParams::additive_no_color_key());
				break;

			case EffectType::CHILL_WIND_IMPACT:
				temp_frame = m_effect_list[i]->m_frame;
				if (temp_frame < 0) break;
				dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
				(*m_effect_sprites)[20]->draw(dX, dY, temp_frame, hb::shared::sprite::DrawParams::additive_no_color_key(0.5f)); // 20
				break;

			case EffectType::ICE_STRIKE_VARIANT_1: // Large Type 1, 2, 3, 4
			case EffectType::ICE_STRIKE_VARIANT_2:
			case EffectType::ICE_STRIKE_VARIANT_3:
			case EffectType::ICE_STRIKE_VARIANT_4:
			case EffectType::ICE_STRIKE_VARIANT_5: // Small Type 1, 2
			case EffectType::ICE_STRIKE_VARIANT_6:
				dX = (m_effect_list[i]->m_x) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_y) - m_game->m_Camera.get_y();
				(*m_effect_sprites)[21]->draw(dX, dY, 48, hb::shared::sprite::DrawParams::fade());
				temp_frame = m_effect_list[i]->m_frame;
				if (temp_frame < 0) break;
				dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
				if ((8 * (static_cast<int>(m_effect_list[i]->m_type) - 41) + temp_frame) < (8 * (static_cast<int>(m_effect_list[i]->m_type) - 41) + 7))
				{
					dvalue = -8 * (6 - temp_frame);
					(*m_effect_sprites)[21]->draw(dX, dY, 8 * (static_cast<size_t>(m_effect_list[i]->m_type) - 41) + temp_frame, hb::shared::sprite::DrawParams::additive_tinted(dvalue, dvalue, dvalue)); // RGB2
				}
				else
				{
					if ((temp_frame - 5) >= 8) temp_frame = ((temp_frame - 5) - 8) + 5;
					(*m_effect_sprites)[21]->draw(dX, dY, 8 * (static_cast<size_t>(m_effect_list[i]->m_type) - 41) + (temp_frame - 5));
				}
				break;

			case EffectType::BLIZZARD_VARIANT_1:
			case EffectType::BLIZZARD_VARIANT_2:
			case EffectType::BLIZZARD_VARIANT_3: // Blizzard
				dX = (m_effect_list[i]->m_x) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_y) - m_game->m_Camera.get_y();
				(*m_effect_sprites)[static_cast<size_t>(m_effect_list[i]->m_type) - 1]->draw(dX, dY, 0, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
				temp_frame = m_effect_list[i]->m_frame;
				if (temp_frame < 0) break;
				dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
				if (temp_frame < 7) {
					dvalue = -8 * (6 - temp_frame);
					(*m_effect_sprites)[static_cast<size_t>(m_effect_list[i]->m_type) - 1]->draw(dX, dY, temp_frame + 1, hb::shared::sprite::DrawParams::additive_tinted(dvalue, dvalue, dvalue));
				}
				else {
					if (temp_frame >= 8) temp_frame = temp_frame % 8;
					(*m_effect_sprites)[static_cast<size_t>(m_effect_list[i]->m_type) - 1]->draw(dX, dY, temp_frame + 1);
				}
				break;

			case EffectType::SMOKE_DUST:
				temp_frame = m_effect_list[i]->m_frame;
				if (temp_frame < 0) break;
				dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();

				if (temp_frame <= 6) {
					dvalue = 0;
					(*m_effect_sprites)[22]->draw(dX, dY, temp_frame, hb::shared::sprite::DrawParams::additive_tinted(dvalue, dvalue, dvalue));	// RGB2
				}
				else {
					dvalue = -5 * (temp_frame - 6);
					(*m_effect_sprites)[22]->draw(dX, dY, 6, hb::shared::sprite::DrawParams::additive_tinted(dvalue, dvalue, dvalue)); // RGB2
				}
				break;

			case EffectType::SPARKLE_SMALL: //
				temp_frame = m_effect_list[i]->m_frame + 11; //15
				if (temp_frame < 0) break;
				dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
				(*m_effect_sprites)[28]->draw(dX, dY, temp_frame, hb::shared::sprite::DrawParams::additive(0.25f)); //20
				break;


			case EffectType::PROTECTION_RING: // Protection Ring - disabled (no visual effect)
				break;


			case EffectType::HOLD_TWIST: // Hold Twist
				temp_frame = m_effect_list[i]->m_frame;
				if (temp_frame < 0) break;
				dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
				(*m_effect_sprites)[25]->draw(dX, dY, temp_frame, hb::shared::sprite::DrawParams::additive_no_color_key()); //25
				break;

			case EffectType::STAR_TWINKLE: //  star twingkling (effect armes brillantes)
				temp_frame = m_effect_list[i]->m_frame;
				if (temp_frame < 0) temp_frame = 0;
				dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
				(*m_effect_sprites)[28]->draw(dX, dY, temp_frame, hb::shared::sprite::DrawParams::additive(0.5f));
				break;

			case EffectType::UNUSED_55: //
				temp_frame = m_effect_list[i]->m_frame;
				if (temp_frame < 0) temp_frame = 0;
				dX = (m_effect_list[i]->m_move_x);
				dY = (m_effect_list[i]->m_move_y);
				(*m_effect_sprites)[28]->draw(dX, dY, temp_frame, hb::shared::sprite::DrawParams::additive_no_color_key());
				break;

			case EffectType::MASS_CHILL_WIND: // Mass-Chill-Wind
				temp_frame = m_effect_list[i]->m_frame;
				if (temp_frame < 0) temp_frame = 0;
				dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
				(*m_effect_sprites)[29]->draw(dX, dY, temp_frame, hb::shared::sprite::DrawParams::additive_no_color_key(0.5f));
				break;

			case EffectType::BUFF_EFFECT_LIGHT:  // absent (220 et 351)
				break;

			case EffectType::METEOR_FLYING:  //
			case EffectType::METEOR_STRIKE_DESCENDING: // MS
				temp_frame = m_effect_list[i]->m_frame;
				if (temp_frame < 0) break;
				if (temp_frame > 4)
				{
					temp_frame = temp_frame / 4;
				}
				if (temp_frame >= 0)
				{
					dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
					dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
					(*m_effect_sprites)[31]->draw(dX, dY, 15 + temp_frame);
					(*m_effect_sprites)[31]->draw(dX, dY, temp_frame, hb::shared::sprite::DrawParams::additive(0.5f));
				}
				break;

			case EffectType::FIRE_AURA_GROUND: // Fire aura on ground (crueffect1, 1)
				temp_frame = m_effect_list[i]->m_frame;
				if (temp_frame < 0) break;
				dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
				(*m_effect_sprites)[32]->draw(dX, dY, temp_frame, hb::shared::sprite::DrawParams::additive_no_color_key());
				break;

			case EffectType::METEOR_IMPACT: // MS strike
				temp_frame = m_effect_list[i]->m_frame;
				if (temp_frame < 0) break;
				if (temp_frame > 0)
				{
					temp_frame = temp_frame - 1;
					dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
					dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
					(*m_effect_sprites)[31]->draw(dX, dY, 20 + temp_frame, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
				}
				break;

			case EffectType::FIRE_EXPLOSION_CRUSADE: // Fire explosion (crueffect1, 2)
				temp_frame = m_effect_list[i]->m_frame;
				if (temp_frame < 0) break;
				dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
				(*m_effect_sprites)[33]->draw(dX, dY, temp_frame, hb::shared::sprite::DrawParams::additive_no_color_key());
				break;

			case EffectType::WHITE_HALO: // Whitish halo effect
				temp_frame = m_effect_list[i]->m_frame;
				if (temp_frame < 0) break;
				dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
				(*m_effect_sprites)[34]->draw(dX, dY, temp_frame, hb::shared::sprite::DrawParams::additive_no_color_key());
				break;

			case EffectType::MS_CRUSADE_CASTING: // MS from crusade striking
				temp_frame = m_effect_list[i]->m_frame;
				if (temp_frame < 0) break;
				temp_frame = temp_frame / 6;
				dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
				(*m_effect_sprites)[31]->draw(dX, dY, 20 + temp_frame, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
				break;

			case EffectType::MS_CRUSADE_EXPLOSION: // MS explodes on the ground
				temp_frame = m_effect_list[i]->m_frame;
				if (temp_frame < 0) break;
				dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
				(*m_effect_sprites)[39]->draw(dX, dY, temp_frame, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
				(*m_effect_sprites)[39]->draw(dX, dY, temp_frame, hb::shared::sprite::DrawParams::additive_no_color_key());
				break;

			case EffectType::MS_FIRE_SMOKE: // MS fire with smoke
				temp_frame = m_effect_list[i]->m_frame;
				if (temp_frame < 0) break;
				dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
				switch (rand() % 3) {
				case 0: (*m_effect_sprites)[0]->draw(dX, dY + 20, 1, hb::shared::sprite::DrawParams::additive_no_color_key(0.25f)); break;
				case 1: (*m_effect_sprites)[0]->draw(dX, dY + 20, 1, hb::shared::sprite::DrawParams::additive_no_color_key()); break;
				case 2: (*m_effect_sprites)[0]->draw(dX, dY + 20, 1, hb::shared::sprite::DrawParams::additive_no_color_key(0.7f)); break;
				}
				(*m_effect_sprites)[35]->draw(dX, dY, temp_frame / 3, hb::shared::sprite::DrawParams::additive_no_color_key(0.7f));
				break;

			case EffectType::WORM_BITE: // worm-bite
				temp_frame = m_effect_list[i]->m_frame;
				if (temp_frame < 0) break;
				dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
				if (temp_frame <= 11)
				{
					(*m_effect_sprites)[40]->draw(dX, dY, temp_frame);
					(*m_effect_sprites)[41]->draw(dX, dY, temp_frame, hb::shared::sprite::DrawParams::additive_no_color_key(0.5f));
					(*m_effect_sprites)[44]->draw(dX - 2, dY - 3, temp_frame, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
					(*m_effect_sprites)[44]->draw(dX - 4, dY - 3, temp_frame, hb::shared::sprite::DrawParams::additive_no_color_key());
				}
				else
				{
					switch (temp_frame) {
					case 12:
					case 13:
					case 14: (*m_effect_sprites)[40]->draw(dX, dY, 11); break;
					case 15: (*m_effect_sprites)[40]->draw(dX, dY, 11, hb::shared::sprite::DrawParams::additive_no_color_key(0.7f)); break;
					case 16: (*m_effect_sprites)[40]->draw(dX, dY, 11, hb::shared::sprite::DrawParams::additive_no_color_key(0.5f)); break;
					case 17: (*m_effect_sprites)[40]->draw(dX, dY, 11, hb::shared::sprite::DrawParams::additive_no_color_key(0.25f)); break;
					}
				}
				break;

			case EffectType::LIGHT_EFFECT_1: // identique au cas 70
				temp_frame = m_effect_list[i]->m_frame;
				if (temp_frame < 0) break;
				dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
				(*m_effect_sprites)[42]->draw(dX, dY, temp_frame, hb::shared::sprite::DrawParams::additive_no_color_key());
				break;

			case EffectType::LIGHT_EFFECT_2: // identique au cas 69
				temp_frame = m_effect_list[i]->m_frame;
				if (temp_frame < 0) break;
				dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
				(*m_effect_sprites)[43]->draw(dX, dY, temp_frame, hb::shared::sprite::DrawParams::additive_no_color_key());
				break;

			case EffectType::BLIZZARD_PROJECTILE: // absent v220 et v351
				break;

			case EffectType::BLIZZARD_IMPACT: // Blizzard
				temp_frame = m_effect_list[i]->m_frame;
				if (temp_frame < 0) break;
				dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
				if (temp_frame <= 8)
				{
					dvalue = 0;
					(*m_effect_sprites)[51]->draw(dX, dY, temp_frame, hb::shared::sprite::DrawParams::additive_tinted(dvalue, dvalue, dvalue));
				}
				else
				{
					dvalue = -1 * (temp_frame - 8);
					(*m_effect_sprites)[51]->draw(dX, dY, 8, hb::shared::sprite::DrawParams::additive_tinted(dvalue, dvalue, dvalue));	// RGB2
				}
				break;

			case EffectType::AURA_EFFECT_1: // absent v220 et v351
			case EffectType::AURA_EFFECT_2: // absent v220 et v351
			case EffectType::ICE_GOLEM_EFFECT_1: // absent v220 et v351
			case EffectType::ICE_GOLEM_EFFECT_2: // absent v220 et v351
			case EffectType::ICE_GOLEM_EFFECT_3: // absent v220 et v351
				break;

			case EffectType::EARTH_SHOCK_WAVE_PARTICLE:
			case EffectType::EARTH_SHOCK_WAVE: // Earth-Shock-Wave
				temp_frame = m_effect_list[i]->m_frame;
				if (temp_frame < 0) break;
				dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
				(*m_effect_sprites)[91]->draw(dX, dY, temp_frame); //Nbe d'arguments modifi�s ds la 351....
				(*m_effect_sprites)[92]->draw(dX, dY, temp_frame, hb::shared::sprite::DrawParams::additive(0.5f));
				break;

			case EffectType::STORM_BLADE: // Snoopy: Added StormBlade
				dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
				temp_frame = m_effect_list[i]->m_frame;
				(*m_effect_sprites)[100]->draw(dX + 70, dY + 70, temp_frame, hb::shared::sprite::DrawParams::additive_no_color_key());
				break;

			case EffectType::GATE_APOCALYPSE: // Gate (apocalypse)
				temp_frame = m_effect_list[i]->m_frame;
				(*m_effect_sprites)[101]->draw(LOGICAL_WIDTH() / 2, LOGICAL_HEIGHT(), temp_frame, hb::shared::sprite::DrawParams::additive_no_color_key());
				break;

			case EffectType::MAGIC_MISSILE_FLYING: // Magic Missile
				dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
				(*m_effect_sprites)[0]->draw(dX, dY, 0, hb::shared::sprite::DrawParams::additive_no_color_key());
				break;

			case EffectType::HEAL: // Heal
			case EffectType::GREAT_HEAL: // Great-Heal
				temp_frame = m_effect_list[i]->m_frame;
				if (temp_frame < 0) break;
				dX = (m_effect_list[i]->m_dest_x * 32) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_dest_y * 32) - m_game->m_Camera.get_y();
				(*m_effect_sprites)[50]->draw(dX, dY, m_effect_list[i]->m_frame, hb::shared::sprite::DrawParams::additive_no_color_key());
				break;

			case EffectType::CREATE_FOOD: // Create Food
			case EffectType::PROTECT_FROM_NM: // Protection from N.M
			case EffectType::HOLD_PERSON: // Hold-Person
			case EffectType::POSSESSION: // Possession
			case EffectType::POISON: // Poison
			case EffectType::PROTECT_FROM_MAGIC: // Protect-From-Magic
			case EffectType::DETECT_INVISIBILITY: // Detect-Invisibility
			case EffectType::PARALYZE: // Paralyze
			case EffectType::CURE: // Cure
			case EffectType::CONFUSE_LANGUAGE: // Confuse Language
			case EffectType::POLYMORPH: // Polymorph
			case EffectType::MASS_POISON: // Mass-Poison
			case EffectType::CONFUSION: // Confusion
			case EffectType::MASS_CONFUSION: // Mass-Confusion
				temp_frame = m_effect_list[i]->m_frame;
				if (temp_frame < 0) break;
				dX = (m_effect_list[i]->m_dest_x * 32) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_dest_y * 32) - m_game->m_Camera.get_y();
				dvalue = (temp_frame - 5) * (-5);
				if (temp_frame < 5)
					(*m_effect_sprites)[4]->draw(dX, dY, m_effect_list[i]->m_frame, hb::shared::sprite::DrawParams::additive_no_color_key());
				else (*m_effect_sprites)[4]->draw(dX, dY, m_effect_list[i]->m_frame, hb::shared::sprite::DrawParams::additive_tinted(dvalue, dvalue, dvalue)); // RGB2
				break;

			case EffectType::ENERGY_BOLT_FLYING: // Energy-Bolt
				dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
				(*m_effect_sprites)[0]->draw(dX, dY, 2 + (rand() % 4), hb::shared::sprite::DrawParams::additive_no_color_key());
				break;

			case EffectType::STAMINA_DRAIN: // Staminar Drain
				temp_frame = m_effect_list[i]->m_frame;
				if (temp_frame < 0) break;
				dX = (m_effect_list[i]->m_dest_x * 32) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_dest_y * 32) - m_game->m_Camera.get_y();
				(*m_effect_sprites)[49]->draw(dX, dY, m_effect_list[i]->m_frame, hb::shared::sprite::DrawParams::additive_no_color_key());
				break;

			case EffectType::RECALL: // Recall
			case EffectType::SUMMON_CREATURE: // Summon-Creature
			case EffectType::INVISIBILITY: // Invisibility
			case EffectType::HASTE: // Haste
				temp_frame = m_effect_list[i]->m_frame;
				if (temp_frame < 0) break;
				dX = (m_effect_list[i]->m_dest_x * 32) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_dest_y * 32) - m_game->m_Camera.get_y();
				(*m_effect_sprites)[52]->draw(dX, dY, m_effect_list[i]->m_frame, hb::shared::sprite::DrawParams::additive_no_color_key());
				break;

			case EffectType::DEFENSE_SHIELD: // Defense Shield
				temp_frame = m_effect_list[i]->m_frame;
				if (temp_frame < 0) break;
				dX = (m_effect_list[i]->m_dest_x * 32) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_dest_y * 32) - m_game->m_Camera.get_y();
				dvalue = (temp_frame - 5) * (-5);
				if (temp_frame < 6)
					(*m_effect_sprites)[62]->draw(dX, dY, m_effect_list[i]->m_frame, hb::shared::sprite::DrawParams::additive_no_color_key());
				else (*m_effect_sprites)[62]->draw(dX, dY, m_effect_list[i]->m_frame, hb::shared::sprite::DrawParams::additive_tinted(dvalue, dvalue, dvalue)); // RGB2
				break;

			case EffectType::FIRE_BALL_FLYING: // Fire Ball
			case EffectType::FIRE_STRIKE_FLYING: // Fire Strike
			case EffectType::MASS_FIRE_STRIKE_FLYING: // Mass-Fire-Strike
			case EffectType::SALMON_BURST: //
				dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
				temp_frame = (m_effect_list[i]->m_dir - 1) * 4 + (rand() % 4);
				if (temp_frame < 0) break;
				(*m_effect_sprites)[5]->draw(dX, dY, temp_frame, hb::shared::sprite::DrawParams::additive_no_color_key());
				break;

			case EffectType::UNUSED_122: // Absent v220 et 351
				break;

			case EffectType::STAMINA_RECOVERY: // Staminar-Recovery
			case EffectType::GREAT_STAMINA_RECOVERY: // Great-Staminar-Recovery
				temp_frame = m_effect_list[i]->m_frame;
				if (temp_frame < 0) break;
				dX = (m_effect_list[i]->m_dest_x * 32) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_dest_y * 32) - m_game->m_Camera.get_y();
				(*m_effect_sprites)[56]->draw(dX, dY, m_effect_list[i]->m_frame, hb::shared::sprite::DrawParams::additive_no_color_key());
				break;

			case EffectType::LIGHTNING_ARROW_FLYING: // Lightning Arrow
				dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
				tX = (m_effect_list[i]->m_move_x2) - m_game->m_Camera.get_x();
				tY = (m_effect_list[i]->m_move_y2) - m_game->m_Camera.get_y();
				err = 0;
				CMisc::get_point(dX, dY, tX, tY, &rX, &rY, &err, 15);
				CMisc::get_point(dX, dY, tX, tY, &x2, &y2, &err, 30);
				CMisc::get_point(dX, dY, tX, tY, &x3, &y3, &err, 45);
				CMisc::get_point(dX, dY, tX, tY, &x4, &y4, &err, 60);
				CMisc::get_point(dX, dY, tX, tY, &x5, &y5, &err, 75);
				temp_frame = (m_effect_list[i]->m_dir - 1) * 4 + (rand() % 4);
				if (temp_frame < 0) break;
				(*m_effect_sprites)[10]->draw(x5, y5, temp_frame, hb::shared::sprite::DrawParams::additive_no_color_key(0.25f));
				temp_frame = (m_effect_list[i]->m_dir - 1) * 4 + (rand() % 4);
				if (temp_frame < 0) break;
				(*m_effect_sprites)[10]->draw(x4, y4, temp_frame, hb::shared::sprite::DrawParams::additive_no_color_key(0.25f));
				temp_frame = (m_effect_list[i]->m_dir - 1) * 4 + (rand() % 4);
				if (temp_frame < 0) break;
				(*m_effect_sprites)[10]->draw(x3, y3, temp_frame, hb::shared::sprite::DrawParams::additive_no_color_key());
				temp_frame = (m_effect_list[i]->m_dir - 1) * 4 + (rand() % 4);
				if (temp_frame < 0) break;
				(*m_effect_sprites)[10]->draw(x2, y2, temp_frame, hb::shared::sprite::DrawParams::additive_no_color_key());
				temp_frame = (m_effect_list[i]->m_dir - 1) * 4 + (rand() % 4);
				if (temp_frame < 0) break;
				(*m_effect_sprites)[10]->draw(rX, rY, temp_frame, hb::shared::sprite::DrawParams::additive_no_color_key(0.7f));
				temp_frame = (m_effect_list[i]->m_dir - 1) * 4 + (rand() % 4);
				if (temp_frame < 0) break;
				(*m_effect_sprites)[10]->draw(dX, dY, temp_frame, hb::shared::sprite::DrawParams::additive(0.5f));
				break;

			case EffectType::LIGHTNING: // Lightning
				weather_manager::get().draw_thunder_effect(m_effect_list[i]->m_dest_x * 32 - m_game->m_Camera.get_x(), m_effect_list[i]->m_dest_y * 32 - m_game->m_Camera.get_y() - LOGICAL_WIDTH(),
					m_effect_list[i]->m_dest_x * 32 - m_game->m_Camera.get_x(), m_effect_list[i]->m_dest_y * 32 - m_game->m_Camera.get_y(),
					m_effect_list[i]->m_render_x, m_effect_list[i]->m_render_y, 1);
				weather_manager::get().draw_thunder_effect(m_effect_list[i]->m_dest_x * 32 - m_game->m_Camera.get_x(), m_effect_list[i]->m_dest_y * 32 - m_game->m_Camera.get_y() - LOGICAL_WIDTH(),
					m_effect_list[i]->m_dest_x * 32 - m_game->m_Camera.get_x(), m_effect_list[i]->m_dest_y * 32 - m_game->m_Camera.get_y(),
					m_effect_list[i]->m_render_x + 4, m_effect_list[i]->m_render_y + 2, 2);
				weather_manager::get().draw_thunder_effect(m_effect_list[i]->m_dest_x * 32 - m_game->m_Camera.get_x(), m_effect_list[i]->m_dest_y * 32 - m_game->m_Camera.get_y() - LOGICAL_WIDTH(),
					m_effect_list[i]->m_dest_x * 32 - m_game->m_Camera.get_x(), m_effect_list[i]->m_dest_y * 32 - m_game->m_Camera.get_y(),
					m_effect_list[i]->m_render_x - 2, m_effect_list[i]->m_render_y - 2, 2);
				break;

			case EffectType::GREAT_DEFENSE_SHIELD: // Great-Defense-Shield
				temp_frame = m_effect_list[i]->m_frame;
				if (temp_frame < 0) break;
				dX = (m_effect_list[i]->m_dest_x * 32) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_dest_y * 32) - m_game->m_Camera.get_y();
				dvalue = (temp_frame - 5) * (-5);
				if (temp_frame < 9)
					(*m_effect_sprites)[63]->draw(dX, dY, m_effect_list[i]->m_frame, hb::shared::sprite::DrawParams::additive_no_color_key());
				else (*m_effect_sprites)[63]->draw(dX, dY, m_effect_list[i]->m_frame, hb::shared::sprite::DrawParams::additive_tinted(dvalue, dvalue, dvalue)); // RGB2
				break;

			case EffectType::LIGHTNING_BOLT: // Lightning Bolt
				weather_manager::get().draw_thunder_effect(m_effect_list[i]->m_move_x - m_game->m_Camera.get_x(), m_effect_list[i]->m_move_y - m_game->m_Camera.get_y(),
					m_effect_list[i]->m_dest_x * 32 - m_game->m_Camera.get_x(), m_effect_list[i]->m_dest_y * 32 - m_game->m_Camera.get_y(),
					m_effect_list[i]->m_render_x, m_effect_list[i]->m_render_y, 1);

				weather_manager::get().draw_thunder_effect(m_effect_list[i]->m_move_x - m_game->m_Camera.get_x(), m_effect_list[i]->m_move_y - m_game->m_Camera.get_y(),
					m_effect_list[i]->m_dest_x * 32 - m_game->m_Camera.get_x(), m_effect_list[i]->m_dest_y * 32 - m_game->m_Camera.get_y(),
					m_effect_list[i]->m_render_x + 2, m_effect_list[i]->m_render_y - 2, 2);

				weather_manager::get().draw_thunder_effect(m_effect_list[i]->m_move_x - m_game->m_Camera.get_x(), m_effect_list[i]->m_move_y - m_game->m_Camera.get_y(),
					m_effect_list[i]->m_dest_x * 32 - m_game->m_Camera.get_x(), m_effect_list[i]->m_dest_y * 32 - m_game->m_Camera.get_y(),
					m_effect_list[i]->m_render_x - 2, m_effect_list[i]->m_render_y - 2, 2);
				break;

			case EffectType::ABSOLUTE_MAGIC_PROTECTION: // Absolute-Magic-Protect
				temp_frame = m_effect_list[i]->m_frame;
				if (temp_frame < 0) break;
				dX = (m_effect_list[i]->m_dest_x * 32) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_dest_y * 32) - m_game->m_Camera.get_y(); // 53 = APFM buble
				(*m_effect_sprites)[53]->draw(dX, dY, m_effect_list[i]->m_frame, hb::shared::sprite::DrawParams::additive_no_color_key());
				break;

			case EffectType::ARMOR_BREAK: // Armor-Break
				temp_frame = m_effect_list[i]->m_frame;
				if (temp_frame < 0) break;
				dX = (m_effect_list[i]->m_dest_x * 32) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_dest_y * 32) - m_game->m_Camera.get_y();
				(*m_effect_sprites)[55]->draw(dX, dY + 35, m_effect_list[i]->m_frame, hb::shared::sprite::DrawParams::tinted_alpha(0, 0, 0, 0.7f));
				(*m_effect_sprites)[54]->draw(dX, dY, m_effect_list[i]->m_frame, hb::shared::sprite::DrawParams::additive(0.5f));
				break;

			case EffectType::CANCELLATION: // Cancellation
				temp_frame = m_effect_list[i]->m_frame;
				if (temp_frame < 0) break;
				dX = (m_effect_list[i]->m_dest_x * 32) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_dest_y * 32) - m_game->m_Camera.get_y();
				(*m_effect_sprites)[90]->draw(dX + 50, dY + 85, temp_frame, hb::shared::sprite::DrawParams::additive_no_color_key());
				break;

			case EffectType::ILLUSION_MOVEMENT: // Illusion-Movement
			case EffectType::ILLUSION: // Illusion
				temp_frame = m_effect_list[i]->m_frame;
				if (temp_frame < 0) break;
				dX = (m_effect_list[i]->m_dest_x * 32) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_dest_y * 32) - m_game->m_Camera.get_y();
				dvalue = (temp_frame - 5) * (-3);
				if (temp_frame < 9)	(*m_effect_sprites)[60]->draw(dX, dY, m_effect_list[i]->m_frame, hb::shared::sprite::DrawParams::additive_no_color_key());
				else (*m_effect_sprites)[60]->draw(dX, dY, m_effect_list[i]->m_frame, hb::shared::sprite::DrawParams::additive_tinted(dvalue, dvalue, dvalue)); // RGB2
				break;

			case EffectType::MASS_MAGIC_MISSILE_FLYING: //Mass-Magic-Missile
				temp_frame = m_effect_list[i]->m_frame;
				dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
				(*m_effect_sprites)[98]->draw(dX, dY, temp_frame, hb::shared::sprite::DrawParams::additive(0.5f));
				break;

			case EffectType::INHIBITION_CASTING: // Inhibition-Casting
				temp_frame = m_effect_list[i]->m_frame;
				if (temp_frame < 0) break;
				dX = (m_effect_list[i]->m_dest_x * 32) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_dest_y * 32) - m_game->m_Camera.get_y();
				dvalue = (temp_frame - 5) * (-3);
				if (temp_frame < 9) (*m_effect_sprites)[94]->draw(dX, dY + 40, m_effect_list[i]->m_frame, hb::shared::sprite::DrawParams::additive_no_color_key());
				else (*m_effect_sprites)[94]->draw(dX, dY + 40, m_effect_list[i]->m_frame, hb::shared::sprite::DrawParams::additive_tinted(dvalue, dvalue, dvalue));
				break;

			case EffectType::MASS_MM_AURA_CASTER: // Snoopy: Moved for new spells: Caster aura for Mass MagicMissile
				//case 184: // Caster aura for Mass MagicMissile
				temp_frame = m_effect_list[i]->m_frame;
				if (temp_frame < 0) break;
				dX = m_effect_list[i]->m_move_x - m_game->m_Camera.get_x();
				dY = m_effect_list[i]->m_move_y - m_game->m_Camera.get_y();
				(*m_effect_sprites)[96]->draw(dX, dY, m_effect_list[i]->m_frame, hb::shared::sprite::DrawParams::additive(0.5f));
				break;

			case EffectType::MASS_ILLUSION: // Mass-Illusion
			case EffectType::MASS_ILLUSION_MOVEMENT: // Mass-Illusion-Movement
				temp_frame = m_effect_list[i]->m_frame;
				if (temp_frame < 0) break;
				dX = (m_effect_list[i]->m_dest_x * 32) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_dest_y * 32) - m_game->m_Camera.get_y();
				dvalue = (temp_frame - 5) * (-3);
				if (temp_frame < 9) (*m_effect_sprites)[61]->draw(dX, dY, m_effect_list[i]->m_frame, hb::shared::sprite::DrawParams::additive_no_color_key());
				else (*m_effect_sprites)[61]->draw(dX, dY, m_effect_list[i]->m_frame, hb::shared::sprite::DrawParams::additive_tinted(dvalue, dvalue, dvalue)); // RGB2
				break;

				//case 192: // Mage Hero set effect
			case EffectType::MAGE_HERO_SET:
				dX = (m_effect_list[i]->m_dest_x * 32) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_dest_y * 32) - m_game->m_Camera.get_y();
				(*m_effect_sprites)[87]->draw(dX + 50, dY + 57, m_effect_list[i]->m_frame, hb::shared::sprite::DrawParams::additive_no_color_key());
				break;

				//case 193: // War Hero set effect
			case EffectType::WAR_HERO_SET:
				dX = (m_effect_list[i]->m_dest_x * 32) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_dest_y * 32) - m_game->m_Camera.get_y();
				(*m_effect_sprites)[88]->draw(dX + 65, dY + 80, m_effect_list[i]->m_frame, hb::shared::sprite::DrawParams::additive_no_color_key());
				break;

			case EffectType::RESURRECTION: // Resurrection
				dX = (m_effect_list[i]->m_dest_x * 32) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_dest_y * 32) - m_game->m_Camera.get_y();
				(*m_effect_sprites)[99]->draw(dX, dY, m_effect_list[i]->m_frame, hb::shared::sprite::DrawParams::additive(0.5f));
				break;

			case EffectType::SHOTSTAR_FALL_1: // shotstar fall on ground
				dX = m_effect_list[i]->m_move_x;
				dY = m_effect_list[i]->m_move_y;
				(*m_effect_sprites)[133]->draw(dX, dY, (rand() % 15), hb::shared::sprite::DrawParams::additive_no_color_key());
				break;

			case EffectType::SHOTSTAR_FALL_2: // shotstar fall on ground
				dX = m_effect_list[i]->m_move_x;
				dY = m_effect_list[i]->m_move_y;
				(*m_effect_sprites)[134]->draw(dX, dY, (rand() % 15), hb::shared::sprite::DrawParams::additive_no_color_key());
				break;

			case EffectType::SHOTSTAR_FALL_3: // shotstar fall on ground
				dX = m_effect_list[i]->m_move_x;
				dY = m_effect_list[i]->m_move_y;
				(*m_effect_sprites)[135]->draw(dX, dY, (rand() % 15), hb::shared::sprite::DrawParams::additive_no_color_key());
				break;

			case EffectType::EXPLOSION_FIRE_APOCALYPSE: // explosion feu apoc
				dX = m_effect_list[i]->m_move_x;
				dY = m_effect_list[i]->m_move_y;
				(*m_effect_sprites)[136]->draw(dX, dY, (rand() % 18), hb::shared::sprite::DrawParams::additive_no_color_key());
				break;

			case EffectType::CRACK_OBLIQUE: // Faille oblique
				dX = m_effect_list[i]->m_move_x;
				dY = m_effect_list[i]->m_move_y;
				(*m_effect_sprites)[137]->draw(dX, dY, (rand() % 12), hb::shared::sprite::DrawParams::additive_no_color_key());
				break;

			case EffectType::CRACK_HORIZONTAL: // Faille horizontale
				dX = m_effect_list[i]->m_move_x;
				dY = m_effect_list[i]->m_move_y;
				(*m_effect_sprites)[138]->draw(dX, dY, (rand() % 12), hb::shared::sprite::DrawParams::additive_no_color_key());
				break;

			case EffectType::STEAMS_SMOKE: // steams
				dX = m_effect_list[i]->m_move_x;
				dY = m_effect_list[i]->m_move_y;
				(*m_effect_sprites)[139]->draw(dX, dY, (rand() % 20), hb::shared::sprite::DrawParams::additive_no_color_key());
				break;

			case EffectType::GATE_ROUND: // Gate (round one)
				dX = m_effect_list[i]->m_move_x - m_game->m_Camera.get_x();
				dY = m_effect_list[i]->m_move_y - m_game->m_Camera.get_y();
				(*m_effect_sprites)[103]->draw(dX, dY, (rand() % 3), hb::shared::sprite::DrawParams::additive_no_color_key());
				break;

			case EffectType::SALMON_BURST_IMPACT: // burst (lisgt salmon color)
				dX = m_effect_list[i]->m_move_x - m_game->m_Camera.get_x();
				dY = m_effect_list[i]->m_move_y - m_game->m_Camera.get_y();
				(*m_effect_sprites)[104]->draw(dX, dY, (rand() % 3), hb::shared::sprite::DrawParams::additive_no_color_key());
				break;
			}
		}
}
