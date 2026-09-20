// Npc.h: interface for the CNpc class.

#pragma once


#include "CommonTypes.h"
#include "Magic.h"
#include "GameGeometry.h"
#include "Appearance.h"
#include "PlayerStatusData.h"
#include "DirectionHelpers.h"
using hb::shared::direction::direction;

namespace hb::server::npc
{

constexpr int MaxWaypoints = 10;

namespace MoveType
{
	enum : int
	{
		stop            = 0,
		SeqWaypoint     = 1,
		RandomWaypoint  = 2,
		Follow          = 3,
		RandomArea      = 4,
		Random          = 5,
		Guard           = 6,
	};
}

namespace Behavior
{
	enum : int
	{
		stop    = 0,
		Move    = 1,
		Attack  = 2,
		Flee    = 3,
		Dead    = 4,
	};
}

} // namespace hb::server::npc

class CNpc  
{
public:
	CNpc(const char * name5);
	virtual ~CNpc();

	// Auras
	char m_magic_config_list[100];

	char  m_npc_name[hb::shared::limits::NpcNameLen];

	char  m_name[6];
	char  m_map_index;
	short m_x, m_y;
	short m_prev_x, m_prev_y; // OPTIMIZATION FIX #3: Track previous position for delta detection
	short m_dx, m_dy;
	short m_vx, m_vy;
	int   m_tmp_error;
	hb::shared::geometry::GameRectangle  m_random_area;	// MOVETYPE_RANDOMAREA

	direction m_dir;
	char  m_action;
	char  m_turn;

	short m_type;
	short m_original_type;
	short m_npc_config_id;
	hb::shared::entity::EntityAppearance m_appearance;
	hb::shared::entity::EntityStatus m_status;

	uint32_t m_time;
	uint32_t m_action_time;
	uint32_t m_hp_up_time, m_mp_up_time;
	uint32_t m_dead_time, m_regen_time;

	int  m_hp, m_max_hp;						// Hit Point 
	uint32_t  m_exp;                    // ? ? . ExpDice  .

	int  m_hp_min;
	int  m_hp_max;
	int  m_hold_resist;
	int  m_defense_ratio;			// Defense Ratio
	int  m_hit_ratio;				// HitRatio
	int  m_magic_hit_ratio;
	int  m_min_bravery;
	uint32_t  m_exp_dice_min;
	uint32_t	 m_exp_dice_max;
	uint32_t  m_gold_dice_min;
	uint32_t  m_gold_dice_max;
	int   m_drop_table_id;

	char m_side;
	char m_action_limit;            // 1 Move   .    2    . 3 Dummy.  ,

	char m_size;					// 0: Small-Medium 1: Large
	int m_min_damage;
	int m_max_damage;
	int m_attack_bonus;
	char m_bravery;
	char m_resist_magic;
	char m_magic_level;
	char m_day_of_week_limit;
	char m_chat_msg_presence;		// ? Chat Msg
	int  m_mana;                   // MagicLevel*30
	int  m_max_mana;
																    
	char  m_move_type;
	char  m_behavior;
	short m_behavior_turn_count;
	char  m_target_search_range;

	int   m_follow_owner_index;
	char  m_follow_owner_type;		// (NPC or Player)
	bool  m_is_summoned;            // NPC? HP  .
	bool  m_bypass_mob_limit;        // GM-spawned: don't count toward map entity limit
	uint32_t m_summoned_time;

	int   m_target_index;
	char  m_target_type;			// (NPC or Player)
	char  m_cur_waypoint;
	char  m_total_waypoint;

	int   m_spot_mob_index;			// spot-mob-generator ?
	int   m_waypoint_index[hb::server::npc::MaxWaypoints+1];
	char  m_magic_effect_status[hb::server::config::MaxMagicEffects];

	bool  m_is_perm_attack_mode;
   	uint32_t   m_no_die_remain_exp;
	int   m_attack_strategy;
	int   m_ai_level;

	int   m_attack_range;
	/*
		AI-Level 
			1: ���� �ൿ 
			2: �������� ���� ���� ��ǥ���� ���� 
			3: ���� ��ȣ���� ��ǥ�� ���� ���ݴ�󿡼�? ���� 
	*/
	int   m_attack_count;
	bool  m_is_killed;
	bool  m_is_unsummoned;

	int   m_last_damage;
	int   m_summon_control_mode;		// ?: 0 Free, 1 Hold 2 Tgt
	char  m_attribute;				// :   1  2  3  4
	int   m_abs_damage;

	int   m_item_ratio;
	int   m_assigned_item;

	char  m_special_ability;

									/*
case 0: break;
case 1:  "Penetrating Invisibility"
case 2:  "Breaking Magic Protection"
case 3:  "Absorbing Physical Damage"
case 4:  "Absorbing Magical Damage"
case 5:  "Poisonous"
case 6:  "Extremely Poisonous"
case 7:  "Explosive"
case 8:  "Hi-Explosive" 

 ���� �� ���� 60���� ũ�� NPC�� ȿ���ʹ� �����ϹǷ� �����Ѵ�.
									*/

	int	  m_build_count;			// ?      .  m_min_bravery.
	int   m_mana_stock;
	bool  m_is_master;

	char m_crop_type;
	char m_crop_skill;

	int   m_v1;
	char m_area;

	int m_npc_item_type;
	int m_npc_item_max;

};
