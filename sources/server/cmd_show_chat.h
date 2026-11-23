#pragma once

#include "server_command.h"

#ifdef _WIN32
	#include <windows.h>
#endif

class CmdShowChat : public ServerCommand
{
public:
	const char* get_name() const override { return "showchat"; }
	const char* GetDescription() const override { return "Open live chat viewer in a new terminal"; }
	const char* GetHelp() const override { return "Usage: showchat\n  Opens a new terminal window that tails gamelogs/chat.log in real-time.\n  Only one viewer can be open at a time."; }
	void execute(CGame* game, const char* args) override;

private:
#ifdef _WIN32
	HANDLE m_hProcess = nullptr;
#else
	int m_pid = 0;
#endif
};
