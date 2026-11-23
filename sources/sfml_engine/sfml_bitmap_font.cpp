// sfml_bitmap_font.cpp: SFML implementation of IBitmapFont
//
// Part of SFMLEngine static library
//////////////////////////////////////////////////////////////////////

#include "sfml_bitmap_font.h"
#include "sprite_types.h"

namespace hb::shared::text {

// Global accessor implementation
static BitmapFontFactory* s_pBitmapFontFactory = nullptr;

BitmapFontFactory* GetBitmapFontFactory()
{
    return s_pBitmapFontFactory;
}

void SetBitmapFontFactory(BitmapFontFactory* factory)
{
    s_pBitmapFontFactory = factory;
}

// SFMLBitmapFont implementation

SFMLBitmapFont::SFMLBitmapFont(hb::shared::sprite::ISprite* sprite, char firstChar, char lastChar,
                               int frameOffset, const FontSpacing& spacing)
    : m_sprite(sprite)
    , m_firstChar(firstChar)
    , m_lastChar(lastChar)
    , m_frameOffset(frameOffset)
    , m_charWidths(spacing.charWidths)
    , m_defaultWidth(spacing.defaultWidth)
    , m_useDynamicSpacing(spacing.useDynamicSpacing)
{
}

int SFMLBitmapFont::GetFrameForChar(char c) const
{
    if (c < m_firstChar || c > m_lastChar)
        return -1;

    return m_frameOffset + (c - m_firstChar);
}

int SFMLBitmapFont::get_char_width(char c) const
{
    if (c == ' ')
        return m_defaultWidth;

    if (c < m_firstChar || c > m_lastChar)
        return m_defaultWidth;

    int charIndex = c - m_firstChar;

    if (m_useDynamicSpacing && m_sprite)
    {
        int frame = GetFrameForChar(c);
        if (frame >= 0 && frame < m_sprite->GetFrameCount())
        {
            hb::shared::sprite::SpriteRect rect = m_sprite->GetFrameRect(frame);
            return rect.width;
        }
    }

    if (!m_charWidths.empty() && charIndex < static_cast<int>(m_charWidths.size()))
    {
        return m_charWidths[charIndex];
    }

    return m_defaultWidth;
}

int SFMLBitmapFont::measure_text(const char* text) const
{
    if (!text)
        return 0;

    int width = 0;
    while (*text)
    {
        width += get_char_width(*text);
        text++;
    }
    return width;
}

void SFMLBitmapFont::draw_text(int x, int y, const char* text, const BitmapTextParams& params)
{
    if (!text || !m_sprite)
        return;

    // Set up draw parameters for bitmap font rendering
    // Bitmap font sprites are pure white - SFML multiply tint acts as color replacement:
    // white(255) * tint / 255 = tint. m_has_tint flag ensures tint block runs even for (0,0,0).
    hb::shared::sprite::DrawParams drawParams;
    drawParams.m_alpha = params.m_alpha;
    drawParams.m_tint_r = params.m_tint_r;
    drawParams.m_tint_g = params.m_tint_g;
    drawParams.m_tint_b = params.m_tint_b;
    drawParams.m_has_tint = params.m_color_replace ||
                            (params.m_tint_r != 0 || params.m_tint_g != 0 || params.m_tint_b != 0);


    // Use additive blending when explicitly requested (for bright text like damage numbers)
    // This matches DDraw's additive tinting behavior for bright colors
    if (params.m_use_additive)
    {
        drawParams.m_blend_mode = hb::shared::sprite::BlendMode::Additive;
    }

    // Bitmap font glyphs must use nearest-neighbor filtering. Bilinear smoothing
    // causes glyph edges to bleed into adjacent transparent pixels, producing
    // visible artifacts with additive blending (especially on Linux/OpenGL).
    drawParams.m_nearest_filter = true;

    int currentX = x;

    while (*text)
    {
        char c = *text;

        if (c != ' ')
        {
            int frame = GetFrameForChar(c);
            if (frame >= 0 && frame < m_sprite->GetFrameCount())
            {
                if (params.m_shadow)
                {
                    // Original PutString_SprFont2 behavior:
                    // draw raw/uncolored sprite at (+1,0) and (+1,+1) before main colored text
                    // In SFML, raw sprites appear white, so we tint near-black to match DDraw
                    hb::shared::sprite::DrawParams shadowParams;
                    shadowParams.m_tint_r = 1;
                    shadowParams.m_tint_g = 1;
                    shadowParams.m_tint_b = 1;
                    shadowParams.m_has_tint = true;
                    shadowParams.m_nearest_filter = true;
                    m_sprite->draw(currentX + 1, y, frame, shadowParams);
                    m_sprite->draw(currentX + 1, y + 1, frame, shadowParams);
                }

                m_sprite->draw(currentX, y, frame, drawParams);
            }
        }

        currentX += get_char_width(c);
        text++;
    }
}

void SFMLBitmapFont::draw_text_centered(int x1, int x2, int y, const char* text, const BitmapTextParams& params)
{
    if (!text)
        return;

    int textWidth = measure_text(text);
    int centerX = (x1 + x2) / 2;
    int drawX = centerX - (textWidth / 2);

    draw_text(drawX, y, text, params);
}

// SFMLBitmapFontFactory implementation

std::unique_ptr<IBitmapFont> SFMLBitmapFontFactory::CreateFont(hb::shared::sprite::ISprite* sprite, char firstChar, char lastChar,
                                                                int frameOffset, const FontSpacing& spacing)
{
    if (!sprite)
        return nullptr;

    return std::make_unique<SFMLBitmapFont>(sprite, firstChar, lastChar, frameOffset, spacing);
}

std::unique_ptr<IBitmapFont> SFMLBitmapFontFactory::CreateFontDynamic(hb::shared::sprite::ISprite* sprite, char firstChar, char lastChar,
                                                                       int frameOffset)
{
    if (!sprite)
        return nullptr;

    FontSpacing spacing;
    spacing.useDynamicSpacing = true;
    spacing.defaultWidth = 5;

    return std::make_unique<SFMLBitmapFont>(sprite, firstChar, lastChar, frameOffset, spacing);
}

} // namespace hb::shared::text
