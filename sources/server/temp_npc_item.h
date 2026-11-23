// teleport_loc.h: interface for the CTeleportLoc class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "common_types.h"
#include "net_constants.h"

class CNpcItem
{
public:
	inline CNpcItem()
	{
		std::memset(m_name, 0, sizeof(m_name));

		m_item_id = 0;
		m_first_probability = 0;
		m_first_target_value = 0;
		m_second_probability = 0;
		m_second_target_value = 0;
	}

	inline virtual ~CNpcItem()
	{
	}

	char m_name[hb::shared::limits::ItemNameLen];
	short m_item_id;
	short m_first_probability;
	short m_second_probability;
	char m_first_target_value;
	char m_second_target_value;

};
