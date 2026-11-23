// portion.h: interface for the CPortion class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "common_types.h"
#include "net_constants.h"

class CPortion
{
public:
	inline CPortion()
	{
		

		std::memset(m_name, 0, sizeof(m_name));
		m_skill_limit = 0;
		m_difficulty = 0;

		for(int i = 0; i < 12; i++)
			m_array[i] = -1;
	}

	inline virtual ~CPortion()
	{

	}

	char  m_name[hb::shared::limits::ItemNameLen];
	short m_array[12];

	int   m_skill_limit, m_difficulty;

};
