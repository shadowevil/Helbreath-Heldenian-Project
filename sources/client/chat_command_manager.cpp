#include "chat_command_manager.h"
#include "game.h"
#include <cstring>

ChatCommandManager& ChatCommandManager::get()
{
	static ChatCommandManager instance;
	return instance;
}

void ChatCommandManager::initialize(CGame* game)
{
	if (m_initialized)
		return;

	m_game = game;
	register_built_in_commands();
	m_initialized = true;
}

void ChatCommandManager::register_command(std::unique_ptr<ChatCommand> command)
{
	m_commands.push_back(std::move(command));
}

bool ChatCommandManager::process_command(const char* message)
{
	if (m_game == nullptr || message == nullptr)
		return false;

	if (message[0] != '/')
		return false;

	// Skip the leading slash
	const char* command = message + 1;

	// Find the command that matches
	for (const auto& cmd : m_commands)
	{
		const char* cmdName = cmd->get_name();
		size_t cmdLen = std::strlen(cmdName);

		// Check if message starts with this command
		if (std::strncmp(command, cmdName, cmdLen) == 0)
		{
			// Make sure it's a complete match (followed by space, null, or end)
			char nextChar = command[cmdLen];
			if (nextChar == '\0' || nextChar == ' ' || nextChar == '\t')
			{
				// get arguments (skip command and any leading whitespace)
				const char* args = command + cmdLen;
				while (*args == ' ' || *args == '\t')
					args++;

				return cmd->execute(m_game, args);
			}
		}
	}

	return false;
}

void ChatCommandManager::register_built_in_commands()
{

}
