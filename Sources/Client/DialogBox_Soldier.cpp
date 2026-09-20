#include "DialogBox_Soldier.h"
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
#include "AudioManager.h"

using namespace hb::shared::net;
using namespace hb::client::sprite_id;
DialogBox_Soldier::DialogBox_Soldier(CGame* game)
	: IDialogBox(DialogBoxId::CrusadeSoldier, game)
{
	set_default_rect(20 , 20 , 310, 386);
}

void DialogBox_Soldier::on_update()
{
	uint32_t time = GameClock::get_time_ms();
	if ((time - m_game->on_game()->m_commander_command_requested_time) > 1000 * 10)
	{
		m_game->request_map_status("middleland", 1);
		m_game->on_game()->m_commander_command_requested_time = time;
	}
}

void DialogBox_Soldier::on_draw()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	short sX, sY, size_x, size_y, MapSzX, MapSzY;
	char map_name[120];
	double v1, v2, v3;
	int tX, tY;
	sX = m_x;
	sY = m_y;
	size_x = m_size_x;

	draw_new_dialog_box(InterfaceNdCrusade, sX, sY - 5, 0, false, config_manager::get().is_dialog_transparency_enabled());
	draw_new_dialog_box(InterfaceNdCrusade, sX, sY, 21, false, config_manager::get().is_dialog_transparency_enabled());
	draw_new_dialog_box(InterfaceNdText, sX, sY, 17, false, config_manager::get().is_dialog_transparency_enabled());

	switch (m_mode) {
	case mode::overview:
		if (teleport_manager::get().get_loc_x() != -1)
		{
			std::string locationBuf;
			std::memset(map_name, 0, sizeof(map_name));
			m_game->get_official_map_name(teleport_manager::get().get_map_name(), map_name);
			locationBuf = std::format(DRAW_DIALOGBOX_SOLDIER1, map_name, teleport_manager::get().get_loc_x(), teleport_manager::get().get_loc_y());
			put_aligned_string(sX, sX + size_x, sY + 40, locationBuf.c_str());
		}
		else put_aligned_string(sX, sX + size_x, sY + 40, DRAW_DIALOGBOX_SOLDIER2);

		if (mouse_in(btn_left))
			m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 20, sY + 340, 15);
		else
			m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 20, sY + 340, 1);

		if (mouse_in(btn_right))
			m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 244, sY + 340, 18);
		else
			m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 244, sY + 340, 4);

		if (mouse_in(btn_left))
			hb::shared::text::draw_text(GameFont::Default, mouse_x + 20, mouse_y + 35, DRAW_DIALOGBOX_SOLDIER3, hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
		else if (mouse_in(btn_right))
			hb::shared::text::draw_text(GameFont::Default, mouse_x + 20, mouse_y + 35, DRAW_DIALOGBOX_SOLDIER4, hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
		break;

	case mode::teleport:
		put_aligned_string(sX, sX + size_x, sY + 40, DRAW_DIALOGBOX_SOLDIER5);

		if (mouse_in(btn_left))
			m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 20, sY + 340, 15);
		else
			m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 20, sY + 340, 1);

		if (mouse_in(btn_middle))
			m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 194, sY + 340, 19);
		else
			m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 194, sY + 340, 20);

		if (mouse_in(btn_right))
			m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 244, sY + 340, 18);
		else
			m_game->m_sprite[InterfaceNdCrusade]->draw(sX + 244, sY + 340, 4);

		if (mouse_in(btn_left))
			hb::shared::text::draw_text(GameFont::Default, mouse_x + 20, mouse_y + 35, DRAW_DIALOGBOX_SOLDIER6, hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
		else if (mouse_in(btn_middle))
			hb::shared::text::draw_text(GameFont::Default, mouse_x + 20, mouse_y + 35, DRAW_DIALOGBOX_SOLDIER7, hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
		else if (mouse_in(btn_right))
			hb::shared::text::draw_text(GameFont::Default, mouse_x + 20, mouse_y + 35, DRAW_DIALOGBOX_SOLDIER8, hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
		break;
	}

	// draw map overlay
	switch (m_mode) {
	case mode::overview:
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
	}
}

bool DialogBox_Soldier::on_click()
{
	if (m_game->on_game()->m_is_crusade_mode == false) return false;

	switch (m_mode) {
	case mode::overview:
		if (mouse_in(btn_left))
		{
			if (teleport_manager::get().get_loc_x() == -1)
			{
				m_game->set_top_msg(m_game->m_game_msg_list[15]->m_pMsg, 5);
			}
			else if (m_game->m_map_name == teleport_manager::get().get_map_name())
			{
				m_game->set_top_msg(m_game->m_game_msg_list[16]->m_pMsg, 5);
			}
			else
			{
				m_mode = mode::teleport;
				audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
			}
		}
		if (mouse_in(btn_right))
		{
			disable_dialog_box(DialogBoxId::Text);
			enable_dialog_box(DialogBoxId::Text, 803, 0, 0);
			audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
		}
		break;

	case mode::teleport:
		if (mouse_in(btn_tp_wide))
		{
			// Guild teleport removed with guild system
			disable_dialog_box(DialogBoxId::CrusadeSoldier);
			audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
		}
		if (mouse_in(btn_middle))
		{
			m_mode = mode::overview;
			audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
		}
		if (mouse_in(btn_right))
		{
			disable_dialog_box(DialogBoxId::Text);
			enable_dialog_box(DialogBoxId::Text, 804, 0, 0);
			audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
		}
		break;
	}
	return false;
}
