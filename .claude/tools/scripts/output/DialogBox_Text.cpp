#include "DialogBox_Text.h"
#include "ConfigManager.h"
#include "Game.h"
#include "IInput.h"
#include "GameFonts.h"
#include "TextLibExt.h"
#include "Screen_OnGame.h"
using namespace hb::client::sprite_id;
// game_limits::max_text_dlg_lines is in GameConstants.h (via Game.h)

DialogBox_Text::DialogBox_Text(CGame* game)
	: IDialogBox(DialogBoxId::Text, game)
{
	set_default_rect(20 , 65 , 258, 339);
}

int DialogBox_Text::get_total_lines() const
{
	int total_lines = 0;
	for (int i = 0; i < game_limits::max_text_dlg_lines; i++)
	{
		if (m_game->on_game()->m_msg_text_list[i] != nullptr)
			total_lines++;
	}
	return total_lines;
}

void DialogBox_Text::on_draw()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	short z = static_cast<short>(hb::shared::input::get_mouse_wheel_delta());
	char lb = hb::shared::input::is_mouse_button_down(hb::shared::input::MouseButton::Left) ? 1 : 0;
	short sX = m_x;
	short sY = m_y;

	m_game->draw_new_dialog_box(InterfaceNdGame2, sX, sY, 0);

	int total_lines = get_total_lines();

	if (total_lines > 17)
		m_game->draw_new_dialog_box(InterfaceNdGame2, sX, sY, 1);

	// Mouse wheel scrolling
	if (m_game->get_dialog_box_manager().get_top_id() == DialogBoxId::Text && z != 0)
	{
		m_scroll_view -= z / 60;

	}

	// clamp scroll view
	if (m_scroll_view < 0)
		m_scroll_view = 0;
	if (total_lines > 17 && m_scroll_view > total_lines - 17)
		m_scroll_view = total_lines - 17;

	// draw scroll bar
	int pointer_loc = 0;
	if (total_lines > 17)
	{
		double d1 = static_cast<double>(m_scroll_view);
		double d2 = static_cast<double>(total_lines - 17);
		double d3 = (274.0 * d1) / d2;
		pointer_loc = static_cast<int>(d3 + 0.5);
		m_game->draw_new_dialog_box(InterfaceNdGame2, sX, sY, 1);
		m_game->draw_new_dialog_box(InterfaceNdGame2, sX + 242, sY + 35 + pointer_loc, 7);
	}

	// draw text lines
	short view = m_scroll_view;
	for (int i = 0; i < 17; i++)
	{
		if ((i + view) < game_limits::max_text_dlg_lines && m_game->on_game()->m_msg_text_list[i + view] != nullptr)
		{
			char* pMsg = m_game->on_game()->m_msg_text_list[i + view]->m_pMsg;
			if (config_manager::get().is_dialog_transparency_enabled() == false)
			{
				switch (pMsg[0])
				{
				case '_':
					hb::shared::text::draw_text_aligned(GameFont::Default, sX + 24, sY + 50 + i * 13, sX + 236 - (sX + 24), 15, pMsg + 1, hb::shared::text::TextStyle::from_color(GameColors::UIWhite), hb::shared::text::Align::TopCenter);
					break;
				case ';':
					hb::shared::text::draw_text_aligned(GameFont::Default, sX + 24, sY + 50 + i * 13, sX + 236 - (sX + 24), 15, pMsg + 1, hb::shared::text::TextStyle::from_color(GameColors::UIMagicBlue), hb::shared::text::Align::TopCenter);
					break;
				default:
					hb::shared::text::draw_text_aligned(GameFont::Default, sX + 24, sY + 50 + i * 13, sX + 236 - (sX + 24), 15, pMsg, hb::shared::text::TextStyle::from_color(GameColors::UILabel), hb::shared::text::Align::TopCenter);
					break;
				}
			}
			else
			{
				hb::shared::text::draw_text_aligned(GameFont::Default, sX + 24, sY + 50 + i * 13, sX + 236 - (sX + 24), 15, pMsg, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
			}
		}
	}

	// Handle scroll bar dragging
	if (lb != 0 && total_lines > 17)
	{
		if (m_game->get_dialog_box_manager().get_top_id() == DialogBoxId::Text)
		{
			if (mouse_in(area_scroll))
			{
				double d1 = static_cast<double>(mouse_y - (sY + 35));
				double d2 = static_cast<double>(total_lines - 17);
				double d3 = (d1 * d2) / 274.0;
				pointer_loc = static_cast<int>(d3);
				if (pointer_loc > total_lines - 17)
					pointer_loc = total_lines - 17;
				m_scroll_view = pointer_loc;
			}
		}
	}
	else
	{
		m_is_scroll_selected = false;
	}

	// Close button hover highlight
	if (mouse_in(btn_close))
		m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 1);
	else
		m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 0);
}

bool DialogBox_Text::on_click()
{
	// Close button
	if (mouse_in(btn_close))
	{
		m_game->get_dialog_box_manager().disable_dialog_box(DialogBoxId::Text);
		m_game->play_game_sound('E', 14, 5);
		return true;
	}

	return false;
}

PressResult DialogBox_Text::on_press()
{
	// Scroll bar region
	if (mouse_in(area_scroll))
		return PressResult::ScrollClaimed;

	return PressResult::Normal;
}

bool DialogBox_Text::on_enable(int type, int64_t v1, int v2, const char* string)
{
	if (is_enabled()) return true;
	switch (type) {
	case 0:
		m_mode = 0;
		m_scroll_view = 0;
		break;
	default:
		m_game->load_text_dlg_contents(type);
		m_mode = 0;
		m_scroll_view = 0;
		break;
	}
	return true;
}
