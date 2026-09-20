// Npc.cpp: implementation of the CNpc class.
//
//////////////////////////////////////////////////////////////////////

#include "CommonTypes.h"
#include "Npc.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CNpc::CNpc(const char * name5)
{
 
	std::memset(m_name, 0, sizeof(m_name));
	memcpy(m_name, name5, 5);
	
	for(int i = 0; i < hb::server::npc::MaxWaypoints; i++)			  
		m_waypoint_index[i] = -1;
	
	for(int i = 0; i < hb::server::config::MaxMagicEffects; i++) 
		m_magic_effect_status[i]	= 0;

	m_is_summoned       = false;
	m_bypass_mob_limit   = false;
	m_is_perm_attack_mode = false;

	m_regen_time = 0;
	m_is_killed   = false;

	m_original_type      = 0;
	m_npc_config_id       = -1;
	m_summon_control_mode = 0;
	
	m_attribute = 0;
	m_abs_damage = 0;
	m_status.clear();
	m_appearance.clear();

	m_attack_range    = 1;
	m_special_ability = 0;
	
	m_exp = 0;

	m_build_count = 0;
	m_mana_stock  = 0;
	m_is_unsummoned = false;
	m_crop_type = 0;
	m_crop_skill = 0;

	m_is_master  = false;
	m_v1 = 0;

	m_npc_item_type = 0;
	m_npc_item_max = 0;
	m_drop_table_id = 0;

	std::memset(m_npc_name, 0, sizeof(m_npc_name));

	// OPTIMIZATION FIX #3: initialize previous position for delta detection
	m_prev_x = -1;
	m_prev_y = -1;

}

CNpc::~CNpc()
{

}


/*// .text:004BAFD0
// xfers to: m_vNpcItem.at, bGetItemNameWhenDeleteNpc, sub_4BC360
void CNpc::m_vNpcItem.size()
{
 int ret;
 class CNpcItem * temp_npc_item;
	
	if (temp_npc_item->m_name == 0) {
		ret = 0;
	}
	else {
		ret = temp_npc_item->8 - (temp_npc_item->m_name >> 5);
	}
	return ret;

}

// .text:.text:004BB010
// xfers to: bGetItemNameWhenDeleteNpc
// xfers from: sub_4BB3D0, sub_4BB340, m_vNpcItem.size
void CNpc::m_vNpcItem.at(int result)
{ 
	if (result < m_vNpcItem.size()) {
		sub_4BB3D0();
	}
	sub_4BB340() += (result << 5);

}*/
