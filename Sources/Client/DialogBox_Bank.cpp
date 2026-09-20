#include "DialogBox_Bank.h"
#include "CursorTarget.h"
#include "DialogBox_ItemDropAmount.h"
#include "DialogBox_NpcActionQuery.h"
#include "Game.h"
#include "InventoryManager.h"
#include "ItemNameFormatter.h"
#include "ItemSpriteMetadata.h"
#include "IInput.h"
#include "lan_eng.h"
#include <format>
#include "TextInputManager.h"
#include "Packet/SharedPackets.h"
#include "PacketSendHelpers.h"
#include "AudioManager.h"
#include "BalanceConstants.h"


using namespace hb::shared::net;
using namespace hb::shared::item;
using namespace hb::client::sprite_id;

DialogBox_Bank::DialogBox_Bank(CGame* game)
	: IDialogBox(DialogBoxId::Bank, game)
{
	set_default_rect(60 , 50 , 258, 339);
	m_item_count = 13; // Number of visible item lines in scrollable list
}

void DialogBox_Bank::on_draw()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	short z = static_cast<short>(hb::shared::input::get_mouse_wheel_delta());
	char lb = hb::shared::input::is_mouse_button_down(hb::shared::input::MouseButton::Left) ? 1 : 0;
	if (!m_game->ensure_item_configs_loaded()) return;
	short sX = m_x;
	short sY = m_y;
	short size_x = m_size_x - 5;

	m_game->draw_new_dialog_box(InterfaceNdGame2, sX, sY, 2);
	m_game->draw_new_dialog_box(InterfaceNdText, sX, sY, 21);

	switch (m_mode) {
	case mode::waiting:
		put_string(sX + 30 + 15, sY + 70, DRAW_DIALOGBOX_BANK1, GameColors::UIBlack);
		put_string(sX + 30 + 15, sY + 85, DRAW_DIALOGBOX_BANK2, GameColors::UIBlack);
		break;

	case mode::list:
		draw_item_list(sX, sY, size_x);
		break;
	}
}

void DialogBox_Bank::draw_item_list(short sX, short sY, short size_x)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	bool flag = false;
	int loc = 45;


	for (int i = 0; i < m_item_count; i++) {
		int itemIndex = i + m_scroll_offset;
		if ((itemIndex < hb::shared::limits::MaxBankItems) && (player().m_bank_list[itemIndex] != 0)) {
			auto itemInfo = item_name_formatter::get().format(player().m_bank_list[itemIndex].get());

			if ((mouse_x > sX + 30) && (mouse_x < sX + 210) && (mouse_y >= sY + 110 + i * 15) && (mouse_y <= sY + 124 + i * 15)) {
				flag = true;
				put_aligned_string(sX, sX + size_x, sY + 110 + i * 15, itemInfo.name.c_str(), GameColors::UIWhite);
				draw_item_details(sX, sY, size_x, itemIndex, loc);
			}
			else {
				if (itemInfo.is_special)
					put_aligned_string(sX, sX + size_x, sY + 110 + i * 15, itemInfo.name.c_str(), GameColors::UIItemName_Special);
				else
					put_aligned_string(sX, sX + size_x, sY + 110 + i * 15, itemInfo.name.c_str(), GameColors::UIBlack);
			}
		}
	}

	// Count total items for scrollbar
	int total_lines = 0;
	for (int i = 0; i < hb::shared::limits::MaxBankItems; i++)
		if (player().m_bank_list[i] != 0) total_lines++;

	draw_scrollbar(sX, sY, total_lines);

	if (!flag) {
		put_aligned_string(sX, sX + size_x, sY + 45, DRAW_DIALOGBOX_BANK3);
		put_aligned_string(sX, sX + size_x, sY + 60, DRAW_DIALOGBOX_BANK4);
		put_aligned_string(sX, sX + size_x, sY + 75, DRAW_DIALOGBOX_BANK5);
	}
}

void DialogBox_Bank::draw_item_details(short sX, short sY, short size_x, int item_index, int loc)
{

	CItem* item = player().m_bank_list[item_index].get();
	CItem* cfg = m_game->get_item_config(item->m_id_num);
	if (cfg == nullptr) return;

	auto itemInfo2 = item_name_formatter::get().format(item);

	if (itemInfo2.is_special)
		put_aligned_string(sX + 70, sX + size_x, sY + loc, itemInfo2.name.c_str(), GameColors::UIItemName_Special);
	else
		put_aligned_string(sX + 70, sX + size_x, sY + loc, itemInfo2.name.c_str(), GameColors::UIWhite);

	auto effect = itemInfo2.effect_text();
	auto extra = itemInfo2.extra_text();
	if (!effect.empty()) {
		loc += 15;
		put_aligned_string(sX + 70, sX + size_x, sY + loc, effect.c_str(), GameColors::UIDisabled);
	}
	if (!extra.empty()) {
		loc += 15;
		put_aligned_string(sX + 70, sX + size_x, sY + loc, extra.c_str(), GameColors::UIDisabled);
	}

	// Level limit
	if (cfg->m_level_requirement != 0) {
		loc += 15;
		auto buf = std::format("{}: {}", DRAW_DIALOGBOX_SHOP24, cfg->m_level_requirement);
		put_aligned_string(sX + 70, sX + size_x, sY + loc, buf.c_str(), GameColors::UIDisabled);
	}

	// Weight for equipment (use cfg base weight, apply light attribute from item)
	int bank_light = item->get_light_percent();
	int eff_weight = (bank_light > 0) ? cfg->m_weight * (100 - bank_light) / 100 : cfg->m_weight;
	int wups = hb::shared::balance::weight_units_per_stone;
	if ((cfg->get_equip_pos() != EquipPos::None) &&
		(eff_weight >= hb::shared::balance::equip_str_threshold)) {
		loc += 15;
		int _wWeight = 0;
		if (eff_weight % wups) _wWeight = 1;
		auto strReq = std::format(DRAW_DIALOGBOX_SHOP15, eff_weight / wups + _wWeight);
		put_aligned_string(sX + 70, sX + size_x, sY + loc, strReq.c_str(), GameColors::UIDisabled);
	}

	// draw item sprite
	char item_color = item->m_instance.item_color;
	auto bank_draw = m_game->get_item_draw(cfg->m_display_id, item_atlas::pack, cfg->sprite_is_female());
	if (item_color == 0) {
		bank_draw.sprite->draw(sX + 60, sY + 68, bank_draw.frame);
	}
	else {
		const auto& tint = m_game->m_color_palette[item_color];
		bank_draw.sprite->draw(sX + 60, sY + 68, bank_draw.frame, hb::shared::sprite::DrawParams::tint(tint.r, tint.g, tint.b));
	}
}

void DialogBox_Bank::draw_scrollbar(short sX, short sY, int total_lines)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	short z = static_cast<short>(hb::shared::input::get_mouse_wheel_delta());
	char lb = hb::shared::input::is_mouse_button_down(hb::shared::input::MouseButton::Left) ? 1 : 0;
	int pointer_loc;
	double d1, d2, d3;

	if (total_lines > m_item_count) {
		d1 = static_cast<double>(m_scroll_offset);
		d2 = static_cast<double>(total_lines - m_item_count);
		d3 = (274.0f * d1) / d2;
		pointer_loc = static_cast<int>(d3);
		m_game->draw_new_dialog_box(InterfaceNdGame2, sX, sY, 3);
		m_game->draw_new_dialog_box(InterfaceNdGame2, sX + 242, sY + pointer_loc + 35, 7);
	}
	else {
		pointer_loc = 0;
	}

	if (lb != 0 && (m_game->get_dialog_box_manager().get_top_id() == DialogBoxId::Bank) && total_lines > m_item_count) {
		if ((mouse_x >= sX + 230) && (mouse_x <= sX + 260) && (mouse_y >= sY + 40) && (mouse_y <= sY + 320)) {
			d1 = static_cast<double>(mouse_y - (sY + 35));
			d2 = static_cast<double>(total_lines - m_item_count);
			d3 = (d1 * d2) / 274.0f;
			m_scroll_offset = static_cast<int>(d3 + 0.5);
		}
		else if ((mouse_x >= sX + 230) && (mouse_x <= sX + 260) && (mouse_y > sY + 10) && (mouse_y < sY + 40)) {
			m_scroll_offset = 0;
		}
	}
	else {
		m_is_scroll_selected = false;
	}

	if (m_game->get_dialog_box_manager().get_top_id() == DialogBoxId::Bank && z != 0) {
		if (total_lines > 50)
			m_scroll_offset = m_scroll_offset - z / 30;
		else {
			if (z > 0) m_scroll_offset--;
			if (z < 0) m_scroll_offset++;
		}
	}

	if (total_lines > m_item_count && m_scroll_offset > total_lines - m_item_count)
		m_scroll_offset = total_lines - m_item_count;
	if (total_lines <= m_item_count)
		m_scroll_offset = 0;
	if (m_scroll_offset < 0)
		m_scroll_offset = 0;
}

bool DialogBox_Bank::on_click()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	short sX = m_x;
	short sY = m_y;

	switch (m_mode) {
	case mode::waiting:
		break;

	case mode::list:
		for (int i = 0; i < m_item_count; i++) {
			if ((mouse_x > sX + 30) && (mouse_x < sX + 210) && (mouse_y >= sY + 110 + i * 15) && (mouse_y <= sY + 124 + i * 15)) {
				int itemIndex = m_scroll_offset + i;
				if ((itemIndex < hb::shared::limits::MaxBankItems) && (player().m_bank_list[itemIndex] != 0)) {
					if (inventory_manager::get().get_total_item_count() >= 50) {
						add_event_list(DLGBOX_CLICK_BANK1, 10);
						return true;
					}
					{
			hb::net::PacketRequestRetrieveItem req{};
			req.header.msg_id = MsgId::RequestRetrieveItem;
			req.header.msg_type = MsgType::Confirm;
			req.item_slot = static_cast<uint8_t>(itemIndex);
			send_game_packet(req);
		}
					m_mode = mode::waiting;
					audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
				}
				return true;
			}
		}
		break;
	}

	return false;
}

bool DialogBox_Bank::on_double_click()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	if (CursorTarget::GetSelectedType() == SelectedObjectType::Item)
		return on_item_drop();

	return false;
}

bool DialogBox_Bank::on_item_drop()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	auto& give = m_game->get_dialog_box_manager().m_give_item;
	give.item_index = CursorTarget::get_selected_id();

	if (player().m_Controller.get_command() < 0) {
		return false;
	}
	if (player().m_item_list[give.item_index] == nullptr) {
		return false;
	}
	if (inventory_manager::get().is_locked(give.item_index)) {
		return false;
	}

	// Check if other dialogs are blocking
	if (m_game->get_dialog_box_manager().is_enabled(DialogBoxId::ItemDropExternal))
	{
		add_event_list(BITEMDROP_SKILLDIALOG1, 10);
		return false;
	}
	if (m_game->get_dialog_box_manager().is_enabled(DialogBoxId::NpcActionQuery) &&
		(m_game->get_dialog_box_manager().get_dialog_as<DialogBox_NpcActionQuery>(DialogBoxId::NpcActionQuery)->m_mode == DialogBox_NpcActionQuery::mode::give_to_player ||
		 m_game->get_dialog_box_manager().get_dialog_as<DialogBox_NpcActionQuery>(DialogBoxId::NpcActionQuery)->m_mode == DialogBox_NpcActionQuery::mode::sell_to_shop))
	{
		add_event_list(BITEMDROP_SKILLDIALOG1, 10);
		return false;
	}
	if (m_game->get_dialog_box_manager().is_enabled(DialogBoxId::SellOrRepair))
	{
		add_event_list(BITEMDROP_SKILLDIALOG1, 10);
		return false;
	}
	if (m_game->get_dialog_box_manager().is_enabled(DialogBoxId::ItemDropConfirm))
	{
		add_event_list(BITEMDROP_SKILLDIALOG1, 10);
		return false;
	}

	// Stackable items - open quantity dialog
	CItem* cfg = m_game->get_item_config(player().m_item_list[give.item_index]->m_id_num);
	if (cfg == nullptr) {
		return false;
	}

	if ((cfg->is_stackable()) &&
		(player().m_item_list[give.item_index]->m_instance.count > 1))
	{
		auto* dropDlg = m_game->get_dialog_box_manager().get_dialog_as<DialogBox_ItemDropAmount>(DialogBoxId::ItemDropExternal);
		dropDlg->m_x = mouse_x - 140;
		dropDlg->m_y = mouse_y - 70;
		if (dropDlg->m_y < 0) dropDlg->m_y = 0;
		dropDlg->m_drop_x = player().m_player_x + 1;
		dropDlg->m_drop_y = player().m_player_y + 1;
		dropDlg->m_drop_target_type = 1002; // NPC
		dropDlg->m_drop_target_id = give.item_index;
		std::memset(dropDlg->m_target_name, 0, sizeof(dropDlg->m_target_name));
		m_game->get_dialog_box_manager().enable_dialog_box(DialogBoxId::ItemDropExternal, give.item_index,
			static_cast<int64_t>(player().m_item_list[give.item_index]->m_instance.count), 0);
	}
	else
	{
		// Single item - deposit directly
		if (inventory_manager::get().get_bank_item_count() >= (m_game->m_max_bank_items - 1))
		{
			add_event_list(DLGBOX_CLICK_NPCACTION_QUERY9, 10);
		}
		else
		{
			{
				auto pkt = hb::net::make_common_command_str(CommonType::GiveItemToChar, player().m_player_x, player().m_player_y, give.item_index);
				pkt.v1 = 1;
				pkt.v2 = give.target_x;
				pkt.v3 = give.target_y;
				std::snprintf(pkt.text, sizeof(pkt.text), "%s", cfg->m_name);
				pkt.v4 = give.object_id;
				send_game_packet(pkt);
			}
		}
	}

	return true;
}

PressResult DialogBox_Bank::on_press()
{
	if (mouse_in(area_scroll))
	{
		return PressResult::ScrollClaimed;
	}

	return PressResult::Normal;
}

bool DialogBox_Bank::on_enable(int type, int64_t v1, int v2, const char* string)
{
	text_input_manager::get().end_input();
	if (!is_enabled()) {
		m_mode = mode::list;
		m_scroll_offset = 0;
		enable_dialog_box(DialogBoxId::Inventory, 0, 0, 0);
	}
	return true;
}

bool DialogBox_Bank::on_disable()
{
	if (m_mode == mode::waiting) return false;
	return true;
}
