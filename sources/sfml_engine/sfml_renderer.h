// sfml_renderer.h: SFML renderer implementing hb::shared::render::IRenderer interface
//
// Part of SFMLEngine static library
// World pass draws into an sf::RenderTexture; screen pass draws straight to the sf::RenderWindow
//////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer.h"
#include "sfml_texture.h"
#include "output_rects.h"
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Graphics/Shader.hpp>
#include <vector>
#include <string>
#include <chrono>

class SFMLRenderer : public hb::shared::render::IRenderer
{
public:
    SFMLRenderer();
    virtual ~SFMLRenderer();

    // ============== hb::shared::render::IRenderer Implementation ==============

    // Initialization
    bool init(hb::shared::types::NativeWindowHandle hWnd) override;
    void shutdown() override;

    // Display Modes (mirrored from the window's display model)
    bool is_fullscreen() const override;
    bool is_fullscreen_stretch() const override;

    // Scaling
    void set_upscale_smoothing(bool enabled) override;
    bool is_upscale_smoothing() const override;

    // Frame Management
    void begin_frame() override;
    void end_frame() override;
    bool end_frame_check_lost_surface() override;
    bool was_frame_presented() const override;

    // World / Screen Space
    void begin_world() override;
    void end_world() override;
    void fill_output(const hb::shared::render::Color& color) override;
    float ui_scale() const override;
    hb::shared::render::screen_extent get_screen_extent() const override;
    void push_world_rect_view() override;
    void pop_world_rect_view() override;
    hb::shared::geometry::GamePoint ui_to_world(hb::shared::geometry::GamePoint ui) const override;
    hb::shared::geometry::GamePoint world_to_ui(hb::shared::geometry::GamePoint world) const override;

    // Frame Metrics
    uint32_t get_fps() const override;
    double get_delta_time() const override;
    double get_delta_time_ms() const override;

    // Primitive Rendering
    void draw_pixel(int x, int y, const hb::shared::render::Color& color) override;
    void draw_line(int x0, int y0, int x1, int y1, const hb::shared::render::Color& color,
                  hb::shared::render::BlendMode blend = hb::shared::render::BlendMode::Alpha) override;
    void draw_rect_filled(int x, int y, int w, int h, const hb::shared::render::Color& color) override;
    void draw_rect_filled_to_edges(int x, int y, int w, int h, const hb::shared::render::Color& color) override;
    void draw_rect_outline(int x, int y, int w, int h, const hb::shared::render::Color& color,
                         int thickness = 1) override;
    void draw_rounded_rect_filled(int x, int y, int w, int h, int radius,
                               const hb::shared::render::Color& color) override;
    void draw_rounded_rect_outline(int x, int y, int w, int h, int radius,
                                const hb::shared::render::Color& color, int thickness = 1) override;

    // Text Rendering
    void begin_text_batch() override;
    void end_text_batch() override;
    void draw_text(int x, int y, const char* text, const hb::shared::render::Color& color) override;
    void draw_text_rect(const hb::shared::geometry::GameRectangle& rect, const char* text, const hb::shared::render::Color& color) override;

    // Surface/Texture Management
    hb::shared::render::ITexture* create_texture(uint16_t width, uint16_t height) override;
    hb::shared::render::ITexture* create_texture_from_file(const char* path, hb::shared::render::texture_filter filter) override;
    void destroy_texture(hb::shared::render::ITexture* texture) override;
    void draw_texture_region(hb::shared::render::ITexture* texture, float src_x, float src_y, float src_w, float src_h,
                             float dst_x, float dst_y, float dst_w, float dst_h, float alpha) override;
    void draw_texture_region(hb::shared::render::ITexture* texture, float src_x, float src_y, float src_w, float src_h,
                             float dst_x, float dst_y, float dst_w, float dst_h, float alpha,
                             const hb::shared::render::draw_tint& tint) override;

    // Clip Area
    void set_clip_area(int x, int y, int w, int h) override;
    hb::shared::geometry::GameRectangle get_clip_area() const override;

    // screenshot
    bool screenshot(const char* filename, const std::function<void()>& overlay = {}) override;

    // Resolution
    int get_width() const override;
    int get_height() const override;
    int get_width_mid() const override;
    int get_height_mid() const override;
    void resize_back_buffer(int width, int height) override;

    // Ambient Light
    char get_ambient_light_level() const override;
    void set_ambient_light_level(char level) override;

    // hb::shared::render::Color Utilities
    void color_transfer_rgb(uint32_t rgb, int* outR, int* outG, int* outB) override;

    // Text Measurement
    int get_text_length(const char* text, int maxWidth) override;
    int get_text_width(const char* text) override;

    // Surface Operations
    void* get_back_buffer_native() override;

    // Native Access
    void* get_native_renderer() override;

    // ============== SFML-Specific Access ==============

    // The target every draw goes to: the world target inside begin_world/end_world, else the window
    // through the UI view. Sprites, text and primitives must draw here, never to a fixed target.
    sf::RenderTarget* get_current_target() { return m_target; }

    // Window pixels per logical unit for text drawn now: the screen-space rect in force (the UI rect,
    // or the world rect inside push/pop_world_rect_view), 1 in the world pass
    float text_scale() const { return m_in_world ? 1.0f : rect_scale(screen_rect()); }

    // Screen space: move a logical (UI-unit) point so it lands on a whole window pixel through the
    // UI view, using the same integer viewport SFML rasterises with. Identity when the UI is 1:1.
    sf::Vector2f snap_to_output_pixel(sf::Vector2f logical) const;
    // Screen space: window pixels per logical unit on each axis through that same integer viewport
    // (the axes differ when the UI is stretched); (0, 0) when the viewport is empty
    sf::Vector2f output_pixels_per_unit() const;

    // True when a draw now goes to screen space, the UI is scaled away from 1:1 and
    // upscaleSmoothing is on: sprites should sample bilinear for that draw, like the world composite
    bool wants_ui_smoothing() const
    {
        return m_upscale_smoothing && !m_in_world
            && (screen_rect().size.x != static_cast<float>(m_width) || screen_rect().size.y != static_cast<float>(m_height));
    }

    // Set the window to render to (called from SFMLWindow)
    // This also creates the render textures since they need an OpenGL context
    void SetRenderWindow(sf::RenderWindow* window);

    // Additive offset shader for PutTransSpriteRGB emulation
    const sf::Shader* get_additive_offset_shader() const;

    // The display state SFMLWindow mirrors here (SFMLWindow::push_display_state)
    void set_fullscreen(bool fullscreen);
    void set_fullscreen_stretch(bool stretch);

    // Engine-owned frame rate control (called from SFMLWindow)
    void SetFramerateLimit(int limit);
    int GetFramerateLimit() const;
    void SetVSyncMode(bool enabled);

private:
    // Create render textures (called when OpenGL context is available)
    bool CreateRenderTextures();

    // Window client size in physical pixels
    sf::Vector2f get_output_size() const;
    // Recompute the output size, world rect and UI rect for this frame
    void update_output_rects();
    // Screen space: the logical region the view shows (0..logical, or wider when the UI rect is
    // narrower than the world rect) and the window rect it lands in
    struct screen_mapping
    {
        sf::FloatRect view;
        sf::FloatRect viewport;
    };
    screen_mapping current_screen_mapping() const;
    // The same for a given screen rect (the UI rect, or the world rect inside push_world_rect_view)
    screen_mapping screen_mapping_for(const sf::FloatRect& rect) const;
    // Logical units of one rect -> logical units of another, through window pixels; identity when equal
    hb::shared::geometry::GamePoint map_between(const sf::FloatRect& from, const sf::FloatRect& to,
        hb::shared::geometry::GamePoint point) const;
    // Window pixels per logical unit of rect, on the smaller axis
    float rect_scale(const sf::FloatRect& rect) const;
    // 1:1 view over the window in output pixels, no scissor
    sf::View output_view() const;
    // Make the window the current target, drawing through the UI view
    void route_to_screen();
    // After the window is set or recreated: recompute rects and re-route screen-space draws
    void refresh_routing();
    // Apply the current target's view and clip (as a view scissor, rounded outward to whole pixels)
    void apply_target_view();
    // The full logical area (0, 0, m_width, m_height): every target's default clip
    hb::shared::geometry::GameRectangle logical_area() const { return hb::shared::geometry::GameRectangle(0, 0, m_width, m_height); }
    hb::shared::geometry::GameRectangle& active_clip();
    const hb::shared::geometry::GameRectangle& active_clip() const;

    sf::RenderWindow* m_render_window;  // Not owned, set by SFMLWindow
    bool m_texturesCreated;
    sf::RenderTexture m_world_target;   // Logical-size world pass, composited in end_world

    // Draw routing
    sf::RenderTarget* m_target;         // Current target: &m_world_target or m_render_window
    bool m_in_world;
    sf::Vector2f m_output_size;         // Window client size in pixels, for this frame
    sf::FloatRect m_world_rect;         // Where the world target lands in the window (pixels)
    sf::FloatRect m_ui_rect;            // Where the 800x600 UI space lands in the window (pixels)
    int m_world_rect_view_depth = 0;    // >0: screen-space draws map 800x600 onto m_world_rect (HUD)
    // Where screen-space draws land now: the world rect inside push/pop_world_rect_view, else the UI rect
    const sf::FloatRect& screen_rect() const { return m_world_rect_view_depth > 0 ? m_world_rect : m_ui_rect; }

    // Display properties
    int m_width;
    int m_height;
    bool m_fullscreen;
    bool m_fullscreen_stretch;
    bool m_upscale_smoothing;
    hb::shared::geometry::GameRectangle m_world_clip;   // Logical units, world target
    hb::shared::geometry::GameRectangle m_screen_clip;  // Logical units, UI space on the window

    // Engine-owned frame timing
    int m_fps_limit;
    bool m_vsync;
    bool m_skip_frame;
    std::chrono::steady_clock::time_point m_lastPresentTime;
    std::chrono::steady_clock::duration m_targetFrameDuration;

    // Frame metrics (tracked at actual present)
    uint32_t m_fps;
    uint32_t m_framesThisSecond;
    double m_delta_time;
    double m_fps_accumulator;
    std::chrono::steady_clock::time_point m_lastPresentedFrameTime;

    // Ambient light level
    char m_ambient_light_level;

    // Additive offset shader (PutTransSpriteRGB emulation)
    sf::Shader m_additive_offset_shader;
    bool m_additive_offset_shader_loaded = false;

};
