// texture.h: Abstract interface for texture/surface objects
//
// This interface wraps platform-specific texture implementations (SFML, etc.)
//////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>

namespace hb::shared::render {

class ITexture
{
public:
	virtual ~ITexture() = default;

	virtual uint16_t get_width() const = 0;
	virtual uint16_t get_height() const = 0;

	// Render-target operations (for off-screen rendering e.g. minimap)
	virtual void begin_render() = 0;
	virtual void end_render() = 0;

	// Set a world-space view that maps to the texture (for scaled rendering)
	// origin_x/y is the top-left corner of the visible area
	virtual void set_view(float origin_x, float origin_y, float width, float height) = 0;
	virtual void reset_view() = 0;

	// Get opaque handle for DrawToSurface() calls
	virtual void* get_render_surface() = 0;

	// Clear to a color
	virtual void clear(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) = 0;

	// Enable/disable bilinear filtering (smooth scaling)
	virtual void set_smooth(bool smooth) = 0;
};

} // namespace hb::shared::render
