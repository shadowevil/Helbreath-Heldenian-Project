#pragma once

#include <string>
#include <vector>

struct sqlite3;
class CItem;
class CGame;

struct color_palette_entry
{
	uint8_t color_id;
	uint8_t r;
	uint8_t g;
	uint8_t b;
};

struct creation_item_entry
{
	int class_type;    // 0=all, 1=warrior, 2=mage, 3=master
	int item_id;
	int count;
	int item_color;
	int durability;    // 0 = use item's max_durability from config
	int is_equipped;
	int gender_limit;  // 0=any, 1=male only, 2=female only
	int sort_order;
};

struct attribute_prefix_type_entry
{
	uint8_t prefix_id;
	uint8_t multiplier;
	uint8_t min_value;
	uint8_t max_value;
};

struct attribute_secondary_type_entry
{
	uint8_t secondary_id;
	uint8_t multiplier;
	uint8_t min_value;
	uint8_t max_value;
};

bool EnsureGameConfigDatabase(sqlite3** outDb, std::string& outPath, bool* outCreated);
bool SaveItemConfigs(sqlite3* db, CItem* const* itemList, int maxItems);
bool LoadItemConfigs(sqlite3* db, CItem** itemList, int maxItems);
bool LoadActiveMaps(sqlite3* db, CGame* game);
bool SaveBannedListConfig(sqlite3* db, const CGame* game);
bool LoadBannedListConfig(sqlite3* db, CGame* game);
bool SaveAdminConfig(sqlite3* db, const CGame* game);
bool LoadAdminConfig(sqlite3* db, CGame* game);
bool SaveCommandPermissions(sqlite3* db, const CGame* game);
bool LoadCommandPermissions(sqlite3* db, CGame* game);
bool SaveNpcConfigs(sqlite3* db, const CGame* game);
bool LoadNpcConfigs(sqlite3* db, CGame* game);
bool LoadDropTables(sqlite3* db, CGame* game);
bool LoadShopConfigs(sqlite3* db, CGame* game);
bool LoadSummonThresholds(sqlite3* db, CGame* game);
bool SaveMagicConfigs(sqlite3* db, const CGame* game);
bool LoadMagicConfigs(sqlite3* db, CGame* game);
bool SaveSkillConfigs(sqlite3* db, const CGame* game);
bool LoadSkillConfigs(sqlite3* db, CGame* game);
bool SaveQuestConfigs(sqlite3* db, const CGame* game);
bool LoadQuestConfigs(sqlite3* db, CGame* game);
bool SavePortionConfigs(sqlite3* db, const CGame* game);
bool LoadPortionConfigs(sqlite3* db, CGame* game);
bool SaveBuildItemConfigs(sqlite3* db, const CGame* game);
bool LoadBuildItemConfigs(sqlite3* db, CGame* game);
bool SaveCrusadeConfig(sqlite3* db, const CGame* game);
bool LoadCrusadeConfig(sqlite3* db, CGame* game);
bool SaveScheduleConfig(sqlite3* db, const CGame* game);
bool LoadScheduleConfig(sqlite3* db, CGame* game);
bool LoadCreationItems(sqlite3* db, std::vector<creation_item_entry>& out_items);
bool SaveCreationItems(sqlite3* db, const std::vector<creation_item_entry>& items);
bool LoadColorPalette(sqlite3* db, std::vector<color_palette_entry>& out_entries);
bool LoadAttributePrefixTypes(sqlite3* db, std::vector<attribute_prefix_type_entry>& out);
bool LoadAttributeSecondaryTypes(sqlite3* db, std::vector<attribute_secondary_type_entry>& out);
bool HasGameConfigRows(sqlite3* db, const char* tableName);
void CloseGameConfigDatabase(sqlite3* db);

namespace hb::shared { class formula_engine; }
bool LoadFormulas(sqlite3* db, hb::shared::formula_engine& engine);
