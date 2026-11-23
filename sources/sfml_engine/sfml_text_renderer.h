// sfml_text_renderer.h: SFML implementation of ITextRenderer
//
// Part of SFMLEngine static library
//////////////////////////////////////////////////////////////////////

#pragma once

#include "text_renderer.h"
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Text.hpp>
#include <string>

class SFMLRenderer;

namespace hb::shared::text {

class SFMLTextRenderer : public ITextRenderer
{
public:
    // Text draws to the renderer's current target (world target or window), never a fixed one
    explicit SFMLTextRenderer(SFMLRenderer* renderer);
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

private:
    // Load the bundled font (bundled_font_path)
    bool LoadDefaultFont();

    // Resolve font size for a draw/measure call.
    // 0 = use m_font_size (default), nonzero = apply GDI-2 adjustment.
    // sf::Font lazily caches glyph pages per character size internally.
    unsigned int resolve_font_size(int fontSize) const;

    // Draw text laid out at logical size with its origin at a logical position. In screen space it is
    // rasterised at the window's resolution (character size logical * ui scale, drawn at 1/scale on a
    // whole-pixel origin); in the world pass, or at 1:1, it draws exactly as given.
    void draw_at_output_resolution(sf::RenderTarget& target, sf::Text& text, unsigned int logical_size,
                                   float x, float y) const;

    SFMLRenderer* m_renderer;     // Not owned
    sf::Font m_font;             // Owned font (client-loaded or default)
    bool m_fontLoaded;
    bool m_font_smoothing;
    std::string m_loaded_font_path; // Source path for active TTF, used to force reload on smoothing toggle
    unsigned int m_font_size;     // Default SFML character size (GDI-adjusted)
};

} // namespace hb::shared::text
