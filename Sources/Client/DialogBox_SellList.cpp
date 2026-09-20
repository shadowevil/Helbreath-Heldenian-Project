#include "DialogBox_SellList.h"
#include "CursorTarget.h"
#include "DialogBox_ItemDropAmount.h"
#include "Game.h"
#include "InventoryManager.h"
#include "ItemNameFormatter.h"
#include "GlobalDef.h"
#include "lan_eng.h"
#include <format>
#include <string>
#include "IInput.h"
#include "Packet/SharedPackets.h"
#include "AudioManager.h"

using namespace hb::shared::net;
using namespace hb::shared::item;
using namespace hb::client::sprite_id;

DialogBox_SellList::DialogBox_SellList(CGame* game)
	: IDialogBox(DialogBoxId::SellList, game)
{
	set_default_rect(170 , 70 , 258, 339);
}

void DialogBox_SellList::on_draw()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	if (!m_game->ensure_item_configs_loaded()) return;
	short sX = m_x;
	short sY = m_y;
	short size_x = m_size_x;

	draw_new_dialog_box(InterfaceNdGame2, sX, sY, 2);
	draw_new_dialog_box(InterfaceNdText, sX, sY, 11);

	int empty_count = 0;
	draw_item_list(sX, sY, size_x, mouse_x, mouse_y, empty_count);

	if (empty_count == game_limits::max_sell_list) {
		draw_empty_list_message(sX, sY, size_x);
	}

	bool has_items = (empty_count < game_limits::max_sell_list);
	draw_buttons(sX, sY, mouse_x, mouse_y, has_items);
}

void DialogBox_SellList::draw_item_list(short sX, short sY, short size_x, short mouse_x, short mouse_y, int& empty_count)
{
	std::string txt;
	int row = 0;

	for (int i = 0; i < game_limits::max_sell_list; i++)
	{
		if (m_items[i].index != -1)
		{
			int item_index = m_items[i].index;
			auto itemInfo = item_name_formatter::get().format(player().m_item_list[item_index].get());
			auto effect = itemInfo.effect_text();
			auto extra = itemInfo.extra_text();

			bool hover = (mouse_x > sX + 25) && (mouse_x < sX + 250) && (mouse_y >= sY + 55 + row * 15) && (mouse_y <= sY + 55 + 14 + row * 15);

			if (m_items[i].amount > 1)
			{
				// Multiple items — itemInfo.name already includes the count prefix
				if (hover)
					put_aligned_string(sX, sX + size_x, sY + 55 + row * 15, itemInfo.name.c_str(), GameColors::UIWhite);
				else if (itemInfo.is_special)
					put_aligned_string(sX, sX + size_x, sY + 55 + row * 15, itemInfo.name.c_str(), GameColors::UIItemName_Special);
				else
					put_aligned_string(sX, sX + size_x, sY + 55 + row * 15, itemInfo.name.c_str(), GameColors::UILabel);
			}
			else
			{
				// Single item
				if (hover)
				{
					if (effect.empty() && extra.empty())
					{
						put_aligned_string(sX, sX + size_x, sY + 55 + row * 15, itemInfo.name.c_str(), GameColors::UIWhite);
					}
					else
					{
						if ((itemInfo.name.size() + effect.size() + extra.size()) < 36)
						{
							if (!effect.empty() && !extra.empty())
								txt = std::format("{}({}, {})", itemInfo.name.c_str(), effect.c_str(), extra.c_str());
							else
								txt = std::format("{}({}{})", itemInfo.name.c_str(), effect.c_str(), extra.c_str());
							put_aligned_string(sX, sX + size_x, sY + 55 + row * 15, txt.c_str(), GameColors::UIWhite);
						}
						else
						{
							if (!effect.empty() && !extra.empty())
								txt = std::format("({}, {})", effect.c_str(), extra.c_str());
							else
								txt = std::format("({}{})", effect.c_str(), extra.c_str());
							put_aligned_string(sX, sX + size_x, sY + 55 + row * 15, itemInfo.name.c_str(), GameColors::UIWhite);
							put_aligned_string(sX, sX + size_x, sY + 55 + row * 15 + 15, txt.c_str(), GameColors::UIDisabled);
							row++;
						}
					}
				}
				else
				{
					if (effect.empty() && extra.empty())
					{
						if (itemInfo.is_special)
							put_aligned_string(sX, sX + size_x, sY + 55 + row * 15, itemInfo.name.c_str(), GameColors::UIItemName_Special);
						else
							put_aligned_string(sX, sX + size_x, sY + 55 + row * 15, itemInfo.name.c_str(), GameColors::UILabel);
					}
					else
					{
						if ((itemInfo.name.size() + effect.size() + extra.size()) < 36)
						{
							if (!effect.empty() && !extra.empty())
								txt = std::format("{}({}, {})", itemInfo.name.c_str(), effect.c_str(), extra.c_str());
							else
								txt = std::format("{}({}{})", itemInfo.name.c_str(), effect.c_str(), extra.c_str());

							if (itemInfo.is_special)
								put_aligned_string(sX, sX + size_x, sY + 55 + row * 15, txt.c_str(), GameColors::UIItemName_Special);
							else
								put_aligned_string(sX, sX + size_x, sY + 55 + row * 15, txt.c_str(), GameColors::UILabel);
						}
						else
						{
							if (itemInfo.is_special)
								put_aligned_string(sX, sX + size_x, sY + 55 + row * 15, itemInfo.name.c_str(), GameColors::UIItemName_Special);
							else
								put_aligned_string(sX, sX + size_x, sY + 55 + row * 15, itemInfo.name.c_str(), GameColors::UILabel);
						}
					}
				}
			}
		}
		else
		{
			empty_count++;
		}
		row++;
	}
}

void DialogBox_SellList::draw_empty_list_message(short sX, short sY, short size_x)
{
	put_aligned_string(sX, sX + size_x, sY + 55 + 30 + 282 - 117 - 170, DRAW_DIALOGBOX_SELL_LIST2);
	put_aligned_string(sX, sX + size_x, sY + 55 + 45 + 282 - 117 - 170, DRAW_DIALOGBOX_SELL_LIST3);
	put_aligned_string(sX, sX + size_x, sY + 55 + 60 + 282 - 117 - 170, DRAW_DIALOGBOX_SELL_LIST4);
	put_aligned_string(sX, sX + size_x, sY + 55 + 75 + 282 - 117 - 170, DRAW_DIALOGBOX_SELL_LIST5);
	put_aligned_string(sX, sX + size_x, sY + 55 + 95 + 282 - 117 - 170, DRAW_DIALOGBOX_SELL_LIST6);
	put_aligned_string(sX, sX + size_x, sY + 55 + 110 + 282 - 117 - 170, DRAW_DIALOGBOX_SELL_LIST7);
	put_aligned_string(sX, sX + size_x, sY + 55 + 125 + 282 - 117 - 170, DRAW_DIALOGBOX_SELL_LIST8);
	put_aligned_string(sX, sX + size_x, sY + 55 + 155 + 282 - 117 - 170, DRAW_DIALOGBOX_SELL_LIST9);
}

void DialogBox_SellList::draw_buttons(short sX, short sY, short mouse_x, short mouse_y, bool has_items)
{
	// Sell button (only enabled when there are items)
	if ((mouse_x >= sX + ui_layout::left_btn_x) && (mouse_x <= sX + ui_layout::left_btn_x + ui_layout::btn_size_x) &&
		(mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y) && has_items)
		draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::left_btn_x, sY + ui_layout::btn_y, 39);
	else
		draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::left_btn_x, sY + ui_layout::btn_y, 38);

	// Cancel button
	if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) &&
		(mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y))
		draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 17);
	else
		draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 16);
}

bool DialogBox_SellList::on_click()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	short sX = m_x;
	short sY = m_y;

	// Check if clicking on an item in the list to remove it
	for (int i = 0; i < game_limits::max_sell_list; i++)
	{
		if ((mouse_x > sX + 25) && (mouse_x < sX + 250) && (mouse_y >= sY + 55 + i * 15) && (mouse_y <= sY + 55 + 14 + i * 15))
		{
			if (player().m_item_list[m_items[i].index] != 0)
			{
				// Re-enable the item
				inventory_manager::get().unlock_item(m_items[i].index);
				m_items[i].index = -1;

				audio_manager::get().play_game_sound(sound_type::effect, 14, 5);

				// Compact the list
				for (int x = 0; x < game_limits::max_sell_list - 1; x++)
				{
					if (m_items[x].index == -1)
					{
						m_items[x].index = m_items[x + 1].index;
						m_items[x].amount = m_items[x + 1].amount;

						m_items[x + 1].index = -1;
						m_items[x + 1].amount = 0;
					}
				}
			}
			return true;
		}
	}

	// Sell button
	if ((mouse_x >= sX + 30) && (mouse_x <= sX + 30 + ui_layout::btn_size_x) &&
		(mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y))
	{
		{
		hb::net::PacketRequestSellItemList req{};
		req.header.msg_id = MsgId::RequestSellItemList;
		req.header.msg_type = 0;
		for (int i = 0; i < game_limits::max_sell_list; i++) {
			req.entries[i].index = static_cast<uint8_t>(get_entry(i).index);
			req.entries[i].amount = get_entry(i).amount;
		}
		m_game->send_game_packet(req);
	}
		audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
		disable_this_dialog();
		return true;
	}

	// Cancel button
	if ((mouse_x >= sX + 154) && (mouse_x <= sX + 154 + ui_layout::btn_size_x) &&
		(mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y))
	{
		audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
		disable_this_dialog();
		return true;
	}

	return false;
}

bool DialogBox_SellList::on_item_drop()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	int item_id = CursorTarget::get_selected_id();
	if (item_id < 0 || item_id >= hb::shared::limits::MaxItems) return false;
	if (player().m_item_list[item_id] == nullptr) return false;
	if (inventory_manager::get().warn_if_locked(item_id)) return false;
	if (player().m_Controller.get_command() < 0) return false;

	// Check if item is already in sell list
	for (int i = 0; i < game_limits::max_sell_list; i++)
	{
		if (m_items[i].index == item_id)
		{
			add_event_list(BITEMDROP_SELLLIST1, 10);
			return false;
		}
	}

	// Can't sell gold
	if (player().m_item_list[item_id]->m_id_num == hb::shared::item::ItemId::Gold)
	{
		auto msg = std::format(NOTIFYMSG_CANNOT_SELL_ITEM3, player().m_item_list[item_id]->m_name);
		add_event_list(msg.c_str(), 10);
		return false;
	}

	// Can't sell broken items
	if (player().m_item_list[item_id]->m_instance.cur_durability == 0)
	{
		std::string G_cTxt;
		auto itemInfo2 = item_name_formatter::get().format(player().m_item_list[item_id].get());
		G_cTxt = std::format(NOTIFYMSG_CANNOT_SELL_ITEM2, itemInfo2.name.c_str());
		add_event_list(G_cTxt.c_str(), 10);
		return false;
	}

	// Stackable items - open quantity dialog
	CItem* cfg = m_game->get_item_config(player().m_item_list[item_id]->m_id_num);
	if (cfg && (cfg->is_stackable()) &&
		(player().m_item_list[item_id]->m_instance.count > 1))
	{
		auto* dropDlg = m_game->get_dialog_box_manager().get_dialog_as<DialogBox_ItemDropAmount>(DialogBoxId::ItemDropExternal);
		dropDlg->m_x = mouse_x - 140;
		dropDlg->m_y = mouse_y - 70;
		if (dropDlg->m_y < 0) dropDlg->m_y = 0;
		dropDlg->m_drop_x = player().m_player_x + 1;
		dropDlg->m_drop_y = player().m_player_y + 1;
		dropDlg->m_drop_target_type = 1001;
		dropDlg->m_drop_target_id = item_id;
		std::memset(dropDlg->m_target_name, 0, sizeof(dropDlg->m_target_name));
		m_game->get_dialog_box_manager().enable_dialog_box(DialogBoxId::ItemDropExternal, item_id, static_cast<int64_t>(player().m_item_list[item_id]->m_instance.count), 0);
		inventory_manager::get().lock_item(item_id);
	}
	else
	{
		// Add single item to sell list
		for (int i = 0; i < game_limits::max_sell_list; i++)
		{
			if (m_items[i].index == -1)
			{
				m_items[i].index = item_id;
				m_items[i].amount = 1;
				inventory_manager::get().lock_item(item_id);
				return true;
			}
		}
		add_event_list(BITEMDROP_SELLLIST3, 10);
	}

	return true;
}

bool DialogBox_SellList::on_disable()
{
	for (int i = 0; i < game_limits::max_sell_list; i++)
	{
		{ int idx = m_items[i].index;
		inventory_manager::get().unlock_item(idx); }
		m_items[i].index = -1;
		m_items[i].amount = 0;
	}
	return true;
}

void DialogBox_SellList::reset()
{
	for (int i = 0; i < game_limits::max_sell_list; i++)
	{
		m_items[i].index = -1;
		m_items[i].amount = 0;
	}
}

bool DialogBox_SellList::add_item(int index, int amount)
{
	for (int i = 0; i < game_limits::max_sell_list; i++)
	{
		if (m_items[i].index == -1)
		{
			m_items[i].index = index;
			m_items[i].amount = amount;
			return true;
		}
	}
	return false;
}

const DialogBox_SellList::SellEntry& DialogBox_SellList::get_entry(int i) const
{
	return m_items[i];
}
