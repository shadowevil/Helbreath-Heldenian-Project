#include "MapInfoSqliteStore.h"

#include <filesystem>
#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "Map.h"
#include "TeleportLoc.h"
#include "StrategicPoint.h"
#include "sqlite3.h"
#include "Log.h"

namespace
{
	bool ExecSql(sqlite3* db, const char* sql)
	{
		char* err = nullptr;
		int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
		if (rc != SQLITE_OK) {
			hb::logger::log("MapInfo SQLite exec failed: {}", err ? err : "unknown");
			sqlite3_free(err);
			return false;
		}
		return true;
	}

	bool PrepareAndBindText(sqlite3_stmt* stmt, int idx, const char* value)
	{
		return sqlite3_bind_text(stmt, idx, value, -1, SQLITE_TRANSIENT) == SQLITE_OK;
	}

	void CopyColumnText(sqlite3_stmt* stmt, int col, char* dest, size_t destSize)
	{
		const unsigned char* text = sqlite3_column_text(stmt, col);
		if (text == nullptr) {
			if (destSize > 0) {
				dest[0] = 0;
			}
			return;
		}
		std::snprintf(dest, destSize, "%s", reinterpret_cast<const char*>(text));
	}

	// Parse comma-separated waypoint indices into a char array
	void ParseWaypointList(const char* waypointStr, char* waypointArray, int maxWaypoints)
	{
		std::memset(waypointArray, -1, maxWaypoints);
		if (waypointStr == nullptr || waypointStr[0] == '\0') {
			return;
		}

		char buffer[256];
		std::strncpy(buffer, waypointStr, sizeof(buffer) - 1);
		buffer[sizeof(buffer) - 1] = '\0';

		int idx = 0;
		char* token = std::strtok(buffer, ",");
		while (token != nullptr && idx < maxWaypoints) {
			waypointArray[idx++] = static_cast<char>(std::atoi(token));
			token = std::strtok(nullptr, ",");
		}
	}
}

bool EnsureMapInfoDatabase(sqlite3** outDb, std::string& outPath, bool* outCreated)
{
	if (outDb == nullptr) {
		return false;
	}

	std::string dbPath = "mapinfo.db";
	if (!std::filesystem::exists(dbPath)) {
		auto exeDir = std::filesystem::current_path();
		dbPath = (exeDir / "mapinfo.db").string();
	}
	outPath = dbPath;

	bool created = !std::filesystem::exists(dbPath);

	sqlite3* db = nullptr;
	if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
		hb::logger::log("MapInfo SQLite open failed: {}", sqlite3_errmsg(db));
		sqlite3_close(db);
		return false;
	}

	sqlite3_busy_timeout(db, 1000);
	if (!ExecSql(db, "PRAGMA foreign_keys = ON;")) {
		sqlite3_close(db);
		return false;
	}

	// Create schema
	const char* schemaSql =
		"BEGIN;"
		"CREATE TABLE IF NOT EXISTS meta ("
		" key TEXT PRIMARY KEY,"
		" value TEXT NOT NULL"
		");"
		"INSERT OR REPLACE INTO meta(key, value) VALUES('schema_version','3');"

		// Core map settings
		"CREATE TABLE IF NOT EXISTS maps ("
		" map_name TEXT PRIMARY KEY CHECK(length(map_name) <= 10),"
		" location_name TEXT NOT NULL DEFAULT '' CHECK(length(location_name) <= 10),"
		" display_name TEXT NOT NULL DEFAULT '' CHECK(length(display_name) <= 30),"
		" maximum_object INTEGER NOT NULL DEFAULT 1000,"
		" level_limit INTEGER NOT NULL DEFAULT 0,"
		" upper_level_limit INTEGER NOT NULL DEFAULT 0,"
		" map_type INTEGER NOT NULL DEFAULT 0,"
		" random_mob_generator_enabled INTEGER NOT NULL DEFAULT 0,"
		" random_mob_generator_level INTEGER NOT NULL DEFAULT 0,"
		" mineral_generator_enabled INTEGER NOT NULL DEFAULT 0,"
		" mineral_generator_level INTEGER NOT NULL DEFAULT 0,"
		" max_fish INTEGER NOT NULL DEFAULT 0,"
		" max_mineral INTEGER NOT NULL DEFAULT 0,"
		" fixed_day_mode INTEGER NOT NULL DEFAULT 0,"
		" recall_impossible INTEGER NOT NULL DEFAULT 0,"
		" apocalypse_map INTEGER NOT NULL DEFAULT 0,"
		" apocalypse_mob_gen_type INTEGER NOT NULL DEFAULT 0,"
		" citizen_limit INTEGER NOT NULL DEFAULT 0,"
		" is_fight_zone INTEGER NOT NULL DEFAULT 0,"
		" heldenian_map INTEGER NOT NULL DEFAULT 0,"
		" heldenian_mode_map INTEGER NOT NULL DEFAULT 0,"
		" mob_event_amount INTEGER NOT NULL DEFAULT 15,"
		" energy_sphere_auto_creation INTEGER NOT NULL DEFAULT 0,"
		" pk_mode INTEGER NOT NULL DEFAULT 0,"
		" attack_enabled INTEGER NOT NULL DEFAULT 1"
		");"

		// Teleport locations
		"CREATE TABLE IF NOT EXISTS map_teleport_locations ("
		" map_name TEXT NOT NULL,"
		" teleport_index INTEGER NOT NULL,"
		" src_x INTEGER NOT NULL,"
		" src_y INTEGER NOT NULL,"
		" dest_map_name TEXT NOT NULL CHECK(length(dest_map_name) <= 10),"
		" dest_x INTEGER NOT NULL,"
		" dest_y INTEGER NOT NULL,"
		" direction INTEGER NOT NULL CHECK(direction >= 0 AND direction <= 8),"
		" PRIMARY KEY (map_name, teleport_index),"
		" FOREIGN KEY (map_name) REFERENCES maps(map_name) ON DELETE CASCADE"
		");"

		// Initial spawn points
		"CREATE TABLE IF NOT EXISTS map_initial_points ("
		" map_name TEXT NOT NULL,"
		" point_index INTEGER NOT NULL CHECK(point_index >= 0 AND point_index < 20),"
		" x INTEGER NOT NULL,"
		" y INTEGER NOT NULL,"
		" PRIMARY KEY (map_name, point_index),"
		" FOREIGN KEY (map_name) REFERENCES maps(map_name) ON DELETE CASCADE"
		");"

		// Waypoints
		"CREATE TABLE IF NOT EXISTS map_waypoints ("
		" map_name TEXT NOT NULL,"
		" waypoint_index INTEGER NOT NULL CHECK(waypoint_index >= 0 AND waypoint_index < 200),"
		" x INTEGER NOT NULL,"
		" y INTEGER NOT NULL,"
		" PRIMARY KEY (map_name, waypoint_index),"
		" FOREIGN KEY (map_name) REFERENCES maps(map_name) ON DELETE CASCADE"
		");"

		// No-attack areas
		"CREATE TABLE IF NOT EXISTS map_no_attack_areas ("
		" map_name TEXT NOT NULL,"
		" area_index INTEGER NOT NULL CHECK(area_index >= 0 AND area_index < 50),"
		" tile_x INTEGER NOT NULL,"
		" tile_y INTEGER NOT NULL,"
		" tile_w INTEGER NOT NULL,"
		" tile_h INTEGER NOT NULL,"
		" PRIMARY KEY (map_name, area_index),"
		" FOREIGN KEY (map_name) REFERENCES maps(map_name) ON DELETE CASCADE"
		");"

		// NPC avoid rects
		"CREATE TABLE IF NOT EXISTS map_npc_avoid_rects ("
		" map_name TEXT NOT NULL,"
		" rect_index INTEGER NOT NULL CHECK(rect_index >= 0 AND rect_index < 50),"
		" tile_x INTEGER NOT NULL,"
		" tile_y INTEGER NOT NULL,"
		" tile_w INTEGER NOT NULL,"
		" tile_h INTEGER NOT NULL,"
		" PRIMARY KEY (map_name, rect_index),"
		" FOREIGN KEY (map_name) REFERENCES maps(map_name) ON DELETE CASCADE"
		");"

		// Spot mob generators
		"CREATE TABLE IF NOT EXISTS map_spot_mob_generators ("
		" map_name TEXT NOT NULL,"
		" generator_index INTEGER NOT NULL CHECK(generator_index >= 0 AND generator_index < 100),"
		" generator_type INTEGER NOT NULL CHECK(generator_type IN (1, 2)),"
		" tile_x INTEGER NOT NULL DEFAULT 0,"
		" tile_y INTEGER NOT NULL DEFAULT 0,"
		" tile_w INTEGER NOT NULL DEFAULT 0,"
		" tile_h INTEGER NOT NULL DEFAULT 0,"
		" waypoints TEXT NOT NULL DEFAULT '',"
		" npc_config_id INTEGER NOT NULL,"
		" max_mobs INTEGER NOT NULL,"
		" prob_sa INTEGER NOT NULL DEFAULT 15,"
		" kind_sa INTEGER NOT NULL DEFAULT 1,"
		" PRIMARY KEY (map_name, generator_index),"
		" FOREIGN KEY (map_name) REFERENCES maps(map_name) ON DELETE CASCADE"
		");"

		// Fish points
		"CREATE TABLE IF NOT EXISTS map_fish_points ("
		" map_name TEXT NOT NULL,"
		" point_index INTEGER NOT NULL CHECK(point_index >= 0 AND point_index < 200),"
		" x INTEGER NOT NULL,"
		" y INTEGER NOT NULL,"
		" PRIMARY KEY (map_name, point_index),"
		" FOREIGN KEY (map_name) REFERENCES maps(map_name) ON DELETE CASCADE"
		");"

		// Mineral points
		"CREATE TABLE IF NOT EXISTS map_mineral_points ("
		" map_name TEXT NOT NULL,"
		" point_index INTEGER NOT NULL CHECK(point_index >= 0 AND point_index < 200),"
		" x INTEGER NOT NULL,"
		" y INTEGER NOT NULL,"
		" PRIMARY KEY (map_name, point_index),"
		" FOREIGN KEY (map_name) REFERENCES maps(map_name) ON DELETE CASCADE"
		");"

		// Strategic points
		"CREATE TABLE IF NOT EXISTS map_strategic_points ("
		" map_name TEXT NOT NULL,"
		" point_index INTEGER NOT NULL CHECK(point_index >= 0 AND point_index < 200),"
		" side INTEGER NOT NULL,"
		" point_value INTEGER NOT NULL,"
		" x INTEGER NOT NULL,"
		" y INTEGER NOT NULL,"
		" PRIMARY KEY (map_name, point_index),"
		" FOREIGN KEY (map_name) REFERENCES maps(map_name) ON DELETE CASCADE"
		");"

		// Energy sphere creation points
		"CREATE TABLE IF NOT EXISTS map_energy_sphere_creation ("
		" map_name TEXT NOT NULL,"
		" point_index INTEGER NOT NULL CHECK(point_index >= 0 AND point_index < 10),"
		" sphere_type INTEGER NOT NULL,"
		" x INTEGER NOT NULL,"
		" y INTEGER NOT NULL,"
		" PRIMARY KEY (map_name, point_index),"
		" FOREIGN KEY (map_name) REFERENCES maps(map_name) ON DELETE CASCADE"
		");"

		// Energy sphere goal points
		"CREATE TABLE IF NOT EXISTS map_energy_sphere_goal ("
		" map_name TEXT NOT NULL,"
		" point_index INTEGER NOT NULL CHECK(point_index >= 0 AND point_index < 10),"
		" result INTEGER NOT NULL,"
		" aresden_x INTEGER NOT NULL,"
		" aresden_y INTEGER NOT NULL,"
		" elvine_x INTEGER NOT NULL,"
		" elvine_y INTEGER NOT NULL,"
		" PRIMARY KEY (map_name, point_index),"
		" FOREIGN KEY (map_name) REFERENCES maps(map_name) ON DELETE CASCADE"
		");"

		// Strike points
		"CREATE TABLE IF NOT EXISTS map_strike_points ("
		" map_name TEXT NOT NULL,"
		" point_index INTEGER NOT NULL CHECK(point_index >= 0 AND point_index < 20),"
		" x INTEGER NOT NULL,"
		" y INTEGER NOT NULL,"
		" hp INTEGER NOT NULL,"
		" effect_x1 INTEGER NOT NULL,"
		" effect_y1 INTEGER NOT NULL,"
		" effect_x2 INTEGER NOT NULL,"
		" effect_y2 INTEGER NOT NULL,"
		" effect_x3 INTEGER NOT NULL,"
		" effect_y3 INTEGER NOT NULL,"
		" effect_x4 INTEGER NOT NULL,"
		" effect_y4 INTEGER NOT NULL,"
		" effect_x5 INTEGER NOT NULL,"
		" effect_y5 INTEGER NOT NULL,"
		" related_map_name TEXT NOT NULL CHECK(length(related_map_name) <= 10),"
		" PRIMARY KEY (map_name, point_index),"
		" FOREIGN KEY (map_name) REFERENCES maps(map_name) ON DELETE CASCADE"
		");"

		// Item events
		"CREATE TABLE IF NOT EXISTS map_item_events ("
		" map_name TEXT NOT NULL,"
		" event_index INTEGER NOT NULL CHECK(event_index >= 0 AND event_index < 200),"
		" item_name TEXT NOT NULL CHECK(length(item_name) <= 41),"
		" amount INTEGER NOT NULL,"
		" total_num INTEGER NOT NULL,"
		" event_month INTEGER NOT NULL,"
		" event_day INTEGER NOT NULL,"
		" event_type INTEGER NOT NULL DEFAULT 0,"
		" mob_list TEXT NOT NULL DEFAULT '',"
		" PRIMARY KEY (map_name, event_index),"
		" FOREIGN KEY (map_name) REFERENCES maps(map_name) ON DELETE CASCADE"
		");"

		// Heldenian towers
		"CREATE TABLE IF NOT EXISTS map_heldenian_towers ("
		" map_name TEXT NOT NULL,"
		" tower_index INTEGER NOT NULL CHECK(tower_index >= 0 AND tower_index < 200),"
		" type_id INTEGER NOT NULL,"
		" side INTEGER NOT NULL,"
		" x INTEGER NOT NULL,"
		" y INTEGER NOT NULL,"
		" PRIMARY KEY (map_name, tower_index),"
		" FOREIGN KEY (map_name) REFERENCES maps(map_name) ON DELETE CASCADE"
		");"

		// Heldenian gate doors
		"CREATE TABLE IF NOT EXISTS map_heldenian_gate_doors ("
		" map_name TEXT NOT NULL,"
		" door_index INTEGER NOT NULL CHECK(door_index >= 0 AND door_index < 200),"
		" direction INTEGER NOT NULL,"
		" x INTEGER NOT NULL,"
		" y INTEGER NOT NULL,"
		" PRIMARY KEY (map_name, door_index),"
		" FOREIGN KEY (map_name) REFERENCES maps(map_name) ON DELETE CASCADE"
		");"

		// Static NPCs
		"CREATE TABLE IF NOT EXISTS map_npcs ("
		" id INTEGER PRIMARY KEY AUTOINCREMENT,"
		" map_name TEXT NOT NULL,"
		" npc_config_id INTEGER NOT NULL,"
		" move_type INTEGER NOT NULL,"
		" waypoint_list TEXT NOT NULL DEFAULT '',"
		" name_prefix TEXT NOT NULL DEFAULT '' CHECK(length(name_prefix) <= 1),"
		" FOREIGN KEY (map_name) REFERENCES maps(map_name) ON DELETE CASCADE"
		");"

		// Apocalypse boss config
		"CREATE TABLE IF NOT EXISTS map_apocalypse_boss ("
		" map_name TEXT PRIMARY KEY,"
		" npc_id INTEGER NOT NULL,"
		" tile_x INTEGER NOT NULL,"
		" tile_y INTEGER NOT NULL,"
		" tile_w INTEGER NOT NULL,"
		" tile_h INTEGER NOT NULL,"
		" FOREIGN KEY (map_name) REFERENCES maps(map_name) ON DELETE CASCADE"
		");"

		// Dynamic gate config
		"CREATE TABLE IF NOT EXISTS map_dynamic_gate ("
		" map_name TEXT PRIMARY KEY,"
		" gate_type INTEGER NOT NULL,"
		" tile_x INTEGER NOT NULL,"
		" tile_y INTEGER NOT NULL,"
		" tile_w INTEGER NOT NULL,"
		" tile_h INTEGER NOT NULL,"
		" dest_map TEXT NOT NULL CHECK(length(dest_map) <= 10),"
		" dest_x INTEGER NOT NULL,"
		" dest_y INTEGER NOT NULL,"
		" FOREIGN KEY (map_name) REFERENCES maps(map_name) ON DELETE CASCADE"
		");"

		// Indexes for performance
		"CREATE INDEX IF NOT EXISTS idx_teleport_map ON map_teleport_locations(map_name);"
		"CREATE INDEX IF NOT EXISTS idx_initial_points_map ON map_initial_points(map_name);"
		"CREATE INDEX IF NOT EXISTS idx_waypoints_map ON map_waypoints(map_name);"
		"CREATE INDEX IF NOT EXISTS idx_no_attack_map ON map_no_attack_areas(map_name);"
		"CREATE INDEX IF NOT EXISTS idx_npc_avoid_map ON map_npc_avoid_rects(map_name);"
		"CREATE INDEX IF NOT EXISTS idx_spot_mob_map ON map_spot_mob_generators(map_name);"
		"CREATE INDEX IF NOT EXISTS idx_fish_map ON map_fish_points(map_name);"
		"CREATE INDEX IF NOT EXISTS idx_mineral_map ON map_mineral_points(map_name);"
		"CREATE INDEX IF NOT EXISTS idx_strategic_map ON map_strategic_points(map_name);"
		"CREATE INDEX IF NOT EXISTS idx_sphere_creation_map ON map_energy_sphere_creation(map_name);"
		"CREATE INDEX IF NOT EXISTS idx_sphere_goal_map ON map_energy_sphere_goal(map_name);"
		"CREATE INDEX IF NOT EXISTS idx_strike_map ON map_strike_points(map_name);"
		"CREATE INDEX IF NOT EXISTS idx_item_events_map ON map_item_events(map_name);"
		"CREATE INDEX IF NOT EXISTS idx_heldenian_towers_map ON map_heldenian_towers(map_name);"
		"CREATE INDEX IF NOT EXISTS idx_heldenian_doors_map ON map_heldenian_gate_doors(map_name);"
		"CREATE INDEX IF NOT EXISTS idx_npcs_map ON map_npcs(map_name);"

		"COMMIT;";

	if (!ExecSql(db, schemaSql)) {
		sqlite3_close(db);
		return false;
	}

	// Migration: add display_name column if missing (v2 -> v3)
	{
		sqlite3_stmt* ti = nullptr;
		bool has_display_name = false;
		if (sqlite3_prepare_v2(db, "PRAGMA table_info(maps);", -1, &ti, nullptr) == SQLITE_OK) {
			while (sqlite3_step(ti) == SQLITE_ROW) {
				const char* col_name = reinterpret_cast<const char*>(sqlite3_column_text(ti, 1));
				if (col_name && std::strcmp(col_name, "display_name") == 0) {
					has_display_name = true;
					break;
				}
			}
			sqlite3_finalize(ti);
		}
		if (!has_display_name) {
			ExecSql(db, "ALTER TABLE maps ADD COLUMN display_name TEXT NOT NULL DEFAULT '';");
		}
	}

	*outDb = db;
	if (outCreated != nullptr) {
		*outCreated = created;
	}
	return true;
}

void CloseMapInfoDatabase(sqlite3* db)
{
	if (db != nullptr) {
		sqlite3_close(db);
	}
}

int GetMapNamesFromDatabase(sqlite3* db, char mapNames[][11], int maxMaps)
{
	if (db == nullptr || mapNames == nullptr) {
		return 0;
	}

	const char* sql = "SELECT map_name FROM maps ORDER BY map_name;";
	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
		hb::logger::log("MapInfo SQLite: GetMapNames prepare failed: {}", sqlite3_errmsg(db));
		return 0;
	}

	int count = 0;
	while (sqlite3_step(stmt) == SQLITE_ROW && count < maxMaps) {
		CopyColumnText(stmt, 0, mapNames[count], 11);
		count++;
	}

	sqlite3_finalize(stmt);
	return count;
}

bool LoadMapBaseSettings(sqlite3* db, const char* map_name, CMap* map)
{
	if (db == nullptr || map_name == nullptr || map == nullptr) {
		return false;
	}

	const char* sql =
		"SELECT location_name, maximum_object, level_limit, upper_level_limit, map_type,"
		" random_mob_generator_enabled, random_mob_generator_level,"
		" mineral_generator_enabled, mineral_generator_level,"
		" max_fish, max_mineral, fixed_day_mode, recall_impossible,"
		" apocalypse_map, apocalypse_mob_gen_type, citizen_limit, is_fight_zone,"
		" heldenian_map, heldenian_mode_map, mob_event_amount, energy_sphere_auto_creation, pk_mode,"
		" attack_enabled, display_name"
		" FROM maps WHERE map_name = ? COLLATE NOCASE;";

	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
		return false;
	}

	PrepareAndBindText(stmt, 1, map_name);

	bool ok = false;
	if (sqlite3_step(stmt) == SQLITE_ROW) {
		int col = 0;
		CopyColumnText(stmt, col++, map->m_location_name, sizeof(map->m_location_name));
		map->m_maximum_object = sqlite3_column_int(stmt, col++);
		map->m_level_requirement = sqlite3_column_int(stmt, col++);
		map->m_upper_level_limit = sqlite3_column_int(stmt, col++);
		map->m_type = static_cast<char>(sqlite3_column_int(stmt, col++));
		map->m_random_mob_generator = sqlite3_column_int(stmt, col++) != 0;
		map->m_random_mob_generator_level = static_cast<char>(sqlite3_column_int(stmt, col++));
		map->m_mineral_generator = sqlite3_column_int(stmt, col++) != 0;
		map->m_mineral_generator_level = static_cast<char>(sqlite3_column_int(stmt, col++));
		map->m_max_fish = sqlite3_column_int(stmt, col++);
		map->m_max_mineral = sqlite3_column_int(stmt, col++);
		map->m_is_fixed_day_mode = sqlite3_column_int(stmt, col++) != 0;
		map->m_is_recall_impossible = sqlite3_column_int(stmt, col++) != 0;
		map->m_is_apocalypse_map = sqlite3_column_int(stmt, col++) != 0;
		map->m_apocalypse_mob_gen_type = sqlite3_column_int(stmt, col++);
		map->m_is_citizen_limit = sqlite3_column_int(stmt, col++) != 0;
		map->m_is_fight_zone = sqlite3_column_int(stmt, col++) != 0;
		map->m_is_heldenian_map = sqlite3_column_int(stmt, col++) != 0;
		map->m_heldenian_mode_map = static_cast<char>(sqlite3_column_int(stmt, col++));
		map->m_mob_event_amount = static_cast<short>(sqlite3_column_int(stmt, col++));
		map->m_is_energy_sphere_auto_creation = sqlite3_column_int(stmt, col++) != 0;
		col++; // pk_mode - read but no direct member
		map->m_is_attack_enabled = sqlite3_column_int(stmt, col++) != 0;
		CopyColumnText(stmt, col++, map->m_display_name, sizeof(map->m_display_name));
		ok = true;
	}

	sqlite3_finalize(stmt);
	return ok;
}

bool LoadMapTeleportLocations(sqlite3* db, const char* map_name, CMap* map)
{
	if (db == nullptr || map_name == nullptr || map == nullptr) {
		return false;
	}

	const char* sql =
		"SELECT teleport_index, src_x, src_y, dest_map_name, dest_x, dest_y, direction"
		" FROM map_teleport_locations WHERE map_name = ? COLLATE NOCASE ORDER BY teleport_index;";

	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
		return false;
	}

	PrepareAndBindText(stmt, 1, map_name);

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		int idx = sqlite3_column_int(stmt, 0);
		if (idx < 0 || idx >= hb::server::map::MaxTeleportLoc) continue;

		if (map->m_teleport_loc[idx] == nullptr) {
			map->m_teleport_loc[idx] = new CTeleportLoc;
		}

		CTeleportLoc* tele = map->m_teleport_loc[idx];
		tele->m_src_x = static_cast<short>(sqlite3_column_int(stmt, 1));
		tele->m_src_y = static_cast<short>(sqlite3_column_int(stmt, 2));
		CopyColumnText(stmt, 3, tele->m_dest_map_name, sizeof(tele->m_dest_map_name));
		tele->m_dest_x = static_cast<short>(sqlite3_column_int(stmt, 4));
		tele->m_dest_y = static_cast<short>(sqlite3_column_int(stmt, 5));
		tele->m_dir = static_cast<direction>(sqlite3_column_int(stmt, 6));
	}

	sqlite3_finalize(stmt);
	return true;
}

bool LoadMapInitialPoints(sqlite3* db, const char* map_name, CMap* map)
{
	if (db == nullptr || map_name == nullptr || map == nullptr) {
		return false;
	}

	const char* sql =
		"SELECT point_index, x, y FROM map_initial_points WHERE map_name = ? COLLATE NOCASE ORDER BY point_index;";

	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
		return false;
	}

	PrepareAndBindText(stmt, 1, map_name);

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		int idx = sqlite3_column_int(stmt, 0);
		if (idx < 0 || idx >= hb::server::map::MaxInitialPoint) continue;

		map->m_initial_point[idx].x = sqlite3_column_int(stmt, 1);
		map->m_initial_point[idx].y = sqlite3_column_int(stmt, 2);
	}

	sqlite3_finalize(stmt);
	return true;
}

bool LoadMapWaypoints(sqlite3* db, const char* map_name, CMap* map)
{
	if (db == nullptr || map_name == nullptr || map == nullptr) {
		return false;
	}

	const char* sql =
		"SELECT waypoint_index, x, y FROM map_waypoints WHERE map_name = ? COLLATE NOCASE ORDER BY waypoint_index;";

	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
		return false;
	}

	PrepareAndBindText(stmt, 1, map_name);

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		int idx = sqlite3_column_int(stmt, 0);
		if (idx < 0 || idx >= hb::server::map::MaxWaypointCfg) continue;

		map->m_waypoint_list[idx].x = sqlite3_column_int(stmt, 1);
		map->m_waypoint_list[idx].y = sqlite3_column_int(stmt, 2);
	}

	sqlite3_finalize(stmt);
	return true;
}

bool LoadMapNoAttackAreas(sqlite3* db, const char* map_name, CMap* map)
{
	if (db == nullptr || map_name == nullptr || map == nullptr) {
		return false;
	}

	const char* sql =
		"SELECT area_index, tile_x, tile_y, tile_w, tile_h"
		" FROM map_no_attack_areas WHERE map_name = ? COLLATE NOCASE ORDER BY area_index;";

	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
		return false;
	}

	PrepareAndBindText(stmt, 1, map_name);

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		int idx = sqlite3_column_int(stmt, 0);
		if (idx < 0 || idx >= hb::server::map::MaxNmr) continue;

		map->m_no_attack_rect[idx] = hb::shared::geometry::GameRectangle(
			sqlite3_column_int(stmt, 1),
			sqlite3_column_int(stmt, 2),
			sqlite3_column_int(stmt, 3),
			sqlite3_column_int(stmt, 4));
	}

	sqlite3_finalize(stmt);
	return true;
}

bool LoadMapNpcAvoidRects(sqlite3* db, const char* map_name, CMap* map)
{
	if (db == nullptr || map_name == nullptr || map == nullptr) {
		return false;
	}

	const char* sql =
		"SELECT rect_index, tile_x, tile_y, tile_w, tile_h"
		" FROM map_npc_avoid_rects WHERE map_name = ? COLLATE NOCASE ORDER BY rect_index;";

	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
		return false;
	}

	PrepareAndBindText(stmt, 1, map_name);

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		int idx = sqlite3_column_int(stmt, 0);
		if (idx < 0 || idx >= hb::server::map::MaxMgar) continue;

		map->m_mob_generator_avoid_rect[idx] = hb::shared::geometry::GameRectangle(
			sqlite3_column_int(stmt, 1),
			sqlite3_column_int(stmt, 2),
			sqlite3_column_int(stmt, 3),
			sqlite3_column_int(stmt, 4));
	}

	sqlite3_finalize(stmt);
	return true;
}

bool LoadMapSpotMobGenerators(sqlite3* db, const char* map_name, CMap* map)
{
	if (db == nullptr || map_name == nullptr || map == nullptr) {
		return false;
	}

	const char* sql =
		"SELECT generator_index, generator_type, tile_x, tile_y, tile_w, tile_h,"
		" waypoints, npc_config_id, max_mobs, prob_sa, kind_sa"
		" FROM map_spot_mob_generators WHERE map_name = ? COLLATE NOCASE ORDER BY generator_index;";

	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
		return false;
	}

	PrepareAndBindText(stmt, 1, map_name);

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		int idx = sqlite3_column_int(stmt, 0);
		if (idx < 0 || idx >= hb::server::map::MaxSpotMobGenerator) continue;

		map->m_spot_mob_generator[idx].is_defined = true;
		map->m_spot_mob_generator[idx].type = static_cast<char>(sqlite3_column_int(stmt, 1));
		map->m_spot_mob_generator[idx].rcRect = hb::shared::geometry::GameRectangle(
			sqlite3_column_int(stmt, 2),
			sqlite3_column_int(stmt, 3),
			sqlite3_column_int(stmt, 4),
			sqlite3_column_int(stmt, 5));

		// Parse waypoints for type 2
		char waypointStr[256] = {};
		CopyColumnText(stmt, 6, waypointStr, sizeof(waypointStr));
		ParseWaypointList(waypointStr, map->m_spot_mob_generator[idx].waypoints, 10);

		map->m_spot_mob_generator[idx].npc_config_id = sqlite3_column_int(stmt, 7);
		map->m_spot_mob_generator[idx].max_mobs = sqlite3_column_int(stmt, 8);
		map->m_spot_mob_generator[idx].prob_sa = sqlite3_column_int(stmt, 9);
		map->m_spot_mob_generator[idx].kind_sa = sqlite3_column_int(stmt, 10);
		map->m_spot_mob_generator[idx].cur_mobs = 0;
		map->m_spot_mob_generator[idx].total_active_mob = 0;
	}

	sqlite3_finalize(stmt);
	return true;
}

bool LoadMapFishPoints(sqlite3* db, const char* map_name, CMap* map)
{
	if (db == nullptr || map_name == nullptr || map == nullptr) {
		return false;
	}

	const char* sql =
		"SELECT point_index, x, y FROM map_fish_points WHERE map_name = ? COLLATE NOCASE ORDER BY point_index;";

	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
		return false;
	}

	PrepareAndBindText(stmt, 1, map_name);

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		int idx = sqlite3_column_int(stmt, 0);
		if (idx < 0 || idx >= hb::server::map::MaxFishPoint) continue;

		map->m_fish_point_list[idx].x = sqlite3_column_int(stmt, 1);
		map->m_fish_point_list[idx].y = sqlite3_column_int(stmt, 2);
		map->m_total_fish_point++;
	}

	sqlite3_finalize(stmt);
	return true;
}

bool LoadMapMineralPoints(sqlite3* db, const char* map_name, CMap* map)
{
	if (db == nullptr || map_name == nullptr || map == nullptr) {
		return false;
	}

	const char* sql =
		"SELECT point_index, x, y FROM map_mineral_points WHERE map_name = ? COLLATE NOCASE ORDER BY point_index;";

	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
		return false;
	}

	PrepareAndBindText(stmt, 1, map_name);

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		int idx = sqlite3_column_int(stmt, 0);
		if (idx < 0 || idx >= hb::server::map::MaxMineralPoint) continue;

		map->m_mineral_point_list[idx].x = sqlite3_column_int(stmt, 1);
		map->m_mineral_point_list[idx].y = sqlite3_column_int(stmt, 2);
		map->m_total_mineral_point++;
	}

	sqlite3_finalize(stmt);
	return true;
}

bool LoadMapStrategicPoints(sqlite3* db, const char* map_name, CMap* map)
{
	if (db == nullptr || map_name == nullptr || map == nullptr) {
		return false;
	}

	const char* sql =
		"SELECT point_index, side, point_value, x, y"
		" FROM map_strategic_points WHERE map_name = ? COLLATE NOCASE ORDER BY point_index;";

	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
		return false;
	}

	PrepareAndBindText(stmt, 1, map_name);

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		int idx = sqlite3_column_int(stmt, 0);
		if (idx < 0 || idx >= hb::server::map::MaxStrategicPoints) continue;

		if (map->m_strategic_point_list[idx] == nullptr) {
			map->m_strategic_point_list[idx] = new CStrategicPoint;
		}

		map->m_strategic_point_list[idx]->m_side = sqlite3_column_int(stmt, 1);
		map->m_strategic_point_list[idx]->m_value = sqlite3_column_int(stmt, 2);
		map->m_strategic_point_list[idx]->m_x = sqlite3_column_int(stmt, 3);
		map->m_strategic_point_list[idx]->m_y = sqlite3_column_int(stmt, 4);
	}

	sqlite3_finalize(stmt);
	return true;
}

bool LoadMapEnergySphereCreationPoints(sqlite3* db, const char* map_name, CMap* map)
{
	if (db == nullptr || map_name == nullptr || map == nullptr) {
		return false;
	}

	const char* sql =
		"SELECT point_index, sphere_type, x, y"
		" FROM map_energy_sphere_creation WHERE map_name = ? COLLATE NOCASE ORDER BY point_index;";

	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
		return false;
	}

	PrepareAndBindText(stmt, 1, map_name);

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		int idx = sqlite3_column_int(stmt, 0);
		if (idx < 0 || idx >= hb::server::map::MaxEnergySpheres) continue;

		map->m_energy_sphere_creation_list[idx].type = static_cast<char>(sqlite3_column_int(stmt, 1));
		map->m_energy_sphere_creation_list[idx].x = sqlite3_column_int(stmt, 2);
		map->m_energy_sphere_creation_list[idx].y = sqlite3_column_int(stmt, 3);
		map->m_total_energy_sphere_creation_point++;
	}

	sqlite3_finalize(stmt);
	return true;
}

bool LoadMapEnergySphereGoalPoints(sqlite3* db, const char* map_name, CMap* map)
{
	if (db == nullptr || map_name == nullptr || map == nullptr) {
		return false;
	}

	const char* sql =
		"SELECT point_index, result, aresden_x, aresden_y, elvine_x, elvine_y"
		" FROM map_energy_sphere_goal WHERE map_name = ? COLLATE NOCASE ORDER BY point_index;";

	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
		return false;
	}

	PrepareAndBindText(stmt, 1, map_name);

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		int idx = sqlite3_column_int(stmt, 0);
		if (idx < 0 || idx >= hb::server::map::MaxEnergySpheres) continue;

		map->m_energy_sphere_goal_list[idx].result = static_cast<char>(sqlite3_column_int(stmt, 1));
		map->m_energy_sphere_goal_list[idx].aresden_x = sqlite3_column_int(stmt, 2);
		map->m_energy_sphere_goal_list[idx].aresden_y = sqlite3_column_int(stmt, 3);
		map->m_energy_sphere_goal_list[idx].elvine_x = sqlite3_column_int(stmt, 4);
		map->m_energy_sphere_goal_list[idx].elvine_y = sqlite3_column_int(stmt, 5);
		map->m_total_energy_sphere_goal_point++;
	}

	sqlite3_finalize(stmt);
	return true;
}

bool LoadMapStrikePoints(sqlite3* db, const char* map_name, CMap* map)
{
	if (db == nullptr || map_name == nullptr || map == nullptr) {
		return false;
	}

	const char* sql =
		"SELECT point_index, x, y, hp, effect_x1, effect_y1, effect_x2, effect_y2,"
		" effect_x3, effect_y3, effect_x4, effect_y4, effect_x5, effect_y5, related_map_name"
		" FROM map_strike_points WHERE map_name = ? COLLATE NOCASE ORDER BY point_index;";

	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
		return false;
	}

	PrepareAndBindText(stmt, 1, map_name);

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		int idx = sqlite3_column_int(stmt, 0);
		if (idx < 0 || idx >= hb::server::map::MaxStrikePoints) continue;

		map->m_strike_point[idx].x = sqlite3_column_int(stmt, 1);
		map->m_strike_point[idx].y = sqlite3_column_int(stmt, 2);
		map->m_strike_point[idx].hp = sqlite3_column_int(stmt, 3);
		map->m_strike_point[idx].init_hp = map->m_strike_point[idx].hp;
		map->m_strike_point[idx].effect_x[0] = sqlite3_column_int(stmt, 4);
		map->m_strike_point[idx].effect_y[0] = sqlite3_column_int(stmt, 5);
		map->m_strike_point[idx].effect_x[1] = sqlite3_column_int(stmt, 6);
		map->m_strike_point[idx].effect_y[1] = sqlite3_column_int(stmt, 7);
		map->m_strike_point[idx].effect_x[2] = sqlite3_column_int(stmt, 8);
		map->m_strike_point[idx].effect_y[2] = sqlite3_column_int(stmt, 9);
		map->m_strike_point[idx].effect_x[3] = sqlite3_column_int(stmt, 10);
		map->m_strike_point[idx].effect_y[3] = sqlite3_column_int(stmt, 11);
		map->m_strike_point[idx].effect_x[4] = sqlite3_column_int(stmt, 12);
		map->m_strike_point[idx].effect_y[4] = sqlite3_column_int(stmt, 13);
		CopyColumnText(stmt, 14, map->m_strike_point[idx].related_map_name,
			sizeof(map->m_strike_point[idx].related_map_name));
	}

	sqlite3_finalize(stmt);
	return true;
}

bool LoadMapItemEvents(sqlite3* db, const char* map_name, CMap* map)
{
	if (db == nullptr || map_name == nullptr || map == nullptr) {
		return false;
	}

	const char* sql =
		"SELECT event_index, item_name, amount, total_num, event_month, event_day, event_type, mob_list"
		" FROM map_item_events WHERE map_name = ? COLLATE NOCASE ORDER BY event_index;";

	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
		return false;
	}

	PrepareAndBindText(stmt, 1, map_name);

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		int idx = sqlite3_column_int(stmt, 0);
		if (idx < 0 || idx >= hb::server::map::MaxItemEvents) continue;

		CopyColumnText(stmt, 1, map->m_item_event_list[idx].item_name,
			sizeof(map->m_item_event_list[idx].item_name));
		map->m_item_event_list[idx].amount = sqlite3_column_int(stmt, 2);
		map->m_item_event_list[idx].total_num = sqlite3_column_int(stmt, 3);
		map->m_item_event_list[idx].month = sqlite3_column_int(stmt, 4);
		map->m_item_event_list[idx].day = sqlite3_column_int(stmt, 5);
		// event_type (column 6) is not stored in CMap struct
		// mob_list (column 7) could be parsed into separate mob fields if needed
		map->m_total_item_events++;
	}

	sqlite3_finalize(stmt);
	return true;
}

bool LoadMapHeldenianTowers(sqlite3* db, const char* map_name, CMap* map)
{
	if (db == nullptr || map_name == nullptr || map == nullptr) {
		return false;
	}

	const char* sql =
		"SELECT tower_index, type_id, side, x, y"
		" FROM map_heldenian_towers WHERE map_name = ? COLLATE NOCASE ORDER BY tower_index;";

	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
		return false;
	}

	PrepareAndBindText(stmt, 1, map_name);

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		int idx = sqlite3_column_int(stmt, 0);
		if (idx < 0 || idx >= hb::server::map::MaxHeldenianTower) continue;

		map->m_heldenian_tower[idx].type_id = static_cast<short>(sqlite3_column_int(stmt, 1));
		map->m_heldenian_tower[idx].side = static_cast<char>(sqlite3_column_int(stmt, 2));
		map->m_heldenian_tower[idx].x = static_cast<short>(sqlite3_column_int(stmt, 3));
		map->m_heldenian_tower[idx].y = static_cast<short>(sqlite3_column_int(stmt, 4));
	}

	sqlite3_finalize(stmt);
	return true;
}

bool LoadMapHeldenianGateDoors(sqlite3* db, const char* map_name, CMap* map)
{
	if (db == nullptr || map_name == nullptr || map == nullptr) {
		return false;
	}

	const char* sql =
		"SELECT door_index, direction, x, y"
		" FROM map_heldenian_gate_doors WHERE map_name = ? COLLATE NOCASE ORDER BY door_index;";

	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
		return false;
	}

	PrepareAndBindText(stmt, 1, map_name);

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		int idx = sqlite3_column_int(stmt, 0);
		if (idx < 0 || idx >= hb::server::map::MaxHeldenianDoor) continue;

		map->m_heldenian_gate_door[idx].dir = static_cast<direction>(sqlite3_column_int(stmt, 1));
		map->m_heldenian_gate_door[idx].x = static_cast<short>(sqlite3_column_int(stmt, 2));
		map->m_heldenian_gate_door[idx].y = static_cast<short>(sqlite3_column_int(stmt, 3));
	}

	sqlite3_finalize(stmt);
	return true;
}

bool LoadMapNpcs(sqlite3* db, const char* map_name, CMap* map)
{
	// NPCs are handled differently - they are spawned by CGame, not stored in CMap directly
	// This function is a placeholder for future NPC spawning from database
	return true;
}

bool LoadMapApocalypseBoss(sqlite3* db, const char* map_name, CMap* map)
{
	if (db == nullptr || map_name == nullptr || map == nullptr) {
		return false;
	}

	const char* sql =
		"SELECT npc_id, tile_x, tile_y, tile_w, tile_h"
		" FROM map_apocalypse_boss WHERE map_name = ? COLLATE NOCASE;";

	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
		return false;
	}

	PrepareAndBindText(stmt, 1, map_name);

	if (sqlite3_step(stmt) == SQLITE_ROW) {
		map->m_apocalypse_boss_mob_npc_id = sqlite3_column_int(stmt, 0);
		map->m_apocalypse_boss_mob = hb::shared::geometry::GameRectangle(
			sqlite3_column_int(stmt, 1),
			sqlite3_column_int(stmt, 2),
			sqlite3_column_int(stmt, 3),
			sqlite3_column_int(stmt, 4));
	}

	sqlite3_finalize(stmt);
	return true;
}

bool LoadMapDynamicGate(sqlite3* db, const char* map_name, CMap* map)
{
	if (db == nullptr || map_name == nullptr || map == nullptr) {
		return false;
	}

	const char* sql =
		"SELECT gate_type, tile_x, tile_y, tile_w, tile_h, dest_map, dest_x, dest_y"
		" FROM map_dynamic_gate WHERE map_name = ? COLLATE NOCASE;";

	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
		return false;
	}

	PrepareAndBindText(stmt, 1, map_name);

	if (sqlite3_step(stmt) == SQLITE_ROW) {
		map->m_dynamic_gate_type = static_cast<char>(sqlite3_column_int(stmt, 0));
		map->m_dynamic_gate_coord = hb::shared::geometry::GameRectangle(
			sqlite3_column_int(stmt, 1),
			sqlite3_column_int(stmt, 2),
			sqlite3_column_int(stmt, 3),
			sqlite3_column_int(stmt, 4));
		CopyColumnText(stmt, 5, map->m_dynamic_gate_coord_dest_map,
			sizeof(map->m_dynamic_gate_coord_dest_map));
		map->m_dynamic_gate_coord_tgt_x = static_cast<short>(sqlite3_column_int(stmt, 6));
		map->m_dynamic_gate_coord_tgt_y = static_cast<short>(sqlite3_column_int(stmt, 7));
	}

	sqlite3_finalize(stmt);
	return true;
}

bool LoadMapConfig(sqlite3* db, const char* map_name, CMap* map)
{
	if (db == nullptr || map_name == nullptr || map == nullptr) {
		return false;
	}

	// Load all components
	if (!LoadMapBaseSettings(db, map_name, map)) {
		hb::logger::log("MapInfo SQLite: failed to load base settings for map {}", map_name);
		return false;
	}

	// Load child tables - these are non-critical, log but don't fail
	LoadMapTeleportLocations(db, map_name, map);
	LoadMapInitialPoints(db, map_name, map);
	LoadMapWaypoints(db, map_name, map);
	LoadMapNoAttackAreas(db, map_name, map);
	LoadMapNpcAvoidRects(db, map_name, map);
	LoadMapSpotMobGenerators(db, map_name, map);
	LoadMapFishPoints(db, map_name, map);
	LoadMapMineralPoints(db, map_name, map);
	LoadMapStrategicPoints(db, map_name, map);
	LoadMapEnergySphereCreationPoints(db, map_name, map);
	LoadMapEnergySphereGoalPoints(db, map_name, map);
	LoadMapStrikePoints(db, map_name, map);
	LoadMapItemEvents(db, map_name, map);
	LoadMapHeldenianTowers(db, map_name, map);
	LoadMapHeldenianGateDoors(db, map_name, map);
	LoadMapNpcs(db, map_name, map);
	LoadMapApocalypseBoss(db, map_name, map);
	LoadMapDynamicGate(db, map_name, map);

	return true;
}

bool SaveMapConfig(sqlite3* db, const CMap* map)
{
	// TODO: Implement save functionality for admin tools
	return false;
}

bool SaveAllMapConfigs(sqlite3* db, CMap** map_list, int mapCount)
{
	// TODO: Implement save functionality for admin tools
	return false;
}

bool HasMapInfoRows(sqlite3* db, const char* tableName)
{
	if (db == nullptr || tableName == nullptr) {
		return false;
	}

	char sql[256] = {};
	std::snprintf(sql, sizeof(sql), "SELECT COUNT(*) FROM %s;", tableName);

	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
		return false;
	}

	bool hasRows = false;
	if (sqlite3_step(stmt) == SQLITE_ROW) {
		hasRows = sqlite3_column_int(stmt, 0) > 0;
	}

	sqlite3_finalize(stmt);
	return hasRows;
}
