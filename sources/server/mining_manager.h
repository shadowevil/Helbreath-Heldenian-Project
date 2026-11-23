// mining_manager.h: Manages mineral spawning, mining actions, and mineral lifecycle.
// Extracted from CGame (Phase B1).

#pragma once

#include "common_types.h"
#include "mineral.h"
#include "game.h"  // For hb::server::config constants

class MiningManager
{
public:
	MiningManager();
	~MiningManager();

	void set_game(CGame* game) { m_game = game; }

	// Lifecycle
	void init_arrays();
	void cleanup_arrays();

	// Core mining methods (moved from CGame)
	void mineral_generator();
	int create_mineral(char map_index, int tX, int tY, char level);
	void check_mining_action(int client_h, int dX, int dY);
	bool delete_mineral(int index);

private:
	CGame* m_game = nullptr;
	CMineral* m_mineral[hb::server::config::MaxMinerals]{};
};
