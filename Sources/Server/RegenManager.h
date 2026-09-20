#pragma once
#include <cstdint>

class CGame;

class RegenManager
{
public:
	RegenManager() = default;
	~RegenManager() = default;

	void set_game(CGame* game) { m_game = game; }

	// Called once per client per game tick from the main loop.
	// Handles hunger consumption, regen slowdown, HP/MP/SP regen, and poison.
	void process_client_tick(int client_h, uint32_t time);

private:
	CGame* m_game = nullptr;

	// Returns true if the client is in a state that blocks all passive regen
	// (dead, not initialized, hunger depleted, meditation active)
	bool is_regen_suppressed(int client_h) const;

	// Tick processors
	void tick_hunger(int client_h, uint32_t time);
	void tick_hp(int client_h, uint32_t time, int hunger_delay_ms);
	void tick_mp(int client_h, uint32_t time, int hunger_delay_ms);
	void tick_sp(int client_h, uint32_t time, int hunger_delay_ms);
	void tick_poison(int client_h, uint32_t time);

	// Regen formulas (pure calculations, no side effects)
	int calc_hp_regen(int client_h) const;
	int calc_mp_regen(int client_h) const;
	int calc_sp_regen(int client_h) const;

	// Returns additional milliseconds to add to regen intervals based on hunger.
	// hunger <= 30: (30 - hunger) * 1000 ms penalty. hunger > 30: 0.
	int calc_hunger_delay(int hunger_status) const;

	// Apply percentage bonus: base + (base * pct / 100), truncated to int
	static int apply_percent_bonus(int base, int percent);
};
