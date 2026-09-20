#pragma once

#include <cstdint>
#include "EntityRelationship.h"
#include "DirectionHelpers.h"
using hb::shared::direction::direction;

class CGame;

class CombatManager
{
public:
	CombatManager() = default;
	~CombatManager() = default;

	void set_game(CGame* game) { m_game = game; }

	// Core attack calculation
	uint32_t calculate_attack_effect(short target_h, char target_type, short attacker_h, char attacker_type, int tdX, int tdY, int attack_mode, bool near_attack = false, bool is_dash = false, bool arrow_use = false);
	bool calculate_durability_decrement(short target_h, short attacker_h, char attacker_type, char target_type, int armor_type);

	// Damage application
	void effect_damage_spot(short attacker_h, char attacker_type, short target_h, char target_type, short v1, short v2, short v3, bool exp, int attr = 0);
	void effect_damage_spot_damage_move(short attacker_h, char attacker_type, short target_h, char target_type, short atk_x, short atk_y, short v1, short v2, short v3, bool exp, int attr);
	void effect_hp_up_spot(short attacker_h, char attacker_type, short target_h, char target_type, short v1, short v2, short v3);
	void effect_sp_up_spot(short attacker_h, char attacker_type, short target_h, char target_type, short v1, short v2, short v3);
	void effect_sp_down_spot(short attacker_h, char attacker_type, short target_h, char target_type, short v1, short v2, short v3);

	// Resistance checks
	bool check_resisting_magic_success(direction attacker_dir, short target_h, char target_type, int hit_ratio);
	bool check_resisting_ice_success(direction attacker_dir, short target_h, char target_type, int hit_ratio);
	bool check_resisting_poison_success(short owner_h, char owner_type);

	// Kill handler
	void client_killed_handler(int client_h, int attacker_h, char attacker_type, short damage);

	// Combat status
	void poison_effect(int client_h, int v1);
	void check_fire_bluring(char map_index, int sX, int sY);
	void armor_life_decrement(int attacker_h, int target_h, char owner_type, int value);

	// Attack type helpers
	void check_attack_type(int client_h, short * spType);
	int  get_weapon_skill_type(int client_h);
	int  get_combo_attack_bonus(int skill, int combo_count);

	// Hostility / criminal
	bool analyze_criminal_action(int client_h, short dX, short dY, bool is_check = false);
	bool get_is_player_hostile(int client_h, int owner_h);
	int  get_player_relationship_raw(int client_h, int opponent_h);
	EntityRelationship get_player_relationship(int owner_h, int viewer_h);

	// Target management
	void remove_from_target(short target_h, char target_type, int code = 0);
	int  get_danger_value(int npc_h, short dX, short dY);

	// Combat validation
	bool check_client_attack_frequency(int client_h, uint32_t client_time);

	// Logging
	bool pk_log(int action, int attacker_h, int victum_h, char * npc);

private:
	CGame* m_game = nullptr;
};
