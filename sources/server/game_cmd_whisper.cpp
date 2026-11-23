#include "game_cmd_whisper.h"
#include "game.h"
#include <cstring>
#include "string_compat.h"

using namespace hb::shared::net;
using namespace hb::server::config;
bool GameCmdWhisper::execute(CGame* game, int client_h, const char* args)
{
	if (game->m_client_list[client_h] == nullptr)
		return true;

	if (args == nullptr || args[0] == '\0')
	{
		// No name = disable whisper mode
		game->m_client_list[client_h]->m_whisper_player_index = -1;
		std::memset(game->m_client_list[client_h]->m_whisper_player_name, 0,
			sizeof(game->m_client_list[client_h]->m_whisper_player_name));
		game->m_client_list[client_h]->m_is_checking_whisper_player = false;

		char name[hb::shared::limits::CharNameLen] = {};
		game->send_notify_msg(0, client_h, Notify::WhisperModeOff, 0, 0, 0, name);
		return true;
	}

	// Extract player name (max 10 chars)
	char name[hb::shared::limits::CharNameLen] = {};
	size_t nameLen = std::strlen(args);
	if (nameLen > hb::shared::limits::CharNameLen - 1) nameLen = hb::shared::limits::CharNameLen - 1;

	// Copy only the first word (stop at space)
	for (size_t i = 0; i < nameLen; i++)
	{
		if (args[i] == ' ' || args[i] == '\t')
			break;
		name[i] = args[i];
	}

	if (name[0] == '\0')
		return true;

	game->m_client_list[client_h]->m_whisper_player_index = -1;

	// Search for player on this server (case-insensitive)
	for(int i = 1; i < MaxClients; i++)
	{
		if (game->m_client_list[i] != nullptr &&
			hb_strnicmp(game->m_client_list[i]->m_char_name, name, hb::shared::limits::CharNameLen - 1) == 0)
		{
			// Can't whisper yourself
			if (i == client_h)
				return true;

			game->m_client_list[client_h]->m_whisper_player_index = i;
			std::memset(game->m_client_list[client_h]->m_whisper_player_name, 0,
				sizeof(game->m_client_list[client_h]->m_whisper_player_name));
			std::strcpy(game->m_client_list[client_h]->m_whisper_player_name,
				game->m_client_list[i]->m_char_name);
			break;
		}
	}

	if (game->m_client_list[client_h]->m_whisper_player_index == -1)
	{
		game->send_notify_msg(0, client_h, Notify::PlayerNotOnGame, 0, 0, 0, name);
	}
	else
	{
		game->send_notify_msg(0, client_h, Notify::WhisperModeOn, 0, 0, 0,
			game->m_client_list[client_h]->m_whisper_player_name);
	}

	return true;
}
