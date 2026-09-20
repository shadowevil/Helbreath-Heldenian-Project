#include "GameCmdCreateItem.h"
#include "Game.h"
#include "ItemManager.h"
#include "Item.h"
#include "Item/ItemEnums.h"
#include <cstring>
#include <cstdio>

using namespace hb::shared::net;
using namespace hb::server::config;
bool GameCmdCreateItem::execute(CGame* game, int client_h, const char* args)
{
	if (game->m_client_list[client_h] == nullptr)
		return true;

	int item_id = 0, amount = 1;
	if (args == nullptr || args[0] == '\0' || sscanf(args, "%d %d", &item_id, &amount) < 1)
	{
		game->send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0, "Usage: /createitem <item_id> [amount]");
		return true;
	}

	if (item_id < 0 || item_id >= MaxItemTypes || game->m_item_config_list[item_id] == nullptr)
	{
		game->send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0, "Invalid item ID.");
		return true;
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
		CItem* item = new CItem();
		if (game->m_item_manager->init_item_attr(item, item_name))
		{
			item->m_instance.count = amount;
			int erase_req = 0;
			if (game->m_item_manager->add_client_item_list(client_h, item, &erase_req))
			{
				game->m_item_manager->send_item_notify_msg(client_h, Notify::ItemObtained, item, 0);
				created = amount;
			}
			else
			{
				delete item;
				game->send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0, "Inventory full.");
				return true;
			}
		}
		else
		{
			delete item;
			game->send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0, "Failed to create item.");
			return true;
		}
	}
	else
	{
		// Soft-linked items: individual items, one bulk notification
		created = game->m_item_manager->add_client_bulk_item_list(client_h, item_name, amount);
	}

	char buf[128];
	std::snprintf(buf, sizeof(buf), "Created %d x %s (ID: %d).", created, item_name, item_id);
	game->send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0, buf);

	return true;
}
