// application.cpp: Base class implementation
//
// Routes IWindowEventHandler callbacks and owns the main loop.
//////////////////////////////////////////////////////////////////////

#include "application.h"
#include "renderer_factory.h"
#include "input.h"

namespace hb::shared::render {

application::~application() = default;

void application::attach_window(IWindow* window)
{
	m_window = window;
}

bool application::initialize()
{
	if (!m_window)
		return false;

	if (!on_initialize())
		return false;

	m_initialized = true;
	return true;
}

int application::run()
{
	// --- Realize the OS window with configured params + init input ---
	if (!Window::realize())
	{
		Window::show_error("ERROR", "Failed to create window!");
		on_uninitialize();
		return 1;
	}

	// --- Post-realize initialization (renderer, resources) ---
	if (!on_start())
	{
		on_uninitialize();
		Window::destroy();
		return 1;
	}

	m_running = true;
	m_window->set_event_handler(this);
	m_window->show();

	// --- Main loop: engine polls, application runs ---
	while (m_running)
	{
		hb::shared::input::begin_frame();

		if (!m_window->process_messages())
		{
			m_running = false;
			break;
		}

		on_run();
	}

	// --- shutdown ---
	m_window->set_event_handler(nullptr);
	on_uninitialize();
	Window::destroy();
	m_window = nullptr;
	m_initialized = false;

	return 0;
}

// --- IWindowEventHandler: Input → IInput + game key hook ---

void application::on_key_down(KeyCode key)
{
	auto* input = hb::shared::input::get();
	if (input)
		input->on_key_down(key);
	on_key_event(key, true);
}

void application::on_key_up(KeyCode key)
{
	auto* input = hb::shared::input::get();
	if (input)
		input->on_key_up(key);
	on_key_event(key, false);
}

void application::on_char(char /*character*/)
{
	// Character input is handled through on_text_input (IME/WM_CHAR path)
}

void application::on_mouse_move(int x, int y)
{
	auto* input = hb::shared::input::get();
	if (input)
		input->on_mouse_move(x, y);
}

void application::on_mouse_button_down(int button, int x, int y)
{
	auto* input = hb::shared::input::get();
	if (input)
	{
		input->on_mouse_move(x, y);
		input->on_mouse_down(button);
	}
}

void application::on_mouse_button_up(int button, int x, int y)
{
	auto* input = hb::shared::input::get();
	if (input)
	{
		input->on_mouse_move(x, y);
		input->on_mouse_up(button);
	}
}

void application::on_mouse_wheel(int delta, int x, int y)
{
	auto* input = hb::shared::input::get();
	if (input)
	{
		input->on_mouse_move(x, y);
		input->on_mouse_wheel(delta);
	}
}

// --- IWindowEventHandler: Window lifecycle → on_event() ---

void application::on_close()
{
	on_event(event::make_closed());
}

void application::on_activate(bool active)
{
	on_event(active ? event::make_focus_gained() : event::make_focus_lost());
}

void application::on_resize(int w, int h)
{
	on_event(event::make_resized(w, h));
}

void application::on_destroy()
{
	// No-op — cleanup is handled by on_uninitialize() in run() teardown
}

// --- Platform-specific passthrough ---

bool application::on_custom_message(uint32_t message, uintptr_t wparam, intptr_t lparam)
{
	return on_native_message(message, wparam, lparam);
}

// --- Default implementations for optional overrides ---

bool application::on_native_message(uint32_t /*message*/, uintptr_t /*wparam*/, intptr_t /*lparam*/)
{
	return false;
}

void application::on_key_event(KeyCode /*key*/, bool /*pressed*/)
{
	// Default: no game-specific key handling
}

bool application::on_text_input(hb::shared::types::NativeWindowHandle /*hwnd*/,
                                uint32_t /*message*/, uintptr_t /*wparam*/, intptr_t /*lparam*/)
{
	return false;
}

} // namespace hb::shared::render
