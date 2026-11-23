#pragma once

#include "input.h"  // For KeyCode
#include <functional>
#include <vector>

class HotkeyManager
{
public:
	static HotkeyManager& get();

	enum class Trigger
	{
		KeyDown,
		KeyUp
	};

	struct KeyCombo
	{
		KeyCode vk;
		bool ctrl;
		bool shift;
		bool alt;
	};

	void clear();
	void register_hotkey(const KeyCombo& combo, Trigger trigger, std::function<void()> callback);
	bool handle_key_down(KeyCode vk);
	bool handle_key_up(KeyCode vk);

private:
	HotkeyManager() = default;
	~HotkeyManager() = default;
	HotkeyManager(const HotkeyManager&) = delete;
	HotkeyManager& operator=(const HotkeyManager&) = delete;

	struct Entry
	{
		KeyCombo combo;
		Trigger trigger;
		std::function<void()> callback;
	};

	bool handle_key(KeyCode vk, Trigger trigger);

	std::vector<Entry> m_entries;
};
