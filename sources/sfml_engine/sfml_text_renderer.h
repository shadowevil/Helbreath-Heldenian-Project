// sfml_text_renderer.h: SFML implementation of ITextRenderer
//
// Part of SFMLEngine static library
//////////////////////////////////////////////////////////////////////

#pragma once

#include "text_renderer.h"
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/RenderStates.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

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
    void draw_text_offset(int x, int y, int dx, int dy, const char* text, const hb::shared::render::Color& color,
                          int fontSize = 0, bool bold = false) override;
    void draw_text_aligned_offset(int x, int y, int width, int height, int dx, int dy, const char* text,
                                  const hb::shared::render::Color& color, Align alignment = Align::TopLeft,
                                  int fontSize = 0, bool bold = false) override;

    // Styled runs in extra faces
    int load_face(const char* path, bool smooth = true) override;
    ink_box measure_run(const char* text, const text_run& run) const override;
    void draw_run(float x, float y, const char* text, const text_run& run, const hb::shared::render::Color& color,
                  hb::shared::render::BlendMode blend) override;
    hb::shared::render::ITexture* bake_run(const char* text, const text_run& run,
                                           const hb::shared::render::Color& color, int pad = 2) override;
    hb::shared::render::ITexture* bake_glow(const char* text, const text_run& run,
                                            const hb::shared::render::Color& color, int dilate, float blur) override;

    // Pixel faces (TTF recreations of bitmap fonts)
    int load_pixel_face(const char* path, int pixel_size) override;
    int measure_pixel_run(const char* text, const pixel_run& run) const override;
    void draw_pixel_run(int x, int y, int dx, int dy, const char* text, const pixel_run& run,
                        const hb::shared::render::Color& color, hb::shared::render::BlendMode blend) override;

    // Batching (no-op for SFML, no DC acquisition needed)
    void begin_batch() override;
    void end_batch() override;

private:
    struct face
    {
        std::string path;
        bool smooth = true;
        std::unique_ptr<sf::Font> font;
    };

    // Printable ASCII, the range a pixel face covers
    static constexpr char32_t pixel_first_char = U' ';
    static constexpr std::size_t pixel_glyph_count = 95;

    struct pixel_glyph
    {
        bool present = false;     // the face maps the character
        int advance = 0;          // logical px
        sf::IntRect mask;         // the 1:1 mask in the face's atlas (empty for a glyph with no ink)
        sf::Vector2i offset;      // the mask's top-left from the pen position on the baseline, logical px
    };

    struct pixel_face
    {
        std::string path;
        int pixel_size = 16;
        std::unique_ptr<sf::Font> font;   // scaled draws, nearest sampled
        sf::Texture atlas;                // 1:1 masks: white, alpha 0 or 255
        std::array<pixel_glyph, pixel_glyph_count> glyphs;
    };

    // The face for a pixel run's face id, or nullptr when it is not loaded
    const pixel_face* pixel_face_at(int face_id) const;
    // The glyph for a byte, or nullptr for a space, a byte outside ' '..'~' or a character the face lacks
    static const pixel_glyph* pixel_glyph_for(const pixel_face& face, char c);
    // Lay out a pixel run: visit(c, glyph, pen) for each glyph at its pen position (logical px from the run's
    // start); a space or anything without a glyph advances run.space_advance. Returns the run's width.
    template <typename Visit>
    static int for_each_pixel_glyph(const pixel_face& face, const char* text, const pixel_run& run, Visit visit);

    // The font for a run's face id, or nullptr when it is not loaded
    const sf::Font* face_font(int face_id) const;

    // A run laid out at its logical pixel size, tracking applied
    sf::Text make_run(const sf::Font& font, const sf::String& text, const text_run& run) const;

    // (dx, dy) logical px as a logical shift of whole output pixels, added after the origin is snapped
    sf::Vector2f output_shift(int dx, int dy) const;

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
                                   float x, float y, const sf::RenderStates& states = sf::RenderStates::Default,
                                   sf::Vector2f subpixel = {}) const;

    SFMLRenderer* m_renderer;     // Not owned
    sf::Font m_font;             // Owned font (client-loaded or default)
    bool m_fontLoaded;
    bool m_font_smoothing;
    std::string m_loaded_font_path; // Source path for active TTF, used to force reload on smoothing toggle
    unsigned int m_font_size;     // Default SFML character size (GDI-adjusted)
    std::vector<face> m_faces;    // load_face fonts; face id = index + 1
    std::vector<std::unique_ptr<pixel_face>> m_pixel_faces;  // load_pixel_face fonts; face id = index + 1
    sf::VertexArray m_pixel_quads{sf::PrimitiveType::Triangles};  // draw_pixel_run's quads, reused per call
};

} // namespace hb::shared::text
