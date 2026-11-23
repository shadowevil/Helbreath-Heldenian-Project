// fish.h: interface for the CFish class.

#pragma once


#include "common_types.h"
#include "item.h"

class CFish  
{
public:
	inline CFish(char map_index, short sX, short sY, short type, CItem * item, int difficulty)
	{
		m_map_index		= map_index;
		m_x			= sX;
		m_y			= sY;
		m_type			= type;
		m_item			= item;

		m_engaging_count = 0;
		m_difficulty    = difficulty;

		if (m_difficulty <= 0)
			m_difficulty = 1;
	}

	inline virtual ~CFish()
	{
		if (m_item != 0) delete m_item;
	}

	char  m_map_index;
	short m_x, m_y;

	short m_type;
	CItem * m_item;

	short m_dynamic_object_handle;

	short m_engaging_count;
	int   m_difficulty;
};

