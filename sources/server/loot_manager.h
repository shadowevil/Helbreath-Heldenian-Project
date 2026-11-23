#pragma once

#include <cstdint>

class CGame;

class LootManager
{
public:
	LootManager() = default;
	~LootManager() = default;

	void set_game(CGame* game) { m_game = game; }

	// Kill rewards
	void pk_kill_reward_handler(short attacker_h, short victum_h);
	void enemy_kill_reward_handler(int attacker_h, int client_h);
	void get_reward_money_handler(int client_h);

	// Kill penalties
	void apply_pk_penalty(short attacker_h, short victum_h);
	void apply_combat_killed_penalty(int client_h, int penalty_level, bool is_s_aattacked);
	void penalty_item_drop(int client_h, int total, bool is_s_aattacked = false);

private:
	CGame* m_game = nullptr;
};
