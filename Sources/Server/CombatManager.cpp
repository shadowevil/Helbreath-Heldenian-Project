#include "CombatManager.h"
#include "Game.h"
#include <algorithm>
#include "StatusEffectManager.h"
#include "WarManager.h"
#include "SkillManager.h"
#include "MagicManager.h"
#include "ItemManager.h"
#include "Item.h"
#include "EntityManager.h"
#include "LootManager.h"
#include "DelayEventManager.h"
#include "DynamicObjectManager.h"
#include "Packet/SharedPackets.h"
#include "SharedCalculations.h"
#include "BalanceConstants.h"
#include "Log.h"
#include "ServerLogChannels.h"

using namespace hb::shared::net;

using hb::log_channel;
using namespace hb::shared::action;
using namespace hb::server::net;
using namespace hb::server::config;
using hb::shared::item::to_int;
using namespace hb::shared::item;
using namespace hb::shared::direction;
namespace sock = hb::shared::net::socket;
namespace dynamic_object = hb::shared::dynamic_object;
namespace smap = hb::server::map;
namespace sdelay = hb::server::delay_event;
using namespace hb::server::npc;

extern char G_cTxt[512];

// Direction lookup tables (duplicated from Game.cpp)
static char _tmp_cTmpDirX[9] = { 0,0,1,1,1,0,-1,-1,-1 };
static char _tmp_cTmpDirY[9] = { 0,-1,-1,0,1,1,1,0,-1 };

// Combo attack bonus tables (duplicated from Game.cpp)
static int ___iCAB5[] = { 0,0, 0,1,2 };
static int ___iCAB6[] = { 0,0, 0,0,0 };
static int ___iCAB7[] = { 0,0, 1,2,3 };
static int ___iCAB8[] = { 0,0, 1,3,5 };
static int ___iCAB9[] = { 0,0, 2,4,8 };
static int ___iCAB10[] = { 0,0, 1,2,3 };

void CombatManager::remove_from_target(short target_h, char target_type, int code)
{
	
	uint32_t time = GameClock::GetTimeMS();

	for(int i = 0; i < MaxNpcs; i++)
		if (m_game->m_npc_list[i] != 0) {
			if ((m_game->m_npc_list[i]->m_target_index == target_h) &&
				(m_game->m_npc_list[i]->m_target_type == target_type)) {

				switch (code) {
				case hb::shared::magic::Invisibility:
					if (m_game->m_npc_list[i]->m_special_ability == 1) {
					}
					else {
						m_game->m_npc_list[i]->m_behavior = Behavior::Move;
						m_game->m_npc_list[i]->m_target_index = 0;
						m_game->m_npc_list[i]->m_target_type = 0;
					}
					break;

				default:
					m_game->m_npc_list[i]->m_behavior = Behavior::Move;
					m_game->m_npc_list[i]->m_target_index = 0;
					m_game->m_npc_list[i]->m_target_type = 0;
					break;
				}
			}
		}
}

int CombatManager::get_danger_value(int npc_h, short dX, short dY)
{
	int danger_value;
	short owner, do_type;
	char  owner_type;
	uint32_t register_time;

	if (m_game->m_npc_list[npc_h] == 0) return false;

	danger_value = 0;

	for(int ix = dX - 2; ix <= dX + 2; ix++)
		for(int iy = dY - 2; iy <= dY + 2; iy++) {
			m_game->m_map_list[m_game->m_npc_list[npc_h]->m_map_index]->get_owner(&owner, &owner_type, ix, iy);
			m_game->m_map_list[m_game->m_npc_list[npc_h]->m_map_index]->get_dynamic_object(ix, iy, &do_type, &register_time);

			if (do_type == 1) danger_value++;

			switch (owner_type) {
			case 0:
				break;
			case hb::shared::owner_class::Player:
				if (m_game->m_client_list[owner] == 0) break;
				if (m_game->m_npc_list[npc_h]->m_side != m_game->m_client_list[owner]->m_side)
					danger_value++;
				else danger_value--;
				break;
			case hb::shared::owner_class::Npc:
				if (m_game->m_npc_list[owner] == 0) break;
				if (m_game->m_npc_list[npc_h]->m_side != m_game->m_npc_list[owner]->m_side)
					danger_value++;
				else danger_value--;
				break;
			}
		}

	return danger_value;
}

void CombatManager::client_killed_handler(int client_h, int attacker_h, char attacker_type, short damage)
{
	char attacker_name[hb::shared::limits::NpcNameLen];
	short attacker_weapon;
	int ex_h;
	bool  is_s_aattacked = false;

	if (m_game->m_client_list[client_h] == 0) return;
	if (m_game->m_client_list[client_h]->m_is_init_complete == false) return;
	if (m_game->m_client_list[client_h]->m_is_killed) return;

	m_game->m_client_list[client_h]->m_is_killed = true;
	// HP 0.
	m_game->m_client_list[client_h]->m_hp = 0;

	// Snoopy: Remove all magic effects and flags
	for(int i = 0; i < hb::server::config::MaxMagicEffects; i++)
		m_game->m_client_list[client_h]->m_magic_effect_status[i] = 0;

	m_game->m_status_effect_manager->set_defense_shield_flag(client_h, hb::shared::owner_class::Player, false);
	m_game->m_status_effect_manager->set_magic_protection_flag(client_h, hb::shared::owner_class::Player, false);
	m_game->m_status_effect_manager->set_protection_from_arrow_flag(client_h, hb::shared::owner_class::Player, false);
	m_game->m_status_effect_manager->set_illusion_movement_flag(client_h, hb::shared::owner_class::Player, false);
	m_game->m_status_effect_manager->set_illusion_flag(client_h, hb::shared::owner_class::Player, false);
	m_game->m_status_effect_manager->set_inhibition_casting_flag(client_h, hb::shared::owner_class::Player, false);
	m_game->m_status_effect_manager->set_poison_flag(client_h, hb::shared::owner_class::Player, false);
	m_game->m_client_list[client_h]->m_is_poisoned = false;
	m_game->m_client_list[client_h]->m_poison_level = 0;
	m_game->send_notify_msg(0, client_h, Notify::MagicEffectOff, hb::shared::magic::Poison, 0, 0, 0);
	m_game->m_status_effect_manager->set_ice_flag(client_h, hb::shared::owner_class::Player, false);
	m_game->m_status_effect_manager->set_berserk_flag(client_h, hb::shared::owner_class::Player, false);
	m_game->m_status_effect_manager->set_invisibility_flag(client_h, hb::shared::owner_class::Player, false);
	m_game->m_item_manager->set_slate_flag(client_h, SlateClearNotify, false);
	m_game->m_status_effect_manager->set_haste_flag(client_h, hb::shared::owner_class::Player, false);

	if (m_game->m_client_list[client_h]->m_is_exchange_mode) {
		ex_h = m_game->m_client_list[client_h]->m_exchange_h;
		m_game->m_item_manager->clear_exchange_status(ex_h);
		m_game->m_item_manager->clear_exchange_status(client_h);
	}

	// NPC    .
	remove_from_target(client_h, hb::shared::owner_class::Player);

	// Delete all summoned NPCs belonging to this player
	for (int i = 0; i < MaxNpcs; i++)
		if (m_game->m_npc_list[i] != 0) {
			if ((m_game->m_npc_list[i]->m_is_summoned) &&
				(m_game->m_npc_list[i]->m_follow_owner_index == client_h) &&
				(m_game->m_npc_list[i]->m_follow_owner_type == hb::shared::owner_class::Player)) {
				m_game->m_entity_manager->delete_entity(i);
			}
		}

	std::memset(attacker_name, 0, sizeof(attacker_name));
	switch (attacker_type) {
	case hb::shared::owner_class::PlayerIndirect:
	case hb::shared::owner_class::Player:
		if (m_game->m_client_list[attacker_h] != 0)
			memcpy(attacker_name, m_game->m_client_list[attacker_h]->m_char_name, hb::shared::limits::CharNameLen - 1);
		break;
	case hb::shared::owner_class::Npc:
		if (m_game->m_npc_list[attacker_h] != 0)
#ifdef DEF_LOCALNPCNAME
			std::snprintf(attacker_name, sizeof(attacker_name), "NPCNPCNPC@%d", m_game->m_npc_list[attacker_h]->m_type);
#else 
			memcpy(attacker_name, m_game->m_npc_list[attacker_h]->m_npc_name, hb::shared::limits::NpcNameLen - 1);
#endif
		break;
	default:
		break;
	}

	m_game->send_notify_msg(0, client_h, Notify::Killed, 0, 0, 0, attacker_name);
	if (attacker_type == hb::shared::owner_class::Player) {
		attacker_weapon = to_int(m_game->m_client_list[attacker_h]->get_equipped_weapon_class());
	}
	else attacker_weapon = 1;
	m_game->send_event_to_near_client_type_a(client_h, hb::shared::owner_class::Player, MsgId::EventMotion, Type::Dying, damage, attacker_weapon, 0);
	m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->clear_owner(12, client_h, hb::shared::owner_class::Player, m_game->m_client_list[client_h]->m_x, m_game->m_client_list[client_h]->m_y);
	m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->set_dead_owner(client_h, hb::shared::owner_class::Player, m_game->m_client_list[client_h]->m_x, m_game->m_client_list[client_h]->m_y);
	if (m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->m_type == smap::MapType::NoPenaltyNoReward) return;
	if (m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->m_is_heldenian_map) {
		if (m_game->m_client_list[client_h]->m_side == 1) {
			m_game->m_heldenian_aresden_dead++;
		}
		else if (m_game->m_client_list[client_h]->m_side == 2) {
			m_game->m_heldenian_elvine_dead++;
		}
		m_game->m_war_manager->update_heldenian_status();
	}

	if (attacker_type == hb::shared::owner_class::Player) {
		// v1.432
		switch (m_game->m_client_list[attacker_h]->m_special_ability_type) {
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
			is_s_aattacked = true;
			break;
		}

		if (attacker_h == client_h) return;
		if (memcmp(m_game->m_client_list[client_h]->m_location, "NONE", 4) == 0) {
			if (m_game->m_client_list[client_h]->m_player_kill_count == 0) {

				m_game->m_loot_manager->apply_pk_penalty(attacker_h, client_h);
			}
			else {

				m_game->m_loot_manager->pk_kill_reward_handler(attacker_h, client_h);
			}
		}
		else {
			if (memcmp(m_game->m_client_list[attacker_h]->m_location, "NONE", 4) == 0) {
				if (m_game->m_client_list[client_h]->m_player_kill_count == 0) {
					m_game->m_loot_manager->apply_pk_penalty(attacker_h, client_h);
				}
				else {

				}
			}
			else {
				if (memcmp(m_game->m_client_list[client_h]->m_location, m_game->m_client_list[attacker_h]->m_location, 10) == 0) {
					if (m_game->m_client_list[client_h]->m_player_kill_count == 0) {
						m_game->m_loot_manager->apply_pk_penalty(attacker_h, client_h);
					}
					else {
						m_game->m_loot_manager->pk_kill_reward_handler(attacker_h, client_h);
					}
				}
				else {
					m_game->m_loot_manager->enemy_kill_reward_handler(attacker_h, client_h);
				}
			}
		}

		if (m_game->m_client_list[client_h]->m_player_kill_count == 0) {
			// Innocent
			if (memcmp(m_game->m_client_list[attacker_h]->m_location, "NONE", 4) == 0) {
				//m_game->m_client_list[client_h]->m_exp -= m_game->dice(1, 100);
				//if (m_game->m_client_list[client_h]->m_exp < 0) m_game->m_client_list[client_h]->m_exp = 0;
				//m_game->send_notify_msg(0, client_h, Notify::Exp, 0, 0, 0, 0);
			}
			else {
				if (memcmp(m_game->m_client_list[attacker_h]->m_location, m_game->m_client_list[client_h]->m_location, 10) == 0) {
					//m_game->m_client_list[client_h]->m_exp -= m_game->dice(1, 100);
					//if (m_game->m_client_list[client_h]->m_exp < 0) m_game->m_client_list[client_h]->m_exp = 0;
					//m_game->send_notify_msg(0, client_h, Notify::Exp, 0, 0, 0, 0);
				}
				else {
					m_game->m_loot_manager->apply_combat_killed_penalty(client_h, 2, is_s_aattacked);
				}
			}
		}
		else if ((m_game->m_client_list[client_h]->m_player_kill_count >= 1) && (m_game->m_client_list[client_h]->m_player_kill_count <= 3)) {
			// Criminal 
			m_game->m_loot_manager->apply_combat_killed_penalty(client_h, 3, is_s_aattacked);
		}
		else if ((m_game->m_client_list[client_h]->m_player_kill_count >= 4) && (m_game->m_client_list[client_h]->m_player_kill_count <= 11)) {
			// Murderer 
			m_game->m_loot_manager->apply_combat_killed_penalty(client_h, 6, is_s_aattacked);
		}
		else if ((m_game->m_client_list[client_h]->m_player_kill_count >= 12)) {
			// Slaughterer 
			m_game->m_loot_manager->apply_combat_killed_penalty(client_h, 12, is_s_aattacked);
		}
		char txt[128];
		std::memset(txt, 0, sizeof(txt));
		std::snprintf(txt, sizeof(txt), "%s killed %s", m_game->m_client_list[attacker_h]->m_char_name, m_game->m_client_list[client_h]->m_char_name);
		for(int killedi = 0; killedi < MaxClients; killedi++) {
			if (m_game->m_client_list[killedi] != 0 && killedi != attacker_h) {
				m_game->send_notify_msg(0, killedi, Notify::NoticeMsg, 0, 0, 0, txt);
			}
		}
		std::memset(txt, 0, sizeof(txt));
		hb::logger::log<log_channel::pvp>("{}({}) killed {}({}) in {}({},{})", m_game->m_client_list[attacker_h]->m_char_name, m_game->m_client_list[attacker_h]->m_ip_address, m_game->m_client_list[client_h]->m_char_name, m_game->m_client_list[client_h]->m_ip_address, m_game->m_client_list[client_h]->m_map_name, m_game->m_client_list[client_h]->m_x, m_game->m_client_list[client_h]->m_y);
	}
	else if (attacker_type == hb::shared::owner_class::Npc) {

		pk_log(PkLog::ByNpc, client_h, 0, attacker_name);

		if (m_game->m_client_list[client_h]->m_player_kill_count == 0) {
			// Innocent
			m_game->m_loot_manager->apply_combat_killed_penalty(client_h, 1, is_s_aattacked);
		}
		else if ((m_game->m_client_list[client_h]->m_player_kill_count >= 1) && (m_game->m_client_list[client_h]->m_player_kill_count <= 3)) {
			// Criminal 
			m_game->m_loot_manager->apply_combat_killed_penalty(client_h, 3, is_s_aattacked);
		}
		else if ((m_game->m_client_list[client_h]->m_player_kill_count >= 4) && (m_game->m_client_list[client_h]->m_player_kill_count <= 11)) {
			// Murderer 
			m_game->m_loot_manager->apply_combat_killed_penalty(client_h, 6, is_s_aattacked);
		}
		else if ((m_game->m_client_list[client_h]->m_player_kill_count >= 12)) {
			// Slaughterer 
			m_game->m_loot_manager->apply_combat_killed_penalty(client_h, 12, is_s_aattacked);
		}
		char txt[128];
		std::memset(txt, 0, sizeof(txt));
		std::snprintf(txt, sizeof(txt), "%s killed %s", m_game->m_npc_list[attacker_h]->m_npc_name, m_game->m_client_list[client_h]->m_char_name);
		for(int Killedi = 0; Killedi < MaxClients; Killedi++) {
			if (m_game->m_client_list[Killedi] != 0) {
				m_game->send_notify_msg(0, Killedi, Notify::NoticeMsg, 0, 0, 0, txt);
			}
		}
	}
	else if (attacker_type == hb::shared::owner_class::PlayerIndirect) {
		pk_log(PkLog::ByOther, client_h, 0, 0);
		// m_game->m_client_list[client_h]->m_exp -= m_game->dice(1, 50);
		// if (m_game->m_client_list[client_h]->m_exp < 0) m_game->m_client_list[client_h]->m_exp = 0;

		// m_game->send_notify_msg(0, client_h, Notify::Exp, 0, 0, 0, 0);
	}
}

void CombatManager::effect_damage_spot(short attacker_h, char attacker_type, short target_h, char target_type, short v1, short v2, short v3, bool exp, int attr)
{
	int party_id, damage, side_condition, index, remain_life, temp, max_super_attack, rep_damage;
	char attacker_side;
	direction damage_move_dir;
	uint32_t time, exp_gained;
	double tmp1, tmp2, tmp3;
	short atk_x, atk_y, tgt_x, tgt_y, dX, dY, item_index;

	if (attacker_type == hb::shared::owner_class::Player)
		if (m_game->m_client_list[attacker_h] == 0) return;

	if (attacker_type == hb::shared::owner_class::Npc)
		if (m_game->m_npc_list[attacker_h] == 0) return;

	if ((attacker_type == hb::shared::owner_class::Player) && (m_game->m_map_list[m_game->m_client_list[attacker_h]->m_map_index] != 0) &&
		(m_game->m_map_list[m_game->m_client_list[attacker_h]->m_map_index]->m_is_heldenian_map == 1) && (m_game->m_heldenian_initiated)) return;

	time = GameClock::GetTimeMS();
	damage = m_game->dice(v1, v2) + v3;
	if (damage <= 0) damage = 0;

	switch (attacker_type) {
	case hb::shared::owner_class::Player:
		if (m_game->m_client_list[attacker_h]->m_hero_armour_bonus == 2) damage += 4;
		if ((m_game->m_client_list[attacker_h]->m_item_equipment_status[to_int(EquipPos::LeftHand)] == -1) || (m_game->m_client_list[attacker_h]->m_item_equipment_status[to_int(EquipPos::TwoHand)] == -1)) {
			item_index = m_game->m_client_list[attacker_h]->m_item_equipment_status[to_int(EquipPos::RightHand)];
			if ((item_index != -1) && (m_game->m_client_list[attacker_h]->m_item_list[item_index] != 0)) {
				if (m_game->m_client_list[attacker_h]->m_item_list[item_index]->m_id_num == 732 || m_game->m_client_list[attacker_h]->m_item_list[item_index]->m_id_num == 738) {
					damage = damage * 12 / 10;
				}
				if (m_game->m_client_list[attacker_h]->m_item_list[item_index]->m_id_num == 863 || m_game->m_client_list[attacker_h]->m_item_list[item_index]->m_id_num == 864) {
					if (m_game->m_client_list[attacker_h]->m_rating > 0) {
						rep_damage = m_game->m_client_list[attacker_h]->m_rating / 100;
						if (rep_damage < 5) rep_damage = 5;
						if (rep_damage > 15) rep_damage = 15;
						damage += rep_damage;
					}
					if (target_type == hb::shared::owner_class::Player) {
						if (m_game->m_client_list[target_h] != 0) {
							if (m_game->m_client_list[target_h]->m_rating < 0) {
								rep_damage = (abs(m_game->m_client_list[target_h]->m_rating) / 10);
								if (rep_damage > 10) rep_damage = 10;
								damage += rep_damage;
							}
						}
					}
				}
			}
			item_index = m_game->m_client_list[attacker_h]->m_item_equipment_status[to_int(EquipPos::Neck)];
			if ((item_index != -1) && (m_game->m_client_list[attacker_h]->m_item_list[item_index] != 0)) {
				if (m_game->m_client_list[attacker_h]->m_item_list[item_index]->m_id_num == 859) { // NecklaceOfKloness  
					if (target_type == hb::shared::owner_class::Player) {
						if (m_game->m_client_list[target_h] != 0) {
							rep_damage = (abs(m_game->m_client_list[target_h]->m_rating) / 20);
							if (rep_damage > 5) rep_damage = 5;
							damage += rep_damage;
						}
					}
				}
			}
		}

		if ((m_game->m_is_crusade_mode == false) && (m_game->m_client_list[attacker_h]->m_is_player_civil) && (target_type == hb::shared::owner_class::Player)) return;

		tmp1 = (double)damage;
		tmp2 = (double)(m_game->m_client_list[attacker_h]->m_mag + m_game->m_client_list[attacker_h]->m_angelic_mag);
		tmp2 = tmp2 / 3.3f;
		tmp3 = tmp1 + (tmp1 * (tmp2 / 100.0f));
		damage = (int)(tmp3 + 0.5f);

		damage += m_game->m_client_list[attacker_h]->m_add_magical_damage;
		if (damage <= 0) damage = 0;

		if (m_game->m_map_list[m_game->m_client_list[attacker_h]->m_map_index]->m_is_fight_zone)
			damage += damage / 3;

		if (m_game->m_war_manager->check_heldenian_map(attacker_h, m_game->m_bt_field_map_index, hb::shared::owner_class::Player) == 1) {
			damage += damage / 3;
		}

		if ((target_type == hb::shared::owner_class::Player) && (m_game->m_is_crusade_mode) && (m_game->m_client_list[attacker_h]->m_crusade_duty == 1)) {
			if (m_game->m_client_list[attacker_h]->m_level <= 80) {
				damage += (damage * 7) / 10;
			}
			else if (m_game->m_client_list[attacker_h]->m_level <= 100) {
				damage += damage / 2;
			}
			else
				damage += damage / 3;
		}

		attacker_side = m_game->m_client_list[attacker_h]->m_side;
		atk_x = m_game->m_client_list[attacker_h]->m_x;
		atk_y = m_game->m_client_list[attacker_h]->m_y;
		party_id = m_game->m_client_list[attacker_h]->m_party_id;
		break;

	case hb::shared::owner_class::Npc:
		attacker_side = m_game->m_npc_list[attacker_h]->m_side;
		atk_x = m_game->m_npc_list[attacker_h]->m_x;
		atk_y = m_game->m_npc_list[attacker_h]->m_y;
		break;
	}

	switch (target_type) {
	case hb::shared::owner_class::Player:

		if (m_game->m_client_list[target_h] == 0) return;
		if (m_game->m_client_list[target_h]->m_is_init_complete == false) return;
		if (m_game->m_client_list[target_h]->m_is_killed) return;

		// GM mode damage immunity
		if (m_game->m_client_list[target_h]->m_is_gm_mode)
		{
			uint32_t now = GameClock::GetTimeMS();
			if (now - m_game->m_client_list[target_h]->m_last_gm_immune_notify_time > 2000)
			{
				m_game->m_client_list[target_h]->m_last_gm_immune_notify_time = now;
				m_game->send_event_to_near_client_type_a(target_h, hb::shared::owner_class::Player, MsgId::EventMotion, Type::Damage, Sentinel::DamageImmune, 0, 0);
			}
			return;
		}

		if (m_game->m_client_list[target_h]->m_status.slate_invincible) return;

		if ((attacker_type == hb::shared::owner_class::Player) && (m_game->m_is_crusade_mode == false) &&
			(m_game->m_client_list[target_h]->m_player_kill_count == 0) && (m_game->m_client_list[target_h]->m_is_player_civil)) return;

		if ((attacker_type == hb::shared::owner_class::Player) && (m_game->m_client_list[target_h]->m_is_neutral) &&
			(m_game->m_client_list[target_h]->m_player_kill_count == 0) && (m_game->m_client_list[target_h]->m_is_own_location)) return;

		if ((time - m_game->m_client_list[target_h]->m_time) > (uint32_t)m_game->m_lag_protection_interval) return;
		if ((m_game->m_map_list[m_game->m_client_list[target_h]->m_map_index]->m_is_attack_enabled == false)) return;
		if ((attacker_type == hb::shared::owner_class::Player) && (m_game->m_client_list[attacker_h]->m_is_neutral) && (m_game->m_client_list[target_h]->m_player_kill_count == 0)) return;
		if ((m_game->m_client_list[target_h]->m_party_id != 0) && (party_id == m_game->m_client_list[target_h]->m_party_id)) return;
		m_game->m_client_list[target_h]->m_logout_hack_check = time;

		if (attacker_type == hb::shared::owner_class::Player) {
			if (m_game->m_client_list[attacker_h]->m_is_safe_attack_mode) {
				if (attacker_h == target_h) return;
				side_condition = get_player_relationship_raw(attacker_h, target_h);
				if ((side_condition == 7) || (side_condition == 2) || (side_condition == 6)) {

				}
				else {
					if (m_game->m_map_list[m_game->m_client_list[attacker_h]->m_map_index]->m_is_fight_zone) {

					}
					else return;
				}
			}

			if (m_game->m_map_list[m_game->m_client_list[target_h]->m_map_index]->get_attribute(m_game->m_client_list[target_h]->m_x, m_game->m_client_list[target_h]->m_y, 0x00000005) != 0) return;
		}

		m_game->m_skill_manager->clear_skill_using_status(target_h);

		switch (attr) {
		case 1:
			if (m_game->m_client_list[target_h]->m_add_abs_earth != 0) {
				tmp1 = (double)damage;
				tmp2 = (double)m_game->m_client_list[target_h]->m_add_abs_earth;
				tmp3 = (tmp2 / 100.0f) * tmp1;
				damage = damage - (int)(tmp3);
				if (damage < 0) damage = 0;
			}
			break;

		case 2:
			if (m_game->m_client_list[target_h]->m_add_abs_air != 0) {
				tmp1 = (double)damage;
				tmp2 = (double)m_game->m_client_list[target_h]->m_add_abs_air;
				tmp3 = (tmp2 / 100.0f) * tmp1;
				damage = damage - (int)(tmp3);
				if (damage < 0) damage = 0;
			}
			break;

		case 3:
			if (m_game->m_client_list[target_h]->m_add_abs_fire != 0) {
				tmp1 = (double)damage;
				tmp2 = (double)m_game->m_client_list[target_h]->m_add_abs_fire;
				tmp3 = (tmp2 / 100.0f) * tmp1;
				damage = damage - (int)(tmp3);
				if (damage < 0) damage = 0;
			}
			break;

		case 4:
			if (m_game->m_client_list[target_h]->m_add_abs_water != 0) {
				tmp1 = (double)damage;
				tmp2 = (double)m_game->m_client_list[target_h]->m_add_abs_water;
				tmp3 = (tmp2 / 100.0f) * tmp1;
				damage = damage - (int)(tmp3);
				if (damage < 0) damage = 0;
			}
			break;

		default: break;
		}

		index = m_game->m_client_list[target_h]->m_magic_damage_save_item_index;
		if ((index != -1) && (index >= 0) && (index < hb::shared::limits::MaxItems)) {

			switch (m_game->m_client_list[target_h]->m_item_list[index]->m_id_num) {
			case 335:
				tmp1 = (double)damage;
				tmp2 = tmp1 * 0.2f;
				tmp3 = tmp1 - tmp2;
				damage = (int)(tmp3 + 0.5f);
				break;

			case 337:
				tmp1 = (double)damage;
				tmp2 = tmp1 * 0.1f;
				tmp3 = tmp1 - tmp2;
				damage = (int)(tmp3 + 0.5f);
				break;
			}
			if (damage <= 0) damage = 0;

			remain_life = m_game->m_client_list[target_h]->m_item_list[index]->m_instance.cur_durability;
			if (remain_life <= damage) {
				m_game->m_item_manager->item_deplete_handler(target_h, index, true);
			}
			else {
				m_game->m_client_list[target_h]->m_item_list[index]->m_instance.cur_durability -= damage;
				m_game->send_notify_msg(0, target_h, Notify::CurDurability, index, m_game->m_client_list[target_h]->m_item_list[index]->m_instance.cur_durability, 0, 0);
			}
		}

		if (m_game->m_client_list[target_h]->m_add_abs_magical_defense != 0) {
			tmp1 = (double)damage;
			tmp2 = (double)m_game->m_client_list[target_h]->m_add_abs_magical_defense;
			tmp3 = (tmp2 / 100.0f) * tmp1;
			damage = damage - (int)tmp3;
		}

		if (target_type == hb::shared::owner_class::Player) {
			damage -= (m_game->dice(1, m_game->m_client_list[target_h]->m_vit / 10) - 1);
			if (damage <= 0) damage = 0;
		}

		if ((m_game->m_client_list[target_h]->m_is_lucky_effect) &&
			(m_game->dice(1, 10) == 5) && (m_game->m_client_list[target_h]->m_hp <= damage)) {
			damage = m_game->m_client_list[target_h]->m_hp - 1;
		}

		if (m_game->m_client_list[target_h]->m_magic_effect_status[hb::shared::magic::Protect] == 2)
			damage = damage / 2;

		if ((attacker_type == hb::shared::owner_class::Player) && (m_game->m_client_list[target_h]->m_is_special_ability_enabled)) {
			switch (m_game->m_client_list[target_h]->m_special_ability_type) {
			case 51:
			case 52:
				return;
			}
		}

		m_game->m_client_list[target_h]->m_hp -= damage;
		// Interrupt spell casting on damage
		if (damage > 0) {
			m_game->m_client_list[target_h]->m_last_damage_taken_time = GameClock::GetTimeMS();
			if (m_game->m_client_list[target_h]->m_magic_pause_time) {
				m_game->m_client_list[target_h]->m_magic_pause_time = false;
				m_game->m_client_list[target_h]->m_spell_count = -1;
				m_game->send_notify_msg(0, target_h, Notify::SpellInterrupted, 0, 0, 0, 0);
			}
		}
		if (m_game->m_client_list[target_h]->m_hp <= 0) {
			client_killed_handler(target_h, attacker_h, attacker_type, damage);
		}
		else {
			if (damage > 0) {
				if (m_game->m_client_list[target_h]->m_add_trans_mana > 0) {
					tmp1 = (double)m_game->m_client_list[target_h]->m_add_trans_mana;
					tmp2 = (double)damage;
					tmp3 = (tmp1 / 100.0f) * tmp2 + 1.0f;

					temp = m_game->get_max_mp(target_h);
					m_game->m_client_list[target_h]->m_mp += (int)tmp3;
					if (m_game->m_client_list[target_h]->m_mp > temp) m_game->m_client_list[target_h]->m_mp = temp;
				}

				if (m_game->m_client_list[target_h]->m_add_charge_critical > 0) {
					if (m_game->dice(1, 100) <= static_cast<uint32_t>(m_game->m_client_list[target_h]->m_add_charge_critical)) {
						max_super_attack = (m_game->m_client_list[target_h]->m_level / 10);
						if (m_game->m_client_list[target_h]->m_super_attack_left < max_super_attack) m_game->m_client_list[target_h]->m_super_attack_left++;
						m_game->send_notify_msg(0, target_h, Notify::SuperAttackLeft, 0, 0, 0, 0);
					}
				}

				m_game->send_notify_msg(0, target_h, Notify::Hp, 0, 0, 0, 0);
				m_game->send_event_to_near_client_type_a(target_h, hb::shared::owner_class::Player, MsgId::EventMotion, Type::Damage, damage, 0, 0);

				if (m_game->m_client_list[target_h]->m_skill_using_status[19] != true) {
					m_game->m_map_list[m_game->m_client_list[target_h]->m_map_index]->clear_owner(0, target_h, hb::shared::owner_class::Player, m_game->m_client_list[target_h]->m_x, m_game->m_client_list[target_h]->m_y);
					m_game->m_map_list[m_game->m_client_list[target_h]->m_map_index]->set_owner(target_h, hb::shared::owner_class::Player, m_game->m_client_list[target_h]->m_x, m_game->m_client_list[target_h]->m_y);
				}

				if (m_game->m_client_list[target_h]->m_magic_effect_status[hb::shared::magic::HoldObject] != 0) {
					m_game->send_notify_msg(0, target_h, Notify::MagicEffectOff, hb::shared::magic::HoldObject, m_game->m_client_list[target_h]->m_magic_effect_status[hb::shared::magic::HoldObject], 0, 0);
					m_game->m_client_list[target_h]->m_magic_effect_status[hb::shared::magic::HoldObject] = 0;
					m_game->m_delay_event_manager->remove_from_delay_event_list(target_h, hb::shared::owner_class::Player, hb::shared::magic::HoldObject);
				}
			}
		}

		tgt_x = m_game->m_client_list[target_h]->m_x;
		tgt_y = m_game->m_client_list[target_h]->m_y;
		break;

	case hb::shared::owner_class::Npc:
		if (m_game->m_npc_list[target_h] == 0) return;
		if (m_game->m_npc_list[target_h]->m_hp <= 0) return;
		if ((m_game->m_is_crusade_mode) && (attacker_side == m_game->m_npc_list[target_h]->m_side)) return;

		tgt_x = m_game->m_npc_list[target_h]->m_x;
		tgt_y = m_game->m_npc_list[target_h]->m_y;

		switch (m_game->m_npc_list[target_h]->m_action_limit) {
		case 1:
		case 2:
			return;

		case 4:
			if (tgt_x == atk_x) {
				if (tgt_y == atk_y) return;
				else if (tgt_y > atk_y) damage_move_dir = direction::south;
				else if (tgt_y < atk_y) damage_move_dir = direction::north;
			}
			else if (tgt_x > atk_x) {
				if (tgt_y == atk_y)     damage_move_dir = direction::east;
				else if (tgt_y > atk_y) damage_move_dir = direction::southeast;
				else if (tgt_y < atk_y) damage_move_dir = direction::northeast;
			}
			else if (tgt_x < atk_x) {
				if (tgt_y == atk_y)     damage_move_dir = direction::west;
				else if (tgt_y > atk_y) damage_move_dir = direction::southwest;
				else if (tgt_y < atk_y) damage_move_dir = direction::northwest;
			}

			dX = m_game->m_npc_list[target_h]->m_x + _tmp_cTmpDirX[damage_move_dir];
			dY = m_game->m_npc_list[target_h]->m_y + _tmp_cTmpDirY[damage_move_dir];

			if (m_game->m_map_list[m_game->m_npc_list[target_h]->m_map_index]->get_moveable(dX, dY, 0) == false) {
				damage_move_dir = static_cast<direction>(m_game->dice(1, 8));
				dX = m_game->m_npc_list[target_h]->m_x + _tmp_cTmpDirX[damage_move_dir];
				dY = m_game->m_npc_list[target_h]->m_y + _tmp_cTmpDirY[damage_move_dir];
				if (m_game->m_map_list[m_game->m_npc_list[target_h]->m_map_index]->get_moveable(dX, dY, 0) == false) return;
			}

			m_game->m_map_list[m_game->m_npc_list[target_h]->m_map_index]->clear_owner(5, target_h, hb::shared::owner_class::Npc, m_game->m_npc_list[target_h]->m_x, m_game->m_npc_list[target_h]->m_y);
			m_game->m_map_list[m_game->m_npc_list[target_h]->m_map_index]->set_owner(target_h, hb::shared::owner_class::Npc, dX, dY);
			m_game->m_npc_list[target_h]->m_x = dX;
			m_game->m_npc_list[target_h]->m_y = dY;
			m_game->m_npc_list[target_h]->m_dir = damage_move_dir;

			m_game->send_event_to_near_client_type_a(target_h, hb::shared::owner_class::Npc, MsgId::EventMotion, Type::Move, 0, 0, 0);

			dX = m_game->m_npc_list[target_h]->m_x + _tmp_cTmpDirX[damage_move_dir];
			dY = m_game->m_npc_list[target_h]->m_y + _tmp_cTmpDirY[damage_move_dir];

			if (m_game->m_map_list[m_game->m_npc_list[target_h]->m_map_index]->get_moveable(dX, dY, 0) == false) {
				damage_move_dir = static_cast<direction>(m_game->dice(1, 8));
				dX = m_game->m_npc_list[target_h]->m_x + _tmp_cTmpDirX[damage_move_dir];
				dY = m_game->m_npc_list[target_h]->m_y + _tmp_cTmpDirY[damage_move_dir];

				if (m_game->m_map_list[m_game->m_npc_list[target_h]->m_map_index]->get_moveable(dX, dY, 0) == false) return;
			}

			m_game->m_map_list[m_game->m_npc_list[target_h]->m_map_index]->clear_owner(5, target_h, hb::shared::owner_class::Npc, m_game->m_npc_list[target_h]->m_x, m_game->m_npc_list[target_h]->m_y);
			m_game->m_map_list[m_game->m_npc_list[target_h]->m_map_index]->set_owner(target_h, hb::shared::owner_class::Npc, dX, dY);
			m_game->m_npc_list[target_h]->m_x = dX;
			m_game->m_npc_list[target_h]->m_y = dY;
			m_game->m_npc_list[target_h]->m_dir = damage_move_dir;

			m_game->send_event_to_near_client_type_a(target_h, hb::shared::owner_class::Npc, MsgId::EventMotion, Type::Move, 0, 0, 0);

			if (m_game->m_war_manager->check_energy_sphere_destination(target_h, attacker_h, attacker_type)) {
				// Use EntityManager for NPC deletion
				if (m_game->m_entity_manager != NULL)
					m_game->m_entity_manager->delete_entity(target_h);
			}
			return;
		}

		if (attacker_type == hb::shared::owner_class::Player) {
			switch (m_game->m_npc_list[target_h]->m_type) {
			case 40:
			case 41:
				if ((m_game->m_client_list[attacker_h]->m_side == 0) || (m_game->m_npc_list[target_h]->m_side == m_game->m_client_list[attacker_h]->m_side)) return;
				break;
			}
		}

		switch (m_game->m_npc_list[target_h]->m_type) {
		case 67: // McGaffin
		case 68: // Perry
		case 69: // Devlin
			return;
		}

		if (m_game->m_npc_list[target_h]->m_abs_damage > 0) {
			tmp1 = (double)damage;
			tmp2 = (double)(m_game->m_npc_list[target_h]->m_abs_damage) / 100.0f;
			tmp3 = tmp1 * tmp2;
			tmp2 = tmp1 - tmp3;
			damage = (int)tmp2;
			if (damage < 0) damage = 1;
		}

		if (m_game->m_npc_list[target_h]->m_magic_effect_status[hb::shared::magic::Protect] == 2)
			damage = damage / 2;

		m_game->m_npc_list[target_h]->m_hp -= damage;
		if (m_game->m_npc_list[target_h]->m_hp < 0) {
			// Use EntityManager for NPC death handling
			if (m_game->m_entity_manager != NULL)
				m_game->m_entity_manager->on_entity_killed(target_h, attacker_h, attacker_type, damage);
		}
		else {
			switch (attacker_type) {
			case hb::shared::owner_class::Player:
				if ((m_game->m_npc_list[target_h]->m_type != 21) && (m_game->m_npc_list[target_h]->m_type != 55) && (m_game->m_npc_list[target_h]->m_type != 56)
					&& (m_game->m_npc_list[target_h]->m_side == attacker_side)) return;
				break;

			case hb::shared::owner_class::Npc:
				if (m_game->m_npc_list[attacker_h]->m_side == m_game->m_npc_list[target_h]->m_side) return;
				break;
			}

			m_game->send_event_to_near_client_type_a(target_h, hb::shared::owner_class::Npc, MsgId::EventMotion, Type::Damage, damage, 0, 0);

			if ((m_game->dice(1, 3) == 2) && (m_game->m_npc_list[target_h]->m_action_limit == 0)) {
				if ((attacker_type == hb::shared::owner_class::Npc) &&
					(m_game->m_npc_list[attacker_h]->m_type == m_game->m_npc_list[target_h]->m_type) &&
					(m_game->m_npc_list[attacker_h]->m_side == m_game->m_npc_list[target_h]->m_side)) return;

				m_game->m_npc_list[target_h]->m_behavior = Behavior::Attack;
				m_game->m_npc_list[target_h]->m_behavior_turn_count = 0;
				m_game->m_npc_list[target_h]->m_target_index = attacker_h;
				m_game->m_npc_list[target_h]->m_target_type = attacker_type;

				m_game->m_npc_list[target_h]->m_time = time;

				if (m_game->m_npc_list[target_h]->m_magic_effect_status[hb::shared::magic::HoldObject] != 0) {
					m_game->m_npc_list[target_h]->m_magic_effect_status[hb::shared::magic::HoldObject] = 0;
					m_game->m_delay_event_manager->remove_from_delay_event_list(target_h, hb::shared::owner_class::Npc, hb::shared::magic::HoldObject);
				}

				if ((m_game->m_npc_list[target_h]->m_no_die_remain_exp > 0) && (m_game->m_npc_list[target_h]->m_is_summoned != true) &&
					(attacker_type == hb::shared::owner_class::Player) && (m_game->m_client_list[attacker_h] != 0)) {
					if (m_game->m_npc_list[target_h]->m_no_die_remain_exp > static_cast<uint32_t>(damage)) {
						exp_gained = damage;
						if ((m_game->m_is_crusade_mode) && (exp_gained > 10)) exp_gained = 10;

						if (m_game->m_client_list[attacker_h]->m_add_exp > 0) {
							tmp1 = (double)m_game->m_client_list[attacker_h]->m_add_exp;
							tmp2 = (double)exp_gained;
							tmp3 = (tmp1 / 100.0f) * tmp2;
							exp_gained += (uint32_t)tmp3;
						}

						if (m_game->m_client_list[attacker_h]->m_level > 100) {
							switch (m_game->m_npc_list[target_h]->m_type) {
							case 55:
							case 56:
								exp_gained = 0;
								break;
							default: break;
							}
						}

						if (exp_gained)
							m_game->get_exp(attacker_h, exp_gained, true);
						else m_game->get_exp(attacker_h, (exp_gained / 2), true);
						m_game->m_npc_list[target_h]->m_no_die_remain_exp -= damage;
					}
					else {
						exp_gained = m_game->m_npc_list[target_h]->m_no_die_remain_exp;
						if ((m_game->m_is_crusade_mode) && (exp_gained > 10)) exp_gained = 10;

						if (m_game->m_client_list[attacker_h]->m_add_exp > 0) {
							tmp1 = (double)m_game->m_client_list[attacker_h]->m_add_exp;
							tmp2 = (double)exp_gained;
							tmp3 = (tmp1 / 100.0f) * tmp2;
							exp_gained += (uint32_t)tmp3;
						}

						if (m_game->m_client_list[attacker_h]->m_level > 100) {
							switch (m_game->m_npc_list[target_h]->m_type) {
							case 55:
							case 56:
								exp_gained = 0;
								break;
							default: break;
							}
						}

						if (exp_gained)
							m_game->get_exp(attacker_h, exp_gained, true);
						else m_game->get_exp(attacker_h, (exp_gained / 2), true);
						m_game->m_npc_list[target_h]->m_no_die_remain_exp = 0;
					}
				}
			}
		}
		break;
	}
}

void CombatManager::effect_damage_spot_damage_move(short attacker_h, char attacker_type, short target_h, char target_type, short atk_x, short atk_y, short v1, short v2, short v3, bool exp, int attr)
{
	int damage, side_condition, index, remain_life, temp, max_super_attack;
	uint32_t time;
	weapon_class::weapon_class wc;
	char attacker_side;
	direction damage_move_dir;
	double tmp1, tmp2, tmp3;
	int party_id, move_damage;
	short tgt_x, tgt_y;

	if (attacker_type == hb::shared::owner_class::Player)
		if (m_game->m_client_list[attacker_h] == 0) return;

	if (attacker_type == hb::shared::owner_class::Npc)
		if (m_game->m_npc_list[attacker_h] == 0) return;

	time = GameClock::GetTimeMS();
	tgt_x = 0;
	tgt_y = 0;

	damage = m_game->dice(v1, v2) + v3;
	if (damage <= 0) damage = 0;

	party_id = 0;

	switch (attacker_type) {
	case hb::shared::owner_class::Player:
		tmp1 = (double)damage;
		tmp2 = (double)(m_game->m_client_list[attacker_h]->m_mag + m_game->m_client_list[attacker_h]->m_angelic_mag);

		tmp2 = tmp2 / 3.3f;
		tmp3 = tmp1 + (tmp1 * (tmp2 / 100.0f));
		damage = (int)(tmp3 + 0.5f);
		if (damage <= 0) damage = 0;

		// v1.432 2001 4 7 13 7
		damage += m_game->m_client_list[attacker_h]->m_add_magical_damage;

		if (m_game->m_map_list[m_game->m_client_list[attacker_h]->m_map_index]->m_is_fight_zone)
			damage += damage / 3;

		// Crusade :     1.33
		if ((target_type == hb::shared::owner_class::Player) && (m_game->m_is_crusade_mode) && (m_game->m_client_list[attacker_h]->m_crusade_duty == 1))
		{
			if (m_game->m_client_list[attacker_h]->m_level <= 80)
			{
				damage += (damage * 7) / 10;
			}
			else if (m_game->m_client_list[attacker_h]->m_level <= 100)
			{
				damage += damage / 2;
			}
			else damage += damage / 3;
		}

		if (m_game->m_client_list[attacker_h]->m_hero_armour_bonus == 2) {
			damage += 4;
		}

		wc = m_game->m_client_list[attacker_h]->get_equipped_weapon_class();
		if (wc == weapon_class::wand) {
			damage += damage / 3;
		}

		if (m_game->m_war_manager->check_heldenian_map(attacker_h, m_game->m_bt_field_map_index, hb::shared::owner_class::Player) == 1) {
			damage += damage / 3;
		}

		attacker_side = m_game->m_client_list[attacker_h]->m_side;

		party_id = m_game->m_client_list[attacker_h]->m_party_id;
		break;

	case hb::shared::owner_class::Npc:
		attacker_side = m_game->m_npc_list[attacker_h]->m_side;
		break;
	}

	switch (target_type) {
	case hb::shared::owner_class::Player:
		if (m_game->m_client_list[target_h] == 0) return;
		if (m_game->m_client_list[target_h]->m_is_init_complete == false) return;
		if (m_game->m_client_list[target_h]->m_is_killed) return;

		// GM mode damage immunity
		if (m_game->m_client_list[target_h]->m_is_gm_mode)
		{
			uint32_t now = GameClock::GetTimeMS();
			if (now - m_game->m_client_list[target_h]->m_last_gm_immune_notify_time > 2000)
			{
				m_game->m_client_list[target_h]->m_last_gm_immune_notify_time = now;
				m_game->send_event_to_near_client_type_a(target_h, hb::shared::owner_class::Player, MsgId::EventMotion, Type::Damage, Sentinel::DamageImmune, 0, 0);
			}
			return;
		}

		if ((time - m_game->m_client_list[target_h]->m_time) > (uint32_t)m_game->m_lag_protection_interval) return;
		if (m_game->m_client_list[target_h]->m_map_index == -1) return;
		if ((m_game->m_map_list[m_game->m_client_list[target_h]->m_map_index]->m_is_attack_enabled == false)) return;
		if ((attacker_type == hb::shared::owner_class::Player) && (m_game->m_client_list[attacker_h]->m_is_neutral) && (m_game->m_client_list[target_h]->m_player_kill_count == 0)) return;

		if ((m_game->m_is_crusade_mode == false) && (m_game->m_client_list[target_h]->m_player_kill_count == 0) && (attacker_type == hb::shared::owner_class::Player) && (m_game->m_client_list[target_h]->m_is_player_civil)) return;
		if ((m_game->m_is_crusade_mode == false) && (m_game->m_client_list[target_h]->m_player_kill_count == 0) && (attacker_type == hb::shared::owner_class::Player) && (m_game->m_client_list[attacker_h]->m_is_player_civil)) return;

		if ((attacker_type == hb::shared::owner_class::Player) && (m_game->m_client_list[target_h]->m_is_neutral) && (m_game->m_client_list[target_h]->m_player_kill_count == 0) && (m_game->m_client_list[target_h]->m_is_player_civil)) return;

		// 01-12-17
		if ((m_game->m_client_list[target_h]->m_party_id != 0) && (party_id == m_game->m_client_list[target_h]->m_party_id)) return;
		m_game->m_client_list[target_h]->m_logout_hack_check = time;

		if (attacker_type == hb::shared::owner_class::Player) {

			if (m_game->m_client_list[attacker_h]->m_is_safe_attack_mode) {
				if (attacker_h == target_h) return;
				side_condition = get_player_relationship_raw(attacker_h, target_h);
				if ((side_condition == 7) || (side_condition == 2) || (side_condition == 6)) {
				}
				else {
					if (m_game->m_map_list[m_game->m_client_list[attacker_h]->m_map_index]->m_is_fight_zone) {
					}
					else return;
				}
			}

			if (m_game->m_map_list[m_game->m_client_list[target_h]->m_map_index]->get_attribute(m_game->m_client_list[target_h]->m_x, m_game->m_client_list[target_h]->m_y, 0x00000005) != 0) return;
		}

		m_game->m_skill_manager->clear_skill_using_status(target_h);

		switch (attr) {
		case 1:
			if (m_game->m_client_list[target_h]->m_add_abs_earth != 0) {
				tmp1 = (double)damage;
				tmp2 = (double)m_game->m_client_list[target_h]->m_add_abs_earth;
				tmp3 = (tmp2 / 100.0f) * tmp1;
				damage = damage - (int)(tmp3);
				if (damage < 0) damage = 0;
			}
			break;

		case 2:
			if (m_game->m_client_list[target_h]->m_add_abs_air != 0) {
				tmp1 = (double)damage;
				tmp2 = (double)m_game->m_client_list[target_h]->m_add_abs_air;
				tmp3 = (tmp2 / 100.0f) * tmp1;
				damage = damage - (int)(tmp3);
				if (damage < 0) damage = 0;
			}
			break;

		case 3:
			if (m_game->m_client_list[target_h]->m_add_abs_fire != 0) {
				tmp1 = (double)damage;
				tmp2 = (double)m_game->m_client_list[target_h]->m_add_abs_fire;
				tmp3 = (tmp2 / 100.0f) * tmp1;
				damage = damage - (int)(tmp3);
				if (damage < 0) damage = 0;
			}
			break;

		case 4:
			if (m_game->m_client_list[target_h]->m_add_abs_water != 0) {
				tmp1 = (double)damage;
				tmp2 = (double)m_game->m_client_list[target_h]->m_add_abs_water;
				tmp3 = (tmp2 / 100.0f) * tmp1;
				damage = damage - (int)(tmp3);
				if (damage < 0) damage = 0;
			}
			break;

		default: break;
		}

		index = m_game->m_client_list[target_h]->m_magic_damage_save_item_index;
		if ((index != -1) && (index >= 0) && (index < hb::shared::limits::MaxItems)) {

			switch (m_game->m_client_list[target_h]->m_item_list[index]->m_id_num) {
			case 335:
				tmp1 = (double)damage;
				tmp2 = tmp1 * 0.2f;
				tmp3 = tmp1 - tmp2;
				damage = (int)(tmp3 + 0.5f);
				break;

			case 337:
				tmp1 = (double)damage;
				tmp2 = tmp1 * 0.1f;
				tmp3 = tmp1 - tmp2;
				damage = (int)(tmp3 + 0.5f);
				break;
			}
			if (damage <= 0) damage = 0;

			remain_life = m_game->m_client_list[target_h]->m_item_list[index]->m_instance.cur_durability;
			if (remain_life <= damage) {
				m_game->m_item_manager->item_deplete_handler(target_h, index, true);
			}
			else {
				m_game->m_client_list[target_h]->m_item_list[index]->m_instance.cur_durability -= damage;
				m_game->send_notify_msg(0, target_h, Notify::CurDurability, index, m_game->m_client_list[target_h]->m_item_list[index]->m_instance.cur_durability, 0, 0);
			}
		}

		if (m_game->m_client_list[target_h]->m_add_abs_magical_defense != 0) {
			tmp1 = (double)damage;
			tmp2 = (double)m_game->m_client_list[target_h]->m_add_abs_magical_defense;
			tmp3 = (tmp2 / 100.0f) * tmp1;
			damage = damage - (int)tmp3;
		}

		if (target_type == hb::shared::owner_class::Player) {
			damage -= (m_game->dice(1, m_game->m_client_list[target_h]->m_vit / 10) - 1);
			if (damage <= 0) damage = 0;
		}

		if (m_game->m_client_list[target_h]->m_magic_effect_status[hb::shared::magic::Protect] == 2)
			damage = damage / 2;

		if ((m_game->m_client_list[target_h]->m_is_lucky_effect) &&
			(m_game->dice(1, 10) == 5) && (m_game->m_client_list[target_h]->m_hp <= damage)) {
			damage = m_game->m_client_list[target_h]->m_hp - 1;
		}

		if ((attacker_type == hb::shared::owner_class::Player) && (m_game->m_client_list[target_h]->m_is_special_ability_enabled)) {
			switch (m_game->m_client_list[target_h]->m_special_ability_type) {
			case 51:
			case 52:
				return;
			}
		}

		m_game->m_client_list[target_h]->m_hp -= damage;
		// Interrupt spell casting on damage
		if (damage > 0) {
			m_game->m_client_list[target_h]->m_last_damage_taken_time = GameClock::GetTimeMS();
			if (m_game->m_client_list[target_h]->m_magic_pause_time) {
				m_game->m_client_list[target_h]->m_magic_pause_time = false;
				m_game->m_client_list[target_h]->m_spell_count = -1;
				m_game->send_notify_msg(0, target_h, Notify::SpellInterrupted, 0, 0, 0, 0);
			}
		}
		if (m_game->m_client_list[target_h]->m_hp <= 0) {
			client_killed_handler(target_h, attacker_h, attacker_type, damage);
		}
		else {
			if (damage > 0) {
				if (m_game->m_client_list[target_h]->m_add_trans_mana > 0) {
					tmp1 = (double)m_game->m_client_list[target_h]->m_add_trans_mana;
					tmp2 = (double)damage;
					tmp3 = (tmp1 / 100.0f) * tmp2 + 1.0f;

					temp = m_game->get_max_mp(target_h);
					m_game->m_client_list[target_h]->m_mp += (int)tmp3;
					if (m_game->m_client_list[target_h]->m_mp > temp) m_game->m_client_list[target_h]->m_mp = temp;
				}

				if (m_game->m_client_list[target_h]->m_add_charge_critical > 0) {
					if (m_game->dice(1, 100) <= static_cast<uint32_t>(m_game->m_client_list[target_h]->m_add_charge_critical)) {
						max_super_attack = (m_game->m_client_list[target_h]->m_level / 10);
						if (m_game->m_client_list[target_h]->m_super_attack_left < max_super_attack) m_game->m_client_list[target_h]->m_super_attack_left++;
						m_game->send_notify_msg(0, target_h, Notify::SuperAttackLeft, 0, 0, 0, 0);
					}
				}

				if ((attacker_type == hb::shared::owner_class::Player) && (m_game->m_map_list[m_game->m_client_list[attacker_h]->m_map_index]->m_is_fight_zone))
					move_damage = 80;
				else move_damage = 50;

				tgt_x = m_game->m_client_list[target_h]->m_x;
				tgt_y = m_game->m_client_list[target_h]->m_y;

				if ((damage >= move_damage) && !(tgt_x == atk_x && tgt_y == atk_y)) {
					if (tgt_x == atk_x) {
						if (tgt_y > atk_y) damage_move_dir = direction::south;
						else if (tgt_y < atk_y) damage_move_dir = direction::north;
					}
					else if (tgt_x > atk_x) {
						if (tgt_y == atk_y)     damage_move_dir = direction::east;
						else if (tgt_y > atk_y) damage_move_dir = direction::southeast;
						else if (tgt_y < atk_y) damage_move_dir = direction::northeast;
					}
					else if (tgt_x < atk_x) {
						if (tgt_y == atk_y)     damage_move_dir = direction::west;
						else if (tgt_y > atk_y) damage_move_dir = direction::southwest;
						else if (tgt_y < atk_y) damage_move_dir = direction::northwest;
					}

					m_game->m_client_list[target_h]->m_last_damage = damage;
					m_game->send_notify_msg(0, target_h, Notify::Hp, 0, 0, 0, 0);
					m_game->send_notify_msg(0, target_h, Notify::DamageMove, damage_move_dir, damage, 0, 0);
				}
				else {
					m_game->send_notify_msg(0, target_h, Notify::Hp, 0, 0, 0, 0);
					m_game->send_event_to_near_client_type_a(target_h, hb::shared::owner_class::Player, MsgId::EventMotion, Type::Damage, damage, 0, 0);
				}

				if (m_game->m_client_list[target_h]->m_skill_using_status[19] != true) {
					m_game->m_map_list[m_game->m_client_list[target_h]->m_map_index]->clear_owner(0, target_h, hb::shared::owner_class::Player, m_game->m_client_list[target_h]->m_x, m_game->m_client_list[target_h]->m_y);
					m_game->m_map_list[m_game->m_client_list[target_h]->m_map_index]->set_owner(target_h, hb::shared::owner_class::Player, m_game->m_client_list[target_h]->m_x, m_game->m_client_list[target_h]->m_y);
				}

				if (m_game->m_client_list[target_h]->m_magic_effect_status[hb::shared::magic::HoldObject] != 0) {
					// Hold-Person    .     .
					// 1: Hold-Person 
					// 2: Paralize
					m_game->send_notify_msg(0, target_h, Notify::MagicEffectOff, hb::shared::magic::HoldObject, m_game->m_client_list[target_h]->m_magic_effect_status[hb::shared::magic::HoldObject], 0, 0);

					m_game->m_client_list[target_h]->m_magic_effect_status[hb::shared::magic::HoldObject] = 0;
					m_game->m_delay_event_manager->remove_from_delay_event_list(target_h, hb::shared::owner_class::Player, hb::shared::magic::HoldObject);
				}
			}
		}
		break;

	case hb::shared::owner_class::Npc:
		if (m_game->m_npc_list[target_h] == 0) return;
		if (m_game->m_npc_list[target_h]->m_hp <= 0) return;
		if ((m_game->m_is_crusade_mode) && (attacker_side == m_game->m_npc_list[target_h]->m_side)) return;

		switch (m_game->m_npc_list[target_h]->m_action_limit) {
		case 1:
		case 2:
		case 4:
			return;
		}

		if (attacker_type == hb::shared::owner_class::Player) {
			switch (m_game->m_npc_list[target_h]->m_type) {
			case 40:
			case 41:
				if ((m_game->m_client_list[attacker_h]->m_side == 0) || (m_game->m_npc_list[target_h]->m_side == m_game->m_client_list[attacker_h]->m_side)) return;
				break;
			}
		}

		switch (m_game->m_npc_list[target_h]->m_type) {
		case 67: // McGaffin
		case 68: // Perry
		case 69: // Devlin
			damage = 0;
			break;
		}

		// (AbsDamage 0 )    .
		if (m_game->m_npc_list[target_h]->m_abs_damage > 0) {
			tmp1 = (double)damage;
			tmp2 = (double)(m_game->m_npc_list[target_h]->m_abs_damage) / 100.0f;
			tmp3 = tmp1 * tmp2;
			tmp2 = tmp1 - tmp3;
			damage = (int)tmp2;
			if (damage < 0) damage = 1;
		}

		if (m_game->m_npc_list[target_h]->m_magic_effect_status[hb::shared::magic::Protect] == 2)
			damage = damage / 2;

		m_game->m_npc_list[target_h]->m_hp -= damage;
		if (m_game->m_npc_list[target_h]->m_hp < 0) {
			// NPC .
			m_game->m_entity_manager->on_entity_killed(target_h, attacker_h, attacker_type, damage);
		}
		else {

			switch (attacker_type) {
			case hb::shared::owner_class::Player:
				if ((m_game->m_npc_list[target_h]->m_type != 21) && (m_game->m_npc_list[target_h]->m_type != 55) && (m_game->m_npc_list[target_h]->m_type != 56)
					&& (m_game->m_npc_list[target_h]->m_side == attacker_side)) return;
				break;

			case hb::shared::owner_class::Npc:
				if (m_game->m_npc_list[attacker_h]->m_side == m_game->m_npc_list[target_h]->m_side) return;
				break;
			}

			m_game->send_event_to_near_client_type_a(target_h, hb::shared::owner_class::Npc, MsgId::EventMotion, Type::Damage, damage, 0, 0);

			if ((m_game->dice(1, 3) == 2) && (m_game->m_npc_list[target_h]->m_action_limit == 0)) {

				if ((attacker_type == hb::shared::owner_class::Npc) &&
					(m_game->m_npc_list[attacker_h]->m_type == m_game->m_npc_list[target_h]->m_type) &&
					(m_game->m_npc_list[attacker_h]->m_side == m_game->m_npc_list[target_h]->m_side)) return;

				// ActionLimit 1   .   .
				m_game->m_npc_list[target_h]->m_behavior = Behavior::Attack;
				m_game->m_npc_list[target_h]->m_behavior_turn_count = 0;
				m_game->m_npc_list[target_h]->m_target_index = attacker_h;
				m_game->m_npc_list[target_h]->m_target_type = attacker_type;

				// Damage    .
				m_game->m_npc_list[target_h]->m_time = time;

				if (m_game->m_npc_list[target_h]->m_magic_effect_status[hb::shared::magic::HoldObject] != 0) {
					// Hold    .
					m_game->m_npc_list[target_h]->m_magic_effect_status[hb::shared::magic::HoldObject] = 0;
					m_game->m_delay_event_manager->remove_from_delay_event_list(target_h, hb::shared::owner_class::Npc, hb::shared::magic::HoldObject);
				}

				//Crusade
				uint32_t exp;

				// NPC           .
				if ((m_game->m_npc_list[target_h]->m_no_die_remain_exp > 0) && (m_game->m_npc_list[target_h]->m_is_summoned != true) &&
					(attacker_type == hb::shared::owner_class::Player) && (m_game->m_client_list[attacker_h] != 0)) {
					// ExpStock .      .
					if (m_game->m_npc_list[target_h]->m_no_die_remain_exp > static_cast<uint32_t>(damage)) {
						// Crusade
						exp = damage;
						if ((m_game->m_is_crusade_mode) && (exp > 10)) exp = 10;

						if (m_game->m_client_list[attacker_h]->m_add_exp > 0) {
							tmp1 = (double)m_game->m_client_list[attacker_h]->m_add_exp;
							tmp2 = (double)exp;
							tmp3 = (tmp1 / 100.0f) * tmp2;
							exp += (uint32_t)tmp3;
						}

						if (m_game->m_client_list[attacker_h]->m_level > 100) {
							switch (m_game->m_npc_list[target_h]->m_type) {
							case 55:
							case 56:
								exp = 0;
								break;
							default: break;
							}
						}

						if (exp)
							m_game->get_exp(attacker_h, exp); //m_game->m_client_list[attacker_h]->m_exp_stock += exp;     //damage;
						else m_game->get_exp(attacker_h, (exp / 2)); //m_game->m_client_list[attacker_h]->m_exp_stock += (exp/2); //(damage/2);
						m_game->m_npc_list[target_h]->m_no_die_remain_exp -= damage;
					}
					else {
						// Crusade
						exp = m_game->m_npc_list[target_h]->m_no_die_remain_exp;
						if ((m_game->m_is_crusade_mode) && (exp > 10)) exp = 10;

						if (m_game->m_client_list[attacker_h]->m_add_exp > 0) {
							tmp1 = (double)m_game->m_client_list[attacker_h]->m_add_exp;
							tmp2 = (double)exp;
							tmp3 = (tmp1 / 100.0f) * tmp2;
							exp += (uint32_t)tmp3;
						}

						if (m_game->m_client_list[attacker_h]->m_level > 100) {
							switch (m_game->m_npc_list[target_h]->m_type) {
							case 55:
							case 56:
								exp = 0;
								break;
							default: break;
							}
						}

						if (exp)
							m_game->get_exp(attacker_h, exp); //m_game->m_client_list[attacker_h]->m_exp_stock += exp;     //m_game->m_npc_list[target_h]->m_no_die_remain_exp;
						else m_game->get_exp(attacker_h, (exp / 2)); //m_game->m_client_list[attacker_h]->m_exp_stock += (exp/2); //(m_game->m_npc_list[target_h]->m_no_die_remain_exp/2);
						m_game->m_npc_list[target_h]->m_no_die_remain_exp = 0;
					}
				}
			}
		}
		break;
	}
}

void CombatManager::effect_hp_up_spot(short attacker_h, char attacker_type, short target_h, char target_type, short v1, short v2, short v3)
{
	int hp, max_hp;
	uint32_t time = GameClock::GetTimeMS();

	if (attacker_type == hb::shared::owner_class::Player)
		if (m_game->m_client_list[attacker_h] == 0) return;

	hp = m_game->dice(v1, v2) + v3;

	switch (target_type) {
	case hb::shared::owner_class::Player:
		if (m_game->m_client_list[target_h] == 0) return;
		if (m_game->m_client_list[target_h]->m_is_killed) return;
		max_hp = (3 * m_game->m_client_list[target_h]->m_vit) + (2 * m_game->m_client_list[target_h]->m_level) + ((m_game->m_client_list[target_h]->m_str + m_game->m_client_list[target_h]->m_angelic_str) / 2);
		if (m_game->m_client_list[target_h]->m_side_effect_max_hp_down != 0)
			max_hp = max_hp - (max_hp / m_game->m_client_list[target_h]->m_side_effect_max_hp_down);
		if (m_game->m_client_list[target_h]->m_hp < max_hp) {
			m_game->m_client_list[target_h]->m_hp += hp;
			if (m_game->m_client_list[target_h]->m_hp > max_hp) m_game->m_client_list[target_h]->m_hp = max_hp;
			if (m_game->m_client_list[target_h]->m_hp <= 0)     m_game->m_client_list[target_h]->m_hp = 1;
			m_game->send_notify_msg(0, target_h, Notify::Hp, 0, 0, 0, 0);
		}
		break;

	case hb::shared::owner_class::Npc:
		if (m_game->m_npc_list[target_h] == 0) return;
		if (m_game->m_npc_list[target_h]->m_hp <= 0) return;
		if (m_game->m_npc_list[target_h]->m_is_killed) return;
		max_hp = m_game->m_npc_list[target_h]->m_max_hp;
		if (m_game->m_npc_list[target_h]->m_hp < max_hp) {
			m_game->m_npc_list[target_h]->m_hp += hp;
			if (m_game->m_npc_list[target_h]->m_hp > max_hp) m_game->m_npc_list[target_h]->m_hp = max_hp;
			if (m_game->m_npc_list[target_h]->m_hp <= 0)     m_game->m_npc_list[target_h]->m_hp = 1;
		}
		break;
	}
}

void CombatManager::effect_sp_down_spot(short attacker_h, char attacker_type, short target_h, char target_type, short v1, short v2, short v3)
{
	int sp, max_sp;
	uint32_t time = GameClock::GetTimeMS();

	if (attacker_type == hb::shared::owner_class::Player)
		if (m_game->m_client_list[attacker_h] == 0) return;

	sp = m_game->dice(v1, v2) + v3;

	switch (target_type) {
	case hb::shared::owner_class::Player:
		if (m_game->m_client_list[target_h] == 0) return;
		if (m_game->m_client_list[target_h]->m_is_killed) return;

		// New 19/05/2004
		// Is the user having an invincibility slate
		if (m_game->m_client_list[target_h]->m_status.slate_invincible) return;

		max_sp = (2 * (m_game->m_client_list[target_h]->m_str + m_game->m_client_list[target_h]->m_angelic_str)) + (2 * m_game->m_client_list[target_h]->m_level);
		if (m_game->m_client_list[target_h]->m_sp > 0) {

			//v1.42 
			if (m_game->m_client_list[target_h]->m_time_left_firm_stamina == 0) {
				m_game->m_client_list[target_h]->m_sp -= sp;
				if (m_game->m_client_list[target_h]->m_sp < 0) m_game->m_client_list[target_h]->m_sp = 0;
				m_game->send_notify_msg(0, target_h, Notify::Sp, 0, 0, 0, 0);
			}
		}
		break;

	case hb::shared::owner_class::Npc:
		// NPC   .
		break;
	}
}

void CombatManager::effect_sp_up_spot(short attacker_h, char attacker_type, short target_h, char target_type, short v1, short v2, short v3)
{
	int sp, max_sp;
	uint32_t time = GameClock::GetTimeMS();

	if (attacker_type == hb::shared::owner_class::Player)
		if (m_game->m_client_list[attacker_h] == 0) return;

	sp = m_game->dice(v1, v2) + v3;

	switch (target_type) {
	case hb::shared::owner_class::Player:
		if (m_game->m_client_list[target_h] == 0) return;
		if (m_game->m_client_list[target_h]->m_is_killed) return;

		max_sp = (2 * (m_game->m_client_list[target_h]->m_str + m_game->m_client_list[target_h]->m_angelic_str)) + (2 * m_game->m_client_list[target_h]->m_level);
		if (m_game->m_client_list[target_h]->m_sp < max_sp) {
			m_game->m_client_list[target_h]->m_sp += sp;

			if (m_game->m_client_list[target_h]->m_sp > max_sp)
				m_game->m_client_list[target_h]->m_sp = max_sp;

			m_game->send_notify_msg(0, target_h, Notify::Sp, 0, 0, 0, 0);
		}
		break;

	case hb::shared::owner_class::Npc:
		// NPC   .
		break;
	}
}

/*********************************************************************************************************************
**  int bool CombatManager::check_resisting_magic_success(direction attacker_dir, short target_h, char target_type, int hit_ratio) **
**  description			:: calculates if a player resists magic														**
**  last updated		:: November 20, 2004; 8:42 PM; Hypnotoad													**
**	return value		:: bool																						**
**  commentary			::	-	hero armor for target mages adds 50 magic resistance								**
**							-	10000 or more it ratio will deduct 10000 hit ratio									**
**							-	invincible tablet is 100% magic resistance											**
**********************************************************************************************************************/
bool CombatManager::check_resisting_magic_success(direction attacker_dir, short target_h, char target_type, int hit_ratio)
{
	double tmp1, tmp2, tmp3;
	int    target_magic_resist_ratio, dest_hit_ratio, result;
	direction target_dir;
	char   protect;

	switch (target_type) {
	case hb::shared::owner_class::Player:
		if (m_game->m_client_list[target_h] == 0) return false;
		if (m_game->m_map_list[m_game->m_client_list[target_h]->m_map_index]->m_is_attack_enabled == false) return false;

		if (m_game->m_client_list[target_h]->m_status.slate_invincible) return true;
		target_dir = m_game->m_client_list[target_h]->m_dir;
		target_magic_resist_ratio = m_game->m_client_list[target_h]->m_skill_mastery[3] + m_game->m_client_list[target_h]->m_add_magic_resistance;
		if ((m_game->m_client_list[target_h]->m_mag + m_game->m_client_list[target_h]->m_angelic_mag) > 50)
			target_magic_resist_ratio += ((m_game->m_client_list[target_h]->m_mag + m_game->m_client_list[target_h]->m_angelic_mag) - 50);
		target_magic_resist_ratio += m_game->m_client_list[target_h]->m_add_resist_magic;
		protect = m_game->m_client_list[target_h]->m_magic_effect_status[hb::shared::magic::Protect];
		break;

	case hb::shared::owner_class::Npc:
		if (m_game->m_npc_list[target_h] == 0) return false;
		target_dir = m_game->m_npc_list[target_h]->m_dir;
		target_magic_resist_ratio = m_game->m_npc_list[target_h]->m_resist_magic;
		protect = m_game->m_npc_list[target_h]->m_magic_effect_status[hb::shared::magic::Protect];
		break;
	}

	if (protect == 5) return true;

	if ((hit_ratio < 1000) && (protect == 2)) return true;
	if (hit_ratio >= 10000) hit_ratio -= 10000;
	if (target_magic_resist_ratio < 1) target_magic_resist_ratio = 1;
	if (target_h < MaxClients)
	{
		if ((attacker_dir != 0) && (m_game->m_client_list[target_h] != 0) && (m_game->m_client_list[target_h]->m_hero_armour_bonus == 2)) {
			hit_ratio += 50;
		}
	}

	tmp1 = (double)(hit_ratio);
	tmp2 = (double)(target_magic_resist_ratio);
	tmp3 = (tmp1 / tmp2) * 50.0f;
	dest_hit_ratio = (int)(tmp3);

	if (dest_hit_ratio < m_game->m_minimum_hit_ratio) dest_hit_ratio = m_game->m_minimum_hit_ratio;
	if (dest_hit_ratio > m_game->m_maximum_hit_ratio) dest_hit_ratio = m_game->m_maximum_hit_ratio;
	if (dest_hit_ratio >= 100) return false;

	result = m_game->dice(1, 100);
	if (result <= dest_hit_ratio) return false;

	if (target_type == hb::shared::owner_class::Player)
		m_game->m_skill_manager->calculate_ssn_skill_index(target_h, 3, 1);
	return true;
}

bool CombatManager::check_resisting_ice_success(direction attacker_dir, short target_h, char target_type, int hit_ratio)
{
	int    target_ice_resist_ratio, result;

	switch (target_type) {
	case hb::shared::owner_class::Player:
		if (m_game->m_client_list[target_h] == 0) return false;

		target_ice_resist_ratio = m_game->m_client_list[target_h]->m_add_abs_water * 2;
		if (m_game->m_client_list[target_h]->m_warm_effect_time == 0) {
		}
		else if ((GameClock::GetTimeMS() - m_game->m_client_list[target_h]->m_warm_effect_time) < 1000 * 30) return true;
		break;

	case hb::shared::owner_class::Npc:
		if (m_game->m_npc_list[target_h] == 0) return false;
		target_ice_resist_ratio = (m_game->m_npc_list[target_h]->m_resist_magic) - (m_game->m_npc_list[target_h]->m_resist_magic / 3); // . NPC    70%
		break;
	}

	if (target_ice_resist_ratio < 1) target_ice_resist_ratio = 1;

	result = m_game->dice(1, 100);
	if (result <= target_ice_resist_ratio) return true;

	return false;
}

bool CombatManager::analyze_criminal_action(int client_h, short dX, short dY, bool is_check)
{
	int   naming_value, tX, tY;
	short owner_h;
	char  owner_type, name[hb::shared::limits::CharNameLen], npc_name[hb::shared::limits::NpcNameLen];
	char  npc_waypoint[11];

	if (m_game->m_client_list[client_h] == 0) return false;
	if (m_game->m_client_list[client_h]->m_is_init_complete == false) return false;
	if (m_game->m_is_crusade_mode) return false;

	m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, dX, dY);

	if ((owner_type == hb::shared::owner_class::Player) && (m_game->m_client_list[owner_h] != 0)) {
		if (get_is_player_hostile(client_h, owner_h) != true) {
			if (is_check) return true;

			std::memset(npc_name, 0, sizeof(npc_name));
			if (strcmp(m_game->m_client_list[client_h]->m_map_name, "aresden") == 0)
				strcpy(npc_name, "Guard-Aresden");
			else if (strcmp(m_game->m_client_list[client_h]->m_map_name, "elvine") == 0)
				strcpy(npc_name, "Guard-Elvine");
			else  if (strcmp(m_game->m_client_list[client_h]->m_map_name, "default") == 0)
				strcpy(npc_name, "Guard-Neutral");

			naming_value = m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_empty_naming_value();
			if (naming_value == -1) {

			}
			else {
				std::memset(npc_waypoint, 0, sizeof(npc_waypoint));
				std::memset(name, 0, sizeof(name));
				std::snprintf(name, sizeof(name), "XX%d", naming_value);
				name[0] = '_';
				name[1] = m_game->m_client_list[client_h]->m_map_index + 65;

				tX = (int)m_game->m_client_list[client_h]->m_x;
				tY = (int)m_game->m_client_list[client_h]->m_y;
				int npc_config_id = m_game->get_npc_config_id_by_name(npc_name);
				if (m_game->create_new_npc(npc_config_id, name, m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->m_name, 0, 0, MoveType::Random,
					&tX, &tY, npc_waypoint, 0, 0, -1, false, true) == false) {
					m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->set_naming_value_empty(naming_value);
				}
				else {
					if (m_game->m_entity_manager != 0) m_game->m_entity_manager->set_npc_attack_mode(name, client_h, hb::shared::owner_class::Player, true);
				}
			}
		}
	}
	return false;
}

bool CombatManager::get_is_player_hostile(int client_h, int owner_h)
{
	if (m_game->m_client_list[client_h] == 0) return false;
	if (m_game->m_client_list[owner_h] == 0) return false;

	if (client_h == owner_h) return true;

	if (m_game->m_client_list[client_h]->m_side == 0) {
		if (m_game->m_client_list[owner_h]->m_player_kill_count != 0)
			return true;
		else return false;
	}
	else {
		if (m_game->m_client_list[client_h]->m_side != m_game->m_client_list[owner_h]->m_side) {
			if (m_game->m_client_list[owner_h]->m_side == 0) {
				if (m_game->m_client_list[owner_h]->m_player_kill_count != 0)
					return true;
				else return false;
			}
			else return true;
		}
		else {
			if (m_game->m_client_list[owner_h]->m_player_kill_count != 0)
				return true;
			else return false;
		}
	}

	return false;
}

void CombatManager::poison_effect(int client_h, int v1)
{
	int poison_level, damage, prev_hp, prob;

	if (m_game->m_client_list[client_h] == 0)     return;
	if (m_game->m_client_list[client_h]->m_is_killed) return;
	if (m_game->m_client_list[client_h]->m_is_init_complete == false) return;

	// GM mode: instantly cure poison instead of taking damage
	if (m_game->m_client_list[client_h]->m_is_gm_mode)
	{
		m_game->m_client_list[client_h]->m_is_poisoned = false;
		m_game->m_status_effect_manager->set_poison_flag(client_h, hb::shared::owner_class::Player, false);
		m_game->send_notify_msg(0, client_h, Notify::MagicEffectOff, hb::shared::magic::Poison, 0, 0, 0);
		return;
	}

	poison_level = m_game->m_client_list[client_h]->m_poison_level;

	damage = m_game->dice(1, poison_level);

	prev_hp = m_game->m_client_list[client_h]->m_hp;
	m_game->m_client_list[client_h]->m_hp -= damage;
	if (m_game->m_client_list[client_h]->m_hp <= 0) m_game->m_client_list[client_h]->m_hp = 1;

	if (prev_hp != m_game->m_client_list[client_h]->m_hp)
	{
		m_game->send_notify_msg(0, client_h, Notify::Hp, 0, 0, 0, 0);
		char buf[64]{};
		std::snprintf(buf, sizeof(buf), "You took -%d poison damage.", damage);
		m_game->send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0, buf);
	}

	prob = m_game->m_client_list[client_h]->m_skill_mastery[23] - 10 + m_game->m_client_list[client_h]->m_add_poison_resistance;
	if (prob <= 10) prob = 10;
	if (m_game->dice(1, 100) <= static_cast<uint32_t>(prob)) {
		m_game->m_client_list[client_h]->m_is_poisoned = false;
		m_game->m_status_effect_manager->set_poison_flag(client_h, hb::shared::owner_class::Player, false);
		m_game->send_notify_msg(0, client_h, Notify::MagicEffectOff, hb::shared::magic::Poison, 0, 0, 0);
		m_game->send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0, "Poison has been cured.");
	}
}

bool CombatManager::check_resisting_poison_success(short owner_h, char owner_type)
{
	int resist, result;

	switch (owner_type) {
	case hb::shared::owner_class::Player:
		if (m_game->m_client_list[owner_h] == 0) return false;
		resist = m_game->m_client_list[owner_h]->m_skill_mastery[23] + m_game->m_client_list[owner_h]->m_add_poison_resistance;
		break;

	case hb::shared::owner_class::Npc:
		if (m_game->m_npc_list[owner_h] == 0) return false;
		resist = 0;
		break;
	}

	result = m_game->dice(1, 100);
	if (result >= resist)
		return false;

	if (owner_type == hb::shared::owner_class::Player)
		m_game->m_skill_manager->calculate_ssn_skill_index(owner_h, 23, 1);

	return true;
}

int CombatManager::get_player_relationship_raw(int client_h, int opponent_h)
{
	int ret;

	if (m_game->m_client_list[client_h] == 0) return 0;
	if (m_game->m_client_list[client_h]->m_is_init_complete == false) return 0;

	if (m_game->m_client_list[opponent_h] == 0) return 0;
	if (m_game->m_client_list[opponent_h]->m_is_init_complete == false) return 0;

	ret = 0;

	if (m_game->m_client_list[client_h]->m_player_kill_count != 0) {
		if ((memcmp(m_game->m_client_list[client_h]->m_location, m_game->m_client_list[opponent_h]->m_location, 10) == 0) &&
			(memcmp(m_game->m_client_list[client_h]->m_location, "NONE", 4) != 0) && (memcmp(m_game->m_client_list[opponent_h]->m_location, "NONE", 4) != 0)) {
			ret = 7;
		}
		else ret = 2;
	}
	else if (m_game->m_client_list[opponent_h]->m_player_kill_count != 0) {
		if ((memcmp(m_game->m_client_list[client_h]->m_location, m_game->m_client_list[opponent_h]->m_location, 10) == 0) &&
			(memcmp(m_game->m_client_list[client_h]->m_location, "NONE", 4) != 0))
			ret = 6;
		else ret = 2;
	}
	else {
		if (m_game->m_client_list[client_h]->m_side != m_game->m_client_list[opponent_h]->m_side) {
			if ((m_game->m_client_list[client_h]->m_side != 0) && (m_game->m_client_list[opponent_h]->m_side != 0)) {
				// 0(Traveler)  .
				ret = 2;
			}
			else {
				ret = 0;
			}
		}
		else {
			// Guild system removed - same-faction players are allies (ret=1), different faction are enemies
			if ((memcmp(m_game->m_client_list[client_h]->m_location, m_game->m_client_list[opponent_h]->m_location, 10) == 0) &&
				(memcmp(m_game->m_client_list[client_h]->m_location, "NONE", 4) != 0) &&
				(memcmp(m_game->m_client_list[opponent_h]->m_location, "NONE", 4) != 0)) {
				ret = 4;
			}
			else ret = 1;
		}
	}

	return ret;
}

EntityRelationship CombatManager::get_player_relationship(int owner_h, int viewer_h)
{
	if (m_game->m_client_list[owner_h] == 0 || m_game->m_client_list[viewer_h] == 0)
		return EntityRelationship::Neutral;

	// Viewer is PK → everyone is enemy to them
	if (m_game->m_client_list[viewer_h]->m_player_kill_count != 0)
		return EntityRelationship::Enemy;

	// Target is PK
	if (m_game->m_client_list[owner_h]->m_player_kill_count != 0)
		return EntityRelationship::PK;

	// No faction = neutral
	if (m_game->m_client_list[owner_h]->m_side == 0 || m_game->m_client_list[viewer_h]->m_side == 0)
		return EntityRelationship::Neutral;

	// Same faction = friendly
	if (m_game->m_client_list[owner_h]->m_side == m_game->m_client_list[viewer_h]->m_side)
		return EntityRelationship::Friendly;

	// Different factions
	if (m_game->m_is_crusade_mode)
		return EntityRelationship::Enemy;

	// Both are combatants (non-hunter) = enemy
	if (!m_game->m_client_list[viewer_h]->m_is_player_civil && !m_game->m_client_list[owner_h]->m_is_player_civil)
		return EntityRelationship::Enemy;

	return EntityRelationship::Neutral;
}

void CombatManager::check_attack_type(int client_h, short* spType)
{
	if (m_game->m_client_list[client_h] == 0) return;
	auto wc = m_game->m_client_list[client_h]->get_equipped_weapon_class();

	switch (*spType) {
	case 2:
		// Effect  .
		if (m_game->m_client_list[client_h]->m_arrow_index == -1) *spType = 0;
		if (wc != weapon_class::bow) *spType = 1;
		break;

	case 20:
		if (m_game->m_client_list[client_h]->m_super_attack_left <= 0)  *spType = 1;
		if (m_game->m_client_list[client_h]->m_skill_mastery[5] < 100) *spType = 1;
		break;

	case 21:
		if (m_game->m_client_list[client_h]->m_super_attack_left <= 0)  *spType = 1;
		if (m_game->m_client_list[client_h]->m_skill_mastery[7] < 100) *spType = 1;
		break;

	case 22:
		if (m_game->m_client_list[client_h]->m_super_attack_left <= 0)  *spType = 1;
		if (m_game->m_client_list[client_h]->m_skill_mastery[9] < 100) *spType = 1;
		break;

	case 23:
		if (m_game->m_client_list[client_h]->m_super_attack_left <= 0)   *spType = 1;
		if (m_game->m_client_list[client_h]->m_skill_mastery[8] < 100) *spType = 1;
		break;

	case 24:
		if (m_game->m_client_list[client_h]->m_super_attack_left <= 0)  *spType = 1;
		if (m_game->m_client_list[client_h]->m_skill_mastery[10] < 100) *spType = 1;
		break;

	case 25:
		if (m_game->m_client_list[client_h]->m_super_attack_left <= 0)  *spType = 2;
		if (m_game->m_client_list[client_h]->m_skill_mastery[6] < 100) *spType = 2;
		if (m_game->m_client_list[client_h]->m_arrow_index == -1)      *spType = 0;
		if (wc != weapon_class::bow) *spType = 1;
		break;

	case 26:
		if (m_game->m_client_list[client_h]->m_super_attack_left <= 0)  *spType = 1;
		if (m_game->m_client_list[client_h]->m_skill_mastery[14] < 100) *spType = 1;
		break;

	case 27:
		if (m_game->m_client_list[client_h]->m_super_attack_left <= 0)  *spType = 1;
		if (m_game->m_client_list[client_h]->m_skill_mastery[21] < 100) *spType = 1;
		break;
	}
}

void CombatManager::check_fire_bluring(char map_index, int sX, int sY)
{
	int item_num;

	for(int ix = sX - 1; ix <= sX + 1; ix++)
		for(int iy = sY - 1; iy <= sY + 1; iy++) {
			item_num = m_game->m_map_list[map_index]->check_item(ix, iy);

			switch (item_num) {
			case 355:
			{
				CItem* remain = nullptr;
				CItem* item = m_game->m_map_list[map_index]->get_item(ix, iy, &remain);
				if (item != nullptr) delete item;
				m_game->m_dynamic_object_manager->add_dynamic_object_list(0, 0, dynamic_object::Fire, map_index, ix, iy, 6000);

				m_game->send_ground_item_event(CommonType::SetItem, map_index, ix, iy, remain);
				break;
			}
			}
		}
}

int CombatManager::get_weapon_skill_type(int client_h)
{
	if (m_game->m_client_list[client_h] == 0) return 0;

	auto wc = m_game->m_client_list[client_h]->get_equipped_weapon_class();

	switch (wc) {
	case weapon_class::none:        return 5;  // hand combat
	case weapon_class::dagger:      return 7;  // short sword / dagger
	case weapon_class::short_sword: return 7;  // short sword
	case weapon_class::long_sword:  return 8;  // long sword
	case weapon_class::fencing:     return 9;  // fencing
	case weapon_class::axe:         return 10; // axe
	case weapon_class::hammer:      return 14; // hammer / club
	case weapon_class::wand:        return 21; // staff / wand
	case weapon_class::bow:         return 6;  // archery
	default:                        return 1;
	}
}

int CombatManager::get_combo_attack_bonus(int skill, int combo_count)
{
	if (combo_count <= 1) return 0;
	if (combo_count > 6) return 0;
	switch (skill) {
	case 5:
		return ___iCAB5[combo_count];
		break;
	case 6:
		return ___iCAB6[combo_count];
		break;
	case 7:
		return ___iCAB7[combo_count];
		break;
	case 8:
		return ___iCAB8[combo_count];
		break;
	case 9:
		return ___iCAB9[combo_count];
		break;
	case 10:
		return ___iCAB10[combo_count];
		break;
	case 14:
		return ___iCAB6[combo_count];
		break;
	case 21:
		return ___iCAB10[combo_count];
		break;
	}

	return 0;
}

void CombatManager::armor_life_decrement(int attacker_h, int target_h, char owner_type, int value)
{
	int temp;

	hb::logger::debug<log_channel::events>("armor_life_decrement: attacker={} target={} owner_type={} value={}", attacker_h, target_h, (int)owner_type, value);

	if (m_game->m_client_list[attacker_h] == 0) { hb::logger::debug<log_channel::events>("armor_life_decrement: attacker null, returning"); return; }
	switch (owner_type) {
	case hb::shared::owner_class::Player:
		if (m_game->m_client_list[target_h] == 0) { hb::logger::debug<log_channel::events>("armor_life_decrement: target null, returning"); return; }
		break;

	case hb::shared::owner_class::Npc:
		hb::logger::debug<log_channel::events>("armor_life_decrement: target is NPC, returning");
		return;
	default:
		hb::logger::debug<log_channel::events>("armor_life_decrement: unknown owner_type={}, returning", (int)owner_type);
		return;
	}

	hb::logger::debug<log_channel::events>("armor_life_decrement: passed all guards, reducing armor for target={}", target_h);

	temp = m_game->m_client_list[target_h]->m_item_equipment_status[to_int(EquipPos::Body)];
	if ((temp != -1) && (m_game->m_client_list[target_h]->m_item_list[temp] != 0)) {
		if (m_game->m_client_list[target_h]->m_item_list[temp]->m_instance.cur_durability > 0) {
			if (m_game->m_client_list[target_h]->m_item_list[temp]->m_instance.cur_durability < static_cast<uint16_t>(value))
				m_game->m_client_list[target_h]->m_item_list[temp]->m_instance.cur_durability = 0;
			else
				m_game->m_client_list[target_h]->m_item_list[temp]->m_instance.cur_durability -= value;
			m_game->send_notify_msg(0, target_h, Notify::CurDurability, temp, m_game->m_client_list[target_h]->m_item_list[temp]->m_instance.cur_durability, 0, 0);
		}
		if (m_game->m_client_list[target_h]->m_item_list[temp]->m_instance.cur_durability == 0) {
			m_game->send_notify_msg(0, target_h, Notify::ItemDurabilityEnd, m_game->m_client_list[target_h]->m_item_list[temp]->m_equip_pos, temp, 0, 0);
			m_game->m_item_manager->release_item_handler(target_h, temp, true);
		}
	}

	temp = m_game->m_client_list[target_h]->m_item_equipment_status[to_int(EquipPos::Leggings)];
	if ((temp != -1) && (m_game->m_client_list[target_h]->m_item_list[temp] != 0)) {
		if (m_game->m_client_list[target_h]->m_item_list[temp]->m_instance.cur_durability > 0) {
			if (m_game->m_client_list[target_h]->m_item_list[temp]->m_instance.cur_durability < static_cast<uint16_t>(value))
				m_game->m_client_list[target_h]->m_item_list[temp]->m_instance.cur_durability = 0;
			else
				m_game->m_client_list[target_h]->m_item_list[temp]->m_instance.cur_durability -= value;
			m_game->send_notify_msg(0, target_h, Notify::CurDurability, temp, m_game->m_client_list[target_h]->m_item_list[temp]->m_instance.cur_durability, 0, 0);
		}
		if (m_game->m_client_list[target_h]->m_item_list[temp]->m_instance.cur_durability == 0) {
			m_game->send_notify_msg(0, target_h, Notify::ItemDurabilityEnd, m_game->m_client_list[target_h]->m_item_list[temp]->m_equip_pos, temp, 0, 0);
			m_game->m_item_manager->release_item_handler(target_h, temp, true);
		}
	}

	temp = m_game->m_client_list[target_h]->m_item_equipment_status[to_int(EquipPos::Boots)];
	if ((temp != -1) && (m_game->m_client_list[target_h]->m_item_list[temp] != 0)) {
		if (m_game->m_client_list[target_h]->m_item_list[temp]->m_instance.cur_durability > 0) {
			if (m_game->m_client_list[target_h]->m_item_list[temp]->m_instance.cur_durability < static_cast<uint16_t>(value))
				m_game->m_client_list[target_h]->m_item_list[temp]->m_instance.cur_durability = 0;
			else
				m_game->m_client_list[target_h]->m_item_list[temp]->m_instance.cur_durability -= value;
			m_game->send_notify_msg(0, target_h, Notify::CurDurability, temp, m_game->m_client_list[target_h]->m_item_list[temp]->m_instance.cur_durability, 0, 0);
		}
		if (m_game->m_client_list[target_h]->m_item_list[temp]->m_instance.cur_durability == 0) {
			m_game->send_notify_msg(0, target_h, Notify::ItemDurabilityEnd, m_game->m_client_list[target_h]->m_item_list[temp]->m_equip_pos, temp, 0, 0);
			m_game->m_item_manager->release_item_handler(target_h, temp, true);
		}
	}

	temp = m_game->m_client_list[target_h]->m_item_equipment_status[to_int(EquipPos::Arms)];
	if ((temp != -1) && (m_game->m_client_list[target_h]->m_item_list[temp] != 0)) {
		if (m_game->m_client_list[target_h]->m_item_list[temp]->m_instance.cur_durability > 0) {
			if (m_game->m_client_list[target_h]->m_item_list[temp]->m_instance.cur_durability < static_cast<uint16_t>(value))
				m_game->m_client_list[target_h]->m_item_list[temp]->m_instance.cur_durability = 0;
			else
				m_game->m_client_list[target_h]->m_item_list[temp]->m_instance.cur_durability -= value;
			m_game->send_notify_msg(0, target_h, Notify::CurDurability, temp, m_game->m_client_list[target_h]->m_item_list[temp]->m_instance.cur_durability, 0, 0);
		}
		if (m_game->m_client_list[target_h]->m_item_list[temp]->m_instance.cur_durability == 0) {
			m_game->send_notify_msg(0, target_h, Notify::ItemDurabilityEnd, m_game->m_client_list[target_h]->m_item_list[temp]->m_equip_pos, temp, 0, 0);
			m_game->m_item_manager->release_item_handler(target_h, temp, true);
		}
	}

	temp = m_game->m_client_list[target_h]->m_item_equipment_status[to_int(EquipPos::Head)];
	if ((temp != -1) && (m_game->m_client_list[target_h]->m_item_list[temp] != 0)) {
		if (m_game->m_client_list[target_h]->m_item_list[temp]->m_instance.cur_durability > 0) {
			if (m_game->m_client_list[target_h]->m_item_list[temp]->m_instance.cur_durability < static_cast<uint16_t>(value))
				m_game->m_client_list[target_h]->m_item_list[temp]->m_instance.cur_durability = 0;
			else
				m_game->m_client_list[target_h]->m_item_list[temp]->m_instance.cur_durability -= value;
			m_game->send_notify_msg(0, target_h, Notify::CurDurability, temp, m_game->m_client_list[target_h]->m_item_list[temp]->m_instance.cur_durability, 0, 0);
		}
		if (m_game->m_client_list[target_h]->m_item_list[temp]->m_instance.cur_durability == 0) {
			m_game->send_notify_msg(0, target_h, Notify::ItemDurabilityEnd, m_game->m_client_list[target_h]->m_item_list[temp]->m_equip_pos, temp, 0, 0);
			m_game->m_item_manager->release_item_handler(target_h, temp, true);
		}
	}
}

bool CombatManager::pk_log(int action, int attacker_h, int victum_h, char* npc)
{
	char  txt[1024], temp1[120], temp2[120];

	std::memset(txt, 0, sizeof(txt));
	std::memset(temp1, 0, sizeof(temp1));
	std::memset(temp2, 0, sizeof(temp2));

	if (m_game->m_client_list[victum_h] == 0) return false;

	switch (action) {

	case PkLog::ReduceCriminal:
		std::snprintf(txt, sizeof(txt), "(%s) PC(%s)\tReduce\tCC(%d)\t%s(%d %d)\t", m_game->m_client_list[victum_h]->m_ip_address, m_game->m_client_list[victum_h]->m_char_name, m_game->m_client_list[victum_h]->m_player_kill_count,
			m_game->m_client_list[victum_h]->m_map_name, m_game->m_client_list[victum_h]->m_x, m_game->m_client_list[victum_h]->m_y);
		break;

	case PkLog::ByPlayer:
		if (m_game->m_client_list[attacker_h] == 0) return false;
		std::snprintf(txt, sizeof(txt), "(%s) PC(%s)\tKilled by PC\t \t%s(%d %d)\t(%s) PC(%s)", m_game->m_client_list[victum_h]->m_ip_address, m_game->m_client_list[victum_h]->m_char_name,
			m_game->m_client_list[victum_h]->m_map_name, m_game->m_client_list[victum_h]->m_x, m_game->m_client_list[victum_h]->m_y, m_game->m_client_list[attacker_h]->m_ip_address, m_game->m_client_list[attacker_h]->m_char_name);
		break;
	case PkLog::ByPk:
		if (m_game->m_client_list[attacker_h] == 0) return false;
		std::snprintf(txt, sizeof(txt), "(%s) PC(%s)\tKilled by PK\tCC(%d)\t%s(%d %d)\t(%s) PC(%s)", m_game->m_client_list[victum_h]->m_ip_address, m_game->m_client_list[victum_h]->m_char_name, m_game->m_client_list[attacker_h]->m_player_kill_count,
			m_game->m_client_list[victum_h]->m_map_name, m_game->m_client_list[victum_h]->m_x, m_game->m_client_list[victum_h]->m_y, m_game->m_client_list[attacker_h]->m_ip_address, m_game->m_client_list[attacker_h]->m_char_name);
		break;
	case PkLog::ByEnemy:
		if (m_game->m_client_list[attacker_h] == 0) return false;
		std::snprintf(txt, sizeof(txt), "(%s) PC(%s)\tKilled by Enemy\t \t%s(%d %d)\t(%s) PC(%s)", m_game->m_client_list[victum_h]->m_ip_address, m_game->m_client_list[victum_h]->m_char_name,
			m_game->m_client_list[victum_h]->m_map_name, m_game->m_client_list[victum_h]->m_x, m_game->m_client_list[victum_h]->m_y, m_game->m_client_list[attacker_h]->m_ip_address, m_game->m_client_list[attacker_h]->m_char_name);
		break;
	case PkLog::ByNpc:
		if (npc == 0) return false;
		std::snprintf(txt, sizeof(txt), "(%s) PC(%s)\tKilled by NPC\t \t%s(%d %d)\tNPC(%s)", m_game->m_client_list[victum_h]->m_ip_address, m_game->m_client_list[victum_h]->m_char_name,
			m_game->m_client_list[victum_h]->m_map_name, m_game->m_client_list[victum_h]->m_x, m_game->m_client_list[victum_h]->m_y, npc);
		break;
	case PkLog::ByOther:
		break;
	default:
		return false;
	}
	hb::logger::log<log_channel::pvp>("({}) PC({})\tKilled by Other\t \t{}({} {})\tUnknown", m_game->m_client_list[victum_h]->m_ip_address, m_game->m_client_list[victum_h]->m_char_name, m_game->m_client_list[victum_h]->m_map_name, m_game->m_client_list[victum_h]->m_x, m_game->m_client_list[victum_h]->m_y);
	return true;
}

bool CombatManager::check_client_attack_frequency(int client_h, uint32_t client_time)
{
	if (m_game->m_client_list[client_h] == 0) return false;

	if (m_game->m_client_list[client_h]->m_attack_freq_time == 0)
		m_game->m_client_list[client_h]->m_attack_freq_time = client_time;
	else {
		uint32_t time_gap = client_time - m_game->m_client_list[client_h]->m_attack_freq_time;
		m_game->m_client_list[client_h]->m_attack_freq_time = client_time;

		// Compute expected minimum swing time from player's weapon speed and status effects.
		constexpr int TOLERANCE_MS = 50;

		const auto& status = m_game->m_client_list[client_h]->m_status;
		int base_swing = hb::shared::calc::swing_time(status.attack_delay);
		int effective_swing = base_swing;
		if (status.frozen) effective_swing += hb::shared::balance::swing_frames * (hb::shared::balance::base_frame_time >> 2);
		if (status.haste)  effective_swing -= hb::shared::balance::swing_frames * static_cast<int>(hb::shared::balance::run_frame_time / 2.3);

		int expectedSwingTime = effective_swing;
		int threshold = expectedSwingTime - TOLERANCE_MS;
		if (threshold < 200) threshold = 200;

		if (time_gap < static_cast<uint32_t>(threshold)) {
			try
			{
				hb::logger::warn<log_channel::security>("Swing hack: IP={} player={}, irregular attack rate (gap={}ms min={}ms)", m_game->m_client_list[client_h]->m_ip_address, m_game->m_client_list[client_h]->m_char_name, time_gap, expectedSwingTime);
				m_game->delete_client(client_h, true, true);
			}
			catch (...)
			{
			}
			return false;
		}
	}

	return false;
}

bool CombatManager::calculate_durability_decrement(short target_h, short attacker_h, char attacker_type, char target_type, int armor_type)
{
	int down_value = 1, hammer_chance = 100, item_index;
	auto wc = weapon_class::none;

	if (m_game->m_client_list[target_h] == 0) return false;

	// Player-specific weapon skill logic (only when attacker is a player)
	if (attacker_type == hb::shared::owner_class::Player) {
		if (attacker_h > MaxClients) return false;
		if (m_game->m_client_list[attacker_h] == 0) return false;
		wc = m_game->m_client_list[attacker_h]->get_equipped_weapon_class();
		if ((target_type == hb::shared::owner_class::Player) && (m_game->m_client_list[target_h]->m_side != m_game->m_client_list[attacker_h]->m_side)) {
			switch (m_game->m_client_list[attacker_h]->m_using_weapon_skill) {
			case 14:
				if (wc == weapon_class::hammer) {
					item_index = m_game->m_client_list[attacker_h]->m_item_equipment_status[to_int(EquipPos::TwoHand)];
					if ((item_index != -1) && (m_game->m_client_list[attacker_h]->m_item_list[item_index] != 0)) {
						if (m_game->m_client_list[attacker_h]->m_item_list[item_index]->m_id_num == 761) { // BattleHammer
							down_value = 30;
						}
						if (m_game->m_client_list[attacker_h]->m_item_list[item_index]->m_id_num == 762) { // GiantBattleHammer
							down_value = 35;
						}
						if (m_game->m_client_list[attacker_h]->m_item_list[item_index]->m_id_num == 843) { // BarbarianHammer
							down_value = 30;
						}
						if (m_game->m_client_list[attacker_h]->m_item_list[item_index]->m_id_num == 745) { // MasterBattleHammer
							down_value = 30;
						}
						break;
					}
				}
				else {
					down_value = 20;
				}
				break;
			case 10:
				down_value = 3;
				break;
			default:
				down_value = 1;
				break;
			}
		}
	}
	// NPC attackers use default down_value = 1
	if (m_game->m_client_list[target_h]->m_is_special_ability_enabled) {
		switch (m_game->m_client_list[target_h]->m_special_ability_type) {
		case 52:
			down_value = 0;
			hammer_chance = 0;
			break;
		}
	}
	if (m_game->m_client_list[target_h]->m_item_list[armor_type]->m_instance.cur_durability > 0) {
		if (m_game->m_client_list[target_h]->m_item_list[armor_type]->m_instance.cur_durability < static_cast<uint16_t>(down_value))
			m_game->m_client_list[target_h]->m_item_list[armor_type]->m_instance.cur_durability = 0;
		else
			m_game->m_client_list[target_h]->m_item_list[armor_type]->m_instance.cur_durability -= down_value;
		m_game->send_notify_msg(0, target_h, Notify::CurDurability, armor_type, m_game->m_client_list[target_h]->m_item_list[armor_type]->m_instance.cur_durability, 0, 0);
	}
	if (m_game->m_client_list[target_h]->m_item_list[armor_type]->m_instance.cur_durability == 0) {
		m_game->m_client_list[target_h]->m_item_list[armor_type]->m_instance.cur_durability = 0;
		m_game->send_notify_msg(0, target_h, Notify::ItemDurabilityEnd, m_game->m_client_list[target_h]->m_item_list[armor_type]->m_equip_pos, armor_type, 0, 0);
		m_game->m_item_manager->release_item_handler(target_h, armor_type, true);
		return true;
	}
	if ((attacker_type == hb::shared::owner_class::Player) && (target_type == hb::shared::owner_class::Player) && (m_game->m_client_list[attacker_h]->m_using_weapon_skill == 14) && (hammer_chance == 100)) {
		if (m_game->m_client_list[target_h]->m_item_list[armor_type]->m_durability < 2000) {
			hammer_chance = m_game->dice(6, (m_game->m_client_list[target_h]->m_item_list[armor_type]->m_durability - m_game->m_client_list[target_h]->m_item_list[armor_type]->m_instance.cur_durability));
		}
		else {
			hammer_chance = m_game->dice(4, (m_game->m_client_list[target_h]->m_item_list[armor_type]->m_durability - m_game->m_client_list[target_h]->m_item_list[armor_type]->m_instance.cur_durability));
		}
		if (wc == weapon_class::hammer) {
			item_index = m_game->m_client_list[attacker_h]->m_item_equipment_status[to_int(EquipPos::TwoHand)];
			if ((item_index != -1) && (m_game->m_client_list[attacker_h]->m_item_list[item_index] != 0)) {
				if (m_game->m_client_list[attacker_h]->m_item_list[item_index]->m_id_num == 761) { // BattleHammer
					hammer_chance -= hammer_chance >> 1;
				}
				if (m_game->m_client_list[attacker_h]->m_item_list[item_index]->m_id_num == 762) { // GiantBattleHammer
					hammer_chance = (((hammer_chance * 5) + 7) >> 3);
				}
				if (m_game->m_client_list[attacker_h]->m_item_list[item_index]->m_id_num == 843) { // BarbarianHammer
					hammer_chance = (((hammer_chance * 5) + 7) >> 3);
				}
				if (m_game->m_client_list[attacker_h]->m_item_list[item_index]->m_id_num == 745) { // MasterBattleHammer
					hammer_chance = (((hammer_chance * 5) + 7) >> 3);
				}
			}
			else {
				hammer_chance = ((hammer_chance + 3) >> 2);
			}
			switch (m_game->m_client_list[target_h]->m_item_list[armor_type]->m_id_num) {
			case 621:
			case 622:
				hammer_chance = 0;
				break;
			}
			if (m_game->m_client_list[target_h]->m_item_list[armor_type]->m_instance.cur_durability < hammer_chance) {
				m_game->m_item_manager->release_item_handler(target_h, armor_type, true);
				m_game->send_notify_msg(0, target_h, Notify::ItemReleased, m_game->m_client_list[target_h]->m_item_list[armor_type]->m_equip_pos, armor_type, 0, 0);
			}
		}
	}
	return true;
}

uint32_t CombatManager::calculate_attack_effect(short target_h, char target_type, short attacker_h, char attacker_type, int tdX, int tdY, int attack_mode, bool near_attack, bool is_dash, bool arrow_use)
{
	int    iAP_SM, iAP_L, attacker_hit_ratio, target_defense_ratio, dest_hit_ratio, result, iAP_Abs_Armor, iAP_Abs_Shield;
	char   attacker_name[hb::shared::limits::NpcNameLen], attacker_side, protect, crop_skill, farming_skill;
	direction attacker_dir, target_dir;
	short  weapon_index, attacker_weapon, dX, dY, sX, sY, atk_x, atk_y, tgt_x, tgt_y;
	uint32_t  time;
	weapon_class::weapon_class wc;
	double tmp1, tmp2, tmp3;
	bool   killed;
	bool   normal_missile_attack;
	bool   is_attacker_berserk;
	int    killed_dice, damage, exp, wep_life_off, side_condition, max_super_attack, weapon_skill, combo_bonus, temp;
	int    attacker_hp, move_damage, rep_damage;
	char   attacker_sa;
	int    attacker_s_avalue, hit_point;
	direction damage_move_dir;
	int    party_id, construction_point, war_contribution, tX, tY, dst1, dst2;
	short item_index;
	short skill_used;

	time = GameClock::GetTimeMS();
	killed = false;
	exp = 0;
	party_id = 0;
	normal_missile_attack = false;
	std::memset(attacker_name, 0, sizeof(attacker_name));
	attacker_sa = 0;
	attacker_s_avalue = 0;
	wc = weapon_class::none;

	switch (attacker_type) {
	case hb::shared::owner_class::Player:

		if (m_game->m_client_list[attacker_h] == 0) return 0;
		if ((m_game->m_map_list[m_game->m_client_list[attacker_h]->m_map_index]->m_is_attack_enabled == false)) return 0;
		if ((m_game->m_map_list[m_game->m_client_list[attacker_h]->m_map_index] == 0) && (m_game->m_map_list[m_game->m_client_list[attacker_h]->m_map_index]->m_is_heldenian_map) && (m_game->m_is_heldenian_mode)) return 0;
		if ((m_game->m_is_crusade_mode == false) && (m_game->m_client_list[attacker_h]->m_is_player_civil) && (target_type == hb::shared::owner_class::Player)) return 0;

		if (m_game->m_client_list[attacker_h]->m_status.invisibility) {
			m_game->m_status_effect_manager->set_invisibility_flag(attacker_h, hb::shared::owner_class::Player, false);
			m_game->m_delay_event_manager->remove_from_delay_event_list(attacker_h, hb::shared::owner_class::Player, hb::shared::magic::Invisibility);
			m_game->m_client_list[attacker_h]->m_magic_effect_status[hb::shared::magic::Invisibility] = 0;
		}

		if (!m_game->m_client_list[attacker_h]->m_appearance.is_walking) return 0;

		iAP_SM = 0;
		iAP_L = 0;

		wc = m_game->m_client_list[attacker_h]->get_equipped_weapon_class();

		skill_used = m_game->m_client_list[attacker_h]->m_using_weapon_skill;
		if ((is_dash) && (m_game->m_client_list[attacker_h]->m_skill_mastery[skill_used] != 100) && (wc != weapon_class::axe)) {
			try
			{
				hb::logger::warn<log_channel::security>("Fullswing hack: IP={} player={}, dashing with weapon skill {}", m_game->m_client_list[attacker_h]->m_ip_address, m_game->m_client_list[attacker_h]->m_char_name, m_game->m_client_list[attacker_h]->m_skill_mastery[skill_used]);
				m_game->delete_client(attacker_h, true, true);
			}
			catch (...)
			{

			}
			return 0;
		}

		attacker_side = m_game->m_client_list[attacker_h]->m_side;

		if (wc == weapon_class::none) {
			iAP_SM = iAP_L = m_game->dice(1, ((m_game->m_client_list[attacker_h]->m_str + m_game->m_client_list[attacker_h]->m_angelic_str) / 12));
			if (iAP_SM <= 0) iAP_SM = 1;
			if (iAP_L <= 0) iAP_L = 1;
			attacker_hit_ratio = m_game->m_client_list[attacker_h]->m_hit_ratio + m_game->m_client_list[attacker_h]->m_skill_mastery[5];
			m_game->m_client_list[attacker_h]->m_using_weapon_skill = 5;

		}
		else if (wc != weapon_class::bow) {
			iAP_SM = m_game->dice(m_game->m_client_list[attacker_h]->m_attack_dice_throw_sm, m_game->m_client_list[attacker_h]->m_attack_dice_range_sm);
			iAP_L = m_game->dice(m_game->m_client_list[attacker_h]->m_attack_dice_throw_l, m_game->m_client_list[attacker_h]->m_attack_dice_range_l);

			iAP_SM += m_game->m_client_list[attacker_h]->m_attack_bonus_sm;
			iAP_L += m_game->m_client_list[attacker_h]->m_attack_bonus_l;

			attacker_hit_ratio = m_game->m_client_list[attacker_h]->m_hit_ratio;

			tmp1 = (double)iAP_SM;
			tmp2 = (double)(m_game->m_client_list[attacker_h]->m_str + m_game->m_client_list[attacker_h]->m_angelic_str);

			tmp2 = tmp2 / 5.0f;
			tmp3 = tmp1 + (tmp1 * (tmp2 / 100.0f));
			iAP_SM = (int)(tmp3 + 0.5f);

			tmp1 = (double)iAP_L;
			tmp2 = (double)(m_game->m_client_list[attacker_h]->m_str + m_game->m_client_list[attacker_h]->m_angelic_str);

			tmp2 = tmp2 / 5.0f;
			tmp3 = tmp1 + (tmp1 * (tmp2 / 100.0f));
			iAP_L = (int)(tmp3 + 0.5f);
		}
		else if (wc == weapon_class::bow) {
			iAP_SM = m_game->dice(m_game->m_client_list[attacker_h]->m_attack_dice_throw_sm, m_game->m_client_list[attacker_h]->m_attack_dice_range_sm);
			iAP_L = m_game->dice(m_game->m_client_list[attacker_h]->m_attack_dice_throw_l, m_game->m_client_list[attacker_h]->m_attack_dice_range_l);

			iAP_SM += m_game->m_client_list[attacker_h]->m_attack_bonus_sm;
			iAP_L += m_game->m_client_list[attacker_h]->m_attack_bonus_l;

			attacker_hit_ratio = m_game->m_client_list[attacker_h]->m_hit_ratio;
			normal_missile_attack = true;

			iAP_SM += m_game->dice(1, ((m_game->m_client_list[attacker_h]->m_str + m_game->m_client_list[attacker_h]->m_angelic_str) / 20));
			iAP_L += m_game->dice(1, ((m_game->m_client_list[attacker_h]->m_str + m_game->m_client_list[attacker_h]->m_angelic_str) / 20));
		}

		attacker_hit_ratio += 50;
		if (iAP_SM <= 0) iAP_SM = 1;
		if (iAP_L <= 0) iAP_L = 1;

		if (m_game->m_client_list[attacker_h]->m_custom_item_value_attack != 0) {
			if ((m_game->m_client_list[attacker_h]->m_min_attack_power_sm != 0) && (iAP_SM < m_game->m_client_list[attacker_h]->m_min_attack_power_sm)) {
				iAP_SM = m_game->m_client_list[attacker_h]->m_min_attack_power_sm;
			}
			if ((m_game->m_client_list[attacker_h]->m_min_attack_power_l != 0) && (iAP_L < m_game->m_client_list[attacker_h]->m_min_attack_power_l)) {
				iAP_L = m_game->m_client_list[attacker_h]->m_min_attack_power_l;
			}
			if ((m_game->m_client_list[attacker_h]->m_max_attack_power_sm != 0) && (iAP_SM > m_game->m_client_list[attacker_h]->m_max_attack_power_sm)) {
				iAP_SM = m_game->m_client_list[attacker_h]->m_max_attack_power_sm;
			}
			if ((m_game->m_client_list[attacker_h]->m_max_attack_power_l != 0) && (iAP_L > m_game->m_client_list[attacker_h]->m_max_attack_power_l)) {
				iAP_L = m_game->m_client_list[attacker_h]->m_max_attack_power_l;
			}
		}

		if (m_game->m_client_list[attacker_h]->m_hero_armour_bonus == 1) {
			attacker_hit_ratio += 100;
			iAP_SM += 5;
			iAP_L += 5;
		}

		item_index = m_game->m_client_list[attacker_h]->m_item_equipment_status[to_int(EquipPos::RightHand)];
		if ((item_index != -1) && (m_game->m_client_list[attacker_h]->m_item_list[item_index] != 0)) {
			if ((m_game->m_client_list[attacker_h]->m_item_list[item_index]->m_id_num == 851) || // KlonessEsterk 
				(m_game->m_client_list[attacker_h]->m_item_list[item_index]->m_id_num == 863) || // KlonessWand(MS.20)
				(m_game->m_client_list[attacker_h]->m_item_list[item_index]->m_id_num == 864)) { // KlonessWand(MS.10) 
				if (m_game->m_client_list[attacker_h]->m_rating > 0) {
					rep_damage = m_game->m_client_list[attacker_h]->m_rating / 100;
					if (rep_damage < 5) rep_damage = 5;
					if (rep_damage > 15) rep_damage = 15;
					iAP_SM += rep_damage;
					iAP_L += rep_damage;
				}
				if (target_type == hb::shared::owner_class::Player) {
					if (m_game->m_client_list[target_h] == 0) return 0;
					if (m_game->m_client_list[target_h]->m_rating < 0) {
						rep_damage = (abs(m_game->m_client_list[target_h]->m_rating) / 10);
						if (rep_damage > 10) rep_damage = 10;
						iAP_SM += rep_damage;
						iAP_L += rep_damage;
					}
				}
			}
			if ((m_game->m_client_list[attacker_h]->m_item_list[item_index]->m_id_num == 732) || // BerserkWand(MS.20)
				(m_game->m_client_list[attacker_h]->m_item_list[item_index]->m_id_num == 738)) { // BerserkWand(MS.10)
				iAP_SM += 1;
				iAP_L += 1;
			}
		}

		item_index = m_game->m_client_list[attacker_h]->m_item_equipment_status[to_int(EquipPos::TwoHand)];
		if ((item_index != -1) && (m_game->m_client_list[attacker_h]->m_item_list[item_index] != 0)) {
			if ((m_game->m_client_list[attacker_h]->m_item_list[item_index]->m_id_num == 847) &&
				(m_game->m_day_or_night == 2)) {
				iAP_SM += 4;
				iAP_L += 4;
			}
			if ((m_game->m_client_list[attacker_h]->m_item_list[item_index]->m_id_num == 848) &&
				(m_game->m_day_or_night == 1)) {
				iAP_SM += 4;
				iAP_L += 4;
			}
			if ((m_game->m_client_list[attacker_h]->m_item_list[item_index]->m_id_num == 849) || // KlonessBlade 
				(m_game->m_client_list[attacker_h]->m_item_list[item_index]->m_id_num == 850)) { // KlonessAxe
				if (m_game->m_client_list[attacker_h]->m_rating > 0) {
					rep_damage = m_game->m_client_list[attacker_h]->m_rating / 100;
					if (rep_damage < 5) rep_damage = 5;
					if (rep_damage > 15) rep_damage = 15;
					iAP_SM += rep_damage;
					iAP_L += rep_damage;
				}
				if (target_type == hb::shared::owner_class::Player) {
					if (m_game->m_client_list[target_h] == 0) return 0;
					if (m_game->m_client_list[target_h]->m_rating < 0) {
						rep_damage = (abs(m_game->m_client_list[target_h]->m_rating) / 10);
						if (rep_damage > 10) rep_damage = 10;
						iAP_SM += rep_damage;
						iAP_L += rep_damage;
					}
				}
			}
		}

		item_index = m_game->m_client_list[attacker_h]->m_item_equipment_status[to_int(EquipPos::Neck)];
		if ((item_index != -1) && (m_game->m_client_list[attacker_h]->m_item_list[item_index] != 0)) {
			if (m_game->m_client_list[attacker_h]->m_item_list[item_index]->m_id_num == 859) { // NecklaceOfKloness  
				if (target_type == hb::shared::owner_class::Player) {
					if (m_game->m_client_list[target_h] == 0) return 0;
					rep_damage = (abs(m_game->m_client_list[target_h]->m_rating) / 20);
					if (rep_damage > 5) rep_damage = 5;
					iAP_SM += rep_damage;
					iAP_L += rep_damage;
				}
			}
		}

		attacker_dir = m_game->m_client_list[attacker_h]->m_dir;
		strcpy(attacker_name, m_game->m_client_list[attacker_h]->m_char_name);

		if (m_game->m_client_list[attacker_h]->m_magic_effect_status[hb::shared::magic::Berserk] != 0)
			is_attacker_berserk = true;
		else is_attacker_berserk = false;

		if ((arrow_use != true) && (m_game->m_client_list[attacker_h]->m_super_attack_left > 0) && (attack_mode >= 20)) {

			tmp1 = (double)iAP_SM;
			tmp2 = (double)m_game->m_client_list[attacker_h]->m_level;
			tmp3 = tmp2 / 100.0f;
			tmp2 = tmp1 * tmp3;
			temp = (int)(tmp2 + 0.5f);
			iAP_SM += temp;

			tmp1 = (double)iAP_L;
			tmp2 = (double)m_game->m_client_list[attacker_h]->m_level;
			tmp3 = tmp2 / 100.0f;
			tmp2 = tmp1 * tmp3;
			temp = (int)(tmp2 + 0.5f);
			iAP_L += temp;

			switch (m_game->m_client_list[attacker_h]->m_using_weapon_skill) {
			case 5:  iAP_SM += (iAP_SM / 10); iAP_L += (iAP_L / 10); attacker_hit_ratio += 20; break; // Boxing
			case 6:  iAP_SM += (iAP_SM / 10); iAP_L += (iAP_L / 10); attacker_hit_ratio += 30; break; // Bow
			case 7:  iAP_SM += (iAP_SM / 10); iAP_L += (iAP_L / 10); attacker_hit_ratio += 40; break; // Dagger/SS
			case 8:  iAP_SM += (iAP_SM / 10); iAP_L += (iAP_L / 10); attacker_hit_ratio += 30; break; // Long Sword
			case 9:  iAP_SM += (iAP_SM / 7);  iAP_L += (iAP_L / 7);  attacker_hit_ratio += 30; break; // Fencing
			case 10: iAP_SM += (iAP_SM / 5);  iAP_L += (iAP_L / 5);                           break; // Axe
			case 14: iAP_SM += (iAP_SM / 5);  iAP_L += (iAP_L / 5);  attacker_hit_ratio += 20; break; // Hammer
			case 21: iAP_SM += (iAP_SM / 5);  iAP_L += (iAP_L / 5);  attacker_hit_ratio += 50; break; // Wand
			default: break;
			}
			attacker_hit_ratio += 100;
			attacker_hit_ratio += m_game->m_client_list[attacker_h]->m_custom_item_value_attack;
		}

		if (is_dash) {

			attacker_hit_ratio += 20;

			switch (m_game->m_client_list[attacker_h]->m_using_weapon_skill) {
			case 8:  iAP_SM += (iAP_SM / 10); iAP_L += (iAP_L / 10); break;
			case 10: iAP_SM += (iAP_SM / 5); iAP_L += (iAP_L / 5); break;
			case 14: iAP_SM += (iAP_SM / 5); iAP_L += (iAP_L / 5); break;
			default: break;
			}
		}

		attacker_hp = m_game->m_client_list[attacker_h]->m_hp;
		attacker_hit_ratio += m_game->m_client_list[attacker_h]->m_add_attack_ratio;

		atk_x = m_game->m_client_list[attacker_h]->m_x;
		atk_y = m_game->m_client_list[attacker_h]->m_y;
		party_id = m_game->m_client_list[attacker_h]->m_party_id;
		break;

	case hb::shared::owner_class::Npc:

		if (m_game->m_npc_list[attacker_h] == 0) return 0;
		if (m_game->m_map_list[m_game->m_npc_list[attacker_h]->m_map_index]->m_is_attack_enabled == false) return 0;

		if (m_game->m_npc_list[attacker_h]->m_status.invisibility) {
			m_game->m_status_effect_manager->set_invisibility_flag(attacker_h, hb::shared::owner_class::Npc, false);
			m_game->m_delay_event_manager->remove_from_delay_event_list(attacker_h, hb::shared::owner_class::Npc, hb::shared::magic::Invisibility);
			m_game->m_npc_list[attacker_h]->m_magic_effect_status[hb::shared::magic::Invisibility] = 0;
		}

		attacker_side = m_game->m_npc_list[attacker_h]->m_side;
		iAP_SM = 0;
		iAP_L = 0;

		int npc_min = m_game->m_npc_list[attacker_h]->m_min_damage;
		int npc_max = m_game->m_npc_list[attacker_h]->m_max_damage;
		if (npc_max > 0)
		{
			iAP_L = iAP_SM = npc_min + (rand() % (npc_max - npc_min + 1));
		}

		attacker_hit_ratio = m_game->m_npc_list[attacker_h]->m_hit_ratio;

		attacker_dir = m_game->m_npc_list[attacker_h]->m_dir;
		memcpy(attacker_name, m_game->m_npc_list[attacker_h]->m_npc_name, hb::shared::limits::NpcNameLen - 1);

		if (m_game->m_npc_list[attacker_h]->m_magic_effect_status[hb::shared::magic::Berserk] != 0)
			is_attacker_berserk = true;
		else is_attacker_berserk = false;

		attacker_hp = m_game->m_npc_list[attacker_h]->m_hp;
		attacker_sa = m_game->m_npc_list[attacker_h]->m_special_ability;

		atk_x = m_game->m_npc_list[attacker_h]->m_x;
		atk_y = m_game->m_npc_list[attacker_h]->m_y;
		break;
	}

	switch (target_type) {
	case hb::shared::owner_class::Player:

		if (m_game->m_client_list[target_h] == 0) return 0;
		if (m_game->m_client_list[target_h]->m_is_killed) return 0;

		// GM mode damage immunity
		if (m_game->m_client_list[target_h]->m_is_gm_mode)
		{
			uint32_t now = GameClock::GetTimeMS();
			if (now - m_game->m_client_list[target_h]->m_last_gm_immune_notify_time > 2000)
			{
				m_game->m_client_list[target_h]->m_last_gm_immune_notify_time = now;
				m_game->send_event_to_near_client_type_a(target_h, hb::shared::owner_class::Player, MsgId::EventMotion, Type::Damage, Sentinel::DamageImmune, 0, 0);
			}
			return 0;
		}

		if (m_game->m_client_list[target_h]->m_status.slate_invincible) return 0;

		if ((attacker_type == hb::shared::owner_class::Player) && (m_game->m_is_crusade_mode == false) &&
			(m_game->m_client_list[target_h]->m_player_kill_count == 0) && (m_game->m_client_list[target_h]->m_is_player_civil)) return 0;

		if ((attacker_type == hb::shared::owner_class::Player) && (m_game->m_client_list[target_h]->m_is_neutral) &&
			(m_game->m_client_list[target_h]->m_player_kill_count == 0) && (m_game->m_client_list[target_h]->m_is_own_location)) return 0;

		if ((m_game->m_client_list[target_h]->m_x != tdX) || (m_game->m_client_list[target_h]->m_y != tdY)) return 0;

		if ((attacker_type == hb::shared::owner_class::Player) && (m_game->m_client_list[attacker_h]->m_is_neutral)
			&& (m_game->m_client_list[target_h]->m_player_kill_count == 0)) return 0;

		if ((m_game->m_client_list[target_h]->m_party_id != 0) && (party_id == m_game->m_client_list[target_h]->m_party_id)) return 0;

		target_dir = m_game->m_client_list[target_h]->m_dir;
		target_defense_ratio = m_game->m_client_list[target_h]->m_defense_ratio;
		m_game->m_client_list[target_h]->m_logout_hack_check = time;
		if ((attacker_type == hb::shared::owner_class::Player) && (m_game->m_client_list[attacker_h]->m_is_safe_attack_mode)) {
			if (attacker_h == target_h) return 0;
			side_condition = get_player_relationship_raw(attacker_h, target_h);
			if ((side_condition == 7) || (side_condition == 2) || (side_condition == 6)) {
				iAP_SM = iAP_SM / 2;
				iAP_L = iAP_L / 2;
			}
			else {
				if (m_game->m_map_list[m_game->m_client_list[attacker_h]->m_map_index]->m_is_fight_zone) {
					iAP_SM = iAP_SM / 2;
					iAP_L = iAP_L / 2;
				}
				else return 0;
			}
		}

		target_defense_ratio += m_game->m_client_list[target_h]->m_add_defense_ratio;

		tgt_x = m_game->m_client_list[target_h]->m_x;
		tgt_y = m_game->m_client_list[target_h]->m_y;
		break;

	case hb::shared::owner_class::Npc:

		if (m_game->m_npc_list[target_h] == 0) return 0;
		if (m_game->m_npc_list[target_h]->m_hp <= 0) return 0;

		if ((m_game->m_npc_list[target_h]->m_x != tdX) || (m_game->m_npc_list[target_h]->m_y != tdY)) return 0;

		target_dir = m_game->m_npc_list[target_h]->m_dir;
		target_defense_ratio = m_game->m_npc_list[target_h]->m_defense_ratio;

		if (attacker_type == hb::shared::owner_class::Player) {
			switch (m_game->m_npc_list[target_h]->m_type) {
			case 40:
			case 41:
				if ((m_game->m_client_list[attacker_h]->m_side == 0) || (m_game->m_npc_list[target_h]->m_side == m_game->m_client_list[attacker_h]->m_side)) return 0;
				break;
			}

			if ((wc == weapon_class::axe) && (m_game->m_npc_list[target_h]->m_action_limit == 5) && (m_game->m_npc_list[target_h]->m_build_count > 0)) {
				if (m_game->m_client_list[attacker_h]->m_crusade_duty != 2) break;

				switch (m_game->m_npc_list[target_h]->m_type) {
				case 36:
				case 37:
				case 38:
				case 39:
					switch (m_game->m_npc_list[target_h]->m_build_count) {
					case 1:
						m_game->m_npc_list[target_h]->m_appearance.special_frame = 0;
						m_game->send_event_to_near_client_type_a(target_h, hb::shared::owner_class::Npc, MsgId::EventMotion, Type::NullAction, 0, 0, 0);
						switch (m_game->m_npc_list[target_h]->m_type) {
						case 36: construction_point = 700; war_contribution = 700; break;
						case 37: construction_point = 700; war_contribution = 700; break;
						case 38: construction_point = 500; war_contribution = 500; break;
						case 39: construction_point = 500; war_contribution = 500; break;
						}

						m_game->m_client_list[attacker_h]->m_war_contribution += war_contribution;
						if (m_game->m_client_list[attacker_h]->m_war_contribution > m_game->m_max_war_contribution)
							m_game->m_client_list[attacker_h]->m_war_contribution = m_game->m_max_war_contribution;
						hb::logger::log("Construction complete, war contribution +{}", war_contribution);
						m_game->send_notify_msg(0, attacker_h, Notify::ConstructionPoint, m_game->m_client_list[attacker_h]->m_construction_point, m_game->m_client_list[attacker_h]->m_war_contribution, 0, 0);
						break;
					case 5:
						m_game->m_npc_list[target_h]->m_appearance.special_frame = 1;
						m_game->send_event_to_near_client_type_a(target_h, hb::shared::owner_class::Npc, MsgId::EventMotion, Type::NullAction, 0, 0, 0);
						break;
					case 10:
						m_game->m_npc_list[target_h]->m_appearance.special_frame = 2;
						m_game->send_event_to_near_client_type_a(target_h, hb::shared::owner_class::Npc, MsgId::EventMotion, Type::NullAction, 0, 0, 0);
						break;
					}
					break;
				}

				m_game->m_npc_list[target_h]->m_build_count--;
				if (m_game->m_npc_list[target_h]->m_build_count <= 0) {
					m_game->m_npc_list[target_h]->m_build_count = 0;
				}
				return 0;
			}
			if ((wc == weapon_class::axe) && (m_game->m_npc_list[target_h]->m_crop_type != 0) && (m_game->m_npc_list[target_h]->m_action_limit == 5) && (m_game->m_npc_list[target_h]->m_build_count > 0)) {
				farming_skill = m_game->m_client_list[attacker_h]->m_skill_mastery[2];
				crop_skill = m_game->m_npc_list[target_h]->m_crop_skill;
				if (farming_skill < 20) return 0;
				if (m_game->m_client_list[attacker_h]->m_level < 20) return 0;
				switch (m_game->m_npc_list[target_h]->m_type) {
				case 64:
					switch (m_game->m_npc_list[target_h]->m_build_count) {
					case 1:
						m_game->m_npc_list[target_h]->m_appearance.special_frame = 3;
						m_game->send_event_to_near_client_type_a(target_h, hb::shared::owner_class::Npc, MsgId::EventMotion, Type::NullAction, 0, 0, 0);
						//sub_4B67E0
						m_game->m_skill_manager->calculate_ssn_skill_index(attacker_h, 2, farming_skill <= crop_skill + 10);
						m_game->m_status_effect_manager->check_farming_action(attacker_h, target_h, 1);
						// Use EntityManager for NPC deletion
						if (m_game->m_entity_manager != NULL)
							m_game->m_entity_manager->delete_entity(target_h);
						return 0;
					case 8:
						m_game->m_npc_list[target_h]->m_appearance.special_frame = 3;
						m_game->send_event_to_near_client_type_a(target_h, hb::shared::owner_class::Npc, MsgId::EventMotion, Type::NullAction, 0, 0, 0);
						m_game->m_skill_manager->calculate_ssn_skill_index(attacker_h, 2, farming_skill <= crop_skill + 10);
						m_game->m_status_effect_manager->check_farming_action(attacker_h, target_h, 0);
						break;
					case 18:
						m_game->m_npc_list[target_h]->m_appearance.special_frame = 2;
						m_game->send_event_to_near_client_type_a(target_h, hb::shared::owner_class::Npc, MsgId::EventMotion, Type::NullAction, 0, 0, 0);
						m_game->m_skill_manager->calculate_ssn_skill_index(attacker_h, 2, farming_skill <= crop_skill + 10);
						m_game->m_status_effect_manager->check_farming_action(attacker_h, target_h, 0);
						break;

					}
					break;
				}
				m_game->m_npc_list[target_h]->m_build_count--;
				if (m_game->m_npc_list[target_h]->m_build_count <= 0) {
					m_game->m_npc_list[target_h]->m_build_count = 0;
				}
				return 0;
			}
		}

		tgt_x = m_game->m_npc_list[target_h]->m_x;
		tgt_y = m_game->m_npc_list[target_h]->m_y;
		break;
	}

	if ((attacker_type == hb::shared::owner_class::Player) && (target_type == hb::shared::owner_class::Player)) {

		sX = m_game->m_client_list[attacker_h]->m_x;
		sY = m_game->m_client_list[attacker_h]->m_y;

		dX = m_game->m_client_list[target_h]->m_x;
		dY = m_game->m_client_list[target_h]->m_y;

		if (m_game->m_map_list[m_game->m_client_list[target_h]->m_map_index]->get_attribute(sX, sY, 0x00000006) != 0) return 0;
		if (m_game->m_map_list[m_game->m_client_list[target_h]->m_map_index]->get_attribute(dX, dY, 0x00000006) != 0) return 0;
	}

	if (attacker_type == hb::shared::owner_class::Player) {
		if ((m_game->m_client_list[attacker_h]->m_dex + m_game->m_client_list[attacker_h]->m_angelic_dex) > 50) {
			attacker_hit_ratio += ((m_game->m_client_list[attacker_h]->m_dex + m_game->m_client_list[attacker_h]->m_angelic_dex) - 50);
		}
	}

	if (wc == weapon_class::bow) {
		switch (m_game->m_map_list[m_game->m_client_list[attacker_h]->m_map_index]->m_weather_status) {
		case 0:	break;
		case 1:	attacker_hit_ratio -= (attacker_hit_ratio / 20); break;
		case 2:	attacker_hit_ratio -= (attacker_hit_ratio / 10); break;
		case 3:	attacker_hit_ratio -= (attacker_hit_ratio / 4);  break;
		}
	}

	if (attacker_hit_ratio < 0)   attacker_hit_ratio = 0;
	switch (target_type) {
	case hb::shared::owner_class::Player:
		protect = m_game->m_client_list[target_h]->m_magic_effect_status[hb::shared::magic::Protect];
		break;

	case hb::shared::owner_class::Npc:
		protect = m_game->m_npc_list[target_h]->m_magic_effect_status[hb::shared::magic::Protect];
		break;
	}

	if (attacker_type == hb::shared::owner_class::Player) {
		if (m_game->m_client_list[attacker_h]->m_item_equipment_status[to_int(EquipPos::TwoHand)] != -1) {
			if (m_game->m_client_list[attacker_h]->m_item_list[m_game->m_client_list[attacker_h]->m_item_equipment_status[to_int(EquipPos::TwoHand)]] == 0) {
				m_game->m_client_list[attacker_h]->m_is_item_equipped[m_game->m_client_list[attacker_h]->m_item_equipment_status[to_int(EquipPos::TwoHand)]] = false;
				m_game->delete_client(attacker_h, true, true);
				return 0;
			}

			if (m_game->m_client_list[attacker_h]->m_item_list[m_game->m_client_list[attacker_h]->m_item_equipment_status[to_int(EquipPos::TwoHand)]]->get_item_effect_type() == ItemEffectType::AttackArrow) {
				if (m_game->m_client_list[attacker_h]->m_arrow_index == -1) {
					return 0;
				}
				else {
					if (m_game->m_client_list[attacker_h]->m_item_list[m_game->m_client_list[attacker_h]->m_arrow_index] == 0)
						return 0;

					int arrow_id_num = m_game->m_client_list[attacker_h]->m_item_list[m_game->m_client_list[attacker_h]->m_arrow_index]->m_id_num;

					if (arrow_use != true)
						m_game->m_client_list[attacker_h]->m_item_list[m_game->m_client_list[attacker_h]->m_arrow_index]->m_instance.count--;
					if (m_game->m_client_list[attacker_h]->m_item_list[m_game->m_client_list[attacker_h]->m_arrow_index]->m_instance.count <= 0) {

						m_game->m_item_manager->item_deplete_handler(attacker_h, m_game->m_client_list[attacker_h]->m_arrow_index, false);
						m_game->m_client_list[attacker_h]->m_arrow_index = m_game->m_item_manager->get_arrow_item_index(attacker_h);
					}
					else {
						m_game->send_notify_msg(0, attacker_h, Notify::set_item_count, m_game->m_client_list[attacker_h]->m_arrow_index, m_game->m_client_list[attacker_h]->m_item_list[m_game->m_client_list[attacker_h]->m_arrow_index]->m_instance.count, false, 0);
						m_game->calc_total_weight(attacker_h);
					}

					// Poison arrows apply poison (same as weapon poison prefix, sa=61)
					if (arrow_id_num == hb::shared::item::ItemId::PoisonArrow)
					{
						attacker_sa = 61;
						attacker_s_avalue = 20;
					}
				}
				if (protect == 1) return 0;
			}
			else {
				switch (protect) {
				case 3: target_defense_ratio += 40;  break;
				case 4: target_defense_ratio += 100; break;
				}
				if (target_defense_ratio < 0) target_defense_ratio = 1;
			}
		}
	}
	else {
		switch (protect) {
		case 1:
			switch (m_game->m_npc_list[attacker_h]->m_type) {
			case 54:
				if ((abs(tgt_x - m_game->m_npc_list[attacker_h]->m_x) >= 1) || (abs(tgt_y - m_game->m_npc_list[attacker_h]->m_y) >= 1)) return 0;
			}
			break;
		case 3: target_defense_ratio += 40;  break;
		case 4: target_defense_ratio += 100; break;
		}
		if (target_defense_ratio < 0) target_defense_ratio = 1;
	}

	if (attacker_dir == target_dir) target_defense_ratio = target_defense_ratio / 2;
	if (target_defense_ratio < 1)   target_defense_ratio = 1;

	tmp1 = (double)(attacker_hit_ratio);
	tmp2 = (double)(target_defense_ratio);
	tmp3 = (tmp1 / tmp2) * 50.0f;
	dest_hit_ratio = (int)(tmp3);

	if (dest_hit_ratio < m_game->m_minimum_hit_ratio) dest_hit_ratio = m_game->m_minimum_hit_ratio;
	if (dest_hit_ratio > m_game->m_maximum_hit_ratio) dest_hit_ratio = m_game->m_maximum_hit_ratio;

	if ((is_attacker_berserk) && (attack_mode < 20)) {
		iAP_SM = iAP_SM * 2;
		iAP_L = iAP_L * 2;
	}

	if (attacker_type == hb::shared::owner_class::Player) {
		iAP_SM += m_game->m_client_list[attacker_h]->m_add_physical_damage;
		iAP_L += m_game->m_client_list[attacker_h]->m_add_physical_damage;
	}

	if (near_attack) {
		iAP_SM = iAP_SM / 2;
		iAP_L = iAP_L / 2;
	}

	if (target_type == hb::shared::owner_class::Player) {
		iAP_SM -= (m_game->dice(1, m_game->m_client_list[target_h]->m_vit / 10) - 1);
		iAP_L -= (m_game->dice(1, m_game->m_client_list[target_h]->m_vit / 10) - 1);
	}

	if (attacker_type == hb::shared::owner_class::Player) {
		if (iAP_SM <= 1) iAP_SM = 1;
		if (iAP_L <= 1) iAP_L = 1;
	}
	else {
		if (iAP_SM <= 0) iAP_SM = 0;
		if (iAP_L <= 0) iAP_L = 0;
	}

	result = m_game->dice(1, 100);

	if (result <= dest_hit_ratio) {
		if (attacker_type == hb::shared::owner_class::Player) {

			if (((m_game->m_client_list[attacker_h]->m_hunger_status <= 10) || (m_game->m_client_list[attacker_h]->m_sp <= 0)) && (m_game->dice(1, 10) == 5)) return false;
			m_game->m_client_list[attacker_h]->m_combo_attack_count++;
			if (m_game->m_client_list[attacker_h]->m_combo_attack_count < 0) m_game->m_client_list[attacker_h]->m_combo_attack_count = 0;
			if (m_game->m_client_list[attacker_h]->m_combo_attack_count > 4) m_game->m_client_list[attacker_h]->m_combo_attack_count = 1;
			weapon_skill = get_weapon_skill_type(attacker_h);
			combo_bonus = get_combo_attack_bonus(weapon_skill, m_game->m_client_list[attacker_h]->m_combo_attack_count);

			if ((m_game->m_client_list[attacker_h]->m_combo_attack_count > 1) && (m_game->m_client_list[attacker_h]->m_add_combo_damage != 0))
				combo_bonus += m_game->m_client_list[attacker_h]->m_add_combo_damage;

			iAP_SM += combo_bonus;
			iAP_L += combo_bonus;

			switch (m_game->m_client_list[attacker_h]->m_special_weapon_effect_type) {
			case 0: break;
			case 1:
				if ((m_game->m_client_list[attacker_h]->m_super_attack_left > 0) && (attack_mode >= 20)) {
					iAP_SM += m_game->m_client_list[attacker_h]->m_special_weapon_effect_value * m_game->m_prefix_multiplier[1];
					iAP_L += m_game->m_client_list[attacker_h]->m_special_weapon_effect_value * m_game->m_prefix_multiplier[1];
				}
				break;

			case 2:
				attacker_sa = 61;
				attacker_s_avalue = m_game->m_client_list[attacker_h]->m_special_weapon_effect_value * m_game->m_prefix_multiplier[2];
				break;

			case 3:
				attacker_sa = 62;
				break;
			}

			if (m_game->m_map_list[m_game->m_client_list[attacker_h]->m_map_index]->m_is_fight_zone) {
				iAP_SM += iAP_SM / 3;
				iAP_L += iAP_L / 3;
			}

			if (m_game->m_war_manager->check_heldenian_map(attacker_h, m_game->m_bt_field_map_index, hb::shared::owner_class::Player) == 1) {
				iAP_SM += iAP_SM / 3;
				iAP_L += iAP_L / 3;
			}

			if ((target_type == hb::shared::owner_class::Player) && (m_game->m_is_crusade_mode) && (m_game->m_client_list[attacker_h]->m_crusade_duty == 1)) {
				if (m_game->m_client_list[attacker_h]->m_level <= 80) {
					iAP_SM += iAP_SM;
					iAP_L += iAP_L;
				}
				else if (m_game->m_client_list[attacker_h]->m_level <= 100) {
					iAP_SM += (iAP_SM * 7) / 10;
					iAP_L += (iAP_L * 7) / 10;
				}
				else {
					iAP_SM += iAP_SM / 3;
					iAP_L += iAP_L / 3;
				}
			}
		}

		switch (target_type) {
		case hb::shared::owner_class::Player:
			m_game->m_skill_manager->clear_skill_using_status(target_h);
			if ((time - m_game->m_client_list[target_h]->m_time) > (uint32_t)m_game->m_lag_protection_interval) {
				return 0;
			}
			else {
				switch (attacker_sa) {
				case 62:
					if (m_game->m_client_list[target_h]->m_rating < 0) {
						temp = abs(m_game->m_client_list[target_h]->m_rating) / 10;
						if (temp > 10) temp = 10;
						iAP_SM += temp;
					}
					break;
				}

				iAP_Abs_Armor = 0;
				iAP_Abs_Shield = 0;
				temp = m_game->dice(1, 10000);
				if ((temp >= 1) && (temp < 5000))           hit_point = 1;
				else if ((temp >= 5000) && (temp < 7500))   hit_point = 2;
				else if ((temp >= 7500) && (temp < 9000))   hit_point = 3;
				else if ((temp >= 9000) && (temp <= 10000)) hit_point = 4;

				switch (hit_point) {
				case 1:
					if (m_game->m_client_list[target_h]->m_damage_absorption_armor[to_int(EquipPos::Body)] > 0) {
						if (m_game->m_client_list[target_h]->m_damage_absorption_armor[to_int(EquipPos::Body)] >= 80)
							tmp1 = 80.0f;
						else tmp1 = (double)m_game->m_client_list[target_h]->m_damage_absorption_armor[to_int(EquipPos::Body)];
						tmp2 = (double)iAP_SM;
						tmp3 = (tmp1 / 100.0f) * tmp2;
						iAP_Abs_Armor = (int)tmp3;
					}
					break;
				case 2:
					if ((m_game->m_client_list[target_h]->m_damage_absorption_armor[to_int(EquipPos::Leggings)] +
						m_game->m_client_list[target_h]->m_damage_absorption_armor[to_int(EquipPos::Boots)]) > 0) {
						if ((m_game->m_client_list[target_h]->m_damage_absorption_armor[to_int(EquipPos::Leggings)] +
							m_game->m_client_list[target_h]->m_damage_absorption_armor[to_int(EquipPos::Boots)]) >= 80)
							tmp1 = 80.0f;
						else tmp1 = (double)(m_game->m_client_list[target_h]->m_damage_absorption_armor[to_int(EquipPos::Leggings)] + m_game->m_client_list[target_h]->m_damage_absorption_armor[to_int(EquipPos::Boots)]);
						tmp2 = (double)iAP_SM;
						tmp3 = (tmp1 / 100.0f) * tmp2;

						iAP_Abs_Armor = (int)tmp3;
					}
					break;

				case 3:
					if (m_game->m_client_list[target_h]->m_damage_absorption_armor[to_int(EquipPos::Arms)] > 0) {
						if (m_game->m_client_list[target_h]->m_damage_absorption_armor[to_int(EquipPos::Arms)] >= 80)
							tmp1 = 80.0f;
						else tmp1 = (double)m_game->m_client_list[target_h]->m_damage_absorption_armor[to_int(EquipPos::Arms)];
						tmp2 = (double)iAP_SM;
						tmp3 = (tmp1 / 100.0f) * tmp2;

						iAP_Abs_Armor = (int)tmp3;
					}
					break;

				case 4:
					if (m_game->m_client_list[target_h]->m_damage_absorption_armor[to_int(EquipPos::Head)] > 0) {
						if (m_game->m_client_list[target_h]->m_damage_absorption_armor[to_int(EquipPos::Head)] >= 80)
							tmp1 = 80.0f;
						else tmp1 = (double)m_game->m_client_list[target_h]->m_damage_absorption_armor[to_int(EquipPos::Head)];
						tmp2 = (double)iAP_SM;
						tmp3 = (tmp1 / 100.0f) * tmp2;

						iAP_Abs_Armor = (int)tmp3;
					}
					break;
				}

				if (m_game->m_client_list[target_h]->m_damage_absorption_shield > 0) {
					if (m_game->dice(1, 100) <= (m_game->m_client_list[target_h]->m_skill_mastery[11])) {
						m_game->m_skill_manager->calculate_ssn_skill_index(target_h, 11, 1);
						if (m_game->m_client_list[target_h]->m_damage_absorption_shield >= 80)
							tmp1 = 80.0f;
						else tmp1 = (double)m_game->m_client_list[target_h]->m_damage_absorption_shield;
						tmp2 = (double)iAP_SM;
						tmp3 = (tmp1 / 100.0f) * tmp2;

						iAP_Abs_Shield = (int)tmp3;

						temp = m_game->m_client_list[target_h]->m_item_equipment_status[to_int(EquipPos::LeftHand)];
						if ((temp != -1) && (m_game->m_client_list[target_h]->m_item_list[temp] != 0)) {
							if (m_game->m_client_list[target_h]->m_item_list[temp]->m_instance.cur_durability > 0) {
								m_game->m_client_list[target_h]->m_item_list[temp]->m_instance.cur_durability--;
								m_game->send_notify_msg(0, target_h, Notify::CurDurability, temp, m_game->m_client_list[target_h]->m_item_list[temp]->m_instance.cur_durability, 0, 0);
							}
							if (m_game->m_client_list[target_h]->m_item_list[temp]->m_instance.cur_durability == 0) {
								m_game->send_notify_msg(0, target_h, Notify::ItemDurabilityEnd, m_game->m_client_list[target_h]->m_item_list[temp]->m_equip_pos, temp, 0, 0);
								m_game->m_item_manager->release_item_handler(target_h, temp, true);
							}
						}
					}
				}

				iAP_SM = iAP_SM - (iAP_Abs_Armor + iAP_Abs_Shield);
				if (iAP_SM < 0) iAP_SM = 0;

				if ((attacker_type == hb::shared::owner_class::Player) && (m_game->m_client_list[attacker_h] != 0) && (m_game->m_client_list[attacker_h]->m_is_special_ability_enabled)) {
					switch (m_game->m_client_list[attacker_h]->m_special_ability_type) {
					case 0: break;
					case 1:
						temp = (m_game->m_client_list[target_h]->m_hp / 2);
						if (temp > iAP_SM) iAP_SM = temp;
						if (iAP_SM <= 0) iAP_SM = 1;
						break;
					case 2:
						if (m_game->m_client_list[target_h]->m_magic_effect_status[hb::shared::magic::Ice] == 0) {
							m_game->m_client_list[target_h]->m_magic_effect_status[hb::shared::magic::Ice] = 1;
							m_game->m_status_effect_manager->set_ice_flag(target_h, hb::shared::owner_class::Player, true);
							m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Ice, time + 30000,
								target_h, hb::shared::owner_class::Player, 0, 0, 0, 1, 0, 0);
							m_game->send_notify_msg(0, target_h, Notify::MagicEffectOn, hb::shared::magic::Ice, 1, 0, 0);
						}
						break;
					case 3:
						if (m_game->m_client_list[target_h]->m_magic_effect_status[hb::shared::magic::HoldObject] == 0) {
							m_game->m_client_list[target_h]->m_magic_effect_status[hb::shared::magic::HoldObject] = 2;
							m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::HoldObject, time + 10000,
								target_h, hb::shared::owner_class::Player, 0, 0, 0, 10, 0, 0);
							m_game->send_notify_msg(0, target_h, Notify::MagicEffectOn, hb::shared::magic::HoldObject, 10, 0, 0);
						}
						break;
					case 4:
						iAP_SM = (m_game->m_client_list[target_h]->m_hp);
						break;
					case 5:
						m_game->m_client_list[attacker_h]->m_hp += iAP_SM;
						if (m_game->get_max_hp(attacker_h) < m_game->m_client_list[attacker_h]->m_hp) m_game->m_client_list[attacker_h]->m_hp = m_game->get_max_hp(attacker_h);
						m_game->send_notify_msg(0, attacker_h, Notify::Hp, 0, 0, 0, 0);
						break;
					}
				}

				if ((attacker_type == hb::shared::owner_class::Player) && (m_game->m_client_list[attacker_h] != 0) && (m_game->m_client_list[target_h]->m_is_special_ability_enabled)) {
					switch (m_game->m_client_list[target_h]->m_special_ability_type) {
					case 50:
						if (m_game->m_client_list[attacker_h]->m_item_equipment_status[to_int(EquipPos::TwoHand)] != -1)
							weapon_index = m_game->m_client_list[attacker_h]->m_item_equipment_status[to_int(EquipPos::TwoHand)];
						else weapon_index = m_game->m_client_list[attacker_h]->m_item_equipment_status[to_int(EquipPos::RightHand)];
						if (weapon_index != -1)	m_game->m_client_list[attacker_h]->m_item_list[weapon_index]->m_instance.cur_durability = 0;
						break;
					case 51:
						if (hit_point == m_game->m_client_list[target_h]->m_special_ability_equip_pos)
							iAP_SM = 0;
						break;
					case 52:
						iAP_SM = 0;
						break;
					}
				}

				if ((m_game->m_client_list[target_h]->m_is_lucky_effect) &&
					(m_game->dice(1, 10) == 5) && (m_game->m_client_list[target_h]->m_hp <= iAP_SM)) {
					iAP_SM = m_game->m_client_list[target_h]->m_hp - 1;
				}

				switch (hit_point) {
				case 1:
					temp = m_game->m_client_list[target_h]->m_item_equipment_status[to_int(EquipPos::Body)];
					if ((temp != -1) && (m_game->m_client_list[target_h]->m_item_list[temp] != 0)) {
						calculate_durability_decrement(target_h, attacker_h, attacker_type, target_type, temp);
					}
					break;

				case 2:
					temp = m_game->m_client_list[target_h]->m_item_equipment_status[to_int(EquipPos::Leggings)];
					if ((temp != -1) && (m_game->m_client_list[target_h]->m_item_list[temp] != 0)) {
						calculate_durability_decrement(target_h, attacker_h, attacker_type, target_type, temp);
					}
					else {
						temp = m_game->m_client_list[target_h]->m_item_equipment_status[to_int(EquipPos::Boots)];
						if ((temp != -1) && (m_game->m_client_list[target_h]->m_item_list[temp] != 0)) {
							calculate_durability_decrement(target_h, attacker_h, attacker_type, target_type, temp);
						}
					}
					break;

				case 3:
					temp = m_game->m_client_list[target_h]->m_item_equipment_status[to_int(EquipPos::Arms)];
					if ((temp != -1) && (m_game->m_client_list[target_h]->m_item_list[temp] != 0)) {
						calculate_durability_decrement(target_h, attacker_h, attacker_type, target_type, temp);
					}
					break;

				case 4:
					temp = m_game->m_client_list[target_h]->m_item_equipment_status[to_int(EquipPos::Head)];
					if ((temp != -1) && (m_game->m_client_list[target_h]->m_item_list[temp] != 0)) {
						calculate_durability_decrement(target_h, attacker_h, attacker_type, target_type, temp);
					}
					break;
				}

				if ((attacker_sa == 2) && (m_game->m_client_list[target_h]->m_magic_effect_status[hb::shared::magic::Protect] != 0)) {
					m_game->send_notify_msg(0, target_h, Notify::MagicEffectOff, hb::shared::magic::Protect, m_game->m_client_list[target_h]->m_magic_effect_status[hb::shared::magic::Protect], 0, 0);
					switch (m_game->m_client_list[target_h]->m_magic_effect_status[hb::shared::magic::Protect]) {
					case 1:
						m_game->m_status_effect_manager->set_protection_from_arrow_flag(target_h, hb::shared::owner_class::Player, false);
						break;
					case 2:
					case 5:
						m_game->m_status_effect_manager->set_magic_protection_flag(target_h, hb::shared::owner_class::Player, false);
						break;
					case 3:
					case 4:
						m_game->m_status_effect_manager->set_defense_shield_flag(target_h, hb::shared::owner_class::Player, false);
						break;
					}
					m_game->m_client_list[target_h]->m_magic_effect_status[hb::shared::magic::Protect] = 0;
					m_game->m_delay_event_manager->remove_from_delay_event_list(target_h, hb::shared::owner_class::Player, hb::shared::magic::Protect);
				}

				if ((m_game->m_client_list[target_h]->m_is_poisoned == false) &&
					((attacker_sa == 5) || (attacker_sa == 6) || (attacker_sa == 61))) {
					if (check_resisting_poison_success(target_h, hb::shared::owner_class::Player) == false) {
						m_game->m_client_list[target_h]->m_is_poisoned = true;
						if (attacker_sa == 5)		m_game->m_client_list[target_h]->m_poison_level = 15;
						else if (attacker_sa == 6)  m_game->m_client_list[target_h]->m_poison_level = 40;
						else if (attacker_sa == 61) m_game->m_client_list[target_h]->m_poison_level = attacker_s_avalue;

						m_game->m_client_list[target_h]->m_poison_time = time;
						m_game->send_notify_msg(0, target_h, Notify::MagicEffectOn, hb::shared::magic::Poison, m_game->m_client_list[target_h]->m_poison_level, 0, 0);
						m_game->m_status_effect_manager->set_poison_flag(target_h, hb::shared::owner_class::Player, true);
					}
				}

				m_game->m_client_list[target_h]->m_hp -= iAP_SM;
				// Interrupt spell casting on damage
				if (iAP_SM > 0) {
					m_game->m_client_list[target_h]->m_last_damage_taken_time = GameClock::GetTimeMS();
					if (m_game->m_client_list[target_h]->m_magic_pause_time) {
						m_game->m_client_list[target_h]->m_magic_pause_time = false;
						m_game->m_client_list[target_h]->m_spell_count = -1;
						m_game->send_notify_msg(0, target_h, Notify::SpellInterrupted, 0, 0, 0, 0);
					}
				}
				if (m_game->m_client_list[target_h]->m_hp <= 0) {
					if (attacker_type == hb::shared::owner_class::Player)
						analyze_criminal_action(attacker_h, m_game->m_client_list[target_h]->m_x, m_game->m_client_list[target_h]->m_y);
					client_killed_handler(target_h, attacker_h, attacker_type, iAP_SM);
					killed = true;
					killed_dice = m_game->m_client_list[target_h]->m_level;
				}
				else {
					if (iAP_SM > 0) {
						if (m_game->m_client_list[target_h]->m_add_trans_mana > 0) {
							tmp1 = (double)m_game->m_client_list[target_h]->m_add_trans_mana;
							tmp2 = (double)iAP_SM;
							tmp3 = (tmp1 / 100.0f) * tmp2;
							temp = m_game->get_max_mp(target_h);
							m_game->m_client_list[target_h]->m_mp += (int)tmp3;
							if (m_game->m_client_list[target_h]->m_mp > temp) m_game->m_client_list[target_h]->m_mp = temp;
						}
						if (m_game->m_client_list[target_h]->m_add_charge_critical > 0) {
							if (m_game->dice(1, 100) <= static_cast<uint32_t>(m_game->m_client_list[target_h]->m_add_charge_critical)) {
								max_super_attack = (m_game->m_client_list[target_h]->m_level / 10);
								if (m_game->m_client_list[target_h]->m_super_attack_left < max_super_attack) m_game->m_client_list[target_h]->m_super_attack_left++;
								m_game->send_notify_msg(0, target_h, Notify::SuperAttackLeft, 0, 0, 0, 0);
							}
						}

						m_game->send_notify_msg(0, target_h, Notify::Hp, 0, 0, 0, 0);

						if (attacker_type == hb::shared::owner_class::Player)
							attacker_weapon = to_int(m_game->m_client_list[attacker_h]->get_equipped_weapon_class());
						else attacker_weapon = 1;

						if ((attacker_type == hb::shared::owner_class::Player) && (m_game->m_map_list[m_game->m_client_list[attacker_h]->m_map_index]->m_is_fight_zone))
							move_damage = 60;
						else move_damage = 40;

						if ((iAP_SM >= move_damage) && !(tgt_x == atk_x && tgt_y == atk_y)) {
							if (tgt_x == atk_x) {
								if (tgt_y > atk_y) damage_move_dir = direction::south;
								else if (tgt_y < atk_y) damage_move_dir = direction::north;
							}
							else if (tgt_x > atk_x) {
								if (tgt_y == atk_y)     damage_move_dir = direction::east;
								else if (tgt_y > atk_y) damage_move_dir = direction::southeast;
								else if (tgt_y < atk_y) damage_move_dir = direction::northeast;
							}
							else if (tgt_x < atk_x) {
								if (tgt_y == atk_y)     damage_move_dir = direction::west;
								else if (tgt_y > atk_y) damage_move_dir = direction::southwest;
								else if (tgt_y < atk_y) damage_move_dir = direction::northwest;
							}
							m_game->m_client_list[target_h]->m_last_damage = iAP_SM;

							m_game->send_notify_msg(0, target_h, Notify::DamageMove, damage_move_dir, iAP_SM, attacker_weapon, 0);
						}
						else {
							int prob;
							if (attacker_type == hb::shared::owner_class::Player) {
								switch (m_game->m_client_list[attacker_h]->m_using_weapon_skill) {
								case 6: prob = 3500; break;
								case 8: prob = 1000; break;
								case 9: prob = 2900; break;
								case 10: prob = 2500; break;
								case 14: prob = 2000; break;
								case 21: prob = 2000; break;
								default: prob = 1; break;
								}
							}
							else prob = 1;

							if (m_game->dice(1, 10000) >= static_cast<uint32_t>(prob))
								m_game->send_event_to_near_client_type_a(target_h, hb::shared::owner_class::Player, MsgId::EventMotion, Type::Damage, iAP_SM, attacker_weapon, 0);
						}

						if (m_game->m_client_list[target_h]->m_magic_effect_status[hb::shared::magic::HoldObject] == 1) {
							m_game->send_notify_msg(0, target_h, Notify::MagicEffectOff, hb::shared::magic::HoldObject, m_game->m_client_list[target_h]->m_magic_effect_status[hb::shared::magic::HoldObject], 0, 0);
							m_game->m_client_list[target_h]->m_magic_effect_status[hb::shared::magic::HoldObject] = 0;
							m_game->m_delay_event_manager->remove_from_delay_event_list(target_h, hb::shared::owner_class::Player, hb::shared::magic::HoldObject);
						}

						m_game->m_client_list[target_h]->m_super_attack_count++;
						if (m_game->m_client_list[target_h]->m_super_attack_count > 14) {
							m_game->m_client_list[target_h]->m_super_attack_count = 0;
							max_super_attack = (m_game->m_client_list[target_h]->m_level / 10);
							if (m_game->m_client_list[target_h]->m_super_attack_left < max_super_attack) m_game->m_client_list[target_h]->m_super_attack_left++;
							m_game->send_notify_msg(0, target_h, Notify::SuperAttackLeft, 0, 0, 0, 0);
						}
					}
				}
			}
			break;

		case hb::shared::owner_class::Npc:
			if (m_game->m_npc_list[target_h]->m_behavior == Behavior::Dead) return 0;
			if (m_game->m_npc_list[target_h]->m_is_killed) return 0;
			if (m_game->m_is_crusade_mode) {
				if (attacker_side == m_game->m_npc_list[target_h]->m_side) {
					switch (m_game->m_npc_list[target_h]->m_type) {
					case 40:
					case 41:
					case 43:
					case 44:
					case 45:
					case 46:
					case 47:
					case 51:
						return 0;

					default: break;
					}
				}
				else {
					switch (m_game->m_npc_list[target_h]->m_type) {
					case 41:
						if (attacker_side != 0) {
							m_game->m_npc_list[target_h]->m_v1 += iAP_L;
							if (m_game->m_npc_list[target_h]->m_v1 > 500) {
								m_game->m_npc_list[target_h]->m_v1 = 0;
								m_game->m_npc_list[target_h]->m_mana_stock--;
								if (m_game->m_npc_list[target_h]->m_mana_stock <= 0) m_game->m_npc_list[target_h]->m_mana_stock = 0;
								hb::logger::log("Mana stock reduced: {}", m_game->m_npc_list[target_h]->m_mana_stock);
							}
						}
						break;
					}
				}
			}
			switch (m_game->m_npc_list[target_h]->m_action_limit) {
			case 1:
			case 2:
				return 0;
			}

			if (m_game->m_npc_list[target_h]->m_size == 0)
				damage = iAP_SM;
			else damage = iAP_L;

			if (m_game->m_npc_list[target_h]->m_abs_damage < 0) {
				tmp1 = (double)damage;
				tmp2 = (double)(abs(m_game->m_npc_list[target_h]->m_abs_damage)) / 100.0f;
				tmp3 = tmp1 * tmp2;
				tmp2 = tmp1 - tmp3;
				damage = (int)tmp2;
				if (damage < 0) damage = 1;
				else if ((m_game->m_npc_list[target_h]->m_type == 31) && (attacker_type == 1) && (m_game->m_client_list[attacker_h] != 0) && (m_game->m_client_list[attacker_h]->m_special_ability_type == 7))
					damage += m_game->dice(3, 2);
			}

			if ((attacker_sa == 2) && (m_game->m_npc_list[target_h]->m_magic_effect_status[hb::shared::magic::Protect] != 0)) {
				switch (m_game->m_npc_list[target_h]->m_magic_effect_status[hb::shared::magic::Protect]) {
				case 1:
					m_game->m_status_effect_manager->set_protection_from_arrow_flag(target_h, hb::shared::owner_class::Npc, false);
					break;
				case 2:
				case 5:
					m_game->m_status_effect_manager->set_magic_protection_flag(target_h, hb::shared::owner_class::Npc, false);
					break;
				case 3:
				case 4:
					m_game->m_status_effect_manager->set_defense_shield_flag(target_h, hb::shared::owner_class::Npc, false);
					break;
				}
				m_game->m_npc_list[target_h]->m_magic_effect_status[hb::shared::magic::Protect] = 0;
				m_game->m_delay_event_manager->remove_from_delay_event_list(target_h, hb::shared::owner_class::Npc, hb::shared::magic::Protect);
			}

			switch (m_game->m_npc_list[target_h]->m_action_limit) {
			case 0:
			case 3:
			case 5:
				m_game->m_npc_list[target_h]->m_hp -= damage;
				break;
			}

			if (m_game->m_npc_list[target_h]->m_hp <= 0) {
				m_game->m_entity_manager->on_entity_killed(target_h, attacker_h, attacker_type, damage);
				killed = true;
				killed_dice = std::max(1, m_game->m_npc_list[target_h]->m_max_hp / 5);
			}
			else {
				bool skip_counter =
					((m_game->m_npc_list[target_h]->m_type != 21) && (m_game->m_npc_list[target_h]->m_type != 55) && (m_game->m_npc_list[target_h]->m_type != 56)
						&& (m_game->m_npc_list[target_h]->m_side == attacker_side))
					|| (m_game->m_npc_list[target_h]->m_action_limit != 0)
					|| (m_game->m_npc_list[target_h]->m_is_perm_attack_mode)
					|| ((m_game->m_npc_list[target_h]->m_is_summoned) && (m_game->m_npc_list[target_h]->m_summon_control_mode == 1))
					|| (m_game->m_npc_list[target_h]->m_type == 51);

				if (!skip_counter) {
					if (m_game->dice(1, 3) == 2) {
						if (m_game->m_npc_list[target_h]->m_behavior == Behavior::Attack) {
							tX = tY = 0;
							switch (m_game->m_npc_list[target_h]->m_target_type) {
							case hb::shared::owner_class::Player:
								if (m_game->m_client_list[m_game->m_npc_list[target_h]->m_target_index] != 0) {
									tX = m_game->m_client_list[m_game->m_npc_list[target_h]->m_target_index]->m_x;
									tY = m_game->m_client_list[m_game->m_npc_list[target_h]->m_target_index]->m_y;
								}
								break;

							case hb::shared::owner_class::Npc:
								if (m_game->m_npc_list[m_game->m_npc_list[target_h]->m_target_index] != 0) {
									tX = m_game->m_npc_list[m_game->m_npc_list[target_h]->m_target_index]->m_x;
									tY = m_game->m_npc_list[m_game->m_npc_list[target_h]->m_target_index]->m_y;
								}
								break;
							}

							dst1 = (m_game->m_npc_list[target_h]->m_x - tX) * (m_game->m_npc_list[target_h]->m_x - tX) + (m_game->m_npc_list[target_h]->m_y - tY) * (m_game->m_npc_list[target_h]->m_y - tY);

							tX = tY = 0;
							switch (attacker_type) {
							case hb::shared::owner_class::Player:
								if (m_game->m_client_list[attacker_h] != 0) {
									tX = m_game->m_client_list[attacker_h]->m_x;
									tY = m_game->m_client_list[attacker_h]->m_y;
								}
								break;

							case hb::shared::owner_class::Npc:
								if (m_game->m_npc_list[attacker_h] != 0) {
									tX = m_game->m_npc_list[attacker_h]->m_x;
									tY = m_game->m_npc_list[attacker_h]->m_y;
								}
								break;
							}

							dst2 = (m_game->m_npc_list[target_h]->m_x - tX) * (m_game->m_npc_list[target_h]->m_x - tX) + (m_game->m_npc_list[target_h]->m_y - tY) * (m_game->m_npc_list[target_h]->m_y - tY);

							if (dst2 <= dst1) {
								m_game->m_npc_list[target_h]->m_behavior = Behavior::Attack;
								m_game->m_npc_list[target_h]->m_behavior_turn_count = 0;
								m_game->m_npc_list[target_h]->m_target_index = attacker_h;
								m_game->m_npc_list[target_h]->m_target_type = attacker_type;
							}
						}
						else {
							m_game->m_npc_list[target_h]->m_behavior = Behavior::Attack;
							m_game->m_npc_list[target_h]->m_behavior_turn_count = 0;
							m_game->m_npc_list[target_h]->m_target_index = attacker_h;
							m_game->m_npc_list[target_h]->m_target_type = attacker_type;
						}
					}
				}

				if ((m_game->dice(1, 3) == 2) && (m_game->m_npc_list[target_h]->m_action_limit == 0))
					m_game->m_npc_list[target_h]->m_time = time;

				if (attacker_type == hb::shared::owner_class::Player)
					attacker_weapon = to_int(m_game->m_client_list[attacker_h]->get_equipped_weapon_class());
				else attacker_weapon = 1;

				if ((wc != weapon_class::bow) && (m_game->m_npc_list[target_h]->m_action_limit == 4)) {
					do {
						if (tgt_x == atk_x) {
							if (tgt_y == atk_y)     break;
							else if (tgt_y > atk_y) damage_move_dir = direction::south;
							else if (tgt_y < atk_y) damage_move_dir = direction::north;
						}
						else if (tgt_x > atk_x) {
							if (tgt_y == atk_y)     damage_move_dir = direction::east;
							else if (tgt_y > atk_y) damage_move_dir = direction::southeast;
							else if (tgt_y < atk_y) damage_move_dir = direction::northeast;
						}
						else if (tgt_x < atk_x) {
							if (tgt_y == atk_y)     damage_move_dir = direction::west;
							else if (tgt_y > atk_y) damage_move_dir = direction::southwest;
							else if (tgt_y < atk_y) damage_move_dir = direction::northwest;
						}

						dX = m_game->m_npc_list[target_h]->m_x + _tmp_cTmpDirX[damage_move_dir];
						dY = m_game->m_npc_list[target_h]->m_y + _tmp_cTmpDirY[damage_move_dir];

						if (m_game->m_map_list[m_game->m_npc_list[target_h]->m_map_index]->get_moveable(dX, dY, 0) == false) {
							damage_move_dir = static_cast<direction>(m_game->dice(1, 8));
							dX = m_game->m_npc_list[target_h]->m_x + _tmp_cTmpDirX[damage_move_dir];
							dY = m_game->m_npc_list[target_h]->m_y + _tmp_cTmpDirY[damage_move_dir];

							if (m_game->m_map_list[m_game->m_npc_list[target_h]->m_map_index]->get_moveable(dX, dY, 0) == false) break;
						}

						m_game->m_map_list[m_game->m_npc_list[target_h]->m_map_index]->clear_owner(5, target_h, hb::shared::owner_class::Npc, m_game->m_npc_list[target_h]->m_x, m_game->m_npc_list[target_h]->m_y);
						m_game->m_map_list[m_game->m_npc_list[target_h]->m_map_index]->set_owner(target_h, hb::shared::owner_class::Npc, dX, dY);
						m_game->m_npc_list[target_h]->m_x = dX;
						m_game->m_npc_list[target_h]->m_y = dY;
						m_game->m_npc_list[target_h]->m_dir = damage_move_dir;

						m_game->send_event_to_near_client_type_a(target_h, hb::shared::owner_class::Npc, MsgId::EventMotion, Type::Move, 0, 0, 0);

						dX = m_game->m_npc_list[target_h]->m_x + _tmp_cTmpDirX[damage_move_dir];
						dY = m_game->m_npc_list[target_h]->m_y + _tmp_cTmpDirY[damage_move_dir];

						if (m_game->m_map_list[m_game->m_npc_list[target_h]->m_map_index]->get_moveable(dX, dY, 0) == false) {
							damage_move_dir = static_cast<direction>(m_game->dice(1, 8));
							dX = m_game->m_npc_list[target_h]->m_x + _tmp_cTmpDirX[damage_move_dir];
							dY = m_game->m_npc_list[target_h]->m_y + _tmp_cTmpDirY[damage_move_dir];
							if (m_game->m_map_list[m_game->m_npc_list[target_h]->m_map_index]->get_moveable(dX, dY, 0) == false) break;
						}

						m_game->m_map_list[m_game->m_npc_list[target_h]->m_map_index]->clear_owner(5, target_h, hb::shared::owner_class::Npc, m_game->m_npc_list[target_h]->m_x, m_game->m_npc_list[target_h]->m_y);
						m_game->m_map_list[m_game->m_npc_list[target_h]->m_map_index]->set_owner(target_h, hb::shared::owner_class::Npc, dX, dY);
						m_game->m_npc_list[target_h]->m_x = dX;
						m_game->m_npc_list[target_h]->m_y = dY;
						m_game->m_npc_list[target_h]->m_dir = damage_move_dir;

						m_game->send_event_to_near_client_type_a(target_h, hb::shared::owner_class::Npc, MsgId::EventMotion, Type::Move, 0, 0, 0);

						if (m_game->m_war_manager->check_energy_sphere_destination(target_h, attacker_h, attacker_type)) {
							if (attacker_type == hb::shared::owner_class::Player) {
								exp = (m_game->m_npc_list[target_h]->m_exp / 3);
								if (m_game->m_npc_list[target_h]->m_no_die_remain_exp > 0)
									exp += m_game->m_npc_list[target_h]->m_no_die_remain_exp;

								if (m_game->m_client_list[attacker_h]->m_add_exp != 0) {
									tmp1 = (double)m_game->m_client_list[attacker_h]->m_add_exp;
									tmp2 = (double)exp;
									tmp3 = (tmp1 / 100.0f) * tmp2;
									exp += (int)tmp3;
								}

								if ((m_game->m_is_crusade_mode) && (exp > 10)) exp = 10;

								m_game->get_exp(attacker_h, exp);

								// Use EntityManager for NPC deletion
								if (m_game->m_entity_manager != NULL)
									m_game->m_entity_manager->delete_entity(target_h);
								return false;
							}
						}
					} while (false);
				}
				else {
					m_game->send_event_to_near_client_type_a(target_h, hb::shared::owner_class::Npc, MsgId::EventMotion, Type::Damage, damage, attacker_weapon, 0);
				}

				if (m_game->m_npc_list[target_h]->m_magic_effect_status[hb::shared::magic::HoldObject] == 1) {
					m_game->m_npc_list[target_h]->m_magic_effect_status[hb::shared::magic::HoldObject] = 0;
					m_game->m_delay_event_manager->remove_from_delay_event_list(target_h, hb::shared::owner_class::Npc, hb::shared::magic::HoldObject);
				}
				else if (m_game->m_npc_list[target_h]->m_magic_effect_status[hb::shared::magic::HoldObject] == 2) {
					if (m_game->m_npc_list[target_h]->m_hold_resist > 0 && m_game->dice(1, 100) <= static_cast<uint32_t>(m_game->m_npc_list[target_h]->m_hold_resist)) {
						m_game->m_npc_list[target_h]->m_magic_effect_status[hb::shared::magic::HoldObject] = 0;
						m_game->m_delay_event_manager->remove_from_delay_event_list(target_h, hb::shared::owner_class::Npc, hb::shared::magic::HoldObject);
					}
				}

				if ((m_game->m_npc_list[target_h]->m_no_die_remain_exp > 0) && (m_game->m_npc_list[target_h]->m_is_summoned != true) &&
					(attacker_type == hb::shared::owner_class::Player) && (m_game->m_client_list[attacker_h] != 0)) {
					if (m_game->m_npc_list[target_h]->m_no_die_remain_exp > static_cast<uint32_t>(damage)) {
						exp = damage;
						m_game->m_npc_list[target_h]->m_no_die_remain_exp -= damage;
					}
					else {
						exp = m_game->m_npc_list[target_h]->m_no_die_remain_exp;
						m_game->m_npc_list[target_h]->m_no_die_remain_exp = 0;
					}

					if (m_game->m_client_list[attacker_h]->m_add_exp != 0) {
						tmp1 = (double)m_game->m_client_list[attacker_h]->m_add_exp;
						tmp2 = (double)exp;
						tmp3 = (tmp1 / 100.0f) * tmp2;
						exp += (int)tmp3;
					}

					if (m_game->m_is_crusade_mode) exp = exp / 3;

					if (m_game->m_client_list[attacker_h]->m_level > 100) {
						switch (m_game->m_npc_list[target_h]->m_type) {
						case 55:
						case 56:
							exp = 0;
							break;
						default: break;
						}
					}
				}
			}
			break;
		}

		if (attacker_type == hb::shared::owner_class::Player) {
			if (m_game->m_client_list[attacker_h]->m_item_equipment_status[to_int(EquipPos::TwoHand)] != -1)
				weapon_index = m_game->m_client_list[attacker_h]->m_item_equipment_status[to_int(EquipPos::TwoHand)];
			else weapon_index = m_game->m_client_list[attacker_h]->m_item_equipment_status[to_int(EquipPos::RightHand)];

			if ((weapon_index != -1) && (arrow_use != true)) {
				if ((m_game->m_client_list[attacker_h]->m_item_list[weapon_index] != 0) &&
					(m_game->m_client_list[attacker_h]->m_item_list[weapon_index]->m_id_num != 231)) {
					if (killed == false)
						m_game->m_item_manager->calculate_ssn_item_index(attacker_h, weapon_index, 1);
					else {
						if (m_game->m_client_list[attacker_h]->m_hp <= 3)
							m_game->m_item_manager->calculate_ssn_item_index(attacker_h, weapon_index, m_game->dice(1, killed_dice) * 2);
						else m_game->m_item_manager->calculate_ssn_item_index(attacker_h, weapon_index, m_game->dice(1, killed_dice));
					}
				}

				if ((m_game->m_client_list[attacker_h]->m_item_list[weapon_index] != 0) &&
					(m_game->m_client_list[attacker_h]->m_item_list[weapon_index]->m_durability != 0)) {
					wep_life_off = 1;
					if (wc != weapon_class::none && wc != weapon_class::bow) {
						switch (m_game->m_map_list[m_game->m_client_list[attacker_h]->m_map_index]->m_weather_status) {
						case 0:	break;
						case 1:	if (m_game->dice(1, 3) == 1) wep_life_off++; break;
						case 2:	if (m_game->dice(1, 2) == 1) wep_life_off += m_game->dice(1, 2); break;
						case 3:	if (m_game->dice(1, 2) == 1) wep_life_off += m_game->dice(1, 3); break;
						}
					}

					if (m_game->m_client_list[attacker_h]->m_item_list[weapon_index]->m_instance.cur_durability < wep_life_off)
						m_game->m_client_list[attacker_h]->m_item_list[weapon_index]->m_instance.cur_durability = 0;
					else m_game->m_client_list[attacker_h]->m_item_list[weapon_index]->m_instance.cur_durability -= wep_life_off;

					m_game->send_notify_msg(0, attacker_h, Notify::CurDurability, weapon_index, m_game->m_client_list[attacker_h]->m_item_list[weapon_index]->m_instance.cur_durability, 0, 0);

					if (m_game->m_client_list[attacker_h]->m_item_list[weapon_index]->m_instance.cur_durability == 0) {
						m_game->send_notify_msg(0, attacker_h, Notify::ItemDurabilityEnd, m_game->m_client_list[attacker_h]->m_item_list[weapon_index]->m_equip_pos, weapon_index, 0, 0);
						m_game->m_item_manager->release_item_handler(attacker_h, weapon_index, true);
					}
				}
			}
			else {
				if (wc == weapon_class::none) {
					m_game->m_skill_manager->calculate_ssn_skill_index(attacker_h, 5, 1);
				}
			}
		}
	}
	else {
		if (attacker_type == hb::shared::owner_class::Player) {
			m_game->m_client_list[attacker_h]->m_combo_attack_count = 0;
		}
	}

	return exp;
}
