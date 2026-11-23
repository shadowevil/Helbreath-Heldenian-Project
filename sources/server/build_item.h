// build_item.h: interface for the CBuildItem class.

#pragma once


#include "common_types.h"
#include "net_constants.h"

class CBuildItem  
{
public:
	inline CBuildItem()
	{
		

		std::memset(m_name, 0, sizeof(m_name));
		m_item_id = -1;

		m_skill_limit = 0;

		for(int i = 0; i < 6; i++) {
			m_material_item_id[i]    = 0;
			m_material_item_count[i] = 0;
			m_material_item_value[i] = 0;
			m_index[i]            = -1;
		}

		m_max_value     = 0;
		m_average_value = 0;
		m_max_skill     = 0;
		m_attribute    = 0;
	}

	inline virtual ~CBuildItem()
	{

	}

	char  m_name[hb::shared::limits::ItemNameLen];
	short m_item_id;

	int  m_skill_limit;
	
	int  m_material_item_id[6];
	int  m_material_item_count[6];
	int  m_material_item_value[6];
	int  m_index[6];

	int	 m_max_value;
	int  m_average_value;	
	int   m_max_skill;
	uint16_t  m_attribute;
};

