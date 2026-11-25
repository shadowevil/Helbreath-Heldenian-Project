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

// TTF shadow / highlight distance in logical px; each copy moves round(shadow_px * ui scale) whole output px
// (2 at the 1.6x UI scale, 1 at 1:1)
static constexpr int shadow_px = 1;

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

// Pixel fonts by id (face -1 = none); they share the bitmap ids, and a pixel font wins over a bitmap one
static pixel_run s_pixel_fonts[MAX_BITMAP_FONTS];

bool load_pixel_font(int fontId, const char* path, int pixel_size, int baseline, int space_advance)
{
	ITextRenderer* renderer = GetTextRenderer();
	if (fontId < 0 || fontId >= MAX_BITMAP_FONTS || !renderer)
		return false;

	const int face = renderer->load_pixel_face(path, pixel_size);
	if (face < 0)
		return false;
	s_pixel_fonts[fontId] = {face, baseline, space_advance};
	return true;
}

static const pixel_run* get_pixel_font(int fontId)
{
	if (fontId < 0 || fontId >= MAX_BITMAP_FONTS || s_pixel_fonts[fontId].face < 0)
		return nullptr;
	return &s_pixel_fonts[fontId];
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

// A pixel font drawn as the bitmap fonts draw: the shadow copies opaque black (Integrated: near-black at
// +1,0 and +1,+1), alpha blended, then the text in its colour with the style's alpha and blend. The copies
// cover the whole string rather than going glyph by glyph as the bitmap's Integrated shadow does.
static void draw_pixel_text(const pixel_run& run, int x, int y, const char* text, const TextStyle& style, ShadowStyle shadow)
{
	ITextRenderer* renderer = GetTextRenderer();
	if (!renderer)
		return;

	// A translucent bitmap draw takes the alpha params, which carry no integrated shadow
	if (shadow == ShadowStyle::Integrated && style.alpha < 1.0f)
		shadow = ShadowStyle::None;

	const hb::shared::render::Color black = hb::shared::render::Color::Black();
	const auto copy = [&](int dx, int dy, const hb::shared::render::Color& color)
	{
		renderer->draw_pixel_run(x, y, dx, dy, text, run, color);
	};
	switch (shadow)
	{
		case ShadowStyle::Highlight:
		{
			uint8_t hr, hg, hb;
			GetHighlightColor(style, hr, hg, hb);
			copy(1, 0, hb::shared::render::Color(hr, hg, hb));
			break;
		}
		case ShadowStyle::DropShadow:
			copy(1, 1, black);
			break;
		case ShadowStyle::TwoPoint:
			copy(0, 1, black);
			copy(1, 1, black);
			break;
		case ShadowStyle::ThreePoint:
			copy(1, 1, black);
			copy(0, 1, black);
			copy(1, 0, black);
			break;
		case ShadowStyle::Integrated:
			copy(1, 0, hb::shared::render::Color(1, 1, 1));
			copy(1, 1, hb::shared::render::Color(1, 1, 1));
			break;
		default:
			break;
	}

	// The bitmap path's opacity: alpha * 255 truncated (SFMLSprite::draw)
	const uint8_t a = style.alpha < 1.0f ? static_cast<uint8_t>(style.alpha * 255.0f) : 255;
	renderer->draw_pixel_run(x, y, 0, 0, text, run, hb::shared::render::Color(style.color.r, style.color.g, style.color.b, a),
	                         style.useAdditive ? hb::shared::render::BlendMode::Additive : hb::shared::render::BlendMode::Alpha);
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

	if (const pixel_run* run = get_pixel_font(fontId))
	{
		draw_pixel_text(*run, x, y, text, style, style.shadow);
	}
	else if (IsBitmapFont(fontId))
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

		// Handle shadow styles. The copies go through draw_text_offset so each sits the same whole number of
		// output pixels from the text at any UI scale (a separately rounded x + 1 gave 1 px on some lines, 2 on others)
		const hb::shared::render::Color black = hb::shared::render::Color::Black();
		switch (style.shadow)
		{
			case ShadowStyle::ThreePoint:
			{
				// 3-point shadow: +1,+1, 0,+1, +1,0 in black
				renderer->draw_text_offset(x, y, shadow_px, shadow_px, text, black, fs, bl);
				renderer->draw_text_offset(x, y, 0, shadow_px, text, black, fs, bl);
				renderer->draw_text_offset(x, y, shadow_px, 0, text, black, fs, bl);
				break;
			}
			case ShadowStyle::DropShadow:
			{
				// Simple drop shadow at +1,+1 in black
				renderer->draw_text_offset(x, y, shadow_px, shadow_px, text, black, fs, bl);
				break;
			}
			case ShadowStyle::TwoPoint:
			{
				// 2-point shadow: 0,+1 and +1,+1 in black
				renderer->draw_text_offset(x, y, 0, shadow_px, text, black, fs, bl);
				renderer->draw_text_offset(x, y, shadow_px, shadow_px, text, black, fs, bl);
				break;
			}
			case ShadowStyle::Highlight:
			{
				// Highlight at +1,0 with brightened color
				uint8_t hr, hg, hb;
				GetHighlightColor(style, hr, hg, hb);
				renderer->draw_text_offset(x, y, shadow_px, 0, text, hb::shared::render::Color(hr, hg, hb), fs, bl);
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

	const pixel_run* pixel = get_pixel_font(fontId);
	IBitmapFont* font = pixel ? nullptr : get_bitmap_font(fontId);
	if (pixel || IsBitmapFont(fontId))
	{
		if (!pixel && !font)
			return;

		int textWidth = measure_text(fontId, text).width;
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

		if (pixel)
		{
			// The bitmap aligned path has no two-point shadow; nor does this
			draw_pixel_text(*pixel, x, y, text, style,
			                style.shadow == ShadowStyle::TwoPoint ? ShadowStyle::None : style.shadow);
			return;
		}

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

		// Shadow copies in whole output pixels, as in draw_text
		const hb::shared::render::Color black = hb::shared::render::Color::Black();
		switch (style.shadow)
		{
			case ShadowStyle::ThreePoint:
				renderer->draw_text_aligned_offset(rectX, rectY, rectWidth, rectHeight, shadow_px, shadow_px, text, black, alignment, fs, bl);
				renderer->draw_text_aligned_offset(rectX, rectY, rectWidth, rectHeight, 0, shadow_px, text, black, alignment, fs, bl);
				renderer->draw_text_aligned_offset(rectX, rectY, rectWidth, rectHeight, shadow_px, 0, text, black, alignment, fs, bl);
				break;
			case ShadowStyle::DropShadow:
				renderer->draw_text_aligned_offset(rectX, rectY, rectWidth, rectHeight, shadow_px, shadow_px, text, black, alignment, fs, bl);
				break;
			case ShadowStyle::Highlight:
			{
				uint8_t hr, hg, hb;
				GetHighlightColor(style, hr, hg, hb);
				renderer->draw_text_aligned_offset(rectX, rectY, rectWidth, rectHeight, shadow_px, 0, text, hb::shared::render::Color(hr, hg, hb), alignment, fs, bl);
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

	if (const pixel_run* run = get_pixel_font(fontId))
	{
		ITextRenderer* renderer = GetTextRenderer();
		return {renderer ? renderer->measure_pixel_run(text, *run) : 0, 16}; // the bitmap fonts' height
	}

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
		if (!get_pixel_font(fontId) && !get_bitmap_font(fontId))
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

			int width = measure_text(fontId, temp).width;
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
