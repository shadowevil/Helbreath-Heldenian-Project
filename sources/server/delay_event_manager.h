// delay_event_manager.h: Manages timed delay events (magic expiration, meteors, etc.).
// Extracted from game.cpp (Phase B5).

#pragma once

#include <cstdint>

class CGame;
class CDelayEvent;

class DelayEventManager
{
public:
	DelayEventManager() = default;
	~DelayEventManager() = default;

	void set_game(CGame* game) { m_game = game; }

	void init_arrays();
	void cleanup_arrays();

	// Core delay event management
	bool register_delay_event(int delay_type, int effect_type, uint32_t last_time, int target_h, char target_type, char map_index, int dX, int dY, int v1, int v2, int v3);
	bool remove_from_delay_event_list(int iH, char type, int effect_type);
	void delay_event_processor();
	void delay_event_process();

	// Public array for backward compatibility
	CDelayEvent* m_delay_event_list[60000];

private:
	CGame* m_game = nullptr;
};
