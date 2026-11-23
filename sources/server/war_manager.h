#pragma once
#include <cstddef>
#include <cstdint>

class CGame;

class WarManager
{
public:
	WarManager() = default;
	~WarManager() = default;
	void set_game(CGame* game) { m_game = game; }

	// ========================================================================
	// Crusade System
	// ========================================================================
	void crusade_war_starter();
	void global_start_crusade_mode();
	void local_start_crusade_mode(uint32_t guild_guid);
	void local_end_crusade_mode(int winner_side);
	void manual_end_crusade_mode(int winner_side);
	void create_crusade_structures();
	void remove_crusade_structures();
	void remove_crusade_npcs(void);
	void remove_crusade_recall_time(void);
	void sync_middleland_map_info();
	void select_crusade_duty_handler(int client_h, int duty);
	void check_crusade_result_calculation(int client_h);
	bool read_crusade_guid_file(const char* fn);
	void create_crusade_guid(uint32_t crusade_guid, int winner_side);
	void check_commander_construction_point(int client_h);
	bool set_construction_kit(int map_index, int dX, int dY, int type, int time_cost, int client_h);

	// ========================================================================
	// Grand Magic / Meteor Strike
	// ========================================================================
	void meteor_strike_handler(int map_index);
	void meteor_strike_msg_handler(char attacker_side);
	void calc_meteor_strike_effect_handler(int map_index);
	void do_meteor_strike_damage_handler(int map_index);
	void link_strike_point_map_index();
	void grand_magic_launch_msg_send(int type, char attacker_side);
	void grand_magic_result_handler(char* map_name, int crashed_structure_num, int structure_damage_amount, int casualities, int active_structure, int total_strike_points, char* data);
	void collected_mana_handler(uint16_t aresden_mana, uint16_t elvine_mana);
	void send_collected_mana();

	// ========================================================================
	// Map Status & Guild War Operations
	// ========================================================================
	void send_map_status(int client_h);
	void map_status_handler(int client_h, int mode, const char* map_name);
	void request_summon_war_unit_handler(int client_h, int dX, int dY, char type, char num, char mode);
	void request_guild_teleport_handler(int client_h);
	void request_set_guild_teleport_loc_handler(int client_h, int dX, int dY, int guild_guid, const char* map_name);
	void request_set_guild_construct_loc_handler(int client_h, int dX, int dY, int guild_guid, const char* map_name);

	// ========================================================================
	// Heldenian Battle System
	// ========================================================================
	void set_heldenian_mode();
	void global_start_heldenian_mode();
	void local_start_heldenian_mode(short v1, short v2, uint32_t heldenian_guid);
	void global_end_heldenian_mode();
	void local_end_heldenian_mode();
	bool update_heldenian_status();
	void create_heldenian_guid(uint32_t heldenian_guid, int winner_side);
	void manual_start_heldenian_mode(int client_h, char* data, size_t msg_size);
	void manual_end_heldenian_mode(int client_h, char* data, size_t msg_size);
	bool notify_heldenian_winner();
	void remove_heldenian_npc(int npc_h);
	void request_heldenian_teleport(int client_h, char* data, size_t msg_size);
	bool check_heldenian_map(int attacker_h, int map_index, char type);
	void check_heldenian_result_calculation(int client_h);
	void remove_occupy_flags(int map_index);

	// ========================================================================
	// Apocalypse System
	// ========================================================================
	void apocalypse_ender();
	void global_end_apocalypse_mode();
	void local_end_apocalypse();
	void local_start_apocalypse(uint32_t apocalypse_guid);
	bool read_apocalypse_guid_file(const char* fn);
	bool read_heldenian_guid_file(const char* fn);
	void create_apocalypse_guid(uint32_t apocalypse_guid);

	// ========================================================================
	// Energy Sphere & Occupy Territory
	// ========================================================================
	void energy_sphere_processor();
	bool check_energy_sphere_destination(int npc_h, short attacker_h, char attacker_type);
	void get_occupy_flag_handler(int client_h);
	size_t compose_flag_status_contents(char* data);
	void set_summon_mob_action(int client_h, int mode, size_t msg_size, char* data = 0);
	bool set_occupy_flag(char map_index, int dX, int dY, int side, int ek_num, int client_h);

	// ========================================================================
	// FightZone System
	// ========================================================================
	void fightzone_reserve_handler(int client_h, char* data, size_t msg_size);
	void fightzone_reserve_processor();
	void get_fightzone_ticket_handler(int client_h);

private:
	CGame* m_game = nullptr;
};
