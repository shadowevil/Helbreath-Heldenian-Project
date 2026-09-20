#include "DialogBox_ItemDrop.h"
#include "Game.h"
#include "PacketSendHelpers.h"

#include "InventoryManager.h"
#include "ItemNameFormatter.h"
#include "lan_eng.h"
#include <string>
#include "IInput.h"

using namespace hb::shared::net;
using namespace hb::client::sprite_id;
DialogBox_ItemDrop::DialogBox_ItemDrop(CGame* game)
	: IDialogBox(DialogBoxId::ItemDropConfirm, game)
{
	set_default_rect(0 , 0 , 270, 105);
}

void DialogBox_ItemDrop::on_draw()
{
	if (!m_game->ensure_item_configs_loaded()) return;
	short sX = m_x;
	short sY = m_y;
	std::string txt;

	draw_new_dialog_box(InterfaceNdGame1, sX, sY, 2);

	auto itemInfo = item_name_formatter::get().format(player().m_item_list[m_item_index].get());

	if (m_name[0] == '\0')
		txt = itemInfo.name.c_str();

	// Item name (green if special, blue otherwise)
	if (itemInfo.is_special)
	{
		put_string(sX + 35, sY + 20, txt.c_str(), GameColors::UIItemName_Special);
		put_string(sX + 36, sY + 20, txt.c_str(), GameColors::UIItemName_Special);
	}
	else
	{
		put_string(sX + 35, sY + 20, txt.c_str(), GameColors::UIMagicBlue);
		put_string(sX + 36, sY + 20, txt.c_str(), GameColors::UIMagicBlue);
	}

	// "Do you want to drop?" text
	put_string(sX + 35, sY + 36, DRAW_DIALOGBOX_ITEM_DROP1, GameColors::UIMagicBlue);
	put_string(sX + 36, sY + 36, DRAW_DIALOGBOX_ITEM_DROP1, GameColors::UIMagicBlue);

	// toggle option text
	if (m_game->m_item_drop)
	{
		if (mouse_in(link_toggle))
		{
			put_string(sX + 35, sY + 80, DRAW_DIALOGBOX_ITEM_DROP2, GameColors::UIWhite);
			put_string(sX + 36, sY + 80, DRAW_DIALOGBOX_ITEM_DROP2, GameColors::UIWhite);
		}
		else
		{
			put_string(sX + 35, sY + 80, DRAW_DIALOGBOX_ITEM_DROP2, GameColors::UIMagicBlue);
			put_string(sX + 36, sY + 80, DRAW_DIALOGBOX_ITEM_DROP2, GameColors::UIMagicBlue);
		}
	}
	else
	{
		if (mouse_in(link_toggle))
		{
			put_string(sX + 35, sY + 80, DRAW_DIALOGBOX_ITEM_DROP3, GameColors::UIWhite);
			put_string(sX + 36, sY + 80, DRAW_DIALOGBOX_ITEM_DROP3, GameColors::UIWhite);
		}
		else
		{
			put_string(sX + 35, sY + 80, DRAW_DIALOGBOX_ITEM_DROP3, GameColors::UIMagicBlue);
			put_string(sX + 36, sY + 80, DRAW_DIALOGBOX_ITEM_DROP3, GameColors::UIMagicBlue);
		}
	}

	// Yes button
	if (mouse_in(btn_yes))
		draw_new_dialog_box(InterfaceNdButton, sX + 30, sY + 55, 19);
	else
		draw_new_dialog_box(InterfaceNdButton, sX + 30, sY + 55, 18);

	// No button
	if (mouse_in(btn_no))
		draw_new_dialog_box(InterfaceNdButton, sX + 170, sY + 55, 3);
	else
		draw_new_dialog_box(InterfaceNdButton, sX + 170, sY + 55, 2);
}

bool DialogBox_ItemDrop::on_click()
{
	if (player().m_Controller.get_command() < 0) return false;

	// Yes button - drop item
	if (mouse_in(btn_yes))
	{
		m_mode = 3;
		CItem* cfg = m_game->get_item_config(player().m_item_list[m_item_index]->m_id_num);
		if (cfg)
		{
			auto pkt = hb::net::make_common_command_str(CommonType::ItemDrop, player().m_player_x, player().m_player_y);
			pkt.v1 = m_item_index;
			pkt.v2 = 1;
			std::snprintf(pkt.text, sizeof(pkt.text), "%s", cfg->m_name);
			send_game_packet(pkt);
		}
		disable_this_dialog();
		return true;
	}

	// No button - cancel
	if (mouse_in(btn_no))
	{
		disable_this_dialog();
		return true;
	}

	// toggle "don't show again" option
	if (mouse_in(link_toggle))
	{
		m_game->m_item_drop = !m_game->m_item_drop;
		return true;
	}

	return false;
}

bool DialogBox_ItemDrop::on_enable(int type, int64_t v1, int v2, const char* string)
{
	if (is_enabled()) return true;
	m_item_index = type;
	return true;
}

bool DialogBox_ItemDrop::on_disable()
{
	{ int idx = m_item_index;
	inventory_manager::get().unlock_item(idx); }
	return true;
}
