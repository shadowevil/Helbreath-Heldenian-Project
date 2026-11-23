// tile.h: interface for the CTile class.

#pragma once

#include "common_types.h"
#include "item.h"

namespace hb::server::map { constexpr int TilePerItems = 12; }

class CTile  
{												  
public:
	CTile();
	virtual ~CTile();

	char  m_owner_class;		// DEF_OT_PLAYER / DEF_OT_NPC
	short m_owner;

	char  m_dead_owner_class;	// DEF_OT_PLAYER / DEF_OT_NPC
	short m_dead_owner;

	CItem * m_item[hb::server::map::TilePerItems];
	char  m_total_item;

	uint16_t  m_dynamic_object_id;
	short m_dynamic_object_type;
	uint32_t m_dynamic_object_register_time;

	bool  m_is_move_allowed, m_is_teleport, m_is_water, m_is_farm, m_is_temp_move_allowed;

	int   m_occupy_status;    // Aresden -, Elvine + .
	int   m_occupy_flag_index;

	// Crusade
	int	  m_attribute;		  // :  ( )  (  )  ()
	
	
};

