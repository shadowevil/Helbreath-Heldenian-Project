// delay_event_manager.cpp: Manages timed delay events (magic expiration, meteors, etc.).
// Extracted from game.cpp (Phase B5).

#include "delay_event_manager.h"
#include "game.h"
#include "status_effect_manager.h"
#include "war_manager.h"
#include "item_manager.h"
#include "delay_event.h"
#include "packet/shared_packets.h"

using namespace hb::shared::net;
using namespace hb::shared::action;
using namespace hb::server::config;
namespace sdelay = hb::server::delay_event;

void DelayEventManager::init_arrays()
{
	for (int i = 0; i < MaxDelayEvents; i++)
		m_delay_event_list[i] = 0;
}

void DelayEventManager::cleanup_arrays()
{
	for (int i = 0; i < MaxDelayEvents; i++)
		if (m_delay_event_list[i] != 0) delete m_delay_event_list[i];
}

bool DelayEventManager::register_delay_event(int delay_type, int effect_type, uint32_t last_time, int target_h, char target_type, char map_index, int dX, int dY, int v1, int v2, int v3)
{

	for(int i = 0; i < MaxDelayEvents; i++)
		if (m_delay_event_list[i] == 0) {
			m_delay_event_list[i] = new class CDelayEvent;
			m_delay_event_list[i]->m_delay_type = delay_type;
			m_delay_event_list[i]->m_effect_type = effect_type;
			m_delay_event_list[i]->m_map_index = map_index;
			m_delay_event_list[i]->m_dx = dX;
			m_delay_event_list[i]->m_dy = dY;
			m_delay_event_list[i]->m_target_handle = target_h;
			m_delay_event_list[i]->m_target_type = target_type;
			m_delay_event_list[i]->m_v1 = v1;
			m_delay_event_list[i]->m_v2 = v2;
			m_delay_event_list[i]->m_v3 = v3;
			m_delay_event_list[i]->m_trigger_time = last_time;
			return true;
		}
	return false;
}

bool DelayEventManager::remove_from_delay_event_list(int iH, char type, int effect_type)
{

	for(int i = 0; i < MaxDelayEvents; i++)
		if (m_delay_event_list[i] != 0) {

			if (effect_type == 0) {
				// Effect
				if ((m_delay_event_list[i]->m_target_handle == iH) && (m_delay_event_list[i]->m_target_type == type)) {
					delete m_delay_event_list[i];
					m_delay_event_list[i] = 0;
				}
			}
			else {
				// Effect .
				if ((m_delay_event_list[i]->m_target_handle == iH) && (m_delay_event_list[i]->m_target_type == type) &&
					(m_delay_event_list[i]->m_effect_type == effect_type)) {
					delete m_delay_event_list[i];
					m_delay_event_list[i] = 0;
				}
			}
		}

	return true;
}

void DelayEventManager::delay_event_processor()
{
	int skill_num, result;
	uint32_t time = GameClock::GetTimeMS();
	int temp;

	for(int i = 0; i < MaxDelayEvents; i++)
		if ((m_delay_event_list[i] != 0) && (m_delay_event_list[i]->m_trigger_time < time)) {

			switch (m_delay_event_list[i]->m_delay_type) {

			case sdelay::Type::AncientTablet:
				if (m_game->m_client_list[m_delay_event_list[i]->m_target_handle]->m_status.slate_invincible) {
					temp = 1;
				}
				else if (m_game->m_client_list[m_delay_event_list[i]->m_target_handle]->m_status.slate_mana) {
					temp = 3;
				}
				else if (m_game->m_client_list[m_delay_event_list[i]->m_target_handle]->m_status.slate_exp) {
					temp = 4;
				}

				m_game->send_notify_msg(0, m_delay_event_list[i]->m_target_handle, Notify::SlateStatus, temp, 0, 0, 0);
				m_game->m_item_manager->set_slate_flag(m_delay_event_list[i]->m_target_handle, temp, false);
				break;

			case sdelay::Type::CalcMeteorStrikeEffect:
				m_game->m_war_manager->calc_meteor_strike_effect_handler(m_delay_event_list[i]->m_map_index);
				break;

			case sdelay::Type::DoMeteorStrikeDamage:
				m_game->m_war_manager->do_meteor_strike_damage_handler(m_delay_event_list[i]->m_map_index);
				break;

			case sdelay::Type::MeteorStrike:
				m_game->m_war_manager->meteor_strike_handler(m_delay_event_list[i]->m_map_index);
				break;

			case sdelay::Type::UseItemSkill:
				switch (m_delay_event_list[i]->m_target_type) {
				case hb::shared::owner_class::Player:
					skill_num = m_delay_event_list[i]->m_effect_type;

					if (m_game->m_client_list[m_delay_event_list[i]->m_target_handle] == 0) break;
					if (m_game->m_client_list[m_delay_event_list[i]->m_target_handle]->m_skill_using_status[skill_num] == false) break;
					// ID   v1.12
					if (m_game->m_client_list[m_delay_event_list[i]->m_target_handle]->m_skill_using_time_id[skill_num] != m_delay_event_list[i]->m_v2) break;

					m_game->m_client_list[m_delay_event_list[i]->m_target_handle]->m_skill_using_status[skill_num] = false;
					m_game->m_client_list[m_delay_event_list[i]->m_target_handle]->m_skill_using_time_id[skill_num] = 0;

					// Skill    .
					result = m_game->m_item_manager->calculate_use_skill_item_effect(m_delay_event_list[i]->m_target_handle, m_delay_event_list[i]->m_target_type,
						m_delay_event_list[i]->m_v1, skill_num, m_delay_event_list[i]->m_map_index, m_delay_event_list[i]->m_dx, m_delay_event_list[i]->m_dy);

					m_game->send_notify_msg(0, m_delay_event_list[i]->m_target_handle, Notify::SkillUsingEnd, result, 0, 0, 0);
					break;
				}
				break;

			case sdelay::Type::DamageObject:
				break;

			case sdelay::Type::MagicRelease:
				// Removes the aura after time
				switch (m_delay_event_list[i]->m_target_type) {
				case hb::shared::owner_class::Player:
					if (m_game->m_client_list[m_delay_event_list[i]->m_target_handle] == 0) break;

					m_game->send_notify_msg(0, m_delay_event_list[i]->m_target_handle, Notify::MagicEffectOff,
						m_delay_event_list[i]->m_effect_type, m_game->m_client_list[m_delay_event_list[i]->m_target_handle]->m_magic_effect_status[m_delay_event_list[i]->m_effect_type], 0, 0);

					m_game->m_client_list[m_delay_event_list[i]->m_target_handle]->m_magic_effect_status[m_delay_event_list[i]->m_effect_type] = 0;

					// Inbitition casting
					if (m_delay_event_list[i]->m_effect_type == hb::shared::magic::Inhibition)
						m_game->m_client_list[m_delay_event_list[i]->m_target_handle]->m_inhibition = false;

					// Invisibility
					if (m_delay_event_list[i]->m_effect_type == hb::shared::magic::Invisibility)
						m_game->m_status_effect_manager->set_invisibility_flag(m_delay_event_list[i]->m_target_handle, hb::shared::owner_class::Player, false);

					// Berserk
					if (m_delay_event_list[i]->m_effect_type == hb::shared::magic::Berserk)
						m_game->m_status_effect_manager->set_berserk_flag(m_delay_event_list[i]->m_target_handle, hb::shared::owner_class::Player, false);

					// Haste
					if (m_delay_event_list[i]->m_effect_type == hb::shared::magic::Haste)
						m_game->m_status_effect_manager->set_haste_flag(m_delay_event_list[i]->m_target_handle, hb::shared::owner_class::Player, false);

					// Confusion
					if (m_delay_event_list[i]->m_effect_type == hb::shared::magic::Confuse)
						switch (m_delay_event_list[i]->m_v1) {
						case 3: m_game->m_status_effect_manager->set_illusion_flag(m_delay_event_list[i]->m_target_handle, hb::shared::owner_class::Player, false); break;
						case 4: m_game->m_status_effect_manager->set_illusion_movement_flag(m_delay_event_list[i]->m_target_handle, hb::shared::owner_class::Player, false); break;
						}

					// Protection Magic
					if (m_delay_event_list[i]->m_effect_type == hb::shared::magic::Protect) {
						switch (m_delay_event_list[i]->m_v1) {
						case 1:
							m_game->m_status_effect_manager->set_protection_from_arrow_flag(m_delay_event_list[i]->m_target_handle, hb::shared::owner_class::Player, false);
							break;
						case 2:
						case 5:
							m_game->m_status_effect_manager->set_magic_protection_flag(m_delay_event_list[i]->m_target_handle, hb::shared::owner_class::Player, false);
							break;
						case 3:
						case 4:
							m_game->m_status_effect_manager->set_defense_shield_flag(m_delay_event_list[i]->m_target_handle, hb::shared::owner_class::Player, false);
							break;
						}
					}


					// polymorph
					if (m_delay_event_list[i]->m_effect_type == hb::shared::magic::Polymorph) {
						m_game->m_client_list[m_delay_event_list[i]->m_target_handle]->m_type = m_game->m_client_list[m_delay_event_list[i]->m_target_handle]->m_original_type;
						m_game->send_event_to_near_client_type_a(m_delay_event_list[i]->m_target_handle, hb::shared::owner_class::Player, MsgId::EventMotion, Type::NullAction, 0, 0, 0);
					}

					if (m_delay_event_list[i]->m_effect_type == hb::shared::magic::Ice)
						m_game->m_status_effect_manager->set_ice_flag(m_delay_event_list[i]->m_target_handle, hb::shared::owner_class::Player, false);
					break;

				case hb::shared::owner_class::Npc:
					if (m_game->m_npc_list[m_delay_event_list[i]->m_target_handle] == 0) break;

					m_game->m_npc_list[m_delay_event_list[i]->m_target_handle]->m_magic_effect_status[m_delay_event_list[i]->m_effect_type] = 0;

					// Invisibility
					if (m_delay_event_list[i]->m_effect_type == hb::shared::magic::Invisibility)
						m_game->m_status_effect_manager->set_invisibility_flag(m_delay_event_list[i]->m_target_handle, hb::shared::owner_class::Npc, false);

					// Berserk
					if (m_delay_event_list[i]->m_effect_type == hb::shared::magic::Berserk)
						m_game->m_status_effect_manager->set_berserk_flag(m_delay_event_list[i]->m_target_handle, hb::shared::owner_class::Npc, false);

					// polymorph
					if (m_delay_event_list[i]->m_effect_type == hb::shared::magic::Polymorph) {
						m_game->m_npc_list[m_delay_event_list[i]->m_target_handle]->m_type = m_game->m_npc_list[m_delay_event_list[i]->m_target_handle]->m_original_type;
						m_game->send_event_to_near_client_type_a(m_delay_event_list[i]->m_target_handle, hb::shared::owner_class::Npc, MsgId::EventMotion, Type::NullAction, 0, 0, 0);
					}

					if (m_delay_event_list[i]->m_effect_type == hb::shared::magic::Ice)
						m_game->m_status_effect_manager->set_ice_flag(m_delay_event_list[i]->m_target_handle, hb::shared::owner_class::Npc, false);

					// Illusion
					if (m_delay_event_list[i]->m_effect_type == hb::shared::magic::Confuse)
						m_game->m_status_effect_manager->set_illusion_flag(m_delay_event_list[i]->m_target_handle, hb::shared::owner_class::Npc, false);

					// Protection Magic
					if (m_delay_event_list[i]->m_effect_type == hb::shared::magic::Protect) {
						switch (m_delay_event_list[i]->m_v1) {
						case 1:
							m_game->m_status_effect_manager->set_protection_from_arrow_flag(m_delay_event_list[i]->m_target_handle, hb::shared::owner_class::Npc, false);
							break;
						case 2:
						case 5:
							m_game->m_status_effect_manager->set_magic_protection_flag(m_delay_event_list[i]->m_target_handle, hb::shared::owner_class::Npc, false);
							break;
						case 3:
						case 4:
							m_game->m_status_effect_manager->set_defense_shield_flag(m_delay_event_list[i]->m_target_handle, hb::shared::owner_class::Npc, false);
							break;
						}
					}

					break;
				}
				break;
			}

			delete m_delay_event_list[i];
			m_delay_event_list[i] = 0;
		}
}

void DelayEventManager::delay_event_process()
{

}
