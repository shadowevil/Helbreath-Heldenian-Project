#include "cmd_help.h"
#include "server_console.h"
#include <cstring>
#include <format>
#include "string_compat.h"

void CmdHelp::execute(CGame* game, const char* args)
{
	// help <command> — show detailed help for a specific command
	if (args != nullptr && args[0] != '\0')
	{
		for (const auto& cmd : m_commands)
		{
			if (hb_stricmp(args, cmd->get_name()) == 0)
			{
				hb::console::info("{} - {}", cmd->get_name(), cmd->GetDescription());

				// Print help text line by line (split on \n)
				const char* help = cmd->GetHelp();
				const char* p = help;
				while (*p != '\0')
				{
					const char* lineEnd = p;
					while (*lineEnd != '\0' && *lineEnd != '\n')
						lineEnd++;

					std::string line(p, lineEnd - p);
					hb::console::write("  {}", line);

					p = (*lineEnd == '\n') ? lineEnd + 1 : lineEnd;
				}
				return;
			}
		}

		hb::console::error("Unknown command: '{}'. Type 'help' for a list.", args);
		return;
	}

	// help — list all commands
	hb::console::info("Available commands:");

	// Find the longest command name for alignment
	size_t max_name_len = 0;
	for (const auto& cmd : m_commands)
	{
		size_t len = std::strlen(cmd->get_name());
		if (len > max_name_len)
			max_name_len = len;
	}

	for (const auto& cmd : m_commands)
	{
		hb::console::write("  {:<{}}  {}", cmd->get_name(), max_name_len, cmd->GetDescription());
	}

	hb::console::write("");
	hb::console::write("Type 'help <command>' for detailed usage.", console_color::muted);
}
