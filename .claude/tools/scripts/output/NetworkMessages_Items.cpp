#include "Game.h"
#include "BuildItemManager.h"
#include "InventoryManager.h"
#include "ItemNameFormatter.h"
#include "NetworkMessageManager.h"
#include "Packet/SharedPackets.h"
#include "lan_eng.h"
#include "DialogBoxIDs.h"
#include "DialogBox_Bank.h"
#include "DialogBox_ItemUpgrade.h"
#include "DialogBox_SellOrRepair.h"
#include "DialogBox_Exchange.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <format>
#include <string>
#include "Screen_OnGame.h"

using namespace hb::shared::net;
using namespace hb::shared::item;

namespace NetworkMessageHandlers {
	void HandleItemPurchased(CGame* game, char* data)
	{
		int i, j;
		uint64_t count;
		char  name[hb::shared::limits::ItemNameLen]{}, equip_pos, gender_limit;
		ItemType item_type;
		bool  is_equipped;
		short level_limit;
		uint16_t cost, weight, cur_life_span, max_life_span;
		std::string txt;

		char item_color;

		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyItemPurchased>(
			data, sizeof(hb::net::PacketNotifyItemPurchased));
		if (!pkt) return;

		memcpy(name, pkt->name, sizeof(pkt->name));
		count = pkt->count;
		item_type = static_cast<ItemType>(pkt->item_type);
		equip_pos = static_cast<char>(pkt->equip_pos);
		is_equipped = (pkt->is_equipped != 0);
		level_limit = static_cast<short>(pkt->level_limit);
		gender_limit = static_cast<char>(pkt->gender_limit);
		cur_life_span = pkt->cur_lifespan;
		max_life_span = pkt->max_lifespan;
		weight = pkt->weight;
		item_color = static_cast<char>(pkt->item_color);
		cost = pkt->cost;
		txt = std::format(NOTIFYMSG_ITEMPURCHASED, name, cost);
		game->add_event_list(txt.c_str(), 10);

		short item_id = pkt->item_id;

		if ((item_type == ItemType::Consume) || (item_type == ItemType::Arrow))
		{
			for (i = 0; i < hb::shared::limits::MaxItems; i++)
				if ((game->m_player->m_item_list[i] != 0) && (game->m_player->m_item_list[i]->m_id_num == item_id))
				{
					game->m_player->m_item_list[i]->m_count += count;
					return;
				}
		}

		short nX, nY;
		for (i = 0; i < hb::shared::limits::MaxItems; i++)
		{
			if ((game->m_player->m_item_list[i] != 0) && (game->m_player->m_item_list[i]->m_id_num == item_id))
			{
				nX = game->m_player->m_item_list[i]->m_x;
				nY = game->m_player->m_item_list[i]->m_y;
				break;
			}
			else
			{
				nX = 40;
				nY = 30;
			}
		}

		for (i = 0; i < hb::shared::limits::MaxItems; i++)
			if (game->m_player->m_item_list[i] == 0)
			{
				game->m_player->m_item_list[i] = std::make_unique<CItem>();
				game->m_player->m_item_list[i]->m_id_num = item_id;
				game->m_player->m_item_list[i]->m_count = count;
				game->m_player->m_item_list[i]->m_x = nX;
				game->m_player->m_item_list[i]->m_y = nY;
				{
			hb::net::PacketRequestSetItemPos req{};
			req.header.msg_id = MsgId::RequestSetItemPos;
			req.header.msg_type = 0;
			req.dir = static_cast<uint8_t>(i);
			req.x = static_cast<int16_t>(nX);
			req.y = static_cast<int16_t>(nY);
			game->send_game_packet(req, false);
		}
				inventory_manager::get().unlock_item(i);
				game->m_is_item_equipped[i] = false;
				game->m_player->m_item_list[i]->m_cur_life_span = cur_life_span;
				game->m_player->m_item_list[i]->m_item_color = item_color;
				game->m_player->m_item_list[i]->m_attribute = 0;

				for (j = 0; j < hb::shared::limits::MaxItems; j++)
					if (game->m_item_order[j] == -1) {
						game->m_item_order[j] = i;
						return;
					}

				return;
			}
	}

	void HandleItemObtained(CGame* game, char* data)
	{
		int i, j;
		uint64_t count;
		uint32_t attribute;
		char  name[hb::shared::limits::ItemNameLen]{}, equip_pos;
		ItemType item_type;
		bool  is_equipped;
		short level_limit, special_ev2;
		std::string txt;

		char gender_limit, item_color;
		uint16_t weight, cur_life_span, max_life_span;

		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyItemObtained>(
			data, sizeof(hb::net::PacketNotifyItemObtained));
		if (!pkt) return;

		memcpy(name, pkt->name, sizeof(pkt->name));
		count = pkt->count;
		item_type = static_cast<ItemType>(pkt->item_type);
		equip_pos = static_cast<char>(pkt->equip_pos);
		is_equipped = (pkt->is_equipped != 0);
		level_limit = static_cast<short>(pkt->level_limit);
		gender_limit = static_cast<char>(pkt->gender_limit);
		cur_life_span = pkt->cur_lifespan;
		max_life_span = pkt->max_lifespan;
		weight = pkt->weight;
		item_color = static_cast<char>(pkt->item_color);
		special_ev2 = static_cast<short>(pkt->spec_value2);
		attribute = pkt->attribute;

		if (count == 1) txt = std::format(NOTIFYMSG_ITEMOBTAINED2, name);
		else txt = std::format(NOTIFYMSG_ITEMOBTAINED1, count, name);

		game->add_event_list(txt.c_str(), 10);
		game->play_game_sound('E', 20, 0);

		game->m_map_data->set_item(game->m_player->m_player_x, game->m_player->m_player_y, 0, 0, 0, false);

		short item_id = pkt->item_id;

		if ((item_type == ItemType::Consume) || (item_type == ItemType::Arrow))
		{
			for (i = 0; i < hb::shared::limits::MaxItems; i++)
				if ((game->m_player->m_item_list[i] != 0) && (game->m_player->m_item_list[i]->m_id_num == item_id))
				{
					game->m_player->m_item_list[i]->m_count += count;
					inventory_manager::get().unlock_item(i);
					return;
				}
		}

		short nX, nY;
		for (i = 0; i < hb::shared::limits::MaxItems; i++)
		{
			if ((game->m_player->m_item_list[i] != 0) && (game->m_player->m_item_list[i]->m_id_num == item_id))
			{
				nX = game->m_player->m_item_list[i]->m_x;
				nY = game->m_player->m_item_list[i]->m_y;
				break;
			}
			else
			{
				nX = 40;
				nY = 30;
			}
		}

		for (i = 0; i < hb::shared::limits::MaxItems; i++)
			if (game->m_player->m_item_list[i] == 0)
			{
				game->m_player->m_item_list[i] = std::make_unique<CItem>();
				game->m_player->m_item_list[i]->m_id_num = item_id;
				game->m_player->m_item_list[i]->m_count = count;
				game->m_player->m_item_list[i]->m_x = nX;
				game->m_player->m_item_list[i]->m_y = nY;
				{
			hb::net::PacketRequestSetItemPos req{};
			req.header.msg_id = MsgId::RequestSetItemPos;
			req.header.msg_type = 0;
			req.dir = static_cast<uint8_t>(i);
			req.x = static_cast<int16_t>(nX);
			req.y = static_cast<int16_t>(nY);
			game->send_game_packet(req, false);
		}
				inventory_manager::get().unlock_item(i);
				game->m_is_item_equipped[i] = false;
				game->m_player->m_item_list[i]->m_cur_life_span = cur_life_span;
				game->m_player->m_item_list[i]->m_item_color = item_color;
				game->m_player->m_item_list[i]->m_item_special_effect_value2 = special_ev2;
				game->m_player->m_item_list[i]->m_attribute = attribute;

				build_item_manager::get().update_available_recipes();

				for (j = 0; j < hb::shared::limits::MaxItems; j++)
					if (game->m_item_order[j] == -1) {
						game->m_item_order[j] = i;
						return;
					}
				return;
			}
	}

	void HandleItemObtainedBulk(CGame* game, char* data)
	{
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyItemObtained>(
			data, sizeof(hb::net::PacketNotifyItemObtained));
		if (!pkt) return;

		char name[hb::shared::limits::ItemNameLen]{};
		memcpy(name, pkt->name, sizeof(pkt->name));

		uint64_t total_count = pkt->count;
		short item_id = pkt->item_id;
		uint16_t cur_life_span = pkt->cur_lifespan;
		uint16_t max_life_span = pkt->max_lifespan;
		uint16_t weight = pkt->weight;
		char item_color = static_cast<char>(pkt->item_color);
		short special_ev2 = static_cast<short>(pkt->spec_value2);
		uint32_t attribute = pkt->attribute;

		// One chat message for the entire batch
		std::string txt;
		if (total_count == 1) txt = std::format(NOTIFYMSG_ITEMOBTAINED2, name);
		else txt = std::format(NOTIFYMSG_ITEMOBTAINED1, total_count, name);
		game->add_event_list(txt.c_str(), 10);
		game->play_game_sound('E', 20, 0);

		// Create individual items in separate slots (no Consume/Arrow merge)
		short nX = 40, nY = 30;
		for (int i = 0; i < hb::shared::limits::MaxItems; i++)
		{
			if ((game->m_player->m_item_list[i] != 0) && (game->m_player->m_item_list[i]->m_id_num == item_id))
			{
				nX = game->m_player->m_item_list[i]->m_x;
				nY = game->m_player->m_item_list[i]->m_y;
				break;
			}
		}

		int created = 0;
		for (uint64_t n = 0; n < total_count; n++)
		{
			for (int i = 0; i < hb::shared::limits::MaxItems; i++)
			{
				if (game->m_player->m_item_list[i] == 0)
				{
					game->m_player->m_item_list[i] = std::make_unique<CItem>();
					game->m_player->m_item_list[i]->m_id_num = item_id;
					game->m_player->m_item_list[i]->m_count = 1;
					game->m_player->m_item_list[i]->m_x = nX;
					game->m_player->m_item_list[i]->m_y = nY;
					{
			hb::net::PacketRequestSetItemPos req{};
			req.header.msg_id = MsgId::RequestSetItemPos;
			req.header.msg_type = 0;
			req.dir = static_cast<uint8_t>(i);
			req.x = static_cast<int16_t>(nX);
			req.y = static_cast<int16_t>(nY);
			game->send_game_packet(req, false);
		}
					inventory_manager::get().unlock_item(i);
					game->m_is_item_equipped[i] = false;
					game->m_player->m_item_list[i]->m_cur_life_span = cur_life_span;
					game->m_player->m_item_list[i]->m_max_life_span = max_life_span;
					game->m_player->m_item_list[i]->m_weight = weight;
					game->m_player->m_item_list[i]->m_item_color = item_color;
					game->m_player->m_item_list[i]->m_item_special_effect_value2 = special_ev2;
					game->m_player->m_item_list[i]->m_attribute = attribute;

					for (int j = 0; j < hb::shared::limits::MaxItems; j++)
					{
						if (game->m_item_order[j] == -1)
						{
							game->m_item_order[j] = i;
							break;
						}
					}
					created++;
					break;
				}
			}
		}

		if (created > 0)
			build_item_manager::get().update_available_recipes();
	}

	void HandleItemLifeSpanEnd(CGame* game, char* data)
	{
		short equip_pos, item_index;
		std::string txt;

		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyItemLifeSpanEnd>(
			data, sizeof(hb::net::PacketNotifyItemLifeSpanEnd));
		if (!pkt) return;
		equip_pos = static_cast<short>(pkt->equip_pos);
		item_index = static_cast<short>(pkt->item_index);
		if (item_index < 0 || item_index >= hb::shared::limits::MaxItems) return;
		if (!game->m_player->m_item_list[item_index]) return;

		auto itemInfo = item_name_formatter::get().format(game->m_player->m_item_list[item_index].get());
		txt = std::format(NOTIFYMSG_ITEMLIFE_SPANEND1, itemInfo.name.c_str());
		game->add_event_list(txt.c_str(), 10);
		game->m_item_equipment_status[game->m_player->m_item_list[item_index]->m_equip_pos] = -1;
		game->m_is_item_equipped[item_index] = false;
		game->m_player->m_item_list[item_index]->m_cur_life_span = 0;

		game->play_game_sound('E', 10, 0);
	}

	void HandleItemReleased(CGame* game, char* data)
	{
		short equip_pos, item_index;
		std::string txt;

		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyItemReleased>(
			data, sizeof(hb::net::PacketNotifyItemReleased));
		if (!pkt) return;
		equip_pos = static_cast<short>(pkt->equip_pos);
		item_index = static_cast<short>(pkt->item_index);
		if (item_index < 0 || item_index >= hb::shared::limits::MaxItems) return;
		if (!game->m_player->m_item_list[item_index]) return;

		auto itemInfo2 = item_name_formatter::get().format(game->m_player->m_item_list[item_index].get());
		txt = std::format(ITEM_EQUIPMENT_RELEASED, itemInfo2.name.c_str());
		game->add_event_list(txt.c_str(), 10);
		game->m_is_item_equipped[item_index] = false;
		game->m_item_equipment_status[game->m_player->m_item_list[item_index]->m_equip_pos] = -1;

		{
			short id = game->m_player->m_item_list[item_index]->m_id_num;
			if (id == hb::shared::item::ItemId::AngelicPandentSTR || id == hb::shared::item::ItemId::AngelicPandentDEX ||
				id == hb::shared::item::ItemId::AngelicPandentINT || id == hb::shared::item::ItemId::AngelicPandentMAG)
				game->play_game_sound('E', 53, 0);
			else
				game->play_game_sound('E', 29, 0);
		}
	}

	void HandleSetItemCount(CGame* game, char* data)
	{
		short  item_index;
		uint64_t count;
		bool   is_item_use_response;
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifySetItemCount>(
			data, sizeof(hb::net::PacketNotifySetItemCount));
		if (!pkt) return;
		item_index = static_cast<short>(pkt->item_index);
		if (item_index < 0 || item_index >= hb::shared::limits::MaxItems) return;
		count = pkt->count;
		is_item_use_response = (pkt->notify != 0);
		if (game->m_player->m_item_list[item_index] != 0)
		{
			game->m_player->m_item_list[item_index]->m_count = count;
			if (is_item_use_response == true) inventory_manager::get().unlock_item(item_index);
		}
	}

	void HandleItemDepleted_EraseItem(CGame* game, char* data)
	{
		short  item_index;
		bool   is_use_item_result;
		std::string txt;

		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyItemDepletedEraseItem>(
			data, sizeof(hb::net::PacketNotifyItemDepletedEraseItem));
		if (!pkt) return;
		item_index = static_cast<short>(pkt->item_index);
		if (item_index < 0 || item_index >= hb::shared::limits::MaxItems) return;
		if (!game->m_player->m_item_list[item_index]) return;
		is_use_item_result = (pkt->use_result != 0);

		auto itemInfo3 = item_name_formatter::get().format(game->m_player->m_item_list[item_index].get());

		CItem* cfg = game->get_item_config(game->m_player->m_item_list[item_index]->m_id_num);

		if (game->m_is_item_equipped[item_index] == true) {
			txt = std::format(ITEM_EQUIPMENT_RELEASED, itemInfo3.name.c_str());
			game->add_event_list(txt.c_str(), 10);

			if (cfg) game->m_item_equipment_status[cfg->m_equip_pos] = -1;
			game->m_is_item_equipped[item_index] = false;
		}

		if (cfg && ((cfg->get_item_type() == ItemType::Consume) ||
			(cfg->get_item_type() == ItemType::Arrow))) {
			txt = std::format(NOTIFYMSG_ITEMDEPlETED_ERASEITEM2, itemInfo3.name.c_str());
		}
		else if (cfg) {
			if (cfg->get_item_type() == ItemType::UseDeplete) {
				if (is_use_item_result == true) {
					txt = std::format(NOTIFYMSG_ITEMDEPlETED_ERASEITEM3, itemInfo3.name.c_str());
				}
			}
			else if (cfg->get_item_type() == ItemType::Eat) {
				if (is_use_item_result == true) {
					txt = std::format(NOTIFYMSG_ITEMDEPlETED_ERASEITEM4, itemInfo3.name.c_str());
					if ((game->m_player->m_player_type >= 1) && (game->m_player->m_player_type <= 3))
						game->play_game_sound('C', 19, 0);
					if ((game->m_player->m_player_type >= 4) && (game->m_player->m_player_type <= 6))
						game->play_game_sound('C', 20, 0);
				}
			}
			else if (cfg->get_item_type() == ItemType::UseDepleteDest) {
				if (is_use_item_result == true) {
					txt = std::format(NOTIFYMSG_ITEMDEPlETED_ERASEITEM3, itemInfo3.name.c_str());
				}
			}
			else {
				if (is_use_item_result == true) {
					txt = std::format(NOTIFYMSG_ITEMDEPlETED_ERASEITEM6, itemInfo3.name.c_str());
					game->play_game_sound('E', 10, 0);
				}
			}
		}
		if (!txt.empty()) game->add_event_list(txt.c_str(), 10);

		if (is_use_item_result == true) game->on_game()->m_item_using_status = false;
		inventory_manager::get().erase_item(static_cast<char>(item_index));
		build_item_manager::get().update_available_recipes();
	}

	void HandleDropItemFin_EraseItem(CGame* game, char* data)
	{
		int amount;
		short  item_index;
		std::string txt;

		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyDropItemFinEraseItem>(
			data, sizeof(hb::net::PacketNotifyDropItemFinEraseItem));
		if (!pkt) return;
		item_index = static_cast<short>(pkt->item_index);
		if (item_index < 0 || item_index >= hb::shared::limits::MaxItems) return;
		if (!game->m_player->m_item_list[item_index]) return;
		amount = static_cast<int>(pkt->amount);

		auto itemInfo4 = item_name_formatter::get().format(game->m_player->m_item_list[item_index].get());

		if (game->m_is_item_equipped[item_index] == true)
		{
			txt = std::format(ITEM_EQUIPMENT_RELEASED, itemInfo4.name.c_str());
			game->add_event_list(txt.c_str(), 10);
			game->m_item_equipment_status[game->m_player->m_item_list[item_index]->m_equip_pos] = -1;
			game->m_is_item_equipped[item_index] = false;
		}
		if (game->m_player->m_hp > 0)
		{
			txt = std::format(NOTIFYMSG_THROW_ITEM2, itemInfo4.name.c_str());
		}
		else
		{
			if (amount < 2)
				txt = std::format(NOTIFYMSG_DROPITEMFIN_ERASEITEM3, itemInfo4.name.c_str());
			else
			{
				txt = std::format(NOTIFYMSG_DROPITEMFIN_ERASEITEM5, itemInfo4.name.c_str());
			}
		}
		game->add_event_list(txt.c_str(), 10);
		inventory_manager::get().erase_item(static_cast<char>(item_index));
		build_item_manager::get().update_available_recipes();
	}

	void HandleGiveItemFin_EraseItem(CGame* game, char* data)
	{
		int amount;
		short  item_index;
		std::string txt;

		char name[hb::shared::limits::ItemNameLen]{};

		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyGiveItemFinEraseItem>(
			data, sizeof(hb::net::PacketNotifyGiveItemFinEraseItem));
		if (!pkt) return;
		item_index = static_cast<short>(pkt->item_index);
		if (item_index < 0 || item_index >= hb::shared::limits::MaxItems) return;
		if (!game->m_player->m_item_list[item_index]) return;
		amount = static_cast<int>(pkt->amount);

		memcpy(name, pkt->name, sizeof(pkt->name));

		CItem* cfg = game->get_item_config(game->m_player->m_item_list[item_index]->m_id_num);
		const char* item_name = cfg ? cfg->m_name : "Unknown";

		if (game->m_is_item_equipped[item_index] == true) {
			txt = std::format(ITEM_EQUIPMENT_RELEASED, item_name);
			game->add_event_list(txt.c_str(), 10);

			if (cfg) game->m_item_equipment_status[cfg->m_equip_pos] = -1;
			game->m_is_item_equipped[item_index] = false;
		}
		if (name[0] == 0) txt = std::format(NOTIFYMSG_GIVEITEMFIN_ERASEITEM2, amount, item_name);
		else {
			if (strcmp(name, "Howard") == 0)
				txt = std::format(NOTIFYMSG_GIVEITEMFIN_ERASEITEM3, amount, item_name);
			else if (strcmp(name, "William") == 0)
				txt = std::format(NOTIFYMSG_GIVEITEMFIN_ERASEITEM4, amount, item_name);
			else if (strcmp(name, "Kennedy") == 0)
				txt = std::format(NOTIFYMSG_GIVEITEMFIN_ERASEITEM5, amount, item_name);
			else if (strcmp(name, "Tom") == 0)
				txt = std::format(NOTIFYMSG_GIVEITEMFIN_ERASEITEM7, amount, item_name);
			else txt = std::format(NOTIFYMSG_GIVEITEMFIN_ERASEITEM8, amount, item_name, name);
		}
		game->add_event_list(txt.c_str(), 10);
		inventory_manager::get().erase_item(static_cast<char>(item_index));
		build_item_manager::get().update_available_recipes();
	}

	void HandleItemRepaired(CGame* game, char* data)
	{
		std::string txt;
		uint32_t item_id, life;

		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyItemRepaired>(
			data, sizeof(hb::net::PacketNotifyItemRepaired));
		if (!pkt) return;
		item_id = pkt->item_id;
		if (item_id >= static_cast<uint32_t>(hb::shared::limits::MaxItems)) return;
		if (!game->m_player->m_item_list[item_id]) return;
		life = pkt->life;

		game->m_player->m_item_list[item_id]->m_cur_life_span = static_cast<uint16_t>(life);
		inventory_manager::get().unlock_item(item_id);
		auto itemInfo5 = item_name_formatter::get().format(game->m_player->m_item_list[item_id].get());

		txt = std::format(NOTIFYMSG_ITEMREPAIRED1, itemInfo5.name.c_str());

		game->add_event_list(txt.c_str(), 10);
		game->get_dialog_box_manager().disable_dialog_box(DialogBoxId::SellOrRepair);
	}

	void HandleRepairItemPrice(CGame* game, char* data)
	{
		char name[hb::shared::limits::ItemNameLen]{};
		uint32_t v1, v2, v3, v4;
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyRepairItemPrice>(
			data, sizeof(hb::net::PacketNotifyRepairItemPrice));
		if (!pkt) return;
		v1 = pkt->v1;
		v2 = pkt->v2;
		v3 = pkt->v3;
		v4 = pkt->v4;
		memcpy(name, pkt->item_name, sizeof(pkt->item_name));
		game->get_dialog_box_manager().enable_dialog_box(DialogBoxId::SellOrRepair, 2, v1, v2);
		if (auto* dlg = game->get_dialog_box_manager().get_dialog_as<DialogBox_SellOrRepair>(DialogBoxId::SellOrRepair))
			dlg->m_secondary_price = v3;
	}

	void HandleRepairAllPrices(CGame* game, char* data)
	{
		int i;

		game->totalPrice = 0;
		const auto* header = hb::net::PacketCast<hb::net::PacketNotifyRepairAllPricesHeader>(
			data, sizeof(hb::net::PacketNotifyRepairAllPricesHeader));
		if (!header) return;
		const auto* entries = reinterpret_cast<const hb::net::PacketNotifyRepairAllPricesEntry*>(
			data + sizeof(hb::net::PacketNotifyRepairAllPricesHeader));
		game->totalItemRepair = (std::min)(static_cast<int>(header->total), hb::shared::limits::MaxItems);

		for (i = 0; i < game->totalItemRepair; i++)
		{
			game->m_repair_all[i].index = entries[i].index;
			game->m_repair_all[i].price = entries[i].price;

			game->totalPrice += game->m_repair_all[i].price;
		}
		if (game->totalItemRepair == 0)
			game->get_dialog_box_manager().enable_dialog_box(DialogBoxId::RepairAll, 1, 0, 0);
		else
			game->get_dialog_box_manager().enable_dialog_box(DialogBoxId::RepairAll, 0, 0, 0);
	}

	void HandleSellItemPrice(CGame* game, char* data)
	{
		char name[hb::shared::limits::ItemNameLen]{};
		uint32_t v1, v2, v3, v4;
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifySellItemPrice>(
			data, sizeof(hb::net::PacketNotifySellItemPrice));
		if (!pkt) return;
		v1 = pkt->v1;
		v2 = pkt->v2;
		v3 = pkt->v3;
		v4 = pkt->v4;
		memcpy(name, pkt->item_name, sizeof(pkt->item_name));
		game->get_dialog_box_manager().enable_dialog_box(DialogBoxId::SellOrRepair, 1, v1, v2);
		if (auto* dlg = game->get_dialog_box_manager().get_dialog_as<DialogBox_SellOrRepair>(DialogBoxId::SellOrRepair))
		{
			dlg->m_secondary_price = v3;
			dlg->m_item_count = v4;
		}
	}

	void HandleCannotRepairItem(CGame* game, char* data)
	{
		std::string txt;


		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyCannotRepairItem>(
			data, sizeof(hb::net::PacketNotifyCannotRepairItem));
		if (!pkt) return;
		const auto v1 = pkt->item_index;
		const auto v2 = pkt->reason;
		if (v1 < 0 || v1 >= hb::shared::limits::MaxItems) return;
		if (!game->m_player->m_item_list[v1]) { inventory_manager::get().unlock_item(v1); return; }
		auto itemInfo6 = item_name_formatter::get().format(game->m_player->m_item_list[v1].get());

		switch (v2) {
		case 1:
			txt = std::format(NOTIFYMSG_CANNOT_REPAIR_ITEM1, itemInfo6.name.c_str());
			game->add_event_list(txt.c_str(), 10);
			break;
		case 2:
			txt = std::format(NOTIFYMSG_CANNOT_REPAIR_ITEM2, itemInfo6.name.c_str());
			game->add_event_list(txt.c_str(), 10);
			break;
		}
		inventory_manager::get().unlock_item(v1);
	}

	void HandleCannotSellItem(CGame* game, char* data)
	{
		std::string txt;


		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyCannotSellItem>(
			data, sizeof(hb::net::PacketNotifyCannotSellItem));
		if (!pkt) return;
		const auto v1 = pkt->item_index;
		const auto v2 = pkt->reason;
		if (v1 < 0 || v1 >= hb::shared::limits::MaxItems) return;
		if (!game->m_player->m_item_list[v1]) { inventory_manager::get().unlock_item(v1); return; }

		auto itemInfo7 = item_name_formatter::get().format(game->m_player->m_item_list[v1].get());

		switch (v2) {
		case 1:
			txt = std::format(NOTIFYMSG_CANNOT_SELL_ITEM1, itemInfo7.name.c_str());
			game->add_event_list(txt.c_str(), 10);
			break;

		case 2:
			txt = std::format(NOTIFYMSG_CANNOT_SELL_ITEM2, itemInfo7.name.c_str());
			game->add_event_list(txt.c_str(), 10);
			break;

		case 3:
			txt = std::format(NOTIFYMSG_CANNOT_SELL_ITEM3, itemInfo7.name.c_str());
			game->add_event_list(txt.c_str(), 10);
			game->add_event_list(NOTIFYMSG_CANNOT_SELL_ITEM4, 10);
			break;

		case 4:
			game->add_event_list(NOTIFYMSG_CANNOT_SELL_ITEM5, 10);
			game->add_event_list(NOTIFYMSG_CANNOT_SELL_ITEM6, 10);
			break;
		}
		inventory_manager::get().unlock_item(v1);
	}

	void HandleCannotGiveItem(CGame* game, char* data)
	{
		std::string txt;

		char name[hb::shared::limits::ItemNameLen]{};

		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyCannotGiveItem>(
			data, sizeof(hb::net::PacketNotifyCannotGiveItem));
		if (!pkt) return;
		const auto item_index = pkt->item_index;
		if (item_index < 0 || item_index >= hb::shared::limits::MaxItems) return;
		if (!game->m_player->m_item_list[item_index]) return;
		const auto amount = static_cast<int>(pkt->amount);
		memcpy(name, pkt->name, sizeof(pkt->name));

		auto itemInfo8 = item_name_formatter::get().format(game->m_player->m_item_list[item_index].get());
		txt = std::format(NOTIFYMSG_CANNOT_GIVE_ITEM2, itemInfo8.name.c_str(), name);

		game->add_event_list(txt.c_str(), 10);
		game->m_is_item_disabled[item_index] = false;
	}

	void HandleItemColorChange(CGame* game, char* data)
	{
		short item_index, item_color;
		std::string txt;

		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyItemColorChange>(
			data, sizeof(hb::net::PacketNotifyItemColorChange));
		if (!pkt) return;
		item_index = static_cast<short>(pkt->item_index);
		if (item_index < 0 || item_index >= hb::shared::limits::MaxItems) return;
		item_color = static_cast<short>(pkt->item_color);

		if (game->m_player->m_item_list[item_index] != 0) {
			auto itemInfo9 = item_name_formatter::get().format(game->m_player->m_item_list[item_index].get());
			if (item_color != -1) {
				game->m_player->m_item_list[item_index]->m_item_color = static_cast<char>(item_color);
				txt = std::format(NOTIFYMSG_ITEMCOLOR_CHANGE1, itemInfo9.name.c_str());
				game->add_event_list(txt.c_str(), 10);
			}
			else {
				txt = std::format(NOTIFYMSG_ITEMCOLOR_CHANGE2, itemInfo9.name.c_str());
				game->add_event_list(txt.c_str(), 10);
			}
		}
	}

	void HandleDropItemFin_CountChanged(CGame* game, char* data)
	{
		std::string txt;

		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyDropItemFinCountChanged>(
			data, sizeof(hb::net::PacketNotifyDropItemFinCountChanged));
		if (!pkt) return;
		const auto item_index = pkt->item_index;
		if (item_index < 0 || item_index >= hb::shared::limits::MaxItems) return;
		if (!game->m_player->m_item_list[item_index]) return;
		const auto amount = static_cast<int>(pkt->amount);

		CItem* cfg = game->get_item_config(game->m_player->m_item_list[item_index]->m_id_num);
		txt = std::format(NOTIFYMSG_THROW_ITEM1, amount, cfg ? cfg->m_name : "Unknown");

		game->add_event_list(txt.c_str(), 10);
	}

	void HandleGiveItemFin_CountChanged(CGame* game, char* data)
	{
		std::string txt;

		char name[hb::shared::limits::ItemNameLen]{};
		uint16_t item_index;
		int amount;

		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyGiveItemFinCountChanged>(
			data, sizeof(hb::net::PacketNotifyGiveItemFinCountChanged));
		if (!pkt) return;
		item_index = pkt->item_index;
		if (item_index >= hb::shared::limits::MaxItems) return;
		if (!game->m_player->m_item_list[item_index]) return;
		amount = static_cast<int>(pkt->amount);

		memcpy(name, pkt->name, sizeof(pkt->name));

		CItem* cfg = game->get_item_config(game->m_player->m_item_list[item_index]->m_id_num);
		if (amount == 1) txt = std::format(NOTIFYMSG_GIVEITEMFIN_COUNTCHANGED1, cfg ? cfg->m_name : "Unknown", name);
		else txt = std::format(NOTIFYMSG_GIVEITEMFIN_COUNTCHANGED2, amount, cfg ? cfg->m_name : "Unknown", name);
		game->add_event_list(txt.c_str(), 10);
	}

	void HandleSetExchangeItem(CGame* game, char* data)
	{
		short dir, cur_life, max_life, performance, item_id;
		int amount, i;
		char color, item_name[hb::shared::limits::ItemNameLen], char_name[12];
		uint32_t attribute;
		std::memset(item_name, 0, sizeof(item_name));

		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyExchangeItem>(
			data, sizeof(hb::net::PacketNotifyExchangeItem));
		if (!pkt) return;
		dir = static_cast<short>(pkt->dir);
		amount = pkt->amount;
		color = static_cast<char>(pkt->color);
		cur_life = pkt->cur_life;
		max_life = pkt->max_life;
		performance = pkt->performance;
		memcpy(item_name, pkt->item_name, sizeof(pkt->item_name));
		memcpy(char_name, pkt->char_name, sizeof(pkt->char_name));
		attribute = pkt->attribute;
		item_id = pkt->item_id;

		auto* exDlg = game->get_dialog_box_manager().get_dialog_as<DialogBox_Exchange>(DialogBoxId::Exchange);
		if (dir >= 1000)  // Set the item I want to exchange
		{
			i = 0;
			while (exDlg->m_slots[i].v1 != -1)
			{
				i++;
				if (i >= 4) return; // Error situation
			}
		}
		else // Set the item he proposes me.
		{
			i = 4;
			while (exDlg->m_slots[i].v1 != -1)
			{
				i++;
				if (i >= 8) return; // Error situation
			}
		}
		exDlg->m_slots[i].v1 = 0; // occupied marker (was sprite)
		exDlg->m_slots[i].v2 = 0; // unused (was sprite_frame)
		exDlg->m_slots[i].v3 = amount;
		exDlg->m_slots[i].v4 = color;
		exDlg->m_slots[i].v5 = static_cast<int>(cur_life);
		exDlg->m_slots[i].v6 = static_cast<int>(max_life);
		exDlg->m_slots[i].v7 = static_cast<int>(performance);
		exDlg->m_slots[i].str1.assign(item_name, strnlen(item_name, hb::shared::limits::ItemNameLen));
		exDlg->m_slots[i].str2.assign(char_name, strnlen(char_name, hb::shared::limits::CharNameLen));
		exDlg->m_slots[i].dw_v1 = attribute;
		exDlg->m_slots[i].item_id = item_id;
	}

	void HandleOpenExchangeWindow(CGame* game, char* data)
	{
		short dir, cur_life, max_life, performance, item_id;
		int amount;
		char color, item_name[hb::shared::limits::ItemNameLen], char_name[12];
		uint32_t attribute;
		std::memset(item_name, 0, sizeof(item_name));

		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyExchangeItem>(
			data, sizeof(hb::net::PacketNotifyExchangeItem));
		if (!pkt) return;
		dir = static_cast<short>(pkt->dir);
		amount = pkt->amount;
		color = static_cast<char>(pkt->color);
		cur_life = pkt->cur_life;
		max_life = pkt->max_life;
		performance = pkt->performance;
		memcpy(item_name, pkt->item_name, sizeof(pkt->item_name));
		memcpy(char_name, pkt->char_name, sizeof(pkt->char_name));
		attribute = pkt->attribute;
		item_id = pkt->item_id;

		game->get_dialog_box_manager().enable_dialog_box(DialogBoxId::Exchange, 1, 0, 0, 0);
		auto* exDlg = game->get_dialog_box_manager().get_dialog_as<DialogBox_Exchange>(DialogBoxId::Exchange);
		// initialize all exchange slots
		for (int j = 0; j < 8; j++)
		{
			exDlg->m_slots[j].v1 = -1;
			exDlg->m_slots[j].v2 = -1;
			exDlg->m_slots[j].v3 = -1;
			exDlg->m_slots[j].v4 = -1;
			exDlg->m_slots[j].v5 = -1;
			exDlg->m_slots[j].v6 = -1;
			exDlg->m_slots[j].v7 = -1;
			exDlg->m_slots[j].item_id = -1;
			exDlg->m_slots[j].inv_slot = -1;
			exDlg->m_slots[j].dw_v1 = 0;
		}
		int i;
		if (dir >= 1000)  // Set the item I want to exchange
		{
			i = 0;
			int inv_idx = dir - 1000;
			if (inv_idx >= 0 && inv_idx < hb::shared::limits::MaxItems)
			{
				inventory_manager::get().lock_item(inv_idx);
				exDlg->m_slots[i].inv_slot = inv_idx;
			}
		}
		else // Set the item he proposes me.
		{
			i = 4;
		}
		exDlg->m_slots[i].v1 = 0; // occupied marker (was sprite)
		exDlg->m_slots[i].v2 = 0; // unused (was sprite_frame)
		exDlg->m_slots[i].v3 = amount;
		exDlg->m_slots[i].v4 = color;
		exDlg->m_slots[i].v5 = static_cast<int>(cur_life);
		exDlg->m_slots[i].v6 = static_cast<int>(max_life);
		exDlg->m_slots[i].v7 = static_cast<int>(performance);
		exDlg->m_slots[i].str1.assign(item_name, strnlen(item_name, hb::shared::limits::ItemNameLen));
		exDlg->m_slots[i].str2.assign(char_name, strnlen(char_name, hb::shared::limits::CharNameLen));
		exDlg->m_slots[i].dw_v1 = attribute;
		exDlg->m_slots[i].item_id = item_id;
	}

	void HandleCurLifeSpan(CGame* game, char* data)
	{
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyCurLifeSpan>(
			data, sizeof(hb::net::PacketNotifyCurLifeSpan));

		if (!pkt)
			return;

		int item_index = pkt->item_index;
		if (item_index < 0 || item_index >= hb::shared::limits::MaxItems) return;

		if (game->m_player->m_item_list[item_index] == nullptr)
			return;

		game->m_player->m_item_list[item_index]->m_cur_life_span = static_cast<uint16_t>(pkt->cur_lifespan);
	}

	void HandleNotEnoughGold(CGame* game, char* data)
	{
		game->get_dialog_box_manager().disable_dialog_box(DialogBoxId::SellOrRepair);
		game->add_event_list(NOTIFY_MSG_HANDLER67, 10);
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyNotEnoughGold>(
			data, sizeof(hb::net::PacketNotifyNotEnoughGold));
		if (!pkt) return;
		if (pkt->item_index >= 0 && pkt->item_index < hb::shared::limits::MaxItems) {
			inventory_manager::get().unlock_item(pkt->item_index);
		}
	}

	void HandleCannotCarryMoreItem(CGame* game, char* data)
	{
		game->add_event_list(NOTIFY_MSG_HANDLER65, 10);
		game->add_event_list(NOTIFY_MSG_HANDLER66, 10);
		// Bank dialog Box
		game->get_dialog_box_manager().get_dialog_as<DialogBox_Bank>(DialogBoxId::Bank)->m_mode = DialogBox_Bank::mode::list;
	}

	void HandleItemAttributeChange(CGame* game, char* data)
	{
		short v1;
		uint32_t temp;
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyItemAttributeChange>(
			data, sizeof(hb::net::PacketNotifyItemAttributeChange));
		if (!pkt) return;
		v1 = static_cast<short>(pkt->item_index);
		if (v1 < 0 || v1 >= hb::shared::limits::MaxItems) return;
		if (!game->m_player->m_item_list[v1]) return;
		temp = game->m_player->m_item_list[v1]->m_attribute;
		game->m_player->m_item_list[v1]->m_attribute = pkt->attribute;
		if (pkt->spec_value1 != 0)
			game->m_player->m_item_list[v1]->m_item_special_effect_value1 = static_cast<short>(pkt->spec_value1);
		if (pkt->spec_value2 != 0)
			game->m_player->m_item_list[v1]->m_item_special_effect_value2 = static_cast<short>(pkt->spec_value2);
		
		if (temp == game->m_player->m_item_list[v1]->m_attribute)
		{
			if (game->get_dialog_box_manager().is_enabled(DialogBoxId::ItemUpgrade) == true)
			{
				game->get_dialog_box_manager().get_dialog_as<DialogBox_ItemUpgrade>(DialogBoxId::ItemUpgrade)->m_mode = DialogBox_ItemUpgrade::mode::failed;
			}
			game->play_game_sound('E', 24, 5);
		}
		else
		{
			if (game->get_dialog_box_manager().is_enabled(DialogBoxId::ItemUpgrade) == true)
			{
				game->get_dialog_box_manager().get_dialog_as<DialogBox_ItemUpgrade>(DialogBoxId::ItemUpgrade)->m_mode = DialogBox_ItemUpgrade::mode::success;
			}
			game->play_game_sound('E', 23, 5);
			switch (game->m_player->m_player_type) {
			case 1:
			case 2:
			case 3:
				game->play_game_sound('C', 21, 0);
				break;
			case 4:
			case 5:
			case 6:
				game->play_game_sound('C', 22, 0);
				break;
			}
		}
	}

	void HandleItemUpgradeFail(CGame* game, char* data)
	{
		short v1;
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyItemUpgradeFail>(
			data, sizeof(hb::net::PacketNotifyItemUpgradeFail));
		if (!pkt) return;
		v1 = static_cast<short>(pkt->reason);
		if (game->get_dialog_box_manager().is_enabled(DialogBoxId::ItemUpgrade) == false) return;
		game->play_game_sound('E', 24, 5);
		switch (v1) {
		case 1:
			game->get_dialog_box_manager().get_dialog_as<DialogBox_ItemUpgrade>(DialogBoxId::ItemUpgrade)->m_mode = DialogBox_ItemUpgrade::mode::max_upgrade;
			break;
		case 2:
			game->get_dialog_box_manager().get_dialog_as<DialogBox_ItemUpgrade>(DialogBoxId::ItemUpgrade)->m_mode = DialogBox_ItemUpgrade::mode::cannot_upgrade;
			break;
		case 3:
			game->get_dialog_box_manager().get_dialog_as<DialogBox_ItemUpgrade>(DialogBoxId::ItemUpgrade)->m_mode = DialogBox_ItemUpgrade::mode::no_points;
			break;
		}
	}

	void HandleGizonItemUpgradeLeft(CGame* game, char* data)
	{
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyGizonItemUpgradeLeft>(
			data, sizeof(hb::net::PacketNotifyGizonItemUpgradeLeft));
		if (!pkt) return;
		game->on_game()->m_gizon_item_upgrade_left = pkt->left;
		switch (pkt->reason) {
		case 1:
			game->add_event_list(NOTIFY_MSG_HANDLER_GIZONITEMUPGRADELEFT1, 10);
			break;
		}
	}

	void HandleGizonItemChange(CGame* game, char* data)
	{
		short v1;
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyGizonItemChange>(
			data, sizeof(hb::net::PacketNotifyGizonItemChange));
		if (!pkt) return;
		v1 = static_cast<short>(pkt->item_index);
		if (v1 < 0 || v1 >= hb::shared::limits::MaxItems) return;
		if (!game->m_player->m_item_list[v1]) return;
		if (pkt->item_id > 0) game->m_player->m_item_list[v1]->m_id_num = pkt->item_id;
		game->m_player->m_item_list[v1]->m_cur_life_span = pkt->cur_lifespan;
		game->m_player->m_item_list[v1]->m_item_color = pkt->item_color;
		game->m_player->m_item_list[v1]->m_item_special_effect_value2 = pkt->spec_value2;
		game->m_player->m_item_list[v1]->m_attribute = pkt->attribute;
		
		if (game->get_dialog_box_manager().is_enabled(DialogBoxId::ItemUpgrade) == true)
		{
			game->get_dialog_box_manager().get_dialog_as<DialogBox_ItemUpgrade>(DialogBoxId::ItemUpgrade)->m_mode = DialogBox_ItemUpgrade::mode::success;
		}
		game->play_game_sound('E', 23, 5);
		switch (game->m_player->m_player_type) {
		case 1:
		case 2:
		case 3:
			game->play_game_sound('C', 21, 0);
			break;
		case 4:
		case 5:
		case 6:
			game->play_game_sound('C', 22, 0);
			break;
		}
	}

	void HandleItemPosList(CGame* game, char* data)
	{
		int i;
		short sX, sY;
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyItemPosList>(
			data, sizeof(hb::net::PacketNotifyItemPosList));
		if (!pkt) return;
		for (i = 0; i < hb::shared::limits::MaxItems; i++) {
			sX = pkt->positions[i * 2];
			sY = pkt->positions[i * 2 + 1];
			if (game->m_player->m_item_list[i] != 0) {
				if (sY < -10) sY = -10;
				if (sX < 0)   sX = 0;
				if (sX > 170) sX = 170;
				if (sY > 95)  sY = 95;

				game->m_player->m_item_list[i]->m_x = sX;
				game->m_player->m_item_list[i]->m_y = sY;
			}
		}
	}

	void HandleItemSold(CGame* game, char* data)
	{
		game->get_dialog_box_manager().disable_dialog_box(DialogBoxId::SellOrRepair);
	}
}

