#pragma once
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <array>
#include "PlayerController.h"
#include "NetConstants.h"
#include "Appearance.h"
#include "PlayerStatusData.h"
#include "ActionID.h"
#include "DirectionHelpers.h"

class CItem;

using hb::shared::direction::direction;

namespace hb::client::config
{
constexpr int PlayerNameLength = 12;
constexpr int PlayerMaxMagicType = 100;
constexpr int PlayerMaxSkillType = 60;
} // namespace hb::client::config

//=============================================================================
// Player Animation Definitions
// All player types (1-6) share these timings.
// Values from original m_stFrame[1-6] initialization in MapData.cpp
//=============================================================================
struct AnimDef
{
	int16_t m_max_frame;
	int16_t m_frame_time;  // Base ms per frame (before status modifiers)
	bool    m_loop;
};

namespace PlayerAnim {
	static constexpr AnimDef stop       = { 14, 60,  false };
	static constexpr AnimDef Move       = {  7, 70,  false };
	static constexpr AnimDef Run        = {  7, 39,  false };
	static constexpr AnimDef Attack     = {  7, 78,  false };
	static constexpr AnimDef AttackMove = { 12, 78,  false };
	static constexpr AnimDef Magic      = { 15, 88,  false };
	static constexpr AnimDef GetItem    = {  3, 75, false };
	static constexpr AnimDef Damage     = {  7, 70,  false }; // 3+4
	static constexpr AnimDef DamageMove = {  3, 50,  false };
	static constexpr AnimDef Dying      = { 12, 80,  false };

	inline const AnimDef& from_action(int8_t action)
	{
		switch (action) {
		case hb::shared::action::Type::stop:       return stop;
		case hb::shared::action::Type::Move:       return Move;
		case hb::shared::action::Type::Run:        return Run;
		case hb::shared::action::Type::Attack:     return Attack;
		case hb::shared::action::Type::AttackMove: return AttackMove;
		case hb::shared::action::Type::Magic:      return Magic;
		case hb::shared::action::Type::GetItem:    return GetItem;
		case hb::shared::action::Type::Damage:     return Damage;
		case hb::shared::action::Type::DamageMove: return DamageMove;
		case hb::shared::action::Type::Dying:      return Dying;
		default:                   return stop;
		}
	}
}

class CPlayer
{
public:
    CPlayer();
    ~CPlayer();
    void reset();

    // Movement Controller
    CPlayerController m_Controller;

    // IDENTITY & ACCOUNT
    std::string m_player_name;
    short m_player_object_id;
    short m_player_type;
    // POSITION & MOVEMENT
    short m_player_x, m_player_y;
    direction m_player_dir;
    short m_damage_move;
    int m_damage_move_amount;

    // RESOURCES
    int m_hp, m_mp, m_sp, m_hunger_status;
    bool m_stats_initialized = false;

    // BASE STATS
    int m_str, m_vit, m_dex, m_int, m_mag, m_charisma;
    int m_angelic_str, m_angelic_int, m_angelic_dex, m_angelic_mag;

    // PROGRESSION
    int m_level;
    uint32_t m_exp;
    int m_lu_point;
    int16_t m_lu_str, m_lu_vit, m_lu_dex, m_lu_int, m_lu_mag, m_lu_char;
    int8_t m_stat_mod_str, m_stat_mod_vit, m_stat_mod_dex, m_stat_mod_int, m_stat_mod_mag, m_stat_mod_chr;

    // COMBAT
    int m_ac, m_thac0;
    hb::shared::entity::PlayerStatus m_playerStatus;
    int m_pk_count, m_enemy_kill_count, m_reward_gold, m_contribution;
    int m_super_attack_left, m_special_ability_type, m_special_ability_time_left_sec;

    // APPEARANCE
    hb::shared::entity::PlayerAppearance m_playerAppearance;

    // Illusion Effect appearance
    hb::shared::entity::PlayerStatus m_illusionStatus;
    hb::shared::entity::PlayerAppearance m_illusionAppearance;
    int8_t m_gender, m_skin_col, m_hair_style, m_hair_col, m_under_col;

    // SKILLS & MAGIC
    std::array<int8_t, hb::client::config::PlayerMaxMagicType> m_magic_mastery{};
    std::array<uint8_t, hb::client::config::PlayerMaxSkillType> m_skill_mastery{};

    // STATUS FLAGS
    bool m_is_poisoned, m_is_confusion, m_paralyze;
    bool m_is_combat_mode, m_is_safe_attack_mode, m_force_attack;
    bool m_super_attack_mode, m_is_special_ability_enabled;
    bool m_hunter, m_aresden, m_citizen;

    // ADMIN / GM
    bool m_is_gm_mode = false;

    // CRUSADE/WAR
    int m_crusade_duty, m_war_contribution, m_construction_point;
    int m_construct_loc_x, m_construct_loc_y;

    // INVENTORY
    std::array<std::unique_ptr<CItem>, hb::shared::limits::MaxItems> m_item_list;

    // BANK
    std::array<std::unique_ptr<CItem>, hb::shared::limits::MaxBankItems> m_bank_list;
};
