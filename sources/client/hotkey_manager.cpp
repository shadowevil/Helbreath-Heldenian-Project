#include "hotkey_manager.h"
#include "input.h"
#include <utility>

HotkeyManager& HotkeyManager::get()
{
	static HotkeyManager s_instance;
	return s_instance;
}

void HotkeyManager::clear()
{
	m_entries.clear();
}

void HotkeyManager::register_hotkey(const KeyCombo& combo, Trigger trigger, std::function<void()> callback)
{
	m_entries.push_back(Entry{ combo, trigger, std::move(callback) });
}

bool HotkeyManager::handle_key_down(KeyCode vk)
{
	return handle_key(vk, Trigger::KeyDown);
}

bool HotkeyManager::handle_key_up(KeyCode vk)
{
	return handle_key(vk, Trigger::KeyUp);
}

bool HotkeyManager::handle_key(KeyCode vk, Trigger trigger)
{
	if (m_entries.empty()) {
		return false;
	}

	const bool ctrlDown = hb::shared::input::is_ctrl_down();
	const bool shiftDown = hb::shared::input::is_shift_down();
	const bool altDown = hb::shared::input::is_alt_down();

	bool handled = false;
	for (const auto& entry : m_entries)
	{
		if (entry.trigger != trigger || entry.combo.vk != vk) {
			continue;
		}
		if (entry.combo.ctrl && !ctrlDown) {
			continue;
		}
		if (!entry.combo.ctrl && ctrlDown) {
			continue;
		}
		if (entry.combo.shift && !shiftDown) {
			continue;
		}
		if (!entry.combo.shift && shiftDown) {
			continue;
		}
		if (entry.combo.alt && !altDown) {
			continue;
		}
		if (!entry.combo.alt && altDown) {
			continue;
		}
		if (entry.callback) {
			entry.callback();
			handled = true;
		}
	}

	return handled;
}
