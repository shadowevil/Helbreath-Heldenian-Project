// MiningManager.cpp: Implementation of MiningManager.
// Mineral spawning, mining actions, and mineral lifecycle.
// Extracted from CGame (Phase B1).

#include "MiningManager.h"
#include "Game.h"
#include "StatusEffectManager.h"
#include "SkillManager.h"
#include "ItemManager.h"
#include "DelayEventManager.h"
#include "DynamicObjectManager.h"
#include "Packet/SharedPackets.h"

using namespace hb::shared::net;
using namespace hb::server::config;
namespace dynamic_object = hb::shared::dynamic_object;

MiningManager::MiningManager()
{
	init_arrays();
}

MiningManager::~MiningManager()
{
	cleanup_arrays();
}

void MiningManager::init_arrays()
{
	for (int i = 0; i < MaxMinerals; i++)
		m_mineral[i] = 0;
}

void MiningManager::cleanup_arrays()
{
	for (int i = 0; i < MaxMinerals; i++)
		if (m_mineral[i] != 0) {
			delete m_mineral[i];
			m_mineral[i] = 0;
		}
}

void MiningManager::mineral_generator()
{
	int iP, tX, tY, ret;

	for(int i = 0; i < MaxMaps; i++) {
		if ((m_game->dice(1, 4) == 1) && (m_game->m_map_list[i] != 0) &&
			(m_game->m_map_list[i]->m_mineral_generator) &&
			(m_game->m_map_list[i]->m_cur_mineral < m_game->m_map_list[i]->m_max_mineral)) {

			iP = m_game->dice(1, m_game->m_map_list[i]->m_total_mineral_point) - 1;
			if ((m_game->m_map_list[i]->m_mineral_point_list[iP].x == -1) || (m_game->m_map_list[i]->m_mineral_point_list[iP].y == -1)) break;

			tX = m_game->m_map_list[i]->m_mineral_point_list[iP].x;
			tY = m_game->m_map_list[i]->m_mineral_point_list[iP].y;

			ret = create_mineral(i, tX, tY, m_game->m_map_list[i]->m_mineral_generator_level);
		}
	}
}

int MiningManager::create_mineral(char map_index, int tX, int tY, char level)
{
	int dynamic_handle, mineral_type;

	if ((map_index < 0) || (map_index >= MaxMaps)) return 0;
	if (m_game->m_map_list[map_index] == 0) return 0;

	for(int i = 1; i < MaxMinerals; i++)
		if (m_mineral[i] == 0) {
			mineral_type = m_game->dice(1, level);
			m_mineral[i] = new class CMineral(mineral_type, map_index, tX, tY, 1);
			if (m_mineral[i] == 0) return 0;

			dynamic_handle = 0;
			switch (mineral_type) {
			case 1:
			case 2:
			case 3:
			case 4:
				dynamic_handle = m_game->m_dynamic_object_manager->add_dynamic_object_list(0, 0, dynamic_object::Mineral1, map_index, tX, tY, 0, i);
				break;

			case 5:
			case 6:
				dynamic_handle = m_game->m_dynamic_object_manager->add_dynamic_object_list(0, 0, dynamic_object::Mineral2, map_index, tX, tY, 0, i);
				break;

			default:
				dynamic_handle = m_game->m_dynamic_object_manager->add_dynamic_object_list(0, 0, dynamic_object::Mineral1, map_index, tX, tY, 0, i);
				break;
			}

			if (dynamic_handle == 0) {
				delete m_mineral[i];
				m_mineral[i] = 0;
				return 0;
			}
			m_mineral[i]->m_dynamic_object_handle = dynamic_handle;
			m_mineral[i]->m_map_index = map_index;

			switch (mineral_type) {
			case 1: m_mineral[i]->m_difficulty = 10; m_mineral[i]->m_remain = 20; break;
			case 2: m_mineral[i]->m_difficulty = 15; m_mineral[i]->m_remain = 15; break;
			case 3: m_mineral[i]->m_difficulty = 20; m_mineral[i]->m_remain = 10; break;
			case 4: m_mineral[i]->m_difficulty = 50; m_mineral[i]->m_remain = 8; break;
			case 5: m_mineral[i]->m_difficulty = 70; m_mineral[i]->m_remain = 6; break;
			case 6: m_mineral[i]->m_difficulty = 90; m_mineral[i]->m_remain = 4; break;
			default: m_mineral[i]->m_difficulty = 10; m_mineral[i]->m_remain = 20; break;
			}

			m_game->m_map_list[map_index]->m_cur_mineral++;

			return i;
		}

	return 0;
}


void MiningManager::check_mining_action(int client_h, int dX, int dY)
{
	short type;
	uint32_t register_time;
	int   dynamic_index, skill_level, result, item_id;
	CItem* item;

	item_id = 0;

	if (m_game->m_client_list[client_h] == 0)  return;

	m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_dynamic_object(dX, dY, &type, &register_time, &dynamic_index);

	if (m_game->m_client_list[client_h]->m_status.invisibility) {
		m_game->m_status_effect_manager->set_invisibility_flag(client_h, hb::shared::owner_class::Player, false);
		m_game->m_delay_event_manager->remove_from_delay_event_list(client_h, hb::shared::owner_class::Player, hb::shared::magic::Invisibility);
		m_game->m_client_list[client_h]->m_magic_effect_status[hb::shared::magic::Invisibility] = 0;
	}

	switch (type) {
	case dynamic_object::Mineral1:
	case dynamic_object::Mineral2:
	{
		auto wc = m_game->m_client_list[client_h]->get_equipped_weapon_class();
		if (wc != hb::shared::item::weapon_class::axe) return;

		if (!m_game->m_client_list[client_h]->m_appearance.is_walking) return;

		skill_level = m_game->m_client_list[client_h]->m_skill_mastery[0];
		if (skill_level == 0) break;

		if (m_game->m_dynamic_object_manager->m_dynamic_object_list[dynamic_index] == 0) break;
		skill_level -= m_mineral[m_game->m_dynamic_object_manager->m_dynamic_object_list[dynamic_index]->m_v1]->m_difficulty;
		if (skill_level <= 0) skill_level = 1;

		result = m_game->dice(1, 100);
		if (result <= skill_level) {
			m_game->m_skill_manager->calculate_ssn_skill_index(client_h, 0, 1);

			switch (m_mineral[m_game->m_dynamic_object_manager->m_dynamic_object_list[dynamic_index]->m_v1]->m_type) {
			case 1:
				switch (m_game->dice(1, 5)) {
				case 1:
				case 2:
				case 3:
					item_id = 355; // Coal
					m_game->m_client_list[client_h]->m_exp_stock += m_game->dice(1, 3);
					break;
				case 4:
					item_id = 357; // IronOre
					m_game->m_client_list[client_h]->m_exp_stock += m_game->dice(1, 3);
					break;
				case 5:
					item_id = 507; // BlondeStone
					m_game->m_client_list[client_h]->m_exp_stock += m_game->dice(1, 3);
					break;
				}
				break;

			case 2:
				switch (m_game->dice(1, 5)) {
				case 1:
				case 2:
					item_id = 355; // Coal
					m_game->m_client_list[client_h]->m_exp_stock += m_game->dice(1, 3);
					break;
				case 3:
				case 4:
					item_id = 357; // IronOre
					m_game->m_client_list[client_h]->m_exp_stock += m_game->dice(1, 3);
					break;
				case 5:
					if (m_game->dice(1, 3) == 2) {
						item_id = 356; // SilverNugget
						m_game->m_client_list[client_h]->m_exp_stock += m_game->dice(1, 4);
					}
					else {
						item_id = 507; // BlondeStone
						m_game->m_client_list[client_h]->m_exp_stock += m_game->dice(1, 3);
					}
					break;
				}
				break;

			case 3:
				switch (m_game->dice(1, 6)) {
				case 1:
					item_id = 355; // Coal
					m_game->m_client_list[client_h]->m_exp_stock += m_game->dice(1, 3);
					break;
				case 2:
				case 3:
				case 4:
				case 5:
					item_id = 357; // IronOre
					m_game->m_client_list[client_h]->m_exp_stock += m_game->dice(1, 3);
					break;
				case 6:
					if (m_game->dice(1, 8) == 3) {
						if (m_game->dice(1, 2) == 1) {
							item_id = 356; // SilverNugget
							m_game->m_client_list[client_h]->m_exp_stock += m_game->dice(1, 4);
						}
						else {
							item_id = 357; // IronOre
							m_game->m_client_list[client_h]->m_exp_stock += m_game->dice(1, 3);
						}
						break;
					}
					else {
						item_id = 357; // IronOre
						m_game->m_client_list[client_h]->m_exp_stock += m_game->dice(1, 3);
					}
					break;
				}
				break;

			case 4:
				switch (m_game->dice(1, 6)) {
				case 1:
					item_id = 355; // Coal
					m_game->m_client_list[client_h]->m_exp_stock += m_game->dice(1, 3);
					break;
				case 2:
					if (m_game->dice(1, 3) == 2) {
						item_id = 356; // SilverNugget
						m_game->m_client_list[client_h]->m_exp_stock += m_game->dice(1, 4);
					}
					break;
				case 3:
				case 4:
				case 5:
					item_id = 357; // IronOre
					m_game->m_client_list[client_h]->m_exp_stock += m_game->dice(1, 3);
					break;
				case 6:
					if (m_game->dice(1, 8) == 3) {
						if (m_game->dice(1, 4) == 3) {
							if (m_game->dice(1, 4) < 3) {
								item_id = 508; // Mithral
								m_game->m_client_list[client_h]->m_exp_stock += m_game->dice(1, 15);
							}
							else {
								item_id = 354; // GoldNugget
								m_game->m_client_list[client_h]->m_exp_stock += m_game->dice(1, 5);
							}
							break;
						}
						else {
							item_id = 356; // SilverNugget
							m_game->m_client_list[client_h]->m_exp_stock += m_game->dice(1, 4);
						}
						break;
					}
					else {
						if (m_game->dice(1, 2) == 1) {
							item_id = 354; // GoldNugget
							m_game->m_client_list[client_h]->m_exp_stock += m_game->dice(1, 5);
						}
						else {
							item_id = 357;  // IronOre
							m_game->m_client_list[client_h]->m_exp_stock += m_game->dice(1, 3);
						}
						break;
					}
					break;
				}
				break;

			case 5:
				switch (m_game->dice(1, 19)) {
				case 3:
					item_id = 352; // Sapphire
					m_game->m_client_list[client_h]->m_exp_stock += m_game->dice(2, 3);
					break;
				default:
					item_id = 358; // Crystal
					m_game->m_client_list[client_h]->m_exp_stock += m_game->dice(2, 3);
					break;
				}
				break;

			case 6:
				switch (m_game->dice(1, 5)) {
				case 1:
					if (m_game->dice(1, 6) == 3) {
						item_id = 353; // Emerald
						m_game->m_client_list[client_h]->m_exp_stock += m_game->dice(2, 4);
					}
					else {
						item_id = 358; // Crystal
						m_game->m_client_list[client_h]->m_exp_stock += m_game->dice(2, 3);
					}
					break;
				case 2:
					if (m_game->dice(1, 6) == 3) {
						item_id = 352; // Saphire
						m_game->m_client_list[client_h]->m_exp_stock += m_game->dice(2, 4);
					}
					else {
						item_id = 358; // Crystal
						m_game->m_client_list[client_h]->m_exp_stock += m_game->dice(2, 3);
					}
					break;
				case 3:
					if (m_game->dice(1, 6) == 3) {
						item_id = 351; // Ruby
						m_game->m_client_list[client_h]->m_exp_stock += m_game->dice(2, 4);
					}
					else {
						item_id = 358; // Crystal
						m_game->m_client_list[client_h]->m_exp_stock += m_game->dice(2, 3);
					}
					break;
				case 4:
					item_id = 358; // Crystal
					m_game->m_client_list[client_h]->m_exp_stock += m_game->dice(2, 3);
					break;
				case 5:
					if (m_game->dice(1, 12) == 3) {
						item_id = 350; // Diamond
						m_game->m_client_list[client_h]->m_exp_stock += m_game->dice(2, 5);
					}
					else {
						item_id = 358; // Crystal
						m_game->m_client_list[client_h]->m_exp_stock += m_game->dice(2, 3);
					}
					break;
				}
				break;

			}

			item = new CItem;
			if (m_game->m_item_manager->init_item_attr(item, item_id) == false) {
				delete item;
			}
			else {
				m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->set_item(m_game->m_client_list[client_h]->m_x,
					m_game->m_client_list[client_h]->m_y, item);
				m_game->send_ground_item_event(CommonType::ItemDrop, m_game->m_client_list[client_h]->m_map_index,
					m_game->m_client_list[client_h]->m_x, m_game->m_client_list[client_h]->m_y, item);
			}

			m_mineral[m_game->m_dynamic_object_manager->m_dynamic_object_list[dynamic_index]->m_v1]->m_remain--;
			if (m_mineral[m_game->m_dynamic_object_manager->m_dynamic_object_list[dynamic_index]->m_v1]->m_remain <= 0) {
				// . Delete Mineral
				delete_mineral(m_game->m_dynamic_object_manager->m_dynamic_object_list[dynamic_index]->m_v1);

				delete m_game->m_dynamic_object_manager->m_dynamic_object_list[dynamic_index];
				m_game->m_dynamic_object_manager->m_dynamic_object_list[dynamic_index] = 0;
			}
		}
	}
		break;

	default:
		break;
	}
}

bool MiningManager::delete_mineral(int index)
{
	int dynamic_index;
	uint32_t time;

	time = GameClock::GetTimeMS();

	if (m_mineral[index] == 0) return false;
	dynamic_index = m_mineral[index]->m_dynamic_object_handle;
	if (m_game->m_dynamic_object_manager->m_dynamic_object_list[dynamic_index] == 0) return false;

	m_game->send_event_to_near_client_type_b(MsgId::DynamicObject, MsgType::Reject, m_game->m_dynamic_object_manager->m_dynamic_object_list[dynamic_index]->m_map_index,
		m_game->m_dynamic_object_manager->m_dynamic_object_list[dynamic_index]->m_x, m_game->m_dynamic_object_manager->m_dynamic_object_list[dynamic_index]->m_y,
		m_game->m_dynamic_object_manager->m_dynamic_object_list[dynamic_index]->m_type, dynamic_index, 0, (short)0);
	m_game->m_map_list[m_game->m_dynamic_object_manager->m_dynamic_object_list[dynamic_index]->m_map_index]->set_dynamic_object(0, 0, m_game->m_dynamic_object_manager->m_dynamic_object_list[dynamic_index]->m_x, m_game->m_dynamic_object_manager->m_dynamic_object_list[dynamic_index]->m_y, time);
	m_game->m_map_list[m_mineral[index]->m_map_index]->set_temp_move_allowed_flag(m_game->m_dynamic_object_manager->m_dynamic_object_list[dynamic_index]->m_x, m_game->m_dynamic_object_manager->m_dynamic_object_list[dynamic_index]->m_y, true);

	m_game->m_map_list[m_mineral[index]->m_map_index]->m_cur_mineral--;

	delete m_mineral[index];
	m_mineral[index] = 0;

	return true;
}
