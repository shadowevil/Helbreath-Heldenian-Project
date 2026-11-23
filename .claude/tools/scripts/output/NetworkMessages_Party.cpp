#include "Game.h"
#include "DialogBox_Party.h"
#include "NetworkMessageManager.h"
#include "Packet/SharedPackets.h"
#include "PacketSendHelpers.h"

#include "lan_eng.h"
#include "DialogBoxIDs.h"
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <format>
#include <string>
#include "Screen_OnGame.h"

using namespace hb::shared::net;
namespace NetworkMessageHandlers {

void HandleParty(CGame* game, char* data)
{
	int v1, v2, v3, v4;
	char txt[120];

	const auto* basic = hb::net::PacketCast<hb::net::PacketNotifyPartyBasic>(
		data, sizeof(hb::net::PacketNotifyPartyBasic));

	// If it's a basic packet, extract values. If not, v1 will be 0 and handled or overwritten later.
	// But mostly this handler handles multiple packet types by casting differently based on type.
	// We should be careful. The original code unconditionally casts to PartyBasic first.
	// PacketNotifyPartyBasic is: header, type(16), v2(16), v3(16), v4(16).
	// This seems to be a common header for party messages.

	if (basic) {
		v1 = basic->type;
		v2 = basic->v2;
		v3 = basic->v3;
		v4 = basic->v4;
	} else {
		// Should not happen if dispatch logic is correct and packet size is sufficient
		return;
	}

	switch (v1) {
	case 1: //
		switch (v2) {
		case 0:
			game->get_dialog_box_manager().enable_dialog_box(DialogBoxId::Party, 0, 0, 0);
			game->get_dialog_box_manager().get_dialog_as<DialogBox_Party>(DialogBoxId::Party)->m_mode = DialogBox_Party::mode::already_in_party;
			break;

		case 1:
			game->on_game()->m_party_status = 1;
			game->on_game()->m_total_party_member = 0;
			game->get_dialog_box_manager().enable_dialog_box(DialogBoxId::Party, 0, 0, 0);
			game->get_dialog_box_manager().get_dialog_as<DialogBox_Party>(DialogBoxId::Party)->m_mode = DialogBox_Party::mode::failed;
			{
				auto pkt = hb::net::make_common_command_str(CommonType::RequestJoinParty, game->m_player->m_player_x, game->m_player->m_player_y);
				pkt.v1 = 2;
				std::snprintf(pkt.text, sizeof(pkt.text), "%s", game->m_mc_name.c_str());
				game->send_game_packet(pkt);
			}
			break;
		}
		break;

	case 2: //
		game->on_game()->m_party_status = 0;
		game->on_game()->m_total_party_member = 0;
		game->get_dialog_box_manager().enable_dialog_box(DialogBoxId::Party, 0, 0, 0);
		game->get_dialog_box_manager().get_dialog_as<DialogBox_Party>(DialogBoxId::Party)->m_mode = DialogBox_Party::mode::disbanded;
		break;

	case 4:
	{
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyPartyName>(
			data, sizeof(hb::net::PacketNotifyPartyName));
		if (!pkt) return;
		std::memset(txt, 0, sizeof(txt));
		memcpy(txt, pkt->name, sizeof(pkt->name));

		switch (v2) {
		case 0: //
			game->get_dialog_box_manager().enable_dialog_box(DialogBoxId::Party, 0, 0, 0);
			game->get_dialog_box_manager().get_dialog_as<DialogBox_Party>(DialogBoxId::Party)->m_mode = DialogBox_Party::mode::already_in_party;
			break;

		case 1: //
			if (strcmp(txt, game->m_player->m_player_name.c_str()) == 0) {
				game->on_game()->m_party_status = 2;
				game->get_dialog_box_manager().enable_dialog_box(DialogBoxId::Party, 0, 0, 0);
				game->get_dialog_box_manager().get_dialog_as<DialogBox_Party>(DialogBoxId::Party)->m_mode = DialogBox_Party::mode::failed;
			}
			else {
				std::string partyMsgBuf;
				partyMsgBuf = std::format(NOTIFY_MSG_HANDLER1, txt);
				game->add_event_list(partyMsgBuf.c_str(), 10);
			}

			game->on_game()->m_total_party_member++;
			game->get_dialog_box_manager().get_dialog_as<DialogBox_Party>(DialogBoxId::Party)->add_member_name(txt);
			break;

		case 2: //
			break;
		}
	}
	break;

	case 5: //
		game->on_game()->m_total_party_member = 0;

		{
			const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyPartyList>(
				data, sizeof(hb::net::PacketNotifyPartyList));
			if (!pkt) return;
			const char* names = pkt->names;
			game->on_game()->m_total_party_member = (std::min)(static_cast<int>(pkt->count), hb::shared::limits::MaxPartyMembers);
			game->get_dialog_box_manager().get_dialog_as<DialogBox_Party>(DialogBoxId::Party)->set_name_list(
				pkt->count, names, hb::shared::limits::CharNameLen);
		}
		break;

	case 6:
	{
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyPartyName>(
			data, sizeof(hb::net::PacketNotifyPartyName));
		if (!pkt) return;
		std::memset(txt, 0, sizeof(txt));
		memcpy(txt, pkt->name, sizeof(pkt->name));

		switch (v2) {
		case 0: //
			game->get_dialog_box_manager().enable_dialog_box(DialogBoxId::Party, 0, 0, 0);
			game->get_dialog_box_manager().get_dialog_as<DialogBox_Party>(DialogBoxId::Party)->m_mode = DialogBox_Party::mode::party_full;
			break;

		case 1: //
			if (strcmp(txt, game->m_player->m_player_name.c_str()) == 0) {
				game->on_game()->m_party_status = 0;
				game->get_dialog_box_manager().enable_dialog_box(DialogBoxId::Party, 0, 0, 0);
				game->get_dialog_box_manager().get_dialog_as<DialogBox_Party>(DialogBoxId::Party)->m_mode = DialogBox_Party::mode::withdrawn;
			}
			else {
				std::string partyMsgBuf;
				partyMsgBuf = std::format(NOTIFY_MSG_HANDLER2, txt);
				game->add_event_list(partyMsgBuf.c_str(), 10);
			}
			if (game->get_dialog_box_manager().get_dialog_as<DialogBox_Party>(DialogBoxId::Party)->remove_member_name(txt))
					game->on_game()->m_total_party_member--;
			break;
		}
	}
	break;

	case 7: //
		game->get_dialog_box_manager().enable_dialog_box(DialogBoxId::Party, 0, 0, 0);
		game->get_dialog_box_manager().get_dialog_as<DialogBox_Party>(DialogBoxId::Party)->m_mode = DialogBox_Party::mode::already_in_party;
		break;

	case 8: //
		game->on_game()->m_party_status = 0;
		game->on_game()->m_total_party_member = 0;
		break;
	}
}

void HandleQueryJoinParty(CGame* game, char* data)
{
	game->get_dialog_box_manager().enable_dialog_box(DialogBoxId::Party, 0, 0, 0);
	auto* party_dlg = game->get_dialog_box_manager().get_dialog_as<DialogBox_Party>(DialogBoxId::Party);
	if (!party_dlg) return;
	party_dlg->m_mode = DialogBox_Party::mode::invited;
	std::memset(party_dlg->m_leader_name, 0, sizeof(party_dlg->m_leader_name));
	const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyQueryJoinParty>(
		data, sizeof(hb::net::PacketNotifyQueryJoinParty));
	if (!pkt) return;
	std::snprintf(party_dlg->m_leader_name, sizeof(party_dlg->m_leader_name), "%s", pkt->name);
}

void HandleResponseCreateNewParty(CGame* game, char* data)
{
	const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyResponseCreateNewParty>(
		data, sizeof(hb::net::PacketNotifyResponseCreateNewParty));
	if (!pkt) return;
	if ((bool)pkt->result == true)
	{
		game->get_dialog_box_manager().get_dialog_as<DialogBox_Party>(DialogBoxId::Party)->m_mode = DialogBox_Party::mode::pointing;
	}
	else
	{
		game->get_dialog_box_manager().get_dialog_as<DialogBox_Party>(DialogBoxId::Party)->m_mode = DialogBox_Party::mode::join_requested;
	}
}

} // namespace NetworkMessageHandlers
