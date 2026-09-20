// Screen_OnGame.Network.cpp: Network message dispatch for gameplay screen
//
// Routes game server messages to existing handler functions.
//
//////////////////////////////////////////////////////////////////////

#include "Screen_OnGame.h"
#include "Game.h"
#include "GameModeManager.h"
#include "NetworkMessageManager.h"
#include "TeleportManager.h"
#include "ShopManager.h"
#include "Player.h"
#include "Packet/SharedPackets.h"
#include "ClientMessages.h"
#include "Log.h"

using namespace hb::shared::net;
using namespace hb::client::net;

namespace NetworkMessageHandlers {
	void HandlePlayerCharacterContents(CGame* game, char* data);
	void HandleServerConfigUpdate(CGame* game, char* data);
}

bool Screen_OnGame::on_game_msg(uint32_t msg_id, uint16_t msg_type, char* data, uint32_t msg_size)
{
	switch (msg_id) {
	case MsgId::Notify:
		if (m_network_message_manager)
			m_network_message_manager->process_message(MsgId::Notify, data, msg_size);
		return true;

	case MsgId::ResponseMotion:
		m_game->motion_response_handler(data);
		return true;

	case MsgId::EventCommon:
		m_game->common_event_handler(data);
		return true;

	case MsgId::EventMotion:
		m_game->motion_event_handler(data);
		return true;

	case MsgId::EventLog:
		m_game->log_event_handler(data);
		return true;

	case MsgId::CommandChatMsg:
		m_game->chat_msg_handler(data);
		return true;

	case MsgId::PlayerItemListContents:
		m_game->init_item_list(data);
		return true;

	case MsgId::PlayerCharacterContents:
		NetworkMessageHandlers::HandlePlayerCharacterContents(m_game, data);
		return true;

	case MsgId::ServerConfigUpdate:
		NetworkMessageHandlers::HandleServerConfigUpdate(m_game, data);
		return true;

	case MsgId::ResponseCivilRight:
		m_game->civil_right_admission_handler(data);
		return true;

	case MsgId::ResponseRetrieveItem:
		m_game->retrieve_item_handler(data);
		return true;

	case MsgId::ResponsePanning:
		m_game->response_panning_handler(data);
		return true;

	case MSGID_RESPONSE_SHOP_CONTENTS:
		shop_manager::get().handle_response(data);
		return true;

	case MsgId::ResponseNoticement:
		m_game->noticement_handler(data);
		return true;

	case MsgId::DynamicObject:
		m_game->dynamic_object_handler(data);
		return true;

	case ClientMsgId::ResponseChargedTeleport:
		teleport_manager::get().handle_charged_teleport(data);
		return true;

	case ClientMsgId::ResponseTeleportList:
		teleport_manager::get().handle_teleport_list(data);
		return true;

	case ClientMsgId::ResponseHeldenianTpList:
		teleport_manager::get().handle_heldenian_teleport_list(data);
		return true;
	}

	return false;
}
