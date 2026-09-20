#include "DialogBox_RepairAll.h"
#include "Game.h"
#include "PacketSendHelpers.h"

#include "InventoryManager.h"
#include "IInput.h"
#include <format>
#include <string>

using namespace hb::shared::net;
using namespace hb::client::sprite_id;
DialogBox_RepairAll::DialogBox_RepairAll(CGame* game)
	: IDialogBox(DialogBoxId::RepairAll, game)
{
	set_default_rect(497 , 57 , 258, 339);
}

void DialogBox_RepairAll::on_draw()
{
	short z = static_cast<short>(hb::shared::input::get_mouse_wheel_delta());
	if (!m_game->ensure_item_configs_loaded()) return;
	short sX = m_x;
	short sY = m_y;
	short size_x = m_size_x;
	std::string txt;
	int total_lines, pointer_loc;
	double d1, d2, d3;

	draw_new_dialog_box(InterfaceNdGame2, sX, sY, 2);
	draw_new_dialog_box(InterfaceNdText, sX, sY, 10);

	for (int i = 0; i < 15; i++)
	{
		if ((i + m_scroll_offset) < m_game->totalItemRepair)
		{
			int idx = m_game->m_repair_all[i + m_scroll_offset].index;
			auto* item = player().m_item_list[idx].get();
			if (!item) continue;
			CItem* cfg = m_game->get_item_config(item->m_id_num);
			txt = std::format("{} - Cost: {}", cfg ? cfg->m_name : "Unknown", m_game->m_repair_all[i + m_scroll_offset].price);

			put_string(sX + 30, sY + 45 + i * 15, txt.c_str(), GameColors::UIBlack);
		}
	}

	total_lines = m_game->totalItemRepair;
	if (total_lines > 15)
	{
		d1 = static_cast<double>(m_scroll_offset);
		d2 = static_cast<double>(total_lines - 15);
		d3 = (274.0f * d1) / d2;
		pointer_loc = static_cast<int>(d3);
	}
	else
	{
		pointer_loc = 0;
	}

	if (total_lines > 15)
	{
		draw_new_dialog_box(InterfaceNdGame2, sX, sY, 1);
		draw_new_dialog_box(InterfaceNdGame2, sX + 242, sY + pointer_loc + 35, 7);
	}

	// Mouse wheel scrolling
	if (total_lines > 15)
	{
		if (m_game->get_dialog_box_manager().get_top_id() == DialogBoxId::RepairAll && z != 0)
		{
			if (z > 0) m_scroll_offset--;
			if (z < 0) m_scroll_offset++;
		}

		if (m_scroll_offset < 0)
			m_scroll_offset = 0;

		if (total_lines > 15 && m_scroll_offset > total_lines - 15)
			m_scroll_offset = total_lines - 15;
	}

	if (m_game->totalItemRepair > 0)
	{
		// Repair button
		if (mouse_in(btn_repair))
			draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::left_btn_x, sY + ui_layout::btn_y, 43);
		else
			draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::left_btn_x, sY + ui_layout::btn_y, 42);

		// Cancel button
		if (mouse_in(btn_cancel))
			draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 17);
		else
			draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 16);

		// Total cost
		txt = std::format("Total cost : {}", m_game->totalPrice);
		put_string(sX + 30, sY + 270, txt.c_str(), GameColors::UIBlack);
	}
	else
	{
		// No items to repair
		put_aligned_string(sX, sX + size_x, sY + 140, "There are no items to repair.", GameColors::UIBlack);

		// Cancel button only
		if (mouse_in(btn_cancel))
			draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 17);
		else
			draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 16);
	}
}

bool DialogBox_RepairAll::on_click()
{
	if (m_game->totalItemRepair > 0)
	{
		// Repair button
		if (mouse_in(btn_repair))
		{
			send_game_packet(hb::net::make_common_command(CommonType::ReqRepairAllConfirm, player().m_player_x, player().m_player_y));
			disable_this_dialog();
			return true;
		}
	}

	// Cancel button
	if (mouse_in(btn_cancel))
	{
		disable_this_dialog();
		return true;
	}

	return false;
}

bool DialogBox_RepairAll::on_enable(int type, int64_t v1, int v2, const char* string)
{
	m_mode = type;
	m_locked_by_us.fill(false);
	for (int i = 0; i < m_game->totalItemRepair; i++)
	{
		int idx = m_game->m_repair_all[i].index;
		if (inventory_manager::get().try_lock_item(idx))
			m_locked_by_us[idx] = true;
	}
	return true;
}

bool DialogBox_RepairAll::on_disable()
{
	for (int i = 0; i < m_game->totalItemRepair; i++)
	{
		int idx = m_game->m_repair_all[i].index;
		if (m_locked_by_us[idx])
		{
			inventory_manager::get().unlock_item(idx);
			m_locked_by_us[idx] = false;
		}
	}
	return true;
}
