// renderer.h: Abstract interface for renderer backends
//
// Part of DDrawEngine static library
// This interface allows swapping renderer implementations (DirectDraw, OpenGL, etc.)
//////////////////////////////////////////////////////////////////////

#pragma once

#include "native_types.h"
#include "game_geometry.h"
#include "primitive_types.h"
#include <cstdint>
#include <functional>

#include "render_constants.h"

namespace hb::shared::render {

// Forward declarations
class ITexture;
class IRenderer
{
public:
    virtual ~IRenderer() = default;

    // ============== Initialization ==============
    virtual bool init(hb::shared::types::NativeWindowHandle hWnd) = 0;
    virtual void shutdown() = 0;

    // ============== Display Modes ==============
    virtual void set_fullscreen(bool fullscreen) = 0;
    virtual bool is_fullscreen() const = 0;
    virtual void change_display_mode(hb::shared::types::NativeWindowHandle hWnd) = 0;

    // ============== Scaling ==============
    virtual void set_fullscreen_stretch(bool stretch) = 0;
    virtual bool is_fullscreen_stretch() const = 0;

    // Upscale filtering at final present (true = smooth/bilinear, false = crisp/nearest)
    virtual void set_upscale_smoothing(bool enabled) { (void)enabled; }
    virtual bool is_upscale_smoothing() const { return true; }

    // ============== Frame Management ==============
    virtual void begin_frame() = 0;      // ClearBackBuffer
    virtual void end_frame() = 0;        // Flip/Present
    virtual bool end_frame_check_lost_surface() = 0;  // Flip and return true if surface lost
    virtual bool was_frame_presented() const { return true; }  // False if frame was skipped by limiter

    // ============== World / Screen Space ==============
    // Draws go to one "current target". After begin_frame they go to the window through the UI view,
    // which maps the logical 800x600 UI space onto the UI rect (screen space). Letterbox: the world rect
    // and the UI rect are the same uniform centred rect. Stretch: the world rect is the whole window and
    // the UI rect stays uniform and centred; UI-unit draws outside 0..800x600 still reach the window.
    // begin_world() routes draws to the logical-size world target until end_world(), which composites
    // that target onto the window in the world rect and routes back to screen space.
    // Screens with no world (menus, login, loading) never call begin_world.
    // end_world is a no-op when no world pass is open; end_frame and fill_output close one left open.
    virtual void begin_world() = 0;
    virtual void end_world() = 0;
    // Fill the whole window in output pixels (fades, dims), ignoring the UI view and clip.
    virtual void fill_output(const Color& color) = 0;
    // Window pixels per UI unit (the UI rect's uniform scale).
    virtual float ui_scale() const = 0;
    // The whole window in UI units, for screen-space draws: 0,0 to the logical size when the UI fills
    // the window or is letterboxed; wider (or taller) when the window extends past the UI rect (stretch,
    // a widescreen window), e.g. about -133..933 x 0..600 in a 16:9 window.
    virtual screen_extent get_screen_extent() const = 0;
    // UI units <-> world-target units through the UI rect and the world rect (the window between them).
    // Identity when the two rects coincide (letterbox, or a window with the logical aspect). Results are
    // not clamped: in stretch the world spans the whole window, beyond the UI rect.
    virtual hb::shared::geometry::GamePoint ui_to_world(hb::shared::geometry::GamePoint ui) const = 0;
    virtual hb::shared::geometry::GamePoint world_to_ui(hb::shared::geometry::GamePoint world) const = 0;
    // Screen space: draws between push and pop map the 800x600 UI space onto the world rect instead of
    // the UI rect, so that block (the HUD action bar) takes the same share of the window as at 800x600.
    // Calls nest; begin_frame resets the depth. No effect on draws inside a world pass.
    virtual void push_world_rect_view() = 0;
    virtual void pop_world_rect_view() = 0;

    // ============== Frame Metrics ==============
    // Tracked internally by the engine at the point of actual present
    virtual uint32_t get_fps() const { return 0; }
    virtual double get_delta_time() const { return 0.0; }     // Seconds between presented frames
    virtual double get_delta_time_ms() const { return 0.0; }   // Milliseconds between presented frames

    // ============== Primitive Rendering ==============
    // All rect-based primitives use (x, y, w, h) format, consistent with hb::shared::geometry::GameRectangle.
    // draw_line uses (x0, y0, x1, y1) endpoint format.

    virtual void draw_pixel(int x, int y, const Color& color) = 0;
    virtual void draw_line(int x0, int y0, int x1, int y1, const Color& color,
                          BlendMode blend = BlendMode::Alpha) = 0;
    virtual void draw_rect_filled(int x, int y, int w, int h, const Color& color) = 0;
    // draw_rect_filled for full-width UI fills (dims, bars): each edge on the UI area's edge (x or y <= 0,
    // right or bottom >= the logical maximum) extends to the window's edge when the UI rect is narrower
    // than the world rect (stretch). Identical to draw_rect_filled when the rects coincide or in a world pass.
    virtual void draw_rect_filled_to_edges(int x, int y, int w, int h, const Color& color) = 0;
    virtual void draw_rect_outline(int x, int y, int w, int h, const Color& color,
                                 int thickness = 1) = 0;
    virtual void draw_rounded_rect_filled(int x, int y, int w, int h, int radius,
                                       const Color& color) = 0;
    virtual void draw_rounded_rect_outline(int x, int y, int w, int h, int radius,
                                        const Color& color, int thickness = 1) = 0;

    // ============== Text Rendering ==============
    virtual void begin_text_batch() = 0;      // GetBackBufferDC
    virtual void end_text_batch() = 0;        // ReleaseBackBufferDC
    virtual void draw_text(int x, int y, const char* text, const Color& color) = 0;
    virtual void draw_text_rect(const hb::shared::geometry::GameRectangle& rect, const char* text, const Color& color) = 0;

    // ============== Surface/Texture Management ==============
    virtual ITexture* create_texture(uint16_t width, uint16_t height) = 0;
    // Load an image for display only: smooth = bilinear with mipmaps, nearest = unfiltered, no mipmaps
    virtual ITexture* create_texture_from_file(const char* path,
                                               texture_filter filter = texture_filter::smooth) = 0;
    virtual void destroy_texture(ITexture* texture) = 0;
    // Draw part of a texture from create_texture_from_file on the current target, sampled with the
    // texture's own filtering. src_* in texture pixels, dst_* in logical units.
    // alpha is opacity (1 = opaque), the same meaning as DrawParams::m_alpha.
    virtual void draw_texture_region(ITexture* texture, float src_x, float src_y, float src_w, float src_h,
                                     float dst_x, float dst_y, float dst_w, float dst_h, float alpha = 1.0f) = 0;
    // The same with a vertex colour and blend mode (see draw_tint). A negative src_w or src_h mirrors
    // the region on that axis (src_x + src_w is then the region's left edge).
    virtual void draw_texture_region(ITexture* texture, float src_x, float src_y, float src_w, float src_h,
                                     float dst_x, float dst_y, float dst_w, float dst_h, float alpha,
                                     const draw_tint& tint) = 0;

    // ============== Blitting ==============
    // Note: For transition, we keep direct surface access methods
    // Future implementations may need to adapt these

    // ============== Clip Area ==============
    // Per target, in logical units. Each target's clip resets to its full logical area at begin_frame.
    virtual void set_clip_area(int x, int y, int w, int h) = 0;
    virtual hb::shared::geometry::GameRectangle get_clip_area() const = 0;

    // ============== screenshot ==============
    // Captures the window as drawn so far this frame; call it after the last draw and before end_frame.
    // overlay, if set, draws onto the captured image only (in UI units, like screen space), never
    // onto the live frame: e.g. a timestamp.
    virtual bool screenshot(const char* filename, const std::function<void()>& overlay = {}) = 0;

    // ============== Resolution ==============
    virtual int get_width() const = 0;
    virtual int get_height() const = 0;
    virtual int get_width_mid() const = 0;
    virtual int get_height_mid() const = 0;
    virtual void resize_back_buffer(int width, int height) = 0;  // Resize back buffer for resolution change

    // ============== Ambient Light ==============
    virtual char get_ambient_light_level() const = 0;
    virtual void set_ambient_light_level(char level) = 0;

    // ============== Color Utilities ==============
    virtual void color_transfer_rgb(uint32_t rgb, int* outR, int* outG, int* outB) = 0;

    // ============== Text Measurement ==============
    virtual int get_text_length(const char* text, int maxWidth) = 0;  // get character count that fits in width
    virtual int get_text_width(const char* text) = 0;  // get pixel width of text string

    // ============== Surface Operations ==============
    virtual void* get_back_buffer_native() = 0;   // Returns the native world target (sf::RenderTexture* for SFML)

    // ============== Native Access (for legacy sprite code) ==============
    // Returns platform-specific renderer handle for code that can't use the interface yet
    // DirectDraw: returns DXC_ddraw*
    virtual void* get_native_renderer() = 0;
};

} // namespace hb::shared::render
