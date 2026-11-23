#pragma once

#include "server_command.h"

class CmdSetAdmin : public ServerCommand
{
public:
	const char* get_name() const override { return "setadmin"; }
	const char* GetDescription() const override { return "Set a player as admin"; }
	const char* GetHelp() const override { return "Usage: setadmin <charname> [level] | setadmin <charname> resetip"; }
	void execute(CGame* game, const char* args) override;
};
