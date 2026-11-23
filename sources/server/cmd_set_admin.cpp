#include "cmd_set_admin.h"
#include "server_console.h"
#include "game.h"
#include "game_config_sqlite_store.h"
#include "account_sqlite_store.h"
#include "admin_level.h"
#include <cstring>
#include <cstdlib>
#include "log.h"
#include "server_log_channels.h"
#include "string_compat.h"
using namespace hb::server::config;

void CmdSetAdmin::execute(CGame* game, const char* args)
{
	if (args == nullptr || args[0] == '\0')
	{
		hb::console::error("Usage: setadmin <charname> [resetip]");
		return;
	}

	// Parse character name (first word)
	char char_name[hb::shared::limits::CharNameLen] = {};
	const char* p = args;
	int ci = 0;
	while (*p != '\0' && *p != ' ' && *p != '\t' && ci < 10)
	{
		char_name[ci++] = *p++;
	}

	// Skip whitespace to check for subcommand
	while (*p == ' ' || *p == '\t')
		p++;

	// Check for "resetip" subcommand
	if (hb_stricmp(p, "resetip") == 0)
	{
		int idx = game->find_admin_by_char_name(char_name);
		if (idx == -1)
		{
			hb::console::error("Admin entry not found for character: {}", char_name);
			return;
		}

		strcpy(game->m_admin_list[idx].approved_ip, "0.0.0.0");

		sqlite3* configDb = nullptr;
		std::string dbPath;
		if (EnsureGameConfigDatabase(&configDb, dbPath, nullptr))
		{
			SaveAdminConfig(configDb, game);
			CloseGameConfigDatabase(configDb);
		}

		hb::console::success("Admin IP reset for character: {}", char_name);
		hb::logger::log<hb::log_channel::commands>("setadmin: IP reset for {}", char_name);
		return;
	}

	// Parse optional admin level (default: GameMaster = 1)
	int admin_level = hb::shared::admin::GameMaster;
	if (*p != '\0')
	{
		admin_level = std::atoi(p);
		if (admin_level < 1)
		{
			hb::console::error("Admin level must be >= 1. Level 0 is Player (non-admin).");
			return;
		}
	}

	// Adding a new admin - find the account name
	char account_name[11] = {};
	bool found = false;

	// First check online clients
	for (int i = 1; i < MaxClients; i++)
	{
		if (game->m_client_list[i] != nullptr &&
			hb_stricmp(game->m_client_list[i]->m_char_name, char_name) == 0)
		{
			strncpy(account_name, game->m_client_list[i]->m_account_name, 10);
			account_name[10] = '\0';
			found = true;
			break;
		}
	}

	// If not found online, search account DB files
	if (!found)
	{
		if (ResolveCharacterToAccount(char_name, account_name, sizeof(account_name)))
		{
			found = true;
		}
	}

	if (!found)
	{
		hb::console::error("Player not found: {}", char_name);
		return;
	}

	// Check if already an admin - if so, update their level
	int existingAdminIdx = game->find_admin_by_account(account_name);
	if (existingAdminIdx != -1)
	{
		game->m_admin_list[existingAdminIdx].m_admin_level = admin_level;

		// Update online player's level
		for (int i = 1; i < MaxClients; i++)
		{
			if (game->m_client_list[i] != nullptr &&
				hb_stricmp(game->m_client_list[i]->m_account_name, account_name) == 0)
			{
				game->m_client_list[i]->m_admin_level = admin_level;
				break;
			}
		}

		sqlite3* configDb = nullptr;
		std::string dbPath;
		if (EnsureGameConfigDatabase(&configDb, dbPath, nullptr))
		{
			SaveAdminConfig(configDb, game);
			CloseGameConfigDatabase(configDb);
		}

		hb::console::success("Admin level updated: {} (account: {}) -> level {}", char_name, account_name, admin_level);
		hb::logger::log<hb::log_channel::commands>("setadmin: {} (account: {}) updated to level {}", char_name, account_name, admin_level);
		return;
	}

	// Check capacity
	if (game->m_admin_count >= MaxAdmins)
	{
		hb::console::error("Admin list is full.");
		return;
	}

	// Add new admin entry
	int idx = game->m_admin_count;
	std::memset(&game->m_admin_list[idx], 0, sizeof(AdminEntry));
	strncpy(game->m_admin_list[idx].m_account_name, account_name, 10);
	strncpy(game->m_admin_list[idx].m_char_name, char_name, 10);
	strcpy(game->m_admin_list[idx].approved_ip, "0.0.0.0");
	game->m_admin_list[idx].m_admin_level = admin_level;
	game->m_admin_count++;

	// If the player is currently online, set their admin index and level
	for (int i = 1; i < MaxClients; i++)
	{
		if (game->m_client_list[i] != nullptr &&
			hb_stricmp(game->m_client_list[i]->m_account_name, account_name) == 0 &&
			hb_stricmp(game->m_client_list[i]->m_char_name, char_name) == 0)
		{
			game->m_client_list[i]->m_admin_index = idx;
			game->m_client_list[i]->m_admin_level = admin_level;
			break;
		}
	}

	// Save to DB
	sqlite3* configDb = nullptr;
	std::string dbPath;
	if (EnsureGameConfigDatabase(&configDb, dbPath, nullptr))
	{
		SaveAdminConfig(configDb, game);
		CloseGameConfigDatabase(configDb);
	}

	hb::console::success("Admin added: {} (account: {}, level: {})", char_name, account_name, admin_level);
	hb::logger::log<hb::log_channel::commands>("setadmin: added {} (account: {}, level: {})", char_name, account_name, admin_level);
}
