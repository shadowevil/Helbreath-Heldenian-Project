#pragma once

#include "server_command.h"

class CmdShutdown : public ServerCommand
{
public:
	const char* get_name() const override { return "shutdown"; }
	const char* GetDescription() const override { return "Save all players and gracefully shut down the server"; }
	const char* GetHelp() const override { return "Usage: shutdown [seconds] [\"message\"]\n  Saves all connected players, sends a shutdown notice to all clients,\n  then gracefully disconnects and shuts down after the specified delay.\n  Examples:\n    shutdown           - Immediate shutdown\n    shutdown 60        - Shutdown in 60 seconds\n    shutdown 60 Server maintenance in progress"; }
	void execute(CGame* game, const char* args) override;
};
