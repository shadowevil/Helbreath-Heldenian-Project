#pragma once

#include "server_command.h"

class CmdSaveAll : public ServerCommand
{
public:
	const char* get_name() const override { return "saveall"; }
	const char* GetDescription() const override { return "Save all connected players' data"; }
	const char* GetHelp() const override { return "Usage: saveall\n  Saves all connected players' data to the database immediately.\n  Players remain connected and are not affected."; }
	void execute(CGame* game, const char* args) override;
};
