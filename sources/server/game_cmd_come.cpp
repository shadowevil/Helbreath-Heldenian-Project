#include "game_cmd_come.h"
#include "game.h"
#include "account_sqlite_store.h"
#include <cstring>
#include <cstdio>

using namespace hb::shared::net;
bool GameCmdCome::execute(CGame* game, int client_h, const char* args)
{
	if (game->m_client_list[client_h] == nullptr)
		return true;

	if (args == nullptr || args[0] == '\0')
	{
		game->send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0, "Usage: /come <playername>");
		return true;
	}

	const char* gmMap = game->m_client_list[client_h]->m_map_name;
	short gmX = game->m_client_list[client_h]->m_x;
	short gmY = game->m_client_list[client_h]->m_y;

	// Try online first
	int target_h = game->find_client_by_name(args);
	if (target_h != 0 && game->m_client_list[target_h] != nullptr)
	{
		if (game->gm_teleport_to(target_h, gmMap, gmX, gmY))
		{
			game->send_notify_msg(0, target_h, Notify::NoticeMsg, 0, 0, 0, "You have been summoned by a GM.");

			char buf[64];
			std::snprintf(buf, sizeof(buf), "Summoned %s to your location.", game->m_client_list[target_h]->m_char_name);
			game->send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0, buf);
		}
		else
		{
			game->send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0, "Summon failed.");
		}
		return true;
	}

	// Offline path: update saved position in DB
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
	if (!loaded)
	{
		CloseAccountDatabase(db);
		game->send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0, "Failed to load character data.");
		return true;
	}

	// Update position to GM's location
	std::memset(state.map_name, 0, sizeof(state.map_name));
	std::memcpy(state.map_name, gmMap, 10);
	state.map_x = gmX;
	state.map_y = gmY;

	InsertCharacterState(db, state);
	CloseAccountDatabase(db);

	char buf[80];
	std::snprintf(buf, sizeof(buf), "Updated %s's saved location (takes effect on next login).", state.character_name);
	game->send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0, buf);

	return true;
}
