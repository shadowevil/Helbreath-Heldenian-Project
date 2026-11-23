#pragma once

#include "server_command.h"

class CmdHelp : public ServerCommand
{
public:
	CmdHelp(const std::vector<std::unique_ptr<ServerCommand>>& commands)
		: m_commands(commands) {}

	const char* get_name() const override { return "help"; }
	const char* GetDescription() const override { return "List available commands"; }
	const char* GetHelp() const override { return "Usage: help [command]\n  Without arguments, lists all commands.\n  With a command name, shows detailed help for that command."; }
	void execute(CGame* game, const char* args) override;

private:
	const std::vector<std::unique_ptr<ServerCommand>>& m_commands;
};
