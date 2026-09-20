// Client.cpp: implementation of the CClient class.

#include "CommonTypes.h"
#include "Client.h"
#ifdef _WIN32
#endif

using namespace hb::shared::item;
using namespace hb::server::config;
using namespace hb::shared::direction;

extern char G_cTxt[512];

// Construction/Destruction

CClient::CClient(asio::io_context& ctx)
{
 

	m_socket = 0;
	m_socket = new class hb::shared::net::ASIOSocket(ctx, hb::server::config::ClientSocketBlockLimit);
	m_socket->init_buffer_size(hb::shared::limits::MsgBufferSize);

	std::memset(m_profile, 0, sizeof(m_profile));
	strcpy(m_profile, "__________");

	std::memset(m_char_name, 0, sizeof(m_char_name));
	std::memset(m_account_name, 0, sizeof(m_account_name));
	std::memset(m_account_password, 0, sizeof(m_account_password));

	std::memset(m_location, 0, sizeof(m_location));
	strcpy(m_location, "NONE");

	m_is_init_complete = false;
	m_is_client_connected = false;
	m_last_msg_id = 0;
	m_last_msg_time = 0;
	m_last_msg_size = 0;
	m_last_full_object_id = 0;
	m_last_full_object_time = 0;
	m_afk_activity_time = 0;

	m_angelic_str = m_angelic_int = m_angelic_dex = m_angelic_mag = 0;

	//m_cLU_Str = m_cLU_Int = m_cLU_Vit = m_cLU_Dex = m_cLU_Mag = m_cLU_Char = 0;
	m_levelup_pool = 0;
	m_aura = 0;

	//m_iHitRatio_ItemEffect_SM = 0;
	//m_iHitRatio_ItemEffect_L  = 0;
	m_var = 0;
	m_enemy_kill_count = 0;
	m_player_kill_count = 0;
	m_reward_gold = 0;
	m_cur_weight_load = 0;
	m_logout_hack_check = 0;

	// Charges
	m_add_trans_mana = 0;
	m_add_charge_critical = 0;

	m_is_safe_attack_mode  = false;

	//50Cent - Repair All
	total_item_repair = 0;
	for(int i = 0; i < hb::shared::limits::MaxItems; i++) {
		m_repair_all[i].index = 0;
		m_repair_all[i].price = 0;
	}

	for(int i = 0; i < DEF_MAXITEMEQUIPPOS; i++) 
		m_item_equipment_status[i] = -1;
	
	// initialize item list 
	for(int i = 0; i < hb::shared::limits::MaxItems; i++) {
		m_item_list[i]       = 0;
		m_item_pos_list[i].x   = 40;
		m_item_pos_list[i].y   = 30;
		m_is_item_equipped[i] = false;
	}
	m_arrow_index = -1;

	// initialize item list.
	for(int i = 0; i < hb::shared::limits::MaxBankItems; i++) {
		m_item_in_bank_list[i] = 0;
	}

	// Magic - Skill
	for(int i = 0; i < hb::shared::limits::MaxMagicType; i++)
		m_magic_mastery[i] = 0;
	
	for(int i = 0; i < hb::shared::limits::MaxSkillType; i++)
		m_skill_mastery[i] = 0;

	for(int i = 0; i < hb::shared::limits::MaxSkillType; i++) {
		m_skill_using_status[i] = false;
		m_skill_using_time_id[i] = 0;
	}

	// testcode
	m_map_index = -1;
	m_x = -1;
	m_y = -1;
	m_dir = direction::south;
	m_type   = 0;
	m_original_type = 0;
	m_appearance.clear();
	m_status.clear();

	m_sex  = 0;
	m_skin = 0;
	m_hair_style  = 0;
	m_hair_color  = 0;
	m_underwear  = 0;

	m_attack_dice_throw_sm = 0;	// @@@@@@@@@@@@@
	m_attack_dice_range_sm = 0;
	m_attack_dice_throw_l = 0;	// @@@@@@@@@@@@@
	m_attack_dice_range_l = 0;
	m_attack_bonus_sm    = 0;
	m_attack_bonus_l     = 0;
	
	// NPC   .
	m_side = 0;

	m_hit_ratio = 0;
	m_defense_ratio = 0;
	
	for(int i = 0; i < DEF_MAXITEMEQUIPPOS; i++) m_damage_absorption_armor[i]  = 0;
	m_damage_absorption_shield = 0;

	m_hp_stock = 0;
	m_is_killed = false;

	for(int i = 0; i < hb::server::config::MaxMagicEffects; i++) 
		m_magic_effect_status[i]	= 0;

	m_whisper_player_index = -1;
	std::memset(m_whisper_player_name, 0, sizeof(m_whisper_player_name));

	m_hunger_status  = 100;  // Maximum value is 100
	
	m_is_war_location = false;

	m_is_poisoned    = false;
	m_poison_level   = 0;

	m_rating          = 0;
	m_time_left_rating = 0;
	m_time_left_force_recall  = 0;
	m_time_left_firm_stamina = 0;
	
	m_recent_walk_time  = 0;
	m_recent_run_time   = 0;
	m_v1			   = 0;

	m_is_on_server_change  = false;
	m_inhibition = false;

	m_exp_stock = 0;

	m_allocated_fish = 0;
	m_fish_chance    = 0;

	std::memset(m_ip_address, 0, sizeof(m_ip_address)); 
	m_is_on_waiting_process = false;

	m_super_attack_left  = 0;
	m_super_attack_count = 0;

	m_using_weapon_skill = 5;

	m_mana_save_ratio   = 0;
	m_add_resist_magic  = 0;
	m_add_physical_damage = 0;
	m_add_magical_damage  = 0;
	m_is_lucky_effect     = false;
	m_side_effect_max_hp_down = 0;

	m_add_abs_air   = 0;
	m_add_abs_earth = 0;
	m_add_abs_fire  = 0;
	m_add_abs_water = 0;

	m_combo_attack_count = 0;
	m_down_skill_index   = -1;
	m_in_recall_impossible_map = 0;

	m_magic_damage_save_item_index = -1;

	m_char_id_num1 = m_char_id_num2 = m_char_id_num3 = 0;

	// New 06/05/2004
	m_party_id = 0;
	m_party_status = 0;
	m_req_join_party_client_h = 0;
	std::memset(m_req_join_party_name, 0, sizeof(m_req_join_party_name));

	/*m_party_rank = -1; // v1.42
	m_party_member_count = 0;
	m_party_guid        = 0;

	for(int i = 0; i < hb::shared::limits::MaxPartyMembers; i++) {
		m_party_member_name[i].index = 0;
		std::memset(m_party_member_name[i].name, 0, sizeof(m_party_member_name[i].name));
	}*/

	m_abuse_count     = 0;
	m_is_exchange_mode = false;

	//hbest
	is_force_set = false;

	m_penalty_block_year = m_penalty_block_month = m_penalty_block_day = 0; // v1.4

	m_exchange_h = 0;
	std::memset(m_exchange_name, 0, sizeof(m_exchange_name));
	for(int i = 0; i<4; i++){
		m_exchange_item_id[i] = 0;
		m_exchange_item_index[i]  = -1;
		m_exchange_item_amount[i] = 0;
	}

	m_is_exchange_confirm = false;

	m_quest		 = 0; // Currently assigned Quest 
	m_quest_id       = 0; // QuestID
	m_asked_quest	 = 0;
	m_cur_quest_count = 0;

	m_quest_reward_type	 = 0;
	m_quest_reward_amount = 0;

	m_contribution = 0;			// Contribution 
	m_quest_match_flag_loc = false;  // Quest location verification flag.
	m_is_quest_completed   = false;

	m_hero_armour_bonus = 0;

	m_is_neutral      = false;
	m_is_observer_mode = false;

	// 2000.8.1
	m_special_event_id = 200081;

	m_special_weapon_effect_type  = 0;	// : 0-None 1- 2- 3- 4-
	m_special_weapon_effect_value = 0;

	m_add_hp = m_add_sp = m_add_mp = 0; 
	m_add_attack_ratio = m_add_poison_resistance = m_add_defense_ratio = 0;
	m_add_abs_physical_defense = m_add_abs_magical_defense = 0;
	m_add_combo_damage = m_add_exp = m_add_gold = 0;
		
	m_special_ability_time = SpecialAbilityTimeSec;		// Special ability can be used once every SpecialAbilityTimeSec seconds.
	m_special_ability_type = 0;
	m_is_special_ability_enabled = false;
	m_special_ability_last_sec   = 0;

	m_special_ability_equip_pos  = 0;

	m_move_msg_recv_count   = 0;
	m_attack_msg_recv_count = 0;
	m_run_msg_recv_count    = 0;
	m_skill_msg_recv_count  = 0;

	m_alter_item_drop_index = -1;

	m_auto_exp_amount = 0;
	m_war_contribution = 0;

	m_move_last_action_time = m_run_last_action_time = m_attack_last_action_time = 0;

	m_initial_check_time_received = 0;
	m_initial_check_time = 0;

	std::memset(m_locked_map_name, 0, sizeof(m_locked_map_name));
	strcpy(m_locked_map_name, "NONE");
	m_locked_map_time = 0;

	m_crusade_duty  = 0;
	m_crusade_guid = 0;
	m_heldenian_guid = 0;

	for(int i = 0; i < hb::shared::limits::MaxCrusadeStructures; i++) {
		m_crusade_structure_info[i].type = 0;
		m_crusade_structure_info[i].side = 0;
		m_crusade_structure_info[i].x = 0;
		m_crusade_structure_info[i].y = 0;
	}

	m_crusade_info_send_point = 0;

	m_is_sending_map_status = false;
	std::memset(m_sending_map_name, 0, sizeof(m_sending_map_name));

	m_construction_point = 0;

	m_is_inside_warehouse = false;
	m_is_inside_wizard_tower = false;
	m_is_inside_own_town = false;
	m_is_checking_whisper_player = false;
	m_is_own_location = false;
	m_is_processing_allowed = false;

	m_hero_armor_bonus = 0;

	m_is_being_resurrected = false;
	m_magic_confirm = false;
	m_magic_item = false;
	m_spell_count = 0;
	m_magic_pause_time = false;

}

CClient::~CClient()
{
	if (m_socket != 0)
		delete m_socket;

	for(int i = 0; i < hb::shared::limits::MaxItems; i++)
	{
		if (m_item_list[i] != 0) {
			delete m_item_list[i];
			m_item_list[i] = 0;
		}
	}

	for(int i = 0; i < hb::shared::limits::MaxBankItems; i++)
	{
		if (m_item_in_bank_list[i] != 0) {
			delete m_item_in_bank_list[i];
			m_item_in_bank_list[i] = 0;
		}
	}
}

bool CClient::create_new_party()
{
 

	if (m_party_rank != -1) return false;

	m_party_rank = 0;
	m_party_member_count = 0;
	m_party_guid = (rand() % 999999) + GameClock::GetTimeMS();

	for(int i = 0; i < hb::shared::limits::MaxPartyMembers; i++) {
		m_party_member_name[i].index = 0;
		std::memset(m_party_member_name[i].name, 0, sizeof(m_party_member_name[i].name));
	}

	return true;
}

