// magic.h: interface for the CMagic class.
//
//////////////////////////////////////////////////////////////////////

#pragma once
#include <string>

#include <cstring>
#include "magic_types.h"

class CMagic
{
public:
	inline CMagic()
	{
	}

	inline virtual ~CMagic()
	{
	}

	std::string m_name;
	int  m_value_1 = 0, m_value_2 = 0, m_value_3 = 0;
	// CLEROTH
	int m_value_4 = 0;
	bool m_is_visible = false;
	int m_type = 0;          // DEF_MAGICTYPE_*
	int m_aoe_radius_x = 0;    // AoE X radius in tiles (server m_value_2)
	int m_aoe_radius_y = 0;    // AoE Y radius in tiles (server m_value_3)
	int m_dynamic_pattern = 0; // Wall=1, Field=2 (server m_sValue11)
	int m_dynamic_radius = 0;  // Wall length / field radius (server m_sValue12)
};
