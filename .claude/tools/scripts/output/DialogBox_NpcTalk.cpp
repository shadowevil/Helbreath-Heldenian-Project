#include "DialogBox_NpcTalk.h"
#include "Game.h"
#include "GameFonts.h"
#include "TextLibExt.h"
#include "IInput.h"
#include "NetMessages.h"
#include "PacketSendHelpers.h"
#include "Screen_OnGame.h"



using namespace hb::shared::net;
using namespace hb::client::sprite_id;
DialogBox_NpcTalk::DialogBox_NpcTalk(CGame* game)
	: IDialogBox(DialogBoxId::NpcTalk, game)
{
	set_default_rect(497 , 57 , 258, 339);
}

int DialogBox_NpcTalk::get_total_lines() const
{
	int total_lines = 0;
	for (int i = 0; i < game_limits::max_text_dlg_lines; i++)
	{
		if (m_game->on_game()->m_msg_text_list2[i] != nullptr)
			total_lines++;
	}
	return total_lines;
}

void DialogBox_NpcTalk::on_draw()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	char lb = hb::shared::input::is_mouse_button_down(hb::shared::input::MouseButton::Left) ? 1 : 0;
	short sX = m_x;
	short sY = m_y;

	m_game->draw_new_dialog_box(InterfaceNdGame2, sX, sY, 2);

	draw_buttons(sX, sY);
	draw_text_content(sX, sY);

	int total_lines = get_total_lines();
	draw_scroll_bar(sX, sY, total_lines);
	handle_scroll_bar_drag(sX, sY, mouse_x, mouse_y, total_lines, lb);
}

void DialogBox_NpcTalk::draw_buttons(short sX, short sY)
{
	switch (m_mode)
	{
	case mode::ok_only:
		if (mouse_in(btn_right))
			m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 1);
		else
			m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 0);
		break;

	case mode::accept_decline:
		if (mouse_in(btn_left))
			m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::left_btn_x, sY + ui_layout::btn_y, 33);
		else
			m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::left_btn_x, sY + ui_layout::btn_y, 32);

		if (mouse_in(btn_right))
			m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 41);
		else
			m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 40);
		break;

	case mode::next:
		if (mouse_in(link_next))
			hb::shared::text::draw_text(GameFont::Bitmap1, sX + 190, sY + 270, "Next", hb::shared::text::TextStyle::with_highlight(GameColors::UIMagicBlue));
		else
			hb::shared::text::draw_text(GameFont::Bitmap1, sX + 190, sY + 270, "Next", hb::shared::text::TextStyle::with_highlight(GameColors::BmpBtnNormal));
		break;
	}
}

void DialogBox_NpcTalk::draw_text_content(short sX, short sY)
{
	short view = m_scroll_view;
	short size_x = m_size_x;

	for (int i = 0; i < 17; i++)
	{
		if ((i < game_limits::max_text_dlg_lines) && (m_game->on_game()->m_msg_text_list2[i + view] != nullptr))
		{
			hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 57 + i * 15, sX + size_x - sX, 15,
				m_game->on_game()->m_msg_text_list2[i + view]->m_pMsg, hb::shared::text::TextStyle::from_color(GameColors::UILabel), hb::shared::text::Align::TopCenter);
		}
	}
}

void DialogBox_NpcTalk::draw_scroll_bar(short sX, short sY, int total_lines)
{
	if (total_lines > 17)
	{
		double d1 = static_cast<double>(m_scroll_view);
		double d2 = static_cast<double>(total_lines - 17);
		double d3 = (274.0 * d1) / d2;
		int pointer_loc = static_cast<int>(d3);
		m_game->draw_new_dialog_box(InterfaceNdGame2, sX, sY, 3);
	}
}

void DialogBox_NpcTalk::handle_scroll_bar_drag(short sX, short sY, short mouse_x, short mouse_y, int total_lines, char lb)
{
	if (lb != 0 && total_lines > 17)
	{
		if (m_game->get_dialog_box_manager().get_top_id() == DialogBoxId::NpcTalk)
		{
			if (mouse_in(area_scroll))
			{
				double d1 = static_cast<double>(mouse_y - (sY + 40));
				double d2 = static_cast<double>(total_lines - 17);
				double d3 = (d1 * d2) / 274.0;
				int pointer_loc = static_cast<int>(d3);

				if (pointer_loc > total_lines)
					pointer_loc = total_lines;
				m_scroll_view = pointer_loc;
			}
		}
	}
	else
	{
		m_is_scroll_selected = false;
	}
}

bool DialogBox_NpcTalk::on_click()
{
	switch (m_mode)
	{
	case mode::ok_only:
		if (mouse_in(btn_right))
		{
			m_game->get_dialog_box_manager().disable_dialog_box(DialogBoxId::NpcTalk);
			m_game->play_game_sound('E', 14, 5);
			return true;
		}
		break;

	case mode::accept_decline:
		if (mouse_in(btn_left))
		{
			// Accept
			m_game->send_game_packet(hb::net::make_common_command(CommonType::QuestAccepted, m_game->m_player->m_player_x, m_game->m_player->m_player_y));
			m_game->get_dialog_box_manager().disable_dialog_box(DialogBoxId::NpcTalk);
			m_game->play_game_sound('E', 14, 5);
			return true;
		}
		if (mouse_in(btn_right))
		{
			// Decline
			m_game->get_dialog_box_manager().disable_dialog_box(DialogBoxId::NpcTalk);
			m_game->play_game_sound('E', 14, 5);
			return true;
		}
		break;

	case mode::next:
		if (mouse_in(btn_right))
		{
			m_game->get_dialog_box_manager().disable_dialog_box(DialogBoxId::NpcTalk);
			m_game->play_game_sound('E', 14, 5);
			return true;
		}
		break;
	}

	return false;
}

PressResult DialogBox_NpcTalk::on_press()
{
	// Scroll bar region
	if (mouse_in(area_scroll))
	{
		return PressResult::ScrollClaimed;
	}

	return PressResult::Normal;
}

bool DialogBox_NpcTalk::on_enable(int type, int64_t v1, int v2, const char* string)
{
	if (is_enabled()) return true;
	m_mode = static_cast<mode>(type);
	m_scroll_view = 0;
	m_text_line_count = m_game->load_text_dlg_contents2(static_cast<int>(v1) + 20);
	m_dialog_id = static_cast<int>(v1) + 20;
	return true;
}

bool DialogBox_NpcTalk::on_disable()
{
	if (m_dialog_id == 500)
	{
		m_game->send_game_packet(hb::net::make_common_command(CommonType::GetMagicAbility, m_game->m_player->m_player_x, m_game->m_player->m_player_y));
	}
	return true;
}
