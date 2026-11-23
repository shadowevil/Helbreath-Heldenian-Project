// delay_event.h: interface for the CDelayEvent class.
//
//////////////////////////////////////////////////////////////////////

#pragma once


#include "common_types.h"

namespace hb::server::delay_event
{
namespace Type
{
	enum : int
	{
		DamageObject            = 1,
		MagicRelease            = 2,
		UseItemSkill            = 3,
		MeteorStrike            = 4,
		DoMeteorStrikeDamage    = 5,
		CalcMeteorStrikeEffect  = 6,
		AncientTablet           = 7,
	};
}
} // namespace hb::server::delay_event

class CDelayEvent
{
public:
	inline CDelayEvent()
	{
	}

	inline virtual ~CDelayEvent()
	{
	}

	int m_delay_type;
	int m_effect_type;

	char m_map_index;
	int m_dx, m_dy;

	int  m_target_handle;
	char m_target_type;
	int m_v1, m_v2, m_v3;

	uint32_t m_trigger_time;
};
