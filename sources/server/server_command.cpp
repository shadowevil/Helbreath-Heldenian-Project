#include "server_command.h"
#include "server_console.h"
#include "cmd_help.h"
#include "cmd_show_chat.h"
#include "cmd_broadcast.h"
#include "cmd_give_item.h"
#include "cmd_reload.h"
#include "cmd_set_admin.h"
#include "cmd_set_cmd_level.h"
#include "cmd_save_all.h"
#include "cmd_shutdown.h"
#include "game.h"
#include <cstring>
#include <cstdio>
#include "string_compat.h"

ServerCommandManager& ServerCommandManager::get()
{
	static ServerCommandManager instance;
	return instance;
}

void ServerCommandManager::initialize(CGame* game)
{
	if (m_initialized)
		return;

	m_game = game;
	register_built_in_commands();
	m_initialized = true;
}

void ServerCommandManager::register_command(std::unique_ptr<ServerCommand> command)
{
	m_commands.push_back(std::move(command));
}

bool ServerCommandManager::process_command(const char* input)
{
	if (input == nullptr)
		return false;

	// Skip leading whitespace
	const char* p = input;
	while (*p == ' ' || *p == '\t')
		p++;

	if (*p == '\0')
		return false;

	// Extract the command name (first word)
	const char* cmdStart = p;
	while (*p != '\0' && *p != ' ' && *p != '\t')
		p++;
	size_t cmdLen = p - cmdStart;

	// Skip whitespace after command name to get args
	while (*p == ' ' || *p == '\t')
		p++;
	const char* args = p;

	// Find matching command (case-insensitive)
	for (const auto& cmd : m_commands)
	{
		const char* cmdName = cmd->get_name();
		if (std::strlen(cmdName) == cmdLen && hb_strnicmp(cmdStart, cmdName, cmdLen) == 0)
		{
			cmd->execute(m_game, args);
			return true;
		}
	}

	hb::console::error("Unknown command: '{}'. Type 'help' for a list of commands.",
		std::string(cmdStart, cmdLen));
	return false;
}

void ServerCommandManager::register_built_in_commands()
{
	register_command(std::make_unique<CmdHelp>(m_commands));
	register_command(std::make_unique<CmdShowChat>());
	register_command(std::make_unique<CmdBroadcast>());
	register_command(std::make_unique<CmdGiveItem>());
	register_command(std::make_unique<CmdReload>());
	register_command(std::make_unique<CmdSetAdmin>());
	register_command(std::make_unique<CmdSetCmdLevel>());
	register_command(std::make_unique<CmdSaveAll>());
	register_command(std::make_unique<CmdShutdown>());
}
