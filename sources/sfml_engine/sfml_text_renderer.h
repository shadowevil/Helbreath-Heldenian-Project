// sfml_text_renderer.h: SFML implementation of ITextRenderer
//
// Part of SFMLEngine static library
//////////////////////////////////////////////////////////////////////

#pragma once

#include "text_renderer.h"
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/RenderTexture.hpp>
#include <string>

namespace hb::shared::text {

class SFMLTextRenderer : public ITextRenderer
{
public:
    SFMLTextRenderer(sf::RenderTexture* backBuffer);
    ~SFMLTextRenderer() override = default;

    // Font loading - client calls these to set up fonts
    bool LoadFontFromFile(const char* fontPath) override;

    // Font configuration
    void SetFontSize(int size) override;
    void set_font_smoothing(bool enabled) override;
    bool is_font_smoothing() const override;
    bool IsFontLoaded() const override;

    // Text measurement (fontSize: 0 = use default)
    TextMetrics measure_text(const char* text, int fontSize = 0) const override;
    int get_fitting_char_count(const char* text, int maxWidth, int fontSize = 0) const override;
    int get_line_height(int fontSize = 0) const override;

    // Drawing (fontSize: 0 = use default, nonzero = per-call override)
    void draw_text(int x, int y, const char* text, const hb::shared::render::Color& color,
                  int fontSize = 0, bool bold = false) override;
    void draw_text_aligned(int x, int y, int width, int height, const char* text, const hb::shared::render::Color& color,
                         Align alignment = Align::TopLeft, int fontSize = 0, bool bold = false) override;

    // Batching (no-op for SFML, no DC acquisition needed)
    void begin_batch() override;
    void end_batch() override;

    // Allow updating the back buffer pointer if needed
    void SetBackBuffer(sf::RenderTexture* backBuffer) { m_back_buffer = backBuffer; }

private:
    // Load the bundled font (bundled_font_path)
    bool LoadDefaultFont();

    // Resolve font size for a draw/measure call.
    // 0 = use m_font_size (default), nonzero = apply GDI-2 adjustment.
    // sf::Font lazily caches glyph pages per character size internally.
    unsigned int resolve_font_size(int fontSize) const;

    sf::RenderTexture* m_back_buffer;
    sf::Font m_font;              // Owned font (client-loaded or default)
    bool m_fontLoaded;
    bool m_font_smoothing;
    std::string m_loaded_font_path; // Source path for active TTF, used to force reload on smoothing toggle
    unsigned int m_font_size;     // Default SFML character size (GDI-adjusted)
};

} // namespace hb::shared::text
