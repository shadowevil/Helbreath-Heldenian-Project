#pragma once

#include "game_chat_command.h"
#include "admin_level.h"

class GameCmdCreateItem : public GameChatCommand
{
public:
	const char* get_name() const override { return "createitem"; }
	int get_default_level() const override { return hb::shared::admin::Administrator; }
	bool execute(CGame* game, int client_h, const char* args) override;
};
