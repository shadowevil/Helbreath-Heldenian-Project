// dynamic_object.h: interface for the CDynamicObject class.
//
//////////////////////////////////////////////////////////////////////

#pragma once


#include "common_types.h"

class CDynamicObject
{
public:
	inline CDynamicObject(short owner, char owner_type, short type, char map_index, short sX, short sY, uint32_t register_time, uint32_t last_time, int v1)
	{
		m_owner         = owner;
		m_owner_type     = owner_type;

		m_type          = type;

		m_map_index      = map_index;
		m_x             = sX;
		m_y             = sY;

		m_register_time = register_time;
		m_last_time     = last_time;

		m_count         = 0;
		m_v1            = v1;
	}

	inline virtual ~CDynamicObject()
	{
	}

	short m_owner;
	char  m_owner_type;

	short m_type;
	char  m_map_index;
	short m_x, m_y; 
	uint32_t m_register_time;
	uint32_t m_last_time;

	int   m_count;			// Ư�� ������Ʈ�� ��� ����ϴ� ī���� ���� 
	int   m_v1;			// �߰� ������ ������ ����Ѵ�.
};

