#include "dialog_box_status_overlay.h"
#include "game.h"
#include "global_def.h"
#include "game_fonts.h"
#include "text_lib_ext.h"
#include "input.h"
#include "lan_eng.h"
#include "audio_manager.h"
#include <algorithm>
#include <format>
#include <string>

DialogBox_StatusOverlay::DialogBox_StatusOverlay(CGame* game)
	: IDialogBox(DialogBoxId::StatusOverlay, game)
{
	m_can_close_on_right_click = false;
	set_default_rect(0, 0, 0, 0);
}

const char* DialogBox_StatusOverlay::get_primary_text() const
{
	if (m_show_levelup) return "Level Up!";
	if (m_show_restart) return "Restart";
	return nullptr;
}

void DialogBox_StatusOverlay::on_update()
{
	m_show_levelup = (m_game->m_player->m_hp > 0) &&
		(m_game->m_player->m_lu_point > 0) &&
		!m_game->get_dialog_box_manager().is_enabled(DialogBoxId::LevelUpSetting);

	m_show_restart = !m_show_levelup &&
		(m_game->m_player->m_hp <= 0) &&
		(m_game->m_restart_count == -1);

	bool has_primary = m_show_levelup || m_show_restart;

#ifdef TESTER_ONLY
	m_show_tester = !m_game->get_dialog_box_manager().is_enabled(DialogBoxId::TesterMenu);
	bool has_tester = m_show_tester;
#else
	bool has_tester = false;
#endif

	if (!has_primary && !has_tester)
	{
		m_size_x = 0;
		m_size_y = 0;
		return;
	}

	// Measure text
	int primary_w = 0, primary_h = 0;
	if (has_primary)
	{
		auto m = hb::shared::text::measure_text(GameFont::Bitmap1, get_primary_text());
		primary_w = m.width;
		primary_h = m.height;
	}

#ifdef TESTER_ONLY
	int tester_w = 0, tester_h = 0;
	if (m_show_tester)
	{
		auto m = hb::shared::text::measure_text(GameFont::Bitmap1, "Tester");
		tester_w = m.width;
		tester_h = m.height;
	}
#endif

	// Compute dialog content size and button rects (relative to dialog origin)
	int content_w = 0;
	int content_h = 0;

#ifdef TESTER_ONLY
	if (has_primary && m_show_tester)
	{
		content_w = std::max(primary_w, tester_w);
		content_h = tester_h + gap + primary_h;
		int dlg_w = content_w + padding * 2;
		m_tester_btn = { (dlg_w - tester_w) / 2, padding, tester_w, tester_h };
		m_primary_btn = { (dlg_w - primary_w) / 2, padding + tester_h + gap, primary_w, primary_h };
	}
	else if (has_primary)
	{
		content_w = primary_w;
		content_h = primary_h;
		int dlg_w = content_w + padding * 2;
		m_primary_btn = { (dlg_w - primary_w) / 2, padding, primary_w, primary_h };
	}
	else
	{
		content_w = tester_w;
		content_h = tester_h;
		int dlg_w = content_w + padding * 2;
		m_tester_btn = { (dlg_w - tester_w) / 2, padding, tester_w, tester_h };
	}
#else
	content_w = primary_w;
	content_h = primary_h;
	{
		int dlg_w = content_w + padding * 2;
		m_primary_btn = { (dlg_w - primary_w) / 2, padding, primary_w, primary_h };
	}
#endif

	m_size_x = static_cast<short>(content_w + padding * 2);
	m_size_y = static_cast<short>(content_h + padding * 2);

	int hud_top = LOGICAL_HEIGHT() - ICON_PANEL_HEIGHT();
	m_x = static_cast<short>(LOGICAL_WIDTH() - m_size_x - margin_right);
	m_y = static_cast<short>(hud_top - above_hud - m_size_y);
}

void DialogBox_StatusOverlay::on_draw()
{
	bool has_primary = m_show_levelup || m_show_restart;

#ifdef TESTER_ONLY
	if (!has_primary && !m_show_tester) return;
#else
	if (!has_primary) return;
#endif

	int flash = (GameClock::get_time_ms() / 3) % 255;
	auto style = hb::shared::text::TextStyle::with_integrated_shadow(
		hb::shared::render::Color(flash, flash, 0));

	if (has_primary)
	{
		hb::shared::text::draw_text_aligned(GameFont::Bitmap1,
			m_x + m_primary_btn.x, m_y + m_primary_btn.y,
			m_primary_btn.w, m_primary_btn.h,
			get_primary_text(), style, hb::shared::text::Align::Center);
	}

#ifdef TESTER_ONLY
	if (m_show_tester)
	{
		hb::shared::text::draw_text_aligned(GameFont::Bitmap1,
			m_x + m_tester_btn.x, m_y + m_tester_btn.y,
			m_tester_btn.w, m_tester_btn.h,
			"Tester", style, hb::shared::text::Align::Center);
	}
#endif
}

bool DialogBox_StatusOverlay::on_click()
{
	bool has_primary = m_show_levelup || m_show_restart;

#ifdef TESTER_ONLY
	if (m_show_tester && mouse_in(m_tester_btn))
	{
		enable_dialog_box(DialogBoxId::TesterMenu, 0, 0, 0);
		audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
		return true;
	}
#endif

	if (has_primary && mouse_in(m_primary_btn))
	{
		if (m_show_levelup)
		{
			enable_dialog_box(DialogBoxId::LevelUpSetting, 0, 0, 0);
			audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
		}
		else if (m_show_restart)
		{
			m_game->m_restart_count = 5;
			m_game->m_restart_count_time = GameClock::get_time_ms();
			std::string txt = std::format(DLGBOX_CLICK_SYSMENU1, m_game->m_restart_count);
			add_event_list(txt.c_str(), 10);
			audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
		}
		return true;
	}

	return true;
}
