// screen_quit.cpp: Quit Screen Implementation
//
//////////////////////////////////////////////////////////////////////

#include "screen_quit.h"
#include "game.h"
#include "game_mode_manager.h"
#include "common_types.h"
#include "input.h"
#include "renderer.h"
#include "sprite_factory.h"
#include "text_renderer.h"
#include "log.h"
#include <algorithm>
#include <cmath>
#include <string>


namespace MouseButton = hb::shared::input::MouseButton;

namespace
{

// The sheet (HH-0103): the 1920x1080 backdrop maps as art_x = 240 + ui_x * 1.8, art_y = ui_y * 1.8. The message
// panel is 802x265 art px, centred where the original 356x102 panel sat, UI (335,183): its middle (513,234).
constexpr float art_width = 1920.0f;
constexpr float art_height = 1080.0f;
constexpr float panel_w = 802.0f;
constexpr float panel_h = 265.0f;
constexpr float panel_art_x = 240.0f + 513.0f * 1.8f - panel_w / 2.0f;
constexpr float panel_art_y = 234.0f * 1.8f - panel_h / 2.0f;

// The message: gold Liberation Serif Bold (the closest shipped face to the original's wide bold serif), with the
// classic drop shadow (a black copy 1 original px right and down = 2 art px). Sizes in art px; each baseline is
// an offset from the panel's middle, the original's lines scaled by the panel's 1.25x.
constexpr const char* face_path = "fonts/liberation_serif_bold.ttf";
constexpr hb::shared::render::Color gold{246, 190, 40};
constexpr float font_art_px = 30.0f;
constexpr float tracking_art_px = 1.2f;
constexpr float shadow_art_px = 2.0f;
constexpr int pad_px = 2;

struct line_spec
{
	const char* text;
	float baseline;   // art px below the panel's middle
};
constexpr std::array<line_spec, 4> message{{
	{"Thank you for playing Helbreath.", -54.0f},
	{"Please visit our homepage board for", -20.0f},
	{"bug report or question about game.", 14.0f},
	{"www.helbreathusa.com", 83.0f},
}};

}

void exit_backdrop::load(CGame* game)
{
	const std::string path = hb::shared::sprite::Sprites::get_sprite_path() + "/interface/front_end/exit_screen.png";
	m_art.reset(game->m_Renderer->create_texture_from_file(path.c_str()));
	if (!m_art)
		hb::logger::error("Exit screen art not loaded: {}", path);
}

void exit_backdrop::release()
{
	m_art.reset();
}

hb::shared::render::art_fit exit_backdrop::draw(CGame* game)
{
	hb::shared::render::IRenderer* renderer = game->m_Renderer;
	const hb::shared::render::art_fit fit = hb::shared::render::cover_fit(renderer->get_screen_extent(), art_width, art_height);
	if (m_art)
	{
		renderer->draw_texture_region(m_art.get(), 0.0f, 0.0f, art_width, art_height,
			fit.origin_x, fit.origin_y, art_width * fit.scale, art_height * fit.scale);
	}
	return fit;
}

Screen_Quit::Screen_Quit(CGame* game)
    : IGameScreen(game)
{
}

void Screen_Quit::on_initialize()
{
    m_dwStartTime = GameClock::get_time_ms();

	m_backdrop.load(m_game);
	const std::string panel_path = hb::shared::sprite::Sprites::get_sprite_path() + "/interface/front_end/exit_panel.png";
	m_panel.reset(m_game->m_Renderer->create_texture_from_file(panel_path.c_str()));
	if (!m_panel)
		hb::logger::error("Exit panel art not loaded: {}", panel_path);
	update_layout();

    // Close game socket
    if (m_game->m_g_sock != nullptr)
    {
        m_game->m_g_sock.reset();
    }
}

void Screen_Quit::on_uninitialize()
{
	m_backdrop.release();
	m_panel.reset();
	for (message_line& line : m_lines)
		line.text.reset();
	m_baked_scale = 0.0f;
}

void Screen_Quit::update_layout()
{
	m_fit = hb::shared::render::cover_fit(m_game->m_Renderer->get_screen_extent(), art_width, art_height);
	if (m_debounce.update(m_baked_scale, m_fit.scale * m_game->m_Renderer->ui_scale()))
		bake_message();
}

void Screen_Quit::bake_message()
{
	// At the window's resolution: art px * (UI units per art px) * (window px per UI unit)
	hb::shared::text::ITextRenderer* text = hb::shared::text::GetTextRenderer();
	const float ui_scale = m_game->m_Renderer->ui_scale();
	const float window_per_art = m_fit.scale * ui_scale;
	const int face = text ? text->load_face(face_path) : -1;
	if (face <= 0)
	{
		hb::logger::error("Exit message not rendered ({})", face_path);
		return;
	}
	const hb::shared::text::text_run run{face, static_cast<int>(std::lround(font_art_px * window_per_art)),
		tracking_art_px * window_per_art};
	for (std::size_t i = 0; i < message.size(); i++)
	{
		const hb::shared::text::ink_box ink = text->measure_run(message[i].text, run);
		message_line& line = m_lines[i];
		line.text.reset(text->bake_run(message[i].text, run, hb::shared::render::Color::White(), pad_px));
		// The origin's baseline is run.pixel_size below it; the texture's ink starts pad in
		line.x = -(ink.right - ink.left) / 2.0f - static_cast<float>(pad_px);
		line.y = ink.top - static_cast<float>(run.pixel_size) - static_cast<float>(pad_px);
		if (!line.text)
			hb::logger::error("Exit message line {} not rendered", i);
	}
	// Only now do the textures and the scale they draw at agree
	m_baked_ui_scale = ui_scale;
	m_baked_scale = window_per_art;
}

void Screen_Quit::on_update()
{
	update_layout();
    uint32_t elapsed = GameClock::get_time_ms() - m_dwStartTime;

    // After 3 seconds, allow input to skip
    if (elapsed >= INPUT_ACTIVE_MS)
    {
        // Handle escape/enter to quit
        if (hb::shared::input::is_key_pressed(KeyCode::Escape) || hb::shared::input::is_key_pressed(KeyCode::Enter))
        {
            m_game->change_game_mode(GameMode::Null);
            hb::shared::render::Window::close();
            return;
        }

        // Check for mouse click
        if (hb::shared::input::is_mouse_button_pressed(MouseButton::Left))
        {
            m_game->change_game_mode(GameMode::Null);
            hb::shared::render::Window::close();
            return;
        }
    }

    // Auto-quit after 10 seconds
    if (elapsed >= AUTO_QUIT_MS)
    {
        m_game->change_game_mode(GameMode::Null);
        hb::shared::render::Window::close();
        return;
    }
}

void Screen_Quit::on_render()
{
	// m_fit is from this frame's on_update (or on_initialize on the frame the screen is switched in)
	m_backdrop.draw(m_game);

	// The panel and its message fade in over 500ms
	hb::shared::render::IRenderer* renderer = m_game->m_Renderer;
	const uint32_t elapsed = GameClock::get_time_ms() - m_dwStartTime;
	const float alpha = (std::min)(1.0f, static_cast<float>(elapsed) / static_cast<float>(FADE_IN_MS));
	if (m_panel)
	{
		renderer->draw_texture_region(m_panel.get(), 0.0f, 0.0f, panel_w, panel_h,
			m_fit.x(panel_art_x), m_fit.y(panel_art_y), panel_w * m_fit.scale, panel_h * m_fit.scale, alpha);
	}

	// Each line centred on the panel's middle; baked textures draw at the scale they were baked for
	const float ui_scale = renderer->ui_scale();
	const float per_px = 1.0f / m_baked_ui_scale;   // UI units per baked texel
	const float centre_x = m_fit.x(panel_art_x + panel_w / 2.0f);
	const float shadow = shadow_art_px * m_fit.scale;
	const auto snap = [ui_scale](float v) { return std::round(v * ui_scale) / ui_scale; };
	for (std::size_t i = 0; i < message.size(); i++)
	{
		hb::shared::render::ITexture* texture = m_lines[i].text.get();
		if (!texture)
			continue;
		const float w = static_cast<float>(texture->get_width());
		const float h = static_cast<float>(texture->get_height());
		const float x = snap(centre_x + m_lines[i].x * per_px);
		const float y = snap(m_fit.y(panel_art_y + panel_h / 2.0f + message[i].baseline) + m_lines[i].y * per_px);
		renderer->draw_texture_region(texture, 0.0f, 0.0f, w, h, x + shadow, y + shadow, w * per_px, h * per_px, alpha,
			hb::shared::render::draw_tint{hb::shared::render::Color::Black()});
		renderer->draw_texture_region(texture, 0.0f, 0.0f, w, h, x, y, w * per_px, h * per_px, alpha,
			hb::shared::render::draw_tint{gold});
	}

    draw_version();
    draw_copyright();
}
