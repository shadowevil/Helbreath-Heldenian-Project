#include "DialogBox_Skill.h"
#include "Game.h"
#include "PacketSendHelpers.h"

#include "IInput.h"
#include "Misc.h"
#include "lan_eng.h"
#include <format>
#include <string>
#include "Screen_OnGame.h"
#include "AudioManager.h"

using namespace hb::shared::net;
using namespace hb::client::sprite_id;
DialogBox_Skill::DialogBox_Skill(CGame* game)
	: IDialogBox(DialogBoxId::Skill, game)
{
	set_default_rect(497 , 57 , 258, 339);
}

void DialogBox_Skill::on_draw()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	short z = static_cast<short>(hb::shared::input::get_mouse_wheel_delta());
	char lb = hb::shared::input::is_mouse_button_down(hb::shared::input::MouseButton::Left) ? 1 : 0;
	if (!m_game->ensure_skill_configs_loaded()) return;
	short sX, sY;
	int i, total_lines, pointer_loc;
	std::string temp2;

	char temp[255];
	double d1, d2, d3;

	sX = m_x;
	sY = m_y;

	draw_new_dialog_box(InterfaceNdGame2, sX, sY, 0); // Normal Dialog
	draw_new_dialog_box(InterfaceNdText, sX, sY, 1); // Skill Dialog Title Bar

	switch (m_mode) {
	case 0:
		for (i = 0; i < 17; i++)
			if (((i + m_scroll_position) < hb::shared::limits::MaxSkillType) && (m_game->m_skill_cfg_list[i + m_scroll_position] != 0))
			{
				std::snprintf(temp, sizeof(temp), "%s", m_game->m_skill_cfg_list[i + m_scroll_position]->m_name.c_str());
				CMisc::replace_string(temp, '-', ' ');
				temp2 = std::format("{:3}%", m_game->m_skill_cfg_list[i + m_scroll_position]->m_level);
				if ((mouse_x >= sX + 25) && (mouse_x <= sX + 166) && (mouse_y >= sY + 45 + i * 15) && (mouse_y <= sY + 59 + i * 15))
				{
					if ((m_game->m_skill_cfg_list[i + m_scroll_position]->m_is_useable == true)
						&& (m_game->m_skill_cfg_list[i + m_scroll_position]->m_level != 0))
					{
						put_string(sX + 30, sY + 45 + i * 15, temp, GameColors::UIWhite);
						put_string(sX + 183, sY + 45 + i * 15, temp2.c_str(), GameColors::UIWhite);
					}
					else
					{
						put_string(sX + 30, sY + 45 + i * 15, temp, GameColors::UIBlack);
						put_string(sX + 183, sY + 45 + i * 15, temp2.c_str(), GameColors::UIBlack);
					}
				}
				else
				{
					if ((m_game->m_skill_cfg_list[i + m_scroll_position]->m_is_useable == true)
						&& (m_game->m_skill_cfg_list[i + m_scroll_position]->m_level != 0))
					{
						put_string(sX + 30, sY + 45 + i * 15, temp, GameColors::UIMagicBlue);
						put_string(sX + 183, sY + 45 + i * 15, temp2.c_str(), GameColors::UIMagicBlue);
					}
					else
					{
						put_string(sX + 30, sY + 45 + i * 15, temp, GameColors::UIBlack);
						put_string(sX + 183, sY + 45 + i * 15, temp2.c_str(), GameColors::UIBlack);
					}
				}

				if (m_game->on_game()->m_down_skill_index == (i + m_scroll_position))
					m_game->m_sprite[InterfaceNdGame2]->draw(sX + 220, sY + 49 + i * 15, 26, hb::shared::sprite::DrawParams::tint(GameColors::UIWhite.r, GameColors::UIWhite.g, GameColors::UIWhite.b));
				else m_game->m_sprite[InterfaceNdGame2]->draw(sX + 220, sY + 49 + i * 15, 25, hb::shared::sprite::DrawParams::tint(1, 1, 1));
			}

		total_lines = 0;
		for (i = 0; i < hb::shared::limits::MaxSkillType; i++)
			if (m_game->m_skill_cfg_list[i] != 0) total_lines++;

		if (total_lines > 17)
		{
			d1 = static_cast<double>(m_scroll_position);
			d2 = static_cast<double>(total_lines - 17);
			d3 = (274.0f * d1) / d2;
			pointer_loc = static_cast<int>(d3);
		}
		else pointer_loc = 0;
		if (total_lines > 17)
		{
			draw_new_dialog_box(InterfaceNdGame2, sX, sY, 1);
			draw_new_dialog_box(InterfaceNdGame2, sX + 242, sY + pointer_loc + 35, 7);
		}

		if (lb != 0 && total_lines > 17)
		{
			if ((m_game->get_dialog_box_manager().get_top_id() == DialogBoxId::Skill))
			{
				if ((mouse_x >= sX + 240) && (mouse_x <= sX + 260) && (mouse_y >= sY + 30) && (mouse_y <= sY + 320))
				{
					d1 = static_cast<double>(mouse_y - (sY + 35));
					d2 = static_cast<double>(total_lines - 17);
					d3 = (d1 * d2) / 274.0f;
					pointer_loc = static_cast<int>(d3 + 0.5);
					if (pointer_loc > total_lines - 17) pointer_loc = total_lines - 17;
					m_scroll_position = pointer_loc;
				}
			}
		}
		else m_is_scroll_selected = false;
		if (m_game->get_dialog_box_manager().get_top_id() == DialogBoxId::Skill && z != 0)
		{
			if (z > 0) m_scroll_position--;
			if (z < 0) m_scroll_position++;

		}
		if (m_scroll_position < 0) m_scroll_position = 0;
		if (total_lines > 17 && m_scroll_position > total_lines - 17) m_scroll_position = total_lines - 17;
		break;
	}
}

bool DialogBox_Skill::on_click()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	int i;
	short sX, sY;
	sX = m_x;
	sY = m_y;
	switch (m_mode) {
	case -1:
		break;
	case 0:
		for (i = 0; i < 17; i++)
			if (((i + m_scroll_position) < hb::shared::limits::MaxSkillType) && (m_game->m_skill_cfg_list[i + m_scroll_position] != 0))
			{
				if ((mouse_x >= sX + 25) && (mouse_x <= sX + 166) && (mouse_y >= sY + 45 + i * 15) && (mouse_y <= sY + 59 + i * 15))
				{
					if ((m_game->m_skill_cfg_list[i + m_scroll_position]->m_is_useable == true)
						&& (m_game->m_skill_cfg_list[i + m_scroll_position]->m_level != 0))
					{
						if (m_game->on_game()->m_skill_using_status == true)
						{
							add_event_list(DLGBOX_CLICK_SKILL1, 10); // "You are already using other skill."
							return true;
						}
						if ((player().m_Controller.is_command_available() == false) || (player().m_hp <= 0))
						{
							add_event_list(DLGBOX_CLICK_SKILL2, 10); // "You can't use a skill while you are moving."
							return true;
						}
						if (m_game->on_game()->m_is_get_pointing_mode == true)
						{
							return true;
						}
						switch (m_game->m_skill_cfg_list[i + m_scroll_position]->m_use_method) {
						case 0:
						case 2:
							{
								auto pkt = hb::net::make_common_command(CommonType::ReqUseSkill, player().m_player_x, player().m_player_y);
								pkt.v1 = (i + m_scroll_position);
								send_game_packet(pkt);
							}
							m_game->on_game()->m_skill_using_status = true;
							disable_this_dialog();
							audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
							return true;
						case 1:
							{
								auto pkt = hb::net::make_common_command(CommonType::ReqUseSkill, player().m_player_x, player().m_player_y);
								pkt.v1 = (i + m_scroll_position);
								send_game_packet(pkt);
							}
							m_game->on_game()->m_skill_using_status = true;
							audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
							return true;
						}
					}
				}
				else if ((mouse_x >= sX + 215) && (mouse_x <= sX + 240) && (mouse_y >= sY + 45 + i * 15) && (mouse_y <= sY + 59 + i * 15))
				{
					if (m_is_down_skill_pending == false)
					{
						{
							auto pkt = hb::net::make_common_command(CommonType::ReqSetDownSkillIndex, player().m_player_x, player().m_player_y);
							pkt.v1 = i + m_scroll_position;
							send_game_packet(pkt);
						}
						audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
						m_is_down_skill_pending = true;
						return true;
					}
				}
			}
		break;
	}
	return false;
}

PressResult DialogBox_Skill::on_press()
{
	if (mouse_in(area_scroll))
		return PressResult::ScrollClaimed;

	return PressResult::Normal;
}

