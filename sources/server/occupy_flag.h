// occupy_flag.h: interface for the COccupyFlag class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "common_types.h"

class COccupyFlag
{
public:
	inline COccupyFlag(int dX, int dY, char side, int ek_num, int doi)
	{
		m_x = dX;
		m_y = dY;

		m_side = side;
		m_enemy_kill_count = ek_num;

		m_dynamic_object_index = doi;
	}

	inline virtual ~COccupyFlag()
	{
	}

	char m_side;
	int  m_enemy_kill_count;
	int  m_x, m_y;

	int  m_dynamic_object_index;
};
