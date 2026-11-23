// bitmap_font.h: Abstract interface for sprite-based bitmap font rendering
//
// Part of the shared interface layer between client and renderers
//////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>

namespace hb::shared::text {

struct BitmapTextParams
{
    int16_t m_tint_r = 0;
    int16_t m_tint_g = 0;
    int16_t m_tint_b = 0;
    float m_alpha = 1.0f;
    bool m_shadow = false;
    bool m_color_replace = false;  // true = direct RGB color, false = offset-based tint
    bool m_use_additive = false;     // true = additive blending for bright text on dark sprites

    // Factory methods for common configurations
    static BitmapTextParams make_default()
    {
        return BitmapTextParams{};
    }

    // ColorReplace: Direct RGB color - what you specify is what you get
    // Use this for bitmap fonts where you want a specific output color
    static BitmapTextParams color_replace(int16_t r, int16_t g, int16_t b)
    {
        BitmapTextParams p;
        p.m_tint_r = r;
        p.m_tint_g = g;
        p.m_tint_b = b;
        p.m_color_replace = true;
        return p;
    }

    static BitmapTextParams color_replace_with_shadow(int16_t r, int16_t g, int16_t b)
    {
        BitmapTextParams p;
        p.m_tint_r = r;
        p.m_tint_g = g;
        p.m_tint_b = b;
        p.m_shadow = true;
        p.m_color_replace = true;
        return p;
    }

    static BitmapTextParams color_replace_with_alpha(int16_t r, int16_t g, int16_t b, float a)
    {
        BitmapTextParams p;
        p.m_tint_r = r;
        p.m_tint_g = g;
        p.m_tint_b = b;
        p.m_alpha = a;
        p.m_color_replace = true;
        return p;
    }

    // Tinted: Offset-based tinting (legacy system using gray base)
    // Values are offsets from base gray (96): final = 96 + tint
    static BitmapTextParams tinted(int16_t r, int16_t g, int16_t b)
    {
        BitmapTextParams p;
        p.m_tint_r = r;
        p.m_tint_g = g;
        p.m_tint_b = b;
        p.m_color_replace = false;
        return p;
    }

    static BitmapTextParams with_shadow()
    {
        BitmapTextParams p;
        p.m_shadow = true;
        return p;
    }

    static BitmapTextParams tinted_with_alpha(int16_t r, int16_t g, int16_t b, float a)
    {
        BitmapTextParams p;
        p.m_tint_r = r;
        p.m_tint_g = g;
        p.m_tint_b = b;
        p.m_alpha = a;
        return p;
    }

    static BitmapTextParams tinted_with_shadow(int16_t r, int16_t g, int16_t b)
    {
        BitmapTextParams p;
        p.m_tint_r = r;
        p.m_tint_g = g;
        p.m_tint_b = b;
        p.m_shadow = true;
        return p;
    }
};

class IBitmapFont
{
public:
    virtual ~IBitmapFont() = default;

    // Text measurement
    virtual int measure_text(const char* text) const = 0;
    virtual int get_char_width(char c) const = 0;

    // Drawing
    virtual void draw_text(int x, int y, const char* text, const BitmapTextParams& params = BitmapTextParams::make_default()) = 0;
    virtual void draw_text_centered(int x1, int x2, int y, const char* text, const BitmapTextParams& params = BitmapTextParams::make_default()) = 0;
};

} // namespace hb::shared::text
