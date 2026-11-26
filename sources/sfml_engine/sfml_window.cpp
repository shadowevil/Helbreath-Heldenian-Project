// sfml_window.cpp: Pure SFML window implementing hb::shared::render::IWindow interface
//
// Part of SFMLEngine static library
// Supports two-phase init: allocate → configure via setters → realize()
//////////////////////////////////////////////////////////////////////

#include "sfml_window.h"
#include "sfml_input.h"
#include "sfml_renderer.h"
#include "renderer_factory.h"
#include "render_constants.h"
#include "output_rects.h"
#include <SFML/Window/Event.hpp>
#include <SFML/Graphics/Image.hpp>
#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "platform_headers.h"

namespace MouseButton = hb::shared::input::MouseButton;
namespace render = hb::shared::render;
namespace window_mode = render::window_mode;

namespace
{
    SFMLRenderer* sfml_renderer()
    {
        return static_cast<SFMLRenderer*>(render::Renderer::get());
    }
}

SFMLWindow::SFMLWindow()
    : m_handle{}
    , m_event_handler(nullptr)
    , m_realized(false)
    , m_open(false)
    , m_active(true)
    , m_title("Application")
    , m_width(800)
    , m_height(600)
    , m_in_exclusive(false)
    , m_mouse_capture_enabled(false)
    , m_background_fps_limit(0)
    , m_nativeInstance{}
    , m_icon_resource_id(0)
{
}

SFMLWindow::~SFMLWindow()
{
    destroy();
}

bool SFMLWindow::realize()
{
    if (m_realized)
        return false;

    create_native_window();
    if (!m_renderWindow.isOpen() && m_display.mode != window_mode::windowed)
    {
        // A fullscreen mode that cannot open must not lock the player out: start windowed instead
        m_display.mode = window_mode::windowed;
        create_native_window();
    }
    if (!m_renderWindow.isOpen())
        return false;

    // Request 1ms timer resolution for accurate sleep-based frame limiting
    hb::platform::set_timer_resolution(1);

    m_realized = true;
    m_open = true;
    m_active = true;
    return true;
}

void SFMLWindow::destroy()
{
#ifdef _WIN32
    ClipCursor(nullptr);
#endif
    m_event_handler = nullptr;

    if (m_renderWindow.isOpen())
    {
        m_renderWindow.close();
    }

    // Restore default timer resolution
    if (m_realized)
        hb::platform::restore_timer_resolution(1);

    m_handle = {};
    m_realized = false;
    m_open = false;
    m_active = false;
    m_in_exclusive = false;
}

bool SFMLWindow::is_open() const
{
    return m_open && m_renderWindow.isOpen();
}

void SFMLWindow::close()
{
    // Don't call m_event_handler->on_close() here — that creates infinite recursion
    // since on_close() may call Window::close(). on_close() is only called from the
    // SFML event loop when the user clicks the close button.
    m_open = false;
    m_renderWindow.close();
}

hb::shared::types::NativeWindowHandle SFMLWindow::get_handle() const
{
    return m_handle;
}

int SFMLWindow::get_width() const
{
    return m_width;
}

int SFMLWindow::get_height() const
{
    return m_height;
}

bool SFMLWindow::is_fullscreen() const
{
    return render::is_fullscreen(m_display.mode);
}

bool SFMLWindow::is_active() const
{
    return m_active;
}

// ============== Display model ==============

bool SFMLWindow::is_window_mode_supported(window_mode::type mode) const
{
#ifdef _WIN32
    (void)mode;
    return true;
#else
    // X11 fullscreen grabs the pointer and can lock mouse movement: exclusive folds into borderless
    return mode != window_mode::exclusive_fullscreen;
#endif
}

render::display_mode SFMLWindow::get_desktop_mode() const
{
    // While the monitor is switched to an exclusive mode the "current" mode is ours, not the desktop's
    if (m_in_exclusive && m_desktop_cache.width > 0)
        return m_desktop_cache;

    const sf::VideoMode desktop = sf::VideoMode::getDesktopMode();
    m_desktop_cache.width = static_cast<int>(desktop.size.x);
    m_desktop_cache.height = static_cast<int>(desktop.size.y);
#ifdef _WIN32
    m_desktop_cache.refresh_rate = hb::platform::get_monitor_refresh_rate();
#else
    m_desktop_cache.refresh_rate = 0;  // sf::VideoMode carries no rate
#endif
    return m_desktop_cache;
}

std::vector<render::resolution_option> SFMLWindow::get_resolution_options() const
{
    return render::build_resolution_options(get_desktop_mode(), RENDER_LOGICAL_WIDTH(), RENDER_LOGICAL_HEIGHT());
}

std::vector<render::refresh_option> SFMLWindow::get_refresh_options() const
{
    std::vector<double> driver_rates;
    for (const int rate : hb::platform::get_display_refresh_rates())
        driver_rates.push_back(static_cast<double>(rate));
    return render::build_refresh_options(driver_rates);
}

const render::display_settings& SFMLWindow::get_display_settings() const
{
    return m_display;
}

bool SFMLWindow::apply_display_settings(const render::display_settings& requested)
{
    render::display_settings next = requested;
    bool exact = true;
    if (!is_window_mode_supported(next.mode))
    {
        next.mode = window_mode::borderless_fullscreen;
        exact = false;
    }
    next.width = (std::max)(next.width, 1);
    next.height = (std::max)(next.height, 1);
    next.refresh_rate = (std::max)(next.refresh_rate, 0);

    const render::display_settings previous = m_display;
    m_display = next;

    // Staged: realize() creates the window from m_display
    if (!m_realized)
        return exact;

    const bool size_changed = next.width != previous.width || next.height != previous.height;
    const bool rebuild = next.mode != previous.mode
        || (next.mode == window_mode::exclusive_fullscreen && size_changed)
        || (next.mode == window_mode::windowed && next.windowed_borderless != previous.windowed_borderless);

    if (rebuild)
    {
        exact = create_native_window() && exact;
        if (!m_renderWindow.isOpen())
        {
            // The new mode could not open a window: come back in the previous one, so a caller never
            // persists a mode that fails
            m_display = previous;
            create_native_window();
            exact = false;
        }
        relink_window();
    }
    else if (next.mode == window_mode::windowed && size_changed)
    {
        // Same window, new client size
        m_width = next.width;
        m_height = next.height;
        m_renderWindow.setSize({static_cast<unsigned int>(m_width), static_cast<unsigned int>(m_height)});
        center_on_current_monitor();
#ifdef _WIN32
        m_renderWindow.setMouseCursorGrabbed(false);  // re-grab to the new bounds
#endif
        apply_cursor_grab();
    }
    else if (next.mode == window_mode::exclusive_fullscreen && next.refresh_rate != previous.refresh_rate)
    {
        exact = apply_exclusive_refresh_rate() && exact;
    }

    push_display_state();
    return exact;
}

bool SFMLWindow::create_native_window()
{
    bool exact = true;
    const render::display_mode desktop = get_desktop_mode();  // before create(): a cached desktop while exclusive

    sf::VideoMode video_mode({static_cast<unsigned int>(m_display.width), static_cast<unsigned int>(m_display.height)});
    sf::State state = sf::State::Windowed;
    auto style = sf::Style::None;

    switch (m_display.mode)
    {
    case window_mode::windowed:
        if (!m_display.windowed_borderless)
            style = sf::Style::Titlebar;
        break;

    case window_mode::borderless_fullscreen:
        video_mode = sf::VideoMode({static_cast<unsigned int>(desktop.width), static_cast<unsigned int>(desktop.height)});
        break;

    case window_mode::exclusive_fullscreen:
    {
        // SFML falls back to its first (largest) mode for a size the monitor does not offer; pick the
        // closest offered size ourselves, as SDL does
        const std::vector<sf::VideoMode>& modes = sf::VideoMode::getFullscreenModes();
        const sf::VideoMode* best = nullptr;
        long best_diff = 0;
        for (const sf::VideoMode& mode : modes)
        {
            const long diff = std::labs(static_cast<long>(mode.size.x) - m_display.width)
                + std::labs(static_cast<long>(mode.size.y) - m_display.height);
            if (!best || diff < best_diff || (diff == best_diff && mode.bitsPerPixel > best->bitsPerPixel))
            {
                best = &mode;
                best_diff = diff;
            }
        }
        if (best)
        {
            video_mode = *best;
            exact = best_diff == 0;
        }
        else
        {
            video_mode = sf::VideoMode({static_cast<unsigned int>(desktop.width), static_cast<unsigned int>(desktop.height)});
            exact = false;
        }
        // Remember the desktop: while exclusive, the "current" mode is ours
        m_desktop_cache = desktop;
        state = sf::State::Fullscreen;
        break;
    }
    }

    m_renderWindow.create(video_mode, m_title.c_str(), style, state);
    m_in_exclusive = state == sf::State::Fullscreen && m_renderWindow.isOpen();
    if (!m_renderWindow.isOpen())
        return false;

    m_width = static_cast<int>(video_mode.size.x);
    m_height = static_cast<int>(video_mode.size.y);
    if (m_display.mode == window_mode::exclusive_fullscreen)
    {
        // SFML only logs a failed mode switch: check the monitor really runs at the chosen size
        if (sf::VideoMode::getDesktopMode().size != video_mode.size)
            exact = false;

        // Report the size actually in effect
        m_display.width = m_width;
        m_display.height = m_height;
    }

#ifdef _WIN32
    m_handle = static_cast<HWND>(m_renderWindow.getNativeHandle());
#endif

    // sf::Style::Titlebar has no minimize button
    if (m_display.mode == window_mode::windowed && !m_display.windowed_borderless && m_handle)
        hb::platform::add_minimize_button(m_handle);

    if (m_in_exclusive && m_display.refresh_rate > 0)
        exact = apply_exclusive_refresh_rate() && exact;

    // Hide the system mouse cursor (game draws its own cursor)
    m_renderWindow.setMouseCursorVisible(false);
    apply_cursor_grab();

    // Fullscreen at the monitor origin, windowed centred on the monitor the window occupies
    center_on_current_monitor();
    update_cursor_clip();
    return exact;
}

bool SFMLWindow::apply_exclusive_refresh_rate()
{
    // 0 (Unlimited) leaves the monitor at whatever rate the mode switch gave it
    if (!m_in_exclusive || m_display.refresh_rate <= 0)
        return true;

    // The rate the driver offers at this size nearest the request (SDL's closest-mode rule)
    const std::vector<int> rates = hb::platform::get_display_refresh_rates(m_width, m_height);
    if (rates.empty())
        return false;
    int rate = rates.front();
    for (const int offered : rates)
    {
        if (std::abs(offered - m_display.refresh_rate) < std::abs(rate - m_display.refresh_rate))
            rate = offered;
    }
    if (!hb::platform::set_display_refresh_rate(m_width, m_height, rate))
        return false;
    // Report (and cap frames at) the rate actually in effect
    const bool exact = rate == m_display.refresh_rate;
    m_display.refresh_rate = rate;
    return exact;
}

void SFMLWindow::relink_window()
{
    if (hb::shared::input::get())
    {
        SFMLInput* input = static_cast<SFMLInput*>(hb::shared::input::get());
        input->Initialize(m_handle);
        input->SetRenderWindow(&m_renderWindow);
    }

    if (SFMLRenderer* renderer = sfml_renderer())
    {
        renderer->SetRenderWindow(&m_renderWindow);
        (void)m_renderWindow.setActive(true);
    }
}

void SFMLWindow::push_display_state()
{
    if (SFMLRenderer* renderer = sfml_renderer())
    {
        renderer->set_fullscreen(render::is_fullscreen(m_display.mode));
        renderer->set_fullscreen_stretch(m_display.aspect == render::aspect_mode::stretch);
        renderer->SetVSyncMode(m_display.vsync);
        renderer->SetFramerateLimit(current_frame_cap());
    }
    update_cursor_clip();
}

int SFMLWindow::current_frame_cap() const
{
    if (!m_active && m_background_fps_limit > 0)
        return m_background_fps_limit;
    // VSync paces to the monitor's current rate (in exclusive, the switched one) without SFML's blocking display()
    if (m_display.vsync)
        return hb::platform::get_monitor_refresh_rate();
    return m_display.refresh_rate;
}

void SFMLWindow::show()
{
    m_renderWindow.setVisible(true);
    m_renderWindow.requestFocus();

    // Position the window after it's visible. On Linux, window managers like
    // GNOME/Mutter ignore setPosition on newly created windows before they're
    // fully mapped. Calling it here (after show) ensures the WM honors it.
    center_on_current_monitor();

    // Force foreground window to ensure focus
    if (m_handle)
    {
        hb::platform::focus_window(m_handle);
    }
}

void SFMLWindow::hide()
{
    m_renderWindow.setVisible(false);
}

void SFMLWindow::set_title(const char* title)
{
    if (title)
    {
        m_title = title;
        if (m_realized)
            m_renderWindow.setTitle(title);
    }
}

void SFMLWindow::set_background_fps_limit(int limit)
{
    m_background_fps_limit = (limit > 0) ? limit : 0;
}

int SFMLWindow::get_background_fps_limit() const
{
    return m_background_fps_limit;
}

void SFMLWindow::set_native_instance(hb::shared::types::NativeInstance instance)
{
    m_nativeInstance = instance;
}

void SFMLWindow::set_icon_resource_id(int id)
{
    m_icon_resource_id = id;
}

void SFMLWindow::set_icon(unsigned int width, unsigned int height, const unsigned char* rgba_pixels)
{
    if (!rgba_pixels)
        return;

    // On Windows the icon is set via the .rc resource file (embedded in the .exe).
    // On Linux/macOS, we use SFML's setIcon to set the window title bar icon.
#ifndef _WIN32
    if (m_realized)
    {
        sf::Image icon({width, height}, rgba_pixels);
        m_renderWindow.setIcon(icon);
    }
#else
    (void)width;
    (void)height;
#endif
}

void SFMLWindow::set_mouse_cursor_visible(bool visible)
{
    if (m_realized)
        m_renderWindow.setMouseCursorVisible(visible);
}

void SFMLWindow::set_mouse_capture_enabled(bool enabled)
{
    m_mouse_capture_enabled = enabled;
    if (m_realized)
        apply_cursor_grab();
}

void SFMLWindow::show_message_box(const char* title, const char* message)
{
    hb::platform::show_error_dialog(m_handle, title, message);
}

bool SFMLWindow::process_messages()
{
    // Check if window was closed programmatically (via close() method)
    if (!m_open || !m_renderWindow.isOpen())
        return false;

    while (const std::optional<sf::Event> event = m_renderWindow.pollEvent())
    {
        if (event->is<sf::Event::Closed>())
        {
            // Notify the event handler — it decides whether to close or not
            if (m_event_handler)
                m_event_handler->on_close();
            // If the handler called close() or request_quit(), m_open is now false
            if (!m_open)
                return false;
            // Otherwise, the application chose to keep the window open (e.g., logout countdown)
            continue;
        }

        if (const auto* resized = event->getIf<sf::Event::Resized>())
        {
            m_width = static_cast<int>(resized->size.x);
            m_height = static_cast<int>(resized->size.y);
            if (m_event_handler)
                m_event_handler->on_resize(m_width, m_height);
        }

        if (const auto* focusGained = event->getIf<sf::Event::FocusGained>())
        {
            m_active = true;
            // Reactivate OpenGL context when focus is regained
            (void)m_renderWindow.setActive(true);
            // Re-grab mouse cursor if capture is enabled
            apply_cursor_grab();
            update_cursor_clip();
            // Restore the user's frame cap
            if (SFMLRenderer* renderer = sfml_renderer())
                renderer->SetFramerateLimit(current_frame_cap());
            if (m_event_handler)
                m_event_handler->on_activate(true);
        }

        if (const auto* focusLost = event->getIf<sf::Event::FocusLost>())
        {
            m_active = false;
#ifdef _WIN32
            ClipCursor(nullptr);
#endif
            // Throttle rendering to save CPU/GPU while window is not focused (current_frame_cap)
            if (SFMLRenderer* renderer = sfml_renderer())
                renderer->SetFramerateLimit(current_frame_cap());
            if (m_event_handler)
                m_event_handler->on_activate(false);
        }

        if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>())
        {
            if (m_event_handler)
                m_event_handler->on_key_down(SfmlKeyToKeyCode(keyPressed->code));
        }

        if (const auto* keyReleased = event->getIf<sf::Event::KeyReleased>())
        {
            if (m_event_handler)
                m_event_handler->on_key_up(SfmlKeyToKeyCode(keyReleased->code));
        }

        if (const auto* textEntered = event->getIf<sf::Event::TextEntered>())
        {
            if (m_event_handler)
            {
                // Call on_text_input with WM_CHAR-style parameters
                // The game's GetText() function expects Win32 message format
                m_event_handler->on_text_input(m_handle, 0x0102 /*WM_CHAR*/,
                    static_cast<uintptr_t>(textEntered->unicode), 0);
            }
        }

        if (const auto* mouseMoved = event->getIf<sf::Event::MouseMoved>())
        {
#ifndef _WIN32
            // Software mouse capture for Linux: warp cursor back if it escapes
            // the window bounds.  We cannot use SFML's setMouseCursorGrabbed on
            // X11 because it calls XGrabPointer with an empty event mask which
            // swallows all mouse events.
            if (!render::is_fullscreen(m_display.mode) && m_mouse_capture_enabled && m_active)
            {
                auto winSize = m_renderWindow.getSize();
                int mx = mouseMoved->position.x;
                int my = mouseMoved->position.y;
                bool clamped = false;
                if (mx < 0) { mx = 0; clamped = true; }
                if (my < 0) { my = 0; clamped = true; }
                if (mx >= static_cast<int>(winSize.x)) { mx = static_cast<int>(winSize.x) - 1; clamped = true; }
                if (my >= static_cast<int>(winSize.y)) { my = static_cast<int>(winSize.y) - 1; clamped = true; }
                if (clamped)
                    sf::Mouse::setPosition({mx, my}, m_renderWindow);
            }
#endif
            if (m_event_handler)
            {
                int logicalX, logicalY;
                TransformMouseCoords(mouseMoved->position.x, mouseMoved->position.y, logicalX, logicalY);
                m_event_handler->on_mouse_move(logicalX, logicalY);
            }
        }

        if (const auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>())
        {
            int button = MouseButton::Left;
            if (mousePressed->button == sf::Mouse::Button::Right)
                button = MouseButton::Right;
            else if (mousePressed->button == sf::Mouse::Button::Middle)
                button = MouseButton::Middle;

            if (m_event_handler)
            {
                int logicalX, logicalY;
                TransformMouseCoords(mousePressed->position.x, mousePressed->position.y, logicalX, logicalY);
                m_event_handler->on_mouse_button_down(button, logicalX, logicalY);
            }
        }

        if (const auto* mouseReleased = event->getIf<sf::Event::MouseButtonReleased>())
        {
            int button = MouseButton::Left;
            if (mouseReleased->button == sf::Mouse::Button::Right)
                button = MouseButton::Right;
            else if (mouseReleased->button == sf::Mouse::Button::Middle)
                button = MouseButton::Middle;

            if (m_event_handler)
            {
                int logicalX, logicalY;
                TransformMouseCoords(mouseReleased->position.x, mouseReleased->position.y, logicalX, logicalY);
                m_event_handler->on_mouse_button_up(button, logicalX, logicalY);
            }
        }

        if (const auto* mouseWheel = event->getIf<sf::Event::MouseWheelScrolled>())
        {
            if (m_event_handler)
            {
                int logicalX, logicalY;
                TransformMouseCoords(mouseWheel->position.x, mouseWheel->position.y, logicalX, logicalY);
                int delta = static_cast<int>(mouseWheel->delta * 120);  // Convert to Windows-style delta
                m_event_handler->on_mouse_wheel(delta, logicalX, logicalY);
            }
        }
    }

    return true;
}

void SFMLWindow::wait_for_message()
{
    // Wait for an event and process it (don't discard it)
    // This is critical for handling FocusGained when the window is inactive
    if (const std::optional<sf::Event> event = m_renderWindow.waitEvent())
    {
        // Process the event we received (same logic as process_messages)
        if (event->is<sf::Event::Closed>())
        {
            if (m_event_handler)
                m_event_handler->on_close();
            if (!m_open)
                return;
        }

        if (const auto* focusGained = event->getIf<sf::Event::FocusGained>())
        {
            m_active = true;
            (void)m_renderWindow.setActive(true);
            apply_cursor_grab();
            if (SFMLRenderer* renderer = sfml_renderer())
                renderer->SetFramerateLimit(current_frame_cap());
            if (m_event_handler)
                m_event_handler->on_activate(true);
        }

        if (const auto* focusLost = event->getIf<sf::Event::FocusLost>())
        {
            m_active = false;
            if (SFMLRenderer* renderer = sfml_renderer())
                renderer->SetFramerateLimit(current_frame_cap());
            if (m_event_handler)
                m_event_handler->on_activate(false);
        }

        // Other events will be handled in the next process_messages() call
        // but focus events are critical to handle here for proper resume
    }
}

void SFMLWindow::set_event_handler(hb::shared::render::IWindowEventHandler* handler)
{
    m_event_handler = handler;
}

hb::shared::render::IWindowEventHandler* SFMLWindow::get_event_handler() const
{
    return m_event_handler;
}

// Convert SFML key codes to abstract KeyCode enum values
KeyCode SFMLWindow::SfmlKeyToKeyCode(sf::Keyboard::Key key)
{
    switch (key)
    {
    // Letters
    case sf::Keyboard::Key::A: return KeyCode::A;
    case sf::Keyboard::Key::B: return KeyCode::B;
    case sf::Keyboard::Key::C: return KeyCode::C;
    case sf::Keyboard::Key::D: return KeyCode::D;
    case sf::Keyboard::Key::E: return KeyCode::E;
    case sf::Keyboard::Key::F: return KeyCode::F;
    case sf::Keyboard::Key::G: return KeyCode::G;
    case sf::Keyboard::Key::H: return KeyCode::H;
    case sf::Keyboard::Key::I: return KeyCode::I;
    case sf::Keyboard::Key::J: return KeyCode::J;
    case sf::Keyboard::Key::K: return KeyCode::K;
    case sf::Keyboard::Key::L: return KeyCode::L;
    case sf::Keyboard::Key::M: return KeyCode::M;
    case sf::Keyboard::Key::N: return KeyCode::N;
    case sf::Keyboard::Key::O: return KeyCode::O;
    case sf::Keyboard::Key::P: return KeyCode::P;
    case sf::Keyboard::Key::Q: return KeyCode::Q;
    case sf::Keyboard::Key::R: return KeyCode::R;
    case sf::Keyboard::Key::S: return KeyCode::S;
    case sf::Keyboard::Key::T: return KeyCode::T;
    case sf::Keyboard::Key::U: return KeyCode::U;
    case sf::Keyboard::Key::V: return KeyCode::V;
    case sf::Keyboard::Key::W: return KeyCode::W;
    case sf::Keyboard::Key::X: return KeyCode::X;
    case sf::Keyboard::Key::Y: return KeyCode::Y;
    case sf::Keyboard::Key::Z: return KeyCode::Z;

    // Numbers
    case sf::Keyboard::Key::Num0: return KeyCode::Num0;
    case sf::Keyboard::Key::Num1: return KeyCode::Num1;
    case sf::Keyboard::Key::Num2: return KeyCode::Num2;
    case sf::Keyboard::Key::Num3: return KeyCode::Num3;
    case sf::Keyboard::Key::Num4: return KeyCode::Num4;
    case sf::Keyboard::Key::Num5: return KeyCode::Num5;
    case sf::Keyboard::Key::Num6: return KeyCode::Num6;
    case sf::Keyboard::Key::Num7: return KeyCode::Num7;
    case sf::Keyboard::Key::Num8: return KeyCode::Num8;
    case sf::Keyboard::Key::Num9: return KeyCode::Num9;

    // Special keys
    case sf::Keyboard::Key::Escape: return KeyCode::Escape;
    case sf::Keyboard::Key::Space: return KeyCode::Space;
    case sf::Keyboard::Key::Enter: return KeyCode::Enter;
    case sf::Keyboard::Key::Backspace: return KeyCode::Backspace;
    case sf::Keyboard::Key::Tab: return KeyCode::Tab;

    // Modifiers
    case sf::Keyboard::Key::LControl: return KeyCode::LControl;
    case sf::Keyboard::Key::LShift: return KeyCode::LShift;
    case sf::Keyboard::Key::LAlt: return KeyCode::LAlt;
    case sf::Keyboard::Key::RControl: return KeyCode::RControl;
    case sf::Keyboard::Key::RShift: return KeyCode::RShift;
    case sf::Keyboard::Key::RAlt: return KeyCode::RAlt;

    // Navigation
    case sf::Keyboard::Key::PageUp: return KeyCode::PageUp;
    case sf::Keyboard::Key::PageDown: return KeyCode::PageDown;
    case sf::Keyboard::Key::End: return KeyCode::End;
    case sf::Keyboard::Key::Home: return KeyCode::Home;
    case sf::Keyboard::Key::Insert: return KeyCode::Insert;
    case sf::Keyboard::Key::Delete: return KeyCode::Delete;

    // Arrow keys
    case sf::Keyboard::Key::Left: return KeyCode::Left;
    case sf::Keyboard::Key::Right: return KeyCode::Right;
    case sf::Keyboard::Key::Up: return KeyCode::Up;
    case sf::Keyboard::Key::Down: return KeyCode::Down;

    // Function keys
    case sf::Keyboard::Key::F1: return KeyCode::F1;
    case sf::Keyboard::Key::F2: return KeyCode::F2;
    case sf::Keyboard::Key::F3: return KeyCode::F3;
    case sf::Keyboard::Key::F4: return KeyCode::F4;
    case sf::Keyboard::Key::F5: return KeyCode::F5;
    case sf::Keyboard::Key::F6: return KeyCode::F6;
    case sf::Keyboard::Key::F7: return KeyCode::F7;
    case sf::Keyboard::Key::F8: return KeyCode::F8;
    case sf::Keyboard::Key::F9: return KeyCode::F9;
    case sf::Keyboard::Key::F10: return KeyCode::F10;
    case sf::Keyboard::Key::F11: return KeyCode::F11;
    case sf::Keyboard::Key::F12: return KeyCode::F12;

    // Numpad numbers
    case sf::Keyboard::Key::Numpad0: return KeyCode::Numpad0;
    case sf::Keyboard::Key::Numpad1: return KeyCode::Numpad1;
    case sf::Keyboard::Key::Numpad2: return KeyCode::Numpad2;
    case sf::Keyboard::Key::Numpad3: return KeyCode::Numpad3;
    case sf::Keyboard::Key::Numpad4: return KeyCode::Numpad4;
    case sf::Keyboard::Key::Numpad5: return KeyCode::Numpad5;
    case sf::Keyboard::Key::Numpad6: return KeyCode::Numpad6;
    case sf::Keyboard::Key::Numpad7: return KeyCode::Numpad7;
    case sf::Keyboard::Key::Numpad8: return KeyCode::Numpad8;
    case sf::Keyboard::Key::Numpad9: return KeyCode::Numpad9;

    // Numpad operators
    case sf::Keyboard::Key::Add: return KeyCode::NumpadAdd;
    case sf::Keyboard::Key::Subtract: return KeyCode::NumpadSubtract;
    case sf::Keyboard::Key::Multiply: return KeyCode::NumpadMultiply;
    case sf::Keyboard::Key::Divide: return KeyCode::NumpadDivide;

    // OEM keys
    case sf::Keyboard::Key::Grave: return KeyCode::Grave;
    case sf::Keyboard::Key::Hyphen: return KeyCode::Hyphen;
    case sf::Keyboard::Key::Equal: return KeyCode::Equal;

    default: return KeyCode::Unknown;
    }
}

void SFMLWindow::window_to_ui(int window_x, int window_y, int& ui_x, int& ui_y) const
{
    TransformMouseCoords(window_x, window_y, ui_x, ui_y);
}

void SFMLWindow::ui_to_window(int ui_x, int ui_y, int& window_x, int& window_y) const
{
    const sf::Vector2i pixel = current_output_rects().ui.first_pixel_of_unit({ui_x, ui_y});
    window_x = pixel.x;
    window_y = pixel.y;
}

void SFMLWindow::clamp_ui(int& ui_x, int& ui_y) const
{
    clamp_ui(current_output_rects(), ui_x, ui_y);
}

void SFMLWindow::clamp_ui(const output_rects& rects, int& ui_x, int& ui_y)
{
    // The UI area, widened to the world rect's extent in UI units: in stretch the world fills
    // the window, so the mouse reaches past 0..LOGICAL_MAX there (negative, or above the maximum)
    const sf::Vector2f world_min = rects.ui.from_output(rects.world.rect.position);
    const sf::Vector2f world_max = rects.ui.from_output({rects.world.rect.position.x + rects.world.rect.size.x - 1.0f,
                                                         rects.world.rect.position.y + rects.world.rect.size.y - 1.0f});
    const int min_x = (std::min)(0, static_cast<int>(std::floor(world_min.x)));
    const int min_y = (std::min)(0, static_cast<int>(std::floor(world_min.y)));
    const int max_x = (std::max)(LOGICAL_MAX_X(), static_cast<int>(std::floor(world_max.x)));
    const int max_y = (std::max)(LOGICAL_MAX_Y(), static_cast<int>(std::floor(world_max.y)));
    ui_x = std::clamp(ui_x, min_x, max_x);
    ui_y = std::clamp(ui_y, min_y, max_y);
}

output_rects SFMLWindow::current_output_rects() const
{
    // get actual window size in physical pixels
    float windowWidth, windowHeight;

    int client_w = 0, client_h = 0;
    if (m_handle && hb::platform::get_client_size(m_handle, client_w, client_h) && client_w > 0 && client_h > 0)
    {
        windowWidth = static_cast<float>(client_w);
        windowHeight = static_cast<float>(client_h);
    }
    else if (m_width > 0 && m_height > 0)
    {
        windowWidth = static_cast<float>(m_width);
        windowHeight = static_cast<float>(m_height);
    }
    else
    {
        // Minimised or not sized yet: a 1:1 logical window keeps the mapping finite (as the renderer does)
        windowWidth = static_cast<float>(RENDER_LOGICAL_WIDTH());
        windowHeight = static_cast<float>(RENDER_LOGICAL_HEIGHT());
    }

    // The same rects as the renderer
    return compute_output_rects({windowWidth, windowHeight},
        {static_cast<float>(RENDER_LOGICAL_WIDTH()), static_cast<float>(RENDER_LOGICAL_HEIGHT())},
        render::uses_letterbox(m_display));
}

void SFMLWindow::TransformMouseCoords(int windowX, int windowY, int& logicalX, int& logicalY) const
{
    // The game reads the mouse in UI units: window pixels -> the UI rect
    const output_rects rects = current_output_rects();
    const sf::Vector2i ui = rects.ui.unit_of_pixel({windowX, windowY});
    logicalX = ui.x;
    logicalY = ui.y;
    clamp_ui(rects, logicalX, logicalY);
}

void SFMLWindow::apply_cursor_grab()
{
    // In fullscreen the cursor is already confined to the window by the OS,
    // so only grab in windowed mode when the user has enabled mouse capture.
    bool grab = !render::is_fullscreen(m_display.mode) && m_mouse_capture_enabled;

#ifdef _WIN32
    // On Windows, SFML uses ClipCursor which confines the pointer without
    // interfering with mouse events.
    m_renderWindow.setMouseCursorGrabbed(grab);
#else
    // On Linux/X11, SFML's setMouseCursorGrabbed calls XGrabPointer with an
    // empty event mask (None), which swallows ALL mouse events and makes the
    // cursor appear frozen.  Instead we do software clamping in process_messages
    // by warping the cursor back when it leaves the window bounds.
    (void)grab;
#endif
}

void SFMLWindow::center_on_current_monitor()
{
    if (render::is_fullscreen(m_display.mode))
    {
        auto mon = hb::platform::get_current_monitor_size(m_handle);
        if (mon.width > 0 && mon.height > 0)
        {
            move_window(mon.x, mon.y, mon.width, mon.height);
        }
        else
        {
            move_window(0, 0, m_width, m_height);
        }
    }
    else
    {
        auto mon = hb::platform::get_current_monitor_work_area(m_handle);
        if (mon.width > 0 && mon.height > 0)
        {
            int pos_x = mon.x + (mon.width - m_width) / 2;
            int pos_y = mon.y + (mon.height - m_height) / 2;
            move_window(pos_x, pos_y, m_width, m_height);
        }
        else
        {
            const render::display_mode desktop = get_desktop_mode();
            int pos_x = (desktop.width - m_width) / 2;
            int pos_y = (desktop.height - m_height) / 2;
            move_window(pos_x, pos_y, m_width, m_height);
        }
    }
}

void SFMLWindow::move_window(int x, int y, int width, int height)
{
#ifdef _WIN32
    hb::platform::position_window(m_handle, x, y, width, height);
#else
    (void)width; (void)height;
    m_renderWindow.setPosition(sf::Vector2i{x, y});
#endif
}

void SFMLWindow::update_cursor_clip()
{
#ifdef _WIN32
    if (render::uses_letterbox(m_display) && m_handle)
    {
        // In 4:3 letterbox mode, clip the OS cursor to the rendered area
        // so the hidden cursor can't drift into the black bars.
        int client_w = 0, client_h = 0;
        if (!hb::platform::get_client_size(m_handle, client_w, client_h))
            return;

        // The world rect, from the same helper the renderer and the mouse transform use
        const output_rects rects = compute_output_rects(
            {static_cast<float>(client_w), static_cast<float>(client_h)},
            {static_cast<float>(RENDER_LOGICAL_WIDTH()), static_cast<float>(RENDER_LOGICAL_HEIGHT())}, true);

        int destW = static_cast<int>(rects.world.rect.size.x);
        int destH = static_cast<int>(rects.world.rect.size.y);
        int offsetX = (client_w - destW) / 2;
        int offsetY = (client_h - destH) / 2;

        POINT topLeft = {offsetX, offsetY};
        ClientToScreen(m_handle, &topLeft);

        RECT clipRect;
        clipRect.left = topLeft.x;
        clipRect.top = topLeft.y;
        clipRect.right = topLeft.x + destW;
        clipRect.bottom = topLeft.y + destH;
        ClipCursor(&clipRect);
    }
    else
    {
        // Windowed or fullscreen-stretch: the world fills the window, so all of it is live; no clip
        ClipCursor(nullptr);
    }
#endif
}
