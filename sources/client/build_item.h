// build_item.h: interface for the build_item class.
//
//////////////////////////////////////////////////////////////////////

#pragma once
#include <string>

#include "common_types.h"
#include "net_constants.h"

class build_item
{
public:
	inline build_item()
	{
		int i;

		m_build_enabled = false;
		m_skill_limit   = 0;
		m_max_skill     = 0;

		m_sprite_handle     = 0;
		m_sprite_frame = 0;

		for (i = 0; i < 7; i++) {
			m_element_count[i] = 0;
			m_element_flag[i]  = 0;
		}
	}

	inline virtual ~build_item()
	{

	}

	bool m_build_enabled;
	std::string m_name;
	int	 m_skill_limit;
	int  m_max_skill;
	int  m_sprite_handle, m_sprite_frame;
	std::string m_element_name_1;
	std::string m_element_name_2;
	std::string m_element_name_3;
	std::string m_element_name_4;
	std::string m_element_name_5;
	std::string m_element_name_6;
	uint32_t m_element_count[7];
	bool  m_element_flag[7];

	const std::string& get_element_name(int index) const
	{
		switch (index)
		{
		case 1: return m_element_name_1;
		case 2: return m_element_name_2;
		case 3: return m_element_name_3;
		case 4: return m_element_name_4;
		case 5: return m_element_name_5;
		default: return m_element_name_6;
		}
	}

};
