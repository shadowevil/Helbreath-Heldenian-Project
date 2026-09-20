#include "LootManager.h"
#include "Game.h"
#include "ItemManager.h"
#include "CombatManager.h"
#include "Packet/SharedPackets.h"
#include "Item.h"
#include "Log.h"
#include "ServerLogChannels.h"

using namespace hb::shared::net;

using hb::log_channel;
using namespace hb::shared::action;
using namespace hb::server::net;
using namespace hb::server::config;
using namespace hb::shared::item;
namespace sock = hb::shared::net::socket;

extern char G_cTxt[512];

void LootManager::apply_pk_penalty(short attacker_h, short victum_h)
{
	uint32_t v1, v2;

	if (m_game->m_client_list[attacker_h] == 0) return;
	if (m_game->m_client_list[victum_h] == 0) return;
	if ((m_game->m_client_list[attacker_h]->m_is_safe_attack_mode) && (m_game->m_client_list[attacker_h]->m_player_kill_count == 0)) return;
	if ((strcmp(m_game->m_client_list[victum_h]->m_location, "aresden") != 0) && (strcmp(m_game->m_client_list[victum_h]->m_location, "elvine") != 0) && (strcmp(m_game->m_client_list[victum_h]->m_location, "elvhunter") != 0) && (strcmp(m_game->m_client_list[victum_h]->m_location, "arehunter") != 0)) {
		return;
	}

	// PK Count
	m_game->m_client_list[attacker_h]->m_player_kill_count++;

	m_game->m_combat_manager->pk_log(PkLog::ByPk, attacker_h, victum_h, 0);

	v1 = m_game->dice((m_game->m_client_list[victum_h]->m_level / 2) + 1, 50);
	v2 = m_game->dice((m_game->m_client_list[attacker_h]->m_level / 2) + 1, 50);

	m_game->m_client_list[attacker_h]->m_exp -= v1;
	m_game->m_client_list[attacker_h]->m_exp -= v2;
	if (m_game->m_client_list[attacker_h]->m_exp < 0) m_game->m_client_list[attacker_h]->m_exp = 0;

	m_game->send_notify_msg(0, attacker_h, Notify::PkPenalty, 0, 0, 0, 0);

	m_game->send_event_to_near_client_type_a(attacker_h, hb::shared::owner_class::Player, MsgId::EventMotion, Type::NullAction, 0, 0, 0);

	// std::snprintf(G_cTxt, sizeof(G_cTxt), "(!) PK-penalty: (%s)  (%d) (%d) ", m_game->m_client_list[attacker_h]->m_char_name, v1+v2, m_game->m_client_list[attacker_h]->m_exp);
	//PutLogFileList(G_cTxt);

	m_game->m_city_status[m_game->m_client_list[attacker_h]->m_side].crimes++;

	m_game->m_client_list[attacker_h]->m_rating -= 10;
	if (m_game->m_client_list[attacker_h]->m_rating > 10000)  m_game->m_client_list[attacker_h]->m_rating = 10000;
	if (m_game->m_client_list[attacker_h]->m_rating < -10000) m_game->m_client_list[attacker_h]->m_rating = -10000;

	if (strcmp(m_game->m_client_list[attacker_h]->m_location, "aresden") == 0) {
		if ((strcmp(m_game->m_map_list[m_game->m_client_list[attacker_h]->m_map_index]->m_name, "arebrk11") == 0) ||
			(strcmp(m_game->m_map_list[m_game->m_client_list[attacker_h]->m_map_index]->m_name, "arebrk12") == 0) ||
			(strcmp(m_game->m_map_list[m_game->m_client_list[attacker_h]->m_map_index]->m_name, "arebrk21") == 0) ||
			(strcmp(m_game->m_map_list[m_game->m_client_list[attacker_h]->m_map_index]->m_name, "arebrk22") == 0) ||
			(strcmp(m_game->m_map_list[m_game->m_client_list[attacker_h]->m_map_index]->m_name, "aresden") == 0) ||
			(strcmp(m_game->m_map_list[m_game->m_client_list[attacker_h]->m_map_index]->m_name, "huntzone2") == 0) ||
			(strcmp(m_game->m_map_list[m_game->m_client_list[attacker_h]->m_map_index]->m_name, "areuni") == 0) ||
			(strcmp(m_game->m_map_list[m_game->m_client_list[attacker_h]->m_map_index]->m_name, "arefarm") == 0)) {

			// PK .   5
			std::memset(m_game->m_client_list[attacker_h]->m_locked_map_name, 0, sizeof(m_game->m_client_list[attacker_h]->m_locked_map_name));
			strcpy(m_game->m_client_list[attacker_h]->m_locked_map_name, "arejail");
			m_game->m_client_list[attacker_h]->m_locked_map_time = 60 * 3;
			m_game->request_teleport_handler(attacker_h, "2   ", "arejail", -1, -1);
			return;
		}
	}

	if (strcmp(m_game->m_client_list[attacker_h]->m_location, "elvine") == 0) {
		if ((strcmp(m_game->m_map_list[m_game->m_client_list[attacker_h]->m_map_index]->m_name, "elvbrk11") == 0) ||
			(strcmp(m_game->m_map_list[m_game->m_client_list[attacker_h]->m_map_index]->m_name, "elvbrk12") == 0) ||
			(strcmp(m_game->m_map_list[m_game->m_client_list[attacker_h]->m_map_index]->m_name, "elvbrk21") == 0) ||
			(strcmp(m_game->m_map_list[m_game->m_client_list[attacker_h]->m_map_index]->m_name, "elvbrk22") == 0) ||
			(strcmp(m_game->m_map_list[m_game->m_client_list[attacker_h]->m_map_index]->m_name, "elvine") == 0) ||
			(strcmp(m_game->m_map_list[m_game->m_client_list[attacker_h]->m_map_index]->m_name, "huntzone1") == 0) ||
			(strcmp(m_game->m_map_list[m_game->m_client_list[attacker_h]->m_map_index]->m_name, "elvuni") == 0) ||
			(strcmp(m_game->m_map_list[m_game->m_client_list[attacker_h]->m_map_index]->m_name, "elvfarm") == 0)) {

			// PK .   5
			std::memset(m_game->m_client_list[attacker_h]->m_locked_map_name, 0, sizeof(m_game->m_client_list[attacker_h]->m_locked_map_name));
			strcpy(m_game->m_client_list[attacker_h]->m_locked_map_name, "elvjail");
			m_game->m_client_list[attacker_h]->m_locked_map_time = 60 * 3;
			m_game->request_teleport_handler(attacker_h, "2   ", "elvjail", -1, -1);
			return;
		}
	}
}

// 05/17/2004 - Hypnotoad - register pk log
void LootManager::pk_kill_reward_handler(short attacker_h, short victum_h)
{
	if (m_game->m_client_list[attacker_h] == 0) return;
	if (m_game->m_client_list[victum_h] == 0)   return;

	m_game->m_combat_manager->pk_log(PkLog::ByPlayer, attacker_h, victum_h, 0);

	if (m_game->m_client_list[attacker_h]->m_player_kill_count != 0) {
		// PK   PK   .

	}
	else {
		m_game->m_client_list[attacker_h]->m_reward_gold += m_game->get_exp_level(m_game->m_client_list[victum_h]->m_exp) * 3;

		if (m_game->m_client_list[attacker_h]->m_reward_gold > MaxRewardGold)
			m_game->m_client_list[attacker_h]->m_reward_gold = MaxRewardGold;
		if (m_game->m_client_list[attacker_h]->m_reward_gold < 0)
			m_game->m_client_list[attacker_h]->m_reward_gold = 0;

		m_game->send_notify_msg(0, attacker_h, Notify::PkCaptured, m_game->m_client_list[victum_h]->m_player_kill_count, m_game->m_client_list[victum_h]->m_level, 0, m_game->m_client_list[victum_h]->m_char_name);
	}
}

void LootManager::enemy_kill_reward_handler(int attacker_h, int client_h)
{
	// enemy-kill-mode = 1 | 0
	// if m_enemy_kill_mode is true than death match mode

	int iEK_Level;
	uint32_t reward_exp;

	if (m_game->m_client_list[attacker_h] == 0) return;
	if (m_game->m_client_list[client_h] == 0)   return;

	m_game->m_combat_manager->pk_log(PkLog::ByEnemy, attacker_h, client_h, 0);

	iEK_Level = 30;
	if (m_game->m_client_list[attacker_h]->m_level >= 80) iEK_Level = 80;
	if (m_game->m_client_list[attacker_h]->m_level >= m_game->m_max_level) {
		if (m_game->get_exp_level(m_game->m_client_list[client_h]->m_exp) >= iEK_Level) {
			if ((memcmp(m_game->m_client_list[client_h]->m_location, m_game->m_client_list[client_h]->m_map_name, 10) != 0)
				&& (m_game->m_enemy_kill_mode == false)) {
				m_game->m_client_list[attacker_h]->m_enemy_kill_count += m_game->m_enemy_kill_adjust;
			}

			if (m_game->m_enemy_kill_mode) {
				m_game->m_client_list[attacker_h]->m_enemy_kill_count += m_game->m_enemy_kill_adjust;
			}
		}
		m_game->m_client_list[attacker_h]->m_reward_gold += m_game->dice(1, (m_game->get_exp_level(m_game->m_client_list[client_h]->m_exp)));
		if (m_game->m_client_list[attacker_h]->m_reward_gold > MaxRewardGold)
			m_game->m_client_list[attacker_h]->m_reward_gold = MaxRewardGold;
		if (m_game->m_client_list[attacker_h]->m_reward_gold < 0)
			m_game->m_client_list[attacker_h]->m_reward_gold = 0;

		m_game->send_notify_msg(0, attacker_h, Notify::EnemyKillReward, client_h, 0, 0, 0);
		return;
	}

	if (m_game->m_client_list[attacker_h]->m_player_kill_count != 0) {
	}
	else {
		{
			reward_exp = (m_game->dice(3, (3 * m_game->get_exp_level(m_game->m_client_list[client_h]->m_exp))) + m_game->get_exp_level(m_game->m_client_list[client_h]->m_exp)) / 3;

			if (m_game->m_is_crusade_mode) {
				m_game->m_client_list[attacker_h]->m_exp += (reward_exp / 3) * 4;
				m_game->m_client_list[attacker_h]->m_war_contribution += (reward_exp - (reward_exp / 3)) * 12;

				if (m_game->m_client_list[attacker_h]->m_war_contribution > m_game->m_max_war_contribution)
					m_game->m_client_list[attacker_h]->m_war_contribution = m_game->m_max_war_contribution;

				m_game->m_client_list[attacker_h]->m_construction_point += m_game->m_client_list[client_h]->m_level / 2;

				if (m_game->m_client_list[attacker_h]->m_construction_point > m_game->m_max_construction_points)
					m_game->m_client_list[attacker_h]->m_construction_point = m_game->m_max_construction_points;

				//testcode
				hb::logger::log("Enemy player killed by player, construction +{}, war contribution +{}", m_game->m_client_list[client_h]->m_level / 2, (reward_exp - (reward_exp / 3)) * 6);

				m_game->send_notify_msg(0, attacker_h, Notify::ConstructionPoint, m_game->m_client_list[attacker_h]->m_construction_point, m_game->m_client_list[attacker_h]->m_war_contribution, 0, 0);

				if (m_game->get_exp_level(m_game->m_client_list[client_h]->m_exp) >= iEK_Level) {
					if (memcmp(m_game->m_client_list[client_h]->m_location, m_game->m_client_list[client_h]->m_map_name, 10) != 0) {
						m_game->m_client_list[attacker_h]->m_enemy_kill_count += m_game->m_enemy_kill_adjust;
					}
					if (m_game->m_enemy_kill_mode) {
						m_game->m_client_list[attacker_h]->m_enemy_kill_count += m_game->m_enemy_kill_adjust;
					}
				}
				m_game->m_client_list[attacker_h]->m_reward_gold += m_game->dice(1, (m_game->get_exp_level(m_game->m_client_list[client_h]->m_exp)));
				if (m_game->m_client_list[attacker_h]->m_reward_gold > MaxRewardGold)
					m_game->m_client_list[attacker_h]->m_reward_gold = MaxRewardGold;
				if (m_game->m_client_list[attacker_h]->m_reward_gold < 0)
					m_game->m_client_list[attacker_h]->m_reward_gold = 0;
			}
			else {
				m_game->m_client_list[attacker_h]->m_exp += reward_exp;
				if (m_game->get_exp_level(m_game->m_client_list[client_h]->m_exp) >= iEK_Level) {
					if ((memcmp(m_game->m_client_list[client_h]->m_location, m_game->m_client_list[client_h]->m_map_name, 10) != 0)
						&& (m_game->m_enemy_kill_mode == false)) {
						m_game->m_client_list[attacker_h]->m_enemy_kill_count += m_game->m_enemy_kill_adjust;
					}

					if (m_game->m_enemy_kill_mode) {
						m_game->m_client_list[attacker_h]->m_enemy_kill_count += m_game->m_enemy_kill_adjust;
					}
				}
				m_game->m_client_list[attacker_h]->m_reward_gold += m_game->dice(1, (m_game->get_exp_level(m_game->m_client_list[client_h]->m_exp)));
				if (m_game->m_client_list[attacker_h]->m_reward_gold > MaxRewardGold)
					m_game->m_client_list[attacker_h]->m_reward_gold = MaxRewardGold;
				if (m_game->m_client_list[attacker_h]->m_reward_gold < 0)
					m_game->m_client_list[attacker_h]->m_reward_gold = 0;
			}
		}

		m_game->send_notify_msg(0, attacker_h, Notify::EnemyKillReward, client_h, 0, 0, 0);

		if (m_game->check_limited_user(attacker_h) == false) {
			m_game->send_notify_msg(0, attacker_h, Notify::Exp, 0, 0, 0, 0);
		}
		m_game->check_level_up(attacker_h);

		m_game->m_city_status[m_game->m_client_list[attacker_h]->m_side].wins++;
	}
}

// 05/22/2004 - Hypnotoad - register in pk log
void LootManager::apply_combat_killed_penalty(int client_h, int penalty_level, bool is_s_aattacked)
{
	uint32_t exp;

	if (m_game->m_client_list[client_h] == 0) return;
	if (m_game->m_client_list[client_h]->m_is_init_complete == false) return;

	// Crusade
	if (m_game->m_is_crusade_mode) {
		// PKcount
		if (m_game->m_client_list[client_h]->m_player_kill_count > 0) {
			m_game->m_client_list[client_h]->m_player_kill_count--;
			m_game->send_notify_msg(0, client_h, Notify::PkPenalty, 0, 0, 0, 0);
			// v2.15
			m_game->m_combat_manager->pk_log(PkLog::ReduceCriminal, 0, client_h, 0);

		}
		return;
	}
	else {
		// PKcount
		if (m_game->m_client_list[client_h]->m_player_kill_count > 0) {
			m_game->m_client_list[client_h]->m_player_kill_count--;
			m_game->send_notify_msg(0, client_h, Notify::PkPenalty, 0, 0, 0, 0);
			// v2.15
			m_game->m_combat_manager->pk_log(PkLog::ReduceCriminal, 0, client_h, 0);
		}

		exp = m_game->dice(1, (5 * penalty_level * m_game->m_client_list[client_h]->m_level));

		if (m_game->m_client_list[client_h]->m_is_neutral) exp = exp / 3;

		// if (m_game->m_client_list[client_h]->m_level == hb::shared::limits::PlayerMaxLevel) exp = 0;

		if (exp > m_game->m_client_list[client_h]->m_exp)
			m_game->m_client_list[client_h]->m_exp = 0;
		else
			m_game->m_client_list[client_h]->m_exp -= exp;

		m_game->send_notify_msg(0, client_h, Notify::Exp, 0, 0, 0, 0);

		if (m_game->m_client_list[client_h]->m_is_neutral != true) {
			if (m_game->m_client_list[client_h]->m_level < 80) {
				// v2.03 60 -> 80
				penalty_level--;
				if (penalty_level <= 0) penalty_level = 1;
				penalty_item_drop(client_h, penalty_level, is_s_aattacked);
			}
			else penalty_item_drop(client_h, penalty_level, is_s_aattacked);
		}
	}
}

// 05/29/2004 - Hypnotoad - Limits some items from not dropping
void LootManager::penalty_item_drop(int client_h, int total, bool is_s_aattacked)
{
	int j, remain_item;
	char item_index_list[hb::shared::limits::MaxItems], item_index;

	if (m_game->m_client_list[client_h] == 0) return;
	if (m_game->m_client_list[client_h]->m_is_init_complete == false) return;

	if ((m_game->m_client_list[client_h]->m_alter_item_drop_index != -1) && (m_game->m_client_list[client_h]->m_item_list[m_game->m_client_list[client_h]->m_alter_item_drop_index] != 0)) {
		// Testcode
		if (m_game->m_client_list[client_h]->m_item_list[m_game->m_client_list[client_h]->m_alter_item_drop_index]->get_item_effect_type() == ItemEffectType::AlterItemDrop) {
			if (m_game->m_client_list[client_h]->m_item_list[m_game->m_client_list[client_h]->m_alter_item_drop_index]->m_instance.cur_durability > 0) {
				m_game->m_client_list[client_h]->m_item_list[m_game->m_client_list[client_h]->m_alter_item_drop_index]->m_instance.cur_durability--;
				m_game->send_notify_msg(0, client_h, Notify::CurDurability, m_game->m_client_list[client_h]->m_alter_item_drop_index, m_game->m_client_list[client_h]->m_item_list[m_game->m_client_list[client_h]->m_alter_item_drop_index]->m_instance.cur_durability, 0, 0);
			}
			m_game->m_item_manager->drop_item_handler(client_h, m_game->m_client_list[client_h]->m_alter_item_drop_index, -1, m_game->m_client_list[client_h]->m_item_list[m_game->m_client_list[client_h]->m_alter_item_drop_index]->m_name);

			m_game->m_client_list[client_h]->m_alter_item_drop_index = -1;
			return;
		}
		else {
			// v2.04 testcode
			hb::logger::log<log_channel::events>("Alter Drop Item Index Error1");
			for(int i = 0; i < hb::shared::limits::MaxItems; i++)
				if ((m_game->m_client_list[client_h]->m_item_list[i] != 0) && (m_game->m_client_list[client_h]->m_item_list[i]->get_item_effect_type() == ItemEffectType::AlterItemDrop)) {
					m_game->m_client_list[client_h]->m_alter_item_drop_index = i;
					if (m_game->m_client_list[client_h]->m_item_list[m_game->m_client_list[client_h]->m_alter_item_drop_index]->m_instance.cur_durability > 0) {
						m_game->m_client_list[client_h]->m_item_list[m_game->m_client_list[client_h]->m_alter_item_drop_index]->m_instance.cur_durability--;
						m_game->send_notify_msg(0, client_h, Notify::CurDurability, m_game->m_client_list[client_h]->m_alter_item_drop_index, m_game->m_client_list[client_h]->m_item_list[m_game->m_client_list[client_h]->m_alter_item_drop_index]->m_instance.cur_durability, 0, 0);
					}
					m_game->m_item_manager->drop_item_handler(client_h, m_game->m_client_list[client_h]->m_alter_item_drop_index, -1, m_game->m_client_list[client_h]->m_item_list[m_game->m_client_list[client_h]->m_alter_item_drop_index]->m_name);
					m_game->m_client_list[client_h]->m_alter_item_drop_index = -1;
					return;
				}
		}
	}

	for(int i = 1; i <= total; i++) {
		remain_item = 0;
		std::memset(item_index_list, 0, sizeof(item_index_list));

		for (j = 0; j < hb::shared::limits::MaxItems; j++)
			if (m_game->m_client_list[client_h]->m_item_list[j] != 0) {
				item_index_list[remain_item] = j;
				remain_item++;
			}

		if (remain_item == 0) return;
		item_index = item_index_list[m_game->dice(1, remain_item) - 1];

		if ((m_game->m_client_list[client_h]->m_item_list[item_index]->get_touch_effect_type() != TouchEffectType::None) &&
			(m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.touch_effect_value1 == m_game->m_client_list[client_h]->m_char_id_num1) &&
			(m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.touch_effect_value2 == m_game->m_client_list[client_h]->m_char_id_num2) &&
			(m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.touch_effect_value3 == m_game->m_client_list[client_h]->m_char_id_num3)) {
		}

		else if (
			(m_game->m_client_list[client_h]->m_item_list[item_index]->m_id_num >= 400) &&
			(m_game->m_client_list[client_h]->m_item_list[item_index]->m_id_num != 402) &&
			(m_game->m_client_list[client_h]->m_item_list[item_index]->m_id_num <= 428)) {
		}

		else if (((m_game->m_client_list[client_h]->m_item_list[item_index]->get_item_effect_type() == ItemEffectType::AttackSpecAbility) ||
			(m_game->m_client_list[client_h]->m_item_list[item_index]->get_item_effect_type() == ItemEffectType::DefenseSpecAbility)) &&
			(is_s_aattacked == false)) {
		}

		else if ((m_game->m_client_list[client_h]->m_is_lucky_effect) && (m_game->dice(1, 10) == 5)) {
			// 10%    .
		}

		else m_game->m_item_manager->drop_item_handler(client_h, item_index, -1, m_game->m_client_list[client_h]->m_item_list[item_index]->m_name);
	}
}

void LootManager::get_reward_money_handler(int client_h)
{
	int ret, erase_req, weight_left;
	uint32_t reward_gold_left;
	CItem* item;

	if (m_game->m_client_list[client_h] == 0) return;
	if (m_game->m_client_list[client_h]->m_is_init_complete == false) return;

	weight_left = m_game->calc_max_load(client_h) - m_game->calc_total_weight(client_h);

	if (weight_left <= 0) return;
	weight_left = weight_left / 2;
	if (weight_left <= 0) return;

	item = new CItem;
	m_game->m_item_manager->init_item_attr(item, hb::shared::item::ItemId::Gold);
	//item->m_instance.count = m_game->m_client_list[client_h]->m_reward_gold;

	// (weight_left / item->m_weight)     Gold.   .
	int gold_weight = m_game->m_item_manager->get_item_weight(item, 1);
	if (gold_weight <= 0) gold_weight = 1;
	uint32_t maxGold = static_cast<uint32_t>(weight_left / gold_weight);
	if (maxGold >= m_game->m_client_list[client_h]->m_reward_gold) {
		item->m_instance.count = m_game->m_client_list[client_h]->m_reward_gold;
		reward_gold_left = 0;
	}
	else {
		// (weight_left / item->m_weight) .
		item->m_instance.count = maxGold;
		reward_gold_left = m_game->m_client_list[client_h]->m_reward_gold - maxGold;
	}

	if (m_game->m_item_manager->add_client_item_list(client_h, item, &erase_req)) {

		m_game->m_client_list[client_h]->m_reward_gold = reward_gold_left;

		ret = m_game->m_item_manager->send_item_notify_msg(client_h, Notify::ItemObtained, item, 0);

		switch (ret) {
		case sock::Event::QueueFull:
		case sock::Event::SocketError:
		case sock::Event::CriticalError:
			m_game->delete_client(client_h, true, true);
			return;
		}

		m_game->send_notify_msg(0, client_h, Notify::RewardGold, 0, 0, 0, 0);
	}
	else {

	}
}
