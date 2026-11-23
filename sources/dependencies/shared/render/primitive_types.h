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

} // namespace hb::shared::render
