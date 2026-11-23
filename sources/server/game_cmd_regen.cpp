#include "game_cmd_regen.h"
#include "game.h"
#include <cstring>
#include <cstdio>

using namespace hb::shared::net;
bool GameCmdRegen::execute(CGame* game, int client_h, const char* args)
{
	if (game->m_client_list[client_h] == nullptr)
		return true;

	int target_h = client_h;

	if (args != nullptr && args[0] != '\0')
	{
		target_h = game->find_client_by_name(args);
		if (target_h == 0)
		{
			game->send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0, "Player not found.");
			return true;
		}
	}

	if (game->m_client_list[target_h] == nullptr)
		return true;

	game->m_client_list[target_h]->m_hp = game->get_max_hp(target_h);
	game->m_client_list[target_h]->m_mp = game->get_max_mp(target_h);
	game->m_client_list[target_h]->m_sp = game->get_max_sp(target_h);
	game->m_client_list[target_h]->m_hunger_status = 100;

	game->send_notify_msg(0, target_h, Notify::Hp, 0, 0, 0, 0);
	game->send_notify_msg(0, target_h, Notify::Mp, 0, 0, 0, 0);
	game->send_notify_msg(0, target_h, Notify::Sp, 0, 0, 0, 0);
	game->send_notify_msg(0, target_h, Notify::Hunger, game->m_client_list[target_h]->m_hunger_status, 0, 0, 0);

	if (target_h != client_h)
	{
		game->send_notify_msg(0, target_h, Notify::NoticeMsg, 0, 0, 0, "Your health has been fully restored by a GM.");

		char buf[80];
		std::snprintf(buf, sizeof(buf), "Restored %s's HP/MP/SP/Hunger to full.", game->m_client_list[target_h]->m_char_name);
		game->send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0, buf);
	}
	else
	{
		game->send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0, "HP/MP/SP/Hunger restored to full.");
	}

	return true;
}
