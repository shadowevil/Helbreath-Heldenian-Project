#include "game_cmd_gm.h"
#include "game.h"
#include <cstring>
#include "string_compat.h"


using namespace hb::shared::net;
using namespace hb::shared::action;

bool GameCmdGM::execute(CGame* game, int client_h, const char* args)
{
	if (game->m_client_list[client_h] == nullptr)
		return true;

	if (args == nullptr || args[0] == '\0')
	{
		game->send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0, "Usage: /gm on | /gm off");
		return true;
	}

	if (hb_stricmp(args, "on") == 0)
	{
		game->m_client_list[client_h]->m_is_gm_mode = true;
		game->m_client_list[client_h]->m_status.gm_mode = true;
		game->send_event_to_near_client_type_a(static_cast<short>(client_h), hb::shared::owner_class::Player, MsgId::EventMotion, Type::NullAction, 0, 0, 0);
		game->send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0, "GM mode enabled.");
		return true;
	}
	else if (hb_stricmp(args, "off") == 0)
	{
		game->m_client_list[client_h]->m_is_gm_mode = false;
		game->m_client_list[client_h]->m_status.gm_mode = false;
		game->send_event_to_near_client_type_a(static_cast<short>(client_h), hb::shared::owner_class::Player, MsgId::EventMotion, Type::NullAction, 0, 0, 0);
		game->send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0, "GM mode disabled.");
		return true;
	}
	else
	{
		game->send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0, "Usage: /gm on | /gm off");
		return true;
	}
}
