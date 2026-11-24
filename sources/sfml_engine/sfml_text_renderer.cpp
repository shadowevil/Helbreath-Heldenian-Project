// sfml_text_renderer.cpp: SFML implementation of ITextRenderer
//
// Part of SFMLEngine static library
//////////////////////////////////////////////////////////////////////

#include "sfml_text_renderer.h"
#include "sfml_renderer.h"
#include <SFML/Graphics/Text.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <utility>

namespace hb::shared::text {

// Global accessor implementation
static ITextRenderer* s_pTextRenderer = nullptr;

ITextRenderer* GetTextRenderer()
{
    return s_pTextRenderer;
}

void SetTextRenderer(ITextRenderer* renderer)
{
    s_pTextRenderer = renderer;
}

SFMLTextRenderer::SFMLTextRenderer(SFMLRenderer* renderer)
    : m_renderer(renderer)
    , m_fontLoaded(false)
    , m_font_smoothing(false)
    , m_loaded_font_path()
    , m_font_size(12)  // Default size to match GDI rendering
{
    LoadDefaultFont();
}

bool SFMLTextRenderer::LoadDefaultFont()
{
    return LoadFontFromFile(bundled_font_path);
}

bool SFMLTextRenderer::LoadFontFromFile(const char* fontPath)
{
    if (!fontPath)
        return false;

    if (m_font.openFromFile(fontPath))
    {
        m_font.setSmooth(m_font_smoothing);
        m_fontLoaded = true;
        m_loaded_font_path = fontPath;
        return true;
    }

    return false;
}

void SFMLTextRenderer::SetFontSize(int size)
{
    // SFML/FreeType renders ~2 points larger than GDI, so compensate
    int adjustedSize = (size > 2) ? (size - 2) : size;
    m_font_size = static_cast<unsigned int>(adjustedSize);
}

void SFMLTextRenderer::set_font_smoothing(bool enabled)
{
    if (m_font_smoothing == enabled)
    {
        return;
    }

    m_font_smoothing = enabled;

    // Force a full font reload so all active TTF glyph pages are rebuilt with the new smoothing mode.
    // sf::Font caches glyph pages internally per character size; toggling smooth alone may not
    // retroactively recreate already-generated pages in all SFML versions.
    if (!m_loaded_font_path.empty())
    {
        sf::Font reloaded;
        if (reloaded.openFromFile(m_loaded_font_path))
        {
            reloaded.setSmooth(m_font_smoothing);
            m_font = std::move(reloaded);
            m_fontLoaded = true;
            return;
        }
    }

    // Fallback: preserve behavior even if reload path is unavailable.
    m_font.setSmooth(enabled);
}

bool SFMLTextRenderer::is_font_smoothing() const
{
    return m_font_smoothing;
}

unsigned int SFMLTextRenderer::resolve_font_size(int fontSize) const
{
    if (fontSize <= 0)
        return m_font_size;  // Use default

    // Apply GDI-2 adjustment for the requested size.
    // sf::Font lazily caches glyph pages per character size internally,
    // so requesting different sizes has no reload cost.
    return static_cast<unsigned int>((fontSize > 2) ? (fontSize - 2) : fontSize);
}

bool SFMLTextRenderer::IsFontLoaded() const
{
    return m_fontLoaded;
}

TextMetrics SFMLTextRenderer::measure_text(const char* text, int fontSize) const
{
    TextMetrics metrics = {0, 0};

    if (!text || !m_fontLoaded)
        return metrics;

    sf::Text sfText(m_font, text, resolve_font_size(fontSize));
    sf::FloatRect bounds = sfText.getLocalBounds();

    metrics.width = static_cast<int>(bounds.size.x);
    metrics.height = static_cast<int>(bounds.size.y);

    return metrics;
}

int SFMLTextRenderer::get_fitting_char_count(const char* text, int maxWidth, int fontSize) const
{
    if (!text || !m_fontLoaded)
        return 0;

    unsigned int resolvedSize = resolve_font_size(fontSize);
    int len = static_cast<int>(strlen(text));
    sf::Text sfText(m_font, "", resolvedSize);

    for (int i = len; i > 0; i--)
    {
        std::string substr(text, i);
        sfText.setString(substr);
        sf::FloatRect bounds = sfText.getLocalBounds();

        if (bounds.size.x <= static_cast<float>(maxWidth))
            return i;
    }

    return 0;
}

int SFMLTextRenderer::get_line_height(int fontSize) const
{
    if (!m_fontLoaded)
        return 0;

    return static_cast<int>(m_font.getLineSpacing(resolve_font_size(fontSize)));
}

void SFMLTextRenderer::draw_text(int x, int y, const char* text, const hb::shared::render::Color& color,
                                int fontSize, bool bold)
{
    draw_text_offset(x, y, 0, 0, text, color, fontSize, bold);
}

void SFMLTextRenderer::draw_text_offset(int x, int y, int dx, int dy, const char* text,
                                       const hb::shared::render::Color& color, int fontSize, bool bold)
{
    sf::RenderTarget* target = m_renderer ? m_renderer->get_current_target() : nullptr;
    if (!text || !m_fontLoaded || !target)
        return;

    unsigned int logical_size = resolve_font_size(fontSize);
    sf::Text sfText(m_font, text, logical_size);
    sfText.setFillColor(sf::Color(color.r, color.g, color.b));
    if (bold)
        sfText.setStyle(sf::Text::Bold);

    draw_at_output_resolution(*target, sfText, logical_size, static_cast<float>(x), static_cast<float>(y),
                              sf::RenderStates::Default, output_shift(dx, dy));
}

sf::Vector2f SFMLTextRenderer::output_shift(int dx, int dy) const
{
    // At 1:1 (and in the world pass) text is placed in logical px, so the shift is too
    const sf::Vector2f shift{static_cast<float>(dx), static_cast<float>(dy)};
    if (std::abs(m_renderer->text_scale() - 1.0f) < 0.001f)
        return shift;
    // Per axis, as snap_to_output_pixel: a stretched UI has different x and y scales
    const sf::Vector2f scale = m_renderer->output_pixels_per_unit();
    if (scale.x <= 0.0f || scale.y <= 0.0f)
        return shift;
    return {std::round(shift.x * scale.x) / scale.x, std::round(shift.y * scale.y) / scale.y};
}

void SFMLTextRenderer::draw_at_output_resolution(sf::RenderTarget& target, sf::Text& text, unsigned int logical_size,
                                                 float x, float y, const sf::RenderStates& states,
                                                 sf::Vector2f subpixel) const
{
    // 1 in the world pass (world text stays at logical size until Phase 3) and at 800x600
    float scale = m_renderer->text_scale();
    if (std::abs(scale - 1.0f) < 0.001f)
    {
        text.setPosition(sf::Vector2f{std::round(x), std::round(y)} + subpixel);
        target.draw(text, states);
        return;
    }

    // Rasterise at window resolution: s = ui scale, character size = floor(logical size * s),
    // drawn at 1/s so the UI view maps it back to logical size. Layout and measurement stay logical.
    // floor, not round: hinted text then tends narrower than measured, so it never overhangs.
    long pixel_size = static_cast<long>(std::floor(static_cast<float>(logical_size) * scale + 0.001f));
    text.setCharacterSize(static_cast<unsigned int>((std::max)(pixel_size, 1L)));
    text.setScale({1.0f / scale, 1.0f / scale});

    // Snap the origin once to a whole window pixel; sf::Text keeps every glyph of a line on one shared
    // baseline at an integer offset from it, so glyph y is never snapped on its own
    text.setPosition(m_renderer->snap_to_output_pixel({x, y}) + subpixel);
    target.draw(text, states);
}

int SFMLTextRenderer::load_face(const char* path, bool smooth)
{
    if (!path)
        return -1;
    for (std::size_t i = 0; i < m_faces.size(); i++)
    {
        if (m_faces[i].path == path && m_faces[i].smooth == smooth)
            return static_cast<int>(i) + 1;
    }
    auto font = std::make_unique<sf::Font>();
    if (!font->openFromFile(path))
        return -1;
    // Per face, whatever the Antialias Font setting (which stays with the bundled font): a recipe's
    // +0.5 faux-bold copy only blends with a smooth glyph texture
    font->setSmooth(smooth);
    m_faces.push_back({path, smooth, std::move(font)});
    return static_cast<int>(m_faces.size());
}

const sf::Font* SFMLTextRenderer::face_font(int face_id) const
{
    if (face_id == 0)
        return m_fontLoaded ? &m_font : nullptr;
    if (face_id < 1 || face_id > static_cast<int>(m_faces.size()))
        return nullptr;
    return m_faces[static_cast<std::size_t>(face_id - 1)].font.get();
}

sf::Text SFMLTextRenderer::make_run(const sf::Font& font, const char* text, const text_run& run) const
{
    const unsigned int size = static_cast<unsigned int>((std::max)(run.pixel_size, 1));
    sf::Text sf_text(font, text, size);
    // SFML spaces letters by (space advance / 3) * (factor - 1) px; solve for the tracking. The factor is
    // relative to the space advance, so it holds when the run is rasterised at another size.
    const float space_advance = font.getGlyph(U' ', size, false).advance;
    if (run.tracking != 0.0f && space_advance > 0.0f)
        sf_text.setLetterSpacing(1.0f + 3.0f * run.tracking / space_advance);
    return sf_text;
}

ink_box SFMLTextRenderer::measure_run(const char* text, const text_run& run) const
{
    const sf::Font* font = face_font(run.face);
    if (!text || !font)
        return {};
    const sf::FloatRect bounds = make_run(*font, text, run).getLocalBounds();
    return {bounds.position.x, bounds.position.y, bounds.position.x + bounds.size.x, bounds.position.y + bounds.size.y};
}

void SFMLTextRenderer::draw_run(float x, float y, const char* text, const text_run& run,
                                const hb::shared::render::Color& color, hb::shared::render::BlendMode blend)
{
    sf::RenderTarget* target = m_renderer ? m_renderer->get_current_target() : nullptr;
    const sf::Font* font = face_font(run.face);
    if (!text || !font || !target)
        return;

    sf::Text sf_text = make_run(*font, text, run);
    sf_text.setFillColor(sf::Color(color.r, color.g, color.b, color.a));
    const sf::RenderStates states(blend == hb::shared::render::BlendMode::Additive ? sf::BlendAdd : sf::BlendAlpha);
    // The whole-pixel part is snapped; the fractional part stays, so a faux-bold copy at +0.5 falls
    // between pixels and blends (the face's glyph texture is smooth), as in text_fx.py
    const float base_x = std::floor(x);
    const float base_y = std::floor(y);
    draw_at_output_resolution(*target, sf_text, sf_text.getCharacterSize(), base_x, base_y, states,
                              {x - base_x, y - base_y});
}

void SFMLTextRenderer::draw_text_aligned(int x, int y, int width, int height, const char* text, const hb::shared::render::Color& color,
                                        Align alignment, int fontSize, bool bold)
{
    draw_text_aligned_offset(x, y, width, height, 0, 0, text, color, alignment, fontSize, bold);
}

void SFMLTextRenderer::draw_text_aligned_offset(int x, int y, int width, int height, int dx, int dy, const char* text,
                                               const hb::shared::render::Color& color, Align alignment, int fontSize,
                                               bool bold)
{
    sf::RenderTarget* target = m_renderer ? m_renderer->get_current_target() : nullptr;
    if (!text || !m_fontLoaded || !target)
        return;

    // Placement uses the logical-size metrics, so aligned text lands exactly where it did at 800x600
    unsigned int logical_size = resolve_font_size(fontSize);
    sf::Text sfText(m_font, text, logical_size);
    if (bold)
        sfText.setStyle(sf::Text::Bold);
    sf::FloatRect bounds = sfText.getLocalBounds();

    // Extract alignment components
    uint8_t hAlign = alignment & Align::HMask;
    uint8_t vAlign = alignment & Align::VMask;

    // Calculate X position based on horizontal alignment
    float drawX = static_cast<float>(x) - bounds.position.x;
    if (hAlign == Align::HCenter)
        drawX = static_cast<float>(x) + (static_cast<float>(width) - bounds.size.x) / 2.0f - bounds.position.x;
    else if (hAlign == Align::Right)
        drawX = static_cast<float>(x + width) - bounds.size.x - bounds.position.x;

    // Calculate Y position based on vertical alignment
    float drawY = static_cast<float>(y) - bounds.position.y;
    if (vAlign == Align::VCenter)
        drawY = static_cast<float>(y) + (static_cast<float>(height) - bounds.size.y) / 2.0f - bounds.position.y;
    else if (vAlign == Align::Bottom)
        drawY = static_cast<float>(y + height) - bounds.size.y - bounds.position.y;

    // Round to nearest pixel for pixel-perfect rendering
    int pixelX = static_cast<int>(drawX + 0.5f);
    int pixelY = static_cast<int>(drawY + 0.5f);

    sfText.setFillColor(sf::Color(color.r, color.g, color.b));

    draw_at_output_resolution(*target, sfText, logical_size, static_cast<float>(pixelX), static_cast<float>(pixelY),
                              sf::RenderStates::Default, output_shift(dx, dy));
}

void SFMLTextRenderer::begin_batch()
{
    // No-op for SFML - we don't need DC acquisition
}

void SFMLTextRenderer::end_batch()
{
    // No-op for SFML
}

} // namespace hb::shared::text
