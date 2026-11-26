// game_screen.cpp: Implementation of IGameScreen helper methods
//
//////////////////////////////////////////////////////////////////////

#include "game_screen.h"
#include "dialog_box.h"   // draw_kit_notice_frame, kit_layout
#include "event_list_manager.h"
#include "game.h"
#include "game_mode_manager.h"
#include "game_fonts.h"
#include "text_lib_ext.h"
#include "global_def.h"
#include "ccontrols.h"
#include "input_state_helper.h"

IGameScreen::IGameScreen(CGame* game)
    : m_game(game)
{
}

// ============== Drawing Helpers ==============

void IGameScreen::draw_new_dialog_box(char type, int sX, int sY, int frame,
                                    bool is_no_color_key, bool is_trans)
{
    m_game->draw_new_dialog_box(type, sX, sY, frame, is_no_color_key, is_trans);
}

void IGameScreen::draw_text_button(int sX, int sY, int frame, bool is_trans)
{
    m_game->draw_text_button(sX, sY, frame, is_trans);
}

void IGameScreen::get_centered_pos(int width, int height, int& out_x, int& out_y)
{
    out_x = (LOGICAL_WIDTH() - width) / 2;
    out_y = (LOGICAL_HEIGHT() - height) / 2;
}

void IGameScreen::get_centered_notice_pos(int& out_x, int& out_y)
{
    get_centered_pos(kit_layout::notice_size.x, kit_layout::notice_size.y, out_x, out_y);
}

void IGameScreen::put_string(int iX, int iY, const char* string, const hb::shared::render::Color& color)
{
    hb::shared::text::draw_text(GameFont::Default, iX, iY, string, hb::shared::text::TextStyle::from_color(color));
}

void IGameScreen::put_aligned_string(int x1, int x2, int iY, const char* string,
                                    const hb::shared::render::Color& color)
{
    hb::shared::text::draw_text_aligned(GameFont::Default, x1, iY, x2 - x1, 15, string,
                             hb::shared::text::TextStyle::from_color(color), hb::shared::text::Align::TopCenter);
}

void IGameScreen::put_string_spr_font(int iX, int iY, const char* str, uint8_t r, uint8_t g, uint8_t b)
{
    hb::shared::text::draw_text(GameFont::Bitmap1, iX, iY, str, hb::shared::text::TextStyle::with_highlight(hb::shared::render::Color(r, g, b)));
}

void IGameScreen::draw_version()
{
    m_game->draw_version();
}

void IGameScreen::draw_copyright(bool show_company)
{
    // The lines are baked sprites (the copyright sign needs UTF-8, which the text path does not read); CGame
    // owns them
    m_game->get_copyright_notice().draw(*m_game->m_Renderer, show_company);
}


// ============== Event/Message Helpers ==============

void IGameScreen::add_event_list(const char* txt, char color, bool dup_allow)
{
    event_list_manager::get().add_event(txt, color, dup_allow);
}

// ============== CControls Input Helpers ==============

void IGameScreen::update_controls(cc::control_collection& controls)
{
	cc::input_state input;
	hb::client::fill_input_state(input);
	controls.update(input, GameClock::get_time_ms());
}

void IGameScreen::discard_pending_controls_input(cc::control_collection& controls)
{
	cc::input_state input;
	hb::client::fill_input_state(input);
	controls.discard_pending_input(input);
}

// ============== Drawing Helpers (Centered) ==============

void IGameScreen::draw_centered_notice_frame(int& out_x, int& out_y, std::string_view title, int title_y)
{
	get_centered_notice_pos(out_x, out_y);
	draw_kit_notice_frame(m_game->get_panel_kit(), out_x, out_y, title, title_y);
}

// ============== GameMode ==============

GameMode IGameScreen::get_game_mode() const
{
	return GameMode::Null;
}

// ============== Per-Screen Transition Timing ==============

float IGameScreen::get_fade_in_duration() const
{
	return GameModeManager::DEFAULT_FADE_DURATION;
}

float IGameScreen::get_fade_out_duration() const
{
	return GameModeManager::DEFAULT_FADE_DURATION;
}

// ============== Timing Helper ==============

uint32_t IGameScreen::get_elapsed_ms() const
{
    return GameClock::get_time_ms() - GameModeManager::get_mode_start_time();
}
