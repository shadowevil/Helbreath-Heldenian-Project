// sfml_input.cpp: SFML implementation of IInput
//
// Part of SFMLEngine static library
// Note: SFMLWindow handles coordinate transformation, so mouse coordinates
// arrive already in logical game coordinates.
//////////////////////////////////////////////////////////////////////

#include "sfml_input.h"
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Window/Clipboard.hpp>
#include <SFML/System/String.hpp>
#include <cstring>

// Global input instance pointer (owned by RendererFactory)
static SFMLInput* s_pInput = nullptr;

// ============== Global Input Namespace Implementation ==============
namespace hb::shared::input {
    void create()
    {
        if (!s_pInput)
        {
            s_pInput = new SFMLInput();
        }
    }

    void destroy()
    {
        delete s_pInput;
        s_pInput = nullptr;
    }

    IInput* get()
    {
        return s_pInput;
    }
}

// ============== SFMLInput Implementation ==============

SFMLInput::SFMLInput()
    : m_handle{}
    , m_render_window(nullptr)
    , m_active(false)
    , m_suppressed(false)
    , m_mouse_x(0)
    , m_mouse_y(0)
    , m_wheel_delta(0)
{
    std::memset(m_mouseDown, 0, sizeof(m_mouseDown));
    std::memset(m_mousePressed, 0, sizeof(m_mousePressed));
    std::memset(m_mouseReleased, 0, sizeof(m_mouseReleased));
    std::memset(m_keyDown, 0, sizeof(m_keyDown));
    std::memset(m_keyPressed, 0, sizeof(m_keyPressed));
    std::memset(m_keyReleased, 0, sizeof(m_keyReleased));
}

SFMLInput::~SFMLInput() = default;

void SFMLInput::Initialize(hb::shared::types::NativeWindowHandle hWnd)
{
    m_handle = hWnd;
    m_active = true;
}

void SFMLInput::SetRenderWindow(sf::RenderWindow* window)
{
    m_render_window = window;
}

void SFMLInput::begin_frame()
{
    // Reset per-frame edge states (pressed/released)
    // Note: m_wheel_delta is NOT reset here — it accumulates across skip frames
    // and is cleared by ResetMouseWheelDelta() after a rendered frame consumes it
    std::memset(m_mousePressed, 0, sizeof(m_mousePressed));
    std::memset(m_mouseReleased, 0, sizeof(m_mouseReleased));
    std::memset(m_keyPressed, 0, sizeof(m_keyPressed));
    std::memset(m_keyReleased, 0, sizeof(m_keyReleased));
}

void SFMLInput::reset_mouse_wheel_delta()
{
    m_wheel_delta = 0;
}

// ============== Keyboard ==============

bool SFMLInput::is_key_down(KeyCode key) const
{
    if (m_suppressed) return false;
    int k = static_cast<int>(key);
    if (k < 0 || k >= kKeyCount)
        return false;
    return m_keyDown[k];
}

bool SFMLInput::is_key_pressed(KeyCode key) const
{
    if (m_suppressed) return false;
    int k = static_cast<int>(key);
    if (k < 0 || k >= kKeyCount)
        return false;
    return m_keyPressed[k];
}

bool SFMLInput::is_key_released(KeyCode key) const
{
    if (m_suppressed) return false;
    int k = static_cast<int>(key);
    if (k < 0 || k >= kKeyCount)
        return false;
    return m_keyReleased[k];
}

void SFMLInput::on_key_down(KeyCode key)
{
    int k = static_cast<int>(key);
    if (k < 0 || k >= kKeyCount)
        return;

    // Only set pressed on initial key down, not on auto-repeat
    if (!m_keyDown[k])
    {
        m_keyPressed[k] = true;
    }
    m_keyDown[k] = true;
}

void SFMLInput::on_key_up(KeyCode key)
{
    int k = static_cast<int>(key);
    if (k < 0 || k >= kKeyCount)
        return;

    if (m_keyDown[k])
    {
        m_keyReleased[k] = true;
    }
    m_keyDown[k] = false;
}

// ============== Mouse Buttons ==============

bool SFMLInput::is_mouse_button_down(int button) const
{
    if (m_suppressed) return false;
    if (button < 0 || button > 2)
        return false;
    return m_mouseDown[button];
}

bool SFMLInput::is_mouse_button_pressed(int button) const
{
    if (m_suppressed) return false;
    if (button < 0 || button > 2)
        return false;
    return m_mousePressed[button];
}

bool SFMLInput::is_mouse_button_released(int button) const
{
    if (m_suppressed) return false;
    if (button < 0 || button > 2)
        return false;
    return m_mouseReleased[button];
}

void SFMLInput::on_mouse_down(int button)
{
    if (button < 0 || button > 2)
        return;

    if (!m_mouseDown[button])
    {
        m_mousePressed[button] = true;
    }
    m_mouseDown[button] = true;
}

void SFMLInput::on_mouse_up(int button)
{
    if (button < 0 || button > 2)
        return;

    if (m_mouseDown[button])
    {
        m_mouseReleased[button] = true;
    }
    m_mouseDown[button] = false;
}

// ============== Mouse Position ==============

int SFMLInput::get_mouse_x() const
{
    return m_mouse_x;
}

int SFMLInput::get_mouse_y() const
{
    return m_mouse_y;
}

void SFMLInput::on_mouse_move(int x, int y)
{
    // SFMLWindow already transforms to logical coordinates
    m_mouse_x = x;
    m_mouse_y = y;
}

// ============== Mouse Wheel ==============

int SFMLInput::get_mouse_wheel_delta() const
{
    if (m_suppressed) return 0;
    return m_wheel_delta;
}

void SFMLInput::on_mouse_wheel(int delta)
{
    m_wheel_delta += delta;
}

// ============== Modifier Keys ==============

bool SFMLInput::is_shift_down() const
{
    if (m_suppressed) return false;
    return is_key_down(KeyCode::Shift) || is_key_down(KeyCode::LShift) || is_key_down(KeyCode::RShift);
}

bool SFMLInput::is_ctrl_down() const
{
    if (m_suppressed) return false;
    return is_key_down(KeyCode::Control) || is_key_down(KeyCode::LControl) || is_key_down(KeyCode::RControl);
}

bool SFMLInput::is_alt_down() const
{
    if (m_suppressed) return false;
    return is_key_down(KeyCode::Alt) || is_key_down(KeyCode::LAlt) || is_key_down(KeyCode::RAlt);
}

// ============== Input Suppression ==============

void SFMLInput::set_suppressed(bool suppressed)
{
    m_suppressed = suppressed;
}

bool SFMLInput::is_suppressed() const
{
    return m_suppressed;
}

// ============== hb::shared::render::Window Focus ==============

bool SFMLInput::is_window_active() const
{
    return m_active;
}

void SFMLInput::set_window_active(bool active)
{
    m_active = active;

    if (!active)
    {
        ClearAllKeys();
    }
}

// ============== Clipboard ==============

std::string SFMLInput::get_clipboard_text() const
{
    return sf::Clipboard::getString().toAnsiString();
}

void SFMLInput::set_clipboard_text(const std::string& text)
{
    sf::Clipboard::setString(sf::String(text));
}

// ============== Typed Character Buffer ==============

void SFMLInput::on_text_char(uint32_t codepoint)
{
    m_typed_chars.push_back(codepoint);
}

std::vector<uint32_t> SFMLInput::take_typed_chars()
{
    return std::move(m_typed_chars);
}

void SFMLInput::ClearAllKeys()
{
    std::memset(m_keyDown, 0, sizeof(m_keyDown));
    std::memset(m_keyPressed, 0, sizeof(m_keyPressed));
    std::memset(m_keyReleased, 0, sizeof(m_keyReleased));
    std::memset(m_mouseDown, 0, sizeof(m_mouseDown));
    std::memset(m_mousePressed, 0, sizeof(m_mousePressed));
    std::memset(m_mouseReleased, 0, sizeof(m_mouseReleased));
}
