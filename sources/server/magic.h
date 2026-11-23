// magic.h: interface for the CMagic class.
//
//////////////////////////////////////////////////////////////////////

#pragma once


#include "common_types.h"
#include "magic_types.h"

namespace hb::server::config { constexpr int MaxMagicEffects = 100; }

class CMagic
{
public:
	inline CMagic()
	{
		std::memset(m_name, 0, sizeof(m_name));
		m_attribute = 0;
	}

	inline virtual ~CMagic()
	{
	}

	char m_name[31];

	short m_type;
	uint32_t m_delay_time, m_last_time;
	short m_value_1, m_value_2, m_value_3, m_value_4, m_value_5, m_value_6;
	short m_value_7, m_value_8, m_value_9, m_value_10, m_value_11, m_value_12;
	short m_intelligence_limit;
	int   m_gold_cost;
	
	char  m_category; // ��a� �l�u: RDa� ��a� 0, �r�ݸ�a� 1, acl� ��a� 2 
	int   m_attribute; // ��a� L�Ls:  �A 1 �D�A 2 sN 3 a� 4
};

