#pragma once

#include "server_command.h"

class CmdGiveItem : public ServerCommand
{
public:
	const char* get_name() const override { return "giveitem"; }
	const char* GetDescription() const override { return "Give an item to a player"; }
	const char* GetHelp() const override { return "Usage: giveitem <playername> <item_id> <amount>"; }
	void execute(CGame* game, const char* args) override;
};
