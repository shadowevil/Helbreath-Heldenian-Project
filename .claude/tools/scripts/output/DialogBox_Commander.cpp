#include "DialogBox_Commander.h"
#include "Game.h"
#include "TeleportManager.h"
#include "lan_eng.h"
#include "GlobalDef.h"
#include "SpriteID.h"
#include "ConfigManager.h"
#include "NetMessages.h"
#include "PacketSendHelpers.h"

#include "GameFonts.h"
#include "TextLibExt.h"
#include <format>
#include "IInput.h"
#include "Screen_OnGame.h"

using namespace hb::shared::net;
using namespace hb::client::sprite_id;
DialogBox_Commander::DialogBox_Commander(CGame* game)
	: IDialogBox(DialogBoxId::CrusadeCommander, game)
{
	set_default_rect(20 , 20 , 310, 386);
	m_can_close_on_right_click = false;
}

void DialogBox_Commander::on_update()
{
	uint32_t time = GameClock::get_time_ms();
	if ((time - m_game->on_game()->m_commander_command_requested_time) > 1000 * 10)
	{
		m_game->request_map_status("middleland", 3);
		m_game->request_map_status("middleland", 1);
		m_game->on_game()->m_commander_command_requested_time = time;
	}
}

void DialogBox_Commander::on_draw()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	short sX, sY, size_x, size_y, MapSzX, MapSzY;
	double v1, v2, v3;
	int i, tX, tY;
	sX = m_x;
	sY = m_y;
	size_x = m_size_x;

	draw_new_dialog_box(InterfaceNdCrusade, sX, sY - 5, 0, false, config_manager::get().is_dialog_transparency_enabled());
	draw_new_dialog_box(InterfaceNdText, sX, sY, 15, false, config_manager::get().is_dialog_transparency_enabled());

	switch (m_mode) {
	case mode::main:
		m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 20, sY + 340, 3);
		m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 20 + 50, sY + 340, 1);
		m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 20 + 100, sY + 340, 2);
		m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 20 + 150, sY + 340, 30);
		m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 20 + 150 + 74, sY + 340, 4);
		put_aligned_string(sX, sX + size_x, sY + 37, DRAW_DIALOGBOX_COMMANDER1);

		if (mouse_in(btn_set_tp))
		{
			m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 20, sY + 340, 17);
			hb::shared::text::draw_text(GameFont::Default, mouse_x + 20, mouse_y + 35, DRAW_DIALOGBOX_COMMANDER2, hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
		}
		else if (mouse_in(btn_use_tp))
		{
			m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 20 + 50, sY + 340, 15);
			hb::shared::text::draw_text(GameFont::Default, mouse_x + 20, mouse_y + 35, DRAW_DIALOGBOX_COMMANDER3, hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
		}
		else if (mouse_in(btn_summon))
		{
			m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 20 + 100, sY + 340, 16);
			hb::shared::text::draw_text(GameFont::Default, mouse_x + 20, mouse_y + 35, DRAW_DIALOGBOX_COMMANDER4, hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
		}
		else if (mouse_in(btn_set_construct))
		{
			m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 20 + 150, sY + 340, 24);
			hb::shared::text::draw_text(GameFont::Default, mouse_x + 20, mouse_y + 35, DRAW_DIALOGBOX_COMMANDER5, hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
		}
		else if (mouse_in(btn_help))
		{
			m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 20 + 150 + 74, sY + 340, 18);
			hb::shared::text::draw_text(GameFont::Default, mouse_x + 20, mouse_y + 35, DRAW_DIALOGBOX_COMMANDER6, hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
		}
		draw_new_dialog_box(InterfaceNdCrusade, sX, sY, 21, false, config_manager::get().is_dialog_transparency_enabled());
		break;

	case mode::set_tp:
		m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 20 + 100 + 74, sY + 340, 20);
		m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 20 + 150 + 74, sY + 340, 4);
		put_aligned_string(sX, sX + size_x, sY + 40, DRAW_DIALOGBOX_COMMANDER7);

		if (mouse_in(btn_back))
		{
			m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 20 + 100 + 74, sY + 340, 19);
			hb::shared::text::draw_text(GameFont::Default, mouse_x + 20, mouse_y + 35, DRAW_DIALOGBOX_COMMANDER8, hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
		}
		else if (mouse_in(btn_help))
		{
			m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 20 + 150 + 74, sY + 340, 18);
			hb::shared::text::draw_text(GameFont::Default, mouse_x + 20, mouse_y + 35, DRAW_DIALOGBOX_COMMANDER9, hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
		}
		draw_new_dialog_box(InterfaceNdCrusade, sX, sY, 21, false, config_manager::get().is_dialog_transparency_enabled());

		if (mouse_in(area_map))
		{
			draw_new_dialog_box(InterfaceNdCrusade, mouse_x, mouse_y, 42, false, true);
		}
		break;

	case mode::use_tp:
		m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 20 + 50, sY + 340, 1);
		m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 20 + 100 + 74, sY + 340, 20);
		m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 20 + 150 + 74, sY + 340, 4);
		put_aligned_string(sX, sX + size_x, sY + 40, DRAW_DIALOGBOX_COMMANDER10);

		if (mouse_in(btn_use_tp))
		{
			m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 20 + 50, sY + 340, 15);
			hb::shared::text::draw_text(GameFont::Default, mouse_x + 20, mouse_y + 35, DRAW_DIALOGBOX_COMMANDER11, hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
		}
		else if (mouse_in(btn_back))
		{
			m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 20 + 100 + 74, sY + 340, 19);
			hb::shared::text::draw_text(GameFont::Default, mouse_x + 20, mouse_y + 35, DRAW_DIALOGBOX_COMMANDER12, hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
		}
		else if (mouse_in(btn_help))
		{
			m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 20 + 150 + 74, sY + 340, 18);
			hb::shared::text::draw_text(GameFont::Default, mouse_x + 20, mouse_y + 35, DRAW_DIALOGBOX_COMMANDER13, hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
		}
		draw_new_dialog_box(InterfaceNdCrusade, sX, sY, 21, false, config_manager::get().is_dialog_transparency_enabled());
		break;

	case mode::summon: {
		if ((player().m_citizen == true) && (player().m_aresden == true))
		{
			m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 20, sY + 220, 6);
			m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 20 + 50, sY + 220, 5);
			m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 20 + 100, sY + 220, 7);
			m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 20 + 150, sY + 220, 35);
		}
		else if ((player().m_citizen == true) && (player().m_aresden == false))
		{
			m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 20, sY + 220, 9);
			m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 20 + 50, sY + 220, 8);
			m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 20 + 100, sY + 220, 7);
			m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 20 + 150, sY + 220, 35);
		}
		m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 20 + 100 + 74, sY + 340, 20);
		m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 20 + 150 + 74, sY + 340, 4);

		put_aligned_string(sX, sX + size_x, sY + 40, DRAW_DIALOGBOX_COMMANDER14);

		auto pointsBuf = std::format("{} {}", DRAW_DIALOGBOX_COMMANDER15, player().m_construction_point);
		put_aligned_string(sX, sX + 323, sY + 190, pointsBuf.c_str());

		if ((player().m_citizen == true) && (player().m_aresden == true))
		{
			if (mouse_in(btn_unit_1))
			{
				if (player().m_construction_point >= 3000)
				{
					m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 20, sY + 220, 11);
				}
				hb::shared::text::draw_text(GameFont::Default, mouse_x + 20, mouse_y + 35, DRAW_DIALOGBOX_COMMANDER16, hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
				hb::shared::text::draw_text(GameFont::Default, mouse_x + 20, mouse_y + 50, DRAW_DIALOGBOX_COMMANDER17, hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
			}
			else if (mouse_in(btn_unit_2))
			{
				if (player().m_construction_point >= 2000)
				{
					m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 20 + 50, sY + 220, 10);
				}
				hb::shared::text::draw_text(GameFont::Default, mouse_x + 20, mouse_y + 35, DRAW_DIALOGBOX_COMMANDER18, hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
				hb::shared::text::draw_text(GameFont::Default, mouse_x + 20, mouse_y + 50, DRAW_DIALOGBOX_COMMANDER19, hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
			}
			else if (mouse_in(btn_unit_3))
			{
				if (player().m_construction_point >= 1000)
				{
					m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 20 + 100, sY + 220, 12);
				}
				hb::shared::text::draw_text(GameFont::Default, mouse_x + 20, mouse_y + 35, DRAW_DIALOGBOX_COMMANDER20, hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
				hb::shared::text::draw_text(GameFont::Default, mouse_x + 20, mouse_y + 50, DRAW_DIALOGBOX_COMMANDER21, hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
			}
			else if (mouse_in(btn_unit_4))
			{
				if (player().m_construction_point >= 5000)
				{
					m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 20 + 150, sY + 220, 29);
				}
				hb::shared::text::draw_text(GameFont::Default, mouse_x + 20, mouse_y + 35, DRAW_DIALOGBOX_COMMANDER22, hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
				hb::shared::text::draw_text(GameFont::Default, mouse_x + 20, mouse_y + 50, DRAW_DIALOGBOX_COMMANDER23, hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
			}
			else if (mouse_in(link_faction_1))
			{
				hb::shared::text::draw_text(GameFont::Default, mouse_x + 20, mouse_y + 35, DRAW_DIALOGBOX_COMMANDER24, hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
			}
			else if (mouse_in(link_faction_2))
			{
				hb::shared::text::draw_text(GameFont::Default, mouse_x + 20, mouse_y + 35, DRAW_DIALOGBOX_COMMANDER25, hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
			}
			else if (mouse_in(btn_back))
			{
				m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 20 + 100 + 74, sY + 340, 19);
				hb::shared::text::draw_text(GameFont::Default, mouse_x + 20, mouse_y + 35, DRAW_DIALOGBOX_COMMANDER26, hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
			}
			else if (mouse_in(btn_help))
			{
				m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 20 + 150 + 74, sY + 340, 18);
				hb::shared::text::draw_text(GameFont::Default, mouse_x + 20, mouse_y + 35, DRAW_DIALOGBOX_COMMANDER27, hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
			}
		}
		else if ((player().m_citizen == true) && (player().m_aresden == false))
		{
			if (mouse_in(btn_unit_1))
			{
				if (player().m_construction_point >= 3000)
				{
					m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 20, sY + 220, 14);
				}
				hb::shared::text::draw_text(GameFont::Default, mouse_x + 20, mouse_y + 35, DRAW_DIALOGBOX_COMMANDER28, hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
				hb::shared::text::draw_text(GameFont::Default, mouse_x + 20, mouse_y + 50, DRAW_DIALOGBOX_COMMANDER29, hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
			}
			else if (mouse_in(btn_unit_2))
			{
				if (player().m_construction_point >= 2000)
				{
					m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 20 + 50, sY + 220, 13);
				}
				hb::shared::text::draw_text(GameFont::Default, mouse_x + 20, mouse_y + 35, DRAW_DIALOGBOX_COMMANDER30, hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
				hb::shared::text::draw_text(GameFont::Default, mouse_x + 20, mouse_y + 50, DRAW_DIALOGBOX_COMMANDER31, hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
			}
			else if (mouse_in(btn_unit_3))
			{
				if (player().m_construction_point >= 1000)
				{
					m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 20 + 100, sY + 220, 12);
				}
				hb::shared::text::draw_text(GameFont::Default, mouse_x + 20, mouse_y + 35, DRAW_DIALOGBOX_COMMANDER32, hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
				hb::shared::text::draw_text(GameFont::Default, mouse_x + 20, mouse_y + 50, DRAW_DIALOGBOX_COMMANDER33, hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
			}
			else if (mouse_in(btn_unit_4))
			{
				if (player().m_construction_point >= 5000)
				{
					m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 20 + 150, sY + 220, 29);
				}
				hb::shared::text::draw_text(GameFont::Default, mouse_x + 20, mouse_y + 35, DRAW_DIALOGBOX_COMMANDER34, hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
				hb::shared::text::draw_text(GameFont::Default, mouse_x + 20, mouse_y + 50, DRAW_DIALOGBOX_COMMANDER35, hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
			}
			else if (mouse_in(link_faction_1))
			{
				hb::shared::text::draw_text(GameFont::Default, mouse_x + 20, mouse_y + 35, DRAW_DIALOGBOX_COMMANDER36, hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
			}
			else if (mouse_in(link_faction_2))
			{
				hb::shared::text::draw_text(GameFont::Default, mouse_x + 20, mouse_y + 35, DRAW_DIALOGBOX_COMMANDER37, hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
			}
			else if (mouse_in(btn_back))
			{
				m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 20 + 100 + 74, sY + 340, 19);
				hb::shared::text::draw_text(GameFont::Default, mouse_x + 20, mouse_y + 35, DRAW_DIALOGBOX_COMMANDER38, hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
			}
			else if (mouse_in(btn_help))
			{
				m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 20 + 150 + 74, sY + 340, 18);
				hb::shared::text::draw_text(GameFont::Default, mouse_x + 20, mouse_y + 35, DRAW_DIALOGBOX_COMMANDER39, hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
			}
		}
		put_aligned_string(sX, sX + 323, sY + 80, DRAW_DIALOGBOX_COMMANDER40);
		put_aligned_string(sX, sX + 323, sY + 95, DRAW_DIALOGBOX_COMMANDER41);
		put_aligned_string(sX, sX + 323, sY + 110, DRAW_DIALOGBOX_COMMANDER42);

		switch (m_selected_faction) {
		case 0:
			put_aligned_string(sX, sX + 323, sY + 140, DRAW_DIALOGBOX_COMMANDER43, GameColors::UIWhite);
			put_aligned_string(sX, sX + 323, sY + 160, DRAW_DIALOGBOX_COMMANDER44, GameColors::UIMagicBlue);
			break;
		case 1:
			put_aligned_string(sX, sX + 323, sY + 140, DRAW_DIALOGBOX_COMMANDER43, GameColors::UIMagicBlue);
			put_aligned_string(sX, sX + 323, sY + 160, DRAW_DIALOGBOX_COMMANDER44, GameColors::UIWhite);
			break;
		}
		break;
	}

	case mode::set_construct:
		m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 20 + 100 + 74, sY + 340, 20);
		m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 20 + 150 + 74, sY + 340, 4);
		put_aligned_string(sX, sX + size_x, sY + 40, DRAW_DIALOGBOX_COMMANDER47);

		if (mouse_in(btn_back))
		{
			m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 20 + 100 + 74, sY + 340, 19);
			hb::shared::text::draw_text(GameFont::Default, mouse_x + 20, mouse_y + 35, DRAW_DIALOGBOX_COMMANDER48, hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
		}
		else if (mouse_in(btn_help))
		{
			m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 20 + 150 + 74, sY + 340, 18);
			hb::shared::text::draw_text(GameFont::Default, mouse_x + 20, mouse_y + 35, DRAW_DIALOGBOX_COMMANDER49, hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
		}
		draw_new_dialog_box(InterfaceNdCrusade, sX, sY, 21);
		if (mouse_in(area_map))
		{
			draw_new_dialog_box(InterfaceNdCrusade, mouse_x, mouse_y, 41, false, true);
		}
		break;
	}

	// draw map structures and positions
	switch (m_mode) {
	case mode::main:
	case mode::set_tp:
	case mode::use_tp:
	case mode::set_construct:
		size_x = 0;
		size_y = 0;
		MapSzX = 0;
		MapSzY = 0;
		if (m_game->m_status_map_name == "aresden")
		{
			size_x = 250;
			size_y = 250;
		}
		else if (m_game->m_status_map_name == "elvine")
		{
			size_x = 250;
			size_y = 250;
		}
		else if (m_game->m_status_map_name == "middleland")
		{
			size_x = 279;
			size_y = 280;
			MapSzX = 524;
			MapSzY = 524;
		}
		if (size_x != 0)
		{
			for (i = 0; i < hb::shared::limits::MaxCrusadeStructures; i++)
				if (m_game->on_game()->m_crusade_structure_info[i].type != 0)
				{
					v1 = static_cast<double>(MapSzX);
					v2 = static_cast<double>(m_game->on_game()->m_crusade_structure_info[i].x);
					v3 = (v2 * static_cast<double>(size_x)) / v1;
					tX = static_cast<int>(v3);
					v1 = static_cast<double>(MapSzY);
					v2 = static_cast<double>(m_game->on_game()->m_crusade_structure_info[i].y);
					v3 = (v2 * static_cast<double>(size_y)) / v1;
					tY = static_cast<int>(v3);
					switch (m_game->on_game()->m_crusade_structure_info[i].type) {
					case 38:
						if (m_game->on_game()->m_crusade_structure_info[i].side == 1)
							draw_new_dialog_box(InterfaceNdCrusade, sX + tX + 15, sY + tY + 60, 39, false, true);
						else draw_new_dialog_box(InterfaceNdCrusade, sX + tX + 15, sY + tY + 60, 37, false, true);
						break;
					case 36:
					case 37:
					case 39:
						if (m_game->on_game()->m_crusade_structure_info[i].side == 1)
							draw_new_dialog_box(InterfaceNdCrusade, sX + tX + 15, sY + tY + 60, 38, false, true);
						else draw_new_dialog_box(InterfaceNdCrusade, sX + tX + 15, sY + tY + 60, 36, false, true);
						break;
					case 42:
						draw_new_dialog_box(InterfaceNdCrusade, sX + tX + 15, sY + tY + 60, 40);
						break;
					}
				}
			if (teleport_manager::get().get_loc_x() != -1)
			{
				v1 = static_cast<double>(MapSzX);
				v2 = static_cast<double>(teleport_manager::get().get_loc_x());
				v3 = (v2 * static_cast<double>(size_x)) / v1;
				tX = static_cast<int>(v3);
				v1 = static_cast<double>(MapSzY);
				v2 = static_cast<double>(teleport_manager::get().get_loc_y());
				v3 = (v2 * static_cast<double>(size_y)) / v1;
				tY = static_cast<int>(v3);
				draw_new_dialog_box(InterfaceNdCrusade, sX + tX + 15, sY + tY + 60, 42, false, true);
			}
			if ((m_mode != mode::use_tp) && (player().m_construct_loc_x != -1))
			{
				v1 = static_cast<double>(MapSzX);
				v2 = static_cast<double>(player().m_construct_loc_x);
				v3 = (v2 * static_cast<double>(size_x)) / v1;
				tX = static_cast<int>(v3);
				v1 = static_cast<double>(MapSzY);
				v2 = static_cast<double>(player().m_construct_loc_y);
				v3 = (v2 * static_cast<double>(size_y)) / v1;
				tY = static_cast<int>(v3);
				draw_new_dialog_box(InterfaceNdCrusade, sX + tX + 15, sY + tY + 60, 41, false, true);
			}
			if (m_game->m_map_name == "middleland")
			{
				v1 = static_cast<double>(MapSzX);
				v2 = static_cast<double>(player().m_player_x);
				v3 = (v2 * static_cast<double>(size_x)) / v1;
				tX = static_cast<int>(v3);
				v1 = static_cast<double>(MapSzY);
				v2 = static_cast<double>(player().m_player_y);
				v3 = (v2 * static_cast<double>(size_y)) / v1;
				tY = static_cast<int>(v3);
				draw_new_dialog_box(InterfaceNdCrusade, sX + tX + 15, sY + tY + 60, 43);
			}
		}
		if (m_mode != mode::summon && size_x > 0 && size_y > 0)
		{
			if (mouse_in(area_map))
			{
				v1 = static_cast<double>(mouse_x - (sX + 15));
				v2 = static_cast<double>(MapSzX);
				v3 = (v2 * v1) / size_x;
				tX = static_cast<int>(v3);
				v1 = static_cast<double>(mouse_y - (sY + 60));
				v2 = static_cast<double>(MapSzY);
				v3 = (v2 * v1) / size_y;
				tY = static_cast<int>(v3);
				if (tX < 30) tX = 30;
				if (tY < 30) tY = 30;
				if (tX > MapSzX - 30) tX = MapSzX - 30;
				if (tY > MapSzY - 30) tY = MapSzY - 30;
				auto coordBuf = std::format("{},{}", tX, tY);
				hb::shared::text::draw_text(GameFont::SprFont3_2, mouse_x + 10, mouse_y - 10, coordBuf.c_str(), hb::shared::text::TextStyle::with_two_point_shadow(GameColors::Yellow4x));
			}
		}
		break;
	}
}

bool DialogBox_Commander::on_click()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	short sX, sY, tX, tY;
	double d1, d2, d3;

	if (m_game->on_game()->m_is_crusade_mode == false) return false;

	sX = m_x;
	sY = m_y;

	switch (m_mode) {
	case mode::main:
		if (mouse_in(btn_set_tp))
		{
			m_mode = mode::set_tp;
			play_sound_effect('E', 14, 5);
		}
		else if (mouse_in(btn_use_tp))
		{
			if (teleport_manager::get().get_loc_x() == -1)
			{
				if (m_game->m_game_msg_list[15]) m_game->set_top_msg(m_game->m_game_msg_list[15]->m_pMsg, 5);
			}
			else if (m_game->m_map_name == teleport_manager::get().get_map_name())
			{
				if (m_game->m_game_msg_list[16]) m_game->set_top_msg(m_game->m_game_msg_list[16]->m_pMsg, 5);
			}
			else
			{
				m_mode = mode::use_tp;
				play_sound_effect('E', 14, 5);
			}
		}
		else if (mouse_in(btn_summon))
		{
			m_mode = mode::summon;
			m_selected_faction = 0;
			play_sound_effect('E', 14, 5);
		}
		else if (mouse_in(btn_set_construct))
		{
			m_mode = mode::set_construct;
			play_sound_effect('E', 14, 5);
		}
		else if (mouse_in(btn_help))
		{
			disable_dialog_box(DialogBoxId::Text);
			enable_dialog_box(DialogBoxId::Text, 808, 0, 0);
			play_sound_effect('E', 14, 5);
		}
		break;

	case mode::set_tp:
		if (mouse_in(area_map))
		{
			d1 = static_cast<double>(mouse_x - (sX + 15));
			d2 = static_cast<double>(524.0f);
			d3 = (d2 * d1) / 279.0f;
			tX = static_cast<int>(d3);
			d1 = static_cast<double>(mouse_y - (sY + 60));
			d2 = static_cast<double>(524.0f);
			d3 = (d2 * d1) / (280.0f);
			tY = static_cast<int>(d3);
			if (tX < 30) tX = 30;
			if (tY < 30) tY = 30;
			if (tX > 494) tX = 494;
			if (tY > 494) tY = 494;
			{
				auto pkt = hb::net::make_common_command_str(CommonType::SetGuildTeleportLoc, player().m_player_x, player().m_player_y);
				pkt.v1 = tX;
				pkt.v2 = tY;
				std::snprintf(pkt.text, sizeof(pkt.text), "%s", "middleland");
				send_game_packet(pkt);
			}
			m_mode = mode::main;
			play_sound_effect('E', 14, 5);
			m_game->request_map_status("middleland", 1);
		}
		if (mouse_in(btn_back))
		{
			m_mode = mode::main;
			play_sound_effect('E', 14, 5);
		}
		if (mouse_in(btn_help))
		{
			disable_dialog_box(DialogBoxId::Text);
			enable_dialog_box(DialogBoxId::Text, 809, 0, 0);
			play_sound_effect('E', 14, 5);
		}
		break;

	case mode::use_tp:
		if (mouse_in(btn_use_tp))
		{
			send_game_packet(hb::net::make_common_command(CommonType::GuildTeleport, player().m_player_x, player().m_player_y));
			disable_dialog_box(DialogBoxId::CrusadeCommander);
			play_sound_effect('E', 14, 5);
		}
		if (mouse_in(btn_back))
		{
			m_mode = mode::main;
			play_sound_effect('E', 14, 5);
		}
		if (mouse_in(btn_help))
		{
			disable_dialog_box(DialogBoxId::Text);
			enable_dialog_box(DialogBoxId::Text, 810, 0, 0);
			play_sound_effect('E', 14, 5);
		}
		break;

	case mode::summon:
		if (player().m_aresden == true)
		{
			if (mouse_in(btn_unit_1))
			{
				if (player().m_construction_point >= 3000)
				{
					{
						auto pkt = hb::net::make_common_command(CommonType::SummonWarUnit, player().m_player_x, player().m_player_y);
						pkt.v1 = 47;
						pkt.v2 = 1;
						pkt.v3 = m_selected_faction;
						send_game_packet(pkt);
					}
					play_sound_effect('E', 14, 5);
					disable_dialog_box(DialogBoxId::CrusadeCommander);
				}
			}
			if (mouse_in(btn_unit_2))
			{
				if (player().m_construction_point >= 2000)
				{
					{
						auto pkt = hb::net::make_common_command(CommonType::SummonWarUnit, player().m_player_x, player().m_player_y);
						pkt.v1 = 46;
						pkt.v2 = 1;
						pkt.v3 = m_selected_faction;
						send_game_packet(pkt);
					}
					play_sound_effect('E', 14, 5);
					disable_dialog_box(DialogBoxId::CrusadeCommander);
				}
			}
			if (mouse_in(btn_unit_3))
			{
				if (player().m_construction_point >= 1000)
				{
					{
						auto pkt = hb::net::make_common_command(CommonType::SummonWarUnit, player().m_player_x, player().m_player_y);
						pkt.v1 = 43;
						pkt.v2 = 1;
						pkt.v3 = m_selected_faction;
						send_game_packet(pkt);
					}
					play_sound_effect('E', 14, 5);
					disable_dialog_box(DialogBoxId::CrusadeCommander);
				}
			}
			if (mouse_in(btn_unit_4))
			{
				if (player().m_construction_point >= 1500)
				{
					{
						auto pkt = hb::net::make_common_command(CommonType::SummonWarUnit, player().m_player_x, player().m_player_y);
						pkt.v1 = 51;
						pkt.v2 = 1;
						pkt.v3 = m_selected_faction;
						send_game_packet(pkt);
					}
					play_sound_effect('E', 14, 5);
					disable_dialog_box(DialogBoxId::CrusadeCommander);
				}
			}
		}
		else if (player().m_aresden == false)
		{
			if (mouse_in(btn_unit_1))
			{
				if (player().m_construction_point >= 3000)
				{
					{
						auto pkt = hb::net::make_common_command(CommonType::SummonWarUnit, player().m_player_x, player().m_player_y);
						pkt.v1 = 45;
						pkt.v2 = 1;
						pkt.v3 = m_selected_faction;
						send_game_packet(pkt);
					}
					play_sound_effect('E', 14, 5);
					disable_dialog_box(DialogBoxId::CrusadeCommander);
				}
			}
			if (mouse_in(btn_unit_2))
			{
				if (player().m_construction_point >= 2000)
				{
					{
						auto pkt = hb::net::make_common_command(CommonType::SummonWarUnit, player().m_player_x, player().m_player_y);
						pkt.v1 = 44;
						pkt.v2 = 1;
						pkt.v3 = m_selected_faction;
						send_game_packet(pkt);
					}
					play_sound_effect('E', 14, 5);
					disable_dialog_box(DialogBoxId::CrusadeCommander);
				}
			}
			if (mouse_in(btn_unit_3))
			{
				if (player().m_construction_point >= 1000)
				{
					{
						auto pkt = hb::net::make_common_command(CommonType::SummonWarUnit, player().m_player_x, player().m_player_y);
						pkt.v1 = 43;
						pkt.v2 = 1;
						pkt.v3 = m_selected_faction;
						send_game_packet(pkt);
					}
					play_sound_effect('E', 14, 5);
					disable_dialog_box(DialogBoxId::CrusadeCommander);
				}
			}
			if (mouse_in(btn_unit_4))
			{
				if (player().m_construction_point >= 1500)
				{
					{
						auto pkt = hb::net::make_common_command(CommonType::SummonWarUnit, player().m_player_x, player().m_player_y);
						pkt.v1 = 51;
						pkt.v2 = 1;
						pkt.v3 = m_selected_faction;
						send_game_packet(pkt);
					}
					play_sound_effect('E', 14, 5);
					disable_dialog_box(DialogBoxId::CrusadeCommander);
				}
			}
		}
		if (mouse_in(link_faction_1))
		{
			m_selected_faction = 0;
			play_sound_effect('E', 14, 5);
		}
		if (mouse_in(link_faction_2))
		{
			m_selected_faction = 1;
			play_sound_effect('E', 14, 5);
		}
		if (mouse_in(btn_back))
		{
			m_mode = mode::main;
			play_sound_effect('E', 14, 5);
		}
		if (mouse_in(btn_help))
		{
			disable_dialog_box(DialogBoxId::Text);
			enable_dialog_box(DialogBoxId::Text, 811, 0, 0);
			play_sound_effect('E', 14, 5);
		}
		break;

	case mode::set_construct:
		if (mouse_in(area_map))
		{
			d1 = static_cast<double>(mouse_x - (sX + 15));
			d2 = static_cast<double>(524.0);
			d3 = (d2 * d1) / 279.0f;
			tX = static_cast<int>(d3);
			d1 = static_cast<double>(mouse_y - (sY + 60));
			d2 = static_cast<double>(524.0);
			d3 = (d2 * d1) / (280.0);
			tY = static_cast<int>(d3);
			if (tX < 30) tX = 30;
			if (tY < 30) tY = 30;
			if (tX > 494) tX = 494;
			if (tY > 494) tY = 494;
			{
				auto pkt = hb::net::make_common_command_str(CommonType::SetGuildConstructLoc, player().m_player_x, player().m_player_y);
				pkt.v1 = tX;
				pkt.v2 = tY;
				std::snprintf(pkt.text, sizeof(pkt.text), "%s", "middleland");
				send_game_packet(pkt);
			}
			m_mode = mode::main;
			play_sound_effect('E', 14, 5);
			m_game->request_map_status("middleland", 1);
		}
		if (mouse_in(btn_back))
		{
			m_mode = mode::main;
			play_sound_effect('E', 14, 5);
		}
		if (mouse_in(btn_help))
		{
			disable_dialog_box(DialogBoxId::Text);
			enable_dialog_box(DialogBoxId::Text, 812, 0, 0);
			play_sound_effect('E', 14, 5);
		}
		break;
	}

	return false;
}
