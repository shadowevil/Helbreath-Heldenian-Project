#pragma once

#include "game_chat_command.h"
#include "admin_level.h"

class GameCmdSaveAll : public GameChatCommand
{
public:
	const char* get_name() const override { return "saveall"; }
	int get_default_level() const override { return hb::shared::admin::GameMaster; }
	bool execute(CGame* game, int client_h, const char* args) override;
};
