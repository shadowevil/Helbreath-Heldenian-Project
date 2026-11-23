// effect_lights.cpp: draw_effect_lights implementation
//
//////////////////////////////////////////////////////////////////////

#include "effect_manager.h"
#include "game.h"
#include "sprite.h"
#include "effect.h"
#include "global_def.h"

void effect_manager::draw_effect_lights_impl()
{
	int i, dX, dY, dvalue;
	uint32_t time = m_game->m_cur_time;
	char  temp_frame;
	for (i = 0; i < game_limits::max_effects; i++)
		if (m_effect_list[i] != 0) {
			switch (m_effect_list[i]->m_type) {
			case EffectType::NORMAL_HIT:
				break;

			case EffectType::ARROW_FLYING:
				break;

			case EffectType::GOLD_DROP:
				break;

			case EffectType::FIREBALL_EXPLOSION:	// Fire Explosion
				if (m_effect_list[i]->m_frame >= 0)
				{
					dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
					dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
					dvalue = (m_effect_list[i]->m_frame - 7) * (-1);
					if (m_effect_list[i]->m_frame < 6)
						(*m_effect_sprites)[0]->draw(dX, dY + 30, 1, hb::shared::sprite::DrawParams::additive_no_color_key());
					else (*m_effect_sprites)[0]->draw(dX, dY + 30, 1, hb::shared::sprite::DrawParams::additive_tinted(dvalue, dvalue, dvalue));
				}
				break;

			case EffectType::ENERGY_BOLT_EXPLOSION:	 // Energy Bolt
			case EffectType::LIGHTNING_ARROW_EXPLOSION: // Lightning Arrow
				if (m_effect_list[i]->m_frame >= 0)
				{
					dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
					dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
					dvalue = (m_effect_list[i]->m_frame - 9) * (-1);
					if (m_effect_list[i]->m_frame < 8)
						(*m_effect_sprites)[0]->draw(dX, dY + 30, 1, hb::shared::sprite::DrawParams::additive_no_color_key());
					else (*m_effect_sprites)[0]->draw(dX, dY + 30, 1, hb::shared::sprite::DrawParams::additive_tinted(dvalue, dvalue, dvalue));
				}
				break;
			case EffectType::MAGIC_MISSILE_EXPLOSION: // Magic Missile Explosion
				if (m_effect_list[i]->m_frame >= 0)
				{
					dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
					dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
					dvalue = (m_effect_list[i]->m_frame - 2) * (-1);
					if (m_effect_list[i]->m_frame < 2)
						(*m_effect_sprites)[0]->draw(dX, dY + 30, 1, hb::shared::sprite::DrawParams::additive_no_color_key());
					else (*m_effect_sprites)[0]->draw(dX, dY + 30, 1, hb::shared::sprite::DrawParams::additive_tinted(dvalue, dvalue, dvalue));
				}
				break;

			case EffectType::PROJECTILE_GENERIC:
			case EffectType::FIRE_AURA_GROUND:
			case EffectType::MS_CRUSADE_EXPLOSION:
			case EffectType::MAGIC_MISSILE_FLYING:
			case EffectType::ENERGY_BOLT_FLYING:
			case EffectType::FIRE_BALL_FLYING:
			case EffectType::FIRE_STRIKE_FLYING:
			case EffectType::LIGHTNING_ARROW_FLYING: // Lightning arrow
				// Light on ground below the flying effect
				dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
				dvalue = -5;
				(*m_effect_sprites)[0]->draw(dX, dY + 30, 1, hb::shared::sprite::DrawParams::additive_tinted(dvalue, dvalue, dvalue));
				break;

			case EffectType::LIGHT_EFFECT_1:
			case EffectType::LIGHT_EFFECT_2:
				dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
				(*m_effect_sprites)[0]->draw(dX, dY + 30, 1, hb::shared::sprite::DrawParams::additive(0.25f));
				break;

			case EffectType::IMPACT_EFFECT:
				break;

			case EffectType::CHILL_WIND_IMPACT:
			case EffectType::MASS_CHILL_WIND:
				if (m_effect_list[i]->m_frame >= 0)
				{
					dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
					dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
					dvalue = (m_effect_list[i]->m_frame - 7) * (-1);
					if (m_effect_list[i]->m_frame < 6)
						(*m_effect_sprites)[0]->draw(dX, dY, 1, hb::shared::sprite::DrawParams::additive());
					else (*m_effect_sprites)[0]->draw(dX, dY, 1, hb::shared::sprite::DrawParams::additive_tinted(dvalue, dvalue, dvalue));
				}
				break;

			case EffectType::PROTECTION_RING: // Protection Ring
				if (m_effect_list[i]->m_frame >= 0)
				{
					dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
					dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
					(*m_effect_sprites)[24]->draw(dX, dY, m_effect_list[i]->m_frame, hb::shared::sprite::DrawParams::additive_no_color_key());
				}
				break;

			case EffectType::BUFF_EFFECT_LIGHT:
				if (m_effect_list[i]->m_frame >= 0) {
					dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
					dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
					(*m_effect_sprites)[30]->draw(dX, dY, m_effect_list[i]->m_frame, hb::shared::sprite::DrawParams::additive_no_color_key());
				}
				break;

			case EffectType::AURA_EFFECT_1:
				dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
				(*m_effect_sprites)[74]->draw(dX, dY - 34, m_effect_list[i]->m_frame, hb::shared::sprite::DrawParams::additive());
				break;

			case EffectType::AURA_EFFECT_2:
				dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
				(*m_effect_sprites)[75]->draw(dX, dY + 35, m_effect_list[i]->m_frame, hb::shared::sprite::DrawParams::additive());
				break;

			case EffectType::ICE_GOLEM_EFFECT_1: // Icegolem
				dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
				(*m_effect_sprites)[76]->draw(dX + m_effect_list[i]->m_dest_x * m_effect_list[i]->m_frame, dY + m_effect_list[i]->m_dest_y * m_effect_list[i]->m_frame, m_effect_list[i]->m_frame, hb::shared::sprite::DrawParams::additive(0.25f));
				break;

			case EffectType::ICE_GOLEM_EFFECT_2:// Icegolem
				dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
				(*m_effect_sprites)[77]->draw(dX + m_effect_list[i]->m_dest_x * m_effect_list[i]->m_frame, dY + m_effect_list[i]->m_dest_y * m_effect_list[i]->m_frame, m_effect_list[i]->m_frame, hb::shared::sprite::DrawParams::additive(0.25f));
				break;

			case EffectType::ICE_GOLEM_EFFECT_3:// Icegolem
				dX = (m_effect_list[i]->m_move_x) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_move_y) - m_game->m_Camera.get_y();
				(*m_effect_sprites)[78]->draw(dX + m_effect_list[i]->m_dest_x * m_effect_list[i]->m_frame, dY + m_effect_list[i]->m_dest_y * m_effect_list[i]->m_frame, m_effect_list[i]->m_frame, hb::shared::sprite::DrawParams::additive(0.25f));
				break;

			case EffectType::BERSERK: // Berserk : Cirlcle 6 magic
				dX = (m_effect_list[i]->m_dest_x * 32) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_dest_y * 32) - m_game->m_Camera.get_y();
				(*m_effect_sprites)[58]->draw(dX, dY, m_effect_list[i]->m_frame, hb::shared::sprite::DrawParams::additive_no_color_key());
				break;

			case EffectType::ILLUSION: // Ilusion
			case EffectType::MASS_ILLUSION: // Mass Illusion
				temp_frame = m_effect_list[i]->m_frame;
				dX = (m_effect_list[i]->m_dest_x * 32) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_dest_y * 32) - m_game->m_Camera.get_y();
				(*m_effect_sprites)[59]->draw(dX, dY, temp_frame, hb::shared::sprite::DrawParams::additive_no_color_key());
				break;

			case EffectType::ILLUSION_MOVEMENT: // Illusion mvt
			case EffectType::MASS_ILLUSION_MOVEMENT: // Mass Illusion mvt
				temp_frame = m_effect_list[i]->m_frame;
				dX = (m_effect_list[i]->m_dest_x * 32) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_dest_y * 32) - m_game->m_Camera.get_y();
				(*m_effect_sprites)[102]->draw(dX, dY + 30, temp_frame, hb::shared::sprite::DrawParams::additive_no_color_key());
				break;

			case EffectType::INHIBITION_CASTING: // Inhibition casting
				temp_frame = m_effect_list[i]->m_frame;
				dX = (m_effect_list[i]->m_dest_x * 32) - m_game->m_Camera.get_x();
				dY = (m_effect_list[i]->m_dest_y * 32) - m_game->m_Camera.get_y();
				(*m_effect_sprites)[95]->draw(dX, dY + 40, temp_frame, hb::shared::sprite::DrawParams::additive_no_color_key());
				break;
			}
		}
}
