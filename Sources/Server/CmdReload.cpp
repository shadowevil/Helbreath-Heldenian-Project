#include "CmdReload.h"
#include "ServerConsole.h"
#include "Game.h"
#include "SkillManager.h"
#include "MagicManager.h"
#include "ItemManager.h"
#include "Log.h"
#include "ServerLogChannels.h"
#include "StringCompat.h"

void CmdReload::execute(CGame* game, const char* args)
{
	if (args == nullptr || args[0] == '\0')
	{
		hb::console::error("Usage: reload <items|magic|skills|npcs|shops|config|formulas|colors|attributes|all>");
		return;
	}

	bool items = false;
	bool magic = false;
	bool skills = false;
	bool npcs = false;
	bool shops = false;
	bool config = false;
	bool formulas = false;
	bool colors = false;
	bool attribute_types = false;

	if (hb_stricmp(args, "items") == 0)
		items = true;
	else if (hb_stricmp(args, "magic") == 0)
		magic = true;
	else if (hb_stricmp(args, "skills") == 0)
		skills = true;
	else if (hb_stricmp(args, "npcs") == 0)
		npcs = true;
	else if (hb_stricmp(args, "shops") == 0)
		shops = true;
	else if (hb_stricmp(args, "config") == 0)
		config = true;
	else if (hb_stricmp(args, "formulas") == 0)
		formulas = true;
	else if (hb_stricmp(args, "colors") == 0)
		colors = true;
	else if (hb_stricmp(args, "attributes") == 0)
		attribute_types = true;
	else if (hb_stricmp(args, "all") == 0)
	{
		items = true;
		magic = true;
		skills = true;
		npcs = true;
		shops = true;
		config = true;
		formulas = true;
		colors = true;
		attribute_types = true;
	}
	else
	{
		hb::console::error("Unknown reload target: '{}'. Use items, magic, skills, npcs, shops, config, formulas, colors, attributes, or all.", args);
		return;
	}

	// Reload server_config.json if requested
	if (config)
	{
		if (game->reload_server_config())
			hb::console::success("server_config.json reloaded");
		else
			hb::console::error("server_config.json reload failed");
	}

	// Reload formulas from database
	if (formulas)
	{
		if (game->reload_formulas())
			hb::console::success("Formulas reloaded");
		else
			hb::console::error("Formula reload failed");
	}

	// Send reload notification to clients first (shows top bar message)
	if (items || magic || skills || npcs || shops || formulas || colors || attribute_types)
		game->send_config_reload_notification(items, magic, skills, npcs, formulas, colors, attribute_types);

	// Reload configs from database
	if (items)           game->m_item_manager->reload_item_configs();
	if (magic)           game->m_magic_manager->reload_magic_configs();
	if (skills)          game->m_skill_manager->reload_skill_configs();
	if (npcs)            game->reload_npc_configs();
	if (shops)           game->reload_shop_configs();
	if (colors)          game->reload_color_palette();
	if (attribute_types) game->reload_attribute_types();

	// Stream updated config data to clients
	if (items || magic || skills || npcs || formulas || colors || attribute_types)
		game->push_config_reload_to_clients(items, magic, skills, npcs, formulas, colors, attribute_types);

	hb::console::success("Reload complete: {}", args);
	hb::logger::log<hb::log_channel::commands>("reload {}", args);
}
