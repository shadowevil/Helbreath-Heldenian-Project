#pragma once

#include "game_chat_command.h"

class GameCmdWhisper : public GameChatCommand
{
public:
	const char* get_name() const override { return "to"; }
	bool execute(CGame* game, int client_h, const char* args) override;
};
