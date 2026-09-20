#include "DialogBox_Slates.h"
#include "CursorTarget.h"
#include "DialogBox_NpcActionQuery.h"
#include "Game.h"
#include "InventoryManager.h"
#include "GameFonts.h"
#include "lan_eng.h"
#include "SpriteID.h"
#include "TextLibExt.h"
#include "NetMessages.h"
#include <format>
#include <string>
#include "IInput.h"
#include "Packet/SharedPackets.h"
#include "AudioManager.h"

using namespace hb::shared::net;
using namespace hb::shared::item;
using namespace hb::client::sprite_id;

DialogBox_Slates::DialogBox_Slates(CGame* game)
	: IDialogBox(DialogBoxId::Slates, game)
{
	set_default_rect(100 , 60 , 258, 339);
	m_can_close_on_right_click = false;
}

void DialogBox_Slates::on_draw()
{
	int adj_x, adj_y;
	short sX, sY;
	uint32_t time = m_game->m_cur_time;

	adj_x = 5;
	adj_y = 8;

	switch (m_mode) {
	case mode::waiting:
		sX = m_x;
		sY = m_y;
		adj_x = -1;
		adj_y = -7;

		draw_new_dialog_box(InterfaceNdInventory, sX, sY, 4);

		if (m_slot_ul != -1) {
			draw_new_dialog_box(InterfaceNdInventory, sX + 20, sY + 12, 5);
		}
		if (m_slot_ll != -1) {
			draw_new_dialog_box(InterfaceNdInventory, sX + 20, sY + 87, 6);
		}
		if (m_slot_ur != -1) {
			draw_new_dialog_box(InterfaceNdInventory, sX + 85, sY + 32, 7);
		}
		if (m_slot_lr != -1) {
			draw_new_dialog_box(InterfaceNdInventory, sX + 70, sY + 97, 8);
		}

		if ((m_slot_ul != -1) && (m_slot_ll != -1) && (m_slot_ur != -1) && (m_slot_lr != -1)) {
			if (mouse_in(link_cast))
				hb::shared::text::draw_text(GameFont::Bitmap1, sX + 120, sY + 150, "Casting", hb::shared::text::TextStyle::with_highlight(GameColors::UIMagicBlue));
			else hb::shared::text::draw_text(GameFont::Bitmap1, sX + 120, sY + 150, "Casting", hb::shared::text::TextStyle::with_highlight(GameColors::BmpBtnNormal));
		}
		break;

	case mode::casting:
		audio_manager::get().play_game_sound(sound_type::effect, 16, 0);
		if (m_anim_amplitude != 0)
		{
			sX = m_x + adj_x + (m_anim_amplitude - (rand() % (m_anim_amplitude * 2)));
			sY = m_y + adj_y + (m_anim_amplitude - (rand() % (m_anim_amplitude * 2)));
		}
		else
		{
			sX = m_x;
			sY = m_y;
		}
		m_game->m_sprite[InterfaceNdInventory]->draw(sX, sY, 4);
		m_game->m_sprite[InterfaceNdInventory]->draw(sX + 22, sY + 14, 3);
		put_aligned_string(199, 438, 201, "KURURURURURURURURU!!!", GameColors::UISlatesPink);
		put_aligned_string(200, 439, 200, "KURURURURURURURURU!!!", GameColors::UISlatesCyan);

		if ((time - m_anim_tick) > 1000)
		{
			m_anim_tick = time;
			m_anim_amplitude++;
		}
		if (m_anim_amplitude >= 5)
		{
			{
				hb::net::PacketCommandCommonItems req{};
				req.base.header.msg_id = MsgId::CommandCommon;
				req.base.header.msg_type = CommonType::ReqCreateSlate;
				req.base.x = player().m_player_x;
				req.base.y = player().m_player_y;
				req.item_ids[0] = static_cast<uint8_t>(m_slot_ul);
				req.item_ids[1] = static_cast<uint8_t>(m_slot_ll);
				req.item_ids[2] = static_cast<uint8_t>(m_slot_ur);
				req.item_ids[3] = static_cast<uint8_t>(m_slot_lr);
				req.item_ids[4] = static_cast<uint8_t>(m_slot_extra1);
				req.item_ids[5] = static_cast<uint8_t>(m_slot_extra2);
				req.padding = 0;
				send_game_packet(req, false);
			}
			disable_dialog_box(DialogBoxId::Slates);
		}
		break;
	}
}

bool DialogBox_Slates::on_click()
{
	switch (m_mode) {
	case mode::waiting:
		if ((m_slot_ul != -1) && (m_slot_ll != -1) && (m_slot_ur != -1) && (m_slot_lr != -1)) {
			if (mouse_in(link_cast)) {
				m_mode = mode::casting;
				audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
			}
		}
		break;
	}
	return false;
}

bool DialogBox_Slates::on_item_drop()
{
	if (player().m_Controller.get_command() < 0) return false;

	int item_id = CursorTarget::get_selected_id();
	if (item_id < 0 || item_id >= hb::shared::limits::MaxItems) return false;
	if (player().m_item_list[item_id] == nullptr) return false;
	if (inventory_manager::get().warn_if_locked(item_id)) return false;

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

	switch (m_mode) {
	case mode::waiting:
	{
		// Only accept slate items (IDs 868-871)
		short slate_id = player().m_item_list[item_id]->m_id_num;
		if (slate_id >= 868 && slate_id <= 871)
		{
			std::string item_id_text;
			switch (slate_id) {
			case 868: // Slate UL
				if (m_slot_ul == -1) {
					inventory_manager::get().lock_item(item_id);
					m_slot_ul = item_id;
					item_id_text = std::format("Item ID : {}", item_id);
					add_event_list(item_id_text.c_str(), 10);
				}
				break;
			case 869: // Slate LL
				if (m_slot_ll == -1) {
					inventory_manager::get().lock_item(item_id);
					m_slot_ll = item_id;
					item_id_text = std::format("Item ID : {}", item_id);
					add_event_list(item_id_text.c_str(), 10);
				}
				break;
			case 870: // Slate UR
				if (m_slot_ur == -1) {
					inventory_manager::get().lock_item(item_id);
					m_slot_ur = item_id;
					item_id_text = std::format("Item ID : {}", item_id);
					add_event_list(item_id_text.c_str(), 10);
				}
				break;
			case 871: // Slate LR
				if (m_slot_lr == -1) {
					inventory_manager::get().lock_item(item_id);
					m_slot_lr = item_id;
					item_id_text = std::format("Item ID : {}", item_id);
					add_event_list(item_id_text.c_str(), 10);
				}
				break;
			}
		}
		break;
	}
	}

	return true;
}

bool DialogBox_Slates::on_enable(int type, int64_t v1, int v2, const char* string)
{
	if (is_enabled()) return true;
	m_mode = static_cast<mode>(type);
	m_slot_ul = -1;
	m_slot_ll = -1;
	m_slot_ur = -1;
	m_slot_lr = -1;
	m_slot_extra1 = -1;
	m_slot_extra2 = -1;
	m_anim_amplitude = 0;
	m_anim_tick = 0;
	m_size_x = 180;
	m_size_y = 183;
	disable_dialog_box(DialogBoxId::ItemDropExternal);
	disable_dialog_box(DialogBoxId::NpcActionQuery);
	disable_dialog_box(DialogBoxId::SellOrRepair);
	disable_dialog_box(DialogBoxId::Manufacture);
	return true;
}

bool DialogBox_Slates::on_disable()
{
	auto clearSlot = [](int idx) {
		inventory_manager::get().unlock_item(idx);
	};
	clearSlot(m_slot_ul);
	clearSlot(m_slot_ll);
	clearSlot(m_slot_ur);
	clearSlot(m_slot_lr);
	m_slot_ul = -1;
	m_slot_ll = -1;
	m_slot_ur = -1;
	m_slot_lr = -1;
	m_slot_extra1 = -1;
	m_slot_extra2 = -1;
	m_anim_amplitude = 0;
	m_anim_tick = 0;
	return true;
}
