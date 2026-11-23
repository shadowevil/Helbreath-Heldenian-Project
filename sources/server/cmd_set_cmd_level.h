#pragma once

#include "server_command.h"

class CmdSetCmdLevel : public ServerCommand
{
public:
	const char* get_name() const override { return "setcmdlevel"; }
	const char* GetDescription() const override { return "Set required admin level for a chat command"; }
	const char* GetHelp() const override { return "Usage: setcmdlevel <command> <level>"; }
	void execute(CGame* game, const char* args) override;
};
