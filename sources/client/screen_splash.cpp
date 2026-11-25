// screen_splash.cpp: Splash Screen Implementation
//
// Displays a splash screen before loading begins.
// Shows contributors one at a time with fade effects over 12 seconds.
//
//////////////////////////////////////////////////////////////////////

#include "screen_splash.h"
#include "screen_loading.h"
#include "game.h"
#include "game_mode_manager.h"
#include "common_types.h"
#include "game_fonts.h"
#include "text_lib_ext.h"
#include "input.h"
#include "renderer.h"
#include "sprite_factory.h"
#include "log.h"
#include <string>

namespace
{

// interface/front_end/splash_logo.png (HH-0107): the logo cut from the 1920x1080 splash art, its alpha box
// (679,365)-(1241,716) plus 16 px of padding, the colour of the transparent edge bled outward so smooth
// filtering leaves no white halo. Drawn over black at its place in the art, through the cover fit.
constexpr float art_width = 1920.0f;
constexpr float art_height = 1080.0f;
constexpr float logo_art_x = 663.0f;
constexpr float logo_art_y = 349.0f;

}

Screen_Splash::Screen_Splash(CGame* game)
    : IGameScreen(game)
{
}

void Screen_Splash::on_initialize()
{
	const std::string logo_path = hb::shared::sprite::Sprites::get_sprite_path() + "/interface/front_end/splash_logo.png";
	m_logo.reset(m_game->m_Renderer->create_texture_from_file(logo_path.c_str()));
	if (!m_logo)
		hb::logger::error("Splash logo not loaded: {}", logo_path);

    m_credits = {{
        { "Centuu - HelbreathServer starting base", "https://github.com/centuu/HelbreathServer" },
        { "Pilgrim - Reasoning and Code Contributor", {} },
        { "Casal0x - Code Contributor", {} },
        { "Giaco - Secondary Code Contributor", {} },
        { "ShadowEvil - Primary Code Contributor", {} }
    }};

    // CControls setup — alignment for credit positioning
    constexpr int line_height = 16;
    m_controls.set_screen_size(LOGICAL_WIDTH(), LOGICAL_HEIGHT());

    auto credit_renderer = [](const cc::control& c, const hb::shared::render::Color& base_color) {
        auto& lbl = static_cast<const cc::label&>(c);
        float alpha = lbl.opacity();
        if (alpha <= 0.0f) return;

        auto style = hb::shared::text::TextStyle::with_drop_shadow(hb::shared::render::Color(
            static_cast<uint8_t>(base_color.r * alpha),
            static_cast<uint8_t>(base_color.g * alpha),
            static_cast<uint8_t>(base_color.b * alpha)));

        auto sb = c.screen_bounds();
        hb::shared::text::draw_text_aligned(GameFont::Default, sb.x, sb.y, sb.w, sb.h,
            lbl.text().c_str(), style, hb::shared::text::Align::TopCenter);
    };

    for (int i = 0; i < NUM_CONTRIBUTORS; i++)
    {
        // Credit name label — centered, 36px from bottom
        auto* name_lbl = m_controls.add<cc::label>(LBL_CREDIT_NAME_0 + i,
            cc::rect{0, 0, LOGICAL_WIDTH(), line_height}, m_credits[i].displayLine.c_str());
        name_lbl->set_horizontal_alignment(cc::align::horizontal::center);
        name_lbl->set_vertical_alignment(cc::align::vertical::bottom, 36);
        name_lbl->set_opacity(0.0f);
        name_lbl->set_render_handler([credit_renderer](const cc::control& c) {
            credit_renderer(c, GameColors::UIWhite);
        });

        // Credit URL label — centered, 20px from bottom
        if (!m_credits[i].url.empty())
        {
            auto* url_lbl = m_controls.add<cc::label>(LBL_CREDIT_URL_0 + i,
                cc::rect{0, 0, LOGICAL_WIDTH(), line_height}, m_credits[i].url.c_str());
            url_lbl->set_horizontal_alignment(cc::align::horizontal::center);
            url_lbl->set_vertical_alignment(cc::align::vertical::bottom, 20);
            url_lbl->set_opacity(0.0f);
            url_lbl->set_render_handler([credit_renderer](const cc::control& c) {
                credit_renderer(c, GameColors::UIFactionChat);
            });
        }
    }
}

void Screen_Splash::on_uninitialize()
{
	m_logo.reset();
}

void Screen_Splash::on_update()
{
    const uint32_t elapsed = get_elapsed_ms();
    const bool skip_clicked = elapsed >= skip_allowed_after_ms
        && hb::shared::input::is_mouse_button_pressed(hb::shared::input::MouseButton::Left);
    if (elapsed >= SPLASH_DURATION_MS || skip_clicked)
    {
        set_screen<Screen_Loading>();
        return;
    }

    // Update credit opacity based on elapsed time
    for (int i = 0; i < NUM_CONTRIBUTORS; i++)
    {
        float alpha = get_contributor_alpha(elapsed, i);
        if (auto* name_lbl = m_controls.find(LBL_CREDIT_NAME_0 + i))
            name_lbl->set_opacity(alpha);
        if (auto* url_lbl = m_controls.find(LBL_CREDIT_URL_0 + i))
            url_lbl->set_opacity(alpha);
    }
}

float Screen_Splash::get_contributor_alpha(uint32_t elapsedMs, int contributorIndex) const
{
    uint32_t startTime = contributorIndex * TIME_PER_CONTRIBUTOR_MS;
    uint32_t endTime = startTime + TIME_PER_CONTRIBUTOR_MS;
    bool last_contributor = (contributorIndex == NUM_CONTRIBUTORS - 1);

    // Before this contributor's time
    if (elapsedMs < startTime)
        return 0.0f;

    // After this contributor's time (except last one stays visible)
    if (elapsedMs >= endTime && !last_contributor)
        return 0.0f;

    uint32_t timeInSlot = elapsedMs - startTime;

    // Fade in during first FADE_DURATION_MS
    if (timeInSlot < FADE_DURATION_MS)
        return static_cast<float>(timeInSlot) / FADE_DURATION_MS;

    // Last contributor doesn't fade out
    if (last_contributor)
        return 1.0f;

    // Fade out during last FADE_DURATION_MS
    uint32_t timeUntilEnd = endTime - elapsedMs;
    if (timeUntilEnd < FADE_DURATION_MS)
        return static_cast<float>(timeUntilEnd) / FADE_DURATION_MS;

    // Fully visible in the middle
    return 1.0f;
}

void Screen_Splash::on_render()
{
	if (m_logo)
	{
		hb::shared::render::IRenderer* renderer = m_game->m_Renderer;
		const hb::shared::render::art_fit fit = hb::shared::render::cover_fit(renderer->get_screen_extent(), art_width, art_height);
		const float w = static_cast<float>(m_logo->get_width());
		const float h = static_cast<float>(m_logo->get_height());
		renderer->draw_texture_region(m_logo.get(), 0.0f, 0.0f, w, h,
			fit.x(logo_art_x), fit.y(logo_art_y), w * fit.scale, h * fit.scale);
	}
    m_controls.render();
}
