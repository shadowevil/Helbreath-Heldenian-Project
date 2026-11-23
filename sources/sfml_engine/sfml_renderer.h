// sfml_renderer.h: SFML renderer implementing hb::shared::render::IRenderer interface
//
// Part of SFMLEngine static library
// Uses sf::RenderTexture as back buffer and renders to sf::RenderWindow
//////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer.h"
#include "sfml_texture.h"
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

    // Display Modes
    void set_fullscreen(bool fullscreen) override;
    bool is_fullscreen() const override;
    void change_display_mode(hb::shared::types::NativeWindowHandle hWnd) override;

    // Scaling
    void set_fullscreen_stretch(bool stretch) override;
    bool is_fullscreen_stretch() const override;
    void set_upscale_smoothing(bool enabled) override;
    bool is_upscale_smoothing() const override;

    // Frame Management
    void begin_frame() override;
    void end_frame() override;
    bool end_frame_check_lost_surface() override;
    bool was_frame_presented() const override;

    // Frame Metrics
    uint32_t get_fps() const override;
    double get_delta_time() const override;
    double get_delta_time_ms() const override;

    // Primitive Rendering
    void draw_pixel(int x, int y, const hb::shared::render::Color& color) override;
    void draw_line(int x0, int y0, int x1, int y1, const hb::shared::render::Color& color,
                  hb::shared::render::BlendMode blend = hb::shared::render::BlendMode::Alpha) override;
    void draw_rect_filled(int x, int y, int w, int h, const hb::shared::render::Color& color) override;
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
    hb::shared::render::ITexture* create_texture_from_file(const char* path) override;
    void destroy_texture(hb::shared::render::ITexture* texture) override;

    // Clip Area
    void set_clip_area(int x, int y, int w, int h) override;
    hb::shared::geometry::GameRectangle get_clip_area() const override;

    // screenshot
    bool screenshot(const char* filename) override;

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

    // get the back buffer render texture for sprite drawing
    sf::RenderTexture* GetBackBuffer() { return &m_backBuffer; }

    // Set the window to render to (called from SFMLWindow)
    // This also creates the render textures since they need an OpenGL context
    void SetRenderWindow(sf::RenderWindow* window);

    // Additive offset shader for PutTransSpriteRGB emulation
    const sf::Shader* get_additive_offset_shader() const;

    // Engine-owned frame rate control (called from SFMLWindow)
    void SetFramerateLimit(int limit);
    int GetFramerateLimit() const;
    void SetVSyncMode(bool enabled);

private:
    // Create render textures (called when OpenGL context is available)
    bool CreateRenderTextures();

    sf::RenderWindow* m_render_window;  // Not owned, set by SFMLWindow
    bool m_texturesCreated;
    sf::RenderTexture m_backBuffer;

    // Display properties
    int m_width;
    int m_height;
    bool m_fullscreen;
    bool m_fullscreen_stretch;
    bool m_upscale_smoothing;
    hb::shared::geometry::GameRectangle m_clipArea;

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
