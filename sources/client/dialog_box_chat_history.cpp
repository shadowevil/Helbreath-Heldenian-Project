#include "dialog_box_chat_history.h"
#include "chat_manager.h"
#include "config_manager.h"
#include "game.h"
#include "input.h"
#include "game_fonts.h"
#include "text_lib_ext.h"
#include "panel_kit.h"

#include <cmath>

#define DEF_CHAT_VISIBLE_LINES 8

namespace
{
	constexpr int scroll_range = game_limits::max_chat_scroll_msgs - DEF_CHAT_VISIBLE_LINES;
}

DialogBox_ChatHistory::DialogBox_ChatHistory(CGame* game)
	: IDialogBox(DialogBoxId::ChatHistory, game)
{
	set_default_rect(218 , 385 , 364, 162);
}

void DialogBox_ChatHistory::on_draw()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	short z = static_cast<short>(hb::shared::input::get_mouse_wheel_delta());
	char lb = hb::shared::input::is_mouse_button_down(hb::shared::input::MouseButton::Left) ? 1 : 0;
	short sX = m_x;
	short sY = m_y;

	const bool dialogTrans = config_manager::get().is_dialog_transparency_enabled();
	// Layout from the approved rebuild, dialog coordinates
	const hb::client::kit_dialog kit(m_game->get_panel_kit(), hb::client::dialog_size::chat_history,
		hb::client::kit_point{sX, sY}, hb::client::dialog_alpha(dialogTrans));
	kit.title("History", 19, {144, 16, 12}, "divider_wave", 14);   // y 19: the sprite title's height (measured, HH-0065)
	kit.inset({13, 28, 327, 121});

	handle_scroll_input(kit);
	draw_scroll_bar(kit);
	draw_chat_messages(sX, sY);
}

void DialogBox_ChatHistory::handle_scroll_input(const hb::client::kit_dialog& kit)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	char lb = hb::shared::input::is_mouse_button_down(hb::shared::input::MouseButton::Left) ? 1 : 0;
	// Mouse wheel scrolling
	if (m_game->get_dialog_box_manager().get_top_id() == DialogBoxId::ChatHistory)
	{
		short wheel_delta = hb::shared::input::get_mouse_wheel_delta();
		if (wheel_delta != 0)
		{
			m_scroll_position += wheel_delta / 30;
		}
	}

	// Scroll bar dragging
	if ((lb != 0) && (m_game->get_dialog_box_manager().get_top_id() == DialogBoxId::ChatHistory))
	{
		// Kit groove: the thumb's centre follows the cursor (the inverse of its draw placement); the
		// newest line (position 0) is at the bottom
		if (mouse_in(kit_track))
		{
			const float pos = kit.scrollbar_pos_at(kit_scrollbar(), mouse_y);
			m_scroll_position = static_cast<short>(std::lround((1.0f - pos) * scroll_range));
		}
	}
	else
	{
		m_is_scroll_selected = false;
	}

	// clamp scroll view (must be after all scroll modifications)
	if (m_scroll_position < 0)
		m_scroll_position = 0;
	if (m_scroll_position > game_limits::max_chat_scroll_msgs - DEF_CHAT_VISIBLE_LINES)
		m_scroll_position = game_limits::max_chat_scroll_msgs - DEF_CHAT_VISIBLE_LINES;
}

hb::client::kit_rect DialogBox_ChatHistory::kit_scrollbar()
{
	return {kit_track.x, kit_track.y, kit_track.w, kit_track.h};
}

void DialogBox_ChatHistory::draw_scroll_bar(const hb::client::kit_dialog& kit)
{
	// Groove + thumb; pos 1 = the newest line (scroll position 0) at the bottom, as the sprite thumb
	const float pos = 1.0f - static_cast<float>(m_scroll_position) / static_cast<float>(scroll_range);
	kit.scrollbar(kit_scrollbar(), pos);
}

void DialogBox_ChatHistory::draw_chat_messages(short sX, short sY)
{
	short view = m_scroll_position;

	for (int i = 0; i < DEF_CHAT_VISIBLE_LINES; i++)
	{
		int index = i + view;
		if (index < 0 || index >= game_limits::max_chat_scroll_msgs) continue;
		CMsg* chat_msg = ChatManager::get().get_message(index);
		if (chat_msg != nullptr)
		{
			int y_pos = sY + 127 - i * 13;
			char* pMsg = chat_msg->m_pMsg;

			switch (chat_msg->m_time)
			{
			case 0:  hb::shared::text::draw_text(GameFont::Default, sX + 25, y_pos, pMsg, hb::shared::text::TextStyle::with_shadow(GameColors::UINearWhite)); break; // Normal
			case 1:  hb::shared::text::draw_text(GameFont::Default, sX + 25, y_pos, pMsg, hb::shared::text::TextStyle::with_shadow(GameColors::UIGuildGreen)); break; // Green
			case 2:  hb::shared::text::draw_text(GameFont::Default, sX + 25, y_pos, pMsg, hb::shared::text::TextStyle::with_shadow(GameColors::UIWorldChat)); break; // Red
			case 3:  hb::shared::text::draw_text(GameFont::Default, sX + 25, y_pos, pMsg, hb::shared::text::TextStyle::with_shadow(GameColors::UIFactionChat)); break; // Blue
			case 4:  hb::shared::text::draw_text(GameFont::Default, sX + 25, y_pos, pMsg, hb::shared::text::TextStyle::with_shadow(GameColors::UIPartyChat)); break; // Yellow
			case 10: hb::shared::text::draw_text(GameFont::Default, sX + 25, y_pos, pMsg, hb::shared::text::TextStyle::with_shadow(GameColors::UIGameMasterChat)); break; // Light green
			case 20: hb::shared::text::draw_text(GameFont::Default, sX + 25, y_pos, pMsg, hb::shared::text::TextStyle::with_shadow(GameColors::UINormalChat)); break; // Gray
			}
		}
	}
}

bool DialogBox_ChatHistory::on_click()
{
	// Chat history dialog has no click actions - scrolling is handled in on_draw
	return false;
}

PressResult DialogBox_ChatHistory::on_press()
{
	// Check if click is in the scroll bar region (the kit groove)
	if (mouse_in(kit_track))
		return PressResult::ScrollClaimed;

	return PressResult::Normal;
}

