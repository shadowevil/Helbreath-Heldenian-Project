#include "DialogBox_NpcActionQuery.h"
#include "Game.h"
#include "PacketSendHelpers.h"

#include "InventoryManager.h"
#include "ItemNameFormatter.h"
#include "GlobalDef.h"
#include "lan_eng.h"
#include <format>
#include <string>
#include "IInput.h"

using namespace hb::shared::net;
using namespace hb::shared::item;
using namespace hb::client::sprite_id;

DialogBox_NpcActionQuery::DialogBox_NpcActionQuery(CGame* game)
	: IDialogBox(DialogBoxId::NpcActionQuery, game)
{
	set_default_rect(237 , 57 , 252, 87);
}

void DialogBox_NpcActionQuery::draw_highlighted_text(short sX, short sY, const char* text, short mouse_x, short mouse_y, short hitX1, short hitX2, short hitY1, short hitY2)
{
	if ((mouse_x > hitX1) && (mouse_x < hitX2) && (mouse_y > hitY1) && (mouse_y < hitY2)) {
		put_string(sX, sY, (char*)text, GameColors::UIWhite);
		put_string(sX + 1, sY, (char*)text, GameColors::UIWhite);
	}
	else {
		put_string(sX, sY, (char*)text, GameColors::UIMagicBlue);
		put_string(sX + 1, sY, (char*)text, GameColors::UIMagicBlue);
	}
}

void DialogBox_NpcActionQuery::DrawMode0_NpcMenu(short sX, short sY)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	draw_new_dialog_box(InterfaceNdGame2, sX, sY, 5);

	if (m_action_type == 90) {
		put_string(sX + 33, sY + 23, "Heldenian staff officer", GameColors::UILabel);
		put_string(sX + 33 - 1, sY + 23 - 1, "Heldenian staff officer", GameColors::UIWhite);
	}
	else {
		put_string(sX + 33, sY + 23, m_npc_name, GameColors::UILabel);
		put_string(sX + 33 - 1, sY + 23 - 1, m_npc_name, GameColors::UIWhite);
	}

	if (m_action_type == 25) {
		// OFFER
		draw_highlighted_text(sX + 28, sY + 55, DRAW_DIALOGBOX_NPCACTION_QUERY13, mouse_x, mouse_y, sX + 25, sX + 100, sY + 55, sY + 70);
	}
	else if (m_action_type == 20) {
		// WITHDRAW
		draw_highlighted_text(sX + 28, sY + 55, DRAW_DIALOGBOX_NPCACTION_QUERY17, mouse_x, mouse_y, sX + 25, sX + 100, sY + 55, sY + 70);
	}
	else if (m_action_type == 19) {
		// LEARN
		draw_highlighted_text(sX + 28, sY + 55, DRAW_DIALOGBOX_NPCACTION_QUERY19, mouse_x, mouse_y, sX + 25, sX + 100, sY + 55, sY + 70);
	}
	else {
		// TRADE
		draw_highlighted_text(sX + 28, sY + 55, DRAW_DIALOGBOX_NPCACTION_QUERY21, mouse_x, mouse_y, sX + 25, sX + 100, sY + 55, sY + 70);
	}

	if (m_game->get_dialog_box_manager().is_enabled(DialogBoxId::NpcTalk) == false) {
		draw_highlighted_text(sX + 125, sY + 55, DRAW_DIALOGBOX_NPCACTION_QUERY25, mouse_x, mouse_y, sX + 125, sX + 180, sY + 55, sY + 70);
	}
}

void DialogBox_NpcActionQuery::DrawMode1_GiveToPlayer(short sX, short sY)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	std::string txt, txt2;


	draw_new_dialog_box(InterfaceNdGame2, sX, sY, 6);
	auto itemInfo = item_name_formatter::get().format(player().m_item_list[m_item_index].get());
	txt = std::format("{} to", itemInfo.name);
	txt2 = std::format(DRAW_DIALOGBOX_NPCACTION_QUERY29_1, m_npc_name);

	put_string(sX + 24, sY + 25, txt.c_str(), GameColors::UILabel);
	put_string(sX + 24, sY + 40, txt2.c_str(), GameColors::UILabel);

	draw_highlighted_text(sX + 28, sY + 55, DRAW_DIALOGBOX_NPCACTION_QUERY30, mouse_x, mouse_y, sX + 25, sX + 100, sY + 55, sY + 70);
	draw_highlighted_text(sX + 155, sY + 55, DRAW_DIALOGBOX_NPCACTION_QUERY34, mouse_x, mouse_y, sX + 155, sX + 210, sY + 55, sY + 70);
}

void DialogBox_NpcActionQuery::DrawMode2_SellToShop(short sX, short sY)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	std::string txt, txt2;


	draw_new_dialog_box(InterfaceNdGame2, sX, sY, 5);
	auto itemInfo2 = item_name_formatter::get().format(player().m_item_list[m_item_index].get());

	txt = std::format("{} to", itemInfo2.name);
	txt2 = std::format(DRAW_DIALOGBOX_NPCACTION_QUERY29_1, m_npc_name);

	put_string(sX + 24, sY + 20, txt.c_str(), GameColors::UILabel);
	put_string(sX + 24, sY + 35, txt2.c_str(), GameColors::UILabel);

	draw_highlighted_text(sX + 28, sY + 55, DRAW_DIALOGBOX_NPCACTION_QUERY39, mouse_x, mouse_y, sX + 25, sX + 100, sY + 55, sY + 70);

	CItem* cfg = m_game->get_item_config(player().m_item_list[m_item_index]->m_id_num);
	if (cfg && !cfg->is_stackable() &&
		m_owner_type == hb::shared::owner::Tom)
	{
		draw_highlighted_text(sX + 125, sY + 55, DRAW_DIALOGBOX_NPCACTION_QUERY43, mouse_x, mouse_y, sX + 125, sX + 180, sY + 55, sY + 70);
	}
}

void DialogBox_NpcActionQuery::DrawMode3_DepositToWarehouse(short sX, short sY)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	std::string txt, txt2;


	draw_new_dialog_box(InterfaceNdGame2, sX, sY, 6);
	auto itemInfo3 = item_name_formatter::get().format(player().m_item_list[m_item_index].get());

	txt = std::format("{} to", itemInfo3.name);
	txt2 = std::format(DRAW_DIALOGBOX_NPCACTION_QUERY29_1, m_npc_name);

	put_aligned_string(sX, sX + 240, sY + 20, txt.c_str(), GameColors::UILabel);
	put_aligned_string(sX, sX + 240, sY + 35, txt2.c_str(), GameColors::UILabel);

	draw_highlighted_text(sX + 28, sY + 55, DRAW_DIALOGBOX_NPCACTION_QUERY48, mouse_x, mouse_y, sX + 25, sX + 100, sY + 55, sY + 70);
}

void DialogBox_NpcActionQuery::DrawMode4_TalkToNpcOrUnicorn(short sX, short sY)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	draw_new_dialog_box(InterfaceNdGame2, sX, sY, 5);

	put_string(sX + 35, sY + 25, m_npc_name, GameColors::UILabel);
	put_string(sX + 35 - 1, sY + 25 - 1, m_npc_name, GameColors::UIWhite);

	if (m_game->get_dialog_box_manager().is_enabled(DialogBoxId::NpcTalk) == false) {
		draw_highlighted_text(sX + 125, sY + 55, DRAW_DIALOGBOX_NPCACTION_QUERY25, mouse_x, mouse_y, sX + 125, sX + 180, sY + 55, sY + 70);
	}
}

void DialogBox_NpcActionQuery::DrawMode5_ShopWithSell(short sX, short sY)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	draw_new_dialog_box(InterfaceNdGame2, sX, sY, 6);

	put_string(sX + 33, sY + 23, m_npc_name, GameColors::UILabel);
	put_string(sX + 33 - 1, sY + 23 - 1, m_npc_name, GameColors::UIWhite);

	if (m_action_type == 24) {
		// Repair All button (Blacksmith only)
		draw_highlighted_text(sX + 155, sY + 22, DRAW_DIALOGBOX_NPCACTION_QUERY49, mouse_x, mouse_y, sX + 155, sX + 210, sY + 22, sY + 37);
	}

	// Trade
	draw_highlighted_text(sX + 28, sY + 55, DRAW_DIALOGBOX_NPCACTION_QUERY21, mouse_x, mouse_y, sX + 25, sX + 100, sY + 55, sY + 70);

	// Sell
	draw_highlighted_text(sX + 28 + 75, sY + 55, DRAW_DIALOGBOX_NPCACTION_QUERY39, mouse_x, mouse_y, sX + 25 + 79, sX + 80 + 75, sY + 55, sY + 70);

	if (m_game->get_dialog_box_manager().is_enabled(DialogBoxId::NpcTalk) == false) {
		draw_highlighted_text(sX + 155, sY + 55, DRAW_DIALOGBOX_NPCACTION_QUERY25, mouse_x, mouse_y, sX + 155, sX + 210, sY + 55, sY + 70);
	}
}

void DialogBox_NpcActionQuery::DrawMode6_Gail(short sX, short sY)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	draw_new_dialog_box(InterfaceNdGame2, sX, sY, 5);

	draw_highlighted_text(sX + 28, sY + 55, DRAW_DIALOGBOX_NPCACTION_QUERY21, mouse_x, mouse_y, sX + 25, sX + 100, sY + 55, sY + 70);

	put_string(sX + 33, sY + 23, "Heldenian staff officer", GameColors::UILabel);
	put_string(sX + 33 - 1, sY + 23 - 1, "Heldenian staff officer", GameColors::UIWhite);
}

void DialogBox_NpcActionQuery::on_draw()
{
	if (!m_game->ensure_item_configs_loaded()) return;
	short sX = m_x;
	short sY = m_y;

	switch (m_mode) {
	case mode::npc_menu:              DrawMode0_NpcMenu(sX, sY); break;
	case mode::give_to_player:        DrawMode1_GiveToPlayer(sX, sY); break;
	case mode::sell_to_shop:          DrawMode2_SellToShop(sX, sY); break;
	case mode::deposit_to_warehouse:  DrawMode3_DepositToWarehouse(sX, sY); break;
	case mode::talk_to_npc:           DrawMode4_TalkToNpcOrUnicorn(sX, sY); break;
	case mode::shop_with_sell:        DrawMode5_ShopWithSell(sX, sY); break;
	case mode::gail:                  DrawMode6_Gail(sX, sY); break;
	}
}

bool DialogBox_NpcActionQuery::on_click()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	short sX = m_x;
	short sY = m_y;
	int absX, absY;

	if (m_game->get_dialog_box_manager().is_enabled(DialogBoxId::Exchange) == true) {
		add_event_list(BITEMDROP_SKILLDIALOG1, 10);
		return true;
	}

	switch (m_mode) {
	case mode::npc_menu:
		if ((mouse_x > sX + 25) && (mouse_x < sX + 100) && (mouse_y > sY + 55) && (mouse_y < sY + 70)) {
			enable_dialog_box((DialogBoxId::Type)m_item_index, m_owner_type, 0, 0);
			disable_this_dialog();
			return true;
		}
		if ((m_game->get_dialog_box_manager().is_enabled(DialogBoxId::NpcTalk) == false) && (mouse_x > sX + 125) && (mouse_x < sX + 180) && (mouse_y > sY + 55) && (mouse_y < sY + 70)) {
			switch (m_item_index) {
			case 7:
				{
					auto pkt = hb::net::make_common_command(CommonType::TalkToNpc, player().m_player_x, player().m_player_y);
					pkt.v1 = 1;
					send_game_packet(pkt);
				}
				add_event_list(TALKING_TO_GUILDHALL_OFFICER, 10);
				break;
			case 11:
				switch (m_owner_type) {
				case 1:
					{
						auto pkt = hb::net::make_common_command(CommonType::TalkToNpc, player().m_player_x, player().m_player_y);
						pkt.v1 = 2;
						send_game_packet(pkt);
					}
					add_event_list(TALKING_TO_SHOP_KEEPER, 10);
					break;
				case 2:
					{
						auto pkt = hb::net::make_common_command(CommonType::TalkToNpc, player().m_player_x, player().m_player_y);
						pkt.v1 = 3;
						send_game_packet(pkt);
					}
					add_event_list(TALKING_TO_BLACKSMITH_KEEPER, 10);
					break;
				}
				break;
			case 13:
				{
					auto pkt = hb::net::make_common_command(CommonType::TalkToNpc, player().m_player_x, player().m_player_y);
					pkt.v1 = 4;
					send_game_packet(pkt);
				}
				add_event_list(TALKING_TO_CITYHALL_OFFICER, 10);
				break;
			case 14:
				{
					auto pkt = hb::net::make_common_command(CommonType::TalkToNpc, player().m_player_x, player().m_player_y);
					pkt.v1 = 5;
					send_game_packet(pkt);
				}
				add_event_list(TALKING_TO_WAREHOUSE_KEEPER, 10);
				break;
			case 16:
				{
					auto pkt = hb::net::make_common_command(CommonType::TalkToNpc, player().m_player_x, player().m_player_y);
					pkt.v1 = 6;
					send_game_packet(pkt);
				}
				add_event_list(TALKING_TO_MAGICIAN, 10);
				break;
			}
			disable_this_dialog();
			return true;
		}
		break;

	case mode::give_to_player:
	{
		CItem* cfg = m_game->get_item_config(player().m_item_list[m_item_index]->m_id_num);
		if ((mouse_x > sX + 25) && (mouse_x < sX + 100) && (mouse_y > sY + 55) && (mouse_y < sY + 70)) {
			absX = abs(m_target_x - player().m_player_x);
			absY = abs(m_target_y - player().m_player_y);
			if ((absX <= 4) && (absY <= 4) && cfg)
				{
					auto pkt = hb::net::make_common_command_str(CommonType::GiveItemToChar, player().m_player_x, player().m_player_y, m_item_index);
					pkt.v1 = m_action_type;
					pkt.v2 = m_target_x;
					pkt.v3 = m_target_y;
					std::snprintf(pkt.text, sizeof(pkt.text), "%s", cfg->m_name);
					pkt.v4 = m_object_id;
					send_game_packet(pkt);
				}
			else add_event_list(DLGBOX_CLICK_NPCACTION_QUERY7, 10);
			disable_this_dialog();
			return true;
		}
		else if ((mouse_x > sX + 155) && (mouse_x < sX + 210) && (mouse_y > sY + 55) && (mouse_y < sY + 70)) {
			absX = abs(m_target_x - player().m_player_x);
			absY = abs(m_target_y - player().m_player_y);
			if ((absX <= 4) && (absY <= 4) && cfg)
				{
					auto pkt = hb::net::make_common_command_str(CommonType::ExchangeItemToChar, player().m_player_x, player().m_player_y, m_item_index);
					pkt.v1 = m_action_type;
					pkt.v2 = m_target_x;
					pkt.v3 = m_target_y;
					std::snprintf(pkt.text, sizeof(pkt.text), "%s", cfg->m_name);
					pkt.v4 = m_object_id;
					send_game_packet(pkt);
				}
			else add_event_list(DLGBOX_CLICK_NPCACTION_QUERY8, 10);
			disable_this_dialog();
			return true;
		}
		break;
	}

	case mode::sell_to_shop:
	{
		CItem* cfg = m_game->get_item_config(player().m_item_list[m_item_index]->m_id_num);
		if ((mouse_x > sX + 25) && (mouse_x < sX + 100) && (mouse_y > sY + 55) && (mouse_y < sY + 70)) {
			// Can't sell gold
			if (player().m_item_list[m_item_index]->m_id_num == ItemId::Gold)
			{
				add_event_list(BITEMDROP_SELLLIST2, 10);
				disable_this_dialog();
				return true;
			}
			if (cfg)
			{
				auto pkt = hb::net::make_common_command_str(CommonType::ReqSellItem, player().m_player_x, player().m_player_y);
				pkt.v1 = m_item_index;
				pkt.v2 = m_owner_type;
				pkt.v3 = m_action_type;
				std::snprintf(pkt.text, sizeof(pkt.text), "%s", cfg->m_name);
				pkt.v4 = m_object_id;
				send_game_packet(pkt);
			}
			disable_this_dialog();
			return true;
		}
		else if ((mouse_x > sX + 125) && (mouse_x < sX + 180) && (mouse_y > sY + 55) && (mouse_y < sY + 70)) {
			if (m_action_type == 1) {
				if (cfg)
				{
					auto pkt = hb::net::make_common_command_str(CommonType::ReqRepairItem, player().m_player_x, player().m_player_y);
					pkt.v1 = m_item_index;
					pkt.v2 = m_owner_type;
					std::snprintf(pkt.text, sizeof(pkt.text), "%s", cfg->m_name);
					pkt.v4 = m_object_id;
					send_game_packet(pkt);
				}
				disable_this_dialog();
				return true;
			}
		}
		break;
	}

	case mode::deposit_to_warehouse:
	{
		CItem* cfg = m_game->get_item_config(player().m_item_list[m_item_index]->m_id_num);
		if ((mouse_x > sX + 25) && (mouse_x < sX + 105) && (mouse_y > sY + 55) && (mouse_y < sY + 70)) {
			absX = abs(m_target_x - player().m_player_x);
			absY = abs(m_target_y - player().m_player_y);
			if ((absX <= 8) && (absY <= 8)) {
				if (inventory_manager::get().get_bank_item_count() >= (m_game->m_max_bank_items - 1)) {
					add_event_list(DLGBOX_CLICK_NPCACTION_QUERY9, 10);
				}
				else if (cfg)
				{
					auto pkt = hb::net::make_common_command_str(CommonType::GiveItemToChar, player().m_player_x, player().m_player_y, m_item_index);
					pkt.v1 = m_action_type;
					pkt.v2 = m_target_x;
					pkt.v3 = m_target_y;
					std::snprintf(pkt.text, sizeof(pkt.text), "%s", cfg->m_name);
					pkt.v4 = m_object_id;
					send_game_packet(pkt);
				}
			}
			else add_event_list(DLGBOX_CLICK_NPCACTION_QUERY7, 10);
			disable_this_dialog();
			return true;
		}
		break;
	}

	case mode::talk_to_npc:
		if ((m_game->get_dialog_box_manager().is_enabled(DialogBoxId::NpcTalk) == false) && (mouse_x > sX + 125) && (mouse_x < sX + 180) && (mouse_y > sY + 55) && (mouse_y < sY + 70)) {
			switch (m_action_type) {
			case 21:
				{
					auto pkt = hb::net::make_common_command(CommonType::TalkToNpc, player().m_player_x, player().m_player_y);
					pkt.v1 = 21;
					send_game_packet(pkt);
				}
				add_event_list(TALKING_TO_GUARD, 10);
				break;
			case 32:
				{
					auto pkt = hb::net::make_common_command(CommonType::TalkToNpc, player().m_player_x, player().m_player_y);
					pkt.v1 = 32;
					send_game_packet(pkt);
				}
				add_event_list(TALKING_TO_UNICORN, 10);
				break;
			case 67:
				{
					auto pkt = hb::net::make_common_command(CommonType::TalkToNpc, player().m_player_x, player().m_player_y);
					pkt.v1 = 67;
					send_game_packet(pkt);
				}
				add_event_list(TALKING_TO_MCGAFFIN, 10);
				break;
			case 68:
				{
					auto pkt = hb::net::make_common_command(CommonType::TalkToNpc, player().m_player_x, player().m_player_y);
					pkt.v1 = 68;
					send_game_packet(pkt);
				}
				add_event_list(TALKING_TO_PERRY, 10);
				break;
			case 69:
				{
					auto pkt = hb::net::make_common_command(CommonType::TalkToNpc, player().m_player_x, player().m_player_y);
					pkt.v1 = 69;
					send_game_packet(pkt);
				}
				add_event_list(TALKING_TO_DEVLIN, 10);
				break;
			}
		}
		disable_this_dialog();
		return true;

	case mode::shop_with_sell:
		if ((mouse_x > sX + 25) && (mouse_x < sX + 100) && (mouse_y > sY + 55) && (mouse_y < sY + 70)) {
			enable_dialog_box((DialogBoxId::Type)m_item_index, m_owner_type, 0, 0);
			disable_this_dialog();
			return true;
		}
		if ((mouse_x > sX + 25 + 75) && (mouse_x < sX + 80 + 75) && (mouse_y > sY + 55) && (mouse_y < sY + 70)) {
			enable_dialog_box(DialogBoxId::SellList, 0, 0, 0);
			disable_this_dialog();
			return true;
		}
		if ((m_game->get_dialog_box_manager().is_enabled(DialogBoxId::NpcTalk) == false) && (mouse_x > sX + 155) && (mouse_x < sX + 210) && (mouse_y > sY + 55) && (mouse_y < sY + 70)) {
			switch (m_item_index) {
			case 7:
				{
					auto pkt = hb::net::make_common_command(CommonType::TalkToNpc, player().m_player_x, player().m_player_y);
					pkt.v1 = 1;
					send_game_packet(pkt);
				}
				add_event_list(TALKING_TO_GUILDHALL_OFFICER, 10);
				break;
			case 11:
				switch (m_owner_type) {
				case 1:
					{
						auto pkt = hb::net::make_common_command(CommonType::TalkToNpc, player().m_player_x, player().m_player_y);
						pkt.v1 = 2;
						send_game_packet(pkt);
					}
					add_event_list(TALKING_TO_SHOP_KEEPER, 10);
					break;
				case 2:
					{
						auto pkt = hb::net::make_common_command(CommonType::TalkToNpc, player().m_player_x, player().m_player_y);
						pkt.v1 = 3;
						send_game_packet(pkt);
					}
					add_event_list(TALKING_TO_BLACKSMITH_KEEPER, 10);
					break;
				}
				break;
			case 13:
				{
					auto pkt = hb::net::make_common_command(CommonType::TalkToNpc, player().m_player_x, player().m_player_y);
					pkt.v1 = 4;
					send_game_packet(pkt);
				}
				add_event_list(TALKING_TO_CITYHALL_OFFICER, 10);
				break;
			case 14:
				{
					auto pkt = hb::net::make_common_command(CommonType::TalkToNpc, player().m_player_x, player().m_player_y);
					pkt.v1 = 5;
					send_game_packet(pkt);
				}
				add_event_list(TALKING_TO_WAREHOUSE_KEEPER, 10);
				break;
			case 16:
				{
					auto pkt = hb::net::make_common_command(CommonType::TalkToNpc, player().m_player_x, player().m_player_y);
					pkt.v1 = 6;
					send_game_packet(pkt);
				}
				add_event_list(TALKING_TO_MAGICIAN, 10);
				break;
			}
			disable_this_dialog();
			return true;
		}
		// Repair All
		if ((mouse_x > sX + 155) && (mouse_x < sX + 210) && (mouse_y > sY + 22) && (mouse_y < sY + 37)) {
			if (m_action_type == 24) {
				send_game_packet(hb::net::make_common_command(CommonType::ReqRepairAll, player().m_player_x, player().m_player_y));
				disable_this_dialog();
				return true;
			}
		}
		break;

	case mode::gail:
		if ((mouse_x > sX + 25) && (mouse_x < sX + 100) && (mouse_y > sY + 55) && (mouse_y < sY + 70)) {
			enable_dialog_box(DialogBoxId::CommandHallMenu, 0, 0, 0);
			disable_this_dialog();
			return true;
		}
		break;
	}

	return false;
}

bool DialogBox_NpcActionQuery::on_enable(int type, int64_t v1, int v2, const char* string)
{
	// Clear previously disabled item
	{ int idx = m_item_index;
	inventory_manager::get().unlock_item(idx); }
	if (!is_enabled())
	{
		m_mode = static_cast<mode>(type);
		m_item_index = static_cast<int>(v1);
		m_owner_type = v2;
	}
	return true;
}

bool DialogBox_NpcActionQuery::on_disable()
{
	{ int idx = m_item_index;
	inventory_manager::get().unlock_item(idx); }
	return true;
}

void DialogBox_NpcActionQuery::enable_with_target(int mode, int64_t item_id, int owner_type,
	int action_type, int object_id,
	int target_x, int target_y,
	const char* npc_name)
{
	m_game->get_dialog_box_manager().enable_dialog_box(DialogBoxId::NpcActionQuery, mode, item_id, owner_type);
	m_action_type = action_type;
	m_object_id = object_id;
	m_target_x = target_x;
	m_target_y = target_y;
	std::snprintf(m_npc_name, sizeof(m_npc_name), "%s", npc_name);
}
