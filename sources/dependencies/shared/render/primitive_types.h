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

} // namespace hb::shared::render
