// RegenManager.cpp: Player HP/MP/SP regen, hunger consumption, and poison ticks.

#include "RegenManager.h"
#include "Game.h"
#include "Client.h"
#include "CombatManager.h"
#include "SharedCalculations.h"
#include "Log.h"
#include <algorithm>

using namespace hb::shared::net;

void RegenManager::process_client_tick(int client_h, uint32_t time)
{
	if (m_game->m_client_list[client_h] == nullptr) return;

	tick_hunger(client_h, time);

	int hunger_delay = calc_hunger_delay(m_game->m_client_list[client_h]->m_hunger_status);

	tick_hp(client_h, time, hunger_delay);
	tick_mp(client_h, time, hunger_delay);
	tick_sp(client_h, time, hunger_delay);
	tick_poison(client_h, time);
}

bool RegenManager::is_regen_suppressed(int client_h) const
{
	if (m_game->m_client_list[client_h] == nullptr) return true;
	if (m_game->m_client_list[client_h]->m_is_killed) return true;
	if (m_game->m_client_list[client_h]->m_is_init_complete == false) return true;
	if (m_game->m_client_list[client_h]->m_hunger_status <= 0) return true;
	if (m_game->m_client_list[client_h]->m_skill_using_status[19]) return true;
	return false;
}

void RegenManager::tick_hunger(int client_h, uint32_t time)
{
	if (m_game->m_client_list[client_h]->m_is_killed) return;

	if ((time - m_game->m_client_list[client_h]->m_hunger_time) > (uint32_t)m_game->m_hunger_consume_interval)
	{
		m_game->m_client_list[client_h]->m_hunger_status--;
		if (m_game->m_client_list[client_h]->m_hunger_status <= 0)
			m_game->m_client_list[client_h]->m_hunger_status = 0;
		m_game->m_client_list[client_h]->m_hunger_time += m_game->m_hunger_consume_interval;
		m_game->send_notify_msg(0, client_h, Notify::Hunger, m_game->m_client_list[client_h]->m_hunger_status, 0, 0, 0);
	}
}

void RegenManager::tick_hp(int client_h, uint32_t time, int hunger_delay_ms)
{
	if ((time - m_game->m_client_list[client_h]->m_hp_time) > (uint32_t)(m_game->m_health_regen_interval + hunger_delay_ms))
	{
		if (!is_regen_suppressed(client_h))
		{
			int max_hp = m_game->get_max_hp(client_h);
			if (m_game->m_client_list[client_h]->m_hp < max_hp)
			{
				int amount = calc_hp_regen(client_h);
				m_game->m_client_list[client_h]->m_hp += amount;
				if (m_game->m_client_list[client_h]->m_hp > max_hp)
					m_game->m_client_list[client_h]->m_hp = max_hp;
				if (m_game->m_client_list[client_h]->m_hp <= 0)
					m_game->m_client_list[client_h]->m_hp = 0;
				m_game->send_notify_msg(0, client_h, Notify::Hp, 0, 0, 0, 0);
			}
		}
		m_game->m_client_list[client_h]->m_hp_stock = 0;
		m_game->m_client_list[client_h]->m_hp_time += (uint32_t)(m_game->m_health_regen_interval + hunger_delay_ms);
	}
}

void RegenManager::tick_mp(int client_h, uint32_t time, int hunger_delay_ms)
{
	if ((time - m_game->m_client_list[client_h]->m_mp_time) > (uint32_t)(m_game->m_mana_regen_interval + hunger_delay_ms))
	{
		if (!is_regen_suppressed(client_h))
		{
			int max_mp = m_game->get_max_mp(client_h);
			if (m_game->m_client_list[client_h]->m_mp < max_mp)
			{
				int amount = calc_mp_regen(client_h);
				m_game->m_client_list[client_h]->m_mp += amount;
				if (m_game->m_client_list[client_h]->m_mp > max_mp)
					m_game->m_client_list[client_h]->m_mp = max_mp;
				m_game->send_notify_msg(0, client_h, Notify::Mp, 0, 0, 0, 0);
			}
		}
		m_game->m_client_list[client_h]->m_mp_time += (uint32_t)(m_game->m_mana_regen_interval + hunger_delay_ms);
	}
}

void RegenManager::tick_sp(int client_h, uint32_t time, int hunger_delay_ms)
{
	if ((time - m_game->m_client_list[client_h]->m_sp_time) > (uint32_t)(m_game->m_stamina_regen_interval + hunger_delay_ms))
	{
		if (!is_regen_suppressed(client_h))
		{
			int max_sp = m_game->get_max_sp(client_h);
			if (m_game->m_client_list[client_h]->m_sp < max_sp)
			{
				int amount = calc_sp_regen(client_h);
				m_game->m_client_list[client_h]->m_sp += amount;
				if (m_game->m_client_list[client_h]->m_sp > max_sp)
					m_game->m_client_list[client_h]->m_sp = max_sp;
				m_game->send_notify_msg(0, client_h, Notify::Sp, 0, 0, 0, 0);
			}
		}
		m_game->m_client_list[client_h]->m_sp_time += (uint32_t)(m_game->m_stamina_regen_interval + hunger_delay_ms);
	}
}

void RegenManager::tick_poison(int client_h, uint32_t time)
{
	if (m_game->m_client_list[client_h]->m_is_poisoned &&
		(time - m_game->m_client_list[client_h]->m_poison_time) > (uint32_t)m_game->m_poison_damage_interval)
	{
		m_game->m_combat_manager->poison_effect(client_h, 0);
		m_game->m_client_list[client_h]->m_poison_time += m_game->m_poison_damage_interval;
	}
}

int RegenManager::calc_hp_regen(int client_h) const
{
	auto* p = m_game->m_client_list[client_h];

	// Evaluate formulas from engine
	int ceiling = hb::shared::calc::hp_regen_max_roll(m_game->m_formula_engine,
		hb::shared::calc::vit{(double)p->m_vit});
	int base_floor = hb::shared::calc::hp_regen_min_roll(m_game->m_formula_engine,
		hb::shared::calc::vit{(double)p->m_vit});
	int variance = hb::shared::calc::hp_regen_roll_variance(m_game->m_formula_engine,
		hb::shared::calc::vit{(double)p->m_vit});

	if (ceiling <= 0) return 0;

	// Pick outcome type first: 1=low, 2=high, 3=average
	int pick = m_game->dice(1, 3);

	// Roll 3 times for the selected outcome
	int rolls[3];
	for (int i = 0; i < 3; i++)
	{
		int roll = m_game->dice(1, ceiling);
		rolls[i] = std::max(base_floor, roll);
	}

	int base;
	if (pick == 1)
		base = std::min({rolls[0], rolls[1], rolls[2]});
	else if (pick == 2)
		base = std::max({rolls[0], rolls[1], rolls[2]});
	else
		base = (rolls[0] + rolls[1] + rolls[2]) / 3;

	// Add variance bonus: [0, variance]
	if (variance > 0)
		base += m_game->dice(1, variance + 1) - 1;

	// Existing modifiers
	if (p->m_side_effect_max_hp_down != 0)
		base -= (base / p->m_side_effect_max_hp_down);
	int total = base + p->m_hp_stock;
	total = apply_percent_bonus(total, p->m_add_hp);
	return total;
}

int RegenManager::calc_mp_regen(int client_h) const
{
	auto* p = m_game->m_client_list[client_h];

	// Evaluate formulas from engine
	int ceiling = hb::shared::calc::mp_regen_max_roll(m_game->m_formula_engine,
		hb::shared::calc::mag{(double)p->m_mag},
		hb::shared::calc::angelic_mag{(double)p->m_angelic_mag});
	int base_floor = hb::shared::calc::mp_regen_min_roll(m_game->m_formula_engine,
		hb::shared::calc::mag{(double)p->m_mag},
		hb::shared::calc::angelic_mag{(double)p->m_angelic_mag});
	int variance = hb::shared::calc::mp_regen_roll_variance(m_game->m_formula_engine,
		hb::shared::calc::mag{(double)p->m_mag},
		hb::shared::calc::angelic_mag{(double)p->m_angelic_mag});

	if (ceiling <= 0) return 0;

	// Pick outcome type first: 1=low, 2=high, 3=average
	int pick = m_game->dice(1, 3);

	// Roll 3 times for the selected outcome
	int rolls[3];
	for (int i = 0; i < 3; i++)
	{
		int roll = m_game->dice(1, ceiling);
		rolls[i] = std::max(base_floor, roll);
	}

	int base;
	if (pick == 1)
		base = std::min({rolls[0], rolls[1], rolls[2]});
	else if (pick == 2)
		base = std::max({rolls[0], rolls[1], rolls[2]});
	else
		base = (rolls[0] + rolls[1] + rolls[2]) / 3;

	// Add variance bonus: [0, variance]
	if (variance > 0)
		base += m_game->dice(1, variance + 1) - 1;

	int total = apply_percent_bonus(base, p->m_add_mp);
	return total;
}

int RegenManager::calc_sp_regen(int client_h) const
{
	auto* p = m_game->m_client_list[client_h];

	// Evaluate formulas from engine
	int ceiling = hb::shared::calc::sp_regen_max_roll(m_game->m_formula_engine,
		hb::shared::calc::vit{(double)p->m_vit});
	int base_floor = hb::shared::calc::sp_regen_min_roll(m_game->m_formula_engine,
		hb::shared::calc::vit{(double)p->m_vit});
	int variance = hb::shared::calc::sp_regen_roll_variance(m_game->m_formula_engine,
		hb::shared::calc::vit{(double)p->m_vit});

	if (ceiling <= 0) return 0;

	// Pick outcome type first: 1=low, 2=high, 3=average
	int pick = m_game->dice(1, 3);

	// Roll 3 times for the selected outcome
	int rolls[3];
	for (int i = 0; i < 3; i++)
	{
		int roll = m_game->dice(1, ceiling);
		rolls[i] = std::max(base_floor, roll);
	}

	int base;
	if (pick == 1)
		base = std::min({rolls[0], rolls[1], rolls[2]});
	else if (pick == 2)
		base = std::max({rolls[0], rolls[1], rolls[2]});
	else
		base = (rolls[0] + rolls[1] + rolls[2]) / 3;

	// Add variance bonus: [0, variance]
	if (variance > 0)
		base += m_game->dice(1, variance + 1) - 1;

	int total = apply_percent_bonus(base, p->m_add_sp);

	return total;
}

int RegenManager::calc_hunger_delay(int hunger_status) const
{
	if (hunger_status > 30 || hunger_status < 0) return 0;
	return (30 - hunger_status) * 1000;
}

int RegenManager::apply_percent_bonus(int base, int percent)
{
	if (percent == 0) return base;
	int bonus = (int)((double)percent / 100.0f * (double)base);
	return base + bonus;
}
