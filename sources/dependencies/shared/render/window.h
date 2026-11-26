// window.h: Abstract interface for window management
//
// This interface allows different renderers to create windows appropriate
// for their backend (DirectDraw, OpenGL, Vulkan, etc.)
//////////////////////////////////////////////////////////////////////

#pragma once

#include "native_types.h"
#include "display_settings.h"
#include <cstdint>
#include <vector>

// Forward declare KeyCode enum
enum class KeyCode : int;

namespace hb::shared::render {

// Forward declaration
class IWindowEventHandler;

// Abstract window interface
class IWindow
{
public:
    virtual ~IWindow() = default;

    // ============== Lifecycle ==============
    virtual bool realize() = 0;   // Creates the OS window from staged params
    virtual void destroy() = 0;
    virtual bool is_open() const = 0;
    virtual void close() = 0;  // Request window close (triggers close event)

    // ============== Properties ==============
    virtual hb::shared::types::NativeWindowHandle get_handle() const = 0;
    virtual int get_width() const = 0;
    virtual int get_height() const = 0;
    virtual bool is_fullscreen() const = 0;  // a fullscreen window mode is in effect
    virtual bool is_active() const = 0;

    // ============== Display model (d-038) ==============
    // The one path that changes how the window is presented. Before realize() it stages the settings and
    // realize() creates the window from them; after, it changes only what differs: mode, size, exclusive
    // rate or decoration rebuild the window, while vsync, the frame cap and the aspect change in place.
    // A mode the platform does not support is folded (exclusive -> borderless on Linux). Returns true when
    // the request took effect exactly; false when the backend had to fall back (a size or rate the monitor
    // does not offer, a failed mode switch). get_display_settings() always reports what is in effect.
    virtual bool apply_display_settings(const display_settings& settings) = 0;
    virtual const display_settings& get_display_settings() const = 0;
    virtual bool is_window_mode_supported(window_mode::type mode) const = 0;

    // The monitor's own mode (refresh_rate 0 when the platform cannot tell); valid before realize()
    virtual display_mode get_desktop_mode() const = 0;
    // The curated sizes between the logical size and the desktop, plus the desktop, labelled
    virtual std::vector<resolution_option> get_resolution_options() const = 0;
    // Unlimited (0) first, then the curated rates plus the driver's, labelled
    virtual std::vector<refresh_option> get_refresh_options() const = 0;

    // ============== Visibility ==============
    virtual void show() = 0;
    virtual void hide() = 0;
    virtual void set_title(const char* title) = 0;

    // ============== Frame Rate ==============
    // Background FPS: when set (> 0), the window automatically throttles to
    // this rate on focus loss and restores the display model's cap on focus gain.
    // Set to 0 to disable background throttling (default).
    virtual void set_background_fps_limit(int limit) = 0;
    virtual int get_background_fps_limit() const = 0;

    // ============== Platform (stageable — set before realize) ==============
    virtual void set_native_instance(hb::shared::types::NativeInstance instance) = 0;
    virtual void set_icon_resource_id(int id) = 0;

    // ============== Icon (cross-platform) ==============
    // Sets the window icon from raw RGBA pixel data (used on Linux/macOS).
    // On Windows the icon is embedded via the .rc resource file instead.
    virtual void set_icon(unsigned int width, unsigned int height, const unsigned char* rgba_pixels) = 0;

    // ============== Cursor ==============
    virtual void set_mouse_cursor_visible(bool visible) = 0;
    virtual void set_mouse_capture_enabled(bool enabled) = 0;

    // ============== Dialogs ==============
    virtual void show_message_box(const char* title, const char* message) = 0;

    // ============== Message Processing ==============
    // Process pending messages, returns false if WM_QUIT received
    virtual bool process_messages() = 0;

    // Wait for a message (used when inactive)
    virtual void wait_for_message() = 0;

    // ============== Coordinate Mapping ==============
    // Window pixels -> UI units: exactly the mapping real mouse input goes through (clamped the same way)
    virtual void window_to_ui(int window_x, int window_y, int& ui_x, int& ui_y) const = 0;
    // UI units -> window pixels through the same rects (unclamped): the first pixel window_to_ui maps into
    // the unit, exact at fractional scales
    virtual void ui_to_window(int ui_x, int ui_y, int& window_x, int& window_y) const = 0;
    // Clamp a UI point to the range real mouse input can reach (window_to_ui applies the same clamp)
    virtual void clamp_ui(int& ui_x, int& ui_y) const = 0;

    // ============== Event Handler ==============
    virtual void set_event_handler(IWindowEventHandler* handler) = 0;
    virtual IWindowEventHandler* get_event_handler() const = 0;
};

// Window event callback interface
// Implement this to receive window events
class IWindowEventHandler
{
public:
    virtual ~IWindowEventHandler() = default;

    // ============== Window Events ==============
    virtual void on_close() = 0;
    virtual void on_destroy() = 0;
    virtual void on_activate(bool active) = 0;
    virtual void on_resize(int width, int height) = 0;

    // ============== Input Events ==============
    virtual void on_key_down(KeyCode key) = 0;
    virtual void on_key_up(KeyCode key) = 0;
    virtual void on_char(char character) = 0;
    virtual void on_mouse_move(int x, int y) = 0;
    virtual void on_mouse_button_down(int button, int x, int y) = 0;
    virtual void on_mouse_button_up(int button, int x, int y) = 0;
    virtual void on_mouse_wheel(int delta, int x, int y) = 0;

    // ============== Custom Messages ==============
    // For game-specific messages (sockets, timers, etc.)
    // Return true if handled, false to pass to DefWindowProc
    virtual bool on_custom_message(uint32_t message, uintptr_t wparam, intptr_t lparam) = 0;

    // ============== Text Input ==============
    // For IME and text composition
    virtual bool on_text_input(hb::shared::types::NativeWindowHandle hwnd, uint32_t message, uintptr_t wparam, intptr_t lparam) = 0;
};

// Mouse button constants: see hb::shared::input::MouseButton in input.h

} // namespace hb::shared::render
