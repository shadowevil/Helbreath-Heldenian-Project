#include "game_cmd_goto.h"
#include "game.h"
#include "account_sqlite_store.h"
#include <cstring>
#include <cstdio>

using namespace hb::shared::net;
bool GameCmdGoto::execute(CGame* game, int client_h, const char* args)
{
	if (game->m_client_list[client_h] == nullptr)
		return true;

	if (args == nullptr || args[0] == '\0')
	{
		game->send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0, "Usage: /goto <playername>");
		return true;
	}

	// Try online first
	int target_h = game->find_client_by_name(args);
	if (target_h != 0 && game->m_client_list[target_h] != nullptr)
	{
		if (game->gm_teleport_to(client_h, game->m_client_list[target_h]->m_map_name,
			game->m_client_list[target_h]->m_x, game->m_client_list[target_h]->m_y))
		{
			char buf[64];
			std::snprintf(buf, sizeof(buf), "Teleported to %s.", game->m_client_list[target_h]->m_char_name);
			game->send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0, buf);
		}
		else
		{
			game->send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0, "Teleport failed (invalid map).");
		}
		return true;
	}

	// Offline path: look up last-logout position from DB
	char account_name[12];
	std::memset(account_name, 0, sizeof(account_name));
	if (!ResolveCharacterToAccount(args, account_name, sizeof(account_name)))
	{
		game->send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0, "Player not found.");
		return true;
	}

	sqlite3* db = nullptr;
	std::string dbPath;
	if (!EnsureAccountDatabase(account_name, &db, dbPath))
	{
		game->send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0, "Failed to open account database.");
		return true;
	}

	AccountDbCharacterState state{};
	bool loaded = LoadCharacterState(db, args, state);
	CloseAccountDatabase(db);

	if (!loaded)
	{
		game->send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0, "Failed to load character data.");
		return true;
	}

	if (game->gm_teleport_to(client_h, state.map_name, static_cast<short>(state.map_x), static_cast<short>(state.map_y)))
	{
		char buf[64];
		std::snprintf(buf, sizeof(buf), "Teleported to %s's last position (offline).", state.character_name);
		game->send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0, buf);
	}
	else
	{
		game->send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0, "Teleport failed (invalid map).");
	}

	return true;
}
