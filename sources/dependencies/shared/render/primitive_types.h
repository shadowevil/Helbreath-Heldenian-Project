// primitive_types.h: Shared types for primitive rendering
//
// Color and BlendMode used by IRenderer primitive drawing methods.
//////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>

namespace hb::shared::render {

struct Color
{
	uint8_t r = 0, g = 0, b = 0, a = 255;

	constexpr Color() = default;
	constexpr Color(uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_ = 255)
		: r(r_), g(g_), b(b_), a(a_) {}

	static constexpr Color Black(uint8_t a = 255) { return {0, 0, 0, a}; }
	static constexpr Color White(uint8_t a = 255) { return {255, 255, 255, a}; }
};

enum class BlendMode : uint8_t
{
	Alpha,      // Normal alpha blending (default)
	Additive    // Additive blending (glow effects)
};

// Sampling of a texture loaded with IRenderer::create_texture_from_file.
// smooth: bilinear with mipmaps (scaled art). nearest: no filtering and no mipmaps, so stretched
// 1 px rows and 9-slice edges stay sharp and atlas neighbours never bleed in (pixel-art kits).
enum class texture_filter : uint8_t
{
	smooth,
	nearest
};

// Colour and blend of an IRenderer::draw_texture_region call. The colour multiplies every texel
// (white = unchanged; tint white masks, not black ones) and its alpha multiplies the draw's alpha.
// Alpha blending draws over the target; additive adds texel * colour * alpha to it.
struct draw_tint
{
	Color color = Color::White();
	BlendMode blend = BlendMode::Alpha;
};

// The window's visible area in UI units (IRenderer::get_screen_extent); fractional in stretch
struct screen_extent
{
	float x = 0.0f;
	float y = 0.0f;
	float width = 0.0f;
	float height = 0.0f;
};

// A source image of art_w x art_h pixels fitted to cover an extent, centred (the overflow is cropped):
// UI units = origin + art pixels * scale
struct art_fit
{
	float scale = 1.0f;
	float origin_x = 0.0f;
	float origin_y = 0.0f;

	float x(float art_x) const { return origin_x + art_x * scale; }
	float y(float art_y) const { return origin_y + art_y * scale; }
};

inline art_fit cover_fit(const screen_extent& extent, float art_w, float art_h)
{
	art_fit fit;
	fit.scale = (extent.width / art_w > extent.height / art_h) ? extent.width / art_w : extent.height / art_h;
	fit.origin_x = extent.x + (extent.width - art_w * fit.scale) / 2.0f;
	fit.origin_y = extent.y + (extent.height - art_h * fit.scale) / 2.0f;
	return fit;
}

} // namespace hb::shared::render
