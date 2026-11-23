// crafting_manager.h: Manages potion brewing and crafting recipe processing.
// Extracted from CGame (Phase B2).

#pragma once

#include "common_types.h"
#include "portion.h"
#include "game.h"  // For hb::server::config constants

class CraftingManager
{
public:
	CraftingManager();
	~CraftingManager();

	void set_game(CGame* game) { m_game = game; }

	// Lifecycle
	void init_arrays();
	void cleanup_arrays();

	// Crafting handlers (moved from CGame)
	void req_create_portion_handler(int client_h, char* data);
	void req_create_crafting_handler(int client_h, char* data);

	// Config arrays (public for database loading)
	CPortion* m_portion_config_list[hb::server::config::MaxPortionTypes]{};
	CPortion* m_crafting_config_list[hb::server::config::MaxPortionTypes]{};

private:
	CGame* m_game = nullptr;
};
