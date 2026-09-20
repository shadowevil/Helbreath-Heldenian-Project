// TESTER MENU — entire file is tester-only
#ifdef TESTER_ONLY
#include "DialogBox_TesterMenu.h"
#include "Game.h"
#include "GlobalDef.h"
#include "SpriteID.h"
#include "NetMessages.h"
#include "PacketSendHelpers.h"

#include "GameFonts.h"
#include "TextLibExt.h"
#include <algorithm>
#include <format>
#include <cstring>
#include "IInput.h"
#include "AudioManager.h"

using namespace hb::shared::net;
using namespace hb::client::sprite_id;

const DialogBox_TesterMenu::action_entry DialogBox_TesterMenu::actions[action_count] = {
	{ "Reset stats" },
	{ "Add 100 contribution" },
	{ "Add 100 majestics" },
	{ "Add 100 eks" },
	{ "Add 1m gold" },
	{ "Add 100 crits" },
	{ "Max all skills" },
	{ "Set level" },
	{ "Create item" },
	{ "Teleport" },
};

DialogBox_TesterMenu::DialogBox_TesterMenu(CGame* game)
	: IDialogBox(DialogBoxId::TesterMenu, game)
{
	set_default_rect(0, 0, 258, 339);
	m_can_close_on_right_click = true;
}

int DialogBox_TesterMenu::get_hovered_row(short sX, short sY, short mouse_x, short mouse_y, int count, int scroll) const
{
	if (mouse_x < sX + row_x1 || mouse_x > sX + row_x2) return -1;

	for (int i = 0; i < count; i++)
	{
		int y_top = sY + first_row_y + i * row_height;
		int y_bot = y_top + row_height - 2;
		if (mouse_y >= y_top && mouse_y <= y_bot)
			return i + scroll;
	}
	return -1;
}

void DialogBox_TesterMenu::draw_main_menu(short sX, short sY, short size_x)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	// Title
	hb::shared::text::draw_text_aligned(GameFont::Bitmap1,
		sX, sY + 8, size_x, 15,
		"Tester Menu",
		hb::shared::text::TextStyle::with_integrated_shadow(GameColors::UIWarningRed),
		hb::shared::text::Align::TopCenter);

	// Action rows
	int hovered = get_hovered_row(sX, sY, mouse_x, mouse_y, action_count);
	for (int i = 0; i < action_count; i++)
	{
		int y = sY + first_row_y + i * row_height;
		auto color = (i == hovered) ? GameColors::UIWhite : GameColors::UIMagicBlue;
		hb::shared::text::draw_text_aligned(GameFont::Default,
			sX + row_x1, y, (sX + row_x2) - (sX + row_x1), 15,
			actions[i].label,
			hb::shared::text::TextStyle::from_color(color),
			hb::shared::text::Align::TopCenter);
	}

	// Close button
	if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) &&
		(mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y))
		draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 1);
	else
		draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 0);
}

void DialogBox_TesterMenu::draw_level_picker(short sX, short sY, short size_x)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	// Title
	hb::shared::text::draw_text_aligned(GameFont::Bitmap1,
		sX, sY + 8, size_x, 15,
		"Set Level",
		hb::shared::text::TextStyle::with_integrated_shadow(GameColors::UIWarningRed),
		hb::shared::text::Align::TopCenter);

	// Level display
	auto level_str = std::format("Level: {}", m_selected_level);
	hb::shared::text::draw_text_aligned(GameFont::Bitmap1,
		sX, sY + 100, size_x, 15,
		level_str.c_str(),
		hb::shared::text::TextStyle::with_integrated_shadow(GameColors::UIWhite),
		hb::shared::text::Align::TopCenter);

	// Buttons: [ -10 ] [ -1 ] [ +1 ] [ +10 ]
	constexpr int btn_w = 40;
	constexpr int btn_gap = 8;
	constexpr int total_w = btn_w * 4 + btn_gap * 3;
	int btn_start_x = sX + (size_x - total_w) / 2;
	int btn_y = sY + 160;

	const char* labels[] = { "-10", "-1", "+1", "+10" };
	for (int i = 0; i < 4; i++)
	{
		int bx = btn_start_x + i * (btn_w + btn_gap);
		bool hover = (mouse_x >= bx && mouse_x <= bx + btn_w && mouse_y >= btn_y && mouse_y <= btn_y + 18);
		auto color = hover ? GameColors::UIWhite : GameColors::UIMagicBlue;
		hb::shared::text::draw_text_aligned(GameFont::Default,
			bx, btn_y, btn_w, 15,
			labels[i],
			hb::shared::text::TextStyle::from_color(color),
			hb::shared::text::Align::TopCenter);
	}

	// [ Apply ] button
	int apply_x = sX + (size_x - 50) / 2;
	int apply_y = sY + 220;
	bool apply_hover = (mouse_x >= apply_x && mouse_x <= apply_x + 50 && mouse_y >= apply_y && mouse_y <= apply_y + 18);
	hb::shared::text::draw_text_aligned(GameFont::Default,
		apply_x, apply_y, 50, 15,
		"Apply",
		hb::shared::text::TextStyle::from_color(apply_hover ? GameColors::UIWhite : GameColors::UIMagicBlue),
		hb::shared::text::Align::TopCenter);

	// [ Back ] button
	int back_x = sX + (size_x - 50) / 2;
	int back_y = sY + 260;
	bool back_hover = (mouse_x >= back_x && mouse_x <= back_x + 50 && mouse_y >= back_y && mouse_y <= back_y + 18);
	hb::shared::text::draw_text_aligned(GameFont::Default,
		back_x, back_y, 50, 15,
		"Back",
		hb::shared::text::TextStyle::from_color(back_hover ? GameColors::UIWhite : GameColors::UIMagicBlue),
		hb::shared::text::Align::TopCenter);
}

void DialogBox_TesterMenu::draw_teleport_page(short sX, short sY, short size_x)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	// Title
	hb::shared::text::draw_text_aligned(GameFont::Bitmap1,
		sX, sY + 8, size_x, 15,
		"Teleport",
		hb::shared::text::TextStyle::with_integrated_shadow(GameColors::UIWarningRed),
		hb::shared::text::Align::TopCenter);

	int list_x = sX + row_x1;
	int list_w = row_x2 - row_x1;

	if (m_map_count == 0)
	{
		put_aligned_string(sX, sX + size_x, sY + 40, "Loading maps...", GameColors::UIBlack);
	}
	else
	{
		auto status = std::format("{} maps - click to teleport", m_map_count);
		put_aligned_string(sX, sX + size_x, sY + 35, status.c_str(), GameColors::UIBlack);

		if (m_map_count > visible_map_rows)
		{
			auto scroll_info = std::format("[{}-{} of {}]",
				m_map_scroll + 1,
				std::min(m_map_scroll + visible_map_rows, m_map_count),
				m_map_count);
			put_aligned_string(sX, sX + size_x, sY + 48, scroll_info.c_str(), GameColors::UIBlack);
		}

		// Map list rows
		int rows_to_show = std::min(visible_map_rows, m_map_count - m_map_scroll);
		int hovered = get_hovered_row(sX, sY, mouse_x, mouse_y, rows_to_show, m_map_scroll);

		for (int i = 0; i < rows_to_show; i++)
		{
			int idx = m_map_scroll + i;
			int y = sY + first_row_y + i * row_height;
			auto color = (idx == hovered) ? GameColors::UIWhite : GameColors::UIMagicBlue;
			const char* label = m_maps[idx].name;
			auto it = m_game->m_map_display_names.find(m_maps[idx].name);
			if (it != m_game->m_map_display_names.end() && !it->second.empty())
				label = it->second.c_str();
			hb::shared::text::draw_text_aligned(GameFont::Default,
				list_x, y, list_w, 15,
				label,
				hb::shared::text::TextStyle::from_color(color),
				hb::shared::text::Align::TopCenter);
		}
	}

	// [ Back ] button
	int back_x = sX + (size_x - 50) / 2;
	int back_y = sY + 300;
	bool back_hover = (mouse_x >= back_x && mouse_x <= back_x + 50 && mouse_y >= back_y && mouse_y <= back_y + 18);
	hb::shared::text::draw_text_aligned(GameFont::Default,
		back_x, back_y, 50, 15,
		"Back",
		hb::shared::text::TextStyle::from_color(back_hover ? GameColors::UIWhite : GameColors::UIMagicBlue),
		hb::shared::text::Align::TopCenter);
}

void DialogBox_TesterMenu::on_draw()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	short z = static_cast<short>(hb::shared::input::get_mouse_wheel_delta());
	short sX = m_x;
	short sY = m_y;
	short size_x = m_size_x;

	draw_new_dialog_box(InterfaceNdGame2, sX, sY, 0);

	if (m_page == 0)
		draw_main_menu(sX, sY, size_x);
	else if (m_page == 1)
		draw_level_picker(sX, sY, size_x);
	else
		draw_teleport_page(sX, sY, size_x);

	// Mouse wheel scroll for teleport page
	if (m_page == 2 && z != 0 && m_map_count > visible_map_rows)
	{
		if (z > 0)
			m_map_scroll = std::max(0, m_map_scroll - 1);
		else
			m_map_scroll = std::min(m_map_count - visible_map_rows, m_map_scroll + 1);
	}
}

bool DialogBox_TesterMenu::on_click_main_menu(short sX, short sY)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	int row = get_hovered_row(sX, sY, mouse_x, mouse_y, action_count);
	if (row >= 0)
	{
		if (row == 7) // Set level
		{
			m_selected_level = player().m_level;
			m_page = 1;
			audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
			return true;
		}
		if (row == 8) // Create item
		{
			m_game->get_dialog_box_manager().enable_dialog_box(DialogBoxId::ItemCreator, 0, 0, 0);
			audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
			return true;
		}
		if (row == 9) // Teleport
		{
			m_page = 2;
			m_map_scroll = 0;
			m_map_count = 0;
			send_game_packet(hb::net::make_common_command(CommonType::TesterMapList, player().m_player_x, player().m_player_y));
			audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
			return true;
		}

		{
			auto pkt = hb::net::make_common_command(CommonType::TesterAction, player().m_player_x, player().m_player_y);
			pkt.v1 = row;
			send_game_packet(pkt);
		}
		audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
		return true;
	}

	// Close button
	if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) &&
		(mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y))
	{
		disable_this_dialog();
		audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
		return true;
	}

	return false;
}

bool DialogBox_TesterMenu::on_click_level_picker(short sX, short sY)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	short size_x = m_size_x;

	constexpr int btn_w = 40;
	constexpr int btn_gap = 8;
	constexpr int total_w = btn_w * 4 + btn_gap * 3;
	int btn_start_x = sX + (size_x - total_w) / 2;
	int btn_y = sY + 160;

	int deltas[] = { -10, -1, 1, 10 };
	for (int i = 0; i < 4; i++)
	{
		int bx = btn_start_x + i * (btn_w + btn_gap);
		if (mouse_x >= bx && mouse_x <= bx + btn_w && mouse_y >= btn_y && mouse_y <= btn_y + 18)
		{
			m_selected_level = std::clamp(m_selected_level + deltas[i], 1, m_game->m_max_level);
			audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
			return true;
		}
	}

	int apply_x = sX + (size_x - 50) / 2;
	int apply_y = sY + 220;
	if (mouse_x >= apply_x && mouse_x <= apply_x + 50 && mouse_y >= apply_y && mouse_y <= apply_y + 18)
	{
		{
			auto pkt = hb::net::make_common_command(CommonType::TesterAction, player().m_player_x, player().m_player_y);
			pkt.v1 = 7;
			pkt.v2 = m_selected_level;
			send_game_packet(pkt);
		}
		m_page = 0;
		audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
		return true;
	}

	int back_x = sX + (size_x - 50) / 2;
	int back_y = sY + 260;
	if (mouse_x >= back_x && mouse_x <= back_x + 50 && mouse_y >= back_y && mouse_y <= back_y + 18)
	{
		m_page = 0;
		audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
		return true;
	}

	return false;
}

bool DialogBox_TesterMenu::on_click_teleport(short sX, short sY)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	short size_x = m_size_x;

	if (m_map_count > 0)
	{
		int rows_to_show = std::min(visible_map_rows, m_map_count - m_map_scroll);
		int clicked = get_hovered_row(sX, sY, mouse_x, mouse_y, rows_to_show, m_map_scroll);
		if (clicked >= 0 && clicked < m_map_count)
		{
			{
				auto pkt = hb::net::make_common_command_str(CommonType::TesterAction, player().m_player_x, player().m_player_y);
				pkt.v1 = 9;
				std::snprintf(pkt.text, sizeof(pkt.text), "%s", m_maps[clicked].name);
				send_game_packet(pkt);
			}
			m_page = 0;
			audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
			return true;
		}
	}

	int back_x = sX + (size_x - 50) / 2;
	int back_y = sY + 300;
	if (mouse_x >= back_x && mouse_x <= back_x + 50 && mouse_y >= back_y && mouse_y <= back_y + 18)
	{
		m_page = 0;
		audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
		return true;
	}

	return false;
}

bool DialogBox_TesterMenu::on_click()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	short sX = m_x;
	short sY = m_y;

	if (m_page == 0)
		return on_click_main_menu(sX, sY);
	else if (m_page == 1)
		return on_click_level_picker(sX, sY);
	else
		return on_click_teleport(sX, sY);
}

void DialogBox_TesterMenu::receive_map_list(const hb::net::PacketNotifyTesterMapListResult* pkt)
{
	m_map_count = std::clamp(static_cast<int>(pkt->count), 0, 100);
	std::memcpy(m_maps, pkt->entries, sizeof(m_maps));
	m_map_scroll = 0;
}

bool DialogBox_TesterMenu::on_enable(int type, int64_t v1, int v2, const char* string)
{
	if (is_enabled()) return true;
	int tester_x = LOGICAL_WIDTH() - 258 - 10;
	int tester_y = LOGICAL_HEIGHT() - 339 - ICON_PANEL_HEIGHT() - 10;
	if (m_game->get_dialog_box_manager().is_enabled(DialogBoxId::LevelUpSetting))
		tester_y -= 30;
	m_x = tester_x;
	m_y = tester_y;
	return true;
}
#endif // TESTER_ONLY
