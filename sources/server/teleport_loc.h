// teleport_loc.h: interface for the CTeleportLoc class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "common_types.h"
#include "direction_helpers.h"
using hb::shared::direction::direction;

class CTeleportLoc  
{
public:
	inline CTeleportLoc()
	{

		std::memset(m_dest_map_name, 0, sizeof(m_dest_map_name));
		//std::memset(m_dest_map_name2, 0, sizeof(m_dest_map_name2));
		m_src_x   = -1;
		m_src_y	  = -1;
		m_dest_x  = -1;
		m_dest_y  = -1;
		m_dest_x2 = -1;
		m_dest_y2 = -1;

		m_v1     = 0;
		m_v2     = 0;
		m_time  = 0;
		m_time2 = 0;

	}

	inline virtual ~CTeleportLoc()
	{

	}
												  
	short m_src_x, m_src_y;

	char  m_dest_map_name[11];
	direction m_dir;
	char  m_dest_map_name2[11];
	short m_dest_x,  m_dest_y;
	short m_dest_x2, m_dest_y2;

	int   m_v1, m_v2;
	uint32_t m_time, m_time2;

};
