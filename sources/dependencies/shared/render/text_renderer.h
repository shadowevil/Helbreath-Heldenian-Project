// text_renderer.h: Abstract interface for system font text rendering
//
// Part of the shared interface layer between client and renderers
//////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include "primitive_types.h"

namespace hb::shared::text {

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

class ITextRenderer
{
public:
    virtual ~ITextRenderer() = default;

    // Font loading - client calls these to set up fonts
    // Returns true on success. Engine has default fallback if these fail.
    virtual bool LoadFontFromFile(const char* fontPath) = 0;
    virtual bool LoadFontByName(const char* fontName) = 0;  // System font by name (e.g., "Arial")

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

    // Batching for performance (DDraw needs DC acquisition)
    virtual void begin_batch() = 0;
    virtual void end_batch() = 0;
};

// Global accessor - set by RendererFactory during initialization
ITextRenderer* GetTextRenderer();
void SetTextRenderer(ITextRenderer* renderer);

} // namespace hb::shared::text
