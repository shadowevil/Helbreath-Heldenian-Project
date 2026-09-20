// QuestManager.cpp: Manages quest assignment, progress tracking, and completion.
// Extracted from CGame (Phase B3).

#include "QuestManager.h"
#include "Game.h"
#include "ItemManager.h"
#include "Quest.h"
#include "Item.h"
#include "Packet/SharedPackets.h"

using namespace hb::shared::net;
using namespace hb::server::config;
namespace squest = hb::server::quest;

QuestManager::QuestManager()
{
	for (int i = 0; i < MaxQuestType; i++)
		m_quest_config_list[i] = 0;
}

QuestManager::~QuestManager()
{
	cleanup_arrays();
}

void QuestManager::init_arrays()
{
	for (int i = 0; i < MaxQuestType; i++)
		m_quest_config_list[i] = 0;
}

void QuestManager::cleanup_arrays()
{
	for (int i = 0; i < MaxQuestType; i++)
		if (m_quest_config_list[i] != 0) delete m_quest_config_list[i];
}

void QuestManager::npc_talk_handler(int client_h, int who)
{
	char reward_name[hb::shared::limits::ItemNameLen], target_name[hb::shared::limits::NpcNameLen];
	int res_mode, quest_num, quest_type, reward_type, reward_amount, contribution, iX, iY, range, target_type, target_count;

	quest_num = 0;
	std::memset(target_name, 0, sizeof(target_name));
	if (m_game->m_client_list[client_h] == 0) return;
	switch (who) {
	case 1: break;
	case 2:	break;
	case 3:	break;
	case 4:
		quest_num = talk_to_npc_result_cityhall(client_h, &quest_type, &res_mode, &reward_type, &reward_amount, &contribution, target_name, &target_type, &target_count, &iX, &iY, &range);
		break;
	case 5: break;
	case 6:	break;
	case 32: break;
	case 21:
		quest_num = talk_to_npc_result_guard(client_h, &quest_type, &res_mode, &reward_type, &reward_amount, &contribution, target_name, &target_type, &target_count, &iX, &iY, &range);
		if (quest_num >= 1000) return;
		break;
	}

	std::memset(reward_name, 0, sizeof(reward_name));
	if (quest_num > 0) {
		if (reward_type > 1) {
			strcpy(reward_name, m_game->m_item_config_list[reward_type]->m_name);
		}
		else {
			switch (reward_type) {
			case -10: strcpy(reward_name, "���F-�"); break;
			}
		}

		m_game->m_client_list[client_h]->m_asked_quest = quest_num;
		m_game->m_client_list[client_h]->m_quest_reward_type = reward_type;
		m_game->m_client_list[client_h]->m_quest_reward_amount = reward_amount;

		m_game->send_notify_msg(0, client_h, Notify::NpcTalk, quest_type, res_mode, reward_amount, reward_name, contribution,
			target_type, target_count, iX, iY, range, target_name);
	}
	else {
		switch (quest_num) {
		case  0: m_game->send_notify_msg(0, client_h, Notify::NpcTalk, (who + 130), 0, 0, 0, 0); break;
		case -1:
		case -2:
		case -3:
		case -4: m_game->send_notify_msg(0, client_h, Notify::NpcTalk, abs(quest_num) + 100, 0, 0, 0, 0); break;
		case -5: break;
		}
	}
}

int QuestManager::talk_to_npc_result_cityhall(int client_h, int* quest_type, int* mode, int* reward_type, int* reward_amount, int* contribution, char* target_name, int* target_type, int* target_count, int* pX, int* pY, int* range)
{
	int quest, erase_req;
	CItem* item;
	uint32_t exp;

	if (m_game->m_client_list[client_h] == 0) return 0;

	if (m_game->m_client_list[client_h]->m_quest != 0) {
		if (m_quest_config_list[m_game->m_client_list[client_h]->m_quest] == 0) return -4;
		else if (m_quest_config_list[m_game->m_client_list[client_h]->m_quest]->m_from == 4) {
			if (m_game->m_client_list[client_h]->m_is_quest_completed) {
				if ((m_game->m_client_list[client_h]->m_quest_reward_type > 0) &&
					(m_game->m_item_config_list[m_game->m_client_list[client_h]->m_quest_reward_type] != 0)) {
					item = new CItem;
					m_game->m_item_manager->init_item_attr(item, m_game->m_item_config_list[m_game->m_client_list[client_h]->m_quest_reward_type]->m_name);
					item->m_instance.count = m_game->m_client_list[client_h]->m_quest_reward_amount;
					if (m_game->m_item_manager->check_item_receive_condition(client_h, item)) {
						m_game->m_item_manager->add_client_item_list(client_h, item, &erase_req);
						m_game->m_item_manager->send_item_notify_msg(client_h, Notify::ItemObtained, item, 0);
						if (erase_req == 1) delete item;

						m_game->m_client_list[client_h]->m_contribution += m_quest_config_list[m_game->m_client_list[client_h]->m_quest]->m_contribution;

						m_game->send_notify_msg(0, client_h, Notify::QuestReward, 4, 1, m_game->m_client_list[client_h]->m_quest_reward_amount,
							m_game->m_item_config_list[m_game->m_client_list[client_h]->m_quest_reward_type]->m_name, m_game->m_client_list[client_h]->m_contribution);

						clear_quest_status(client_h);
						return -5;
					}
					else {
						delete item;
						m_game->m_item_manager->send_item_notify_msg(client_h, Notify::CannotCarryMoreItem, 0, 0);

						m_game->send_notify_msg(0, client_h, Notify::QuestReward, 4, 0, m_game->m_client_list[client_h]->m_quest_reward_amount,
							m_game->m_item_config_list[m_game->m_client_list[client_h]->m_quest_reward_type]->m_name, m_game->m_client_list[client_h]->m_contribution);

						return -5;
					}
				}
				else if (m_game->m_client_list[client_h]->m_quest_reward_type == -1) {
					m_game->m_client_list[client_h]->m_exp_stock += m_game->m_client_list[client_h]->m_quest_reward_amount;
					m_game->m_client_list[client_h]->m_contribution += m_quest_config_list[m_game->m_client_list[client_h]->m_quest]->m_contribution;

					m_game->send_notify_msg(0, client_h, Notify::QuestReward, 4, 1, m_game->m_client_list[client_h]->m_quest_reward_amount,
						"°æÇèÄ¡              ", m_game->m_client_list[client_h]->m_contribution);

					clear_quest_status(client_h);
					return -5;
				}
				else if (m_game->m_client_list[client_h]->m_quest_reward_type == -2) {
					exp = m_game->dice(1, (10 * m_game->m_client_list[client_h]->m_level));
					exp = exp * m_game->m_client_list[client_h]->m_quest_reward_amount;

					m_game->m_client_list[client_h]->m_exp_stock += exp;
					m_game->m_client_list[client_h]->m_contribution += m_quest_config_list[m_game->m_client_list[client_h]->m_quest]->m_contribution;

					m_game->send_notify_msg(0, client_h, Notify::QuestReward, 4, 1, exp,
						"°æÇèÄ¡              ", m_game->m_client_list[client_h]->m_contribution);

					clear_quest_status(client_h);
					return -5;
				}
				else {
					m_game->m_client_list[client_h]->m_contribution += m_quest_config_list[m_game->m_client_list[client_h]->m_quest]->m_contribution;

					m_game->send_notify_msg(0, client_h, Notify::QuestReward, 4, 1, 0,
						"                     ", m_game->m_client_list[client_h]->m_contribution);

					clear_quest_status(client_h);
					return -5;
				}
			}
			else return -1;
		}

		return -4;
	}

	if (memcmp(m_game->m_client_list[client_h]->m_location, m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->m_location_name, 10) == 0) {
		if (m_game->m_client_list[client_h]->m_player_kill_count > 0) return -3;

		quest = search_for_quest(client_h, 4, quest_type, mode, reward_type, reward_amount, contribution, target_name, target_type, target_count, pX, pY, range);
		if (quest <= 0) return -4;

		return quest;
	}
	else return -2;

	return -4;
}


int QuestManager::talk_to_npc_result_guard(int client_h, int* quest_type, int* mode, int* reward_type, int* reward_amount, int* contribution, char* target_name, int* target_type, int* target_count, int* pX, int* pY, int* range)
{
	if (m_game->m_client_list[client_h] == 0) return 0;

	if (memcmp(m_game->m_client_list[client_h]->m_location, "are", 3) == 0) {
		if (memcmp(m_game->m_client_list[client_h]->m_map_name, "aresden", 7) == 0) {
			m_game->send_notify_msg(0, client_h, Notify::NpcTalk, (200), 0, 0, 0, 0);
			return 1000;
		}
		else
			if (memcmp(m_game->m_client_list[client_h]->m_map_name, "elv", 3) == 0) {
				m_game->send_notify_msg(0, client_h, Notify::NpcTalk, (201), 0, 0, 0, 0);
				return 1001;
			}
	}
	else
		if (memcmp(m_game->m_client_list[client_h]->m_location, "elv", 3) == 0) {
			if (memcmp(m_game->m_client_list[client_h]->m_map_name, "aresden", 7) == 0) {
				m_game->send_notify_msg(0, client_h, Notify::NpcTalk, (202), 0, 0, 0, 0);
				return 1002;
			}
			else
				if (memcmp(m_game->m_client_list[client_h]->m_map_name, "elv", 3) == 0) {
					m_game->send_notify_msg(0, client_h, Notify::NpcTalk, (203), 0, 0, 0, 0);
					return 1003;
				}
		}
		else
			if (memcmp(m_game->m_client_list[client_h]->m_location, "NONE", 4) == 0) {
				if (memcmp(m_game->m_client_list[client_h]->m_map_name, "aresden", 7) == 0) {
					m_game->send_notify_msg(0, client_h, Notify::NpcTalk, (204), 0, 0, 0, 0);
					return 1004;
				}
				else
					if (memcmp(m_game->m_client_list[client_h]->m_map_name, "elvine", 6) == 0) {
						m_game->send_notify_msg(0, client_h, Notify::NpcTalk, (205), 0, 0, 0, 0);
						return 1005;
					}
					else
						if (memcmp(m_game->m_client_list[client_h]->m_map_name, "default", 7) == 0) {
							m_game->send_notify_msg(0, client_h, Notify::NpcTalk, (206), 0, 0, 0, 0);
							return 1006;
						}
			}

	return 0;
}


int QuestManager::search_for_quest(int client_h, int who, int* quest_type, int* mode, int* reward_type, int* reward_amount, int* contribution, char* target_name, int* target_type, int* target_count, int* pX, int* pY, int* range)
{
	int quest_list[MaxQuestType], index, quest, reward, quest_index;

	if (m_game->m_client_list[client_h] == 0) return -1;

	index = 0;
	for(int i = 0; i < MaxQuestType; i++)
		quest_list[i] = -1;

	for(int i = 1; i < MaxQuestType; i++)
		if (m_quest_config_list[i] != 0) {

			if (m_quest_config_list[i]->m_from != who) goto SFQ_SKIP;
			if (m_quest_config_list[i]->m_side != m_game->m_client_list[client_h]->m_side) goto SFQ_SKIP;
			if (m_quest_config_list[i]->m_min_level > m_game->m_client_list[client_h]->m_level) goto SFQ_SKIP;
			if (m_quest_config_list[i]->m_max_level < m_game->m_client_list[client_h]->m_level) goto SFQ_SKIP;
			if (m_quest_config_list[i]->m_req_contribution > m_game->m_client_list[client_h]->m_contribution) goto SFQ_SKIP;

			if (m_quest_config_list[i]->m_required_skill_num != -1) {
				if (m_game->m_client_list[client_h]->m_skill_mastery[m_quest_config_list[i]->m_required_skill_num] <
					m_quest_config_list[i]->m_required_skill_level) goto SFQ_SKIP;
			}

			if ((m_game->m_is_crusade_mode) && (m_quest_config_list[i]->m_assign_type != 1)) goto SFQ_SKIP;
			if ((m_game->m_is_crusade_mode == false) && (m_quest_config_list[i]->m_assign_type == 1)) goto SFQ_SKIP;

			if (m_quest_config_list[i]->m_contribution_limit < m_game->m_client_list[client_h]->m_contribution) goto SFQ_SKIP;

			quest_list[index] = i;
			index++;

		SFQ_SKIP:;
		}

	// index     .    1 .
	if (index == 0) return -1;
	quest = (m_game->dice(1, index)) - 1;
	quest_index = quest_list[quest];
	reward = m_game->dice(1, 3);
	*mode = m_quest_config_list[quest_index]->m_response_mode;
	*reward_type = m_quest_config_list[quest_index]->m_reward_type[reward];
	*reward_amount = m_quest_config_list[quest_index]->m_reward_amount[reward];
	*contribution = m_quest_config_list[quest_index]->m_contribution;

	strcpy(target_name, m_quest_config_list[quest_index]->m_target_name);
	*pX = m_quest_config_list[quest_index]->m_x;
	*pY = m_quest_config_list[quest_index]->m_y;
	*range = m_quest_config_list[quest_index]->m_range;

	*target_type = m_quest_config_list[quest_index]->m_target_config_id;
	*target_count = m_quest_config_list[quest_index]->m_max_count;
	*quest_type = m_quest_config_list[quest_index]->m_type;

	return quest_index;
}

// New 14/05/2004
void QuestManager::quest_accepted_handler(int client_h)
{
	int index;

	if (m_game->m_client_list[client_h] == 0) return;

	// Does the quest exist ??
	if (m_quest_config_list[m_game->m_client_list[client_h]->m_asked_quest] == 0) return;

	if (m_quest_config_list[m_game->m_client_list[client_h]->m_asked_quest]->m_assign_type == 1) {
		switch (m_quest_config_list[m_game->m_client_list[client_h]->m_asked_quest]->m_type) {
		case 10:
			clear_quest_status(client_h);
			m_game->request_teleport_handler(client_h, "2   ", m_quest_config_list[m_game->m_client_list[client_h]->m_asked_quest]->m_target_name,
				m_quest_config_list[m_game->m_client_list[client_h]->m_asked_quest]->m_x, m_quest_config_list[m_game->m_client_list[client_h]->m_asked_quest]->m_y);
			return;
		}
	}

	m_game->m_client_list[client_h]->m_quest = m_game->m_client_list[client_h]->m_asked_quest;
	index = m_game->m_client_list[client_h]->m_quest;
	m_game->m_client_list[client_h]->m_quest_id = m_quest_config_list[index]->m_quest_id;
	m_game->m_client_list[client_h]->m_cur_quest_count = 0;
	m_game->m_client_list[client_h]->m_is_quest_completed = false;

	check_quest_environment(client_h);
	send_quest_contents(client_h);
}


void QuestManager::send_quest_contents(int client_h)
{
	int who, index, quest_type, contribution, target_type, target_count, iX, iY, range, quest_completed;
	char target_name[hb::shared::limits::NpcNameLen];

	if (m_game->m_client_list[client_h] == 0) return;

	index = m_game->m_client_list[client_h]->m_quest;
	if (index == 0) {
		// Quest .
		m_game->send_notify_msg(0, client_h, Notify::QuestContents, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0);
	}
	else {
		// Quest  .
		who = m_quest_config_list[index]->m_from;
		quest_type = m_quest_config_list[index]->m_type;
		contribution = m_quest_config_list[index]->m_contribution;
		target_type = m_quest_config_list[index]->m_target_config_id;
		target_count = m_quest_config_list[index]->m_max_count;
		iX = m_quest_config_list[index]->m_x;
		iY = m_quest_config_list[index]->m_y;
		range = m_quest_config_list[index]->m_range;
		std::memset(target_name, 0, sizeof(target_name));
		memcpy(target_name, m_quest_config_list[index]->m_target_name, hb::shared::limits::NpcNameLen - 1);
		quest_completed = (int)m_game->m_client_list[client_h]->m_is_quest_completed;

		m_game->send_notify_msg(0, client_h, Notify::QuestContents, who, quest_type, contribution, 0,
			target_type, target_count, iX, iY, range, quest_completed, target_name);
	}
}

void QuestManager::check_quest_environment(int client_h)
{
	int index;
	char target_name[hb::shared::limits::NpcNameLen];

	if (m_game->m_client_list[client_h] == 0) return;

	index = m_game->m_client_list[client_h]->m_quest;
	if (index == 0) return;
	if (m_quest_config_list[index] == 0) return;

	if (index >= 35 && index <= 40) {
		m_game->m_client_list[client_h]->m_quest = 0;
		m_game->m_client_list[client_h]->m_quest_id = 0;
		m_game->m_client_list[client_h]->m_quest_reward_amount = 0;
		m_game->m_client_list[client_h]->m_quest_reward_type = 0;
		m_game->send_notify_msg(0, client_h, Notify::QuestAborted, 0, 0, 0, 0);
		return;
	}

	if (m_quest_config_list[index]->m_quest_id != m_game->m_client_list[client_h]->m_quest_id) {
		m_game->m_client_list[client_h]->m_quest = 0;
		m_game->m_client_list[client_h]->m_quest_id = 0;
		m_game->m_client_list[client_h]->m_quest_reward_amount = 0;
		m_game->m_client_list[client_h]->m_quest_reward_type = 0;

		m_game->send_notify_msg(0, client_h, Notify::QuestAborted, 0, 0, 0, 0);
		return;
	}

	switch (m_quest_config_list[index]->m_type) {
	case squest::Type::MonsterHunt:
	case squest::Type::GoPlace:
		std::memset(target_name, 0, sizeof(target_name));
		memcpy(target_name, m_quest_config_list[index]->m_target_name, hb::shared::limits::NpcNameLen - 1);
		if (memcmp(m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->m_name, target_name, 10) == 0)
			m_game->m_client_list[client_h]->m_quest_match_flag_loc = true;
		else m_game->m_client_list[client_h]->m_quest_match_flag_loc = false;
		break;
	}

}

bool QuestManager::check_is_quest_completed(int client_h)
{
	int quest_index;

	if (m_game->m_client_list[client_h] == 0) return false;
	if (m_game->m_client_list[client_h]->m_is_quest_completed) return false;
	quest_index = m_game->m_client_list[client_h]->m_quest;
	if (quest_index == 0) return false;

	if (m_quest_config_list[quest_index] != 0) {
		switch (m_quest_config_list[quest_index]->m_type) {
		case squest::Type::MonsterHunt:
			if ((m_game->m_client_list[client_h]->m_quest_match_flag_loc) &&
				(m_game->m_client_list[client_h]->m_cur_quest_count >= m_quest_config_list[quest_index]->m_max_count)) {
				m_game->m_client_list[client_h]->m_is_quest_completed = true;
				m_game->send_notify_msg(0, client_h, Notify::QuestCompleted, 0, 0, 0, 0);
				return true;
			}
			break;

		case squest::Type::GoPlace:
			if ((m_game->m_client_list[client_h]->m_quest_match_flag_loc) &&
				(m_game->m_client_list[client_h]->m_x >= m_quest_config_list[quest_index]->m_x - m_quest_config_list[quest_index]->m_range) &&
				(m_game->m_client_list[client_h]->m_x <= m_quest_config_list[quest_index]->m_x + m_quest_config_list[quest_index]->m_range) &&
				(m_game->m_client_list[client_h]->m_y >= m_quest_config_list[quest_index]->m_y - m_quest_config_list[quest_index]->m_range) &&
				(m_game->m_client_list[client_h]->m_y <= m_quest_config_list[quest_index]->m_y + m_quest_config_list[quest_index]->m_range)) {
				m_game->m_client_list[client_h]->m_is_quest_completed = true;
				m_game->send_notify_msg(0, client_h, Notify::QuestCompleted, 0, 0, 0, 0);
				return true;
			}
			break;
		}
	}

	return false;
}

void QuestManager::clear_quest_status(int client_h)
{
	if (m_game->m_client_list[client_h] == 0) return;

	m_game->m_client_list[client_h]->m_quest = 0;
	m_game->m_client_list[client_h]->m_quest_id = 0;
	m_game->m_client_list[client_h]->m_quest_reward_type = 0;
	m_game->m_client_list[client_h]->m_quest_reward_amount = 0;
	m_game->m_client_list[client_h]->m_is_quest_completed = false;
}

void QuestManager::cancel_quest_handler(int client_h)
{
	if (m_game->m_client_list[client_h] == 0) return;

	clear_quest_status(client_h);
	m_game->send_notify_msg(0, client_h, Notify::QuestAborted, 0, 0, 0, 0);
}
