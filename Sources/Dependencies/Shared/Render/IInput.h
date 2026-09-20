// IInput.h: Raylib-style input interface
//
// Provides static global access to input state with frame-based semantics.
// Each engine (DDraw, SFML) implements its own input handling.
//////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace hb::shared::input
{
namespace MouseButton
{
	enum : int { Left = 0, Right = 1, Middle = 2 };
}
} // namespace hb::shared::input


// ============== Abstract Key Codes ==============
// Engine-agnostic key codes - each engine maps its native keys to these
// This avoids Windows-specific VK_* codes in game logic
enum class KeyCode : int
{
    // Invalid/unknown key
    Unknown = 0,

    // Letters (A-Z: 65-90, matches ASCII)
    A = 'A', B = 'B', C = 'C', D = 'D', E = 'E', F = 'F', G = 'G', H = 'H',
    I = 'I', J = 'J', K = 'K', L = 'L', M = 'M', N = 'N', O = 'O', P = 'P',
    Q = 'Q', R = 'R', S = 'S', T = 'T', U = 'U', V = 'V', W = 'W', X = 'X',
    Y = 'Y', Z = 'Z',

    // Numbers (0-9: 48-57, matches ASCII)
    Num0 = '0', Num1 = '1', Num2 = '2', Num3 = '3', Num4 = '4',
    Num5 = '5', Num6 = '6', Num7 = '7', Num8 = '8', Num9 = '9',

    // Function keys (start at 112 to match VK_F1)
    F1 = 112, F2 = 113, F3 = 114, F4 = 115, F5 = 116, F6 = 117,
    F7 = 118, F8 = 119, F9 = 120, F10 = 121, F11 = 122, F12 = 123,

    // Navigation (match Windows VK codes for compatibility)
    Insert = 45,
    Delete = 46,
    Home = 36,
    End = 35,
    PageUp = 33,
    PageDown = 34,

    // Arrow keys
    Left = 37,
    Up = 38,
    Right = 39,
    Down = 40,

    // Special keys
    Escape = 27,
    Enter = 13,
    Tab = 9,
    Backspace = 8,
    Space = 32,

    // Generic modifiers (used for filtering, not typically sent by SFML)
    Shift = 16,      // VK_SHIFT
    Control = 17,    // VK_CONTROL
    Alt = 18,        // VK_MENU

    // Specific modifiers
    LShift = 160,
    RShift = 161,
    LControl = 162,
    RControl = 163,
    LAlt = 164,
    RAlt = 165,

    // Numpad
    Numpad0 = 96, Numpad1 = 97, Numpad2 = 98, Numpad3 = 99, Numpad4 = 100,
    Numpad5 = 101, Numpad6 = 102, Numpad7 = 103, Numpad8 = 104, Numpad9 = 105,

    // Numpad operators
    NumpadMultiply = 106,  // *
    NumpadAdd = 107,       // +
    NumpadSeparator = 108,
    NumpadSubtract = 109,  // -
    NumpadDecimal = 110,   // .
    NumpadDivide = 111,    // /

    // Lock keys
    NumLock = 144,
    ScrollLock = 145,

    // Windows keys
    LWin = 91,
    RWin = 92,

    // OEM keys
    Grave = 192,   // ` / ~ (VK_OEM_3)
    Hyphen = 189,  // - / _ (VK_OEM_MINUS)
    Equal = 187,   // = / + (VK_OEM_PLUS)
};

// Abstract input interface
class IInput
{
public:
    virtual ~IInput() = default;

    // ============== Frame Management ==============
    // Called at start of frame to reset pressed/released states
    virtual void begin_frame() = 0;

    // ============== Keyboard ==============
    virtual bool is_key_down(KeyCode key) const = 0;      // Currently held
    virtual bool is_key_pressed(KeyCode key) const = 0;   // Just pressed this frame
    virtual bool is_key_released(KeyCode key) const = 0;  // Just released this frame

    // ============== Mouse Buttons ==============
    virtual bool is_mouse_button_down(int button) const = 0;
    virtual bool is_mouse_button_pressed(int button) const = 0;
    virtual bool is_mouse_button_released(int button) const = 0;

    // ============== Mouse Position ==============
    // Returns position in logical game coordinates (0-639, 0-479)
    virtual int get_mouse_x() const = 0;
    virtual int get_mouse_y() const = 0;

    // ============== Mouse Wheel ==============
    // Returns accumulated delta since last reset
    virtual int get_mouse_wheel_delta() const = 0;
    // Clear accumulated delta (called after a rendered frame consumes it)
    virtual void reset_mouse_wheel_delta() = 0;

    // ============== Modifier Keys ==============
    virtual bool is_shift_down() const = 0;
    virtual bool is_ctrl_down() const = 0;
    virtual bool is_alt_down() const = 0;

    // ============== hb::shared::render::Window Focus ==============
    virtual bool is_window_active() const = 0;
    virtual void set_window_active(bool active) = 0;

    // ============== Input Suppression ==============
    // When suppressed, all input queries return false/zero (except mouse position)
    // Used internally by overlay system - developers shouldn't call this directly
    virtual void set_suppressed(bool suppressed) = 0;
    virtual bool is_suppressed() const = 0;

    // ============== Clipboard ==============
    virtual std::string get_clipboard_text() const = 0;
    virtual void set_clipboard_text(const std::string& text) = 0;

    // ============== Typed Character Buffer ==============
    // Buffered Unicode codepoints from text input events this frame.
    // Filled by on_text_char(), consumed by take_typed_chars().
    virtual void on_text_char(uint32_t codepoint) = 0;
    virtual std::vector<uint32_t> take_typed_chars() = 0;

    // ============== Input Events ==============
    // Called by window/event handler to feed input
    virtual void on_key_down(KeyCode key) = 0;
    virtual void on_key_up(KeyCode key) = 0;
    virtual void on_mouse_move(int x, int y) = 0;
    virtual void on_mouse_down(int button) = 0;
    virtual void on_mouse_up(int button) = 0;
    virtual void on_mouse_wheel(int delta) = 0;
};

// ============== Global Input System ==============
namespace hb::shared::input {
    // Lifecycle - called by engine
    void create();
    void destroy();

    // get the implementation
    IInput* get();

    // ============== Static Convenience Functions (Raylib style) ==============

    // Keyboard
    inline bool is_key_down(KeyCode key) { return get()->is_key_down(key); }
    inline bool is_key_pressed(KeyCode key) { return get()->is_key_pressed(key); }
    inline bool is_key_released(KeyCode key) { return get()->is_key_released(key); }

    // Mouse buttons
    inline bool is_mouse_button_down(int btn) { return get()->is_mouse_button_down(btn); }
    inline bool is_mouse_button_pressed(int btn) { return get()->is_mouse_button_pressed(btn); }
    inline bool is_mouse_button_released(int btn) { return get()->is_mouse_button_released(btn); }

    // Mouse position
    inline int get_mouse_x() { return get()->get_mouse_x(); }
    inline int get_mouse_y() { return get()->get_mouse_y(); }
    inline int get_mouse_wheel_delta() { return get()->get_mouse_wheel_delta(); }
    inline void reset_mouse_wheel_delta() { get()->reset_mouse_wheel_delta(); }

    // Modifier keys
    inline bool is_shift_down() { return get()->is_shift_down(); }
    inline bool is_ctrl_down() { return get()->is_ctrl_down(); }
    inline bool is_alt_down() { return get()->is_alt_down(); }

    // hb::shared::render::Window state
    inline bool is_window_active() { return get()->is_window_active(); }

    // Frame management
    inline void begin_frame() { get()->begin_frame(); }

    // Input suppression (used internally by overlay system)
    inline void set_suppressed(bool suppressed) { get()->set_suppressed(suppressed); }
    inline bool is_suppressed() { return get()->is_suppressed(); }

    // Clipboard
    inline std::string get_clipboard_text() { return get()->get_clipboard_text(); }
    inline void set_clipboard_text(const std::string& text) { get()->set_clipboard_text(text); }

    // Typed character buffer
    inline void on_text_char(uint32_t codepoint) { get()->on_text_char(codepoint); }
    inline std::vector<uint32_t> take_typed_chars() { return get()->take_typed_chars(); }

    // ============== Hit-Testing Helpers (replaces CMouseInterface) ==============

    // Check if point is inside rectangle (x, y, width, height)
    inline bool point_in_rect(int px, int py, int x, int y, int w, int h) {
        return px >= x && px < x + w && py >= y && py < y + h;
    }

    // Check if mouse is inside rectangle (x, y, width, height)
    inline bool is_mouse_in_rect(int x, int y, int w, int h) {
        return point_in_rect(get_mouse_x(), get_mouse_y(), x, y, w, h);
    }

    // Check if left click occurred inside rectangle this frame (x, y, width, height)
    inline bool is_click_in_rect(int x, int y, int w, int h) {
        return is_mouse_button_pressed(hb::shared::input::MouseButton::Left) && is_mouse_in_rect(x, y, w, h);
    }

    // Check if right click occurred inside rectangle this frame (x, y, width, height)
    inline bool is_right_click_in_rect(int x, int y, int w, int h) {
        return is_mouse_button_pressed(hb::shared::input::MouseButton::Right) && is_mouse_in_rect(x, y, w, h);
    }
}
