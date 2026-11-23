// quest.h: interface for the CQuest class.

#pragma once

#include "common_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>


namespace hb::server::quest
{
namespace Type
{
	enum : int
	{
		MonsterHunt             = 1,
		MonsterHuntTimeLimit    = 2,
		Assassination           = 3,
		Delivery                = 4,
		Escort                  = 5,
		Guard                   = 6,
		GoPlace                 = 7,
		BuildStructure          = 8,
		SupplyBuildStructure    = 9,
		StrategicStrike         = 10,
		SendToBattle            = 11,
		SetOccupyFlag           = 12,
	};
}
} // namespace hb::server::quest

class CQuest  
{
public:
	
	char m_side;
	
	int m_type;				// Quest
	int m_target_config_id;			// Quest target NPC config ID
	int m_max_count;

	int m_from;				// Quest  NPC
	
	int m_min_level;			// Quest    .
	int m_max_level;			// Quest

	int m_required_skill_num;
	int m_required_skill_level;

	int m_time_limit;
	int m_assign_type;			// . -1 . 1 Crusade .

								// . 3  1  . 0   .
	int m_reward_type[4]; 
	int m_reward_amount[4];

	int m_contribution;
	int m_contribution_limit;

	int m_response_mode;		// : 0(ok) 1(Accept/Decline) 2(Next)

	char m_target_name[hb::shared::limits::NpcNameLen];
	int  m_x, m_y, m_range;

	int  m_quest_id;

	int  m_req_contribution;


	//CQuest();
	//virtual ~CQuest();

};
