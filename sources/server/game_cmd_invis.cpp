#include "game_cmd_invis.h"
#include "game.h"
#include <cstring>


using namespace hb::shared::net;
using namespace hb::shared::action;

bool GameCmdInvis::execute(CGame* game, int client_h, const char* args)
{
	if (game->m_client_list[client_h] == nullptr)
		return true;

	if (game->m_client_list[client_h]->m_is_admin_invisible)
	{
		// Toggle OFF
		game->m_client_list[client_h]->m_is_admin_invisible = false;

		// Broadcast full appearance to all nearby (re-appear)
		game->send_event_to_near_client_type_a(client_h, hb::shared::owner_class::Player, MsgId::EventMotion, Type::NullAction, 0, 0, 0);

		game->send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0, "Admin invisibility disabled.");
	}
	else
	{
		// Toggle ON — despawn from non-qualifying viewers BEFORE setting the flag,
		// otherwise the filtering in send_event_to_near_client_type_a will skip the despawn packet
		game->send_event_to_near_client_type_a(client_h, hb::shared::owner_class::Player, MsgId::EventLog, MsgType::Reject, 0, 0, 0);

		game->m_client_list[client_h]->m_is_admin_invisible = true;

		// Now re-broadcast as a NULLACTION so higher-level admins see the invis+GM flagged version
		game->send_event_to_near_client_type_a(client_h, hb::shared::owner_class::Player, MsgId::EventMotion, Type::NullAction, 0, 0, 0);

		game->send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0, "Admin invisibility enabled.");
	}

	return true;
}
