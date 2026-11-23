// fishing_manager.h: Manages fish spawning, catch processing, and fishing interactions.
// Extracted from CGame (Phase B1).

#pragma once

#include "common_types.h"
#include "fish.h"
#include "game.h"  // For hb::server::config constants

class FishingManager
{
public:
	FishingManager();
	~FishingManager();

	void set_game(CGame* game) { m_game = game; }

	// Lifecycle
	void init_arrays();
	void cleanup_arrays();

	// Core fishing methods (moved from CGame)
	void fish_generator();
	void fish_processor();
	void req_get_fish_this_time_handler(int client_h);
	int check_fish(int client_h, char map_index, short dX, short dY);
	int create_fish(char map_index, short sX, short sY, short type, CItem* item, int difficulty, uint32_t last_time);
	bool delete_fish(int handle, int del_mode);

	// Called by CGame when a client disconnects or stops using skills
	void release_fish_engagement(int client_h);

	// Timer
	uint32_t m_fish_time = 0;

private:
	CGame* m_game = nullptr;
	CFish* m_fish[hb::server::config::MaxFishs]{};
};
