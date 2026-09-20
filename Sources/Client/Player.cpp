#include "Player.h"
#include "Item/Item.h"

CPlayer::CPlayer()
{
    reset();
}

CPlayer::~CPlayer()
{
}

void CPlayer::reset()
{
    // IDENTITY & ACCOUNT
    m_player_name.clear();
    m_player_object_id = 0;
    m_player_type = 0;
    // POSITION & MOVEMENT
    m_player_x = 0;
    m_player_y = 0;
    m_player_dir = direction{};
    m_damage_move = 0;
    m_damage_move_amount = 0;

    // RESOURCES
    m_hp = 0;
    m_mp = 0;
    m_sp = 0;
    m_hunger_status = 0;
    m_stats_initialized = false;

    // BASE STATS
    m_str = 0;
    m_vit = 0;
    m_dex = 0;
    m_int = 0;
    m_mag = 0;
    m_charisma = 0;
    m_angelic_str = 0;
    m_angelic_int = 0;
    m_angelic_dex = 0;
    m_angelic_mag = 0;

    // PROGRESSION
    m_level = 0;
    m_exp = 0;
    m_lu_point = 0;
    m_lu_str = 0;
    m_lu_vit = 0;
    m_lu_dex = 0;
    m_lu_int = 0;
    m_lu_mag = 0;
    m_lu_char = 0;
    m_stat_mod_str = 0;
    m_stat_mod_vit = 0;
    m_stat_mod_dex = 0;
    m_stat_mod_int = 0;
    m_stat_mod_mag = 0;
    m_stat_mod_chr = 0;

    // COMBAT
    m_ac = 0;
    m_thac0 = 0;
    m_playerStatus.clear();
    m_pk_count = 0;
    m_enemy_kill_count = 0;
    m_reward_gold = 0;
    m_contribution = 0;
    m_super_attack_left = 0;
    m_special_ability_type = 0;
    m_special_ability_time_left_sec = 0;

    // APPEARANCE
    m_playerAppearance.clear();
    m_illusionStatus.clear();
    m_illusionAppearance.clear();
    m_gender = 0;
    m_skin_col = 0;
    m_hair_style = 0;
    m_hair_col = 0;
    m_under_col = 0;

    // SKILLS & MAGIC
    m_magic_mastery.fill(0);
    m_skill_mastery.fill(0);

    // STATUS FLAGS
    m_is_poisoned = false;
    m_is_confusion = false;
    m_paralyze = false;
    m_is_combat_mode = false;
    m_is_safe_attack_mode = false;
    m_force_attack = false;
    m_super_attack_mode = false;
    m_is_special_ability_enabled = false;
    m_hunter = false;
    m_aresden = false;
    m_citizen = false;

    // ADMIN / GM
    m_is_gm_mode = false;

    // CRUSADE/WAR
    m_crusade_duty = 0;
    m_war_contribution = 0;
    m_construction_point = 0;
    m_construct_loc_x = -1;
    m_construct_loc_y = -1;

    // INVENTORY
    for (auto& item : m_item_list) item.reset();

    // BANK
    for (auto& item : m_bank_list) item.reset();

    // MOVEMENT CONTROLLER
    m_Controller.reset();
}
