// Map.cpp: implementation of the CMap class.

#include "CommonTypes.h"
#include "Map.h"
#include "StringCompat.h"
using namespace hb::server::map;
using namespace hb::server::config;
using namespace hb::shared::direction;

// Construction/Destruction

CMap::CMap(class CGame* game)
	: m_is_snow_enabled(false)
{
	for(int i = 0; i < MaxTeleportLoc; i++)
		m_teleport_loc[i] = 0;

	for(int i = 0; i < MaxWaypointCfg; i++) {
		m_waypoint_list[i].x = -1;
		m_waypoint_list[i].y = -1;
	}

	for(int i = 0; i < MaxMgar; i++) {
		m_mob_generator_avoid_rect[i].y = -1;
		m_mob_generator_avoid_rect[i].x = -1;
	}

	for(int i = 0; i < MaxNmr; i++) {
		m_no_attack_rect[i].y = -1;
		m_no_attack_rect[i].x = -1;
	}

	for(int i = 0; i < MaxSpotMobGenerator; i++) {
		m_spot_mob_generator[i].is_defined = false;
		m_spot_mob_generator[i].total_active_mob = 0;
	}

	for(int i = 0; i < MaxFishPoint; i++) {
		m_fish_point_list[i].x = -1;
		m_fish_point_list[i].y = -1;
	}

	for(int i = 0; i < MaxMineralPoint; i++) {
		m_mineral_point_list[i].x = -1;
		m_mineral_point_list[i].y = -1;
	}

	for(int i = 0; i < MaxInitialPoint; i++) {
		m_initial_point[i].x = -1;
		m_initial_point[i].y = -1;
	}

	for(int i = 0; i < 1000; i++)
		m_naming_value_using_status[i] = false;

	for(int i = 0; i < MaxOccupyFlag; i++)
		m_occupy_flag[i] = 0;

	for(int i = 0; i < MaxStrategicPoints; i++)
		m_strategic_point_list[i] = 0;

	for(int i = 0; i < MaxEnergySpheres; i++) {
		m_energy_sphere_creation_list[i].type = 0;
		m_energy_sphere_goal_list[i].result = 0;
	}

	m_is_heldenian_map = false;
	m_total_active_object = 0;
	m_total_alive_object = 0;
	m_maximum_object = 1000;  // Default max objects per map (can be overridden by config)
	m_total_item_events = 0;
	m_mob_event_amount = 15;
	//m_sInitialPointX = 0;
	//m_sInitialPointY = 0;

	m_is_fixed_day_mode = false;

	m_total_fish_point = 0;
	m_max_fish = 0;
	m_cur_fish = 0;

	m_total_mineral_point = 0;
	m_max_mineral = 0;
	m_cur_mineral = 0;

	m_tile = 0;

	m_weather_status = 0;
	m_type = MapType::Normal;

	m_game = game;

	m_level_requirement = 0;
	m_upper_level_limit = 0; // v1.4
	m_mineral_generator = false;

	m_total_occupy_flags = 0;

	m_is_attack_enabled = true;
	m_random_mob_generator_level = 0;

	m_is_fight_zone = false;

	m_total_energy_sphere_creation_point = 0;
	m_total_energy_sphere_goal_point = 0;

	m_is_energy_sphere_goal_enabled = false;
	m_cur_energy_sphere_goal_point_index = -1;

	for(int ix = 0; ix < MaxSectors; ix++)
		for(int iy = 0; iy < MaxSectors; iy++) {
			m_sector_info[ix][iy].neutral_activity = 0;
			m_sector_info[ix][iy].aresden_activity = 0;
			m_sector_info[ix][iy].elvine_activity = 0;
			m_sector_info[ix][iy].monster_activity = 0;
			m_sector_info[ix][iy].player_activity = 0;

			m_temp_sector_info[ix][iy].neutral_activity = 0;
			m_temp_sector_info[ix][iy].aresden_activity = 0;
			m_temp_sector_info[ix][iy].elvine_activity = 0;
			m_temp_sector_info[ix][iy].monster_activity = 0;
			m_temp_sector_info[ix][iy].player_activity = 0;
		}

	m_top_neutral_sector_x = m_top_neutral_sector_y = m_top_aresden_sector_x = m_top_aresden_sector_y = m_top_elvine_sector_x = m_top_elvine_sector_y = m_top_monster_sector_x = m_top_monster_sector_y = m_top_player_sector_x = m_top_player_sector_y = 0;

	for(int i = 0; i < MaxHeldenianDoor; i++) {
		m_heldenian_gate_door[i].dir = direction{};
		m_heldenian_gate_door[i].x = 0;
		m_heldenian_gate_door[i].y = 0;
	}

	for(int i = 0; i < MaxHeldenianTower; i++) {
		m_heldenian_tower[i].type_id = 0;
		m_heldenian_tower[i].x = 0;
		m_heldenian_tower[i].y = 0;
		m_heldenian_tower[i].side = 0;
	}

	for(int i = 0; i < MaxStrikePoints; i++) {
		m_strike_point[i].x = 0;
		m_strike_point[i].y = 0;
		m_strike_point[i].hp = 0;
		m_strike_point[i].map_index = -1;
		std::memset(m_strike_point[i].related_map_name, 0, sizeof(m_strike_point[i].related_map_name));
	}
	m_total_strike_points = 0;
	m_is_disabled = false;

	for(int i = 0; i < hb::shared::limits::MaxCrusadeStructures; i++) {
		m_crusade_structure_info[i].type = 0;
		m_crusade_structure_info[i].side = 0;
		m_crusade_structure_info[i].x = 0;
		m_crusade_structure_info[i].y = 0;
	}
	m_total_crusade_structures = 0;
	m_total_agriculture = 0;

	// initialize boolean members that may not be set in map config files
	m_random_mob_generator = false;
	m_is_citizen_limit = false;
	m_is_energy_sphere_auto_creation = false;
	m_is_recall_impossible = false;
	m_is_apocalypse_map = false;

	for(int i = 0; i < MaxDynamicGates; i++) {
		m_dynamic_gate_coords[i].is_gate_map = false;
		m_dynamic_gate_coords[i].dynamic_gate_x = 0;
		m_dynamic_gate_coords[i].dynamic_gate_y = 0;
		std::memset(m_dynamic_gate_coords[i].dynamic_gate_map, 0, sizeof(m_dynamic_gate_coords[i].dynamic_gate_map));
	}
}

CMap::~CMap()
{

	

	if (m_tile != 0)
		delete[]m_tile;

	for(int i = 0; i < MaxTeleportLoc; i++)
		if (m_teleport_loc[i] != 0) delete m_teleport_loc[i];

	for(int i = 0; i < MaxOccupyFlag; i++)
		if (m_occupy_flag[i] != 0) delete m_occupy_flag[i];

	for(int i = 0; i < MaxStrategicPoints; i++)
		if (m_strategic_point_list[i] != 0) delete m_strategic_point_list[i];
}

void CMap::set_owner(short owner, char owner_class, short sX, short sY)
{
	class CTile* tile;

	if ((sX < 0) || (sX >= m_size_x) || (sY < 0) || (sY >= m_size_y)) return;

	tile = (class CTile*)(m_tile + sX + sY * m_size_x);
	tile->m_owner = owner;
	tile->m_owner_class = owner_class;
}

char _tmp_cMoveDirX[9] = { 0,0,1,1,1,0,-1,-1,-1 };
char _tmp_cMoveDirY[9] = { 0,-1,-1,0,1,1,1,0,-1 };
bool CMap::check_fly_space_available(short sX, char sY, direction dir, short owner)
{
	class CTile* tile;
	short dX, dY;

	if ((dir <= 0) || (dir > 8)) return 0;
	dX = _tmp_cMoveDirX[dir] + sX;
	dY = _tmp_cMoveDirY[dir] + sY;
	if ((dX < 20) || (dX >= m_size_x - 20) || (dY < 20) || (dY >= m_size_y - 20)) return 0;
	tile = (class CTile*)(m_tile + sX + sY * m_size_x);
	if (tile->m_owner != 0) return 0;
	tile->m_owner = owner;
	return 1;
}

void CMap::set_dead_owner(short owner, char owner_class, short sX, short sY)
{
	class CTile* tile;

	if ((sX < 0) || (sX >= m_size_x) || (sY < 0) || (sY >= m_size_y)) return;

	tile = (class CTile*)(m_tile + sX + sY * m_size_x);
	tile->m_dead_owner = owner;
	tile->m_dead_owner_class = owner_class;
}

/*********************************************************************************************************************
**  void CMap::get_owner(short * owner, char * owner_class, short sX, short sY)										**
**  description			:: check if the tile contains a player														**
**  last updated		:: November 17, 2004; 10:48 PM; Hypnotoad													**
**	return value		:: void																						**
**  commentary			::	-	added check to see if owner is class 1 or if is greater than max clients 			**
**********************************************************************************************************************/
void CMap::get_owner(short* owner, char* owner_class, short sX, short sY)
{
	class CTile* tile;

	if ((sX < 0) || (sX >= m_size_x) || (sY < 0) || (sY >= m_size_y)) {
		*owner = 0;
		*owner_class = 0;
		return;
	}

	tile = (class CTile*)(m_tile + sX + sY * m_size_x);
	*owner = tile->m_owner;
	*owner_class = tile->m_owner_class;

	if ((*owner_class == 1) && (*owner > MaxClients)) {
		*owner = 0;
		*owner_class = 0;
		return;
	}

	if (tile->m_owner == 0) *owner_class = 0;
}

/*********************************************************************************************************************
**  void CMap::get_dead_owner(short * owner, char * owner_class, short sX, short sY)									**
**  description			:: check if the tile contains a dead player													**
**  last updated		:: November 20, 2004; 9:13 PM; Hypnotoad													**
**	return value		:: void																						**
**********************************************************************************************************************/
void CMap::get_dead_owner(short* owner, char* owner_class, short sX, short sY)
{
	class CTile* tile;

	if ((sX < 0) || (sX >= m_size_x) || (sY < 0) || (sY >= m_size_y)) {
		*owner = 0;
		*owner_class = 0;
		return;
	}

	tile = (class CTile*)(m_tile + sX + sY * m_size_x);
	*owner = tile->m_dead_owner;
	*owner_class = tile->m_dead_owner_class;
}

bool CMap::get_moveable(short dX, short dY, short* d_otype, short* top_item)
{
	class CTile* tile;

	if ((dX < 20) || (dX >= m_size_x - 20) || (dY < 20) || (dY >= m_size_y - 20)) return false;
	tile = (class CTile*)(m_tile + dX + dY * m_size_x);

	if (d_otype != 0) *d_otype = tile->m_dynamic_object_type;
	if (top_item != 0) *top_item = tile->m_total_item;

	if (tile->m_owner != 0) return false;
	if (tile->m_is_move_allowed == false) return false;
	if (tile->m_is_temp_move_allowed == false) return false;

	return true;
}

bool CMap::get_is_move_allowed_tile(short dX, short dY)
{
	class CTile* tile;

	if ((dX < 20) || (dX >= m_size_x - 20) || (dY < 20) || (dY >= m_size_y - 20)) return false;

	tile = (class CTile*)(m_tile + dX + dY * m_size_x);

	if (tile->m_is_move_allowed == false) return false;
	if (tile->m_is_temp_move_allowed == false) return false;

	return true;
}

/*bool CMap::sub_4C0F20(short dX, short dY)
{
 class CTile * tile;

	3CA18h = 0;

	if ((dX < 14) || (dX >= m_size_x - 16) || (dY < 12) || (dY >= m_size_y - 14)) return false;

	tile = (class CTile *)(m_tile + dX + dY*m_size_y);

}*/

bool CMap::get_is_teleport(short dX, short dY)
{
	class CTile* tile;

	if ((dX < 14) || (dX >= m_size_x - 16) || (dY < 12) || (dY >= m_size_y - 14)) return false;

	tile = (class CTile*)(m_tile + dX + dY * m_size_x);

	if (tile->m_is_teleport == false) return false;

	return true;
}

void CMap::clear_owner(int debug_code, short owner_h, char owner_type, short sX, short sY)
{
	class CTile* tile;

	if ((sX < 0) || (sX >= m_size_x) || (sY < 0) || (sY >= m_size_y)) return;

	tile = (class CTile*)(m_tile + sX + sY * m_size_x);

	if ((tile->m_owner == owner_h) && (tile->m_owner_class == owner_type)) {
		tile->m_owner = 0;
		tile->m_owner_class = 0;
	}

	if ((tile->m_dead_owner == owner_h) && (tile->m_dead_owner_class == owner_type)) {
		tile->m_dead_owner = 0;
		tile->m_dead_owner_class = 0;
	}
}

void CMap::clear_dead_owner(short sX, short sY)
{
	class CTile* tile;

	if ((sX < 0) || (sX >= m_size_x) || (sY < 0) || (sY >= m_size_y)) return;

	tile = (class CTile*)(m_tile + sX + sY * m_size_x);
	tile->m_dead_owner = 0;
	tile->m_dead_owner_class = 0;
}

bool CMap::set_item(short sX, short sY, CItem* item)
{
	class CTile* tile;
	

	if ((sX < 0) || (sX >= m_size_x) || (sY < 0) || (sY >= m_size_y)) return 0;

	tile = (class CTile*)(m_tile + sX + sY * m_size_x);

	if (tile->m_item[TilePerItems - 1] != 0)
		delete tile->m_item[TilePerItems - 1];
	else tile->m_total_item++;

	for(int i = TilePerItems - 2; i >= 0; i--)
		tile->m_item[i + 1] = tile->m_item[i];

	tile->m_item[0] = item;
	//tile->m_total_item++;
	return true;
}

CItem* CMap::get_item(short sX, short sY, CItem** remain)
{
	if ((sX < 0) || (sX >= m_size_x) || (sY < 0) || (sY >= m_size_y)) return nullptr;

	CTile* tile = (m_tile + sX + sY * m_size_x);
	if (tile->m_total_item == 0) return nullptr;

	CItem* item = tile->m_item[0];

	for (int i = 0; i <= TilePerItems - 2; i++)
		tile->m_item[i] = tile->m_item[i + 1];
	tile->m_total_item--;
	tile->m_item[tile->m_total_item] = nullptr;

	if (remain)
		*remain = tile->m_item[0]; // next item on tile, or nullptr if empty

	return item;
}

int CMap::check_item(short sX, short sY)
{
	class CTile* tile;
	CItem* item;

	if ((sX < 0) || (sX >= m_size_x) || (sY < 0) || (sY >= m_size_y)) return 0;

	tile = (class CTile*)(m_tile + sX + sY * m_size_x);
	item = tile->m_item[0];
	if (tile->m_total_item == 0) return 0;

	return item->m_id_num;
}

bool CMap::is_valid_loc(short sX, short sY)
{
	if ((sX < 0) || (sX >= m_size_x) || (sY < 0) || (sY >= m_size_y)) return false;
	return true;
}

bool CMap::init(char* name)
{
	
	std::memset(m_name, 0, sizeof(m_name));
	strcpy(m_name, name);

	std::memset(m_location_name, 0, sizeof(m_location_name));
	std::memset(m_display_name, 0, sizeof(m_display_name));

	if (decode_map_data_file_contents() == false)
		return false;

	for(int i = 0; i < MaxTeleportLoc; i++)
		m_teleport_loc[i] = 0;

	return true;
}

bool CMap::decode_map_data_file_contents()
{
	FILE* map_file;
	char  map_file_name[256], header[260], temp[100];
	size_t nRead;
	char* token, * context, read_mode;
	char seps[] = "= \t\r\n";
	class CTile* tile;

	std::memset(map_file_name, 0, sizeof(map_file_name));
	strcat(map_file_name, "mapdata/");
	strcat(map_file_name, m_name);
	strcat(map_file_name, ".amd");

	map_file = fopen(map_file_name, "rb");
	if (!map_file) return false;

	std::memset(header, 0, sizeof(header));
	nRead = fread(header, 1, 256, map_file);

	for(int i = 0; i < 256; i++)
		if (header[i] == 0) header[i] = ' ';

	read_mode = 0;

	token = strtok_s(header, seps, &context);
	while (token != 0) {

		if (read_mode != 0) {
			switch (read_mode) {
			case 1:
				m_size_x = atoi(token);
				read_mode = 0;
				break;
			case 2:
				m_size_y = atoi(token);
				read_mode = 0;
				break;
			case 3:
				m_tile_data_size = atoi(token);
				read_mode = 0;
				break;
			}
		}
		else {
			if (memcmp(token, "MAPSIZEX", 8) == 0) read_mode = 1;
			if (memcmp(token, "MAPSIZEY", 8) == 0) read_mode = 2;
			if (memcmp(token, "TILESIZE", 8) == 0) read_mode = 3;
		}

		token = strtok_s(NULL, seps, &context);
	}

	m_tile = (class CTile*)new class CTile[m_size_x * m_size_y];

	for(int iy = 0; iy < m_size_y; iy++)
		for(int ix = 0; ix < m_size_x; ix++) {
			nRead = fread(temp, 1, m_tile_data_size, map_file);
			tile = (class CTile*)(m_tile + ix + iy * m_size_x);
			if ((temp[8] & 0x80) != 0) {
				tile->m_is_move_allowed = false;
			}
			else tile->m_is_move_allowed = true;

			if ((temp[8] & 0x40) != 0) {
				tile->m_is_teleport = true;
			}
			else tile->m_is_teleport = false;

			if ((temp[8] & 0x20) != 0) {
				tile->m_is_farm = true;
			}
			else tile->m_is_farm = false;

			short tile_id;
			std::memcpy(&tile_id, &temp[0], sizeof(short));
			if (tile_id == 19) {
				tile->m_is_water = true;
			}
			else tile->m_is_water = false;

		}

	fclose(map_file);

	return true;
}

bool CMap::search_teleport_dest(int sX, int sY, char* map_name, int* dx, int* dy, direction* dir)
{
	// Collect all matching teleport entries for this source tile
	int matches[MaxTeleportLoc];
	int matchCount = 0;

	for (int i = 0; i < MaxTeleportLoc; i++) {
		if ((m_teleport_loc[i] != 0) && (m_teleport_loc[i]->m_src_x == sX) && (m_teleport_loc[i]->m_src_y == sY)) {
			matches[matchCount++] = i;
		}
	}

	if (matchCount == 0) return false;

	// Randomly select among matching entries
	int pick = matches[rand() % matchCount];
	memcpy(map_name, m_teleport_loc[pick]->m_dest_map_name, 10);
	*dx = m_teleport_loc[pick]->m_dest_x;
	*dy = m_teleport_loc[pick]->m_dest_y;
	*dir = m_teleport_loc[pick]->m_dir;
	return true;
}

void CMap::set_dynamic_object(uint16_t id, short type, short sX, short sY, uint32_t register_time)
{
	class CTile* tile;

	if ((sX < 0) || (sX >= m_size_x) || (sY < 0) || (sY >= m_size_y)) return;

	tile = (class CTile*)(m_tile + sX + sY * m_size_x);

	tile->m_dynamic_object_id = id;
	tile->m_dynamic_object_type = type;
	tile->m_dynamic_object_register_time = register_time;
}

bool CMap::get_dynamic_object(short sX, short sY, short* type, uint32_t* register_time, int* index)
{
	class CTile* tile;

	if ((sX < 0) || (sX >= m_size_x) || (sY < 0) || (sY >= m_size_y)) return false;

	tile = (class CTile*)(m_tile + sX + sY * m_size_x);

	*type = tile->m_dynamic_object_type;
	*register_time = tile->m_dynamic_object_register_time;
	if (index != 0) *index = tile->m_dynamic_object_id;

	return true;
}

int CMap::get_empty_naming_value()
{
	

	for(int i = 0; i < 1000; i++)
		if (m_naming_value_using_status[i] == false) {

			m_naming_value_using_status[i] = true;
			return i;
		}

	return -1;
}

void CMap::set_naming_value_empty(int value)
{
	m_naming_value_using_status[value] = false;
}

bool CMap::get_is_water(short dX, short dY)
{
	class CTile* tile;

	if ((dX < 14) || (dX >= m_size_x - 16) || (dY < 12) || (dY >= m_size_y - 14)) return false;

	tile = (class CTile*)(m_tile + dX + dY * m_size_x);

	if (tile->m_is_water == false) return false;

	return true;
}

bool CMap::remove_crops_total_sum()
{
	if (m_total_agriculture < MaxAgriculture)
	{
		m_total_agriculture--;
		if (m_total_agriculture < 0)
		{
			m_total_agriculture = 0;
		}
		return true;
	}
	return false;
}

bool CMap::add_crops_total_sum()
{
	if (m_total_agriculture < MaxAgriculture)
	{
		m_total_agriculture++;
		return true;
	}
	return false;
}

bool CMap::get_is_farm(short tX, short tY)
{
	class CTile* tile;

	if ((tX < 14) || (tX >= m_size_x - 16) || (tY < 12) || (tY >= m_size_y - 14)) return false;

	tile = (class CTile*)(m_tile + tX + tY * m_size_x);

	if (tile->m_is_farm == false) return false;

	return true;
}

int CMap::analyze(char type, int* pX, int* pY, int* v1, int* v2, int* v3)
{

	switch (type) {
	case 1:

		break;

	}

	return 0;
}

void CMap::set_temp_move_allowed_flag(int dX, int dY, bool flag)
{
	class CTile* tile;

	if ((dX < 20) || (dX >= m_size_x - 20) || (dY < 20) || (dY >= m_size_y - 20)) return;

	tile = (class CTile*)(m_tile + dX + dY * m_size_x);
	tile->m_is_temp_move_allowed = flag;
}

int CMap::register_occupy_flag(int dX, int dY, int side, int ek_num, int doi)
{
	

	if ((dX < 20) || (dX >= m_size_x - 20) || (dY < 20) || (dY >= m_size_y - 20)) return -1;

	for(int i = 1; i < MaxOccupyFlag; i++)
		if (m_occupy_flag[i] == 0) {
			m_occupy_flag[i] = new class COccupyFlag(dX, dY, side, ek_num, doi);
			if (m_occupy_flag == 0) return -1;
			else return i;
		}

	return -1;
}

void CMap::clear_sector_info()
{
	for(int ix = 0; ix < MaxSectors; ix++)
		for(int iy = 0; iy < MaxSectors; iy++) {
			m_sector_info[ix][iy].neutral_activity = 0;
			m_sector_info[ix][iy].aresden_activity = 0;
			m_sector_info[ix][iy].elvine_activity = 0;
			m_sector_info[ix][iy].monster_activity = 0;
			m_sector_info[ix][iy].player_activity = 0;
		}
}

void CMap::clear_temp_sector_info()
{
	for(int ix = 0; ix < MaxSectors; ix++)
		for(int iy = 0; iy < MaxSectors; iy++) {
			m_temp_sector_info[ix][iy].neutral_activity = 0;
			m_temp_sector_info[ix][iy].aresden_activity = 0;
			m_temp_sector_info[ix][iy].elvine_activity = 0;
			m_temp_sector_info[ix][iy].monster_activity = 0;
			m_temp_sector_info[ix][iy].player_activity = 0;
		}
}

void CMap::setup_no_attack_area()
{
	class CTile* tile;

	for(int i = 0; i < MaxNmr; i++) {
		if ((m_no_attack_rect[i].y > 0)) {
			for(int ix = m_no_attack_rect[i].Left(); ix <= m_no_attack_rect[i].Right(); ix++)
				for(int iy = m_no_attack_rect[i].Top(); iy <= m_no_attack_rect[i].Bottom(); iy++) {
					tile = (class CTile*)(m_tile + ix + iy * m_size_x);
					tile->m_attribute = tile->m_attribute | 0x00000004;
				}
		}
		else if (m_no_attack_rect[i].y == -10) {
			for(int ix = 0; ix < m_size_x; ix++)
				for(int iy = 0; iy < m_size_y; iy++) {
					tile = (class CTile*)(m_tile + ix + iy * m_size_x);
					tile->m_attribute = tile->m_attribute | 0x00000004;
				}
		}
	}
}

/*********************************************************************************************************************
**  int CMap::get_attribute(int dX, int dY, int bit_mask)															**
**  description			:: check if the tile contains a dead player													**
**  last updated		:: November 20, 2004; 9:55 PM; Hypnotoad													**
**	return value		:: int																						**
**********************************************************************************************************************/
int CMap::get_attribute(int dX, int dY, int bit_mask)
{
	class CTile* tile;

	if ((dX < 20) || (dX >= m_size_x - 20) || (dY < 20) || (dY >= m_size_y - 20)) return -1;

	tile = (class CTile*)(m_tile + dX + dY * m_size_x);
	return (tile->m_attribute & bit_mask);
}

bool CMap::add_crusade_structure_info(char type, short sX, short sY, char side)
{
	

	for(int i = 0; i < hb::shared::limits::MaxCrusadeStructures; i++)
		if (m_crusade_structure_info[i].type == 0) {
			m_crusade_structure_info[i].type = type;
			m_crusade_structure_info[i].side = side;
			m_crusade_structure_info[i].x = sX;
			m_crusade_structure_info[i].y = sY;

			m_total_crusade_structures++;
			return true;
		}

	return false;
}

/*bool CMap::bAddHeldenianTowerInfo(char type, short sX, short sY, char side)
{
 

	for(int i = 0; i < MaxHeldenianTower; i++)
	if (m_heldenian_tower[i].type == 0) {
	if (m_heldenian_tower[i].side == 1) {
		m_heldenian_tower[i].type_id = type_id;
		m_heldenian_tower[i].side = side;
		m_heldenian_tower[i].x = sX;
		m_heldenian_tower[i].y = sY;
		m_heldenian_aresden_left_tower++;
		return true;
	}
	else if (m_heldenian_tower[i].side == 2) {
		m_heldenian_tower[i].type_id = type_id;
		m_heldenian_tower[i].side = side;
		m_heldenian_tower[i].x = sX;
		m_heldenian_tower[i].y = sY;
		m_heldenian_elvine_left_tower++;
		return true;
	}

	return false;
}*/

bool CMap::remove_crusade_structure_info(short sX, short sY)
{
	

	bool found = false;
	for(int i = 0; i < hb::shared::limits::MaxCrusadeStructures; i++)
		if ((m_crusade_structure_info[i].x == sX) && (m_crusade_structure_info[i].y == sY)) {
			m_crusade_structure_info[i].type = 0;
			m_crusade_structure_info[i].side = 0;
			m_crusade_structure_info[i].x = 0;
			m_crusade_structure_info[i].y = 0;
			found = true;
			break;
		}

	if (!found) return false;

	for(int i = 0; i < hb::shared::limits::MaxCrusadeStructures - 1; i++)
		if ((m_crusade_structure_info[i].type == 0) && (m_crusade_structure_info[i + 1].type != 0)) {
			m_crusade_structure_info[i].type = m_crusade_structure_info[i + 1].type;
			m_crusade_structure_info[i].side = m_crusade_structure_info[i + 1].side;
			m_crusade_structure_info[i].x = m_crusade_structure_info[i + 1].x;
			m_crusade_structure_info[i].y = m_crusade_structure_info[i + 1].y;

			m_crusade_structure_info[i + 1].type = 0;
			m_crusade_structure_info[i + 1].side = 0;
			m_crusade_structure_info[i + 1].x = 0;
			m_crusade_structure_info[i + 1].y = 0;
		}

	m_total_crusade_structures--;
	return true;
}

void CMap::restore_strike_points()
{
	

	for(int i = 0; i < MaxStrikePoints; i++) {
		m_strike_point[i].init_hp = m_strike_point[i].hp;
	}
}
