#include "CmdGiveItem.h"
#include "ServerConsole.h"
#include "Game.h"
#include "ItemManager.h"
#include "Item.h"
#include "Item/ItemEnums.h"
#include <cstring>
#include <cstdlib>
#include "Log.h"
#include "ServerLogChannels.h"
#include "StringCompat.h"
using namespace hb::server::config;

void CmdGiveItem::execute(CGame* game, const char* args)
{
	if (args == nullptr || args[0] == '\0')
	{
		hb::console::error("Usage: giveitem <playername> <item_id> <amount>");
		return;
	}

	// Parse player name
	char player_name[32];
	std::memset(player_name, 0, sizeof(player_name));
	const char* p = args;
	int i = 0;
	while (*p != '\0' && *p != ' ' && *p != '\t' && i < (int)(sizeof(player_name) - 1))
	{
		player_name[i++] = *p++;
	}
	player_name[i] = '\0';

	// Skip whitespace
	while (*p == ' ' || *p == '\t')
		p++;

	if (*p == '\0')
	{
		hb::console::error("Usage: giveitem <playername> <item_id> <amount>");
		return;
	}

	// Parse item ID
	int item_id = std::atoi(p);
	while (*p != '\0' && *p != ' ' && *p != '\t')
		p++;

	// Skip whitespace
	while (*p == ' ' || *p == '\t')
		p++;

	if (*p == '\0')
	{
		hb::console::error("Usage: giveitem <playername> <item_id> <amount>");
		return;
	}

	// Parse amount
	int amount = std::atoi(p);

	// Find player by name
	int client_h = 0;
	for(int j = 1; j < MaxClients; j++)
	{
		if (game->m_client_list[j] != nullptr &&
			hb_stricmp(game->m_client_list[j]->m_char_name, player_name) == 0)
		{
			client_h = j;
			break;
		}
	}

	if (client_h == 0)
	{
		hb::console::error("Player '{}' not found.", player_name);
		return;
	}

	// Validate item ID
	if (item_id < 0 || item_id >= MaxItemTypes || game->m_item_config_list[item_id] == nullptr)
	{
		hb::console::error("Invalid item ID: {}.", item_id);
		return;
	}

	if (amount < 1) amount = 1;
	bool is_gold = (item_id == hb::shared::item::ItemId::Gold);
	if (!is_gold && amount > 1000) amount = 1000;

	const char* item_name = game->m_item_config_list[item_id]->m_name;
	bool true_stack = game->m_item_config_list[item_id]->is_stackable() || is_gold;

	int created = 0;

	if (true_stack)
	{
		// True stacks: single item with count = amount (arrows, materials, gold)
		CItem* item = new CItem;
		if (game->m_item_manager->init_item_attr(item, item_id) == false)
		{
			delete item;
			hb::console::error("Failed to initialize item ID: {}.", item_id);
			return;
		}
		item->m_instance.count = amount;

		if (game->m_item_manager->add_item(client_h, item, 0) == false)
		{
			hb::console::error("Failed to give item: player inventory full.");
			return;
		}
		created = amount;
	}
	else
	{
		// Soft-linked items: individual items, one bulk notification
		created = game->m_item_manager->add_client_bulk_item_list(client_h, item_name, amount);
	}

	// Success
	hb::console::success("Gave {}x {} (ID: {}) to {}.", created, item_name, item_id, player_name);
	hb::logger::log<hb::log_channel::commands>("giveitem: {}x {} (ID: {}) to {}", created, item_name, item_id, player_name);
}
