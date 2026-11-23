// mineral.h: interface for the CMineral class.
//
//////////////////////////////////////////////////////////////////////

#pragma once
#include "common_types.h"

class CMineral
{
public:
	inline CMineral(char type, char map_index, int sX, int sY, int remain)
	{
		m_type = type;
		m_map_index = map_index;
		m_x = sX;
		m_y = sY;;
		m_remain = remain;
		m_difficulty = 0;
	}

	inline virtual ~CMineral()
	{

	}

	char  m_type;

	char  m_map_index;
	int   m_x, m_y;
	int   m_difficulty;
	short m_dynamic_object_handle;

	int   m_remain;
};
