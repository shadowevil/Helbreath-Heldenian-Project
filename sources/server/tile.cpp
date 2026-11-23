// tile.cpp: implementation of the CTile class.
//
//////////////////////////////////////////////////////////////////////

#include "common_types.h"
#include "tile.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CTile::CTile()
{ 
 

	m_is_move_allowed = true;
	m_is_teleport    = false;
	m_is_water       = false;
	m_is_farm        = false;
												   
	m_owner      = 0;
	m_owner_class = 0;
	
	m_dead_owner      = 0;
	m_dead_owner_class = 0;
	
	for(int i = 0; i < hb::server::map::TilePerItems; i++) 
		m_item[i] = 0;
	m_total_item = 0;

	m_dynamic_object_id   = 0;
	m_dynamic_object_type = 0;

	m_is_temp_move_allowed = true;

	m_occupy_status    = 0;
	m_occupy_flag_index = 0;
	
	m_attribute  = 0;
}

CTile::~CTile()
{
 
	for(int i = 0; i < hb::server::map::TilePerItems; i++) 
	if (m_item[i] != 0) delete m_item[i];
}
