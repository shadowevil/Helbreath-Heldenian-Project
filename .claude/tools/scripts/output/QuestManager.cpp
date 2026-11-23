// quest_manager.cpp: Handles client-side quest network messages.
// Extracted from NetworkMessages_Quest.cpp (Phase B3).

#include "QuestManager.h"
#include "Game.h"
#include "Packet/SharedPackets.h"
#include "lan_eng.h"
#include "DialogBoxIDs.h"
#include "DialogBox_NpcTalk.h"
#include <cstdio>
#include <cstring>
#include <format>
#include <string>
#include "Screen_OnGame.h"

void quest_manager::handle_quest_counter(char* data)
{
	const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyQuestCounter>(
		data, sizeof(hb::net::PacketNotifyQuestCounter));
	if (!pkt) return;
	m_game->on_game()->m_quest.current_count = static_cast<short>(pkt->current_count);
}

void quest_manager::handle_quest_contents(char* data)
{
	const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyQuestContents>(
		data, sizeof(hb::net::PacketNotifyQuestContents));
	if (!pkt) return;
	m_game->on_game()->m_quest.who = pkt->who;
	m_game->on_game()->m_quest.quest_type = pkt->quest_type;
	m_game->on_game()->m_quest.contribution = pkt->contribution;
	m_game->on_game()->m_quest.target_type = pkt->target_config_id;
	m_game->on_game()->m_quest.target_count = pkt->target_count;
	m_game->on_game()->m_quest.x = pkt->x;
	m_game->on_game()->m_quest.y = pkt->y;
	m_game->on_game()->m_quest.range = pkt->range;
	m_game->on_game()->m_quest.is_quest_completed = (pkt->is_completed != 0);
	m_game->on_game()->m_quest.target_name.assign(pkt->target_name, strnlen(pkt->target_name, 20));
}

void quest_manager::handle_quest_reward(char* data)
{
	short who, flag;
	std::string txt;

	char reward_name[hb::shared::limits::ItemNameLen]{};
	int amount, index, pre_con;
	const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyQuestReward>(
		data, sizeof(hb::net::PacketNotifyQuestReward));
	if (!pkt) return;
	who = pkt->who;
	flag = pkt->flag;
	amount = pkt->amount;
	memcpy(reward_name, pkt->reward_name, sizeof(pkt->reward_name));
	pre_con = m_game->m_player->m_contribution;
	m_game->m_player->m_contribution = pkt->contribution;

	if (flag == 1)
	{
		m_game->on_game()->m_quest.who = 0;
		m_game->on_game()->m_quest.quest_type = 0;
		m_game->on_game()->m_quest.contribution = 0;
		m_game->on_game()->m_quest.target_type = 0;
		m_game->on_game()->m_quest.target_count = 0;
		m_game->on_game()->m_quest.x = 0;
		m_game->on_game()->m_quest.y = 0;
		m_game->on_game()->m_quest.range = 0;
		m_game->on_game()->m_quest.current_count = 0;
		m_game->on_game()->m_quest.is_quest_completed = false;
		m_game->get_dialog_box_manager().enable_dialog_box(DialogBoxId::NpcTalk, 0, who + 110, 0);
		index = m_game->get_dialog_box_manager().get_dialog_as<DialogBox_NpcTalk>(DialogBoxId::NpcTalk)->m_text_line_count;
		if (index < 0 || index + 3 >= game_limits::max_text_dlg_lines) return;
		m_game->on_game()->m_msg_text_list2[index] = std::make_unique<CMsg>(0, "  ", 0);
		index++;
		// Gold reward sentinel — raw bytes from EUC-KR source (encoding was corrupted during UTF-8 conversion)
		if (memcmp(reward_name, "\xC4\xA1", 2) == 0)
		{
			if (amount > 0) txt = std::format(NOTIFYMSG_QUEST_REWARD1, amount);
		}
		else
		{
			txt = std::format(NOTIFYMSG_QUEST_REWARD2, amount, reward_name);
		}
		m_game->on_game()->m_msg_text_list2[index] = std::make_unique<CMsg>(0, txt.empty() ? "  " : txt.c_str(), 0);
		index++;
		m_game->on_game()->m_msg_text_list2[index] = std::make_unique<CMsg>(0, "  ", 0);
		index++;
		if (pre_con < m_game->m_player->m_contribution)
			txt = std::format(NOTIFYMSG_QUEST_REWARD3, m_game->m_player->m_contribution - pre_con);
		else txt = std::format(NOTIFYMSG_QUEST_REWARD4, pre_con - m_game->m_player->m_contribution);

		m_game->on_game()->m_msg_text_list2[index] = std::make_unique<CMsg>(0, "  ", 0);
		index++;
	}
	else m_game->get_dialog_box_manager().enable_dialog_box(DialogBoxId::NpcTalk, 0, who + 120, 0);
}

void quest_manager::handle_quest_completed(char* data)
{
	m_game->on_game()->m_quest.is_quest_completed = true;
	m_game->get_dialog_box_manager().disable_dialog_box(DialogBoxId::Quest);
	m_game->get_dialog_box_manager().enable_dialog_box(DialogBoxId::Quest, 1, 0, 0);
	switch (m_game->m_player->m_player_type) {
	case 1:
	case 2:
	case 3:
		m_game->play_game_sound('C', 21, 0);
		break;
	case 4:
	case 5:
	case 6:
		m_game->play_game_sound('C', 22, 0);
		break;
	}
	m_game->play_game_sound('E', 23, 0);
	m_game->add_event_list(NOTIFY_MSG_HANDLER44, 10);
}

void quest_manager::handle_quest_aborted(char* data)
{
	m_game->on_game()->m_quest.quest_type = 0;
	m_game->get_dialog_box_manager().disable_dialog_box(DialogBoxId::Quest);
	m_game->get_dialog_box_manager().enable_dialog_box(DialogBoxId::Quest, 2, 0, 0);
}
