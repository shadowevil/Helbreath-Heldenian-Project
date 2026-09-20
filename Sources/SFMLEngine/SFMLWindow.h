// SFMLWindow.h: Pure SFML window implementing hb::shared::render::IWindow interface
//
// Part of SFMLEngine static library
// Supports two-phase initialization:
//   1. Allocate (constructor) — store default params
//   2. Configure via setters — write to staged params (no OS window yet)
//   3. realize() — create the OS window from staged params
// After realize(), setters modify the live OS window directly.
//////////////////////////////////////////////////////////////////////

#pragma once

#include "IWindow.h"
#include "IInput.h"  // For KeyCode enum
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <string>

class SFMLWindow : public hb::shared::render::IWindow
{
public:
    SFMLWindow();
    virtual ~SFMLWindow();

    // ============== hb::shared::render::IWindow Implementation ==============

    // Lifecycle
    bool realize() override;
    void destroy() override;
    bool is_open() const override;
    void close() override;

    // Properties
    hb::shared::types::NativeWindowHandle get_handle() const override;
    int get_width() const override;
    int get_height() const override;
    bool is_fullscreen() const override;
    bool is_active() const override;

    // Display
    void set_fullscreen(bool fullscreen) override;
    void set_borderless(bool borderless) override;
    bool is_borderless() const override;
    void set_size(int width, int height, bool center = true) override;
    void show() override;
    void hide() override;
    void set_title(const char* title) override;

    // Frame Rate
    void set_framerate_limit(int limit) override;
    int get_framerate_limit() const override;
    void set_vsync_enabled(bool enabled) override;
    bool is_vsync_enabled() const override;
    void set_background_fps_limit(int limit) override;
    int get_background_fps_limit() const override;

    // Scaling
    void set_fullscreen_stretch(bool stretch) override;
    bool is_fullscreen_stretch() const override;

    // Platform (stageable)
    void set_native_instance(hb::shared::types::NativeInstance instance) override;
    void set_icon_resource_id(int id) override;

    // Icon (cross-platform)
    void set_icon(unsigned int width, unsigned int height, const unsigned char* rgba_pixels) override;

    // Cursor
    void set_mouse_cursor_visible(bool visible) override;
    void set_mouse_capture_enabled(bool enabled) override;

    // Dialogs
    void show_message_box(const char* title, const char* message) override;

    // Message Processing
    bool process_messages() override;
    void wait_for_message() override;

    // Event Handler
    void set_event_handler(hb::shared::render::IWindowEventHandler* handler) override;
    hb::shared::render::IWindowEventHandler* get_event_handler() const override;

    // ============== SFML-Specific Access ==============

    sf::RenderWindow* GetRenderWindow() { return &m_renderWindow; }
    const sf::RenderWindow* GetRenderWindow() const { return &m_renderWindow; }

private:
    // Convert SFML key to abstract KeyCode
    static KeyCode SfmlKeyToKeyCode(sf::Keyboard::Key key);

    // Transform window coordinates to logical game coordinates (640x480)
    void TransformMouseCoords(int windowX, int windowY, int& logicalX, int& logicalY) const;

    // Apply cursor grab based on mode: skip in fullscreen (cursor already confined),
    // apply user preference in windowed mode.
    void apply_cursor_grab();

    // Center the window on the monitor it currently occupies.
    // Fullscreen: position at monitor origin. Windowed: center in work area.
    void center_on_current_monitor();

    // Cross-platform helpers — use platform wrappers on Windows, SFML on Linux.
    void get_desktop_size(int& width, int& height) const;
    void move_window(int x, int y, int width, int height);

    // On Windows, clip the OS cursor to the rendered area (letterbox region)
    // in fullscreen non-stretch mode.  No-op on Linux and in windowed mode.
    void update_cursor_clip();

    sf::RenderWindow m_renderWindow;
    hb::shared::types::NativeWindowHandle m_handle;  // Native handle for compatibility
    hb::shared::render::IWindowEventHandler* m_event_handler;
    bool m_realized;
    bool m_open;
    bool m_active;

    // Staged/live params
    std::string m_title;
    int m_width;
    int m_height;
    bool m_fullscreen;
    bool m_fullscreen_stretch;
    bool m_borderless;
    bool m_mouse_capture_enabled;
    bool m_vsync;
    int m_fps_limit;
    int m_background_fps_limit;  // 0 = disabled, >0 = throttle to this FPS on focus loss
    int m_windowed_width;
    int m_windowed_height;
    hb::shared::types::NativeInstance m_nativeInstance;
    int m_icon_resource_id;
};
