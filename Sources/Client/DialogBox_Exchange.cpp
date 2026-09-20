#include "DialogBox_Exchange.h"
#include "DialogBox_ConfirmExchange.h"
#include "DialogBox_ItemDropAmount.h"
#include "CursorTarget.h"
#include "Game.h"
#include "PacketSendHelpers.h"

#include "InventoryManager.h"
#include "ItemNameFormatter.h"
#include "ItemSpriteMetadata.h"
#include "GameFonts.h"
#include "TextLibExt.h"
#include "lan_eng.h"
#include <format>
#include <string>
#include "IInput.h"
#include "AudioManager.h"

using namespace hb::shared::net;
using namespace hb::shared::item;
using hb::shared::item::EquipPos;
using hb::shared::item::to_int;
using namespace hb::client::sprite_id;

DialogBox_Exchange::DialogBox_Exchange(CGame* game)
	: IDialogBox(DialogBoxId::Exchange, game)
{
	set_default_rect(140 , 30 , 520, 357);
}

void DialogBox_Exchange::on_draw()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	if (!m_game->ensure_item_configs_loaded()) return;
	short sX = m_x;
	short sY = m_y;
	short size_x = m_size_x;

	m_game->draw_new_dialog_box(InterfaceNdNewExchange, sX, sY, 0);

	switch (m_mode) {
	case mode::pending:
		put_aligned_string(sX + 80, sX + 180, sY + 38, player().m_player_name.c_str(), GameColors::UIDarkGreen);
		if (m_slots[4].v1 != -1)
			put_aligned_string(sX + 250, sX + 540, sY + 38, m_slots[4].str2.c_str(), GameColors::UIDarkGreen);

		draw_items(sX, sY, mouse_x, mouse_y, 0, 8);

		if ((m_slots[0].v1 != -1) && (m_slots[4].v1 == -1)) {
			put_aligned_string(sX, sX + size_x, sY + 245, DRAW_DIALOGBOX_EXCHANGE9, GameColors::UILabel);
			put_aligned_string(sX, sX + size_x, sY + 260, DRAW_DIALOGBOX_EXCHANGE10, GameColors::UILabel);
			put_aligned_string(sX, sX + size_x, sY + 275, DRAW_DIALOGBOX_EXCHANGE11, GameColors::UILabel);
			hb::shared::text::draw_text(GameFont::Bitmap1, sX + 220, sY + 310, "Exchange", hb::shared::text::TextStyle::with_highlight(GameColors::BmpBtnHover));
		}
		else if ((m_slots[0].v1 == -1) && (m_slots[4].v1 != -1)) {
			put_aligned_string(sX, sX + size_x, sY + 215, DRAW_DIALOGBOX_EXCHANGE12, GameColors::UILabel);
			put_aligned_string(sX, sX + size_x, sY + 230, DRAW_DIALOGBOX_EXCHANGE13, GameColors::UILabel);
			put_aligned_string(sX, sX + size_x, sY + 245, DRAW_DIALOGBOX_EXCHANGE14, GameColors::UILabel);
			put_aligned_string(sX, sX + size_x, sY + 260, DRAW_DIALOGBOX_EXCHANGE15, GameColors::UILabel);
			put_aligned_string(sX, sX + size_x, sY + 275, DRAW_DIALOGBOX_EXCHANGE16, GameColors::UILabel);
			put_aligned_string(sX, sX + size_x, sY + 290, DRAW_DIALOGBOX_EXCHANGE17, GameColors::UILabel);
			hb::shared::text::draw_text(GameFont::Bitmap1, sX + 220, sY + 310, "Exchange", hb::shared::text::TextStyle::with_highlight(GameColors::BmpBtnHover));
		}
		else if ((m_slots[0].v1 != -1) && (m_slots[4].v1 != -1)) {
			put_aligned_string(sX, sX + size_x, sY + 215, DRAW_DIALOGBOX_EXCHANGE18, GameColors::UILabel);
			put_aligned_string(sX, sX + size_x, sY + 230, DRAW_DIALOGBOX_EXCHANGE19, GameColors::UILabel);
			put_aligned_string(sX, sX + size_x, sY + 245, DRAW_DIALOGBOX_EXCHANGE20, GameColors::UILabel);
			put_aligned_string(sX, sX + size_x, sY + 260, DRAW_DIALOGBOX_EXCHANGE21, GameColors::UILabel);
			put_aligned_string(sX, sX + size_x, sY + 275, DRAW_DIALOGBOX_EXCHANGE22, GameColors::UILabel);
			put_aligned_string(sX, sX + size_x, sY + 290, DRAW_DIALOGBOX_EXCHANGE23, GameColors::UILabel);
			if (mouse_in(btn_exchange))
				hb::shared::text::draw_text(GameFont::Bitmap1, sX + 220, sY + 310, "Exchange", hb::shared::text::TextStyle::with_highlight(GameColors::UIMagicBlue));
			else
				hb::shared::text::draw_text(GameFont::Bitmap1, sX + 220, sY + 310, "Exchange", hb::shared::text::TextStyle::with_highlight(GameColors::BmpBtnNormal));
		}
		if (mouse_in(btn_cancel)
			&& (m_game->get_dialog_box_manager().is_enabled(DialogBoxId::ConfirmExchange) == false))
			hb::shared::text::draw_text(GameFont::Bitmap1, sX + 450, sY + 310, "Cancel", hb::shared::text::TextStyle::with_highlight(GameColors::UIMagicBlue));
		else
			hb::shared::text::draw_text(GameFont::Bitmap1, sX + 450, sY + 310, "Cancel", hb::shared::text::TextStyle::with_highlight(GameColors::BmpBtnNormal));
		break;

	case mode::confirmed:
		put_aligned_string(sX + 80, sX + 180, sY + 38, player().m_player_name.c_str(), GameColors::UIDarkGreen);
		if (m_slots[4].v1 != -1)
			put_aligned_string(sX + 250, sX + 540, sY + 38, m_slots[4].str2.c_str(), GameColors::UIDarkGreen);

		draw_items(sX, sY, mouse_x, mouse_y, 0, 8);

		std::string exchangeBuf;
		exchangeBuf = std::format(DRAW_DIALOGBOX_EXCHANGE33, m_slots[4].str2);
		put_aligned_string(sX, sX + size_x, sY + 215, exchangeBuf.c_str(), GameColors::UILabel);
		put_aligned_string(sX, sX + size_x, sY + 230, DRAW_DIALOGBOX_EXCHANGE34, GameColors::UILabel);
		put_aligned_string(sX, sX + size_x, sY + 245, DRAW_DIALOGBOX_EXCHANGE35, GameColors::UILabel);
		put_aligned_string(sX, sX + size_x, sY + 260, DRAW_DIALOGBOX_EXCHANGE36, GameColors::UILabel);
		put_aligned_string(sX, sX + size_x, sY + 275, DRAW_DIALOGBOX_EXCHANGE37, GameColors::UILabel);
		put_aligned_string(sX, sX + size_x, sY + 290, DRAW_DIALOGBOX_EXCHANGE38, GameColors::UILabel);
		put_aligned_string(sX, sX + size_x, sY + 305, DRAW_DIALOGBOX_EXCHANGE39, GameColors::UILabel);
		break;
	}
}

void DialogBox_Exchange::draw_items(short sX, short sY, short mouse_x, short mouse_y, int start_index, int end_index)
{
	uint32_t time = m_game->m_cur_time;
	char item_color;
	short xadd;

	for (int i = start_index; i < end_index; i++) {
		xadd = (58 * i) + 48;
		if (i > 3) xadd += 20;

		if (m_slots[i].v1 != -1) {
			item_color = m_slots[i].v4;
			CItem* ex_cfg = m_game->get_item_config(m_slots[i].item_id);
			auto ex_draw = m_game->get_item_draw(ex_cfg ? ex_cfg->m_display_id : 0, item_atlas::pack, ex_cfg ? ex_cfg->sprite_is_female() : false);
			if (item_color == 0) {
				ex_draw.sprite->draw(sX + xadd, sY + 130, ex_draw.frame);
			}
			else {
				const auto& ex_tint = m_game->m_color_palette[item_color];
				ex_draw.sprite->draw(sX + xadd, sY + 130, ex_draw.frame, hb::shared::sprite::DrawParams::tint(ex_tint.r, ex_tint.g, ex_tint.b));
			}

			draw_item_info(sX, sY, m_size_x, mouse_x, mouse_y, i, xadd);
		}
	}
}

void DialogBox_Exchange::draw_item_info(short sX, short sY, short size_x, short mouse_x, short mouse_y, int item_index, short xadd)
{
	std::string txt, txt2;
	int loc;

	auto itemInfo = item_name_formatter::get().format(static_cast<short>(m_slots[item_index].item_id), m_slots[item_index].item_data);

	if ((mouse_x >= sX + xadd - 6) && (mouse_x <= sX + xadd + 42) && (mouse_y >= sY + 61) && (mouse_y <= sY + 200)) {
		txt = itemInfo.name.c_str();
		if (itemInfo.is_special) {
			put_aligned_string(sX + 15, sX + 155, sY + 215, txt.c_str(), GameColors::UIItemName_Special);
			put_aligned_string(sX + 16, sX + 156, sY + 215, txt.c_str(), GameColors::UIItemName_Special);
		}
		else {
			put_aligned_string(sX + 15, sX + 155, sY + 215, txt.c_str(), GameColors::UILabel);
			put_aligned_string(sX + 16, sX + 156, sY + 215, txt.c_str(), GameColors::UILabel);
		}

		loc = 0;
		auto effect = itemInfo.effect_text();
		auto extra = itemInfo.extra_text();
		if (!effect.empty()) {
			put_aligned_string(sX + 16, sX + 155, sY + 235 + loc, effect.c_str(), GameColors::UIBlack);
			loc += 15;
		}
		if (!extra.empty()) {
			put_aligned_string(sX + 16, sX + 155, sY + 235 + loc, extra.c_str(), GameColors::UIBlack);
			loc += 15;
		}

		if (m_slots[item_index].v3 != 1) {
			if (m_slots[item_index].v3 > 1) {
				txt2 = m_game->format_comma_number(m_slots[item_index].v3);
			}
			else {
				txt2 = std::format(DRAW_DIALOGBOX_EXCHANGE2, m_slots[item_index].v3);
			}
			put_aligned_string(sX + 16, sX + 155, sY + 235 + loc, txt2.c_str(), GameColors::UILabel);
			loc += 15;
		}

		if (m_slots[item_index].v5 != -1) {
			// Crafting Magins completion fix
			CItem* magin_cfg = m_game->get_item_config(m_slots[item_index].item_id);
			if (magin_cfg && magin_cfg->get_item_sub_type() == hb::shared::item::item_sub_type::accessory && magin_cfg->get_item_type() == hb::shared::item::item_type::equipment) {
				// Magic gems (Diamond/Emerald/Ruby/Sapphire Ware) � show completion %
				txt = std::format(GET_ITEM_NAME2, (m_slots[item_index].v7 - 100));
			}
			else if (magin_cfg && magin_cfg->get_item_type() == hb::shared::item::item_type::material) {
				txt = std::format(GET_ITEM_NAME1, (m_slots[item_index].v7 - 100));
			}
			else {
				txt = std::format(GET_ITEM_NAME2, m_slots[item_index].v7);
			}
			put_aligned_string(sX + 16, sX + 155, sY + 235 + loc, txt.c_str(), GameColors::UILabel);
			loc += 15;
		}

		if (loc < 45) {
			txt = std::format(DRAW_DIALOGBOX_EXCHANGE3, m_slots[item_index].v5, m_slots[item_index].v6);
			put_aligned_string(sX + 16, sX + 155, sY + 235 + loc, txt.c_str(), GameColors::UILabel);
		}
	}
}

bool DialogBox_Exchange::on_click()
{
	switch (m_mode) {
	case mode::pending:
		if (mouse_in(btn_exchange)) {
			// Exchange button
			if ((m_slots[0].v1 != -1) && (m_slots[4].v1 != -1)) {
				audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
				m_mode = mode::confirmed;
				// Show confirmation dialog
				m_game->get_dialog_box_manager().enable_dialog_box(DialogBoxId::ConfirmExchange, 0, 0, 0);
				get_dialog_box_as<DialogBox_ConfirmExchange>(DialogBoxId::ConfirmExchange)->m_mode = DialogBox_ConfirmExchange::mode::question;
			}
			return true;
		}
		if (mouse_in(btn_cancel)
			&& (m_game->get_dialog_box_manager().is_enabled(DialogBoxId::ConfirmExchange) == false)) {
			// Cancel button
			m_game->get_dialog_box_manager().disable_dialog_box(DialogBoxId::Exchange);
			m_game->get_dialog_box_manager().disable_dialog_box(DialogBoxId::Map);
			send_game_packet(hb::net::make_common_command(CommonType::cancel_exchange_item, player().m_player_x, player().m_player_y));
			audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
			return true;
		}
		break;

	case mode::confirmed:
		break;
	}

	return false;
}

bool DialogBox_Exchange::on_item_drop()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	if (player().m_Controller.get_command() < 0) return false;
	if (m_slots[3].v1 != -1) return false; // Already 4 items

	int item_id = CursorTarget::get_selected_id();
	if (item_id < 0 || item_id >= hb::shared::limits::MaxItems) return false;
	if (inventory_manager::get().warn_if_locked(item_id)) return false;

	// Find first empty exchange slot
	int slot = -1;
	if (m_slots[0].v1 == -1) slot = 0;
	else if (m_slots[1].v1 == -1) slot = 1;
	else if (m_slots[2].v1 == -1) slot = 2;
	else if (m_slots[3].v1 == -1) slot = 3;
	else return false; // Impossible case

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
		dropDlg->m_drop_target_type = 1000;
		dropDlg->m_drop_target_id = item_id;
		m_slots[slot].inv_slot = item_id;
		std::memset(dropDlg->m_target_name, 0, sizeof(dropDlg->m_target_name));
		m_game->get_dialog_box_manager().enable_dialog_box(DialogBoxId::ItemDropExternal, item_id,
			static_cast<int64_t>(player().m_item_list[item_id]->m_instance.count), 0);
	}
	else
	{
		// Single item - add directly
		m_slots[slot].inv_slot = item_id;
		inventory_manager::get().lock_item(item_id);
		{
			auto pkt = hb::net::make_common_command(CommonType::set_exchange_item, player().m_player_x, player().m_player_y);
			pkt.v1 = item_id;
			pkt.v2 = 1;
			send_game_packet(pkt);
		}
	}

	return true;
}

bool DialogBox_Exchange::on_enable(int type, int64_t v1, int v2, const char* string)
{
	if (is_enabled()) return true;
	m_mode = static_cast<mode>(type);
	for (int i = 0; i < 8; i++)
	{
		m_slots[i].v1 = -1;
		m_slots[i].v2 = -1;
		m_slots[i].v3 = -1;
		m_slots[i].v4 = -1;
		m_slots[i].v5 = -1;
		m_slots[i].v6 = -1;
		m_slots[i].v7 = -1;
		m_slots[i].inv_slot = -1;
		m_slots[i].item_data = {};
	}
	disable_dialog_box(DialogBoxId::ItemDropExternal);
	disable_dialog_box(DialogBoxId::NpcActionQuery);
	disable_dialog_box(DialogBoxId::SellOrRepair);
	disable_dialog_box(DialogBoxId::Manufacture);
	return true;
}

bool DialogBox_Exchange::on_disable()
{
	for (int i = 0; i < 8; i++)
	{
		int slot = m_slots[i].inv_slot;
		if (inventory_manager::get().is_locked(slot))
			inventory_manager::get().unlock_item(slot);

		m_slots[i].v1 = -1;
		m_slots[i].v2 = -1;
		m_slots[i].v3 = -1;
		m_slots[i].v4 = -1;
		m_slots[i].v5 = -1;
		m_slots[i].v6 = -1;
		m_slots[i].v7 = -1;
		m_slots[i].item_id = -1;
		m_slots[i].inv_slot = -1;
		m_slots[i].item_data = {};
	}
	return true;
}

void DialogBox_Exchange::reset_slots()
{
	for (int i = 0; i < 8; i++)
	{
		m_slots[i].v1 = -1;
		m_slots[i].v2 = -1;
		m_slots[i].v3 = -1;
		m_slots[i].v4 = -1;
		m_slots[i].v5 = -1;
		m_slots[i].v6 = -1;
		m_slots[i].v7 = -1;
		m_slots[i].item_id = -1;
		m_slots[i].inv_slot = -1;
		m_slots[i].item_data = {};
	}
}

int DialogBox_Exchange::find_empty_slot(int start, int end) const
{
	for (int i = start; i < end; i++)
		if (m_slots[i].v1 == -1) return i;
	return -1;
}
