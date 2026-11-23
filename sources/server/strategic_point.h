// strategic_point.h: interface for the CStrategicPoint class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "common_types.h"


class CStrategicPoint
{
public:
	inline CStrategicPoint()
	{
		m_x	= 0;
		m_y	= 0;
		m_side = 0;
	}

	inline virtual ~CStrategicPoint()
	{
	}

	int		m_side;			// ������ �������� �Ҽ�: 0�̸� �߸�
	int     m_value;			// �߿䵵
	int		m_x, m_y;			// ��ġ
};
