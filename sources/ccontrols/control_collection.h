#pragma once

#include "types.h"
#include "input_state.h"
#include "panel.h"
#include "focus_manager.h"
#include <cstdint>
#include <string>

namespace cc {

class button;
class textbox;
class text_input;
class toggle_button;

class control_collection
{
public:
	control_collection();

	// ===== Screen size (for root-level alignment) =====
	void set_screen_size(int w, int h);

	// ===== Tree building =====
	panel* root();

	template<typename T, typename... Args>
	T* add(Args&&... args) { return m_root.add<T>(std::forward<Args>(args)...); }

	void clear();

	control* find(int id);

	template<typename T>
	T* find_as(int id) { return dynamic_cast<T*>(find(id)); }

	// ===== Focus =====
	void set_focus_order(std::initializer_list<int> control_ids);
	int focused_id() const;
	void set_focus(int control_id);

	void set_hover_focus(bool enabled);
	void set_enter_advances_focus(bool enabled);
	void set_default_button(int button_id);
	void set_focus_nav_horizontal(bool enabled);

	// ===== Click sound =====
	void set_click_sound(sound_callback cb);

	// ===== Tooltip =====
	void set_tooltip_delay_ms(uint32_t ms);

	// ===== Key repeat =====
	void set_key_repeat(uint32_t initial_delay_ms, uint32_t repeat_interval_ms);

	// ===== Clipboard provider =====
	using clipboard_get_fn = std::function<std::string()>;
	using clipboard_set_fn = std::function<void(const std::string&)>;
	void set_clipboard_provider(clipboard_get_fn get, clipboard_set_fn set);

	// ===== Per-frame calls =====
	void update(const input_state& input, uint32_t time_ms);
	void render() const;

	// ===== Queries =====
	const char* active_tooltip() const;
	bool enter_pressed() const;
	bool escape_pressed() const;

	// ===== Debug =====
	void debug_walk(std::function<void(const control& c)> fn) const;
	void set_debug_draw(bool enabled);
	bool debug_draw_enabled() const;

	// ===== Input management =====
	void discard_pending_input(const input_state& input);

private:
	void play_click_sound(control* ctrl);
	void handle_button_clicks();
	void handle_enter_key();
	void handle_toggle_keys(const input_state& input, uint32_t time_ms);
	void handle_text_input_keys(const input_state& input, uint32_t time_ms);
	void update_tooltip(uint32_t time_ms);

	panel m_root;
	focus_manager m_focus;
	input_state m_prev_input{};
	sound_callback m_default_click_sound;

	// Clipboard
	clipboard_get_fn m_clipboard_get;
	clipboard_set_fn m_clipboard_set;

	// Tooltip delay
	uint32_t m_tooltip_delay_ms = 500;
	uint32_t m_hover_start_ms = 0;
	int m_hover_control_id = -1;
	const char* m_active_tooltip = nullptr;

	// Key repeat for toggle arrows
	uint32_t m_key_repeat_initial_ms = 400;
	uint32_t m_key_repeat_interval_ms = 80;
	int m_toggle_repeat_dir = 0;
	uint32_t m_toggle_repeat_next_ms = 0;

	// Key repeat for text input (backspace, delete, arrows)
	int m_text_repeat_key = 0;  // 0=none, 1=backspace, 2=delete, 3=left, 4=right
	uint32_t m_text_repeat_next_ms = 0;

	bool m_hover_focus = false;
	bool m_enter_advances_focus = false;
	int m_default_button_id = -1;
	bool m_enter_pressed = false;
	bool m_escape_pressed = false;
	bool m_mouse_released = false;
	bool m_debug_draw = false;
};

} // namespace cc
