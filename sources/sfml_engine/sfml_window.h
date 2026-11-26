// sfml_window.h: Pure SFML window implementing hb::shared::render::IWindow interface
//
// Part of SFMLEngine static library
// Supports two-phase initialization:
//   1. Allocate (constructor) — store default params
//   2. Configure via setters — write to staged params (no OS window yet)
//   3. realize() — create the OS window from staged params
// After realize(), setters modify the live OS window directly.
//////////////////////////////////////////////////////////////////////

#pragma once

#include "window.h"
#include "input.h"  // For KeyCode enum
#include "output_rects.h"
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <string>
#include <vector>

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

    // Display model
    bool apply_display_settings(const hb::shared::render::display_settings& settings) override;
    const hb::shared::render::display_settings& get_display_settings() const override;
    bool is_window_mode_supported(hb::shared::render::window_mode::type mode) const override;
    hb::shared::render::display_mode get_desktop_mode() const override;
    std::vector<hb::shared::render::resolution_option> get_resolution_options() const override;
    std::vector<hb::shared::render::refresh_option> get_refresh_options() const override;

    // Visibility
    void show() override;
    void hide() override;
    void set_title(const char* title) override;

    // Frame Rate
    void set_background_fps_limit(int limit) override;
    int get_background_fps_limit() const override;

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

    // Coordinate mapping
    void window_to_ui(int window_x, int window_y, int& ui_x, int& ui_y) const override;
    void ui_to_window(int ui_x, int ui_y, int& window_x, int& window_y) const override;
    void clamp_ui(int& ui_x, int& ui_y) const override;

    // Event Handler
    void set_event_handler(hb::shared::render::IWindowEventHandler* handler) override;
    hb::shared::render::IWindowEventHandler* get_event_handler() const override;

    // ============== SFML-Specific Access ==============

    sf::RenderWindow* GetRenderWindow() { return &m_renderWindow; }
    const sf::RenderWindow* GetRenderWindow() const { return &m_renderWindow; }

    // Push the display state the renderer mirrors (letterbox, stretch, vsync, frame cap); called by apply
    // and when a renderer is linked after realize()
    void push_display_state();

private:
    // Create (or recreate) the OS window for m_display; false when the result differs from the request
    bool create_native_window();
    // Exclusive only: switch the monitor to m_display.refresh_rate; false when the driver refuses
    bool apply_exclusive_refresh_rate();
    // Re-point input and the renderer at the recreated window
    void relink_window();
    // The frame cap for the current state: background throttle, else the monitor rate under vsync, else
    // the refresh setting (0 = unlimited)
    int current_frame_cap() const;

    // Convert SFML key to abstract KeyCode
    static KeyCode SfmlKeyToKeyCode(sf::Keyboard::Key key);

    // Transform window coordinates to logical game coordinates (640x480)
    void TransformMouseCoords(int windowX, int windowY, int& logicalX, int& logicalY) const;

    // The world and UI rects for the current client size: the one copy both mapping directions use
    output_rects current_output_rects() const;
    // The range the mouse can reach in UI units for these rects (TransformMouseCoords clamps to it)
    static void clamp_ui(const output_rects& rects, int& ui_x, int& ui_y);

    // Apply cursor grab based on mode: skip in fullscreen (cursor already confined),
    // apply user preference in windowed mode.
    void apply_cursor_grab();

    // Center the window on the monitor it currently occupies.
    // Fullscreen: position at monitor origin. Windowed: center in work area.
    void center_on_current_monitor();

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
    hb::shared::render::display_settings m_display;  // staged before realize, in effect after
    int m_width;   // live client size (m_display holds the configured size)
    int m_height;
    bool m_in_exclusive;  // the monitor is switched to an exclusive mode right now
    mutable hb::shared::render::display_mode m_desktop_cache;  // the desktop mode, kept while exclusive hides it
    bool m_mouse_capture_enabled;
    int m_background_fps_limit;  // 0 = disabled, >0 = throttle to this FPS on focus loss
    hb::shared::types::NativeInstance m_nativeInstance;
    int m_icon_resource_id;
};
