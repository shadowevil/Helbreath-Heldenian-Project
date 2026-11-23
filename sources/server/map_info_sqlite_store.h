#pragma once

#include <string>

struct sqlite3;
class CMap;

// Database management
bool EnsureMapInfoDatabase(sqlite3** outDb, std::string& outPath, bool* outCreated);
void CloseMapInfoDatabase(sqlite3* db);

// Load all map names from database (returns count, fills mapNames array)
int GetMapNamesFromDatabase(sqlite3* db, char mapNames[][11], int maxMaps);

// Load complete map configuration from database into CMap object
bool LoadMapConfig(sqlite3* db, const char* map_name, CMap* map);

// Individual loaders (used internally by LoadMapConfig)
bool LoadMapBaseSettings(sqlite3* db, const char* map_name, CMap* map);
bool LoadMapTeleportLocations(sqlite3* db, const char* map_name, CMap* map);
bool LoadMapInitialPoints(sqlite3* db, const char* map_name, CMap* map);
bool LoadMapWaypoints(sqlite3* db, const char* map_name, CMap* map);
bool LoadMapNoAttackAreas(sqlite3* db, const char* map_name, CMap* map);
bool LoadMapNpcAvoidRects(sqlite3* db, const char* map_name, CMap* map);
bool LoadMapSpotMobGenerators(sqlite3* db, const char* map_name, CMap* map);
bool LoadMapFishPoints(sqlite3* db, const char* map_name, CMap* map);
bool LoadMapMineralPoints(sqlite3* db, const char* map_name, CMap* map);
bool LoadMapStrategicPoints(sqlite3* db, const char* map_name, CMap* map);
bool LoadMapEnergySphereCreationPoints(sqlite3* db, const char* map_name, CMap* map);
bool LoadMapEnergySphereGoalPoints(sqlite3* db, const char* map_name, CMap* map);
bool LoadMapStrikePoints(sqlite3* db, const char* map_name, CMap* map);
bool LoadMapItemEvents(sqlite3* db, const char* map_name, CMap* map);
bool LoadMapHeldenianTowers(sqlite3* db, const char* map_name, CMap* map);
bool LoadMapHeldenianGateDoors(sqlite3* db, const char* map_name, CMap* map);
bool LoadMapNpcs(sqlite3* db, const char* map_name, CMap* map);
bool LoadMapApocalypseBoss(sqlite3* db, const char* map_name, CMap* map);
bool LoadMapDynamicGate(sqlite3* db, const char* map_name, CMap* map);

// Save functions (for admin tools / future use)
bool SaveMapConfig(sqlite3* db, const CMap* map);
bool SaveAllMapConfigs(sqlite3* db, CMap** map_list, int mapCount);

// Utility
bool HasMapInfoRows(sqlite3* db, const char* tableName);
