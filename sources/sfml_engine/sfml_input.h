// sfml_input.h: SFML implementation of IInput
//
// Part of SFMLEngine static library
//////////////////////////////////////////////////////////////////////

#pragma once

#include "native_types.h"
#include "input.h"

namespace sf { class RenderWindow; }

class SFMLInput : public IInput
{
public:
    SFMLInput();
    virtual ~SFMLInput();

    void Initialize(hb::shared::types::NativeWindowHandle hWnd);
    void SetRenderWindow(sf::RenderWindow* window);

    // ============== IInput Implementation ==============

    // Frame management
    virtual void begin_frame() override;

    // Keyboard
    virtual bool is_key_down(KeyCode key) const override;
    virtual bool is_key_pressed(KeyCode key) const override;
    virtual bool is_key_released(KeyCode key) const override;

    // Mouse buttons
    virtual bool is_mouse_button_down(int button) const override;
    virtual bool is_mouse_button_pressed(int button) const override;
    virtual bool is_mouse_button_released(int button) const override;

    // Mouse position (already in logical coordinates from SFMLWindow)
    virtual int get_mouse_x() const override;
    virtual int get_mouse_y() const override;

    // Mouse wheel
    virtual int get_mouse_wheel_delta() const override;
    virtual void reset_mouse_wheel_delta() override;

    // Modifier keys
    virtual bool is_shift_down() const override;
    virtual bool is_ctrl_down() const override;
    virtual bool is_alt_down() const override;

    // hb::shared::render::Window focus
    virtual bool is_window_active() const override;
    virtual void set_window_active(bool active) override;

    // Input suppression
    virtual void set_suppressed(bool suppressed) override;
    virtual bool is_suppressed() const override;

    // Clipboard
    virtual std::string get_clipboard_text() const override;
    virtual void set_clipboard_text(const std::string& text) override;

    // Typed character buffer
    virtual void on_text_char(uint32_t codepoint) override;
    virtual std::vector<uint32_t> take_typed_chars() override;

    // Input events (called by SFMLWindow with logical coordinates)
    virtual void on_key_down(KeyCode key) override;
    virtual void on_key_up(KeyCode key) override;
    virtual void on_mouse_move(int x, int y) override;
    virtual void on_mouse_down(int button) override;
    virtual void on_mouse_up(int button) override;
    virtual void on_mouse_wheel(int delta) override;

private:
    void ClearAllKeys();

    hb::shared::types::NativeWindowHandle m_handle;
    sf::RenderWindow* m_render_window;
    bool m_active;
    bool m_suppressed;

    // Mouse state
    int m_mouse_x;
    int m_mouse_y;
    int m_wheel_delta;
    bool m_mouseDown[3];     // Left, Right, Middle
    bool m_mousePressed[3];
    bool m_mouseReleased[3];

    // Keyboard state
    static constexpr int kKeyCount = 256;
    bool m_keyDown[kKeyCount];
    bool m_keyPressed[kKeyCount];
    bool m_keyReleased[kKeyCount];

    // Typed character buffer (Unicode codepoints, filled by on_text_char)
    std::vector<uint32_t> m_typed_chars;
};
