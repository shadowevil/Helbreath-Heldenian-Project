// application.h: Base class for the game application
//
// Owns the main loop, routes IWindowEventHandler callbacks:
//   - Input (keyboard/mouse) → IInput transparently
//   - Window lifecycle (close/resize/focus) → on_event() as discrete events
//   - Text input / platform messages → protected virtual hooks
//
// CGame derives from this. Entry point creates, attaches window, runs.
//////////////////////////////////////////////////////////////////////

#pragma once

#include "window.h"
#include "event.h"
#include "native_types.h"
#include <memory>

namespace hb::shared::render {

class application : public IWindowEventHandler
{
public:
	virtual ~application();

	// Factory: creates a derived application instance
	template<typename T, typename... Args>
	static std::unique_ptr<T> create(Args&&... args)
	{
		static_assert(std::is_base_of_v<application, T>,
			"T must derive from application");
		return std::make_unique<T>(std::forward<Args>(args)...);
	}

	// Attach a window (must be called before initialize)
	void attach_window(IWindow* window);

	// initialize the application (calls on_initialize — pre-realize)
	bool initialize();

	// Run the main loop. Returns exit code.
	// Realizes the OS window, calls on_start(), shows the window,
	// polls messages each iteration, calls on_run().
	// Calls on_uninitialize() when loop exits.
	int run();

	// Access the attached window
	IWindow* get_window() const { return m_window; }

protected:
	// --- Derived class overrides (5 required + 3 optional) ---

	// Called once during initialize(), BEFORE the OS window is realized.
	// Configure window params (set_title, set_size, etc.), load config files.
	virtual bool on_initialize() = 0;

	// Called once during run(), AFTER the OS window is realized.
	// Create renderer, load resources, set display settings.
	virtual bool on_start() = 0;

	// Called once when run() exits. Explicit cleanup.
	virtual void on_uninitialize() = 0;

	// Called every loop iteration after messages are processed.
	// The application has full control: frame timing, update/render split.
	virtual void on_run() = 0;

	// Discrete event handler. Receives window lifecycle events and game events.
	// Input (keyboard/mouse) is NOT routed here — it goes to IInput.
	virtual void on_event(const event& e) = 0;

	// Called for platform-specific messages (Win32, etc.).
	// Return true if handled. Default returns false.
	virtual bool on_native_message(uint32_t message, uintptr_t wparam, intptr_t lparam);

	// Called for key down/up events alongside IInput routing.
	// Override in derived class for game-specific key handling (hotkeys, etc.).
	// Default does nothing.
	virtual void on_key_event(KeyCode key, bool pressed);

	// Called for text input / IME events.
	// Return true if handled. Default returns false.
	bool on_text_input(hb::shared::types::NativeWindowHandle hwnd,
	                   uint32_t message, uintptr_t wparam, intptr_t lparam) override;

	// Post a game-defined event. Dispatches to on_event() immediately (synchronous).
	void post_event(const event& e) { on_event(e); }

	// Request the run loop to stop (run() will return after the current iteration).
	void request_quit() { m_running = false; }

private:
	// --- IWindowEventHandler implementation ---
	// Input callbacks route into IInput + game key hook.
	// Window lifecycle callbacks create event structs and call on_event().
	void on_close() override;
	void on_destroy() override;
	void on_activate(bool active) override;
	void on_resize(int width, int height) override;
	void on_key_down(KeyCode key) override;
	void on_key_up(KeyCode key) override;
	void on_char(char character) override;
	void on_mouse_move(int x, int y) override;
	void on_mouse_button_down(int button, int x, int y) override;
	void on_mouse_button_up(int button, int x, int y) override;
	void on_mouse_wheel(int delta, int x, int y) override;
	bool on_custom_message(uint32_t message, uintptr_t wparam, intptr_t lparam) override;

	IWindow* m_window = nullptr;
	bool m_initialized = false;
	bool m_running = false;
};

} // namespace hb::shared::render
