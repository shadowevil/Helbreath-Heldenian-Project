// text_lib_ext.h: Game-side extensions for TextLib
//
// Includes text_lib.h and adds game-specific overloads.
// Game code should include this instead of text_lib.h directly.
//////////////////////////////////////////////////////////////////////

#pragma once

#include "text_lib.h"
#include "common_types.h"

// ============== TextLib Namespace Extensions ==============

namespace hb::shared::text {

// draw_text overload accepting hb::shared::render::Color
inline void draw_text(int fontId, int x, int y, const char* text, const hb::shared::render::Color& color) {
	draw_text(fontId, x, y, text, TextStyle::from_color(color));
}

// draw_text_aligned overload accepting hb::shared::geometry::GameRectangle
inline void draw_text_aligned(int fontId, const hb::shared::geometry::GameRectangle& rect, const char* text,
                            const TextStyle& style, Align alignment = Align::TopLeft) {
	draw_text_aligned(fontId, rect.x, rect.y, rect.width, rect.height, text, style, alignment);
}

// draw_text_aligned overload accepting hb::shared::geometry::GameRectangle + hb::shared::render::Color
inline void draw_text_aligned(int fontId, const hb::shared::geometry::GameRectangle& rect, const char* text,
                            const hb::shared::render::Color& color, Align alignment = Align::TopLeft) {
	draw_text_aligned(fontId, rect, text, TextStyle::from_color(color), alignment);
}

// draw_text_aligned overload accepting hb::shared::render::Color
inline void draw_text_aligned(int fontId, int x, int y, int width, int height, const char* text,
                            const hb::shared::render::Color& color, Align alignment = Align::TopLeft) {
	draw_text_aligned(fontId, x, y, width, height, text, TextStyle::from_color(color), alignment);
}

} // namespace hb::shared::text
