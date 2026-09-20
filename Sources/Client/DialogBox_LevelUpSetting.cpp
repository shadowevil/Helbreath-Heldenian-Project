#include "DialogBox_LevelUpSetting.h"
#include "Game.h"
#include "lan_eng.h"
#include <algorithm>
#include <format>
#include <string>
#include "IInput.h"
#include "Packet/SharedPackets.h"
#include "Screen_OnGame.h"
#include "AudioManager.h"

using namespace hb::shared::net;
using namespace hb::client::sprite_id;
DialogBox_LevelUpSetting::DialogBox_LevelUpSetting(CGame* game)
	: IDialogBox(DialogBoxId::LevelUpSetting, game)
{
	set_default_rect(0 , 0 , 258, 339);
	m_can_close_on_right_click = false;
}

void DialogBox_LevelUpSetting::draw_stat_row(short sX, short sY, int y_offset, const char* label,
                                            int current_stat, int pending_change, short mouse_x, short mouse_y,
                                            int arrow_y_offset, bool can_increase, bool can_decrease)
{
	std::string txt;
	uint32_t time = m_game->m_cur_time;

	// Stat label
	put_string(sX + 24, sY + y_offset, (char*)label, GameColors::UIBlack);

	// Current value
	txt = std::format("{}", current_stat);
	put_string(sX + 109, sY + y_offset, txt.c_str(), GameColors::UILabel);

	// New value (with pending changes)
	int new_stat = current_stat + pending_change;
	txt = std::format("{}", new_stat);
	if (new_stat != current_stat)
		put_string(sX + 162, sY + y_offset, txt.c_str(), GameColors::UIRed);
	else
		put_string(sX + 162, sY + y_offset, txt.c_str(), GameColors::UILabel);

	// + arrow highlight
	if ((mouse_x >= sX + 195) && (mouse_x <= sX + 205) && (mouse_y >= sY + arrow_y_offset) && (mouse_y <= sY + arrow_y_offset + 6) && can_increase)
		m_game->m_sprite[InterfaceNdGame4]->draw(sX + 195, sY + arrow_y_offset, 5);

	// - arrow highlight
	if ((mouse_x >= sX + 210) && (mouse_x <= sX + 220) && (mouse_y >= sY + arrow_y_offset) && (mouse_y <= sY + arrow_y_offset + 6) && can_decrease)
		m_game->m_sprite[InterfaceNdGame4]->draw(sX + 210, sY + arrow_y_offset, 6);
}

void DialogBox_LevelUpSetting::on_draw()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	short sX = m_x;
	short sY = m_y;
	short size_x = m_size_x;
	uint32_t time = m_game->m_cur_time;
	std::string txt;

	draw_new_dialog_box(InterfaceNdGame2, sX, sY, 0);
	draw_new_dialog_box(InterfaceNdText, sX, sY, 2);
	draw_new_dialog_box(InterfaceNdGame4, sX + 16, sY + 100, 4);

	// Header text
	put_aligned_string(sX, sX + size_x, sY + 50, DRAW_DIALOGBOX_LEVELUP_SETTING1);
	put_aligned_string(sX, sX + size_x, sY + 65, DRAW_DIALOGBOX_LEVELUP_SETTING2);

	// Points Left
	put_string(sX + 20, sY + 85, DRAW_DIALOGBOX_LEVELUP_SETTING3, GameColors::UIBlack);
	txt = std::format("{}", player().m_lu_point);
	if (player().m_lu_point > 0)
		put_string(sX + 73, sY + 102, txt.c_str(), GameColors::UIGreen);
	else
		put_string(sX + 73, sY + 102, txt.c_str(), GameColors::UIBlack);

	// draw stat rows — can_increase checks base + pending against max
	draw_stat_row(sX, sY, 125, DRAW_DIALOGBOX_LEVELUP_SETTING4, player().m_str, player().m_lu_str,
	            mouse_x, mouse_y, 127, ((player().m_str + player().m_lu_str) < m_game->m_max_stats), (player().m_lu_str > 0));

	draw_stat_row(sX, sY, 144, DRAW_DIALOGBOX_LEVELUP_SETTING5, player().m_vit, player().m_lu_vit,
	            mouse_x, mouse_y, 146, ((player().m_vit + player().m_lu_vit) < m_game->m_max_stats), (player().m_lu_vit > 0));

	draw_stat_row(sX, sY, 163, DRAW_DIALOGBOX_LEVELUP_SETTING6, player().m_dex, player().m_lu_dex,
	            mouse_x, mouse_y, 165, ((player().m_dex + player().m_lu_dex) < m_game->m_max_stats), (player().m_lu_dex > 0));

	draw_stat_row(sX, sY, 182, DRAW_DIALOGBOX_LEVELUP_SETTING7, player().m_int, player().m_lu_int,
	            mouse_x, mouse_y, 184, ((player().m_int + player().m_lu_int) < m_game->m_max_stats), (player().m_lu_int > 0));

	draw_stat_row(sX, sY, 201, DRAW_DIALOGBOX_LEVELUP_SETTING8, player().m_mag, player().m_lu_mag,
	            mouse_x, mouse_y, 203, ((player().m_mag + player().m_lu_mag) < m_game->m_max_stats), (player().m_lu_mag > 0));

	draw_stat_row(sX, sY, 220, DRAW_DIALOGBOX_LEVELUP_SETTING9, player().m_charisma, player().m_lu_char,
	            mouse_x, mouse_y, 222, ((player().m_charisma + player().m_lu_char) < m_game->m_max_stats), (player().m_lu_char > 0));

	// Close button
	if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) &&
	    (mouse_y > sY + ui_layout::btn_y) && (mouse_y < sY + ui_layout::btn_y + ui_layout::btn_size_y))
		draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 1);
	else
		draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 0);

	// Majestic button (only if no pending changes and no points left)
	if ((player().m_lu_str == 0) && (player().m_lu_vit == 0) && (player().m_lu_dex == 0) &&
	    (player().m_lu_int == 0) && (player().m_lu_mag == 0) && (player().m_lu_char == 0))
	{
		if ((mouse_x >= sX + ui_layout::left_btn_x) && (mouse_x <= sX + ui_layout::left_btn_x + ui_layout::btn_size_x) &&
		    (mouse_y > sY + ui_layout::btn_y) && (mouse_y < sY + ui_layout::btn_y + ui_layout::btn_size_y))
		{
			if (player().m_lu_point <= 0)
				draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::left_btn_x, sY + ui_layout::btn_y, 21);
		}
		else
		{
			if (player().m_lu_point <= 0)
				draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::left_btn_x, sY + ui_layout::btn_y, 20);
		}
	}
}

bool DialogBox_LevelUpSetting::handle_stat_click(short mouse_x, short mouse_y, short sX, short sY,
                                                int y_offset, int& current_stat, int16_t& pending_change)
{
	bool majestic_open = m_game->get_dialog_box_manager().is_enabled(DialogBoxId::ChangeStatsMajestic);

	// + button
	if ((mouse_x >= sX + 195) && (mouse_x <= sX + 205) && (mouse_y >= sY + y_offset) && (mouse_y <= sY + y_offset + 6) &&
	    ((current_stat + pending_change) < m_game->m_max_stats) && (player().m_lu_point > 0))
	{
		int room = m_game->m_max_stats - (current_stat + pending_change);
		if (hb::shared::input::is_ctrl_down() && hb::shared::input::is_shift_down())
		{
			int add = std::min({ player().m_lu_point, room });
			if ((add > 0) && !majestic_open)
			{
				player().m_lu_point -= add;
				pending_change += static_cast<int16_t>(add);
			}
		}
		else if (hb::shared::input::is_ctrl_down())
		{
			int add = std::min({ 5, player().m_lu_point, room });
			if ((add > 0) && !majestic_open)
			{
				player().m_lu_point -= add;
				pending_change += static_cast<int16_t>(add);
			}
		}
		else if (hb::shared::input::is_shift_down())
		{
			int add = std::min({ 10, player().m_lu_point, room });
			if ((add > 0) && !majestic_open)
			{
				player().m_lu_point -= add;
				pending_change += static_cast<int16_t>(add);
			}
		}
		else
		{
			if ((player().m_lu_point > 0) && (room > 0) && !majestic_open)
			{
				player().m_lu_point--;
				pending_change++;
			}
		}
		audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
		return true;
	}

	// - button
	if ((mouse_x >= sX + 210) && (mouse_x <= sX + 220) && (mouse_y >= sY + y_offset) && (mouse_y <= sY + y_offset + 6) &&
	    (pending_change > 0))
	{
		if (hb::shared::input::is_ctrl_down() && hb::shared::input::is_shift_down())
		{
			if ((pending_change > 0) && !majestic_open)
			{
				player().m_lu_point += pending_change;
				pending_change = 0;
			}
		} else if(hb::shared::input::is_ctrl_down())
		{
			int remove = std::min(static_cast<int>(pending_change), 5);
			if ((remove > 0) && !majestic_open)
			{
				pending_change -= static_cast<int16_t>(remove);
				player().m_lu_point += remove;
			}
		}
		else if (hb::shared::input::is_shift_down())
		{
			int remove = std::min(static_cast<int>(pending_change), 10);
			if ((remove > 0) && !majestic_open)
			{
				pending_change -= static_cast<int16_t>(remove);
				player().m_lu_point += remove;
			}
		}
		else
		{
			if ((pending_change > 0) && !majestic_open)
			{
				pending_change--;
				player().m_lu_point++;
			}
		}
		audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
		return true;
	}

	return false;
}

bool DialogBox_LevelUpSetting::on_click()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	short sX = m_x;
	short sY = m_y;

	// Strength +/-
	if (handle_stat_click(mouse_x, mouse_y, sX, sY, 127, player().m_str, player().m_lu_str))
		return true;

	// Vitality +/-
	if (handle_stat_click(mouse_x, mouse_y, sX, sY, 146, player().m_vit, player().m_lu_vit))
		return true;

	// Dexterity +/-
	if (handle_stat_click(mouse_x, mouse_y, sX, sY, 165, player().m_dex, player().m_lu_dex))
		return true;

	// Intelligence +/-
	if (handle_stat_click(mouse_x, mouse_y, sX, sY, 184, player().m_int, player().m_lu_int))
		return true;

	// Magic +/-
	if (handle_stat_click(mouse_x, mouse_y, sX, sY, 203, player().m_mag, player().m_lu_mag))
		return true;

	// Charisma +/-
	if (handle_stat_click(mouse_x, mouse_y, sX, sY, 222, player().m_charisma, player().m_lu_char))
		return true;

	// Close/OK button
	if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) &&
	    (mouse_y > sY + ui_layout::btn_y) && (mouse_y < sY + ui_layout::btn_y + ui_layout::btn_size_y))
	{
		if (m_initial_lu_points != player().m_lu_point)
			{
		hb::net::PacketRequestLevelUpSettings req{};
		req.header.msg_id = MsgId::LevelUpSettings;
		req.header.msg_type = 0;
		req.str = player().m_lu_str;
		req.vit = player().m_lu_vit;
		req.dex = player().m_lu_dex;
		req.intel = player().m_lu_int;
		req.mag = player().m_lu_mag;
		req.chr = player().m_lu_char;
		send_game_packet(req);
	}
		disable_this_dialog();
		audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
		return true;
	}

	// Majestic button
	if ((mouse_x >= sX + ui_layout::left_btn_x) && (mouse_x <= sX + ui_layout::left_btn_x + ui_layout::btn_size_x) &&
	    (mouse_y > sY + ui_layout::btn_y) && (mouse_y < sY + ui_layout::btn_y + ui_layout::btn_size_y))
	{
		if ((m_game->on_game()->m_gizon_item_upgrade_left > 0) && (player().m_lu_point <= 0) &&
		    (player().m_lu_str == 0) && (player().m_lu_vit == 0) && (player().m_lu_dex == 0) &&
		    (player().m_lu_int == 0) && (player().m_lu_mag == 0) && (player().m_lu_char == 0))
		{
			disable_this_dialog();
			enable_dialog_box(DialogBoxId::ChangeStatsMajestic, 0, 0, 0);
			return true;
		}
	}

	return false;
}

bool DialogBox_LevelUpSetting::on_enable(int type, int64_t v1, int v2, const char* string)
{
	if (is_enabled()) return true;
	auto* charDlg = get_dialog_box(DialogBoxId::CharacterInfo);
	if (charDlg) { m_x = charDlg->m_x + 20; m_y = charDlg->m_y + 20; }
	m_initial_lu_points = player().m_lu_point;
	return true;
}
