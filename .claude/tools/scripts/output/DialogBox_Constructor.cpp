#include "DialogBox_Constructor.h"
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
#include <string>
#include "IInput.h"
#include "Screen_OnGame.h"

using namespace hb::shared::net;
using namespace hb::client::sprite_id;
DialogBox_Constructor::DialogBox_Constructor(CGame* game)
	: IDialogBox(DialogBoxId::CrusadeConstructor, game)
{
	set_default_rect(20 , 20 , 310, 386);
}

void DialogBox_Constructor::on_update()
{
	uint32_t time = GameClock::get_time_ms();
	if ((time - m_game->on_game()->m_commander_command_requested_time) > 1000 * 10)
	{
		m_game->request_map_status("middleland", 1);
		m_game->on_game()->m_commander_command_requested_time = time;
	}
}

void DialogBox_Constructor::on_draw()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	short sX, sY, size_x, size_y, MapSzX, MapSzY;
	double v1, v2, v3;
	int tX, tY;
	char map_name[12];
	sX = m_x;
	sY = m_y;
	size_x = m_size_x;

	draw_new_dialog_box(InterfaceNdCrusade, sX, sY - 5, 0, false, config_manager::get().is_dialog_transparency_enabled());
	draw_new_dialog_box(InterfaceNdText, sX, sY, 16, false, config_manager::get().is_dialog_transparency_enabled());

	switch (m_mode) {
	case mode::main:
		if (player().m_construct_loc_x != -1)
		{
			std::string locationBuf;
			std::memset(map_name, 0, sizeof(map_name));
			m_game->get_official_map_name(m_game->m_construct_map_name.c_str(), map_name);
			locationBuf = std::format(DRAW_DIALOGBOX_CONSTRUCTOR1, map_name, player().m_construct_loc_x, player().m_construct_loc_y);
			put_aligned_string(sX, sX + size_x, sY + 40, locationBuf.c_str());
		}
		else put_aligned_string(sX, sX + size_x, sY + 40, DRAW_DIALOGBOX_CONSTRUCTOR2);

		draw_new_dialog_box(InterfaceNdCrusade, sX, sY, 21, false, config_manager::get().is_dialog_transparency_enabled());

		if (mouse_in(btn_construct))
			m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 20, sY + 340, 24);
		else m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 20, sY + 340, 30);

		if (mouse_in(btn_set_tp))
			m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 70, sY + 340, 15);
		else m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 70, sY + 340, 1);

		if (mouse_in(btn_help_main))
			m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 244, sY + 340, 18);
		else m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 244, sY + 340, 4);

		// Tooltips
		if (mouse_in(btn_construct))
			hb::shared::text::draw_text(GameFont::Default, mouse_x + 20, mouse_y + 35, DRAW_DIALOGBOX_CONSTRUCTOR3, hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
		else if (mouse_in(btn_set_tp))
			hb::shared::text::draw_text(GameFont::Default, mouse_x + 20, mouse_y + 35, DRAW_DIALOGBOX_CONSTRUCTOR4, hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
		else if (mouse_in(btn_help_main))
			hb::shared::text::draw_text(GameFont::Default, mouse_x + 20, mouse_y + 35, DRAW_DIALOGBOX_CONSTRUCTOR5, hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
		break;

	case mode::select_building:
		put_aligned_string(sX, sX + size_x, sY + 40, DRAW_DIALOGBOX_CONSTRUCTOR6);
		put_aligned_string(sX, sX + 323, sY + 80, DRAW_DIALOGBOX_CONSTRUCTOR7);
		put_aligned_string(sX, sX + 323, sY + 95, DRAW_DIALOGBOX_CONSTRUCTOR8);
		put_aligned_string(sX, sX + 323, sY + 110, DRAW_DIALOGBOX_CONSTRUCTOR9);
		put_aligned_string(sX, sX + 323, sY + 125, DRAW_DIALOGBOX_CONSTRUCTOR10);
		put_aligned_string(sX, sX + 323, sY + 140, DRAW_DIALOGBOX_CONSTRUCTOR11);
		put_aligned_string(sX, sX + 323, sY + 155, DRAW_DIALOGBOX_CONSTRUCTOR12);

		if (mouse_in(btn_building_1))
			m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 20, sY + 220, 27);
		else m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 20, sY + 220, 33);

		if (mouse_in(btn_building_2))
			m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 70, sY + 220, 28);
		else m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 70, sY + 220, 34);

		if (mouse_in(btn_building_3))
			m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 120, sY + 220, 26);
		else m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 120, sY + 220, 32);

		if (mouse_in(btn_building_4))
			m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 170, sY + 220, 25);
		else m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 170, sY + 220, 31);

		if (mouse_in(btn_back_build))
			m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 194, sY + 322, 19);
		else m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 194, sY + 322, 20);

		if (mouse_in(btn_help_build))
			m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 244, sY + 322, 18);
		else m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 244, sY + 322, 4);

		// Tooltips
		if (mouse_in(btn_building_1))
			hb::shared::text::draw_text(GameFont::Default, mouse_x + 20, mouse_y + 35, DRAW_DIALOGBOX_CONSTRUCTOR13, hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
		else if (mouse_in(btn_building_2))
			hb::shared::text::draw_text(GameFont::Default, mouse_x + 20, mouse_y + 35, DRAW_DIALOGBOX_CONSTRUCTOR14, hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
		else if (mouse_in(btn_building_3))
			hb::shared::text::draw_text(GameFont::Default, mouse_x + 20, mouse_y + 35, DRAW_DIALOGBOX_CONSTRUCTOR15, hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
		else if (mouse_in(btn_building_4))
			hb::shared::text::draw_text(GameFont::Default, mouse_x + 20, mouse_y + 35, DRAW_DIALOGBOX_CONSTRUCTOR16, hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
		else if (mouse_in(btn_back_build))
			hb::shared::text::draw_text(GameFont::Default, mouse_x + 20, mouse_y + 35, DRAW_DIALOGBOX_CONSTRUCTOR17, hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
		else if (mouse_in(btn_help_build))
			hb::shared::text::draw_text(GameFont::Default, mouse_x + 20, mouse_y + 35, DRAW_DIALOGBOX_CONSTRUCTOR18, hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
		break;

	case mode::teleport:
		put_aligned_string(sX, sX + size_x, sY + 40, DRAW_DIALOGBOX_CONSTRUCTOR19);
		draw_new_dialog_box(InterfaceNdCrusade, sX, sY, 21, false, config_manager::get().is_dialog_transparency_enabled());

		if (mouse_in(btn_set_tp))
			m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 70, sY + 340, 15);
		else m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 70, sY + 340, 1);

		if (mouse_in(btn_back_tp))
			m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 194, sY + 340, 19);
		else m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 194, sY + 340, 20);

		if (mouse_in(btn_help_main))
			m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 244, sY + 340, 18);
		else m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 244, sY + 340, 4);

		// Tooltips
		if (mouse_in(btn_set_tp))
			hb::shared::text::draw_text(GameFont::Default, mouse_x + 20, mouse_y + 35, DRAW_DIALOGBOX_CONSTRUCTOR20, hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
		else if (mouse_in(btn_back_tp))
			hb::shared::text::draw_text(GameFont::Default, mouse_x + 20, mouse_y + 35, DRAW_DIALOGBOX_CONSTRUCTOR21, hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
		else if (mouse_in(btn_help_main))
			hb::shared::text::draw_text(GameFont::Default, mouse_x + 20, mouse_y + 35, DRAW_DIALOGBOX_CONSTRUCTOR22, hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
		break;
	}

	// draw map overlay
	switch (m_mode) {
	case mode::main:
	case mode::teleport:
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
			for (int i = 0; i < hb::shared::limits::MaxCrusadeStructures; i++)
				if (m_game->on_game()->m_crusade_structure_info[i].type == 42)
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
			if ((m_mode != mode::teleport) && (player().m_construct_loc_x != -1))
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
		if (size_x > 0 && size_y > 0 && mouse_in(area_map))
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
			std::string coordBuf;
			coordBuf = std::format("{},{}", tX, tY);
			hb::shared::text::draw_text(GameFont::SprFont3_2, mouse_x + 10, mouse_y - 10, coordBuf.c_str(), hb::shared::text::TextStyle::with_two_point_shadow(GameColors::Yellow4x));
		}
		break;
	default:
		break;
	}
}

bool DialogBox_Constructor::on_click()
{
	if (m_game->on_game()->m_is_crusade_mode == false) return false;
	short sX = m_x;
	short sY = m_y;

	switch (m_mode) {
	case mode::main:
		if (mouse_in(btn_construct))
		{
			if (player().m_construct_loc_x == -1)
			{
				m_game->set_top_msg(m_game->m_game_msg_list[14]->m_pMsg, 5);
			}
			else
			{
				m_mode = mode::select_building;
				play_sound_effect('E', 14, 5);
			}
		}
		else if (mouse_in(btn_set_tp))
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
				m_mode = mode::teleport;
				play_sound_effect('E', 14, 5);
			}
		}
		else if (mouse_in(btn_help_main))
		{
			disable_dialog_box(DialogBoxId::Text);
			enable_dialog_box(DialogBoxId::Text, 805, 0, 0);
			play_sound_effect('E', 14, 5);
		}
		break;

	case mode::select_building:
		if (mouse_in(btn_building_1))
		{
			{
				auto pkt = hb::net::make_common_command(CommonType::SummonWarUnit, player().m_player_x, player().m_player_y);
				pkt.v1 = 38;
				pkt.v2 = 1;
				pkt.v3 = m_game->get_dialog_box_manager().get_dialog_as<DialogBox_Commander>(DialogBoxId::CrusadeCommander)->m_selected_faction;
				send_game_packet(pkt);
			}
			play_sound_effect('E', 14, 5);
			disable_dialog_box(DialogBoxId::CrusadeConstructor);
		}
		if (mouse_in(btn_building_2))
		{
			{
				auto pkt = hb::net::make_common_command(CommonType::SummonWarUnit, player().m_player_x, player().m_player_y);
				pkt.v1 = 39;
				pkt.v2 = 1;
				pkt.v3 = m_game->get_dialog_box_manager().get_dialog_as<DialogBox_Commander>(DialogBoxId::CrusadeCommander)->m_selected_faction;
				send_game_packet(pkt);
			}
			play_sound_effect('E', 14, 5);
			disable_dialog_box(DialogBoxId::CrusadeConstructor);
		}
		if (mouse_in(btn_building_3))
		{
			{
				auto pkt = hb::net::make_common_command(CommonType::SummonWarUnit, player().m_player_x, player().m_player_y);
				pkt.v1 = 36;
				pkt.v2 = 1;
				pkt.v3 = m_game->get_dialog_box_manager().get_dialog_as<DialogBox_Commander>(DialogBoxId::CrusadeCommander)->m_selected_faction;
				send_game_packet(pkt);
			}
			play_sound_effect('E', 14, 5);
			disable_dialog_box(DialogBoxId::CrusadeConstructor);
		}
		if (mouse_in(btn_building_4))
		{
			{
				auto pkt = hb::net::make_common_command(CommonType::SummonWarUnit, player().m_player_x, player().m_player_y);
				pkt.v1 = 37;
				pkt.v2 = 1;
				pkt.v3 = m_game->get_dialog_box_manager().get_dialog_as<DialogBox_Commander>(DialogBoxId::CrusadeCommander)->m_selected_faction;
				send_game_packet(pkt);
			}
			play_sound_effect('E', 14, 5);
			disable_dialog_box(DialogBoxId::CrusadeConstructor);
		}

		if (mouse_in(btn_back_build))
		{
			m_mode = mode::main;
			play_sound_effect('E', 14, 5);
		}
		if (mouse_in(btn_help_build))
		{
			disable_dialog_box(DialogBoxId::Text);
			enable_dialog_box(DialogBoxId::Text, 806, 0, 0);
			play_sound_effect('E', 14, 5);
		}
		break;

	case mode::teleport:
		if (mouse_in(btn_set_tp))
		{
			send_game_packet(hb::net::make_common_command(CommonType::GuildTeleport, player().m_player_x, player().m_player_y));
			disable_dialog_box(DialogBoxId::CrusadeConstructor);
			play_sound_effect('E', 14, 5);
		}
		if (mouse_in(btn_back_tp))
		{
			m_mode = mode::main;
			play_sound_effect('E', 14, 5);
		}
		if (mouse_in(btn_help_main))
		{
			disable_dialog_box(DialogBoxId::Text);
			enable_dialog_box(DialogBoxId::Text, 807, 0, 0);
			play_sound_effect('E', 14, 5);
		}
		break;
	}
	return false;
}
