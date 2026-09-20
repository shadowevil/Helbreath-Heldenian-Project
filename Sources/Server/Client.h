// Client.h: interface for the CClient class.

#pragma once


#include "CommonTypes.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "ASIOSocket.h"
#include "Item.h"
#include "Magic.h"
#include "GlobalDef.h"
#include "NetConstants.h"
#include <fstream>
#include <vector>
#include <string>
#include <set>
#include "GameGeometry.h"
#include "Appearance.h"
#include "PlayerStatusData.h"
#include "StringCompat.h"
#include "DirectionHelpers.h"
using hb::shared::direction::direction;
using namespace std;

namespace hb::server::config { constexpr int ClientSocketBlockLimit = 2000; } // Queue size per client

// hb::shared::limits::MaxMagicType and hb::shared::limits::MaxSkillType are defined in NetConstants.h

namespace hb::server::config { constexpr int SpecialAbilityTimeSec = 1200; }

class CClient  
{
public:

	int m_angelic_str, m_angelic_int, m_angelic_dex, m_angelic_mag;
		
	char m_var;
	int m_recent_walk_time;
	int m_recent_run_time;
	short m_v1;
	char m_hero_armour_bonus;

	bool create_new_party();

	// Hack Checkers
	uint32_t m_magic_freq_time, m_move_freq_time, m_attack_freq_time;
	bool m_is_move_blocked, m_magic_item;
	uint32_t client_time;
	bool m_magic_confirm;
	int m_spell_count;
	bool m_magic_pause_time;
	//int m_iUninteruptibleCheck;
	//char m_cConnectionCheck;

	bool m_is_client_connected;
	uint32_t m_last_msg_id;
	uint32_t m_last_msg_time;
	size_t m_last_msg_size;
	uint32_t m_last_full_object_id;
	uint32_t m_last_full_object_time;

	CClient(asio::io_context& ctx);
	virtual ~CClient();

	char m_char_name[hb::shared::limits::CharNameLen];
	char m_account_name[hb::shared::limits::AccountNameLen];
	char m_account_password[hb::shared::limits::AccountPassLen];

	bool  m_is_init_complete;
	bool  m_is_msg_send_available;

	char  m_map_name[11];
	char  m_map_index;
	short m_x, m_y;
	
	char  m_location[11];

	direction m_dir;
	short m_type;
	short m_original_type;
	hb::shared::entity::PlayerAppearance m_appearance;
	hb::shared::entity::PlayerStatus m_status;

	// Get the weapon_class of the currently equipped weapon.
	// Returns weapon_class::none if unarmed. Reads directly from equipped item config.
	hb::shared::item::weapon_class::weapon_class get_equipped_weapon_class() const
	{
		using hb::shared::item::EquipPos;
		using hb::shared::item::to_int;
		int slot = m_item_equipment_status[to_int(EquipPos::RightHand)];
		if (slot == -1) slot = m_item_equipment_status[to_int(EquipPos::TwoHand)];
		if (slot == -1 || !m_item_list[slot]) return hb::shared::item::weapon_class::none;
		return m_item_list[slot]->get_weapon_class();
	}

	uint32_t m_time, m_hp_time, m_mp_time, m_sp_time, m_auto_save_time, m_hunger_time, m_warm_effect_time;
	uint32_t m_afk_activity_time;
	// Player

	char m_sex, m_skin, m_hair_style, m_hair_color, m_underwear;

	int  m_hp;						// Hit Point
	int  m_hp_stock;
	int  m_mp;
	int  m_sp;
	uint32_t  m_exp;
	uint32_t m_next_level_exp;
	bool m_is_killed;

	int  m_defense_ratio;		// Defense Ratio
	int  m_hit_ratio;			// Hit Ratio

	// int  m_iHitRatio_ItemEffect_SM; //    HitRatio
	//int  m_iHitRatio_ItemEffect_L;

	int  m_damage_absorption_armor[hb::shared::item::DEF_MAXITEMEQUIPPOS];		// Damage
	int  m_damage_absorption_shield;	// Parrying ?  Damage

	int  m_level;
	int  m_str, m_int, m_vit, m_dex, m_mag, m_charisma;
	// char m_cLU_Str, m_cLU_Int, m_cLU_Vit, m_cLU_Dex, m_cLU_Mag, m_cLU_Char;   //  ?  .
	int  m_luck; 
	int  m_levelup_pool;
	char m_aura;
	//MOG var - 3.51
	int m_gizon_item_upgrade_left;

	int m_add_trans_mana, m_add_charge_critical;


	uint32_t m_reward_gold;
	int  m_enemy_kill_count, m_player_kill_count;
	int  m_cur_weight_load;

	char m_side;
	
	bool m_inhibition;

	//50Cent - Repair All
	short total_item_repair;
	struct
	{
		char index;
		int32_t price;
	} m_repair_all[hb::shared::limits::MaxItems];

	int m_attack_dice_throw_sm;
	int m_attack_dice_range_sm;
	int m_attack_dice_throw_l;
	int m_attack_dice_range_l;
	int m_attack_bonus_sm;
	int m_attack_bonus_l;

	CItem * m_item_list[hb::shared::limits::MaxItems];
	hb::shared::geometry::GamePoint m_item_pos_list[hb::shared::limits::MaxItems];
	CItem * m_item_in_bank_list[hb::shared::limits::MaxBankItems];
	
	bool  m_is_item_equipped[hb::shared::limits::MaxItems];
	short m_item_equipment_status[hb::shared::item::DEF_MAXITEMEQUIPPOS];
	char  m_arrow_index;		// ?   .  -1( )

	char           m_magic_mastery[hb::shared::limits::MaxMagicType];
	unsigned char  m_skill_mastery[hb::shared::limits::MaxSkillType]; // v1.4

	int   m_skill_progress[hb::shared::limits::MaxSkillType];
	bool  m_skill_using_status[hb::shared::limits::MaxSkillType];
	int   m_skill_using_time_id[hb::shared::limits::MaxSkillType]; //v1.12

	char  m_magic_effect_status[hb::server::config::MaxMagicEffects];

	int   m_whisper_player_index;
	char  m_profile[256];

	int   m_hunger_status;

	uint32_t m_war_begin_time;
	bool  m_is_war_location;

	bool  m_is_poisoned;
	int   m_poison_level;
	uint32_t m_poison_time;
	
	int   m_penalty_block_year, m_penalty_block_month, m_penalty_block_day; // v1.4

	class hb::shared::net::ASIOSocket * m_socket;

	int   m_rating;

	int   m_time_left_rating;
	int   m_time_left_force_recall;
	int   m_time_left_firm_stamina;

	bool is_force_set;   //hbest
	time_t m_force_start;

	bool  m_is_on_server_change;

	uint32_t   m_exp_stock;
	uint32_t m_exp_stock_time;		 // ExpStock ? .

	uint32_t   m_auto_exp_amount;		 // Auto-Exp
	uint32_t m_auto_exp_time;		 // Auto-Exp ? .

	uint32_t m_recent_attack_time;

	int   m_allocated_fish;
	int   m_fish_chance;
	
	char  m_ip_address[21];		 // IP address
	bool  m_is_safe_attack_mode;

	bool  m_is_on_waiting_process;
	
	int   m_super_attack_left;
	int   m_super_attack_count;

	short m_using_weapon_skill;

	int   m_mana_save_ratio;
	
	bool  m_is_lucky_effect;
	int   m_side_effect_max_hp_down;

	int   m_combo_attack_count;
	int   m_down_skill_index;

	int   m_magic_damage_save_item_index;

	short m_char_id_num1, m_char_id_num2, m_char_id_num3;

	int   m_abuse_count;
	
	// bool  m_is_exchange_mode;		//    ??
	// int   m_exchange_h;				//  ?
	// char  m_exchange_name[11];		//  ?
	// char  m_cExchangeItemName[21];	//
	// char  m_exchange_item_index;  //
	// int   m_exchange_item_amount; //
	// bool  m_is_exchange_confirm;  //

	bool  m_is_exchange_mode;			// Is In Exchange Mode? 
	int   m_exchange_h;					// Client ID to Exchanging with 
	char  m_exchange_name[hb::shared::limits::CharNameLen];	// Name of Client to Exchanging with
	short m_exchange_item_id[4];	// Item ID to validate exchange hasn't been tampered

	char  m_exchange_item_index[4];		// ItemID to Exchange
	int   m_exchange_item_amount[4];		// Ammount to exchange with

	bool  m_is_exchange_confirm;			// Has the user hit confirm? 
	int	  exchange_count;				//Keeps track of items which are on list

	int   m_quest;				 // ? Quest
	int   m_quest_id;			 // ? Quest ID
	int   m_asked_quest;
	int   m_cur_quest_count;
	
	int   m_quest_reward_type;
	int   m_quest_reward_amount;

	int   m_contribution;

	bool  m_quest_match_flag_loc;
	bool  m_is_quest_completed;

	int   m_custom_item_value_attack;
	int   m_custom_item_value_defense;

	int   m_min_attack_power_sm;			// Custom-Item    AP
	int   m_min_attack_power_l;

	int   m_max_attack_power_sm;			// Custom-Item    AP
	int   m_max_attack_power_l;

	bool  m_is_neutral;
	bool  m_is_observer_mode;

	int   m_special_event_id;

	int   m_special_weapon_effect_type;
	int   m_special_weapon_effect_value;
	// 0-None 1-? 2- 3-
	// 5- 6- 7- 8- 9-

	// v1.42
	int   m_add_hp, m_add_sp, m_add_mp; 
	int   m_add_attack_ratio, m_add_poison_resistance, m_add_defense_ratio;
	int   m_add_magic_resistance, m_add_abs_physical_defense, m_add_abs_magical_defense; 
	int   m_add_combo_damage, m_add_exp, m_add_gold;

	int   m_add_resist_magic;
	int   m_add_physical_damage;
	int   m_add_magical_damage;	

	int   m_add_abs_air;
	int   m_add_abs_earth;
	int   m_add_abs_fire;
	int   m_add_abs_water;
	
	int   m_last_damage;

	int   m_move_msg_recv_count, m_attack_msg_recv_count, m_run_msg_recv_count, m_skill_msg_recv_count;
	uint32_t m_move_last_action_time, m_run_last_action_time, m_attack_last_action_time;

	int   m_special_ability_time;
	bool  m_is_special_ability_enabled;
	uint32_t m_special_ability_start_time;
	int   m_special_ability_last_sec;

	int   m_special_ability_type;
	int   m_special_ability_equip_pos;
	int   m_alter_item_drop_index;

	int   m_war_contribution;

	uint32_t m_speed_hack_check_time;
	int   m_speed_hack_check_exp;		
	uint32_t m_logout_hack_check;

	uint32_t m_initial_check_time_received;
	uint32_t m_initial_check_time;

	char  m_locked_map_name[11];
	int   m_locked_map_time;

	int   m_crusade_duty;						// : 1-. 2-. 3-
	uint32_t m_crusade_guid;						// GUID
	uint32_t m_heldenian_guid;
	bool m_in_recall_impossible_map;

	struct {
		char type;
		char side;
		short x, y;
	} m_crusade_structure_info[hb::shared::limits::MaxCrusadeStructures];
	int m_crusade_info_send_point;

	char m_sending_map_name[11];
	bool m_is_sending_map_status;

	int  m_construction_point;

	// 2.06
	bool m_is_player_civil;
	bool m_is_attack_mode_change;

	// New 06/05/2004
	// Party Stuff
	int m_party_id;
	int m_party_status;
	int m_req_join_party_client_h;
	char m_req_join_party_name[hb::shared::limits::CharNameLen];

	int   m_party_rank;										// Party . -1 . 1  . 12 ?
	int   m_party_member_count;
	int   m_party_guid;										// v1.42 Party GUID
	struct {
	int  index;
	char name[hb::shared::limits::CharNameLen];
	} m_party_member_name[hb::shared::limits::MaxPartyMembers];

	// New 07/05/2004
	uint32_t m_last_action_time;
	int m_dead_penalty_time;

	// New 16/05/2004
	char m_whisper_player_name[hb::shared::limits::CharNameLen];
	bool m_is_inside_warehouse;
	bool m_is_inside_wizard_tower;
	bool m_is_inside_own_town;
	bool m_is_checking_whisper_player;
	bool m_is_own_location;
	bool m_is_processing_allowed;

	// Updated 10/11/2004 - 24/05/2004
	char m_hero_armor_bonus;

	// New 25/05/2004
	bool m_is_being_resurrected;

	char m_save_count;

	uint32_t m_last_version_warning_time = 0;
	uint32_t m_last_config_request_time = 0;
	uint32_t m_last_damage_taken_time = 0;

	// Admin / GM
	bool m_is_gm_mode = false;
	bool m_is_admin_invisible = false;
	uint32_t m_last_gm_immune_notify_time = 0;
	int m_admin_index = -1;
	int m_admin_level = 0;

	// Block list
	struct CaseInsensitiveLess {
		bool operator()(const std::string& a, const std::string& b) const {
			return hb_stricmp(a.c_str(), b.c_str()) < 0;
		}
	};
	std::set<std::string, CaseInsensitiveLess> m_blocked_accounts;
	std::vector<std::pair<std::string, std::string>> m_blocked_accounts_list;
	bool m_block_list_dirty = false;

};
