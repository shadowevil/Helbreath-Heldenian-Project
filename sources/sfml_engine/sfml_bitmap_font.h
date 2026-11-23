// sfml_bitmap_font.h: SFML implementation of IBitmapFont
//
// Part of SFMLEngine static library
//////////////////////////////////////////////////////////////////////

#pragma once

#include "bitmap_font.h"
#include "bitmap_font_factory.h"
#include "sprite.h"
#include <vector>
#include <memory>

namespace hb::shared::text {

class SFMLBitmapFont : public IBitmapFont
{
public:
    SFMLBitmapFont(hb::shared::sprite::ISprite* sprite, char firstChar, char lastChar,
                   int frameOffset, const FontSpacing& spacing);
    ~SFMLBitmapFont() override = default;

    // Text measurement
    int measure_text(const char* text) const override;
    int get_char_width(char c) const override;

    // Drawing
    void draw_text(int x, int y, const char* text, const BitmapTextParams& params) override;
    void draw_text_centered(int x1, int x2, int y, const char* text, const BitmapTextParams& params) override;

private:
    int GetFrameForChar(char c) const;

    hb::shared::sprite::ISprite* m_sprite;
    char m_firstChar;
    char m_lastChar;
    int m_frameOffset;
    std::vector<int> m_charWidths;
    int m_defaultWidth;
    bool m_useDynamicSpacing;
};

class SFMLBitmapFontFactory : public BitmapFontFactory
{
public:
    ~SFMLBitmapFontFactory() override = default;

    std::unique_ptr<IBitmapFont> CreateFont(hb::shared::sprite::ISprite* sprite, char firstChar, char lastChar,
                                            int frameOffset, const FontSpacing& spacing) override;
    std::unique_ptr<IBitmapFont> CreateFontDynamic(hb::shared::sprite::ISprite* sprite, char firstChar, char lastChar,
                                                   int frameOffset) override;
};

} // namespace hb::shared::text
