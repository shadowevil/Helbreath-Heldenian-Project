// color_utils.h: hb::shared::render::Color conversion utilities for renderer-agnostic color handling
//
// All interfaces use RGB888 format (8 bits per channel, 0-255 range).
// This utility provides conversions from legacy 565/555 formats.
//////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>

namespace hb::shared::color {

// Create RGB888 color from 8-bit components (0-255 each)
// Returns color in 0x00RRGGBB format
inline uint32_t RGB888(uint8_t r, uint8_t g, uint8_t b)
{
    return static_cast<uint32_t>(r) | (static_cast<uint32_t>(g) << 8) | (static_cast<uint32_t>(b) << 16);
}

// Create RGB888 color with alpha from 8-bit components (0-255 each)
// Returns color in 0xAARRGGBB format
inline uint32_t RGBA8888(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
{
    return static_cast<uint32_t>(r) | (static_cast<uint32_t>(g) << 8) |
           (static_cast<uint32_t>(b) << 16) | (static_cast<uint32_t>(a) << 24);
}

// Extract components from RGB888 color (0x00RRGGBB format)
inline void ToComponents(uint32_t color, uint8_t& r, uint8_t& g, uint8_t& b)
{
    r = static_cast<uint8_t>(color & 0xFF);
    g = static_cast<uint8_t>((color >> 8) & 0xFF);
    b = static_cast<uint8_t>((color >> 16) & 0xFF);
}

// Extract components from RGBA8888 color (0xAARRGGBB format)
inline void ToComponentsWithAlpha(uint32_t color, uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& a)
{
    r = static_cast<uint8_t>(color & 0xFF);
    g = static_cast<uint8_t>((color >> 8) & 0xFF);
    b = static_cast<uint8_t>((color >> 16) & 0xFF);
    a = static_cast<uint8_t>((color >> 24) & 0xFF);
}

} // namespace hb::shared::color
