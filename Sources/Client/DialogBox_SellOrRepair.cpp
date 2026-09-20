#include "DialogBox_SellOrRepair.h"
#include "Game.h"
#include "InventoryManager.h"
#include "ItemNameFormatter.h"
#include "ItemSpriteMetadata.h"
#include "GlobalDef.h"
#include "SpriteID.h"
#include "lan_eng.h"
#include "NetMessages.h"
#include "PacketSendHelpers.h"

#include <format>
#include <string>
#include "IInput.h"

using namespace hb::shared::net;
using namespace hb::client::sprite_id;
using hb::shared::item::EquipPos;
using hb::shared::item::to_int;
DialogBox_SellOrRepair::DialogBox_SellOrRepair(CGame* game)
	: IDialogBox(DialogBoxId::SellOrRepair, game)
{
	set_default_rect(497 , 57 , 258, 339);
}

void DialogBox_SellOrRepair::on_draw()
{
	if (!m_game->ensure_item_configs_loaded()) return;
	short sX, sY;
	uint32_t time = m_game->m_cur_time;
	std::string txt;

	int item_id;
	char item_color;

	sX = m_x;
	sY = m_y;

	switch (m_mode) {
	case mode::sell:
	{
		draw_new_dialog_box(InterfaceNdGame2, sX, sY, 2);
		draw_new_dialog_box(InterfaceNdText, sX, sY, 11);

		item_id = m_item_index;

		item_color = player().m_item_list[item_id]->m_instance.item_color;
		{
			CItem* sell_cfg = m_game->get_item_config(player().m_item_list[item_id]->m_id_num);
			auto sell_draw = m_game->get_item_draw(sell_cfg ? sell_cfg->m_display_id : 0, item_atlas::pack, sell_cfg ? sell_cfg->sprite_is_female() : false);
			if (item_color == 0)
				sell_draw.sprite->draw(sX + 62 + 15, sY + 84 + 30, sell_draw.frame);
			else
			{
				const auto& sell_tint = m_game->m_color_palette[item_color];
				sell_draw.sprite->draw(sX + 62 + 15, sY + 84 + 30, sell_draw.frame, hb::shared::sprite::DrawParams::tint(sell_tint.r, sell_tint.g, sell_tint.b));
			}
		}

		auto itemInfo = item_name_formatter::get().format(player().m_item_list[item_id].get());
		txt = itemInfo.name;

		if (itemInfo.is_special)
		{
			put_aligned_string(sX + 25, sX + 240, sY + 60, txt.c_str(), GameColors::UIItemName_Special);
			put_aligned_string(sX + 25 + 1, sX + 240 + 1, sY + 60, txt.c_str(), GameColors::UIItemName_Special);
		}
		else
		{
			put_aligned_string(sX + 25, sX + 240, sY + 60, txt.c_str(), GameColors::UILabel);
			put_aligned_string(sX + 25 + 1, sX + 240 + 1, sY + 60, txt.c_str(), GameColors::UILabel);
		}

		txt = std::format(DRAW_DIALOGBOX_SELLOR_REPAIR_ITEM2, m_sell_price);
		put_string(sX + 95 + 15, sY + 53 + 60, txt.c_str(), GameColors::UILabel);
		txt = std::format(DRAW_DIALOGBOX_SELLOR_REPAIR_ITEM3, m_secondary_price);
		put_string(sX + 95 + 15, sY + 53 + 75, txt.c_str(), GameColors::UILabel);
		put_string(sX + 55, sY + 190, DRAW_DIALOGBOX_SELLOR_REPAIR_ITEM4, GameColors::UILabel);

		if (mouse_in(btn_confirm))
			draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::left_btn_x, sY + ui_layout::btn_y, 39);
		else draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::left_btn_x, sY + ui_layout::btn_y, 38);

		if (mouse_in(btn_cancel))
			draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 17);
		else draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 16);
		break;
	}

	case mode::repair:
	{
		draw_new_dialog_box(InterfaceNdGame2, sX, sY, 2);
		draw_new_dialog_box(InterfaceNdText, sX, sY, 10);
		item_id = m_item_index;
		item_color = player().m_item_list[item_id]->m_instance.item_color;
		{
			CItem* rep_cfg = m_game->get_item_config(player().m_item_list[item_id]->m_id_num);
			auto rep_draw = m_game->get_item_draw(rep_cfg ? rep_cfg->m_display_id : 0, item_atlas::pack, rep_cfg ? rep_cfg->sprite_is_female() : false);
			if (item_color == 0)
				rep_draw.sprite->draw(sX + 62 + 15, sY + 84 + 30, rep_draw.frame);
			else
			{
				const auto& rep_tint = m_game->m_color_palette[item_color];
				rep_draw.sprite->draw(sX + 62 + 15, sY + 84 + 30, rep_draw.frame, hb::shared::sprite::DrawParams::tint(rep_tint.r, rep_tint.g, rep_tint.b));
			}
		}
		auto itemInfo2 = item_name_formatter::get().format(player().m_item_list[item_id].get());
		txt = itemInfo2.name.c_str();
		if (itemInfo2.is_special)
		{
			put_aligned_string(sX + 25, sX + 240, sY + 60, txt.c_str(), GameColors::UIItemName_Special);
			put_aligned_string(sX + 25 + 1, sX + 240 + 1, sY + 60, txt.c_str(), GameColors::UIItemName_Special);
		}
		else
		{
			put_aligned_string(sX + 25, sX + 240, sY + 60, txt.c_str(), GameColors::UILabel);
			put_aligned_string(sX + 25 + 1, sX + 240 + 1, sY + 60, txt.c_str(), GameColors::UILabel);
		}
		txt = std::format(DRAW_DIALOGBOX_SELLOR_REPAIR_ITEM2, m_sell_price);
		put_string(sX + 95 + 15, sY + 53 + 60, txt.c_str(), GameColors::UILabel);
		txt = std::format(DRAW_DIALOGBOX_SELLOR_REPAIR_ITEM6, m_secondary_price);
		put_string(sX + 95 + 15, sY + 53 + 75, txt.c_str(), GameColors::UILabel);
		put_string(sX + 55, sY + 190, DRAW_DIALOGBOX_SELLOR_REPAIR_ITEM7, GameColors::UILabel);

		if (mouse_in(btn_confirm))
			draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::left_btn_x, sY + ui_layout::btn_y, 43);
		else draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::left_btn_x, sY + ui_layout::btn_y, 42);

		if (mouse_in(btn_cancel))
			draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 17);
		else draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 16);
		break;
	}

	case mode::sell_pending:
		draw_new_dialog_box(InterfaceNdGame2, sX, sY, 2);
		draw_new_dialog_box(InterfaceNdText, sX, sY, 11);

		put_string(sX + 55, sY + 100, DRAW_DIALOGBOX_SELLOR_REPAIR_ITEM8, GameColors::UILabel);
		put_string(sX + 55, sY + 120, DRAW_DIALOGBOX_SELLOR_REPAIR_ITEM9, GameColors::UILabel);
		put_string(sX + 55, sY + 135, DRAW_DIALOGBOX_SELLOR_REPAIR_ITEM10, GameColors::UILabel);
		break;

	case mode::repair_pending:
		draw_new_dialog_box(InterfaceNdGame2, sX, sY, 2);
		draw_new_dialog_box(InterfaceNdText, sX, sY, 10);

		put_string(sX + 55, sY + 100, DRAW_DIALOGBOX_SELLOR_REPAIR_ITEM11, GameColors::UILabel);
		put_string(sX + 55, sY + 120, DRAW_DIALOGBOX_SELLOR_REPAIR_ITEM9, GameColors::UILabel);
		put_string(sX + 55, sY + 135, DRAW_DIALOGBOX_SELLOR_REPAIR_ITEM10, GameColors::UILabel);
		break;
	}
}

bool DialogBox_SellOrRepair::on_click()
{
	short sX, sY;

	sX = m_x;
	sY = m_y;

	switch (m_mode) {
	case mode::sell:
	{
		CItem* cfg = m_game->get_item_config(player().m_item_list[m_item_index]->m_id_num);
		if (mouse_in(btn_confirm)) {
			// Sell
			if (cfg)
			{
				auto pkt = hb::net::make_common_command_str(CommonType::ReqSellItemConfirm, m_game->m_player->m_player_x, m_game->m_player->m_player_y);
				pkt.v1 = m_item_index;
				pkt.v2 = m_item_count;
				pkt.v3 = m_secondary_price;
				std::snprintf(pkt.text, sizeof(pkt.text), "%s", cfg->m_name);
				m_game->send_game_packet(pkt);
			}
			m_mode = mode::sell_pending;
			return true;
		}
		if (mouse_in(btn_cancel)) {
			// Cancel
			inventory_manager::get().unlock_item(m_item_index);
			m_game->get_dialog_box_manager().disable_dialog_box(DialogBoxId::SellOrRepair);
			return true;
		}
		break;
	}

	case mode::repair:
	{
		CItem* cfg = m_game->get_item_config(player().m_item_list[m_item_index]->m_id_num);
		if (mouse_in(btn_confirm)) {
			// Repair
			if (cfg)
			{
				auto pkt = hb::net::make_common_command_str(CommonType::ReqRepairItemConfirm, m_game->m_player->m_player_x, m_game->m_player->m_player_y);
				pkt.v1 = m_item_index;
				std::snprintf(pkt.text, sizeof(pkt.text), "%s", cfg->m_name);
				m_game->send_game_packet(pkt);
			}
			m_mode = mode::repair_pending;
			return true;
		}
		if (mouse_in(btn_cancel)) {
			// Cancel
			inventory_manager::get().unlock_item(m_item_index);
			m_game->get_dialog_box_manager().disable_dialog_box(DialogBoxId::SellOrRepair);
			return true;
		}
		break;
	}
	}

	return false;
}

bool DialogBox_SellOrRepair::on_enable(int type, int64_t v1, int v2, const char* string)
{
	if (is_enabled()) return true;
	m_mode = static_cast<mode>(type);
	m_item_index = static_cast<int>(v1);
	m_sell_price = v2;
	if (type == 2)
	{
		auto* saleDlg = get_dialog_box(DialogBoxId::SaleMenu);
		if (saleDlg) { m_x = saleDlg->m_x; m_y = saleDlg->m_y; }
	}
	return true;
}

bool DialogBox_SellOrRepair::on_disable()
{
	inventory_manager::get().unlock_item(m_item_index);
	return true;
}
