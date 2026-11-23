// text_lib.h: Unified text rendering API for Helbreath
//
// This is the single entry point for all text rendering in the game.
// The game should only use hb::shared::text:: functions for text rendering.
// Engines provide ITextRenderer (TTF) and IBitmapFont implementations.
//////////////////////////////////////////////////////////////////////

#pragma once

#include "text_renderer.h"
#include "bitmap_font.h"
#include "bitmap_font_factory.h"
#include "primitive_types.h"
#include <algorithm>

namespace hb::shared::text {

// ============== Font ID System ==============
// TextLib uses integer font IDs. The game defines its own enum that maps to these.
// Font ID 0 is reserved for the default TTF font.
// Bitmap fonts use IDs 1 and up.

constexpr int FONT_ID_DEFAULT = 0;      // TTF font (bundled_font_path, Wine Tahoma)
constexpr int MAX_BITMAP_FONTS = 16;    // Maximum number of bitmap fonts

// ============== Shadow Styles ==============

enum class ShadowStyle {
	None,           // No shadow
	Highlight,      // +1,0 offset with brightened color (PutString_SprFont style)
	TwoPoint,       // 0,+1 and +1,+1 offsets in black (PutString_SprFont3 style)
	ThreePoint,     // +1,+1, 0,+1, +1,0 offsets in black (PutString2 style)
	DropShadow,     // +1,+1 offset in black (simple drop shadow)
	Integrated      // Font handles shadow internally (PutString_SprFont2 style)
};

// ============== Text Style ==============

struct TextStyle
{
	hb::shared::render::Color color{255, 255, 255};
	float alpha = 1.0f;
	ShadowStyle shadow = ShadowStyle::None;
	int fontSize = 0;  // 0 = use default size, ignored for bitmap fonts
	bool bold = false;  // Bold style (TTF only, ignored for bitmap fonts)
	bool useAdditive = false;  // Use additive blending for bright text on dark sprites

	// Default constructor
	constexpr TextStyle() = default;

	// hb::shared::render::Color constructor
	constexpr TextStyle(const hb::shared::render::Color& c)
		: color(c) {}

	// hb::shared::render::Color + shadow constructor
	constexpr TextStyle(const hb::shared::render::Color& c, ShadowStyle s)
		: color(c), shadow(s) {}

	// hb::shared::render::Color + alpha constructor
	constexpr TextStyle(const hb::shared::render::Color& c, float a)
		: color(c), alpha(a) {}

	// Full constructor
	constexpr TextStyle(const hb::shared::render::Color& c, float a, ShadowStyle s)
		: color(c), alpha(a), shadow(s) {}

	// Full constructor with size
	constexpr TextStyle(const hb::shared::render::Color& c, float a, ShadowStyle s, int size)
		: color(c), alpha(a), shadow(s), fontSize(size) {}

	// Full constructor with additive
	constexpr TextStyle(const hb::shared::render::Color& c, float a, ShadowStyle s, int size, bool additive, bool b = false)
		: color(c), alpha(a), shadow(s), fontSize(size), bold(b), useAdditive(additive) {}

	// ============== Factory Methods ==============

	// Simple color
	static constexpr TextStyle from_color(const hb::shared::render::Color& c) {
		return TextStyle(c);
	}

	// hb::shared::render::Color with 3-point shadow (like PutString2)
	static constexpr TextStyle with_shadow(const hb::shared::render::Color& c) {
		return TextStyle(c, ShadowStyle::ThreePoint);
	}

	// hb::shared::render::Color with 2-point shadow (0,+1 and +1,+1) (like PutString_SprFont3)
	static constexpr TextStyle with_two_point_shadow(const hb::shared::render::Color& c) {
		return TextStyle(c, ShadowStyle::TwoPoint);
	}

	// hb::shared::render::Color with drop shadow (+1,+1)
	static constexpr TextStyle with_drop_shadow(const hb::shared::render::Color& c) {
		return TextStyle(c, ShadowStyle::DropShadow);
	}

	// hb::shared::render::Color with highlight effect (like PutString_SprFont)
	static constexpr TextStyle with_highlight(const hb::shared::render::Color& c) {
		return TextStyle(c, ShadowStyle::Highlight);
	}

	// hb::shared::render::Color with integrated shadow (like PutString_SprFont2)
	static constexpr TextStyle with_integrated_shadow(const hb::shared::render::Color& c) {
		return TextStyle(c, ShadowStyle::Integrated);
	}

	// hb::shared::render::Color with alpha transparency
	static constexpr TextStyle transparent(const hb::shared::render::Color& c, float alpha) {
		return TextStyle(c, alpha);
	}

	// ============== Modifier Methods ==============

	// Create a copy with different shadow style
	constexpr TextStyle with_shadow_style(ShadowStyle s) const {
		return TextStyle(color, alpha, s, fontSize, useAdditive, bold);
	}

	// Create a copy with different alpha
	constexpr TextStyle with_alpha(float a) const {
		return TextStyle(color, a, shadow, fontSize, useAdditive, bold);
	}

	// Create a copy with different font size (ignored for bitmap fonts)
	constexpr TextStyle with_font_size(int size) const {
		return TextStyle(color, alpha, shadow, size, useAdditive, bold);
	}

	// Create a copy with additive blending enabled
	// Use this for bright text (damage numbers) that needs DDraw-like brightness
	constexpr TextStyle with_additive() const {
		return TextStyle(color, alpha, shadow, fontSize, true, bold);
	}

	// Create a copy with bold enabled (TTF only)
	constexpr TextStyle with_bold() const {
		return TextStyle(color, alpha, shadow, fontSize, useAdditive, true);
	}
};

// ============== Bitmap Font Management ==============

// Load and register a bitmap font with explicit character widths
// TextLib takes ownership of the font - no need to store it elsewhere
void load_bitmap_font(int fontId, hb::shared::sprite::ISprite* sprite, char firstChar, char lastChar,
                    int frameOffset, const FontSpacing& spacing);

// Load and register a bitmap font with dynamic spacing (width from sprite frames)
void load_bitmap_font_dynamic(int fontId, hb::shared::sprite::ISprite* sprite, char firstChar, char lastChar,
                           int frameOffset);

// Check if a bitmap font is loaded
bool is_bitmap_font_loaded(int fontId);

// get a registered bitmap font (for internal use)
IBitmapFont* get_bitmap_font(int fontId);

// ============== Text Rendering API ==============

// draw text at position (x, y)
// fontId: FONT_ID_DEFAULT for TTF, or game-defined bitmap font ID
void draw_text(int fontId, int x, int y, const char* text, const TextStyle& style);

// draw text aligned within a rectangle (x, y, width, height) — single line, no wrapping
// Use Align flags: Align::Left, Align::HCenter, Align::Right, Align::Top, Align::VCenter, Align::Bottom
// Combine with bitwise OR: Align::HCenter | Align::VCenter, or use presets: Align::Center
void draw_text_aligned(int fontId, int x, int y, int width, int height, const char* text,
                     const TextStyle& style, Align alignment = Align::TopLeft);

// draw text with word-wrapping within a rectangle — splits long text into multiple lines
void draw_text_wrapped(int fontId, int x, int y, int width, int height, const char* text,
                     const TextStyle& style, Align alignment = Align::TopLeft);

// ============== Text Measurement ==============

// Measure text dimensions
TextMetrics measure_text(int fontId, const char* text);

// get number of characters that fit within maxWidth pixels
int get_fitting_char_count(int fontId, const char* text, int maxWidth);

// get line height for a font
int get_line_height(int fontId);

// get pixel height of text after word-wrapping within maxWidth
int measure_wrapped_text_height(int fontId, const char* text, int maxWidth);

// ============== Batching ==============
// For DDraw, text rendering requires DC acquisition. Wrap multiple text calls
// in Begin/End for better performance. SFML ignores these (no-op).

void begin_batch();
void end_batch();

// RAII helper for automatic batch management
class ScopedBatch
{
public:
	ScopedBatch() { begin_batch(); }
	~ScopedBatch() { end_batch(); }

	// Non-copyable
	ScopedBatch(const ScopedBatch&) = delete;
	ScopedBatch& operator=(const ScopedBatch&) = delete;
};

} // namespace hb::shared::text
