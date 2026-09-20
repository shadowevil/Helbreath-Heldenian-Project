// Screen_Splash.cpp: Splash Screen Implementation
//
// Displays a splash screen before loading begins.
// Shows contributors one at a time with fade effects over 12 seconds.
//
//////////////////////////////////////////////////////////////////////

#include "Screen_Splash.h"
#include "Screen_Loading.h"
#include "Game.h"
#include "GameModeManager.h"
#include "CommonTypes.h"
#include "GameFonts.h"
#include "TextLibExt.h"
#include "SpriteLoader.h"
using namespace hb::client::sprite_id;

Screen_Splash::Screen_Splash(CGame* game)
    : IGameScreen(game)
{
}

void Screen_Splash::on_initialize()
{
    hb::shared::sprite::SpriteLoader::open_pak("interface/intermediate_screens", [&](hb::shared::sprite::SpriteLoader& loader) {
        m_game->m_sprite[SplashScreen] = loader.get_sprite(3, false);
        });

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
    m_game->m_sprite.remove(SplashScreen);
}

void Screen_Splash::on_update()
{
    if (get_elapsed_ms() >= SPLASH_DURATION_MS)
    {
        set_screen<Screen_Loading>();
        return;
    }

    // Update credit opacity based on elapsed time
    uint32_t elapsed = get_elapsed_ms();
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
    m_game->m_sprite[SplashScreen]->draw(0, 0, 0);
    m_controls.render();
    draw_version();
}
