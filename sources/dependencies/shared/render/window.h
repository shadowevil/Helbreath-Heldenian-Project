// window.h: Abstract interface for window management
//
// This interface allows different renderers to create windows appropriate
// for their backend (DirectDraw, OpenGL, Vulkan, etc.)
//////////////////////////////////////////////////////////////////////

#pragma once

#include "native_types.h"
#include <cstdint>

// Forward declare KeyCode enum
enum class KeyCode : int;

namespace hb::shared::render {

// Forward declaration
class IWindowEventHandler;

// Window creation parameters (staged before realize, applied on realize)
struct window_params
{
    const char* title = "Application";
    int width = 800;
    int height = 600;
    bool fullscreen = false;
    bool borderless = false;
    bool centered = true;
    bool mouse_capture_enabled = false;
    hb::shared::types::NativeInstance native_instance = {};
    int icon_resource_id = 0;
};

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
    virtual bool is_fullscreen() const = 0;
    virtual bool is_active() const = 0;

    // ============== Display ==============
    virtual void set_fullscreen(bool fullscreen) = 0;
    virtual void set_borderless(bool borderless) = 0;
    virtual bool is_borderless() const = 0;
    virtual void set_size(int width, int height, bool center = true) = 0;  // Resize window
    virtual void show() = 0;
    virtual void hide() = 0;
    virtual void set_title(const char* title) = 0;

    // ============== Frame Rate ==============
    virtual void set_framerate_limit(int limit) = 0;  // 0 = unlimited
    virtual int get_framerate_limit() const = 0;
    virtual void set_vsync_enabled(bool enabled) = 0;
    virtual bool is_vsync_enabled() const = 0;

    // Background FPS: when set (> 0), the window automatically throttles to
    // this rate on focus loss and restores the user's limit on focus gain.
    // Set to 0 to disable background throttling (default).
    virtual void set_background_fps_limit(int limit) = 0;
    virtual int get_background_fps_limit() const = 0;

    // ============== Scaling ==============
    virtual void set_fullscreen_stretch(bool stretch) = 0;
    virtual bool is_fullscreen_stretch() const = 0;

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
