// Map.h: interface for the CMap class.

#pragma once


#include "CommonTypes.h"
#include "NetConstants.h"
#include "OccupyFlag.h"
#include "Tile.h"
#include "StrategicPoint.h"
#include "GameGeometry.h"


#include "OwnerClass.h"

namespace hb::server::map
{
constexpr int MaxTeleportLoc        = 200;
constexpr int MaxWaypointCfg        = 200;
constexpr int MaxMgar               = 50;
constexpr int MaxNmr                = 50;
constexpr int MaxSpotMobGenerator   = 100;
constexpr int MaxFishPoint          = 200;
constexpr int MaxItemEvents         = 200;
constexpr int MaxMineralPoint       = 200;
constexpr int MaxHeldenianDoor      = 200;
constexpr int MaxOccupyFlag         = 20001;
constexpr int MaxInitialPoint       = 20;
constexpr int MaxAgriculture        = 200;
constexpr int MaxDynamicGates       = 10;
constexpr int MaxHeldenianTower     = 200;
} // namespace hb::server::map


#include "CommonTypes.h"
#include "Game.h"
#include "TeleportLoc.h"
#include "GlobalDef.h"

namespace hb::server::map
{
namespace MapType
{
	enum : int
	{
		Normal              = 0,
		NoPenaltyNoReward   = 1,
	};
}
constexpr int MaxEnergySpheres      = 10;
constexpr int MaxStrategicPoints    = 200;
constexpr int MaxSectors            = 60;
constexpr int MaxStrikePoints       = 20;
} // namespace hb::server::map




class CMap  
{
public:
	
	bool check_fly_space_available(short sX, char sY, direction dir, short owner);
	bool get_is_farm(short tX, short tY);
	void restore_strike_points();
	bool remove_crusade_structure_info(short sX, short sY);
	bool add_crusade_structure_info(char type, short sX, short sY, char side);
	int get_attribute(int dX, int dY, int bit_mask);
	void setup_no_attack_area();
	void clear_temp_sector_info();
	void clear_sector_info();
	int register_occupy_flag(int dX, int dY, int side, int ek_num, int doi);
	int  check_item(short sX, short sY);
	void set_temp_move_allowed_flag(int dX, int dY, bool flag);
	int analyze(char type, int *pX, int *pY, int * v1, int *v2, int * v3);
	bool get_is_water(short dX, short dY);
	void get_dead_owner(short * owner, char * owner_class, short sX, short sY);
	bool get_is_move_allowed_tile(short dX, short dY);
	void set_naming_value_empty(int value);
	int get_empty_naming_value();
	bool get_dynamic_object(short sX, short sY, short * type, uint32_t * register_time, int * index = 0);
	void set_dynamic_object(uint16_t id, short type, short sX, short sY, uint32_t register_time);
	bool get_is_teleport(short dX, short dY);
	bool search_teleport_dest(int sX, int sY, char * map_name, int * dx, int * dy, direction * dir);
	bool init(char * name);
	bool is_valid_loc(short sX, short sY);
	CItem* get_item(short sX, short sY, CItem** remain = nullptr);
	bool set_item(short sX, short sY, CItem * item);
	void clear_dead_owner(short sX, short sY);
	void clear_owner(int debug_code, short owner_h, char owner_type, short sX, short sY);
	bool get_moveable(short dX, short dY, short * d_otype = 0, short * top_item = 0);
	void get_owner(short * owner, char * owner_class, short sX, short sY);
	void set_owner(short owner, char owner_class, short sX, short sY);
	void set_dead_owner(short owner, char owner_class, short sX, short sY);
	bool remove_crops_total_sum();
	bool add_crops_total_sum();
	void set_big_owner(short owner, char owner_class, short sX, short sY, char area);

	CMap(class CGame * game);
	virtual ~CMap();

	class CTile * m_tile;
	class CGame * m_game;
	char  m_name[11];
	char  m_location_name[11];
	char  m_display_name[31];
	short m_size_x, m_size_y, m_tile_data_size;
	class CTeleportLoc * m_teleport_loc[hb::server::map::MaxTeleportLoc];
	
	//short m_sInitialPointX, m_sInitialPointY;
	hb::shared::geometry::GamePoint m_initial_point[hb::server::map::MaxInitialPoint];

	bool  m_naming_value_using_status[1000]; // 0~999
	bool  m_random_mob_generator;
	char  m_random_mob_generator_level;
	int   m_total_active_object;
	int   m_total_alive_object;
	int   m_maximum_object;

	char  m_type;

	bool  m_is_fixed_day_mode;

	struct {
		bool is_defined;
		char type;				// 1:RANDOMAREA   2:RANDOMWAYPOINT

		char waypoints[10];     // RANDOMWAYPOINT
		hb::shared::geometry::GameRectangle rcRect;			// RANDOMAREA

		int  total_active_mob;
		int  npc_config_id;
		int  max_mobs;
		int  cur_mobs;
		int  prob_sa;
		int  kind_sa;

	} m_spot_mob_generator[hb::server::map::MaxSpotMobGenerator];

	hb::shared::geometry::GamePoint m_waypoint_list[hb::server::map::MaxWaypointCfg];
	hb::shared::geometry::GameRectangle  m_mob_generator_avoid_rect[hb::server::map::MaxMgar];
	hb::shared::geometry::GameRectangle  m_no_attack_rect[hb::server::map::MaxNmr];

	hb::shared::geometry::GamePoint m_fish_point_list[hb::server::map::MaxFishPoint];
	int   m_total_fish_point, m_max_fish, m_cur_fish;
	
	int	  m_apocalypse_mob_gen_type, m_apocalypse_boss_mob_npc_id;
	hb::shared::geometry::GameRectangle m_apocalypse_boss_mob;
	char  m_dynamic_gate_type;
	hb::shared::geometry::GameRectangle m_dynamic_gate_coord;
	char  m_dynamic_gate_coord_dest_map[11];
	short m_dynamic_gate_coord_tgt_x, m_dynamic_gate_coord_tgt_y;
	bool  m_is_citizen_limit;
	short m_heldenian_tower_type, m_heldenian_tower_x_pos, m_heldenian_tower_y_pos;
	char  m_heldenian_tower_side;
	char  m_heldenian_mode_map;

	bool  m_mineral_generator;
	char  m_mineral_generator_level;
	hb::shared::geometry::GamePoint m_mineral_point_list[hb::server::map::MaxMineralPoint];
	int   m_total_mineral_point, m_max_mineral, m_cur_mineral;

	char  m_weather_status;		// . 0 . 1~3  4~6  7~9
	uint32_t m_weather_duration, m_weather_start_time;

	int   m_level_requirement;
	int   m_upper_level_limit;

	class COccupyFlag * m_occupy_flag[hb::server::map::MaxOccupyFlag];
	int   m_total_occupy_flags;
	
	class CStrategicPoint * m_strategic_point_list[hb::server::map::MaxStrategicPoints];
	bool  m_is_attack_enabled;

	bool  m_is_fight_zone;

	struct {
		char type;
		int x, y;

	} m_energy_sphere_creation_list[hb::server::map::MaxEnergySpheres];

	int m_total_energy_sphere_creation_point;
	
	struct {
		char result;
		int aresden_x, aresden_y, elvine_x, elvine_y;
	} m_energy_sphere_goal_list[hb::server::map::MaxEnergySpheres];

	int m_total_energy_sphere_goal_point;

	bool m_is_energy_sphere_goal_enabled;
	int m_cur_energy_sphere_goal_point_index; 

	struct {
		bool is_gate_map;
		char dynamic_gate_map[11];
		int dynamic_gate_x;
		int dynamic_gate_y;
	} m_dynamic_gate_coords[hb::server::map::MaxDynamicGates];

	struct {
		int player_activity;
		int neutral_activity;
		int aresden_activity;
		int elvine_activity;
		int monster_activity;

	} m_sector_info[hb::server::map::MaxSectors][hb::server::map::MaxSectors], m_temp_sector_info[hb::server::map::MaxSectors][hb::server::map::MaxSectors];
	short m_mob_event_amount;
	int m_total_item_events;
	struct {
		char item_name[hb::shared::limits::ItemNameLen];
		int amount;
		int total;
		int month;
		int day;
		int total_num;
	} m_item_event_list[hb::server::map::MaxItemEvents]{};

	struct {
		direction dir;
		short x;
		short y;
	} m_heldenian_gate_door[hb::server::map::MaxHeldenianDoor];

	struct {
		short type_id;
		short x;
		short y;
		char  side;
	} m_heldenian_tower[hb::server::map::MaxHeldenianTower];

	int m_top_neutral_sector_x, m_top_neutral_sector_y, m_top_aresden_sector_x, m_top_aresden_sector_y, m_top_elvine_sector_x, m_top_elvine_sector_y, m_top_monster_sector_x, m_top_monster_sector_y, m_top_player_sector_x, m_top_player_sector_y;
	
	struct {
		char related_map_name[11];
		int map_index;
		int x, y;
		int hp, init_hp;

		int effect_x[5];
		int effect_y[5];
	
	} m_strike_point[hb::server::map::MaxStrikePoints];
	int m_total_strike_points;

	bool m_is_disabled;
	int m_total_agriculture;

	struct {
		char type;			// NULL   .
		char side;
		short x, y;
	} m_crusade_structure_info[hb::shared::limits::MaxCrusadeStructures];
	int m_total_crusade_structures;
	bool m_is_energy_sphere_auto_creation;
private:
	bool decode_map_data_file_contents();
public:
	// Snow BOOLean for certain maps to snow instead of rain
	bool m_is_snow_enabled;
	bool m_is_recall_impossible;
	bool m_is_apocalypse_map;
	bool m_is_heldenian_map;
};
