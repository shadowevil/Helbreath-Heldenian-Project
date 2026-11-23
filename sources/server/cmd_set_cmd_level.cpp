#include "cmd_set_cmd_level.h"
#include "server_console.h"
#include "game.h"
#include "game_config_sqlite_store.h"
#include <cstring>
#include <cstdlib>
#include "log.h"
#include "server_log_channels.h"

void CmdSetCmdLevel::execute(CGame* game, const char* args)
{
	if (args == nullptr || args[0] == '\0')
	{
		hb::console::error("Usage: setcmdlevel <command> <level>");
		if (!game->m_command_permissions.empty())
		{
			hb::console::info("Current command permissions:");
			for (const auto& pair : game->m_command_permissions)
			{
				if (pair.second.description.empty())
					hb::console::write("  /{} -> level {}", pair.first, pair.second.admin_level);
				else
					hb::console::write("  /{} -> level {} ({})", pair.first, pair.second.admin_level, pair.second.description);
			}
		}
		return;
	}

	// Parse command name (first word)
	char cmd_name[64] = {};
	const char* p = args;
	int ci = 0;
	while (*p != '\0' && *p != ' ' && *p != '\t' && ci < 63)
	{
		cmd_name[ci++] = *p++;
	}

	// Skip whitespace
	while (*p == ' ' || *p == '\t')
		p++;

	if (*p == '\0')
	{
		// Show current level for this command
		auto it = game->m_command_permissions.find(cmd_name);
		if (it != game->m_command_permissions.end())
		{
			hb::console::write("Command '/{}' requires admin level {}", cmd_name, it->second.admin_level);
		}
		else
		{
			hb::console::error("Command '/{}' not found in permissions table", cmd_name);
		}
		return;
	}

	int level = std::atoi(p);
	if (level < 0)
	{
		hb::console::error("Level must be >= 0.");
		return;
	}

	// Update or create the permission entry, preserving existing description
	auto it = game->m_command_permissions.find(cmd_name);
	if (it != game->m_command_permissions.end())
	{
		it->second.admin_level = level;
	}
	else
	{
		CommandPermission perm;
		perm.admin_level = level;
		game->m_command_permissions[cmd_name] = perm;
	}

	// Save to DB
	sqlite3* configDb = nullptr;
	std::string dbPath;
	if (EnsureGameConfigDatabase(&configDb, dbPath, nullptr))
	{
		SaveCommandPermissions(configDb, game);
		CloseGameConfigDatabase(configDb);
	}

	hb::console::success("Command '/{}' now requires admin level {}", cmd_name, level);
	hb::logger::log<hb::log_channel::commands>("setcmdlevel: /{} set to level {}", cmd_name, level);
}
