// text_renderer.h: Abstract interface for system font text rendering
//
// Part of the shared interface layer between client and renderers
//////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include "primitive_types.h"

namespace hb::shared::render {
class ITexture;
}

namespace hb::shared::text {

// The one font the client ships: Wine's Tahoma replacement (LGPL 2.1, licence beside it).
// No system font is ever used.
constexpr const char* bundled_font_path = "fonts/wine_tahoma.ttf";

// ============== Basic Types ==============

struct TextMetrics
{
    int width;
    int height;
};

// Text alignment flags (combine horizontal and vertical with bitwise OR)
enum Align : uint8_t
{
    // Horizontal (bits 0-1)
    Left    = 0x00,
    HCenter = 0x01,
    Right   = 0x02,

    // Vertical (bits 2-3)
    Top     = 0x00,
    VCenter = 0x04,
    Bottom  = 0x08,

    // Common combinations
    TopLeft      = Top | Left,
    TopCenter    = Top | HCenter,
    TopRight     = Top | Right,
    MiddleLeft   = VCenter | Left,
    Center       = VCenter | HCenter,
    MiddleRight  = VCenter | Right,
    BottomLeft   = Bottom | Left,
    BottomCenter = Bottom | HCenter,
    BottomRight  = Bottom | Right,

    // Masks for extracting components
    HMask = 0x03,
    VMask = 0x0C
};

// A styled TTF run (panel kit labels): an exact pixel size in a loaded face, with extra space
// after every letter. Unlike fontSize below, pixel_size gets no GDI adjustment.
struct text_run
{
    int face = 0;             // 0 = the bundled font, otherwise an id from load_face
    int pixel_size = 12;      // FreeType pixel size (PIL's truetype size)
    float tracking = 0.0f;    // extra logical px after every letter
};

// Ink box of a run in logical units, relative to the origin draw_run takes
struct ink_box
{
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
};

class ITextRenderer
{
public:
    virtual ~ITextRenderer() = default;

    // Font loading - client calls these to set up fonts
    // Returns true on success. The engine loads bundled_font_path itself at construction.
    virtual bool LoadFontFromFile(const char* fontPath) = 0;

    // Font configuration
    virtual void SetFontSize(int size) = 0;
    virtual void set_font_smoothing(bool enabled) { (void)enabled; }
    virtual bool is_font_smoothing() const { return false; }

    // Check if a font is loaded (either client-provided or default)
    virtual bool IsFontLoaded() const = 0;

    // Text measurement
    // fontSize: 0 = use default, nonzero = override for this call only
    virtual TextMetrics measure_text(const char* text, int fontSize = 0) const = 0;
    virtual int get_fitting_char_count(const char* text, int maxWidth, int fontSize = 0) const = 0;
    virtual int get_line_height(int fontSize = 0) const = 0;

    // Drawing
    // fontSize: 0 = use default, nonzero = override for this call only.
    // sf::Font lazily caches glyph pages per character size, so per-call size is free.
    virtual void draw_text(int x, int y, const char* text, const hb::shared::render::Color& color,
                          int fontSize = 0, bool bold = false) = 0;
    virtual void draw_text_aligned(int x, int y, int width, int height, const char* text, const hb::shared::render::Color& color,
                                 Align alignment = Align::TopLeft, int fontSize = 0, bool bold = false) = 0;
    // A shadow or highlight copy: the text placed as by draw_text / draw_text_aligned at (x, y), then moved
    // (dx, dy) logical px as whole output pixels (round(d * ui scale)). At a fractional UI scale, x + 1 and x
    // round to output pixels separately, so the copy would sit 1 px from its text on some lines and 2 on others.
    virtual void draw_text_offset(int x, int y, int dx, int dy, const char* text, const hb::shared::render::Color& color,
                                  int fontSize = 0, bool bold = false)
    {
        draw_text(x + dx, y + dy, text, color, fontSize, bold);
    }
    virtual void draw_text_aligned_offset(int x, int y, int width, int height, int dx, int dy, const char* text,
                                          const hb::shared::render::Color& color, Align alignment = Align::TopLeft,
                                          int fontSize = 0, bool bold = false)
    {
        draw_text_aligned(x + dx, y + dy, width, height, text, color, alignment, fontSize, bold);
    }

    // Styled runs. load_face returns a face id >= 1 for a TTF file (the same id again for a path and
    // smooth already loaded), or -1 when it cannot be opened. smooth = the glyph texture samples
    // linearly (a fractional origin blends between pixels) rather than nearest; the font smoothing
    // setting applies to the bundled font only.
    virtual int load_face(const char* path, bool smooth = true) { (void)path; (void)smooth; return -1; }
    virtual ink_box measure_run(const char* text, const text_run& run) const { (void)text; (void)run; return {}; }
    // The run with its origin at (x, y): the whole part snapped to an output pixel, the fractional part
    // kept, so with a smooth face a +0.5 faux-bold copy blends between pixels. color.a is the opacity;
    // additive adds the glyphs to the target.
    virtual void draw_run(float x, float y, const char* text, const text_run& run,
                          const hb::shared::render::Color& color,
                          hb::shared::render::BlendMode blend = hb::shared::render::BlendMode::Alpha)
    {
        (void)x; (void)y; (void)text; (void)run; (void)color; (void)blend;
    }
    // The run rasterised once into a display texture, for art drawn like a sprite label: run.pixel_size is
    // in texture pixels, the texture is the ink box plus pad pixels on every side, every pixel is color
    // with the glyph coverage as alpha, sampled smooth with mipmaps. nullptr when the face or text is
    // unusable; the caller owns the texture.
    // text is UTF-8 here (unlike draw_text and the other runs, whose const char* is read as ANSI, so
    // legacy server bytes keep working): a baked label can carry any glyph its face has, e.g. the
    // copyright sign as "\xC2\xA9".
    virtual hb::shared::render::ITexture* bake_run(const char* text, const text_run& run,
                                                   const hb::shared::render::Color& color, int pad = 2)
    {
        (void)text; (void)run; (void)color; (void)pad;
        return nullptr;
    }
    // A soft glow for a run: its glyph coverage dilated by dilate px (a square window), blurred with a
    // Gaussian of sigma blur px, drawn as color (color.a is the glow's peak opacity) with that coverage as
    // alpha. run.pixel_size, dilate and blur are in texture pixels. The texture is the ink box plus
    // glow_pad(dilate, blur) pixels on every side, so it lines up with a bake_run of the same run at
    // that pad. text is UTF-8, as for bake_run. nullptr when the face or text is unusable; the caller
    // owns the texture.
    virtual hb::shared::render::ITexture* bake_glow(const char* text, const text_run& run,
                                                    const hb::shared::render::Color& color, int dilate, float blur)
    {
        (void)text; (void)run; (void)color; (void)dilate; (void)blur;
        return nullptr;
    }
    static int glow_pad(int dilate, float blur)
    {
        return dilate + static_cast<int>(blur * 3.0f + 0.999f) + 1;
    }

    // Batching for performance (DDraw needs DC acquisition)
    virtual void begin_batch() = 0;
    virtual void end_batch() = 0;
};

// Global accessor - set by RendererFactory during initialization
ITextRenderer* GetTextRenderer();
void SetTextRenderer(ITextRenderer* renderer);

} // namespace hb::shared::text
