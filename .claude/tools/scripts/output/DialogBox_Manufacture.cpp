#include "DialogBox_Manufacture.h"
#include "CursorTarget.h"
#include "DialogBox_NpcActionQuery.h"
#include "Game.h"
#include "InventoryManager.h"
#include "BuildItemManager.h"
#include "ItemNameFormatter.h"
#include "ItemSpriteMetadata.h"
#include "GameFonts.h"
#include "GlobalDef.h"
#include "SpriteID.h"
#include "TextLibExt.h"
#include "lan_eng.h"
#include "NetMessages.h"
#include "IInput.h"
#include "Packet/SharedPackets.h"
#include <format>
#include <string>
#include "Screen_OnGame.h"

using namespace hb::shared::net;
using namespace hb::shared::item;
using namespace hb::client::sprite_id;

DialogBox_Manufacture::DialogBox_Manufacture(CGame* game)
	: IDialogBox(DialogBoxId::Manufacture, game)
{
	set_default_rect(100 , 60 , 258, 339);
}

void DialogBox_Manufacture::on_draw()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	short z = static_cast<short>(hb::shared::input::get_mouse_wheel_delta());
	char lb = hb::shared::input::is_mouse_button_down(hb::shared::input::MouseButton::Left) ? 1 : 0;
	if (!m_game->ensure_item_configs_loaded()) return;
	int adj_x = 5;
	int adj_y = 8;
	short sX, sY;

	switch (m_mode) {
	case mode::alchemy_waiting: // Alchemy waiting for ingredients
		if (m_progress != 0)
		{
			sX = m_x + adj_x + (m_progress - (rand() % (m_progress * 2)));
			sY = m_y + adj_y + (m_progress - (rand() % (m_progress * 2)));
		}
		else
		{
			sX = m_x;
			sY = m_y;
		}
		draw_alchemy_waiting(sX, sY);
		break;

	case mode::alchemy_creating: // Alchemy creating potion
		if (m_progress != 0)
		{
			sX = m_x + adj_x + (m_progress - (rand() % (m_progress * 2)));
			sY = m_y + adj_y + (m_progress - (rand() % (m_progress * 2)));
		}
		else
		{
			sX = m_x;
			sY = m_y;
		}
		draw_alchemy_creating(sX, sY);
		break;

	case mode::manufacture_list: // Manufacture: item list
		sX = m_x;
		sY = m_y;
		draw_manufacture_list(sX, sY);
		break;

	case mode::manufacture_waiting: // Manufacture: waiting for ingredients
		sX = m_x;
		sY = m_y;
		draw_manufacture_waiting(sX, sY);
		break;

	case mode::manufacture_in_progress: // Manufacture: in progress
		sX = m_x;
		sY = m_y;
		draw_manufacture_in_progress(sX, sY);
		break;

	case mode::manufacture_done: // Manufacture: done
		sX = m_x;
		sY = m_y;
		draw_manufacture_done(sX, sY);
		break;

	case mode::crafting_waiting: // Crafting: waiting for ingredients
		if (m_progress != 0)
		{
			sX = m_x + adj_x + (m_progress - (rand() % (m_progress * 2)));
			sY = m_y + adj_y + (m_progress - (rand() % (m_progress * 2)));
		}
		else
		{
			sX = m_x;
			sY = m_y;
		}
		draw_crafting_waiting(sX, sY);
		break;

	case mode::crafting_in_progress: // Crafting: in progress
		if (m_progress != 0)
		{
			sX = m_x + 5 + (m_progress - (rand() % (m_progress * 2)));
			sY = m_y + 8 + (m_progress - (rand() % (m_progress * 2)));
		}
		else
		{
			sX = m_x;
			sY = m_y;
		}
		draw_crafting_in_progress(sX, sY);
		break;
	}
}

void DialogBox_Manufacture::draw_alchemy_waiting(short sX, short sY)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	int adj_x = 5;
	int adj_y = 8;
	uint32_t time = m_game->m_cur_time;

	m_game->m_sprite[InterfaceAddInterface]->draw(sX, sY, 1);

	if (m_slot_1 != -1) {
		CItem* cfg = m_game->get_item_config(player().m_item_list[m_slot_1]->m_id_num);
		if (cfg) { auto d = m_game->get_item_draw(cfg->m_display_id, item_atlas::pack, cfg->sprite_is_female());
		d.sprite->draw(sX + adj_x + 55 + (1 - (rand() % 3)), sY + adj_y + 55 + (1 - (rand() % 3)), d.frame, hb::shared::sprite::DrawParams::alpha_blend(0.5f)); }
	}

	if (m_slot_2 != -1) {
		CItem* cfg = m_game->get_item_config(player().m_item_list[m_slot_2]->m_id_num);
		if (cfg) { auto d = m_game->get_item_draw(cfg->m_display_id, item_atlas::pack, cfg->sprite_is_female());
		d.sprite->draw(sX + adj_x + 55 + 45 * 1 + (1 - (rand() % 3)), sY + adj_y + 55 + (1 - (rand() % 3)), d.frame, hb::shared::sprite::DrawParams::alpha_blend(0.5f)); }
	}

	if (m_slot_3 != -1) {
		CItem* cfg = m_game->get_item_config(player().m_item_list[m_slot_3]->m_id_num);
		if (cfg) { auto d = m_game->get_item_draw(cfg->m_display_id, item_atlas::pack, cfg->sprite_is_female());
		d.sprite->draw(sX + adj_x + 55 + 45 * 2 + (1 - (rand() % 3)), sY + adj_y + 55 + (1 - (rand() % 3)), d.frame, hb::shared::sprite::DrawParams::alpha_blend(0.5f)); }
	}

	if (m_slot_4 != -1) {
		CItem* cfg = m_game->get_item_config(player().m_item_list[m_slot_4]->m_id_num);
		if (cfg) { auto d = m_game->get_item_draw(cfg->m_display_id, item_atlas::pack, cfg->sprite_is_female());
		d.sprite->draw(sX + adj_x + 55 + (1 - (rand() % 3)), sY + adj_y + 100 + (1 - (rand() % 3)), d.frame, hb::shared::sprite::DrawParams::alpha_blend(0.5f)); }
	}

	if (m_slot_5 != -1) {
		CItem* cfg = m_game->get_item_config(player().m_item_list[m_slot_5]->m_id_num);
		if (cfg) { auto d = m_game->get_item_draw(cfg->m_display_id, item_atlas::pack, cfg->sprite_is_female());
		d.sprite->draw(sX + adj_x + 55 + 45 * 1 + (1 - (rand() % 3)), sY + adj_y + 100 + (1 - (rand() % 3)), d.frame, hb::shared::sprite::DrawParams::alpha_blend(0.5f)); }
	}

	if (m_slot_6 != -1) {
		CItem* cfg = m_game->get_item_config(player().m_item_list[m_slot_6]->m_id_num);
		if (cfg) { auto d = m_game->get_item_draw(cfg->m_display_id, item_atlas::pack, cfg->sprite_is_female());
		d.sprite->draw(sX + adj_x + 55 + 45 * 2 + (1 - (rand() % 3)), sY + adj_y + 100 + (1 - (rand() % 3)), d.frame, hb::shared::sprite::DrawParams::alpha_blend(0.5f)); }
	}

	if ((mouse_x >= sX + adj_x + 60) && (mouse_x <= sX + adj_x + 153) && (mouse_y >= sY + adj_y + 175) && (mouse_y <= sY + adj_y + 195))
		hb::shared::text::draw_text(GameFont::Bitmap1, sX + adj_x + 60, sY + adj_y + 175, "Try Now!", hb::shared::text::TextStyle::with_highlight(GameColors::BmpBtnBlue));
	else hb::shared::text::draw_text(GameFont::Bitmap1, sX + adj_x + 60, sY + adj_y + 175, "Try Now!", hb::shared::text::TextStyle::with_highlight(GameColors::UIMagicBlue));
}

void DialogBox_Manufacture::draw_alchemy_creating(short sX, short sY)
{
	int adj_x = 5;
	int adj_y = 8;
	uint32_t time = m_game->m_cur_time;

	m_game->m_sprite[InterfaceAddInterface]->draw(sX, sY, 1);

	if (m_slot_1 != -1) {
		CItem* cfg = m_game->get_item_config(player().m_item_list[m_slot_1]->m_id_num);
		if (cfg) { auto d = m_game->get_item_draw(cfg->m_display_id, item_atlas::pack, cfg->sprite_is_female());
		d.sprite->draw(sX + adj_x + 55 + (1 - (rand() % 3)), sY + adj_y + 55 + (1 - (rand() % 3)), d.frame, hb::shared::sprite::DrawParams::alpha_blend(0.5f)); }
	}

	if (m_slot_2 != -1) {
		CItem* cfg = m_game->get_item_config(player().m_item_list[m_slot_2]->m_id_num);
		if (cfg) { auto d = m_game->get_item_draw(cfg->m_display_id, item_atlas::pack, cfg->sprite_is_female());
		d.sprite->draw(sX + adj_x + 55 + 45 * 1 + (1 - (rand() % 3)), sY + adj_y + 55 + (1 - (rand() % 3)), d.frame, hb::shared::sprite::DrawParams::alpha_blend(0.5f)); }
	}

	if (m_slot_3 != -1) {
		CItem* cfg = m_game->get_item_config(player().m_item_list[m_slot_3]->m_id_num);
		if (cfg) { auto d = m_game->get_item_draw(cfg->m_display_id, item_atlas::pack, cfg->sprite_is_female());
		d.sprite->draw(sX + adj_x + 55 + 45 * 2 + (1 - (rand() % 3)), sY + adj_y + 55 + (1 - (rand() % 3)), d.frame, hb::shared::sprite::DrawParams::alpha_blend(0.5f)); }
	}

	if (m_slot_4 != -1) {
		CItem* cfg = m_game->get_item_config(player().m_item_list[m_slot_4]->m_id_num);
		if (cfg) { auto d = m_game->get_item_draw(cfg->m_display_id, item_atlas::pack, cfg->sprite_is_female());
		d.sprite->draw(sX + adj_x + 55 + (1 - (rand() % 3)), sY + adj_y + 100 + (1 - (rand() % 3)), d.frame, hb::shared::sprite::DrawParams::alpha_blend(0.5f)); }
	}

	if (m_slot_5 != -1) {
		CItem* cfg = m_game->get_item_config(player().m_item_list[m_slot_5]->m_id_num);
		if (cfg) { auto d = m_game->get_item_draw(cfg->m_display_id, item_atlas::pack, cfg->sprite_is_female());
		d.sprite->draw(sX + adj_x + 55 + 45 * 1 + (1 - (rand() % 3)), sY + adj_y + 100 + (1 - (rand() % 3)), d.frame, hb::shared::sprite::DrawParams::alpha_blend(0.5f)); }
	}

	if (m_slot_6 != -1) {
		CItem* cfg = m_game->get_item_config(player().m_item_list[m_slot_6]->m_id_num);
		if (cfg) { auto d = m_game->get_item_draw(cfg->m_display_id, item_atlas::pack, cfg->sprite_is_female());
		d.sprite->draw(sX + adj_x + 55 + 45 * 2 + (1 - (rand() % 3)), sY + adj_y + 100 + (1 - (rand() % 3)), d.frame, hb::shared::sprite::DrawParams::alpha_blend(0.5f)); }
	}

	hb::shared::text::draw_text(GameFont::Bitmap1, sX + adj_x + 60, sY + adj_y + 175, "Creating...", hb::shared::text::TextStyle::with_highlight(GameColors::BmpBtnRed));

	if ((time - m_anim_timer) > 1000)
	{
		m_anim_timer = time;
		m_progress++;
	}

	if (m_progress >= 5)
	{
		{
			hb::net::PacketCommandCommonItems req{};
			req.base.header.msg_id = MsgId::CommandCommon;
			req.base.header.msg_type = CommonType::ReqCreatePortion;
			req.base.x = player().m_player_x;
			req.base.y = player().m_player_y;
			req.item_ids[0] = static_cast<uint8_t>(m_slot_1);
			req.item_ids[1] = static_cast<uint8_t>(m_slot_2);
			req.item_ids[2] = static_cast<uint8_t>(m_slot_3);
			req.item_ids[3] = static_cast<uint8_t>(m_slot_4);
			req.item_ids[4] = static_cast<uint8_t>(m_slot_5);
			req.item_ids[5] = static_cast<uint8_t>(m_slot_6);
			req.padding = 0;
			m_game->send_game_packet(req, false);
		}
		m_game->get_dialog_box_manager().disable_dialog_box(DialogBoxId::Manufacture);
		m_game->play_game_sound('E', 42, 0);
	}
}

void DialogBox_Manufacture::draw_manufacture_list(short sX, short sY)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	short z = static_cast<short>(hb::shared::input::get_mouse_wheel_delta());
	char lb = hb::shared::input::is_mouse_button_down(hb::shared::input::MouseButton::Left) ? 1 : 0;
	int adj_x = 5;
	int adj_y = 8;
	uint32_t time = m_game->m_cur_time;
	short size_x = m_size_x;
	std::string temp, temp2;
	int loc;

	draw_new_dialog_box(InterfaceNdGame3, sX, sY, 0);
	draw_new_dialog_box(InterfaceNdText, sX, sY, 8);
	put_string(sX + adj_x + 44, sY + adj_y + 38, "Name", GameColors::UIBlack);
	put_string(sX + adj_x + 171, sY + adj_y + 38, "Max.Skill", GameColors::UIBlack);

	loc = 0;
	for (int i = 0; i < 13; i++)
		if (build_item_manager::get().get_display_list()[i + m_scroll_view] != 0) {

			auto itemInfo = item_name_formatter::get().format(m_game->find_item_id_by_name(build_item_manager::get().get_display_list()[i + m_scroll_view]->m_name.c_str()),  0);
			temp = itemInfo.name.c_str();
			temp2 = std::format("{}%", build_item_manager::get().get_display_list()[i + m_scroll_view]->m_max_skill);

			if ((mouse_x >= sX + 30) && (mouse_x <= sX + 180) && (mouse_y >= sY + adj_y + 55 + loc * 15) && (mouse_y <= sY + adj_y + 69 + loc * 15))
			{
				put_string(sX + 30, sY + adj_y + 55 + loc * 15, temp.c_str(), GameColors::UIWhite);
				put_string(sX + 190, sY + adj_y + 55 + loc * 15, temp2.c_str(), GameColors::UIWhite);
			}
			else
			{
				if (build_item_manager::get().get_display_list()[i + m_scroll_view]->m_build_enabled == true)
				{
					put_string(sX + 30, sY + adj_y + 55 + loc * 15, temp.c_str(), GameColors::UIMagicBlue);
					put_string(sX + 190, sY + adj_y + 55 + loc * 15, temp2.c_str(), GameColors::UIMagicBlue);
				}
				else
				{
					put_string(sX + 30, sY + adj_y + 55 + loc * 15, temp.c_str(), GameColors::UILabel);
					put_string(sX + 190, sY + adj_y + 55 + loc * 15, temp2.c_str(), GameColors::UILabel);
				}
			}
			loc++;
		}

	if ((m_scroll_view >= 1) && (build_item_manager::get().get_display_list()[m_scroll_view - 1] != 0))
		m_game->m_sprite[InterfaceNdGame2]->draw(sX + adj_x + 225, sY + adj_y + 210, 23);
	else m_game->m_sprite[InterfaceNdGame2]->draw(sX + adj_x + 225, sY + adj_y + 210, 23, hb::shared::sprite::DrawParams::tinted_alpha(GameColors::UIBlack.r, GameColors::UIBlack.g, GameColors::UIBlack.b, 0.7f));

	if (build_item_manager::get().get_display_list()[m_scroll_view + 13] != 0)
		m_game->m_sprite[InterfaceNdGame2]->draw(sX + adj_x + 225, sY + adj_y + 230, 24);
	else m_game->m_sprite[InterfaceNdGame2]->draw(sX + adj_x + 225, sY + adj_y + 230, 24, hb::shared::sprite::DrawParams::tinted_alpha(GameColors::UIBlack.r, GameColors::UIBlack.g, GameColors::UIBlack.b, 0.7f));

	if ((lb != 0) && (m_game->get_dialog_box_manager().get_top_id() == DialogBoxId::Manufacture)) {
		if ((mouse_x >= sX + adj_x + 225) && (mouse_x <= sX + adj_x + 245) && (mouse_y >= sY + adj_y + 210) && (mouse_y <= sY + adj_y + 230)) {
			m_scroll_view--;
		}

		if ((mouse_x >= sX + adj_x + 225) && (mouse_x <= sX + adj_x + 245) && (mouse_y >= sY + adj_y + 230) && (mouse_y <= sY + adj_y + 250)) {
			if (build_item_manager::get().get_display_list()[m_scroll_view + 13] != 0)
				m_scroll_view++;
		}
	}
	if ((z != 0) && (m_game->get_dialog_box_manager().get_top_id() == DialogBoxId::Manufacture)) {
		m_scroll_view = m_scroll_view - z / 60;

	}
	if (build_item_manager::get().get_display_list()[m_scroll_view + 12] == 0)
	{
		while (1)
		{
			m_scroll_view--;
			if (m_scroll_view < 1) break;
			if (build_item_manager::get().get_display_list()[m_scroll_view + 12] != 0) break;
		}
	}
	if (m_scroll_view < 0) m_scroll_view = 0;

	put_aligned_string(sX, sX + m_size_x, sY + 265, DRAW_DIALOGBOX_SKILLDLG2, GameColors::UILabel);
	put_aligned_string(sX, sX + m_size_x, sY + 280, DRAW_DIALOGBOX_SKILLDLG3, GameColors::UILabel);
	put_aligned_string(sX, sX + m_size_x, sY + 295, DRAW_DIALOGBOX_SKILLDLG4, GameColors::UILabel);
	put_aligned_string(sX, sX + m_size_x, sY + 310, DRAW_DIALOGBOX_SKILLDLG5, GameColors::UILabel);
	put_aligned_string(sX, sX + m_size_x, sY + 340, DRAW_DIALOGBOX_SKILLDLG6, GameColors::UILabel);
}

void DialogBox_Manufacture::draw_manufacture_waiting(short sX, short sY)
{
	int adj_x = -1;
	int adj_y = -7;
	uint32_t time = m_game->m_cur_time;
	short size_x = m_size_x;
	std::string temp;
	int loc;

	draw_new_dialog_box(InterfaceNdGame3, sX, sY, 0);
	draw_new_dialog_box(InterfaceNdText, sX, sY, 8);
	{
		int recipe_cfg_id = m_game->find_item_id_by_name(build_item_manager::get().get_display_list()[m_progress]->m_name.c_str());
		CItem* recipe_cfg = m_game->get_item_config(recipe_cfg_id);
		auto recipe_draw = m_game->get_item_draw(recipe_cfg ? recipe_cfg->m_display_id : 0, item_atlas::pack, recipe_cfg ? recipe_cfg->sprite_is_female() : false);
		recipe_draw.sprite->draw(sX + adj_x + 62 + 5, sY + adj_y + 84 + 17, recipe_draw.frame);
	};

	auto itemInfo2 = item_name_formatter::get().format(m_game->find_item_id_by_name(build_item_manager::get().get_display_list()[m_progress]->m_name.c_str()),  0);
	temp = itemInfo2.name.c_str();
	put_string(sX + adj_x + 44 + 10 + 60, sY + adj_y + 55, temp.c_str(), GameColors::UIWhite);

	temp = std::format(DRAW_DIALOGBOX_SKILLDLG7, build_item_manager::get().get_display_list()[m_progress]->m_skill_limit, build_item_manager::get().get_display_list()[m_progress]->m_max_skill);
	put_string(sX + adj_x + 44 + 10 + 60, sY + adj_y + 55 + 2 * 15, temp.c_str(), GameColors::UILabel);
	put_string(sX + adj_x + 44 + 10 + 60, sY + adj_y + 55 + 3 * 15 + 5, DRAW_DIALOGBOX_SKILLDLG8, GameColors::UILabel);

	loc = 4;
	for (int elem = 1; elem <= 6; elem++)
	{
		if (build_item_manager::get().get_display_list()[m_progress]->m_element_count[elem] != 0)
		{
			const char* elemName = nullptr;
			bool elemFlag = false;
			switch (elem) {
			case 1: elemName = build_item_manager::get().get_display_list()[m_progress]->m_element_name_1.c_str(); elemFlag = build_item_manager::get().get_display_list()[m_progress]->m_element_flag[1]; break;
			case 2: elemName = build_item_manager::get().get_display_list()[m_progress]->m_element_name_2.c_str(); elemFlag = build_item_manager::get().get_display_list()[m_progress]->m_element_flag[2]; break;
			case 3: elemName = build_item_manager::get().get_display_list()[m_progress]->m_element_name_3.c_str(); elemFlag = build_item_manager::get().get_display_list()[m_progress]->m_element_flag[3]; break;
			case 4: elemName = build_item_manager::get().get_display_list()[m_progress]->m_element_name_4.c_str(); elemFlag = build_item_manager::get().get_display_list()[m_progress]->m_element_flag[4]; break;
			case 5: elemName = build_item_manager::get().get_display_list()[m_progress]->m_element_name_5.c_str(); elemFlag = build_item_manager::get().get_display_list()[m_progress]->m_element_flag[5]; break;
			case 6: elemName = build_item_manager::get().get_display_list()[m_progress]->m_element_name_6.c_str(); elemFlag = build_item_manager::get().get_display_list()[m_progress]->m_element_flag[6]; break;
			}
			auto itemInfo3 = item_name_formatter::get().format(m_game->find_item_id_by_name(elemName),  0);
			temp = itemInfo3.name.c_str();
			if (elemFlag)
				put_string(sX + adj_x + 44 + 20 + 60, sY + adj_y + 55 + loc * 15 + 5, temp.c_str(), GameColors::UILabel);
			else
				put_string(sX + adj_x + 44 + 20 + 60, sY + adj_y + 55 + loc * 15 + 5, temp.c_str(), GameColors::UIDisabled);
			loc++;
		}
	}

	if (build_item_manager::get().get_display_list()[m_progress]->m_build_enabled == true)
	{
		// draw ingredient slots
		for (int slot = 0; slot < 6; slot++)
		{
			int slotX = (slot % 3) * 45;
			int slotY = (slot / 3) * 45;
			m_game->m_sprite[InterfaceAddInterface]->draw(sX + adj_x + 55 + 30 + slotX + 13, sY + adj_y + 55 + slotY + 180, 2);
		}

		// draw items in slots
		if (m_slot_1 != -1) {
			CItem* cfg = m_game->get_item_config(player().m_item_list[m_slot_1]->m_id_num);
			if (cfg) { auto d = m_game->get_item_draw(cfg->m_display_id, item_atlas::pack, cfg->sprite_is_female());
			d.sprite->draw(sX + adj_x + 55 + 30 + 13, sY + adj_y + 55 + 180, d.frame); }
		}
		if (m_slot_2 != -1) {
			CItem* cfg = m_game->get_item_config(player().m_item_list[m_slot_2]->m_id_num);
			if (cfg) { auto d = m_game->get_item_draw(cfg->m_display_id, item_atlas::pack, cfg->sprite_is_female());
			d.sprite->draw(sX + adj_x + 55 + 45 + 30 + 13, sY + adj_y + 55 + 180, d.frame); }
		}
		if (m_slot_3 != -1) {
			CItem* cfg = m_game->get_item_config(player().m_item_list[m_slot_3]->m_id_num);
			if (cfg) { auto d = m_game->get_item_draw(cfg->m_display_id, item_atlas::pack, cfg->sprite_is_female());
			d.sprite->draw(sX + adj_x + 55 + 90 + 30 + 13, sY + adj_y + 55 + 180, d.frame); }
		}
		if (m_slot_4 != -1) {
			CItem* cfg = m_game->get_item_config(player().m_item_list[m_slot_4]->m_id_num);
			if (cfg) { auto d = m_game->get_item_draw(cfg->m_display_id, item_atlas::pack, cfg->sprite_is_female());
			d.sprite->draw(sX + adj_x + 55 + 30 + 13, sY + adj_y + 100 + 180, d.frame); }
		}
		if (m_slot_5 != -1) {
			CItem* cfg = m_game->get_item_config(player().m_item_list[m_slot_5]->m_id_num);
			if (cfg) { auto d = m_game->get_item_draw(cfg->m_display_id, item_atlas::pack, cfg->sprite_is_female());
			d.sprite->draw(sX + adj_x + 55 + 45 + 30 + 13, sY + adj_y + 100 + 180, d.frame); }
		}
		if (m_slot_6 != -1) {
			CItem* cfg = m_game->get_item_config(player().m_item_list[m_slot_6]->m_id_num);
			if (cfg) { auto d = m_game->get_item_draw(cfg->m_display_id, item_atlas::pack, cfg->sprite_is_female());
			d.sprite->draw(sX + adj_x + 55 + 90 + 30 + 13, sY + adj_y + 100 + 180, d.frame); }
		}

		put_aligned_string(sX, sX + size_x, sY + adj_y + 230 + 75, DRAW_DIALOGBOX_SKILLDLG15, GameColors::UILabel);
		put_aligned_string(sX, sX + size_x, sY + adj_y + 245 + 75, DRAW_DIALOGBOX_SKILLDLG16, GameColors::UILabel);
		put_aligned_string(sX, sX + size_x, sY + adj_y + 260 + 75, DRAW_DIALOGBOX_SKILLDLG17, GameColors::UILabel);

		if (mouse_in(btn_back_mfg))
			hb::shared::text::draw_text(GameFont::Bitmap1, sX + adj_x + 25, sY + adj_y + 330 + 23, "Back", hb::shared::text::TextStyle::with_highlight(GameColors::UIMagicBlue));
		else hb::shared::text::draw_text(GameFont::Bitmap1, sX + adj_x + 25, sY + adj_y + 330 + 23, "Back", hb::shared::text::TextStyle::with_highlight(GameColors::BmpBtnNormal));

		if (mouse_in(btn_manufacture)) {
			if (m_recipe_valid == 1)
				hb::shared::text::draw_text(GameFont::Bitmap1, sX + adj_x + 153, sY + adj_y + 330 + 23, "Manufacture", hb::shared::text::TextStyle::with_highlight(GameColors::UIMagicBlue));
			else hb::shared::text::draw_text(GameFont::Bitmap1, sX + adj_x + 153, sY + adj_y + 330 + 23, "Manufacture", hb::shared::text::TextStyle::with_highlight(GameColors::BmpBtnActive));
		}
		else {
			if (m_recipe_valid == 1)
				hb::shared::text::draw_text(GameFont::Bitmap1, sX + adj_x + 153, sY + adj_y + 330 + 23, "Manufacture", hb::shared::text::TextStyle::with_highlight(GameColors::BmpBtnNormal));
			else hb::shared::text::draw_text(GameFont::Bitmap1, sX + adj_x + 153, sY + adj_y + 330 + 23, "Manufacture", hb::shared::text::TextStyle::with_highlight(GameColors::BmpBtnActive));
		}
	}
	else {
		put_aligned_string(sX, sX + size_x, sY + adj_y + 200 + 75, DRAW_DIALOGBOX_SKILLDLG18, GameColors::UILabel);
		put_aligned_string(sX, sX + size_x, sY + adj_y + 215 + 75, DRAW_DIALOGBOX_SKILLDLG19, GameColors::UILabel);
		put_aligned_string(sX, sX + size_x, sY + adj_y + 230 + 75, DRAW_DIALOGBOX_SKILLDLG20, GameColors::UILabel);
		if (mouse_in(btn_back_mfg))
			hb::shared::text::draw_text(GameFont::Bitmap1, sX + adj_x + 25, sY + adj_y + 330 + 23, "Back", hb::shared::text::TextStyle::with_highlight(GameColors::UIMagicBlue));
		else hb::shared::text::draw_text(GameFont::Bitmap1, sX + adj_x + 25, sY + adj_y + 330 + 23, "Back", hb::shared::text::TextStyle::with_highlight(GameColors::BmpBtnNormal));
	}
}

void DialogBox_Manufacture::draw_manufacture_in_progress(short sX, short sY)
{
	int adj_x = -1;
	int adj_y = -7;
	uint32_t time = m_game->m_cur_time;
	short size_x = m_size_x;
	std::string temp;
	int loc;

	draw_new_dialog_box(InterfaceNdGame3, sX, sY, 0);
	draw_new_dialog_box(InterfaceNdText, sX, sY, 8);
	{
		int recipe_cfg_id = m_game->find_item_id_by_name(build_item_manager::get().get_display_list()[m_progress]->m_name.c_str());
		CItem* recipe_cfg = m_game->get_item_config(recipe_cfg_id);
		auto recipe_draw = m_game->get_item_draw(recipe_cfg ? recipe_cfg->m_display_id : 0, item_atlas::pack, recipe_cfg ? recipe_cfg->sprite_is_female() : false);
		recipe_draw.sprite->draw(sX + adj_x + 62 + 5, sY + adj_y + 84 + 17, recipe_draw.frame);
	};

	auto itemInfo4 = item_name_formatter::get().format(m_game->find_item_id_by_name(build_item_manager::get().get_display_list()[m_progress]->m_name.c_str()),  0);
	temp = itemInfo4.name.c_str();
	put_string(sX + adj_x + 44 + 10 + 60, sY + adj_y + 55, temp.c_str(), GameColors::UIWhite);

	temp = std::format(DRAW_DIALOGBOX_SKILLDLG7, build_item_manager::get().get_display_list()[m_progress]->m_skill_limit, build_item_manager::get().get_display_list()[m_progress]->m_max_skill);
	put_string(sX + adj_x + 44 + 10 + 60, sY + adj_y + 55 + 2 * 15, temp.c_str(), GameColors::UILabel);
	put_string(sX + adj_x + 44 + 10 + 60, sY + adj_y + 55 + 3 * 15 + 5, DRAW_DIALOGBOX_SKILLDLG8, GameColors::UILabel);

	loc = 4;
	for (int elem = 1; elem <= 6; elem++)
	{
		if (build_item_manager::get().get_display_list()[m_progress]->m_element_count[elem] != 0)
		{
			const char* elemName = nullptr;
			bool elemFlag = false;
			switch (elem) {
			case 1: elemName = build_item_manager::get().get_display_list()[m_progress]->m_element_name_1.c_str(); elemFlag = build_item_manager::get().get_display_list()[m_progress]->m_element_flag[1]; break;
			case 2: elemName = build_item_manager::get().get_display_list()[m_progress]->m_element_name_2.c_str(); elemFlag = build_item_manager::get().get_display_list()[m_progress]->m_element_flag[2]; break;
			case 3: elemName = build_item_manager::get().get_display_list()[m_progress]->m_element_name_3.c_str(); elemFlag = build_item_manager::get().get_display_list()[m_progress]->m_element_flag[3]; break;
			case 4: elemName = build_item_manager::get().get_display_list()[m_progress]->m_element_name_4.c_str(); elemFlag = build_item_manager::get().get_display_list()[m_progress]->m_element_flag[4]; break;
			case 5: elemName = build_item_manager::get().get_display_list()[m_progress]->m_element_name_5.c_str(); elemFlag = build_item_manager::get().get_display_list()[m_progress]->m_element_flag[5]; break;
			case 6: elemName = build_item_manager::get().get_display_list()[m_progress]->m_element_name_6.c_str(); elemFlag = build_item_manager::get().get_display_list()[m_progress]->m_element_flag[6]; break;
			}
			auto itemInfo5 = item_name_formatter::get().format(m_game->find_item_id_by_name(elemName),  0);
			temp = itemInfo5.name.c_str();
			if (elemFlag)
				put_string(sX + adj_x + 44 + 20 + 60, sY + adj_y + 55 + loc * 15 + 5, temp.c_str(), GameColors::UILabel);
			else
				put_string(sX + adj_x + 44 + 20 + 60, sY + adj_y + 55 + loc * 15 + 5, temp.c_str(), GameColors::UIDisabledMed);
			loc++;
		}
	}

	// draw ingredient slots
	for (int slot = 0; slot < 6; slot++)
	{
		int slotX = (slot % 3) * 45;
		int slotY = (slot / 3) * 45;
		m_game->m_sprite[InterfaceAddInterface]->draw(sX + adj_x + 55 + 30 + slotX + 13, sY + adj_y + 55 + slotY + 180, 2);
	}

	// draw items in slots
	if (m_slot_1 != -1) {
		CItem* cfg = m_game->get_item_config(player().m_item_list[m_slot_1]->m_id_num);
		if (cfg) { auto d = m_game->get_item_draw(cfg->m_display_id, item_atlas::pack, cfg->sprite_is_female());
		d.sprite->draw(sX + adj_x + 55 + 30 + 13, sY + adj_y + 55 + 180, d.frame); }
	}
	if (m_slot_2 != -1) {
		CItem* cfg = m_game->get_item_config(player().m_item_list[m_slot_2]->m_id_num);
		if (cfg) { auto d = m_game->get_item_draw(cfg->m_display_id, item_atlas::pack, cfg->sprite_is_female());
		d.sprite->draw(sX + adj_x + 55 + 45 + 30 + 13, sY + adj_y + 55 + 180, d.frame); }
	}
	if (m_slot_3 != -1) {
		CItem* cfg = m_game->get_item_config(player().m_item_list[m_slot_3]->m_id_num);
		if (cfg) { auto d = m_game->get_item_draw(cfg->m_display_id, item_atlas::pack, cfg->sprite_is_female());
		d.sprite->draw(sX + adj_x + 55 + 90 + 30 + 13, sY + adj_y + 55 + 180, d.frame); }
	}
	if (m_slot_4 != -1) {
		CItem* cfg = m_game->get_item_config(player().m_item_list[m_slot_4]->m_id_num);
		if (cfg) { auto d = m_game->get_item_draw(cfg->m_display_id, item_atlas::pack, cfg->sprite_is_female());
		d.sprite->draw(sX + adj_x + 55 + 30 + 13, sY + adj_y + 100 + 180, d.frame); }
	}
	if (m_slot_5 != -1) {
		CItem* cfg = m_game->get_item_config(player().m_item_list[m_slot_5]->m_id_num);
		if (cfg) { auto d = m_game->get_item_draw(cfg->m_display_id, item_atlas::pack, cfg->sprite_is_female());
		d.sprite->draw(sX + adj_x + 55 + 45 + 30 + 13, sY + adj_y + 100 + 180, d.frame); }
	}
	if (m_slot_6 != -1) {
		CItem* cfg = m_game->get_item_config(player().m_item_list[m_slot_6]->m_id_num);
		if (cfg) { auto d = m_game->get_item_draw(cfg->m_display_id, item_atlas::pack, cfg->sprite_is_female());
		d.sprite->draw(sX + adj_x + 55 + 90 + 30 + 13, sY + adj_y + 100 + 180, d.frame); }
	}

	put_string(sX + adj_x + 33, sY + adj_y + 230 + 75, DRAW_DIALOGBOX_SKILLDLG29, GameColors::UILabel);
	put_string(sX + adj_x + 33, sY + adj_y + 245 + 75, DRAW_DIALOGBOX_SKILLDLG30, GameColors::UILabel);

	if ((time - m_anim_timer) > 1000)
	{
		m_anim_timer = time;
		m_anim_frame++;
		if (m_anim_frame >= 7) m_anim_frame = 7;
	}

	if (m_anim_frame == 4)
	{
		{
			hb::net::PacketCommandCommonBuild req{};
			req.base.header.msg_id = MsgId::CommandCommon;
			req.base.header.msg_type = CommonType::BuildItem;
			req.base.x = player().m_player_x;
			req.base.y = player().m_player_y;
			const char* name = build_item_manager::get().get_display_list()[m_progress]->m_name.c_str();
			std::size_t name_len = std::strlen(name);
			if (name_len > sizeof(req.name)) name_len = sizeof(req.name);
			std::memcpy(req.name, name, name_len);
			req.item_ids[0] = static_cast<uint8_t>(m_slot_1);
			req.item_ids[1] = static_cast<uint8_t>(m_slot_2);
			req.item_ids[2] = static_cast<uint8_t>(m_slot_3);
			req.item_ids[3] = static_cast<uint8_t>(m_slot_4);
			req.item_ids[4] = static_cast<uint8_t>(m_slot_5);
			req.item_ids[5] = static_cast<uint8_t>(m_slot_6);
			m_game->send_game_packet(req, false);
		}
		m_anim_frame++;
	}
}

void DialogBox_Manufacture::draw_manufacture_done(short sX, short sY)
{
	int adj_x = -1;
	int adj_y = -7;
	uint32_t time = m_game->m_cur_time;
	std::string temp;

	draw_new_dialog_box(InterfaceNdGame3, sX, sY, 0);
	draw_new_dialog_box(InterfaceNdText, sX, sY, 8);
	{
		int recipe_cfg_id = m_game->find_item_id_by_name(build_item_manager::get().get_display_list()[m_progress]->m_name.c_str());
		CItem* recipe_cfg = m_game->get_item_config(recipe_cfg_id);
		auto recipe_draw = m_game->get_item_draw(recipe_cfg ? recipe_cfg->m_display_id : 0, item_atlas::pack, recipe_cfg ? recipe_cfg->sprite_is_female() : false);
		recipe_draw.sprite->draw(sX + adj_x + 62 + 5, sY + adj_y + 84 + 17, recipe_draw.frame);
	};

	auto itemInfo6 = item_name_formatter::get().format(m_game->find_item_id_by_name(build_item_manager::get().get_display_list()[m_progress]->m_name.c_str()),  0);

	temp = itemInfo6.name.c_str();
	put_string(sX + adj_x + 44 + 10 + 60, sY + adj_y + 55, temp.c_str(), GameColors::UIWhite);

	if (m_result_flag == 1) {
		put_string(sX + adj_x + 33 + 11, sY + adj_y + 200 - 45, DRAW_DIALOGBOX_SKILLDLG31, GameColors::UILabel);

		std::string resultBuf;
		if (static_cast<ItemType>(m_slot_1) == ItemType::Material) {
			resultBuf = std::format(DRAW_DIALOGBOX_SKILLDLG32, m_result_value);
			put_string(sX + adj_x + 33 + 11, sY + adj_y + 215 - 45, resultBuf.c_str(), GameColors::UILabel);
		}
		else {
			resultBuf = std::format(DRAW_DIALOGBOX_SKILLDLG33, static_cast<int>(m_result_value) + 100);
			put_string(sX + adj_x + 33, sY + adj_y + 215 - 45, resultBuf.c_str(), GameColors::UILabel);
		}
	}
	else {
		put_string(sX + adj_x + 33 + 11, sY + adj_y + 200, DRAW_DIALOGBOX_SKILLDLG34, GameColors::UILabel);
	}

	if (mouse_in(btn_back_mfg))
		hb::shared::text::draw_text(GameFont::Bitmap1, sX + adj_x + 35, sY + adj_y + 330 + 23, "Back", hb::shared::text::TextStyle::with_highlight(GameColors::UIMagicBlue));
	else hb::shared::text::draw_text(GameFont::Bitmap1, sX + adj_x + 35, sY + adj_y + 330 + 23, "Back", hb::shared::text::TextStyle::with_highlight(GameColors::BmpBtnNormal));
}

void DialogBox_Manufacture::draw_crafting_waiting(short sX, short sY)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	int adj_x = 5;
	int adj_y = 8;
	uint32_t time = m_game->m_cur_time;

	m_game->m_sprite[InterfaceCrafting]->draw(sX, sY, 0);

	if (m_slot_1 != -1) {
		CItem* cfg = m_game->get_item_config(player().m_item_list[m_slot_1]->m_id_num);
		if (cfg) { auto d = m_game->get_item_draw(cfg->m_display_id, item_atlas::pack, cfg->sprite_is_female());
		d.sprite->draw(sX + adj_x + 55 + (1 - (rand() % 3)), sY + adj_y + 55 + (1 - (rand() % 3)), d.frame); }
	}
	if (m_slot_2 != -1) {
		CItem* cfg = m_game->get_item_config(player().m_item_list[m_slot_2]->m_id_num);
		if (cfg) { auto d = m_game->get_item_draw(cfg->m_display_id, item_atlas::pack, cfg->sprite_is_female());
		d.sprite->draw(sX + adj_x + 65 + 45 + (1 - (rand() % 3)), sY + adj_y + 40 + (1 - (rand() % 3)), d.frame); }
	}
	if (m_slot_3 != -1) {
		CItem* cfg = m_game->get_item_config(player().m_item_list[m_slot_3]->m_id_num);
		if (cfg) { auto d = m_game->get_item_draw(cfg->m_display_id, item_atlas::pack, cfg->sprite_is_female());
		d.sprite->draw(sX + adj_x + 65 + 90 + (1 - (rand() % 3)), sY + adj_y + 55 + (1 - (rand() % 3)), d.frame); }
	}
	if (m_slot_4 != -1) {
		CItem* cfg = m_game->get_item_config(player().m_item_list[m_slot_4]->m_id_num);
		if (cfg) { auto d = m_game->get_item_draw(cfg->m_display_id, item_atlas::pack, cfg->sprite_is_female());
		d.sprite->draw(sX + adj_x + 65 + (1 - (rand() % 3)), sY + adj_y + 100 + (1 - (rand() % 3)), d.frame); }
	}
	if (m_slot_5 != -1) {
		CItem* cfg = m_game->get_item_config(player().m_item_list[m_slot_5]->m_id_num);
		if (cfg) { auto d = m_game->get_item_draw(cfg->m_display_id, item_atlas::pack, cfg->sprite_is_female());
		d.sprite->draw(sX + adj_x + 65 + 45 + (1 - (rand() % 3)), sY + adj_y + 115 + (1 - (rand() % 3)), d.frame); }
	}
	if (m_slot_6 != -1) {
		CItem* cfg = m_game->get_item_config(player().m_item_list[m_slot_6]->m_id_num);
		if (cfg) { auto d = m_game->get_item_draw(cfg->m_display_id, item_atlas::pack, cfg->sprite_is_female());
		d.sprite->draw(sX + adj_x + 75 + 90 + (1 - (rand() % 3)), sY + adj_y + 100 + (1 - (rand() % 3)), d.frame); }
	}

	if ((mouse_x >= sX + adj_x + 60) && (mouse_x <= sX + adj_x + 153) && (mouse_y >= sY + adj_y + 175) && (mouse_y <= sY + adj_y + 195))
		hb::shared::text::draw_text(GameFont::Bitmap1, sX + adj_x + 60, sY + adj_y + 175, "Try Now!", hb::shared::text::TextStyle::with_highlight(GameColors::BmpBtnBlue));
	else hb::shared::text::draw_text(GameFont::Bitmap1, sX + adj_x + 60, sY + adj_y + 175, "Try Now!", hb::shared::text::TextStyle::with_highlight(GameColors::UIMagicBlue));
}

void DialogBox_Manufacture::draw_crafting_in_progress(short sX, short sY)
{
	int adj_x = 5;
	int adj_y = 8;
	uint32_t time = m_game->m_cur_time;

	m_game->m_sprite[InterfaceCrafting]->draw(sX, sY, 0);

	if (m_slot_1 != -1)
	{
		CItem* cfg = m_game->get_item_config(player().m_item_list[m_slot_1]->m_id_num);
		if (cfg) {
			auto d = m_game->get_item_draw(cfg->m_display_id, item_atlas::pack, cfg->sprite_is_female());
			d.sprite->draw(sX + adj_x + 55 + (1 - (rand() % 3)), sY + adj_y + 55 + (1 - (rand() % 3)), d.frame);
			if ((cfg->get_item_type() == ItemType::Equip) && (cfg->get_equip_pos() == EquipPos::Neck))
				m_game->m_contribution_price = 10;
		}
	}
	if (m_slot_2 != -1)
	{
		CItem* cfg = m_game->get_item_config(player().m_item_list[m_slot_2]->m_id_num);
		if (cfg) {
			auto d = m_game->get_item_draw(cfg->m_display_id, item_atlas::pack, cfg->sprite_is_female());
			d.sprite->draw(sX + adj_x + 65 + 45 + (1 - (rand() % 3)), sY + adj_y + 40 + (1 - (rand() % 3)), d.frame);
			if ((cfg->get_item_type() == ItemType::Equip) && (cfg->get_equip_pos() == EquipPos::Neck))
				m_game->m_contribution_price = 10;
		}
	}
	if (m_slot_3 != -1)
	{
		CItem* cfg = m_game->get_item_config(player().m_item_list[m_slot_3]->m_id_num);
		if (cfg) {
			auto d = m_game->get_item_draw(cfg->m_display_id, item_atlas::pack, cfg->sprite_is_female());
			d.sprite->draw(sX + adj_x + 65 + 90 + (1 - (rand() % 3)), sY + adj_y + 55 + (1 - (rand() % 3)), d.frame);
			if ((cfg->get_item_type() == ItemType::Equip) && (cfg->get_equip_pos() == EquipPos::Neck))
				m_game->m_contribution_price = 10;
		}
	}
	if (m_slot_4 != -1)
	{
		CItem* cfg = m_game->get_item_config(player().m_item_list[m_slot_4]->m_id_num);
		if (cfg) {
			auto d = m_game->get_item_draw(cfg->m_display_id, item_atlas::pack, cfg->sprite_is_female());
			d.sprite->draw(sX + adj_x + 65 + (1 - (rand() % 3)), sY + adj_y + 100 + (1 - (rand() % 3)), d.frame);
			if ((cfg->get_item_type() == ItemType::Equip) && (cfg->get_equip_pos() == EquipPos::Neck))
				m_game->m_contribution_price = 10;
		}
	}
	if (m_slot_5 != -1)
	{
		CItem* cfg = m_game->get_item_config(player().m_item_list[m_slot_5]->m_id_num);
		if (cfg) {
			auto d = m_game->get_item_draw(cfg->m_display_id, item_atlas::pack, cfg->sprite_is_female());
			d.sprite->draw(sX + adj_x + 65 + 45 + (1 - (rand() % 3)), sY + adj_y + 115 + (1 - (rand() % 3)), d.frame);
			if ((cfg->get_item_type() == ItemType::Equip) && (cfg->get_equip_pos() == EquipPos::Neck))
				m_game->m_contribution_price = 10;
		}
	}
	if (m_slot_6 != -1)
	{
		CItem* cfg = m_game->get_item_config(player().m_item_list[m_slot_6]->m_id_num);
		if (cfg) {
			auto d = m_game->get_item_draw(cfg->m_display_id, item_atlas::pack, cfg->sprite_is_female());
			d.sprite->draw(sX + adj_x + 75 + 90 + (1 - (rand() % 3)), sY + adj_y + 100 + (1 - (rand() % 3)), d.frame);
			if ((cfg->get_item_type() == ItemType::Equip) && (cfg->get_equip_pos() == EquipPos::Neck))
				m_game->m_contribution_price = 10;
		}
	}

	hb::shared::text::draw_text(GameFont::Bitmap1, sX + adj_x + 60, sY + adj_y + 175, "Creating...", hb::shared::text::TextStyle::with_highlight(GameColors::BmpBtnRed));

	if ((time - m_anim_timer) > 1000)
	{
		m_anim_timer = time;
		m_anim_frame++;
	}
	if (m_anim_frame >= 5)
	{
		{
			hb::net::PacketCommandCommonBuild req{};
			req.base.header.msg_id = MsgId::CommandCommon;
			req.base.header.msg_type = CommonType::CraftItem;
			req.base.x = player().m_player_x;
			req.base.y = player().m_player_y;
			std::memset(req.name, ' ', sizeof(req.name));
			req.item_ids[0] = static_cast<uint8_t>(m_slot_1);
			req.item_ids[1] = static_cast<uint8_t>(m_slot_2);
			req.item_ids[2] = static_cast<uint8_t>(m_slot_3);
			req.item_ids[3] = static_cast<uint8_t>(m_slot_4);
			req.item_ids[4] = static_cast<uint8_t>(m_slot_5);
			req.item_ids[5] = static_cast<uint8_t>(m_slot_6);
			m_game->send_game_packet(req, false);
		}
		m_game->get_dialog_box_manager().disable_dialog_box(DialogBoxId::Manufacture);
		m_game->play_game_sound('E', 42, 0);
	}
}

bool DialogBox_Manufacture::on_click()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	short sX = m_x;
	short sY = m_y;
	int adj_x = 5;
	int adj_y = 8;

	switch (m_mode) {
	case mode::alchemy_waiting:
		if (mouse_in(btn_try_now))
		{
			m_mode = mode::alchemy_creating;
			m_progress = 1;
			m_anim_timer = m_game->m_cur_time;
			m_game->play_game_sound('E', 14, 5);
			m_game->add_event_list(DLGBOX_CLICK_SKILLDLG1, 10);
			m_game->play_game_sound('E', 41, 0);
			return true;
		}
		break;

	case mode::crafting_waiting:
		if (mouse_in(btn_try_now))
		{
			if (m_slot_1 == -1)
			{
				m_game->add_event_list(DLGBOX_CLICK_SKILLDLG2, 10);
				m_game->play_game_sound('E', 14, 5);
			}
			else
			{
				m_mode = mode::crafting_in_progress;
				m_anim_timer = m_game->m_cur_time;
				m_anim_frame = 1;
				m_game->play_game_sound('E', 14, 5);
				m_game->add_event_list(DLGBOX_CLICK_SKILLDLG3, 10);
				m_game->play_game_sound('E', 51, 0);
			}
			return true;
		}
		break;

	case mode::manufacture_list:
		adj_x = 5;
		adj_y = 8;
		for (int i = 0; i < 13; i++)
			if (build_item_manager::get().get_display_list()[i + m_scroll_view] != 0)
			{
				if ((mouse_x >= sX + adj_x + 44) && (mouse_x <= sX + adj_x + 135 + 44) && (mouse_y >= sY + adj_y + 55 + i * 15) && (mouse_y <= sY + adj_y + 55 + 14 + i * 15)) {
					m_mode = mode::manufacture_waiting;
					m_progress = i + m_scroll_view;
					m_game->play_game_sound('E', 14, 5);
					return true;
				}
			}
		break;

	case mode::manufacture_waiting:
		if (build_item_manager::get().get_display_list()[m_progress]->m_build_enabled == true)
		{
			if (mouse_in(btn_back_mfg)) {
				// Back
				reset_item_slots();
				m_mode = mode::manufacture_list;
				m_game->play_game_sound('E', 14, 5);
				return true;
			}

			if (mouse_in(btn_manufacture))
			{
				// Manufacture
				if (m_recipe_valid == 1)
				{
					m_mode = mode::manufacture_in_progress;
					m_anim_frame = 0;
					m_anim_timer = m_game->m_cur_time;
					m_game->play_game_sound('E', 14, 5);
					m_game->play_game_sound('E', 44, 0);
				}
				return true;
			}
		}
		else
		{
			if (mouse_in(btn_back_mfg))
			{
				// Back
				reset_item_slots();
				m_mode = mode::manufacture_list;
				m_game->play_game_sound('E', 14, 5);
				return true;
			}
		}
		break;

	case mode::manufacture_done:
		if (mouse_in(btn_back_mfg)) {
			// Back
			reset_item_slots();
			m_mode = mode::manufacture_list;
			m_game->play_game_sound('E', 14, 5);
			return true;
		}
		break;
	}

	return false;
}

// Helper: Check if clicking on item in a manufacture slot and handle selection
bool DialogBox_Manufacture::check_slot_item_click(int slotIndex, int itemIdx, int drawX, int drawY)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	if (itemIdx == -1 || player().m_item_list[itemIdx] == nullptr)
		return false;

	CItem* cfg = m_game->get_item_config(player().m_item_list[itemIdx]->m_id_num);
	if (!cfg) return false;

	auto mfg_draw = m_game->get_item_draw(cfg->m_display_id, item_atlas::pack, cfg->sprite_is_female());
	mfg_draw.sprite->CalculateBounds(drawX, drawY, mfg_draw.frame);
	auto bounds = mfg_draw.sprite->GetBoundRect();

	if (mouse_x > bounds.left && mouse_x < bounds.right && mouse_y > bounds.top && mouse_y < bounds.bottom)
	{
		// clear the slot
		switch (slotIndex)
		{
		case 1: m_slot_1 = -1; break;
		case 2: m_slot_2 = -1; break;
		case 3: m_slot_3 = -1; break;
		case 4: m_slot_4 = -1; break;
		case 5: m_slot_5 = -1; break;
		case 6: m_slot_6 = -1; break;
		}
		inventory_manager::get().unlock_item(itemIdx);
		CursorTarget::set_selection(SelectedObjectType::Item, itemIdx, mouse_x - drawX, mouse_y - drawY);
		return true;
	}
	return false;
}

PressResult DialogBox_Manufacture::on_press()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	short sX = m_x;
	short sY = m_y;
	const int adj_x = 5;
	const int adj_y = 10;

	short array[7] = { 0 };
	array[1] = m_slot_1;
	array[2] = m_slot_2;
	array[3] = m_slot_3;
	array[4] = m_slot_4;
	array[5] = m_slot_5;
	array[6] = m_slot_6;

	switch (m_mode)
	{
	case mode::alchemy_waiting:
		for (int i = 1; i <= 6; i++)
		{
			int itemDrawX, itemDrawY;
			switch (i)
			{
			case 1: itemDrawX = sX + adj_x + 55;          itemDrawY = sY + adj_y + 55; break;
			case 2: itemDrawX = sX + adj_x + 55 + 45 * 1; itemDrawY = sY + adj_y + 55; break;
			case 3: itemDrawX = sX + adj_x + 55 + 45 * 2; itemDrawY = sY + adj_y + 55; break;
			case 4: itemDrawX = sX + adj_x + 55;          itemDrawY = sY + adj_y + 100; break;
			case 5: itemDrawX = sX + adj_x + 55 + 45 * 1; itemDrawY = sY + adj_y + 100; break;
			case 6: itemDrawX = sX + adj_x + 55 + 45 * 2; itemDrawY = sY + adj_y + 100; break;
			default: continue;
			}
			if (check_slot_item_click(i, array[i], itemDrawX, itemDrawY))
				return PressResult::ItemSelected;
		}
		break;

	case mode::manufacture_waiting:
		for (int i = 1; i <= 6; i++)
		{
			int itemDrawX, itemDrawY;
			switch (i)
			{
			case 1: itemDrawX = sX + adj_x + 55 + 30 + 13;          itemDrawY = sY + adj_y + 55 + 180; break;
			case 2: itemDrawX = sX + adj_x + 55 + 45 * 1 + 30 + 13; itemDrawY = sY + adj_y + 55 + 180; break;
			case 3: itemDrawX = sX + adj_x + 55 + 45 * 2 + 30 + 13; itemDrawY = sY + adj_y + 55 + 180; break;
			case 4: itemDrawX = sX + adj_x + 55 + 30 + 13;          itemDrawY = sY + adj_y + 100 + 180; break;
			case 5: itemDrawX = sX + adj_x + 55 + 45 * 1 + 30 + 13; itemDrawY = sY + adj_y + 100 + 180; break;
			case 6: itemDrawX = sX + adj_x + 55 + 45 * 2 + 30 + 13; itemDrawY = sY + adj_y + 100 + 180; break;
			default: continue;
			}
			if (check_slot_item_click(i, array[i], itemDrawX, itemDrawY))
			{
				m_recipe_valid = static_cast<char>(build_item_manager::get().validate_current_recipe());
				return PressResult::ItemSelected;
			}
		}
		break;

	case mode::crafting_waiting:
		for (int i = 1; i <= 6; i++)
		{
			int itemDrawX, itemDrawY;
			switch (i)
			{
			case 1: itemDrawX = sX + adj_x + 55;          itemDrawY = sY + adj_y + 55; break;
			case 2: itemDrawX = sX + adj_x + 65 + 45 * 1; itemDrawY = sY + adj_y + 40; break;
			case 3: itemDrawX = sX + adj_x + 65 + 45 * 2; itemDrawY = sY + adj_y + 55; break;
			case 4: itemDrawX = sX + adj_x + 65;          itemDrawY = sY + adj_y + 100; break;
			case 5: itemDrawX = sX + adj_x + 65 + 45 * 1; itemDrawY = sY + adj_y + 115; break;
			case 6: itemDrawX = sX + adj_x + 75 + 45 * 2; itemDrawY = sY + adj_y + 100; break;
			default: continue;
			}
			if (check_slot_item_click(i, array[i], itemDrawX, itemDrawY))
				return PressResult::ItemSelected;
		}
		break;
	}

	return PressResult::Normal;
}

bool DialogBox_Manufacture::try_add_item_to_slot(int item_id, bool updateBuildStatus)
{
	int* slots[] = { &m_slot_1, &m_slot_2, &m_slot_3, &m_slot_4, &m_slot_5, &m_slot_6 };

	for (int i = 0; i < 6; i++)
	{
		if (*slots[i] == -1)
		{
			*slots[i] = item_id;
			if (updateBuildStatus)
				m_recipe_valid = static_cast<char>(build_item_manager::get().validate_current_recipe());

			// Only disable non-stackable items (stackable consumables can be added multiple times)
			CItem* cfg = m_game->get_item_config(player().m_item_list[item_id]->m_id_num);
			if (!cfg || cfg->get_item_type() != ItemType::Consume ||
				player().m_item_list[item_id]->m_count <= 1)
			{
				inventory_manager::get().lock_item(item_id);
			}
			return true;
		}
	}
	return false;
}

bool DialogBox_Manufacture::on_item_drop()
{
	if (player().m_Controller.get_command() < 0) return false;

	int item_id = CursorTarget::get_selected_id();
	if (item_id < 0 || item_id >= hb::shared::limits::MaxItems) return false;
	if (player().m_item_list[item_id] == nullptr) return false;
	if (inventory_manager::get().is_locked(item_id)) return false;
	CItem* cfg = m_game->get_item_config(player().m_item_list[item_id]->m_id_num);
	if (!cfg) return false;

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
	case mode::alchemy_waiting:
	{
		// Check consumable item count - can't add if all instances are already used
		if (cfg->get_item_type() == ItemType::Consume)
		{
			int consume_num = 0;
			if (m_slot_1 == item_id) consume_num++;
			if (m_slot_2 == item_id) consume_num++;
			if (m_slot_3 == item_id) consume_num++;
			if (m_slot_4 == item_id) consume_num++;
			if (m_slot_5 == item_id) consume_num++;
			if (m_slot_6 == item_id) consume_num++;
			if (consume_num >= static_cast<int>(player().m_item_list[item_id]->m_count)) return false;
		}

		// Only allow EAT, CONSUME, or NONE item types for alchemy
		if (cfg->get_item_type() != ItemType::Eat &&
			cfg->get_item_type() != ItemType::Consume &&
			cfg->get_item_type() != ItemType::None)
		{
			return false;
		}

		if (!try_add_item_to_slot(item_id, false))
			add_event_list(BITEMDROP_SKILLDIALOG4, 10);
		break;
	}

	case mode::manufacture_waiting:
	{
		// Check consumable item count
		if (cfg->get_item_type() == ItemType::Consume)
		{
			int consume_num = 0;
			if (m_slot_1 == item_id) consume_num++;
			if (m_slot_2 == item_id) consume_num++;
			if (m_slot_3 == item_id) consume_num++;
			if (m_slot_4 == item_id) consume_num++;
			if (m_slot_5 == item_id) consume_num++;
			if (m_slot_6 == item_id) consume_num++;
			if (consume_num >= static_cast<int>(player().m_item_list[item_id]->m_count)) return false;
		}

		if (!try_add_item_to_slot(item_id, true))
			add_event_list(BITEMDROP_SKILLDIALOG4, 10);
		break;
	}

	case mode::crafting_waiting:
	{
		// Only allow specific item types for crafting
		if (cfg->get_item_type() != ItemType::None &&      // Merien Stone
			cfg->get_item_type() != ItemType::Equip &&     // Necklaces, Rings
			cfg->get_item_type() != ItemType::Consume &&   // Stones
			cfg->get_item_type() != ItemType::Material)    // Craftwares
		{
			return false;
		}

		if (!try_add_item_to_slot(item_id, false))
			add_event_list(BITEMDROP_SKILLDIALOG4, 10);
		break;
	}
	}

	return true;
}

void DialogBox_Manufacture::reset_item_slots()
{
	if ((m_slot_1 != -1) && (player().m_item_list[m_slot_1] != 0))
		inventory_manager::get().unlock_item(m_slot_1);
	if ((m_slot_2 != -1) && (player().m_item_list[m_slot_2] != 0))
		inventory_manager::get().unlock_item(m_slot_2);
	if ((m_slot_3 != -1) && (player().m_item_list[m_slot_3] != 0))
		inventory_manager::get().unlock_item(m_slot_3);
	if ((m_slot_4 != -1) && (player().m_item_list[m_slot_4] != 0))
		inventory_manager::get().unlock_item(m_slot_4);
	if ((m_slot_5 != -1) && (player().m_item_list[m_slot_5] != 0))
		inventory_manager::get().unlock_item(m_slot_5);
	if ((m_slot_6 != -1) && (player().m_item_list[m_slot_6] != 0))
		inventory_manager::get().unlock_item(m_slot_6);

	m_slot_1 = -1;
	m_slot_2 = -1;
	m_slot_3 = -1;
	m_slot_4 = -1;
	m_slot_5 = -1;
	m_slot_6 = -1;
	m_progress = 0;
	m_anim_frame = 0;
	m_recipe_valid = 0;
}

bool DialogBox_Manufacture::on_enable(int type, int64_t v1, int v2, const char* string)
{
	switch (type) {
	case DialogBoxId::CharacterInfo:
	case DialogBoxId::Inventory:
		if (!is_enabled())
		{
			m_mode = static_cast<mode>(type);
			m_slot_1 = -1;
			m_slot_2 = -1;
			m_slot_3 = -1;
			m_slot_4 = -1;
			m_slot_5 = -1;
			m_slot_6 = -1;
			m_progress = 0;
			m_game->on_game()->m_skill_using_status = true;
			m_size_x = 195;
			m_size_y = 215;
			disable_dialog_box(DialogBoxId::ItemDropExternal);
			disable_dialog_box(DialogBoxId::NpcActionQuery);
			disable_dialog_box(DialogBoxId::SellOrRepair);
		}
		break;

	case DialogBoxId::Magic:
		if (!is_enabled())
		{
			m_scroll_view = 0;
			m_mode = static_cast<mode>(type);
			m_slot_1 = -1;
			m_slot_2 = -1;
			m_slot_3 = -1;
			m_slot_4 = -1;
			m_slot_5 = -1;
			m_slot_6 = -1;
			m_progress = 0;
			m_anim_frame = 0;
			m_recipe_valid = 0;
			m_game->on_game()->m_skill_using_status = true;
			build_item_manager::get().update_available_recipes();
			m_size_x = 270;
			m_size_y = 381;
			disable_dialog_box(DialogBoxId::ItemDropExternal);
			disable_dialog_box(DialogBoxId::NpcActionQuery);
			disable_dialog_box(DialogBoxId::SellOrRepair);
		}
		break;

	case DialogBoxId::WarningBattleArea:
		if (!is_enabled())
		{
			m_mode = static_cast<mode>(type);
			m_result_flag = static_cast<char>(v1);
			m_result_value = v2;
			m_size_x = 270;
			m_size_y = 381;
			m_game->on_game()->m_skill_using_status = true;
			build_item_manager::get().update_available_recipes();
			disable_dialog_box(DialogBoxId::ItemDropExternal);
			disable_dialog_box(DialogBoxId::NpcActionQuery);
			disable_dialog_box(DialogBoxId::SellOrRepair);
		}
		break;

	case DialogBoxId::GuildMenu:
	case DialogBoxId::GuildOperation:
		if (!is_enabled())
		{
			m_mode = static_cast<mode>(type);
			m_slot_1 = -1;
			m_slot_2 = -1;
			m_slot_3 = -1;
			m_slot_4 = -1;
			m_slot_5 = -1;
			m_slot_6 = -1;
			m_progress = 0;
			m_anim_frame = 0;
			m_game->on_game()->m_skill_using_status = true;
			m_size_x = 195;
			m_size_y = 215;
			disable_dialog_box(DialogBoxId::ItemDropExternal);
			disable_dialog_box(DialogBoxId::NpcActionQuery);
			disable_dialog_box(DialogBoxId::SellOrRepair);
		}
		break;
	}
	return true;
}

bool DialogBox_Manufacture::on_disable()
{
	auto clearItem = [](int idx) { inventory_manager::get().unlock_item(idx); };
	clearItem(m_slot_1); clearItem(m_slot_2); clearItem(m_slot_3);
	clearItem(m_slot_4); clearItem(m_slot_5); clearItem(m_slot_6);
	m_game->on_game()->m_skill_using_status = false;
	return true;
}
