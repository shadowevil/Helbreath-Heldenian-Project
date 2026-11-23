#include "game_cmd_block.h"
#include "game.h"
#include "account_sqlite_store.h"
#include <cstring>
#include "string_compat.h"

using namespace hb::shared::net;
bool GameCmdBlock::execute(CGame* game, int client_h, const char* args)
{
	if (game->m_client_list[client_h] == nullptr)
		return true;

	if (args == nullptr || args[0] == '\0')
	{
		game->send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0, "Usage: /block CharName");
		return true;
	}

	// Extract character name (max 10 chars, first word only)
	char char_name[hb::shared::limits::CharNameLen] = {};
	size_t nameLen = std::strlen(args);
	if (nameLen > 10) nameLen = 10;
	for (size_t i = 0; i < nameLen; i++)
	{
		if (args[i] == ' ' || args[i] == '\t')
			break;
		char_name[i] = args[i];
	}

	if (char_name[0] == '\0')
		return true;

	// Can't block yourself
	if (hb_stricmp(char_name, game->m_client_list[client_h]->m_char_name) == 0)
	{
		game->send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0, "You cannot block yourself.");
		return true;
	}

	// Resolve character name to account name
	char account_name[11] = {};
	if (!ResolveCharacterToAccount(char_name, account_name, sizeof(account_name)))
	{
		char msg[64] = {};
		std::snprintf(msg, sizeof(msg), "Character '%s' not found.", char_name);
		game->send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0, msg);
		return true;
	}

	// Check if already blocked
	if (game->m_client_list[client_h]->m_blocked_accounts.count(account_name) > 0)
	{
		char msg[64] = {};
		std::snprintf(msg, sizeof(msg), "'%s' is already blocked.", char_name);
		game->send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0, msg);
		return true;
	}

	// Add to block set and list
	game->m_client_list[client_h]->m_blocked_accounts.insert(account_name);
	game->m_client_list[client_h]->m_blocked_accounts_list.push_back(
		std::make_pair(std::string(account_name), std::string(char_name)));
	game->m_client_list[client_h]->m_block_list_dirty = true;

	char msg[64] = {};
	std::snprintf(msg, sizeof(msg), "'%s' has been blocked.", char_name);
	game->send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0, msg);

	return true;
}
