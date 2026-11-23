#pragma once

#include "game_chat_command.h"
#include "admin_level.h"

class GameCmdGoto : public GameChatCommand
{
public:
	const char* get_name() const override { return "goto"; }
	int get_default_level() const override { return hb::shared::admin::GameMaster; }
	bool execute(CGame* game, int client_h, const char* args) override;
};
