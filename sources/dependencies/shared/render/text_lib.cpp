// text_lib.cpp: Unified text rendering implementation
//
// This provides the single entry point for all text rendering.
// It delegates to ITextRenderer (TTF) or IBitmapFont based on Font type.
//////////////////////////////////////////////////////////////////////

#include "text_lib.h"
#include "bitmap_font_factory.h"
#include <cstring>
#include <algorithm>
#include <memory>
#include <vector>
#include <string>

namespace hb::shared::text {

// ============== Static Storage ==============

// TextLib owns the bitmap fonts - no need to store them elsewhere
static std::unique_ptr<IBitmapFont> s_ownedFonts[MAX_BITMAP_FONTS];

// ============== Bitmap Font Management ==============

void load_bitmap_font(int fontId, hb::shared::sprite::ISprite* sprite, char firstChar, char lastChar,
                    int frameOffset, const FontSpacing& spacing)
{
	if (fontId < 0 || fontId >= MAX_BITMAP_FONTS)
		return;

	s_ownedFonts[fontId] = CreateBitmapFont(sprite, firstChar, lastChar, frameOffset, spacing);
}

void load_bitmap_font_dynamic(int fontId, hb::shared::sprite::ISprite* sprite, char firstChar, char lastChar,
                           int frameOffset)
{
	if (fontId < 0 || fontId >= MAX_BITMAP_FONTS)
		return;

	s_ownedFonts[fontId] = CreateBitmapFontDynamic(sprite, firstChar, lastChar, frameOffset);
}

bool is_bitmap_font_loaded(int fontId)
{
	if (fontId >= 0 && fontId < MAX_BITMAP_FONTS)
	{
		return s_ownedFonts[fontId] != nullptr;
	}
	return false;
}

IBitmapFont* get_bitmap_font(int fontId)
{
	if (fontId >= 0 && fontId < MAX_BITMAP_FONTS)
	{
		return s_ownedFonts[fontId].get();
	}
	return nullptr;
}

// ============== Internal Helpers ==============

static bool IsBitmapFont(int fontId)
{
	return fontId != FONT_ID_DEFAULT;
}

// Convert TextStyle to BitmapTextParams for bitmap font rendering
static BitmapTextParams StyleToBitmapParams(const TextStyle& style)
{
	BitmapTextParams params;
	if (style.alpha < 1.0f)
	{
		params = BitmapTextParams::color_replace_with_alpha(style.color.r, style.color.g, style.color.b, style.alpha);
	}
	else if (style.shadow == ShadowStyle::Integrated)
	{
		params = BitmapTextParams::color_replace_with_shadow(style.color.r, style.color.g, style.color.b);
	}
	else
	{
		params = BitmapTextParams::color_replace(style.color.r, style.color.g, style.color.b);
	}
	params.m_use_additive = style.useAdditive;
	return params;
}

// Calculate brightened highlight color
static void GetHighlightColor(const TextStyle& style, uint8_t& hr, uint8_t& hg, uint8_t& hb)
{
	hr = static_cast<uint8_t>(std::min(255, static_cast<int>(style.color.r) + 90));
	hg = static_cast<uint8_t>(std::min(255, static_cast<int>(style.color.g) + 55));
	hb = static_cast<uint8_t>(std::min(255, static_cast<int>(style.color.b) + 50));
}

// ============== Batching ==============

void begin_batch()
{
	ITextRenderer* renderer = GetTextRenderer();
	if (renderer)
		renderer->begin_batch();
}

void end_batch()
{
	ITextRenderer* renderer = GetTextRenderer();
	if (renderer)
		renderer->end_batch();
}

// ============== Text Rendering ==============

void draw_text(int fontId, int x, int y, const char* text, const TextStyle& style)
{
	if (!text || text[0] == '\0')
		return;

	if (IsBitmapFont(fontId))
	{
		// ============== Bitmap Font Rendering ==============
		IBitmapFont* font = get_bitmap_font(fontId);
		if (!font)
			return;

		// Handle shadow styles that require multiple draw calls
		switch (style.shadow)
		{
			case ShadowStyle::Highlight:
			{
				// draw highlight first at +1,0 with brightened color
				uint8_t hr, hg, hb;
				GetHighlightColor(style, hr, hg, hb);
				font->draw_text(x + 1, y, text, BitmapTextParams::color_replace(hr, hg, hb));
				// Main text drawn below
				break;
			}
			case ShadowStyle::DropShadow:
			{
				// draw shadow at +1,+1 in black
				font->draw_text(x + 1, y + 1, text, BitmapTextParams::color_replace(0, 0, 0));
				break;
			}
			case ShadowStyle::TwoPoint:
			{
				// 2-point shadow: 0,+1 and +1,+1 in black
				font->draw_text(x, y + 1, text, BitmapTextParams::color_replace(0, 0, 0));
				font->draw_text(x + 1, y + 1, text, BitmapTextParams::color_replace(0, 0, 0));
				break;
			}
			case ShadowStyle::ThreePoint:
			{
				// 3-point shadow: +1,+1, 0,+1, +1,0 in black
				font->draw_text(x + 1, y + 1, text, BitmapTextParams::color_replace(0, 0, 0));
				font->draw_text(x, y + 1, text, BitmapTextParams::color_replace(0, 0, 0));
				font->draw_text(x + 1, y, text, BitmapTextParams::color_replace(0, 0, 0));
				break;
			}
			default:
				// No pre-shadow needed for None or Integrated
				break;
		}

		// draw main text
		font->draw_text(x, y, text, StyleToBitmapParams(style));
	}
	else
	{
		// ============== TTF Font Rendering ==============
		ITextRenderer* renderer = GetTextRenderer();
		if (!renderer)
			return;

		// Per-call font size: passed through to renderer (0 = use default).
		// sf::Font lazily caches glyph pages per character size, so no global mutation needed.
		int fs = style.fontSize;
		bool bl = style.bold;

		// Handle shadow styles
		switch (style.shadow)
		{
			case ShadowStyle::ThreePoint:
			{
				// 3-point shadow: +1,+1, 0,+1, +1,0 in black
				renderer->draw_text(x + 1, y + 1, text, hb::shared::render::Color::Black(), fs, bl);
				renderer->draw_text(x, y + 1, text, hb::shared::render::Color::Black(), fs, bl);
				renderer->draw_text(x + 1, y, text, hb::shared::render::Color::Black(), fs, bl);
				break;
			}
			case ShadowStyle::DropShadow:
			{
				// Simple drop shadow at +1,+1 in black
				renderer->draw_text(x + 1, y + 1, text, hb::shared::render::Color::Black(), fs, bl);
				break;
			}
			case ShadowStyle::TwoPoint:
			{
				// 2-point shadow: 0,+1 and +1,+1 in black
				renderer->draw_text(x, y + 1, text, hb::shared::render::Color::Black(), fs, bl);
				renderer->draw_text(x + 1, y + 1, text, hb::shared::render::Color::Black(), fs, bl);
				break;
			}
			case ShadowStyle::Highlight:
			{
				// Highlight at +1,0 with brightened color
				uint8_t hr, hg, hb;
				GetHighlightColor(style, hr, hg, hb);
				renderer->draw_text(x + 1, y, text, hb::shared::render::Color(hr, hg, hb), fs, bl);
				break;
			}
			default:
				// None or Integrated - no pre-shadow for TTF
				break;
		}

		// draw main text
		renderer->draw_text(x, y, text, style.color, fs, bl);
	}
}

// ============== Word Wrap (cached, 32-entry ring) ==============

static constexpr int WRAP_CACHE_SIZE = 32;

struct WrapCacheEntry {
	std::string text;
	int fontId = -1;
	int maxWidth = 0;
	std::vector<std::string> lines;
};

static WrapCacheEntry s_wrapCache[WRAP_CACHE_SIZE];
static int s_wrapCacheNext = 0;

static const WrapCacheEntry* FindWrapCache(const char* text, int fontId, int maxWidth)
{
	for (int i = 0; i < WRAP_CACHE_SIZE; i++)
	{
		const auto& e = s_wrapCache[i];
		if (e.fontId == fontId && e.maxWidth == maxWidth && e.text == text)
			return &e;
	}
	return nullptr;
}

static WrapCacheEntry& AllocWrapCache()
{
	WrapCacheEntry& slot = s_wrapCache[s_wrapCacheNext];
	s_wrapCacheNext = (s_wrapCacheNext + 1) % WRAP_CACHE_SIZE;
	return slot;
}

static void WordWrap(int fontId, const char* text, int maxWidth, std::vector<std::string>& outLines)
{
	if (!text || !text[0]) {
		outLines.emplace_back("");
		return;
	}

	// Check cache
	const WrapCacheEntry* cached = FindWrapCache(text, fontId, maxWidth);
	if (cached) {
		outLines = cached->lines;
		return;
	}

	std::string textStr(text);

	if (maxWidth <= 0) {
		outLines.push_back(textStr);
		auto& slot = AllocWrapCache();
		slot = {textStr, fontId, maxWidth, outLines};
		return;
	}

	// Check if entire text fits on one line
	TextMetrics metrics = measure_text(fontId, text);
	if (metrics.width <= maxWidth) {
		outLines.push_back(textStr);
		auto& slot = AllocWrapCache();
		slot = {textStr, fontId, maxWidth, outLines};
		return;
	}

	const char* remaining = text;
	while (*remaining)
	{
		// Check if remaining fits
		if (measure_text(fontId, remaining).width <= maxWidth) {
			outLines.emplace_back(remaining);
			break;
		}

		// get max chars that fit
		int fitCount = get_fitting_char_count(fontId, remaining, maxWidth);
		if (fitCount <= 0) fitCount = 1;

		// Scan backwards for last space within fitCount
		int lastSpace = -1;
		for (int i = fitCount - 1; i >= 0; i--) {
			if (remaining[i] == ' ') {
				lastSpace = i;
				break;
			}
		}

		if (lastSpace > 0) {
			outLines.emplace_back(remaining, lastSpace);
			remaining += lastSpace + 1; // skip the space
		} else {
			outLines.emplace_back(remaining, fitCount);
			remaining += fitCount;
		}
	}

	if (outLines.empty())
		outLines.emplace_back("");

	// Store in cache
	auto& slot = AllocWrapCache();
	slot = {textStr, fontId, maxWidth, outLines};
}

// ============== Single-line aligned draw (internal) ==============

static void DrawTextAlignedSingleLine(int fontId, int rectX, int rectY, int rectWidth, int rectHeight,
                                       const char* text, const TextStyle& style, Align alignment)
{
	if (!text || text[0] == '\0')
		return;

	uint8_t hAlign = alignment & Align::HMask;
	uint8_t vAlign = alignment & Align::VMask;

	if (IsBitmapFont(fontId))
	{
		IBitmapFont* font = get_bitmap_font(fontId);
		if (!font)
			return;

		int textWidth = font->measure_text(text);
		int textHeight = 16;

		int x = rectX;
		if (hAlign == Align::HCenter)
			x = rectX + (rectWidth - textWidth) / 2;
		else if (hAlign == Align::Right)
			x = rectX + rectWidth - textWidth;

		int y = rectY;
		if (vAlign == Align::VCenter)
			y = rectY + (rectHeight - textHeight) / 2;
		else if (vAlign == Align::Bottom)
			y = rectY + rectHeight - textHeight;

		switch (style.shadow)
		{
			case ShadowStyle::Highlight:
			{
				uint8_t hr, hg, hb;
				GetHighlightColor(style, hr, hg, hb);
				font->draw_text(x + 1, y, text, BitmapTextParams::color_replace(hr, hg, hb));
				break;
			}
			case ShadowStyle::DropShadow:
				font->draw_text(x + 1, y + 1, text, BitmapTextParams::color_replace(0, 0, 0));
				break;
			case ShadowStyle::ThreePoint:
				font->draw_text(x + 1, y + 1, text, BitmapTextParams::color_replace(0, 0, 0));
				font->draw_text(x, y + 1, text, BitmapTextParams::color_replace(0, 0, 0));
				font->draw_text(x + 1, y, text, BitmapTextParams::color_replace(0, 0, 0));
				break;
			default:
				break;
		}

		font->draw_text(x, y, text, StyleToBitmapParams(style));
	}
	else
	{
		ITextRenderer* renderer = GetTextRenderer();
		if (!renderer)
			return;

		// Per-call font size: passed through to renderer (0 = use default)
		int fs = style.fontSize;
		bool bl = style.bold;

		switch (style.shadow)
		{
			case ShadowStyle::ThreePoint:
				renderer->draw_text_aligned(rectX + 1, rectY + 1, rectWidth, rectHeight, text, hb::shared::render::Color::Black(), alignment, fs, bl);
				renderer->draw_text_aligned(rectX, rectY + 1, rectWidth, rectHeight, text, hb::shared::render::Color::Black(), alignment, fs, bl);
				renderer->draw_text_aligned(rectX + 1, rectY, rectWidth, rectHeight, text, hb::shared::render::Color::Black(), alignment, fs, bl);
				break;
			case ShadowStyle::DropShadow:
				renderer->draw_text_aligned(rectX + 1, rectY + 1, rectWidth, rectHeight, text, hb::shared::render::Color::Black(), alignment, fs, bl);
				break;
			case ShadowStyle::Highlight:
			{
				uint8_t hr, hg, hb;
				GetHighlightColor(style, hr, hg, hb);
				renderer->draw_text_aligned(rectX + 1, rectY, rectWidth, rectHeight, text, hb::shared::render::Color(hr, hg, hb), alignment, fs, bl);
				break;
			}
			default:
				break;
		}

		renderer->draw_text_aligned(rectX, rectY, rectWidth, rectHeight, text, style.color, alignment, fs, bl);
	}
}

// ============== Public draw_text_aligned (single-line, no wrapping) ==============

void draw_text_aligned(int fontId, int rectX, int rectY, int rectWidth, int rectHeight, const char* text,
                     const TextStyle& style, Align alignment)
{
	DrawTextAlignedSingleLine(fontId, rectX, rectY, rectWidth, rectHeight, text, style, alignment);
}

// ============== Public draw_text_wrapped (word-wrap + multi-line) ==============

void draw_text_wrapped(int fontId, int rectX, int rectY, int rectWidth, int rectHeight, const char* text,
                     const TextStyle& style, Align alignment)
{
	if (!text || text[0] == '\0')
		return;

	// Word-wrap into lines (cached)
	std::vector<std::string> lines;
	WordWrap(fontId, text, rectWidth, lines);

	if (lines.size() <= 1) {
		DrawTextAlignedSingleLine(fontId, rectX, rectY, rectWidth, rectHeight, text, style, alignment);
		return;
	}

	// Multi-line layout
	int lineHeight = get_line_height(fontId);
	int totalTextHeight = static_cast<int>(lines.size()) * lineHeight;

	uint8_t vAlign = alignment & Align::VMask;
	Align lineAlign = static_cast<Align>((alignment & Align::HMask) | Align::Top);

	int startY = rectY;
	if (vAlign == Align::VCenter)
		startY = rectY + (rectHeight - totalTextHeight) / 2;
	else if (vAlign == Align::Bottom)
		startY = rectY + rectHeight - totalTextHeight;

	for (size_t i = 0; i < lines.size(); i++)
	{
		int lineY = startY + static_cast<int>(i) * lineHeight;
		DrawTextAlignedSingleLine(fontId, rectX, lineY, rectWidth, lineHeight,
		                          lines[i].c_str(), style, lineAlign);
	}
}

// ============== Text Measurement ==============

TextMetrics measure_text(int fontId, const char* text)
{
	if (!text || text[0] == '\0')
		return {0, 0};

	if (IsBitmapFont(fontId))
	{
		IBitmapFont* font = get_bitmap_font(fontId);
		if (font)
		{
			// IBitmapFont::measure_text returns width only, estimate height
			int width = font->measure_text(text);
			return {width, 16}; // Approximate height for bitmap fonts
		}
	}
	else
	{
		ITextRenderer* renderer = GetTextRenderer();
		if (renderer)
			return renderer->measure_text(text);
	}

	return {0, 0};
}

int get_fitting_char_count(int fontId, const char* text, int maxWidth)
{
	if (!text || text[0] == '\0')
		return 0;

	if (IsBitmapFont(fontId))
	{
		IBitmapFont* font = get_bitmap_font(fontId);
		if (!font)
			return 0;

		// Measure progressively until we exceed maxWidth
		int len = static_cast<int>(strlen(text));
		for (int i = len; i > 0; i--)
		{
			// Create substring and measure
			char temp[512];
			int copyLen = (i < 511) ? i : 511;
			memcpy(temp, text, copyLen);
			temp[copyLen] = '\0';

			int width = font->measure_text(temp);
			if (width <= maxWidth)
				return i;
		}
		return 0;
	}
	else
	{
		ITextRenderer* renderer = GetTextRenderer();
		if (renderer)
			return renderer->get_fitting_char_count(text, maxWidth);
	}

	return 0;
}

int get_line_height(int fontId)
{
	if (IsBitmapFont(fontId))
		return 16; // Approximate height for bitmap fonts

	ITextRenderer* renderer = GetTextRenderer();
	if (renderer)
		return renderer->get_line_height();

	return 0;
}

int measure_wrapped_text_height(int fontId, const char* text, int maxWidth)
{
	if (!text || text[0] == '\0')
		return 0;

	std::vector<std::string> lines;
	WordWrap(fontId, text, maxWidth, lines);

	return static_cast<int>(lines.size()) * get_line_height(fontId);
}

} // namespace hb::shared::text
