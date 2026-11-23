#include "Game.h"
#include "TeleportManager.h"
#include "NetworkMessageManager.h"
#include "Packet/SharedPackets.h"
#include "lan_eng.h"
#include "DialogBoxIDs.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <format>
#include <string>
#include "Screen_OnGame.h"

namespace NetworkMessageHandlers {
	void HandleCrusade(CGame* game, char* data)
	{
		int v1, v2, v3, v4;
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyCrusade>(
			data, sizeof(hb::net::PacketNotifyCrusade));
		if (!pkt) return;
		v1 = pkt->crusade_mode;
		v2 = pkt->crusade_duty;
		v3 = pkt->v3;
		v4 = pkt->v4;

		if (game->on_game()->m_is_crusade_mode == false)
		{
			if (v1 != 0) // begin crusade
			{
				game->on_game()->m_is_crusade_mode = true;
				game->m_player->m_crusade_duty = v2;
				if ((game->m_player->m_crusade_duty != 3) && (game->m_player->m_citizen == true))
					game->request_map_status("middleland", 3);
				if (game->m_player->m_crusade_duty != 0)
					game->get_dialog_box_manager().enable_dialog_box(DialogBoxId::CrusadeJob, 2, v2, 0);
				else game->get_dialog_box_manager().enable_dialog_box(DialogBoxId::CrusadeJob, 1, 0, 0);
				
				if (game->m_player->m_citizen == false) game->get_dialog_box_manager().enable_dialog_box(DialogBoxId::Text, LOGICAL_WIDTH(), 0, 0);
				else if (game->m_player->m_aresden == true) game->get_dialog_box_manager().enable_dialog_box(DialogBoxId::Text, 801, 0, 0);
				else if (game->m_player->m_aresden == false) game->get_dialog_box_manager().enable_dialog_box(DialogBoxId::Text, 802, 0, 0);
				
				if (game->m_player->m_citizen == false) game->set_top_msg(NOTIFY_MSG_CRUSADESTART_NONE, 10);
				else if (game->m_game_msg_list[9]) game->set_top_msg(game->m_game_msg_list[9]->m_pMsg, 10);
				game->play_game_sound('E', 25, 0, 0);
			}
			if (v3 != 0) // Crusade finished, show XP result screen
			{
				game->crusade_contribution_result(v3);
			}
			if (v4 == -1) // The crusade you played in was finished.
			{
				game->crusade_contribution_result(0); 
			}
		}
		else
		{
			if (v1 == 0) // crusade finished show result (1st result: winner)
			{
				game->on_game()->m_is_crusade_mode = false;
				game->m_player->m_crusade_duty = 0;
				game->crusade_war_result(v4);
				if (game->m_game_msg_list[57]) game->set_top_msg(game->m_game_msg_list[57]->m_pMsg, 8);
			}
			else
			{
				if (game->m_player->m_crusade_duty != v2)
				{
					game->m_player->m_crusade_duty = v2;
					game->get_dialog_box_manager().enable_dialog_box(DialogBoxId::CrusadeJob, 2, v2, 0);
					game->play_game_sound('E', 25, 0, 0);
				}
			}
			if (v4 == -1)
			{
				game->crusade_contribution_result(0); 
			}
		}
	}

	void HandleGrandMagicResult(CGame* game, char* data)
	{
		char txt[120]{};
		int v1, v2, v3, v4, v5, v6, v7, v8, v9;
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyGrandMagicResult>(
			data, sizeof(hb::net::PacketNotifyGrandMagicResult));
		if (!pkt) return;
		v1 = pkt->crashed_structures;
		v2 = pkt->structure_damage;
		v3 = pkt->casualities;
		memcpy(txt, pkt->map_name, sizeof(pkt->map_name));
		v4 = pkt->active_structure;
		v5 = pkt->value_count;
		v6 = v7 = v8 = v9 = 0;
		if (v5 > 0) v6 = pkt->values[0];
		if (v5 > 1) v7 = pkt->values[1];
		if (v5 > 2) v8 = pkt->values[2];
		if (v5 > 3) v9 = pkt->values[3];

		game->grand_magic_result(txt, v1, v2, v3, v4, v6, v7, v8, v9);
	}

	void HandleMeteorStrikeComing(CGame* game, char* data)
	{
		int v1;
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyMeteorStrikeComing>(
			data, sizeof(hb::net::PacketNotifyMeteorStrikeComing));
		if (!pkt) return;
		v1 = pkt->phase;
		game->meteor_strike_coming(v1);
		game->play_game_sound('E', 25, 0, 0);
	}

	void HandleMeteorStrikeHit(CGame* game, char* data)
	{
		int i;
		if (game->m_game_msg_list[17]) game->set_top_msg(game->m_game_msg_list[17]->m_pMsg, 5);
		for (i = 0; i < 36; i++)
			game->m_effect_manager->add_effect(EffectType::METEOR_FLYING, game->m_Camera.get_x() + (rand() % LOGICAL_MAX_X()), game->m_Camera.get_y() + (rand() % LOGICAL_MAX_Y()), 0, 0, -(rand() % 80));
	}

	void HandleCannotConstruct(CGame* game, char* data)
	{
		short v1;
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyCannotConstruct>(
			data, sizeof(hb::net::PacketNotifyCannotConstruct));
		if (!pkt) return;
		v1 = static_cast<short>(pkt->reason);
		game->cannot_construct(v1);
		game->play_game_sound('E', 25, 0, 0);
	}

	void HandleTCLoc(CGame* game, char* data)
	{
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyTCLoc>(
			data, sizeof(hb::net::PacketNotifyTCLoc));
		if (!pkt) return;
		teleport_manager::get().set_location(pkt->dest_x, pkt->dest_y);
		teleport_manager::get().set_map_name(pkt->teleport_map, sizeof(pkt->teleport_map));
		game->m_player->m_construct_loc_x = pkt->construct_x;
		game->m_player->m_construct_loc_y = pkt->construct_y;
		game->m_construct_map_name.assign(pkt->construct_map, strnlen(pkt->construct_map, sizeof(pkt->construct_map)));
	}

	void HandleConstructionPoint(CGame* game, char* data)
	{
		short v1, v2, v3;
		std::string txt;
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyConstructionPoint>(
			data, sizeof(hb::net::PacketNotifyConstructionPoint));
		if (!pkt) return;
		v1 = static_cast<short>(pkt->construction_point);
		v2 = static_cast<short>(pkt->war_contribution);
		v3 = static_cast<short>(pkt->notify_type);

		if (v3 == 0) {
			if ((v1 > game->m_player->m_construction_point) && (v2 > game->m_player->m_war_contribution)) {
				if (game->m_game_msg_list[13] && game->m_game_msg_list[21]) {
					txt = std::format("{} +{}, {} +{}", game->m_game_msg_list[13]->m_pMsg, (v1 - game->m_player->m_construction_point), game->m_game_msg_list[21]->m_pMsg, (v2 - game->m_player->m_war_contribution));
					game->set_top_msg(txt.c_str(), 5);
				}
				game->play_game_sound('E', 23, 0, 0);
			}

			if ((v1 > game->m_player->m_construction_point) && (v2 == game->m_player->m_war_contribution)) {
				if (game->m_player->m_crusade_duty == 3) {
					if (game->m_game_msg_list[13]) {
						txt = std::format("{} +{}", game->m_game_msg_list[13]->m_pMsg, v1 - game->m_player->m_construction_point);
						game->set_top_msg(txt.c_str(), 5);
					}
					game->play_game_sound('E', 23, 0, 0);
				}
			}

			if ((v1 == game->m_player->m_construction_point) && (v2 > game->m_player->m_war_contribution)) {
				if (game->m_game_msg_list[21]) {
					txt = std::format("{} +{}", game->m_game_msg_list[21]->m_pMsg, v2 - game->m_player->m_war_contribution);
					game->set_top_msg(txt.c_str(), 5);
				}
				game->play_game_sound('E', 23, 0, 0);
			}

			if (v1 < game->m_player->m_construction_point) {
				if (game->m_player->m_crusade_duty == 3) {
					if (game->m_game_msg_list[13]) {
						txt = std::format("{} -{}", game->m_game_msg_list[13]->m_pMsg, game->m_player->m_construction_point - v1);
						game->set_top_msg(txt.c_str(), 5);
					}
					game->play_game_sound('E', 25, 0, 0);
				}
			}

			if (v2 < game->m_player->m_war_contribution) {
				if (game->m_game_msg_list[21]) {
					txt = std::format("{} -{}", game->m_game_msg_list[21]->m_pMsg, game->m_player->m_war_contribution - v2);
					game->set_top_msg(txt.c_str(), 5);
				}
				game->play_game_sound('E', 24, 0, 0);
			}
		}

		game->m_player->m_construction_point = v1;
		game->m_player->m_war_contribution = v2;
	}

	void HandleNoMoreCrusadeStructure(CGame* game, char* data)
	{
		if (game->m_game_msg_list[12]) game->set_top_msg(game->m_game_msg_list[12]->m_pMsg, 5);
		game->play_game_sound('E', 25, 0, 0);
	}

	void HandleEnergySphereGoalIn(CGame* game, char* data)
	{
		int v1, v2, v3;
		char name[120]{};
		std::string txt;
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyEnergySphereGoalIn>(
			data, sizeof(hb::net::PacketNotifyEnergySphereGoalIn));
		if (!pkt) return;
		v1 = pkt->result;
		v2 = pkt->side;
		v3 = pkt->goal;
		memcpy(name, pkt->name, sizeof(pkt->name));

		if (v2 == v3)
		{
			game->play_game_sound('E', 24, 0);
			if (strcmp(name, game->m_player->m_player_name.c_str()) == 0)
			{
				game->add_event_list(NOTIFY_MSG_HANDLER33, 10);
				game->m_player->m_contribution += v1;
				game->m_contribution_price = 0;
				if (game->m_player->m_contribution < 0) game->m_player->m_contribution = 0;
			}
			else {
				if (game->m_player->m_aresden == true) txt = std::format(NOTIFY_MSG_HANDLER34, name);
				else if (game->m_player->m_aresden == false) txt = std::format(NOTIFY_MSG_HANDLER34_ELV, name);
				if (!txt.empty()) game->add_event_list(txt.c_str(), 10);
			}
		}
		else
		{
			game->play_game_sound('E', 23, 0);
			if (strcmp(name, game->m_player->m_player_name.c_str()) == 0)
			{
				switch (game->m_player->m_player_type) {
				case 1:
				case 2:
				case 3:
					game->play_game_sound('C', 21, 0);
					break;
				case 4:
				case 5:
				case 6:
					game->play_game_sound('C', 22, 0);
					break;
				}
				game->add_event_list(NOTIFY_MSG_HANDLER35, 10);
				game->m_player->m_contribution += 5;
				if (game->m_player->m_contribution < 0) game->m_player->m_contribution = 0;
			}
			else
			{
				if (v3 == 1)
				{
					txt = std::format(NOTIFY_MSG_HANDLER36, name);
					game->add_event_list(txt.c_str(), 10);
				}
				else if (v3 == 2)
				{
					txt = std::format(NOTIFY_MSG_HANDLER37, name);
					game->add_event_list(txt.c_str(), 10);
				}
			}
		}
	}

	void HandleEnergySphereCreated(CGame* game, char* data)
	{
		int v1, v2;
		std::string txt;
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyEnergySphereCreated>(
			data, sizeof(hb::net::PacketNotifyEnergySphereCreated));
		if (!pkt) return;
		v1 = pkt->x;
		v2 = pkt->y;
		txt = std::format(NOTIFY_MSG_HANDLER38, v1, v2);
		game->add_event_list(txt.c_str(), 10);
		game->add_event_list(NOTIFY_MSG_HANDLER39, 10);
	}
}
