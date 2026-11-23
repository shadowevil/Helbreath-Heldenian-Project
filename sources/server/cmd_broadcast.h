#pragma once

#include "server_command.h"

class CmdBroadcast : public ServerCommand
{
public:
	const char* get_name() const override { return "broadcast"; }
	const char* GetDescription() const override { return "Send a broadcast message to all players"; }
	const char* GetHelp() const override { return "Usage: broadcast <message>\n  Sends a server-wide chat message visible to all connected players.\n  The message appears as from 'Server' in the broadcast chat channel."; }
	void execute(CGame* game, const char* args) override;
};
