#include "dynamic_object_manager.h"
#include "game.h"
#include "status_effect_manager.h"
#include "combat_manager.h"
#include "dynamic_object.h"
#include "delay_event_manager.h"
#include "fishing_manager.h"
#include "entity_manager.h"
#include "packet/shared_packets.h"
#include "entity/dynamic_object_id.h"

using namespace hb::shared::net;
using namespace hb::shared::action;
using namespace hb::shared::direction;
using namespace hb::server::config;
namespace sdelay = hb::server::delay_event;
namespace dynamic_object = hb::shared::dynamic_object;

extern char G_cTxt[512];

void DynamicObjectManager::init_arrays()
{
	for (int i = 0; i < MaxDynamicObjects; i++)
		m_dynamic_object_list[i] = 0;
}

void DynamicObjectManager::cleanup_arrays()
{
	for (int i = 0; i < MaxDynamicObjects; i++)
		if (m_dynamic_object_list[i] != 0) delete m_dynamic_object_list[i];
}

int DynamicObjectManager::add_dynamic_object_list(short owner, char owner_type, short type, char map_index, short sX, short sY, uint32_t last_time, int v1)
{

	short pre_type;
	uint32_t time, register_time;

	m_game->m_map_list[map_index]->get_dynamic_object(sX, sY, &pre_type, &register_time);
	if (pre_type != 0) return 0;

	switch (type) {
	case dynamic_object::Fire3:
	case dynamic_object::Fire:
		if (m_game->m_map_list[map_index]->get_is_move_allowed_tile(sX, sY) == false)
			return 0;
		if (last_time != 0) {
			switch (m_game->m_map_list[map_index]->m_weather_status) {
			case 1:	last_time = last_time - (last_time / 2);       break;
			case 2:	last_time = (last_time / 2) - (last_time / 3); break;
			case 3:	last_time = (last_time / 3) - (last_time / 4); break;
			}

			if (last_time == 0) last_time = 1000;
		}
		break;

	case dynamic_object::FishObject:
	case dynamic_object::Fish:
		if (m_game->m_map_list[map_index]->get_is_water(sX, sY) == false)
			return 0;
		break;

	case dynamic_object::AresdenFlag1:
	case dynamic_object::ElvineFlag1:
	case dynamic_object::Mineral1:
	case dynamic_object::Mineral2:
		if (m_game->m_map_list[map_index]->get_moveable(sX, sY) == false)
			return 0;
		m_game->m_map_list[map_index]->set_temp_move_allowed_flag(sX, sY, false);
		break;

	}

	for(int i = 1; i < MaxDynamicObjects; i++)
		if (m_dynamic_object_list[i] == 0) {
			time = GameClock::GetTimeMS();

			if (last_time != 0)
				last_time += (m_game->dice(1, 4) * 1000);

			m_dynamic_object_list[i] = new class CDynamicObject(owner, owner_type, type, map_index, sX, sY, time, last_time, v1);
			m_game->m_map_list[map_index]->set_dynamic_object(i, type, sX, sY, time);
			m_game->send_event_to_near_client_type_b(MsgId::DynamicObject, MsgType::Confirm, map_index, sX, sY, type, i, 0, (short)0);

			return i;
		}
	return 0;
}

void DynamicObjectManager::check_dynamic_object_list()
{

	uint32_t time = GameClock::GetTimeMS(), register_time;
	short type;

	for(int i = 1; i < MaxDynamicObjects; i++) {
		if ((m_dynamic_object_list[i] != 0) && (m_dynamic_object_list[i]->m_last_time != 0)) {

			switch (m_dynamic_object_list[i]->m_type) {
			case dynamic_object::Fire3:
			case dynamic_object::Fire:
				switch (m_game->m_map_list[m_dynamic_object_list[i]->m_map_index]->m_weather_status) {
				case 0: break;
				case 1:
				case 2:
				case 3:
					// ( /10)*    .
					m_dynamic_object_list[i]->m_last_time = m_dynamic_object_list[i]->m_last_time -
						(m_dynamic_object_list[i]->m_last_time / 10) * m_game->m_map_list[m_dynamic_object_list[i]->m_map_index]->m_weather_status;
					break;
				}
				break;
			}
		}
	}

	// .  NULL    .
	for(int i = 1; i < MaxDynamicObjects; i++) {
		if ((m_dynamic_object_list[i] != 0) && (m_dynamic_object_list[i]->m_last_time != 0) &&
			((time - m_dynamic_object_list[i]->m_register_time) >= m_dynamic_object_list[i]->m_last_time)) {

			m_game->m_map_list[m_dynamic_object_list[i]->m_map_index]->get_dynamic_object(m_dynamic_object_list[i]->m_x, m_dynamic_object_list[i]->m_y, &type, &register_time);

			if (register_time == m_dynamic_object_list[i]->m_register_time) {
				m_game->send_event_to_near_client_type_b(MsgId::DynamicObject, MsgType::Reject, m_dynamic_object_list[i]->m_map_index, m_dynamic_object_list[i]->m_x, m_dynamic_object_list[i]->m_y, m_dynamic_object_list[i]->m_type, i, 0, (short)0);
				m_game->m_map_list[m_dynamic_object_list[i]->m_map_index]->set_dynamic_object(0, 0, m_dynamic_object_list[i]->m_x, m_dynamic_object_list[i]->m_y, time);
			}

			switch (type) {
			case dynamic_object::FishObject:
			case dynamic_object::Fish:
				m_game->m_fishing_manager->delete_fish(m_dynamic_object_list[i]->m_owner, 2);
				break;
			}

			delete m_dynamic_object_list[i];
			m_dynamic_object_list[i] = 0;
		}
	}
}

void DynamicObjectManager::dynamic_object_effect_processor()
{
	int index;
	short owner_h, type;
	int damage;
	char  owner_type;
	uint32_t time = GameClock::GetTimeMS(), register_time;

	for(int i = 0; i < MaxDynamicObjects; i++)
		if (m_dynamic_object_list[i] != 0) {
			switch (m_dynamic_object_list[i]->m_type) {

			case dynamic_object::PCloudBegin:
				for(int ix = m_dynamic_object_list[i]->m_x - 1; ix <= m_dynamic_object_list[i]->m_x + 1; ix++)
					for(int iy = m_dynamic_object_list[i]->m_y - 1; iy <= m_dynamic_object_list[i]->m_y + 1; iy++) {

						m_game->m_map_list[m_dynamic_object_list[i]->m_map_index]->get_owner(&owner_h, &owner_type, ix, iy);
						if (owner_h != 0) {
							// Poison Damage .
							switch (owner_type) {
							case hb::shared::owner_class::Player:
								if (m_game->m_client_list[owner_h] == 0) break;
								if (m_game->m_client_list[owner_h]->m_is_killed) break;
								//if ((m_game->m_client_list[owner_h]->m_is_neutral ) && !m_game->m_client_list[owner_h]->m_appearance.is_walking) break;

								if (m_dynamic_object_list[i]->m_v1 < 20)
									damage = m_game->dice(1, 6);
								else damage = m_game->dice(1, 8);

								// New 17/05/2004 Changed
								m_game->m_client_list[owner_h]->m_hp -= damage;

								if (m_game->m_client_list[owner_h]->m_hp <= 0) {
									m_game->m_combat_manager->client_killed_handler(owner_h, owner_h, owner_type, damage);
								}
								else {
									if (damage > 0) {
										m_game->send_notify_msg(0, owner_h, Notify::Hp, 0, 0, 0, 0);

										if (m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::HoldObject] != 0) {
											// 1: Hold-Person
											// 2: Paralize
											m_game->send_notify_msg(0, owner_h, Notify::MagicEffectOff, hb::shared::magic::HoldObject, m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::HoldObject], 0, 0);

											m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::HoldObject] = 0;
											m_game->m_delay_event_manager->remove_from_delay_event_list(owner_h, hb::shared::owner_class::Player, hb::shared::magic::HoldObject);
										}
									}

									if ((m_game->m_combat_manager->check_resisting_magic_success(direction::north, owner_h, hb::shared::owner_class::Player, 100) == false) &&
										(m_game->m_client_list[owner_h]->m_is_poisoned == false)) {

										m_game->m_client_list[owner_h]->m_is_poisoned = true;
										m_game->m_client_list[owner_h]->m_poison_level = m_dynamic_object_list[i]->m_v1;
										m_game->m_client_list[owner_h]->m_poison_time = time;
										m_game->m_status_effect_manager->set_poison_flag(owner_h, owner_type, true);// poison aura appears from dynamic objects
										m_game->send_notify_msg(0, owner_h, Notify::MagicEffectOn, hb::shared::magic::Poison, m_game->m_client_list[owner_h]->m_poison_level, 0, 0);
									}
								}
								break;

							case hb::shared::owner_class::Npc:
								if (m_game->m_npc_list[owner_h] == 0) break;

								if (m_dynamic_object_list[i]->m_v1 < 20)
									damage = m_game->dice(1, 6);
								else damage = m_game->dice(1, 8);

								switch (m_game->m_npc_list[owner_h]->m_type) {
								case 40: // ESG
								case 41: // GMG
								case 67: // McGaffin
								case 68: // Perry
								case 69: // Devlin
									damage = 0;
									break;
								}

								// HP . Action Limit  .
								switch (m_game->m_npc_list[owner_h]->m_action_limit) {
								case 0:
								case 3:
								case 5:
									m_game->m_npc_list[owner_h]->m_hp -= damage;
									break;
								}
								//if (m_game->m_npc_list[owner_h]->m_action_limit == 0)
								//	m_game->m_npc_list[owner_h]->m_hp -= damage;

								if (m_game->m_npc_list[owner_h]->m_hp <= 0) {
									// NPC .
									m_game->m_entity_manager->on_entity_killed(owner_h, owner_h, owner_type, 0);
								}
								else {
									// Damage    .
									if (m_game->dice(1, 3) == 2)
										m_game->m_npc_list[owner_h]->m_time = time;

									if (m_game->m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::HoldObject] != 0) {
										// Hold    .
										m_game->m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::HoldObject] = 0;
									}

									// NPC   .
									m_game->send_event_to_near_client_type_a(owner_h, hb::shared::owner_class::Npc, MsgId::EventMotion, Type::Damage, damage, 0, 0);
								}
								break;
							}
						}
					}
				break;

			case dynamic_object::IceStorm:
				for(int ix = m_dynamic_object_list[i]->m_x - 2; ix <= m_dynamic_object_list[i]->m_x + 2; ix++)
					for(int iy = m_dynamic_object_list[i]->m_y - 2; iy <= m_dynamic_object_list[i]->m_y + 2; iy++) {

						m_game->m_map_list[m_dynamic_object_list[i]->m_map_index]->get_owner(&owner_h, &owner_type, ix, iy);
						if (owner_h != 0) {
							switch (owner_type) {
							case hb::shared::owner_class::Player:
								if (m_game->m_client_list[owner_h] == 0) break;
								if (m_game->m_client_list[owner_h]->m_is_killed) break;

								damage = m_game->dice(3, 3) + 5;

								m_game->m_client_list[owner_h]->m_hp -= damage;

								if (m_game->m_client_list[owner_h]->m_hp <= 0) {
									m_game->m_combat_manager->client_killed_handler(owner_h, owner_h, owner_type, damage);
								}
								else {
									if (damage > 0) {

										m_game->send_notify_msg(0, owner_h, Notify::Hp, 0, 0, 0, 0);

										if (m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::HoldObject] == 1) {

											m_game->send_notify_msg(0, owner_h, Notify::MagicEffectOff, hb::shared::magic::HoldObject, m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::HoldObject], 0, 0);

											m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::HoldObject] = 0;
											m_game->m_delay_event_manager->remove_from_delay_event_list(owner_h, hb::shared::owner_class::Player, hb::shared::magic::HoldObject);
										}
									}

									if ((m_game->m_combat_manager->check_resisting_ice_success(direction::north, owner_h, hb::shared::owner_class::Player, m_dynamic_object_list[i]->m_v1) == false) &&
										(m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] == 0)) {

										m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] = 1;
										m_game->m_status_effect_manager->set_ice_flag(owner_h, owner_type, true);
										m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Ice, time + (20 * 1000),
											owner_h, owner_type, 0, 0, 0, 1, 0, 0);

										m_game->send_notify_msg(0, owner_h, Notify::MagicEffectOn, hb::shared::magic::Ice, 1, 0, 0);
									}
								}
								break;

							case hb::shared::owner_class::Npc:
								if (m_game->m_npc_list[owner_h] == 0) break;

								damage = m_game->dice(3, 3) + 5;

								switch (m_game->m_npc_list[owner_h]->m_type) {
								case 40: // ESG
								case 41: // GMG
								case 67: // McGaffin
								case 68: // Perry
								case 69: // Devlin
									damage = 0;
									break;
								}

								switch (m_game->m_npc_list[owner_h]->m_action_limit) {
								case 0:
								case 3:
								case 5:
									m_game->m_npc_list[owner_h]->m_hp -= damage;
									break;
								}

								if (m_game->m_npc_list[owner_h]->m_hp <= 0) {
									// NPC .
									m_game->m_entity_manager->on_entity_killed(owner_h, owner_h, owner_type, 0);
								}
								else {
									// Damage    .
									if (m_game->dice(1, 3) == 2)
										m_game->m_npc_list[owner_h]->m_time = time;

									if (m_game->m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::HoldObject] != 0) {
										// Hold    .
										m_game->m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::HoldObject] = 0;
									}

									// NPC   .
									m_game->send_event_to_near_client_type_a(owner_h, hb::shared::owner_class::Npc, MsgId::EventMotion, Type::Damage, damage, 0, 0);

									if ((m_game->m_combat_manager->check_resisting_ice_success(direction::north, owner_h, hb::shared::owner_class::Npc, m_dynamic_object_list[i]->m_v1) == false) &&
										(m_game->m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] == 0)) {

										m_game->m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] = 1;
										m_game->m_status_effect_manager->set_ice_flag(owner_h, owner_type, true);
										m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Ice, time + (20 * 1000),
											owner_h, owner_type, 0, 0, 0, 1, 0, 0);
									}
								}
								break;
							}
						}

						m_game->m_map_list[m_dynamic_object_list[i]->m_map_index]->get_dead_owner(&owner_h, &owner_type, ix, iy);
						if ((owner_type == hb::shared::owner_class::Player) && (m_game->m_client_list[owner_h] != 0) &&
							(m_game->m_client_list[owner_h]->m_hp > 0)) {
							damage = m_game->dice(3, 2);
							m_game->m_client_list[owner_h]->m_hp -= damage;

							if (m_game->m_client_list[owner_h]->m_hp <= 0) {
								m_game->m_combat_manager->client_killed_handler(owner_h, owner_h, owner_type, damage);
							}
							else {
								if (damage > 0) {
									m_game->send_notify_msg(0, owner_h, Notify::Hp, 0, 0, 0, 0);
								}
							}
						}

						// Fire Object   .
						m_game->m_map_list[m_dynamic_object_list[i]->m_map_index]->get_dynamic_object(ix, iy, &type, &register_time, &index);
						if (((type == dynamic_object::Fire) || (type == dynamic_object::Fire3)) && (m_dynamic_object_list[index] != 0))
							m_dynamic_object_list[index]->m_last_time = m_dynamic_object_list[index]->m_last_time - (m_dynamic_object_list[index]->m_last_time / 10);
					}
				break;

			case dynamic_object::Fire3:
			case dynamic_object::Fire:
				// Fire-Wall
				if (m_dynamic_object_list[i]->m_count == 1) {
					m_game->m_combat_manager->check_fire_bluring(m_dynamic_object_list[i]->m_map_index, m_dynamic_object_list[i]->m_x, m_dynamic_object_list[i]->m_y);
				}
				m_dynamic_object_list[i]->m_count++;
				if (m_dynamic_object_list[i]->m_count > 10) m_dynamic_object_list[i]->m_count = 10;

				for(int ix = m_dynamic_object_list[i]->m_x - 1; ix <= m_dynamic_object_list[i]->m_x + 1; ix++)
					for(int iy = m_dynamic_object_list[i]->m_y - 1; iy <= m_dynamic_object_list[i]->m_y + 1; iy++) {

						m_game->m_map_list[m_dynamic_object_list[i]->m_map_index]->get_owner(&owner_h, &owner_type, ix, iy);
						if (owner_h != 0) {
							// Fire Damage .
							switch (owner_type) {

							case hb::shared::owner_class::Player:
								if (m_game->m_client_list[owner_h] == 0) break;
								if (m_game->m_client_list[owner_h]->m_is_killed) break;
								//if ((m_game->m_client_list[owner_h]->m_is_neutral ) && !m_game->m_client_list[owner_h]->m_appearance.is_walking) break;

								damage = m_game->dice(1, 6);
								// New 17/05/2004
								m_game->m_client_list[owner_h]->m_hp -= damage;

								if (m_game->m_client_list[owner_h]->m_hp <= 0) {
									m_game->m_combat_manager->client_killed_handler(owner_h, owner_h, owner_type, damage);
								}
								else {
									if (damage > 0) {
										m_game->send_notify_msg(0, owner_h, Notify::Hp, 0, 0, 0, 0);

										if (m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::HoldObject] != 0) {
											// Hold-Person    . Fire Field   .
											// 1: Hold-Person
											// 2: Paralize
											m_game->send_notify_msg(0, owner_h, Notify::MagicEffectOff, hb::shared::magic::HoldObject, m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::HoldObject], 0, 0);

											m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::HoldObject] = 0;
											m_game->m_delay_event_manager->remove_from_delay_event_list(owner_h, hb::shared::owner_class::Player, hb::shared::magic::HoldObject);
										}
									}
								}
								break;

							case hb::shared::owner_class::Npc:
								if (m_game->m_npc_list[owner_h] == 0) break;

								damage = m_game->dice(1, 6);

								switch (m_game->m_npc_list[owner_h]->m_type) {
								case 40: // ESG
								case 41: // GMG
								case 67: // McGaffin
								case 68: // Perry
								case 69: // Devlin
									damage = 0;
									break;
								}

								// HP . Action Limit  .
								switch (m_game->m_npc_list[owner_h]->m_action_limit) {
								case 0:
								case 3:
								case 5:
									m_game->m_npc_list[owner_h]->m_hp -= damage;
									break;
								}
								//if (m_game->m_npc_list[owner_h]->m_action_limit == 0)
								//	m_game->m_npc_list[owner_h]->m_hp -= damage;

								if (m_game->m_npc_list[owner_h]->m_hp <= 0) {
									// NPC .
									m_game->m_entity_manager->on_entity_killed(owner_h, owner_h, owner_type, 0);
								}
								else {
									// Damage    .
									if (m_game->dice(1, 3) == 2)
										m_game->m_npc_list[owner_h]->m_time = time;

									if (m_game->m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::HoldObject] != 0) {
										// Hold    .
										m_game->m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::HoldObject] = 0;
									}

									// NPC   .
									m_game->send_event_to_near_client_type_a(owner_h, hb::shared::owner_class::Npc, MsgId::EventMotion, Type::Damage, damage, 0, 0);
								}
								break;
							}
						}

						m_game->m_map_list[m_dynamic_object_list[i]->m_map_index]->get_dead_owner(&owner_h, &owner_type, ix, iy);
						if ((owner_type == hb::shared::owner_class::Player) && (m_game->m_client_list[owner_h] != 0) &&
							(m_game->m_client_list[owner_h]->m_hp > 0)) {
							damage = m_game->dice(1, 6);
							m_game->m_client_list[owner_h]->m_hp -= damage;

							if (m_game->m_client_list[owner_h]->m_hp <= 0) {
								m_game->m_combat_manager->client_killed_handler(owner_h, owner_h, owner_type, damage);
							}
							else {
								if (damage > 0) {
									m_game->send_notify_msg(0, owner_h, Notify::Hp, 0, 0, 0, 0);
								}
							}
						}

						// Ice Object   .
						m_game->m_map_list[m_dynamic_object_list[i]->m_map_index]->get_dynamic_object(ix, iy, &type, &register_time, &index);
						if ((type == dynamic_object::IceStorm) && (m_dynamic_object_list[index] != 0))
							m_dynamic_object_list[index]->m_last_time = m_dynamic_object_list[index]->m_last_time - (m_dynamic_object_list[index]->m_last_time / 10);
					}
				break;
			}
		}
}
