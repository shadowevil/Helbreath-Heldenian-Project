// skill.h: interface for the CSkill class.

#pragma once


#include "common_types.h"



namespace hb::server::skill
{
namespace EffectType
{
	enum : int
	{
		get     = 1,
		Pretend = 2,
		Taming  = 3,
	};
}
} // namespace hb::server::skill


class CSkill
{
public:
	inline CSkill()
	{
		std::memset(m_name, 0, sizeof(m_name));
		m_is_useable = false;
		m_use_method = 0;
	}

	inline virtual ~CSkill()
	{
	}

	char m_name[42];

	short m_type;
	short m_value_1, m_value_2, m_value_3, m_value_4, m_value_5, m_value_6;

	// Client display fields (sent via PacketSkillConfig)
	bool  m_is_useable;    // Whether skill can be actively used
	char  m_use_method;    // Use method (0=passive, 1=click, 2=target)
};
