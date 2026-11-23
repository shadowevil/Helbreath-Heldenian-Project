// sfml_text_renderer.cpp: SFML implementation of ITextRenderer
//
// Part of SFMLEngine static library
//////////////////////////////////////////////////////////////////////

#include "sfml_text_renderer.h"
#include <SFML/Graphics/Text.hpp>
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

SFMLTextRenderer::SFMLTextRenderer(sf::RenderTexture* backBuffer)
    : m_back_buffer(backBuffer)
    , m_fontLoaded(false)
    , m_font_smoothing(false)
    , m_loaded_font_path()
    , m_font_size(12)  // Default size to match GDI rendering
{
    // Try to load a default font as fallback
    LoadDefaultFont();
}

bool SFMLTextRenderer::LoadDefaultFont()
{
    // Try bundled fonts first (most portable), then system fonts
    const char* font_paths[] = {
        "fonts/tahoma.ttf",
        "fonts/arial.ttf",
        "fonts/segoeui.ttf",
#ifdef _WIN32
        "C:\\Windows\\Fonts\\tahoma.ttf",
        "C:\\Windows\\Fonts\\arial.ttf",
        "C:\\Windows\\Fonts\\segoeui.ttf",
#elif defined(__APPLE__)
        "/Library/Fonts/Tahoma.ttf",
        "/Library/Fonts/Arial.ttf",
        "/System/Library/Fonts/Supplemental/Tahoma.ttf",
#else
        "/usr/share/fonts/truetype/msttcorefonts/tahoma.ttf",
        "/usr/share/fonts/truetype/msttcorefonts/arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
#endif
    };

    for (const char* path : font_paths)
    {
        if (m_font.openFromFile(path))
        {
            m_font.setSmooth(m_font_smoothing);
            m_fontLoaded = true;
            m_loaded_font_path = path;
            return true;
        }
    }

    return false;
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

bool SFMLTextRenderer::LoadFontByName(const char* fontName)
{
    if (!fontName)
        return false;

    // Search directories: bundled first, then platform system fonts
    const char* font_directories[] = {
        "fonts/",
#ifdef _WIN32
        "C:\\Windows\\Fonts\\",
#elif defined(__APPLE__)
        "/Library/Fonts/",
        "/System/Library/Fonts/Supplemental/",
#else
        "/usr/share/fonts/truetype/msttcorefonts/",
        "/usr/share/fonts/truetype/",
        "/usr/share/fonts/TTF/",
#endif
    };

    const char* extensions[] = { ".ttf", ".otf", ".TTF", ".OTF", "" };

    for (const char* dir : font_directories)
    {
        for (const char* ext : extensions)
        {
            std::string full_path = std::string(dir) + fontName + ext;
            if (m_font.openFromFile(full_path))
            {
                m_font.setSmooth(m_font_smoothing);
                m_fontLoaded = true;
                m_loaded_font_path = full_path;
                return true;
            }
        }
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
    if (!text || !m_fontLoaded || !m_back_buffer)
        return;

    sf::Text sfText(m_font, text, resolve_font_size(fontSize));
    sfText.setPosition({static_cast<float>(x), static_cast<float>(y)});
    sfText.setFillColor(sf::Color(color.r, color.g, color.b));
    if (bold)
        sfText.setStyle(sf::Text::Bold);

    m_back_buffer->draw(sfText);
}

void SFMLTextRenderer::draw_text_aligned(int x, int y, int width, int height, const char* text, const hb::shared::render::Color& color,
                                        Align alignment, int fontSize, bool bold)
{
    if (!text || !m_fontLoaded || !m_back_buffer)
        return;

    sf::Text sfText(m_font, text, resolve_font_size(fontSize));
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

    sfText.setPosition({static_cast<float>(pixelX), static_cast<float>(pixelY)});
    sfText.setFillColor(sf::Color(color.r, color.g, color.b));

    m_back_buffer->draw(sfText);
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
