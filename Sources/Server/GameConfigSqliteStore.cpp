#include "GameConfigSqliteStore.h"

#include <filesystem>
#include <cstdio>
#include <cstring>
#include <unordered_map>

#include "Item.h"
#include "FormulaEngine.h"
#include "BuildItem.h"
#include "Game.h"
#include "ItemManager.h"
#include "CraftingManager.h"
#include "QuestManager.h"
#include "Magic.h"
#include "Npc.h"
#include "Portion.h"
#include "Quest.h"
#include "Skill.h"
#include "sqlite3.h"
#include "Log.h"
#include "StringCompat.h"
using namespace hb::server::config;

namespace
{
    bool ExecSql(sqlite3* db, const char* sql)
    {
        char* err = nullptr;
        int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
        if (rc != SQLITE_OK) {
            char logMsg[512] = {};
            hb::logger::error("SQLite exec failed: {}", err ? err : "unknown");
            sqlite3_free(err);
            return false;
        }
        return true;
    }

    bool BeginTransaction(sqlite3* db)
    {
        return ExecSql(db, "BEGIN;");
    }

    bool CommitTransaction(sqlite3* db)
    {
        return ExecSql(db, "COMMIT;");
    }

    bool RollbackTransaction(sqlite3* db)
    {
        return ExecSql(db, "ROLLBACK;");
    }

    bool ClearTable(sqlite3* db, const char* tableName)
    {
        char sql[256] = {};
        std::snprintf(sql, sizeof(sql), "DELETE FROM %s;", tableName);
        return ExecSql(db, sql);
    }

    bool PrepareAndBindText(sqlite3_stmt* stmt, int idx, const char* value)
    {
        return sqlite3_bind_text(stmt, idx, value, -1, SQLITE_TRANSIENT) == SQLITE_OK;
    }

    bool HasColumn(sqlite3* db, const char* tableName, const char* columnName)
    {
        if (db == nullptr || tableName == nullptr || columnName == nullptr) {
            return false;
        }
        char sql[256] = {};
        std::snprintf(sql, sizeof(sql), "PRAGMA table_info(%s);", tableName);
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return false;
        }
        bool found = false;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char* name = sqlite3_column_text(stmt, 1);
            if (name != nullptr && std::strcmp(reinterpret_cast<const char*>(name), columnName) == 0) {
                found = true;
                break;
            }
        }
        sqlite3_finalize(stmt);
        return found;
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

    bool InsertKeyValue(sqlite3_stmt* stmt, const char* key, const char* value)
    {
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
        bool ok = true;
        ok &= PrepareAndBindText(stmt, 1, key);
        ok &= PrepareAndBindText(stmt, 2, value);
        if (!ok) {
            return false;
        }
        return sqlite3_step(stmt) == SQLITE_DONE;
    }

    bool InsertKeyValueInt(sqlite3_stmt* stmt, const char* key, int value)
    {
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
        bool ok = true;
        ok &= PrepareAndBindText(stmt, 1, key);
        ok &= (sqlite3_bind_int(stmt, 2, value) == SQLITE_OK);
        if (!ok) {
            return false;
        }
        return sqlite3_step(stmt) == SQLITE_DONE;
    }

    bool InsertKeyValueFloat(sqlite3_stmt* stmt, const char* key, float value)
    {
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
        bool ok = true;
        ok &= PrepareAndBindText(stmt, 1, key);
        ok &= (sqlite3_bind_double(stmt, 2, static_cast<double>(value)) == SQLITE_OK);
        if (!ok) {
            return false;
        }
        return sqlite3_step(stmt) == SQLITE_DONE;
    }
}

bool EnsureGameConfigDatabase(sqlite3** outDb, std::string& outPath, bool* outCreated)
{
    if (outDb == nullptr) {
        return false;
    }

    std::string dbPath = "gamedata.db";
    if (!std::filesystem::exists(dbPath)) {
        auto exeDir = std::filesystem::current_path();
        dbPath = (exeDir / "gamedata.db").string();
    }
    outPath = dbPath;

    bool created = !std::filesystem::exists(dbPath);

    sqlite3* db = nullptr;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        char logMsg[512] = {};
        hb::logger::error("SQLite open failed: {}", sqlite3_errmsg(db));
        sqlite3_close(db);
        return false;
    }

    sqlite3_busy_timeout(db, 1000);
    if (!ExecSql(db, "PRAGMA foreign_keys = ON;")) {
        sqlite3_close(db);
        return false;
    }

    const char* schemaSql =
        "BEGIN;"
        "CREATE TABLE IF NOT EXISTS meta ("
        " key TEXT PRIMARY KEY,"
        " value TEXT NOT NULL"
        ");"
        "INSERT OR REPLACE INTO meta(key, value) VALUES('schema_version','7');"
        "CREATE TABLE IF NOT EXISTS items ("
        " item_id INTEGER PRIMARY KEY,"
        " name TEXT NOT NULL,"
        " item_type INTEGER NOT NULL DEFAULT 0,"
        " item_sub_type INTEGER NOT NULL DEFAULT 0,"
        " equip_pos INTEGER NOT NULL DEFAULT 0,"
        " weapon_class INTEGER NOT NULL DEFAULT 0,"
        " item_effect_type INTEGER NOT NULL DEFAULT 0,"
        " item_effect_value1 INTEGER NOT NULL DEFAULT 0,"
        " item_effect_value2 INTEGER NOT NULL DEFAULT 0,"
        " item_effect_value3 INTEGER NOT NULL DEFAULT 0,"
        " item_effect_value4 INTEGER NOT NULL DEFAULT 0,"
        " item_effect_value5 INTEGER NOT NULL DEFAULT 0,"
        " item_effect_value6 INTEGER NOT NULL DEFAULT 0,"
        " durability INTEGER NOT NULL DEFAULT 0,"
        " special_effect INTEGER NOT NULL DEFAULT 0,"
        " sell_price INTEGER NOT NULL DEFAULT 0,"
        " weight INTEGER NOT NULL DEFAULT 0,"
        " swing_speed INTEGER NOT NULL DEFAULT 0,"
        " level_requirement INTEGER NOT NULL DEFAULT 0,"
        " gender_requirement INTEGER NOT NULL DEFAULT 0,"
        " special_effect_value1 INTEGER NOT NULL DEFAULT 0,"
        " special_effect_value2 INTEGER NOT NULL DEFAULT 0,"
        " related_skill INTEGER NOT NULL DEFAULT 0,"
        " hide_armor INTEGER NOT NULL DEFAULT 0,"
        " is_skirt INTEGER NOT NULL DEFAULT 0,"
        " stackable INTEGER NOT NULL DEFAULT 0,"
        " is_dyeable INTEGER NOT NULL DEFAULT 0,"
        " armor_class INTEGER NOT NULL DEFAULT 0,"
        " set_id INTEGER NOT NULL DEFAULT 0,"
        " item_color INTEGER NOT NULL DEFAULT 0,"
        " display_id INTEGER NOT NULL DEFAULT -1"
        ");"
        "CREATE TABLE IF NOT EXISTS active_maps ("
        " map_index INTEGER PRIMARY KEY,"
        " map_name TEXT NOT NULL,"
        " active INTEGER NOT NULL DEFAULT 0"
        ");"
        "CREATE TABLE IF NOT EXISTS banned_list ("
        " ip_address TEXT PRIMARY KEY"
        ");"
        "CREATE TABLE IF NOT EXISTS admins ("
        " account_name TEXT PRIMARY KEY,"
        " character_name TEXT NOT NULL,"
        " approved_ip TEXT NOT NULL DEFAULT '0.0.0.0',"
        " admin_level INTEGER NOT NULL DEFAULT 1"
        ");"
        "CREATE TABLE IF NOT EXISTS admin_command_permissions ("
        " command TEXT PRIMARY KEY COLLATE NOCASE,"
        " admin_level INTEGER NOT NULL DEFAULT 1000,"
        " description TEXT NOT NULL DEFAULT ''"
        ");"
        "CREATE TABLE IF NOT EXISTS npc_configs ("
        " npc_id INTEGER PRIMARY KEY,"
        " name TEXT NOT NULL,"
        " npc_type INTEGER NOT NULL,"
        " hp_min INTEGER NOT NULL,"
        " hp_max INTEGER NOT NULL,"
        " hold_resist INTEGER NOT NULL DEFAULT 0,"
        " defense_ratio INTEGER NOT NULL,"
        " hit_ratio INTEGER NOT NULL,"
        " min_bravery INTEGER NOT NULL,"
        " exp_min INTEGER NOT NULL,"
        " exp_max INTEGER NOT NULL,"
        " gold_min INTEGER NOT NULL,"
        " gold_max INTEGER NOT NULL,"
        " min_damage INTEGER NOT NULL,"
        " max_damage INTEGER NOT NULL,"
        " npc_size INTEGER NOT NULL,"
        " side INTEGER NOT NULL,"
        " action_limit INTEGER NOT NULL,"
        " action_time INTEGER NOT NULL,"
        " resist_magic INTEGER NOT NULL,"
        " magic_level INTEGER NOT NULL,"
        " day_of_week_limit INTEGER NOT NULL,"
        " chat_msg_presence INTEGER NOT NULL,"
        " target_search_range INTEGER NOT NULL,"
        " regen_time INTEGER NOT NULL,"
        " attribute INTEGER NOT NULL,"
        " abs_damage INTEGER NOT NULL,"
        " max_mana INTEGER NOT NULL,"
        " magic_hit_ratio INTEGER NOT NULL,"
        " attack_range INTEGER NOT NULL,"
        " drop_table_id INTEGER NOT NULL DEFAULT 0"
        ");"
        "CREATE TABLE IF NOT EXISTS drop_tables ("
        " drop_table_id INTEGER PRIMARY KEY,"
        " name TEXT NOT NULL,"
        " description TEXT NOT NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS drop_entries ("
        " drop_table_id INTEGER NOT NULL,"
        " tier INTEGER NOT NULL,"
        " item_id INTEGER NOT NULL,"
        " weight INTEGER NOT NULL,"
        " min_count INTEGER NOT NULL,"
        " max_count INTEGER NOT NULL,"
        " PRIMARY KEY (drop_table_id, tier, item_id)"
        ");"
        "CREATE TABLE IF NOT EXISTS magic_configs ("
        " magic_id INTEGER PRIMARY KEY,"
        " name TEXT NOT NULL,"
        " magic_type INTEGER NOT NULL,"
        " delay_time INTEGER NOT NULL,"
        " last_time INTEGER NOT NULL,"
        " value1 INTEGER NOT NULL,"
        " value2 INTEGER NOT NULL,"
        " value3 INTEGER NOT NULL,"
        " value4 INTEGER NOT NULL,"
        " value5 INTEGER NOT NULL,"
        " value6 INTEGER NOT NULL,"
        " value7 INTEGER NOT NULL,"
        " value8 INTEGER NOT NULL,"
        " value9 INTEGER NOT NULL,"
        " value10 INTEGER NOT NULL,"
        " value11 INTEGER NOT NULL,"
        " value12 INTEGER NOT NULL,"
        " int_limit INTEGER NOT NULL,"
        " gold_cost INTEGER NOT NULL,"
        " category INTEGER NOT NULL,"
        " attribute INTEGER NOT NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS skill_configs ("
        " skill_id INTEGER PRIMARY KEY,"
        " name TEXT NOT NULL,"
        " skill_type INTEGER NOT NULL,"
        " value1 INTEGER NOT NULL,"
        " value2 INTEGER NOT NULL,"
        " value3 INTEGER NOT NULL,"
        " value4 INTEGER NOT NULL,"
        " value5 INTEGER NOT NULL,"
        " value6 INTEGER NOT NULL,"
        " is_useable INTEGER NOT NULL DEFAULT 0,"
        " use_method INTEGER NOT NULL DEFAULT 0"
        ");"
        "CREATE TABLE IF NOT EXISTS quest_configs ("
        " quest_index INTEGER PRIMARY KEY,"
        " side INTEGER NOT NULL,"
        " quest_type INTEGER NOT NULL,"
        " target_config_id INTEGER NOT NULL,"
        " max_count INTEGER NOT NULL,"
        " quest_from INTEGER NOT NULL,"
        " min_level INTEGER NOT NULL,"
        " max_level INTEGER NOT NULL,"
        " required_skill_num INTEGER NOT NULL,"
        " required_skill_level INTEGER NOT NULL,"
        " time_limit INTEGER NOT NULL,"
        " assign_type INTEGER NOT NULL,"
        " reward_type1 INTEGER NOT NULL,"
        " reward_amount1 INTEGER NOT NULL,"
        " reward_type2 INTEGER NOT NULL,"
        " reward_amount2 INTEGER NOT NULL,"
        " reward_type3 INTEGER NOT NULL,"
        " reward_amount3 INTEGER NOT NULL,"
        " contribution INTEGER NOT NULL,"
        " contribution_limit INTEGER NOT NULL,"
        " response_mode INTEGER NOT NULL,"
        " target_name TEXT NOT NULL,"
        " target_x INTEGER NOT NULL,"
        " target_y INTEGER NOT NULL,"
        " target_range INTEGER NOT NULL,"
        " quest_id INTEGER NOT NULL,"
        " req_contribution INTEGER NOT NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS potion_configs ("
        " potion_id INTEGER PRIMARY KEY,"
        " name TEXT NOT NULL,"
        " array0 INTEGER NOT NULL,"
        " array1 INTEGER NOT NULL,"
        " array2 INTEGER NOT NULL,"
        " array3 INTEGER NOT NULL,"
        " array4 INTEGER NOT NULL,"
        " array5 INTEGER NOT NULL,"
        " array6 INTEGER NOT NULL,"
        " array7 INTEGER NOT NULL,"
        " array8 INTEGER NOT NULL,"
        " array9 INTEGER NOT NULL,"
        " array10 INTEGER NOT NULL,"
        " array11 INTEGER NOT NULL,"
        " skill_limit INTEGER NOT NULL,"
        " difficulty INTEGER NOT NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS crafting_configs ("
        " crafting_id INTEGER PRIMARY KEY,"
        " name TEXT NOT NULL,"
        " array0 INTEGER NOT NULL,"
        " array1 INTEGER NOT NULL,"
        " array2 INTEGER NOT NULL,"
        " array3 INTEGER NOT NULL,"
        " array4 INTEGER NOT NULL,"
        " array5 INTEGER NOT NULL,"
        " array6 INTEGER NOT NULL,"
        " array7 INTEGER NOT NULL,"
        " array8 INTEGER NOT NULL,"
        " array9 INTEGER NOT NULL,"
        " array10 INTEGER NOT NULL,"
        " array11 INTEGER NOT NULL,"
        " skill_limit INTEGER NOT NULL,"
        " difficulty INTEGER NOT NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS builditem_configs ("
        " build_id INTEGER PRIMARY KEY,"
        " name TEXT NOT NULL,"
        " skill_limit INTEGER NOT NULL,"
        " material_id1 INTEGER NOT NULL,"
        " material_count1 INTEGER NOT NULL,"
        " material_value1 INTEGER NOT NULL,"
        " material_id2 INTEGER NOT NULL,"
        " material_count2 INTEGER NOT NULL,"
        " material_value2 INTEGER NOT NULL,"
        " material_id3 INTEGER NOT NULL,"
        " material_count3 INTEGER NOT NULL,"
        " material_value3 INTEGER NOT NULL,"
        " material_id4 INTEGER NOT NULL,"
        " material_count4 INTEGER NOT NULL,"
        " material_value4 INTEGER NOT NULL,"
        " material_id5 INTEGER NOT NULL,"
        " material_count5 INTEGER NOT NULL,"
        " material_value5 INTEGER NOT NULL,"
        " material_id6 INTEGER NOT NULL,"
        " material_count6 INTEGER NOT NULL,"
        " material_value6 INTEGER NOT NULL,"
        " average_value INTEGER NOT NULL,"
        " max_skill INTEGER NOT NULL,"
        " attribute INTEGER NOT NULL,"
        " item_id INTEGER NOT NULL"
        ");"
                "CREATE TABLE IF NOT EXISTS crusade_structures ("
        " structure_id INTEGER PRIMARY KEY,"
        " map_name TEXT NOT NULL,"
        " structure_type INTEGER NOT NULL,"
        " pos_x INTEGER NOT NULL,"
        " pos_y INTEGER NOT NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS event_schedule ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " event_type TEXT NOT NULL,"
        " schedule_index INTEGER NOT NULL,"
        " day INTEGER NOT NULL,"
        " start_hour INTEGER NOT NULL,"
        " start_minute INTEGER NOT NULL,"
        " end_hour INTEGER,"
        " end_minute INTEGER,"
        " is_active INTEGER NOT NULL DEFAULT 0,"
        " UNIQUE(event_type, schedule_index)"
        ");"
        "CREATE TABLE IF NOT EXISTS npc_shop_mapping ("
        " npc_config_id INTEGER PRIMARY KEY,"
        " shop_id INTEGER NOT NULL,"
        " description TEXT"
        ");"
        "CREATE TABLE IF NOT EXISTS shop_items ("
        " shop_id INTEGER NOT NULL,"
        " item_id INTEGER NOT NULL,"
        " sort_order INTEGER NOT NULL DEFAULT 0,"
        " PRIMARY KEY (shop_id, item_id)"
        ");"
        "CREATE TABLE IF NOT EXISTS character_creation_items ("
        " class_type INTEGER NOT NULL,"
        " item_id INTEGER NOT NULL,"
        " count INTEGER NOT NULL DEFAULT 1,"
        " item_color INTEGER NOT NULL DEFAULT 0,"
        " durability INTEGER NOT NULL DEFAULT 0,"
        " is_equipped INTEGER NOT NULL DEFAULT 0,"
        " gender_limit INTEGER NOT NULL DEFAULT 0,"
        " sort_order INTEGER NOT NULL DEFAULT 0,"
        " PRIMARY KEY (class_type, item_id, gender_limit)"
        ");"
        "CREATE TABLE IF NOT EXISTS color_palette ("
        " color_id INTEGER PRIMARY KEY,"
        " r INTEGER NOT NULL,"
        " g INTEGER NOT NULL,"
        " b INTEGER NOT NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS attribute_prefix_types ("
        " prefix_id INTEGER PRIMARY KEY,"
        " name TEXT NOT NULL,"
        " display_name TEXT NOT NULL,"
        " effect_label TEXT NOT NULL,"
        " effect_format TEXT NOT NULL,"
        " min_value INTEGER NOT NULL DEFAULT 0,"
        " max_value INTEGER NOT NULL DEFAULT 0,"
        " weapon_color INTEGER NOT NULL DEFAULT 0,"
        " multiplier INTEGER NOT NULL DEFAULT 1"
        ");"
        "CREATE TABLE IF NOT EXISTS attribute_secondary_types ("
        " secondary_id INTEGER PRIMARY KEY,"
        " name TEXT NOT NULL,"
        " effect_label TEXT NOT NULL,"
        " effect_format TEXT NOT NULL,"
        " min_value INTEGER NOT NULL DEFAULT 0,"
        " max_value INTEGER NOT NULL DEFAULT 0,"
        " multiplier INTEGER NOT NULL DEFAULT 1"
        ");"
        "COMMIT;";

    if (!ExecSql(db, schemaSql)) {
        sqlite3_close(db);
        return false;
    }

    // Migrate old items schema (has appr_value) → new schema (has weapon_class etc.)
    if (HasColumn(db, "items", "appr_value")) {
        hb::logger::log("Migrating items table from old schema to new item type system...");

        const char* migrationSql =
            "BEGIN;"

            // Create new table with the new schema
            "CREATE TABLE items_new ("
            " item_id INTEGER PRIMARY KEY,"
            " name TEXT NOT NULL,"
            " item_type INTEGER NOT NULL DEFAULT 0,"
            " item_sub_type INTEGER NOT NULL DEFAULT 0,"
            " equip_pos INTEGER NOT NULL DEFAULT 0,"
            " weapon_class INTEGER NOT NULL DEFAULT 0,"
            " item_effect_type INTEGER NOT NULL DEFAULT 0,"
            " item_effect_value1 INTEGER NOT NULL DEFAULT 0,"
            " item_effect_value2 INTEGER NOT NULL DEFAULT 0,"
            " item_effect_value3 INTEGER NOT NULL DEFAULT 0,"
            " item_effect_value4 INTEGER NOT NULL DEFAULT 0,"
            " item_effect_value5 INTEGER NOT NULL DEFAULT 0,"
            " item_effect_value6 INTEGER NOT NULL DEFAULT 0,"
            " durability INTEGER NOT NULL DEFAULT 0,"
            " special_effect INTEGER NOT NULL DEFAULT 0,"
            " sell_price INTEGER NOT NULL DEFAULT 0,"
            " weight INTEGER NOT NULL DEFAULT 0,"
            " swing_speed INTEGER NOT NULL DEFAULT 0,"
            " level_requirement INTEGER NOT NULL DEFAULT 0,"
            " gender_requirement INTEGER NOT NULL DEFAULT 0,"
            " special_effect_value1 INTEGER NOT NULL DEFAULT 0,"
            " special_effect_value2 INTEGER NOT NULL DEFAULT 0,"
            " related_skill INTEGER NOT NULL DEFAULT 0,"
            " hide_armor INTEGER NOT NULL DEFAULT 0,"
            " is_skirt INTEGER NOT NULL DEFAULT 0,"
            " stackable INTEGER NOT NULL DEFAULT 0,"
            " is_dyeable INTEGER NOT NULL DEFAULT 0,"
            " armor_class INTEGER NOT NULL DEFAULT 0,"
            " set_id INTEGER NOT NULL DEFAULT 0,"
            " item_color INTEGER NOT NULL DEFAULT 0,"
            " display_id INTEGER NOT NULL DEFAULT -1"
            ");"

            // Copy data with transformations
            // item_type mapping: 0→varies, 1→2(equip), 3→1(consumable), 5→3(material),
            //   6→1(consumable/ammo), 7→1(consumable), 8→5(tool), 9→5(tool),
            //   10→5(tool), 11→1(consumable/target), 12→3(material/crafted)
            // sell_price: ABS(price) if price >= 0, else ABS(price) (old negative=not-for-sale, new: 0=unsellable handled by migration script)
            // weapon_class: derived from appr_value ranges for equip type
            // hide_armor: appr_value >= 100 on body armor
            // is_skirt: appr_value == 1 on pants
            // stackable: old types 5(Consume), 6(Arrow), 12(Material)
            // is_dyeable: old category in (1, 3, 6, 8, 11, 12, 13, 15)
            "INSERT INTO items_new("
            " item_id, name, item_type, item_sub_type, equip_pos, weapon_class,"
            " item_effect_type, item_effect_value1, item_effect_value2, item_effect_value3,"
            " item_effect_value4, item_effect_value5, item_effect_value6,"
            " durability, special_effect, sell_price, weight, swing_speed,"
            " level_requirement, gender_requirement,"
            " special_effect_value1, special_effect_value2, related_skill,"
            " hide_armor, is_skirt, stackable, is_dyeable, armor_class, set_id,"
            " item_color, display_id)"
            " SELECT"
            "  item_id, name,"
            // item_type: map old→new
            "  CASE item_type"
            "   WHEN 1 THEN 2"   // Equip → equipment
            "   WHEN 3 THEN 1"   // UseDeplete → consumable
            "   WHEN 5 THEN 3"   // Consume → material
            "   WHEN 6 THEN 1"   // Arrow → consumable
            "   WHEN 7 THEN 1"   // Eat → consumable
            "   WHEN 8 THEN 5"   // UseSkill → tool
            "   WHEN 9 THEN 5"   // UsePerm → tool
            "   WHEN 10 THEN 5"  // UseSkillEnableDialogBox → tool
            "   WHEN 11 THEN 1"  // UseDepleteDest → consumable
            "   WHEN 12 THEN 3"  // Material → material
            "   ELSE 0"          // None/Apply/Install → none (will need manual review)
            "  END,"
            // item_sub_type: derive from old type + equip_pos
            "  CASE"
            "   WHEN item_type = 6 THEN 1"   // Arrow → ammo
            "   WHEN item_type = 11 THEN 2"  // UseDepleteDest → target
            "   WHEN item_type = 1 AND (equip_pos = 7 OR equip_pos = 8 OR equip_pos = 9) THEN 3"  // Equip weapon → weapon
            "   WHEN item_type = 1 AND (equip_pos IN (1,2,3,4,5,12,13)) THEN 4"  // Equip armor → armor
            "   WHEN item_type = 1 AND (equip_pos IN (6,10,11)) THEN 5"  // Equip accessory → accessory
            "   WHEN item_type = 12 THEN 8"  // Material → crafted
            "   WHEN item_type = 8 THEN 10"  // UseSkill → fishing
            "   WHEN item_type = 10 THEN 11" // UseSkillEnableDialogBox → crafting
            "   WHEN item_type = 9 THEN 12"  // UsePerm → map
            "   ELSE 0"
            "  END,"
            "  equip_pos,"
            // weapon_class: derive from appr_value for weapons
            "  CASE"
            "   WHEN item_type != 1 OR equip_pos NOT IN (7,8,9) THEN 0"  // Not a weapon
            "   WHEN appr_value >= 1 AND appr_value <= 1 THEN 1"   // dagger
            "   WHEN appr_value = 2 THEN 2"                        // short_sword
            "   WHEN appr_value IN (7, 18) THEN 4"                 // fencing (Esterk, KlonessEsterk)
            "   WHEN appr_value >= 3 AND appr_value <= 19 THEN 3"  // long_sword
            "   WHEN appr_value = 29 THEN 3"                       // long_sword variant (LightingBlade)
            "   WHEN appr_value >= 20 AND appr_value <= 28 THEN 5" // axe
            "   WHEN appr_value = 33 THEN 3"                       // long_sword variant (BlackShadow)
            "   WHEN appr_value >= 30 AND appr_value <= 32 THEN 6" // hammer
            "   WHEN appr_value >= 34 AND appr_value <= 39 THEN 7" // wand
            "   WHEN appr_value >= 40 THEN 8"                      // bow
            "   ELSE 0"
            "  END,"
            "  item_effect_type, item_effect_value1, item_effect_value2, item_effect_value3,"
            "  item_effect_value4, item_effect_value5, item_effect_value6,"
            "  max_durability, special_effect,"
            // sell_price: only sellable items get a price (0 = cannot sell)
            "  CASE WHEN is_for_sale = 1 THEN price ELSE 0 END,"
            "  weight, speed, level_limit, gender_limit,"
            "  special_effect_value1, special_effect_value2, related_skill,"
            // hide_armor: body armor with appr_value >= 100
            "  CASE WHEN (equip_pos = 2 OR equip_pos = 13) AND appr_value >= 100 THEN 1 ELSE 0 END,"
            // is_skirt: pants with appr_value == 1
            "  CASE WHEN equip_pos = 4 AND appr_value = 1 THEN 1 ELSE 0 END,"
            // stackable: old types Consume(5), Arrow(6), Material(12)
            "  CASE WHEN item_type IN (5, 6, 12) THEN 1 ELSE 0 END,"
            // is_dyeable: old category in (1, 3, 6, 8, 11, 12, 13, 15)
            "  CASE WHEN category IN (1, 3, 6, 8, 11, 12, 13, 15) THEN 1 ELSE 0 END,"
            // armor_class: default 0 (user populates via database after migration)
            "  0,"
            // set_id: default 0 (hero sets assigned by Python migration script)
            "  0,"
            "  item_color,"
            "  COALESCE(display_id, -1)"
            " FROM items;"

            // Drop old table and rename new
            "DROP TABLE items;"
            "ALTER TABLE items_new RENAME TO items;"
            "COMMIT;";

        if (!ExecSql(db, migrationSql)) {
            hb::logger::error("Failed to migrate items table! Database may be corrupted.");
            sqlite3_close(db);
            return false;
        }
        hb::logger::log("Items table migration complete.");
    }

    if (!HasColumn(db, "npc_configs", "drop_table_id")) {
        ExecSql(db, "ALTER TABLE npc_configs ADD COLUMN drop_table_id INTEGER NOT NULL DEFAULT 0;");
    }

    if (!HasColumn(db, "admins", "admin_level")) {
        ExecSql(db, "ALTER TABLE admins ADD COLUMN admin_level INTEGER NOT NULL DEFAULT 1;");
    }

    *outDb = db;
    if (outCreated != nullptr) {
        *outCreated = created;
    }
    return true;
}

bool SaveItemConfigs(sqlite3* db, CItem* const* itemList, int maxItems)
{
    if (db == nullptr || itemList == nullptr || maxItems <= 0) {
        return false;
    }

    if (!ExecSql(db, "BEGIN;")) {
        return false;
    }
    if (!ExecSql(db, "DELETE FROM items;")) {
        ExecSql(db, "ROLLBACK;");
        return false;
    }

    const char* sql =
        "INSERT INTO items("
        " item_id, name, item_type, item_sub_type, equip_pos, weapon_class,"
        " item_effect_type, item_effect_value1, item_effect_value2, item_effect_value3,"
        " item_effect_value4, item_effect_value5, item_effect_value6,"
        " durability, special_effect, sell_price, weight, swing_speed,"
        " level_requirement, gender_requirement,"
        " special_effect_value1, special_effect_value2, related_skill,"
        " hide_armor, is_skirt, stackable, is_dyeable, armor_class, set_id,"
        " item_color, display_id"
        ") VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        ExecSql(db, "ROLLBACK;");
        return false;
    }

    for(int i = 0; i < maxItems; i++) {
        if (itemList[i] == nullptr) {
            continue;
        }

        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
        int col = 1;
        bool ok = true;
        ok &= (sqlite3_bind_int(stmt, col++, itemList[i]->m_id_num) == SQLITE_OK);
        ok &= PrepareAndBindText(stmt, col++, itemList[i]->m_name);
        ok &= (sqlite3_bind_int(stmt, col++, itemList[i]->m_item_type) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, itemList[i]->m_item_sub_type) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, itemList[i]->m_equip_pos) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, itemList[i]->m_weapon_class) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, itemList[i]->m_item_effect_type) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, itemList[i]->m_item_effect_value1) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, itemList[i]->m_item_effect_value2) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, itemList[i]->m_item_effect_value3) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, itemList[i]->m_item_effect_value4) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, itemList[i]->m_item_effect_value5) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, itemList[i]->m_item_effect_value6) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, itemList[i]->m_durability) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, itemList[i]->m_special_effect) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, static_cast<int>(itemList[i]->m_sell_price)) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, itemList[i]->m_weight) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, itemList[i]->m_swing_speed) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, itemList[i]->m_level_requirement) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, itemList[i]->m_gender_requirement) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, itemList[i]->m_special_effect_value1) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, itemList[i]->m_special_effect_value2) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, itemList[i]->m_related_skill) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, itemList[i]->m_hide_armor) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, itemList[i]->m_is_skirt) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, itemList[i]->m_stackable) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, itemList[i]->m_is_dyeable) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, itemList[i]->m_armor_class) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, itemList[i]->m_set_id) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, itemList[i]->m_instance.item_color) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, itemList[i]->m_display_id) == SQLITE_OK);

        if (!ok || sqlite3_step(stmt) != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            ExecSql(db, "ROLLBACK;");
            return false;
        }
    }

    sqlite3_finalize(stmt);
    if (!ExecSql(db, "COMMIT;")) {
        ExecSql(db, "ROLLBACK;");
        return false;
    }
    return true;
}

bool LoadItemConfigs(sqlite3* db, CItem** itemList, int maxItems)
{
    if (db == nullptr || itemList == nullptr || maxItems <= 0) {
        return false;
    }

    const char* sql =
        "SELECT item_id, name, item_type, item_sub_type, equip_pos, weapon_class,"
        " item_effect_type, item_effect_value1, item_effect_value2, item_effect_value3,"
        " item_effect_value4, item_effect_value5, item_effect_value6,"
        " durability, special_effect, sell_price, weight, swing_speed,"
        " level_requirement, gender_requirement,"
        " special_effect_value1, special_effect_value2, related_skill,"
        " hide_armor, is_skirt, stackable, is_dyeable, armor_class, set_id,"
        " item_color, display_id"
        " FROM items ORDER BY item_id;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = 0;
        int item_id = sqlite3_column_int(stmt, col++);
        if (item_id < 0 || item_id >= maxItems) {
            continue;
        }

        if (itemList[item_id] != nullptr) {
            delete itemList[item_id];
            itemList[item_id] = nullptr;
        }

        CItem* item = new CItem();
        item->m_id_num = (short)item_id;
        CopyColumnText(stmt, col++, item->m_name, sizeof(item->m_name));
        item->m_item_type = (char)sqlite3_column_int(stmt, col++);
        item->m_item_sub_type = (char)sqlite3_column_int(stmt, col++);
        item->m_equip_pos = (char)sqlite3_column_int(stmt, col++);
        item->m_weapon_class = (char)sqlite3_column_int(stmt, col++);
        item->m_item_effect_type = (short)sqlite3_column_int(stmt, col++);
        item->m_item_effect_value1 = (short)sqlite3_column_int(stmt, col++);
        item->m_item_effect_value2 = (short)sqlite3_column_int(stmt, col++);
        item->m_item_effect_value3 = (short)sqlite3_column_int(stmt, col++);
        item->m_item_effect_value4 = (short)sqlite3_column_int(stmt, col++);
        item->m_item_effect_value5 = (short)sqlite3_column_int(stmt, col++);
        item->m_item_effect_value6 = (short)sqlite3_column_int(stmt, col++);
        item->m_durability = (uint16_t)sqlite3_column_int(stmt, col++);
        item->m_special_effect = (short)sqlite3_column_int(stmt, col++);
        item->m_sell_price = (uint32_t)sqlite3_column_int(stmt, col++);
        item->m_weight = (uint16_t)sqlite3_column_int(stmt, col++);
        item->m_swing_speed = (char)sqlite3_column_int(stmt, col++);
        item->m_level_requirement = (short)sqlite3_column_int(stmt, col++);
        item->m_gender_requirement = (char)sqlite3_column_int(stmt, col++);
        item->m_special_effect_value1 = (short)sqlite3_column_int(stmt, col++);
        item->m_special_effect_value2 = (short)sqlite3_column_int(stmt, col++);
        item->m_related_skill = (short)sqlite3_column_int(stmt, col++);
        item->m_hide_armor = (char)sqlite3_column_int(stmt, col++);
        item->m_is_skirt = (char)sqlite3_column_int(stmt, col++);
        item->m_stackable = (char)sqlite3_column_int(stmt, col++);
        item->m_is_dyeable = (char)sqlite3_column_int(stmt, col++);
        item->m_armor_class = (char)sqlite3_column_int(stmt, col++);
        item->m_set_id = (int16_t)sqlite3_column_int(stmt, col++);
        item->m_instance.item_color = (char)sqlite3_column_int(stmt, col++);
        item->m_display_id = (short)sqlite3_column_int(stmt, col++);

        itemList[item_id] = item;
    }

    sqlite3_finalize(stmt);
    return true;
}

bool LoadActiveMaps(sqlite3* db, CGame* game)
{
    if (db == nullptr || game == nullptr) {
        return false;
    }

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, "SELECT map_name FROM active_maps WHERE active = 1 ORDER BY map_index;", -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    // Count total maps in database
    int totalMaps = 0;
    sqlite3_stmt* countStmt = nullptr;
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM active_maps;", -1, &countStmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(countStmt) == SQLITE_ROW)
            totalMaps = sqlite3_column_int(countStmt, 0);
        sqlite3_finalize(countStmt);
    }

    int mapsLoaded = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* nameText = sqlite3_column_text(stmt, 0);
        if (nameText == nullptr) {
            continue;
        }
        const char* name = reinterpret_cast<const char*>(nameText);
        if (!game->register_map(const_cast<char*>(name))) {
            hb::logger::error("Map load failed: {}", name);
            sqlite3_finalize(stmt);
            return false;
        }
        mapsLoaded++;
    }

    sqlite3_finalize(stmt);
    hb::logger::log("- {}/{} maps active", mapsLoaded, totalMaps);
    return true;
}

bool SaveBannedListConfig(sqlite3* db, const CGame* game)
{
    if (db == nullptr || game == nullptr) {
        return false;
    }

    if (!BeginTransaction(db)) {
        return false;
    }

    if (!ClearTable(db, "banned_list")) {
        RollbackTransaction(db);
        return false;
    }

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, "INSERT INTO banned_list(ip_address) VALUES(?);", -1, &stmt, nullptr) != SQLITE_OK) {
        RollbackTransaction(db);
        return false;
    }

    for(int i = 0; i < MaxBanned; i++) {
        if (game->m_banned_list[i].banned_ip_address[0] == 0) {
            continue;
        }
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
        if (!PrepareAndBindText(stmt, 1, game->m_banned_list[i].banned_ip_address) ||
            sqlite3_step(stmt) != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            RollbackTransaction(db);
            return false;
        }
    }

    sqlite3_finalize(stmt);
    if (!CommitTransaction(db)) {
        RollbackTransaction(db);
        return false;
    }
    return true;
}

bool LoadBannedListConfig(sqlite3* db, CGame* game)
{
    if (db == nullptr || game == nullptr) {
        return false;
    }

    for(int i = 0; i < MaxBanned; i++) {
        std::memset(game->m_banned_list[i].banned_ip_address, 0, sizeof(game->m_banned_list[i].banned_ip_address));
    }

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, "SELECT ip_address FROM banned_list ORDER BY ip_address;", -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    int index = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (index >= MaxBanned) {
            break;
        }
        CopyColumnText(stmt, 0, game->m_banned_list[index].banned_ip_address, sizeof(game->m_banned_list[index].banned_ip_address));
        index++;
    }

    sqlite3_finalize(stmt);
    return true;
}

bool SaveAdminConfig(sqlite3* db, const CGame* game)
{
    if (db == nullptr || game == nullptr) {
        return false;
    }

    if (!BeginTransaction(db)) {
        return false;
    }

    if (!ClearTable(db, "admins")) {
        RollbackTransaction(db);
        return false;
    }

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, "INSERT INTO admins(account_name, character_name, approved_ip, admin_level) VALUES(?,?,?,?);", -1, &stmt, nullptr) != SQLITE_OK) {
        RollbackTransaction(db);
        return false;
    }

    for (int i = 0; i < game->m_admin_count; i++) {
        if (game->m_admin_list[i].m_account_name[0] == 0) {
            continue;
        }
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
        if (!PrepareAndBindText(stmt, 1, game->m_admin_list[i].m_account_name) ||
            !PrepareAndBindText(stmt, 2, game->m_admin_list[i].m_char_name) ||
            !PrepareAndBindText(stmt, 3, game->m_admin_list[i].approved_ip) ||
            sqlite3_bind_int(stmt, 4, game->m_admin_list[i].m_admin_level) != SQLITE_OK ||
            sqlite3_step(stmt) != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            RollbackTransaction(db);
            return false;
        }
    }

    sqlite3_finalize(stmt);
    if (!CommitTransaction(db)) {
        RollbackTransaction(db);
        return false;
    }
    return true;
}

bool LoadAdminConfig(sqlite3* db, CGame* game)
{
    if (db == nullptr || game == nullptr) {
        return false;
    }

    for (int i = 0; i < MaxAdmins; i++) {
        std::memset(&game->m_admin_list[i], 0, sizeof(AdminEntry));
    }
    game->m_admin_count = 0;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, "SELECT account_name, character_name, approved_ip, admin_level FROM admins ORDER BY account_name;", -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    int index = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (index >= MaxAdmins) {
            break;
        }
        CopyColumnText(stmt, 0, game->m_admin_list[index].m_account_name, sizeof(game->m_admin_list[index].m_account_name));
        CopyColumnText(stmt, 1, game->m_admin_list[index].m_char_name, sizeof(game->m_admin_list[index].m_char_name));
        CopyColumnText(stmt, 2, game->m_admin_list[index].approved_ip, sizeof(game->m_admin_list[index].approved_ip));
        game->m_admin_list[index].m_admin_level = sqlite3_column_int(stmt, 3);
        if (game->m_admin_list[index].m_admin_level < 1)
            game->m_admin_list[index].m_admin_level = 1;
        index++;
    }

    sqlite3_finalize(stmt);
    game->m_admin_count = index;
    return true;
}

bool SaveCommandPermissions(sqlite3* db, const CGame* game)
{
    if (db == nullptr || game == nullptr) {
        return false;
    }

    if (!BeginTransaction(db)) {
        return false;
    }

    if (!ClearTable(db, "admin_command_permissions")) {
        RollbackTransaction(db);
        return false;
    }

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, "INSERT INTO admin_command_permissions(command, admin_level, description) VALUES(?,?,?);", -1, &stmt, nullptr) != SQLITE_OK) {
        RollbackTransaction(db);
        return false;
    }

    for (const auto& pair : game->m_command_permissions) {
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
        sqlite3_bind_text(stmt, 1, pair.first.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, pair.second.admin_level);
        sqlite3_bind_text(stmt, 3, pair.second.description.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            RollbackTransaction(db);
            return false;
        }
    }

    sqlite3_finalize(stmt);
    if (!CommitTransaction(db)) {
        RollbackTransaction(db);
        return false;
    }
    return true;
}

bool LoadCommandPermissions(sqlite3* db, CGame* game)
{
    if (db == nullptr || game == nullptr) {
        return false;
    }

    game->m_command_permissions.clear();

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, "SELECT command, admin_level, description FROM admin_command_permissions;", -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        int level = sqlite3_column_int(stmt, 1);
        const char* desc = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        if (name != nullptr) {
            CommandPermission perm;
            perm.admin_level = level;
            perm.description = (desc != nullptr) ? desc : "";
            game->m_command_permissions[name] = perm;
        }
    }

    sqlite3_finalize(stmt);
    return true;
}

bool SaveNpcConfigs(sqlite3* db, const CGame* game)
{
    if (db == nullptr || game == nullptr) {
        return false;
    }

    if (!BeginTransaction(db)) {
        return false;
    }

    if (!ClearTable(db, "npc_configs")) {
        RollbackTransaction(db);
        return false;
    }

    const char* sql =
        "INSERT INTO npc_configs("
        " npc_id, name, npc_type, hp_min, hp_max, hold_resist, defense_ratio, hit_ratio, min_bravery,"
        " exp_min, exp_max, gold_min, gold_max, min_damage, max_damage,"
        " npc_size, side, action_limit, action_time, resist_magic, magic_level,"
        " day_of_week_limit, chat_msg_presence, target_search_range, regen_time,"
        " attribute, abs_damage, max_mana, magic_hit_ratio, attack_range, drop_table_id"
        ") VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        RollbackTransaction(db);
        return false;
    }

    for(int i = 0; i < MaxNpcTypes; i++) {
        if (game->m_npc_config_list[i] == nullptr) {
            continue;
        }

        const CNpc* npc = game->m_npc_config_list[i];
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
        int col = 1;
        bool ok = true;
        ok &= (sqlite3_bind_int(stmt, col++, i) == SQLITE_OK);
        ok &= PrepareAndBindText(stmt, col++, npc->m_npc_name);
        ok &= (sqlite3_bind_int(stmt, col++, npc->m_type) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, npc->m_hp_min) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, npc->m_hp_max) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, npc->m_hold_resist) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, npc->m_defense_ratio) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, npc->m_hit_ratio) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, npc->m_min_bravery) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, static_cast<int>(npc->m_exp_dice_min)) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, static_cast<int>(npc->m_exp_dice_max)) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, static_cast<int>(npc->m_gold_dice_min)) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, static_cast<int>(npc->m_gold_dice_max)) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, npc->m_min_damage) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, npc->m_max_damage) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, npc->m_size) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, npc->m_side) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, npc->m_action_limit) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, static_cast<int>(npc->m_action_time)) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, npc->m_resist_magic) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, npc->m_magic_level) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, npc->m_day_of_week_limit) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, npc->m_chat_msg_presence) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, npc->m_target_search_range) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, static_cast<int>(npc->m_regen_time)) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, npc->m_attribute) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, npc->m_abs_damage) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, npc->m_max_mana) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, npc->m_magic_hit_ratio) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, npc->m_attack_range) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, npc->m_drop_table_id) == SQLITE_OK);

        if (!ok || sqlite3_step(stmt) != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            RollbackTransaction(db);
            return false;
        }
    }

    sqlite3_finalize(stmt);
    if (!CommitTransaction(db)) {
        RollbackTransaction(db);
        return false;
    }
    return true;
}

bool LoadNpcConfigs(sqlite3* db, CGame* game)
{
    if (db == nullptr || game == nullptr) {
        return false;
    }

    for(int i = 0; i < MaxNpcTypes; i++) {
        delete game->m_npc_config_list[i];
        game->m_npc_config_list[i] = nullptr;
    }

    const char* sql =
        "SELECT npc_id, name, npc_type, hp_min, hp_max, hold_resist, defense_ratio, hit_ratio, min_bravery,"
        " exp_min, exp_max, gold_min, gold_max, min_damage, max_damage,"
        " npc_size, side, action_limit, action_time, resist_magic, magic_level,"
        " day_of_week_limit, chat_msg_presence, target_search_range, regen_time,"
        " attribute, abs_damage, max_mana, magic_hit_ratio, attack_range, drop_table_id"
        " FROM npc_configs ORDER BY npc_id;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = 0;
        int npcId = sqlite3_column_int(stmt, col++);
        if (npcId < 0 || npcId >= MaxNpcTypes) {
            continue;
        }

        CNpc* npc = new CNpc(" ");
        std::memset(npc->m_npc_name, 0, sizeof(npc->m_npc_name));
        CopyColumnText(stmt, col++, npc->m_npc_name, sizeof(npc->m_npc_name));
        npc->m_type = (short)sqlite3_column_int(stmt, col++);
        npc->m_hp_min = sqlite3_column_int(stmt, col++);
        npc->m_hp_max = sqlite3_column_int(stmt, col++);
        npc->m_hold_resist = sqlite3_column_int(stmt, col++);
        npc->m_defense_ratio = sqlite3_column_int(stmt, col++);
        npc->m_hit_ratio = sqlite3_column_int(stmt, col++);
        npc->m_min_bravery = sqlite3_column_int(stmt, col++);
        npc->m_exp_dice_min = sqlite3_column_int(stmt, col++);
        npc->m_exp_dice_max = sqlite3_column_int(stmt, col++);
        npc->m_gold_dice_min = sqlite3_column_int(stmt, col++);
        npc->m_gold_dice_max = sqlite3_column_int(stmt, col++);
        npc->m_min_damage = sqlite3_column_int(stmt, col++);
        npc->m_max_damage = sqlite3_column_int(stmt, col++);
        npc->m_size = (char)sqlite3_column_int(stmt, col++);
        npc->m_side = (char)sqlite3_column_int(stmt, col++);
        npc->m_action_limit = (char)sqlite3_column_int(stmt, col++);
        npc->m_action_time = (uint32_t)sqlite3_column_int(stmt, col++);
        npc->m_resist_magic = (char)sqlite3_column_int(stmt, col++);
        npc->m_magic_level = (char)sqlite3_column_int(stmt, col++);
        npc->m_day_of_week_limit = (char)sqlite3_column_int(stmt, col++);
        npc->m_chat_msg_presence = (char)sqlite3_column_int(stmt, col++);
        npc->m_target_search_range = (char)sqlite3_column_int(stmt, col++);
        npc->m_regen_time = (uint32_t)sqlite3_column_int(stmt, col++);
        npc->m_attribute = (char)sqlite3_column_int(stmt, col++);
        npc->m_abs_damage = sqlite3_column_int(stmt, col++);
        npc->m_max_mana = sqlite3_column_int(stmt, col++);
        npc->m_magic_hit_ratio = sqlite3_column_int(stmt, col++);
        npc->m_attack_range = sqlite3_column_int(stmt, col++);
        npc->m_drop_table_id = sqlite3_column_int(stmt, col++);

        game->m_npc_config_list[npcId] = npc;
    }

    sqlite3_finalize(stmt);
    return true;
}

bool LoadDropTables(sqlite3* db, CGame* game)
{
    if (db == nullptr || game == nullptr) {
        return false;
    }

    game->m_drop_tables.clear();

    const char* tableSql = "SELECT drop_table_id, name, description FROM drop_tables ORDER BY drop_table_id;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, tableSql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        DropTable table = {};
        table.id = sqlite3_column_int(stmt, 0);
        std::memset(table.name, 0, sizeof(table.name));
        std::memset(table.description, 0, sizeof(table.description));
        CopyColumnText(stmt, 1, table.name, sizeof(table.name));
        CopyColumnText(stmt, 2, table.description, sizeof(table.description));
        std::memset(table.total_weight, 0, sizeof(table.total_weight));
        game->m_drop_tables[table.id] = table;
    }
    sqlite3_finalize(stmt);

    const char* entrySql =
        "SELECT drop_table_id, tier, item_id, weight, min_count, max_count"
        " FROM drop_entries ORDER BY drop_table_id, tier;";
    if (sqlite3_prepare_v2(db, entrySql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int tableId = sqlite3_column_int(stmt, 0);
        int tier = sqlite3_column_int(stmt, 1);
        int item_id = sqlite3_column_int(stmt, 2);
        int weight = sqlite3_column_int(stmt, 3);
        int min_count = sqlite3_column_int(stmt, 4);
        int max_count = sqlite3_column_int(stmt, 5);

        auto it = game->m_drop_tables.find(tableId);
        if (it == game->m_drop_tables.end()) {
            continue;
        }
        if (tier < 1 || tier > 2) {
            continue;
        }
        if (weight <= 0) {
            continue;
        }

        DropEntry entry = {};
        entry.item_id = item_id;
        entry.weight = weight;
        entry.min_count = min_count;
        entry.max_count = max_count;
        it->second.tierEntries[tier].push_back(entry);
        it->second.total_weight[tier] += weight;
    }
    sqlite3_finalize(stmt);

    return !game->m_drop_tables.empty();
}

bool LoadShopConfigs(sqlite3* db, CGame* game)
{
    if (db == nullptr || game == nullptr) {
        return false;
    }

    game->m_npc_shop_mappings.clear();
    game->m_shop_data.clear();
    game->m_is_shop_data_available = false;

    hb::logger::log("Loading shop configs from gamedata.db:");

    // Load NPC -> shop mappings
    const char* mappingSql = "SELECT npc_config_id, shop_id, description FROM npc_shop_mapping ORDER BY npc_config_id;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, mappingSql, -1, &stmt, nullptr) != SQLITE_OK) {
        hb::logger::warn("- Failed to prepare npc_shop_mapping query");
        return false;
    }

    int mappingCount = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int npc_config_id = sqlite3_column_int(stmt, 0);
        int shop_id = sqlite3_column_int(stmt, 1);
        game->m_npc_shop_mappings[npc_config_id] = shop_id;
        mappingCount++;
    }
    sqlite3_finalize(stmt);

    // Load shop items
    const char* itemsSql = "SELECT shop_id, item_id FROM shop_items ORDER BY shop_id, sort_order, item_id;";
    if (sqlite3_prepare_v2(db, itemsSql, -1, &stmt, nullptr) != SQLITE_OK) {
        hb::logger::warn("- Failed to prepare shop_items query");
        return false;
    }

    int itemCount = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int shop_id = sqlite3_column_int(stmt, 0);
        int16_t item_id = static_cast<int16_t>(sqlite3_column_int(stmt, 1));

        // Create shop if it doesn't exist
        if (game->m_shop_data.find(shop_id) == game->m_shop_data.end()) {
            ShopData shop = {};
            shop.shop_id = shop_id;
            game->m_shop_data[shop_id] = shop;
        }

        game->m_shop_data[shop_id].item_ids.push_back(item_id);
        itemCount++;
    }
    sqlite3_finalize(stmt);

    if (mappingCount > 0 || itemCount > 0) {
        game->m_is_shop_data_available = true;
        for (const auto& [npc_id, shop_id] : game->m_npc_shop_mappings) {
            const char* npc_name = "Unknown";
            if (npc_id >= 0 && npc_id < MaxNpcTypes && game->m_npc_config_list[npc_id] != nullptr)
                npc_name = game->m_npc_config_list[npc_id]->m_npc_name;
            auto sit = game->m_shop_data.find(shop_id);
            int count = (sit != game->m_shop_data.end()) ? static_cast<int>(sit->second.item_ids.size()) : 0;
            hb::logger::log("- {}: {} items", npc_name, count);
        }
        hb::logger::log("- Total shop items loaded: {}", itemCount);
    }

    return true;
}

bool LoadSummonThresholds(sqlite3* db, CGame* game)
{
    if (db == nullptr || game == nullptr)
        return false;

    game->m_summon_thresholds.clear();

    const char* sql = "SELECT min_mastery, npc_id, weight FROM summon_thresholds ORDER BY min_mastery, npc_id;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        summon_threshold_entry entry;
        entry.min_mastery = sqlite3_column_int(stmt, 0);
        entry.npc_id = sqlite3_column_int(stmt, 1);
        entry.weight = sqlite3_column_int(stmt, 2);
        game->m_summon_thresholds.push_back(entry);
    }

    sqlite3_finalize(stmt);
    return !game->m_summon_thresholds.empty();
}

bool LoadCreationItems(sqlite3* db, std::vector<creation_item_entry>& out_items)
{
    if (db == nullptr) {
        return false;
    }

    out_items.clear();

    const char* sql =
        "SELECT class_type, item_id, count, item_color, durability,"
        " is_equipped, gender_limit, sort_order"
        " FROM character_creation_items"
        " ORDER BY sort_order, class_type, item_id;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        hb::logger::warn("Failed to prepare character_creation_items query");
        return false;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        creation_item_entry entry = {};
        int col = 0;
        entry.class_type    = sqlite3_column_int(stmt, col++);
        entry.item_id       = sqlite3_column_int(stmt, col++);
        entry.count         = sqlite3_column_int(stmt, col++);
        entry.item_color    = sqlite3_column_int(stmt, col++);
        entry.durability      = sqlite3_column_int(stmt, col++);
        entry.is_equipped   = sqlite3_column_int(stmt, col++);
        entry.gender_limit  = sqlite3_column_int(stmt, col++);
        entry.sort_order    = sqlite3_column_int(stmt, col++);
        out_items.push_back(entry);
    }

    sqlite3_finalize(stmt);
    hb::logger::log("- {} character creation items loaded", (int)out_items.size());
    return true;
}

bool LoadColorPalette(sqlite3* db, std::vector<color_palette_entry>& out_entries)
{
    if (db == nullptr) return false;

    out_entries.clear();

    const char* sql = "SELECT color_id, r, g, b FROM color_palette ORDER BY color_id;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        hb::logger::warn("Failed to prepare color_palette query");
        return false;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        color_palette_entry entry = {};
        entry.color_id = static_cast<uint8_t>(sqlite3_column_int(stmt, 0));
        entry.r        = static_cast<uint8_t>(sqlite3_column_int(stmt, 1));
        entry.g        = static_cast<uint8_t>(sqlite3_column_int(stmt, 2));
        entry.b        = static_cast<uint8_t>(sqlite3_column_int(stmt, 3));
        out_entries.push_back(entry);
    }

    sqlite3_finalize(stmt);
    hb::logger::log("- {} color palette entries loaded", (int)out_entries.size());
    return true;
}

bool LoadAttributePrefixTypes(sqlite3* db, std::vector<attribute_prefix_type_entry>& out)
{
    if (db == nullptr) return false;

    out.clear();

    const char* sql = "SELECT prefix_id, multiplier, min_value, max_value FROM attribute_prefix_types ORDER BY prefix_id;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        hb::logger::warn("Failed to prepare attribute_prefix_types query");
        return false;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        attribute_prefix_type_entry entry = {};
        entry.prefix_id  = static_cast<uint8_t>(sqlite3_column_int(stmt, 0));
        entry.multiplier = static_cast<uint8_t>(sqlite3_column_int(stmt, 1));
        entry.min_value  = static_cast<uint8_t>(sqlite3_column_int(stmt, 2));
        entry.max_value  = static_cast<uint8_t>(sqlite3_column_int(stmt, 3));
        out.push_back(entry);
    }

    sqlite3_finalize(stmt);
    hb::logger::log("- {} attribute prefix types loaded", (int)out.size());
    return true;
}

bool LoadAttributeSecondaryTypes(sqlite3* db, std::vector<attribute_secondary_type_entry>& out)
{
    if (db == nullptr) return false;

    out.clear();

    const char* sql = "SELECT secondary_id, multiplier, min_value, max_value FROM attribute_secondary_types ORDER BY secondary_id;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        hb::logger::warn("Failed to prepare attribute_secondary_types query");
        return false;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        attribute_secondary_type_entry entry = {};
        entry.secondary_id = static_cast<uint8_t>(sqlite3_column_int(stmt, 0));
        entry.multiplier   = static_cast<uint8_t>(sqlite3_column_int(stmt, 1));
        entry.min_value    = static_cast<uint8_t>(sqlite3_column_int(stmt, 2));
        entry.max_value    = static_cast<uint8_t>(sqlite3_column_int(stmt, 3));
        out.push_back(entry);
    }

    sqlite3_finalize(stmt);
    hb::logger::log("- {} attribute secondary types loaded", (int)out.size());
    return true;
}

bool SaveCreationItems(sqlite3* db, const std::vector<creation_item_entry>& items)
{
    if (db == nullptr) {
        return false;
    }

    if (!BeginTransaction(db)) {
        return false;
    }

    if (!ClearTable(db, "character_creation_items")) {
        RollbackTransaction(db);
        return false;
    }

    const char* sql =
        "INSERT INTO character_creation_items("
        " class_type, item_id, count, item_color, durability,"
        " is_equipped, gender_limit, sort_order"
        ") VALUES(?,?,?,?,?,?,?,?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        RollbackTransaction(db);
        return false;
    }

    for (const auto& entry : items) {
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
        int col = 1;
        sqlite3_bind_int(stmt, col++, entry.class_type);
        sqlite3_bind_int(stmt, col++, entry.item_id);
        sqlite3_bind_int(stmt, col++, entry.count);
        sqlite3_bind_int(stmt, col++, entry.item_color);
        sqlite3_bind_int(stmt, col++, entry.durability);
        sqlite3_bind_int(stmt, col++, entry.is_equipped);
        sqlite3_bind_int(stmt, col++, entry.gender_limit);
        sqlite3_bind_int(stmt, col++, entry.sort_order);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            hb::logger::error("Failed to insert creation item: {}", sqlite3_errmsg(db));
            sqlite3_finalize(stmt);
            RollbackTransaction(db);
            return false;
        }
    }

    sqlite3_finalize(stmt);
    if (!CommitTransaction(db)) {
        RollbackTransaction(db);
        return false;
    }
    return true;
}

bool SaveMagicConfigs(sqlite3* db, const CGame* game)
{
    if (db == nullptr || game == nullptr) {
        return false;
    }

    if (!BeginTransaction(db)) {
        return false;
    }

    if (!ClearTable(db, "magic_configs")) {
        RollbackTransaction(db);
        return false;
    }

    const char* sql =
        "INSERT INTO magic_configs("
        " magic_id, name, magic_type, delay_time, last_time, value1, value2, value3,"
        " value4, value5, value6, value7, value8, value9, value10, value11, value12,"
        " int_limit, gold_cost, category, attribute"
        ") VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        RollbackTransaction(db);
        return false;
    }

    for(int i = 0; i < hb::shared::limits::MaxMagicType; i++) {
        if (game->m_magic_config_list[i] == nullptr) {
            continue;
        }

        const CMagic* magic = game->m_magic_config_list[i];
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
        int col = 1;
        bool ok = true;
        ok &= (sqlite3_bind_int(stmt, col++, i) == SQLITE_OK);
        ok &= PrepareAndBindText(stmt, col++, magic->m_name);
        ok &= (sqlite3_bind_int(stmt, col++, magic->m_type) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, static_cast<int>(magic->m_delay_time)) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, static_cast<int>(magic->m_last_time)) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, magic->m_value_1) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, magic->m_value_2) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, magic->m_value_3) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, magic->m_value_4) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, magic->m_value_5) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, magic->m_value_6) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, magic->m_value_7) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, magic->m_value_8) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, magic->m_value_9) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, magic->m_value_10) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, magic->m_value_11) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, magic->m_value_12) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, magic->m_intelligence_limit) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, magic->m_gold_cost) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, magic->m_category) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, magic->m_attribute) == SQLITE_OK);

        if (!ok || sqlite3_step(stmt) != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            RollbackTransaction(db);
            return false;
        }
    }

    sqlite3_finalize(stmt);
    if (!CommitTransaction(db)) {
        RollbackTransaction(db);
        return false;
    }
    return true;
}

bool LoadMagicConfigs(sqlite3* db, CGame* game)
{
    if (db == nullptr || game == nullptr) {
        return false;
    }

    for(int i = 0; i < hb::shared::limits::MaxMagicType; i++) {
        delete game->m_magic_config_list[i];
        game->m_magic_config_list[i] = nullptr;
    }

    const char* sql =
        "SELECT magic_id, name, magic_type, delay_time, last_time, value1, value2, value3,"
        " value4, value5, value6, value7, value8, value9, value10, value11, value12,"
        " int_limit, gold_cost, category, attribute"
        " FROM magic_configs ORDER BY magic_id;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = 0;
        int magicId = sqlite3_column_int(stmt, col++);
        if (magicId < 0 || magicId >= hb::shared::limits::MaxMagicType) {
            continue;
        }

        CMagic* magic = new CMagic();
        CopyColumnText(stmt, col++, magic->m_name, sizeof(magic->m_name));
        magic->m_type = (short)sqlite3_column_int(stmt, col++);
        magic->m_delay_time = (uint32_t)sqlite3_column_int(stmt, col++);
        magic->m_last_time = (uint32_t)sqlite3_column_int(stmt, col++);
        magic->m_value_1 = (short)sqlite3_column_int(stmt, col++);
        magic->m_value_2 = (short)sqlite3_column_int(stmt, col++);
        magic->m_value_3 = (short)sqlite3_column_int(stmt, col++);
        magic->m_value_4 = (short)sqlite3_column_int(stmt, col++);
        magic->m_value_5 = (short)sqlite3_column_int(stmt, col++);
        magic->m_value_6 = (short)sqlite3_column_int(stmt, col++);
        magic->m_value_7 = (short)sqlite3_column_int(stmt, col++);
        magic->m_value_8 = (short)sqlite3_column_int(stmt, col++);
        magic->m_value_9 = (short)sqlite3_column_int(stmt, col++);
        magic->m_value_10 = (short)sqlite3_column_int(stmt, col++);
        magic->m_value_11 = (short)sqlite3_column_int(stmt, col++);
        magic->m_value_12 = (short)sqlite3_column_int(stmt, col++);
        magic->m_intelligence_limit = (short)sqlite3_column_int(stmt, col++);
        magic->m_gold_cost = sqlite3_column_int(stmt, col++);
        magic->m_category = (char)sqlite3_column_int(stmt, col++);
        magic->m_attribute = sqlite3_column_int(stmt, col++);

        game->m_magic_config_list[magicId] = magic;
    }

    sqlite3_finalize(stmt);
    return true;
}

bool SaveSkillConfigs(sqlite3* db, const CGame* game)
{
    if (db == nullptr || game == nullptr) {
        return false;
    }

    if (!BeginTransaction(db)) {
        return false;
    }

    if (!ClearTable(db, "skill_configs")) {
        RollbackTransaction(db);
        return false;
    }

    const char* sql =
        "INSERT INTO skill_configs("
        " skill_id, name, skill_type, value1, value2, value3, value4, value5, value6,"
        " is_useable, use_method"
        ") VALUES(?,?,?,?,?,?,?,?,?,?,?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        RollbackTransaction(db);
        return false;
    }

    for(int i = 0; i < hb::shared::limits::MaxSkillType; i++) {
        if (game->m_skill_config_list[i] == nullptr) {
            continue;
        }

        const CSkill* skill = game->m_skill_config_list[i];
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
        int col = 1;
        bool ok = true;
        ok &= (sqlite3_bind_int(stmt, col++, i) == SQLITE_OK);
        ok &= PrepareAndBindText(stmt, col++, skill->m_name);
        ok &= (sqlite3_bind_int(stmt, col++, skill->m_type) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, skill->m_value_1) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, skill->m_value_2) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, skill->m_value_3) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, skill->m_value_4) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, skill->m_value_5) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, skill->m_value_6) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, skill->m_is_useable ? 1 : 0) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, skill->m_use_method) == SQLITE_OK);

        if (!ok || sqlite3_step(stmt) != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            RollbackTransaction(db);
            return false;
        }
    }

    sqlite3_finalize(stmt);
    if (!CommitTransaction(db)) {
        RollbackTransaction(db);
        return false;
    }
    return true;
}

bool LoadSkillConfigs(sqlite3* db, CGame* game)
{
    if (db == nullptr || game == nullptr) {
        return false;
    }

    for(int i = 0; i < hb::shared::limits::MaxSkillType; i++) {
        delete game->m_skill_config_list[i];
        game->m_skill_config_list[i] = nullptr;
    }

    const char* sql =
        "SELECT skill_id, name, skill_type, value1, value2, value3, value4, value5, value6,"
        " is_useable, use_method"
        " FROM skill_configs ORDER BY skill_id;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = 0;
        int skillId = sqlite3_column_int(stmt, col++);
        if (skillId < 0 || skillId >= hb::shared::limits::MaxSkillType) {
            continue;
        }

        CSkill* skill = new CSkill();
        CopyColumnText(stmt, col++, skill->m_name, sizeof(skill->m_name));
        skill->m_type = (short)sqlite3_column_int(stmt, col++);
        skill->m_value_1 = (short)sqlite3_column_int(stmt, col++);
        skill->m_value_2 = (short)sqlite3_column_int(stmt, col++);
        skill->m_value_3 = (short)sqlite3_column_int(stmt, col++);
        skill->m_value_4 = (short)sqlite3_column_int(stmt, col++);
        skill->m_value_5 = (short)sqlite3_column_int(stmt, col++);
        skill->m_value_6 = (short)sqlite3_column_int(stmt, col++);
        skill->m_is_useable = (sqlite3_column_int(stmt, col++) != 0);
        skill->m_use_method = (char)sqlite3_column_int(stmt, col++);

        game->m_skill_config_list[skillId] = skill;
    }

    sqlite3_finalize(stmt);
    return true;
}

bool SaveQuestConfigs(sqlite3* db, const CGame* game)
{
    if (db == nullptr || game == nullptr) {
        return false;
    }

    if (!BeginTransaction(db)) {
        return false;
    }

    if (!ClearTable(db, "quest_configs")) {
        RollbackTransaction(db);
        return false;
    }

    const char* sql =
        "INSERT INTO quest_configs("
        " quest_index, side, quest_type, target_config_id, max_count, quest_from, min_level, max_level,"
        " required_skill_num, required_skill_level, time_limit, assign_type,"
        " reward_type1, reward_amount1, reward_type2, reward_amount2, reward_type3, reward_amount3,"
        " contribution, contribution_limit, response_mode, target_name, target_x, target_y, target_range,"
        " quest_id, req_contribution"
        ") VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        RollbackTransaction(db);
        return false;
    }

    for(int i = 0; i < hb::server::config::MaxQuestType; i++) {
        if (game->m_quest_manager->m_quest_config_list[i] == nullptr) {
            continue;
        }

        const CQuest* quest = game->m_quest_manager->m_quest_config_list[i];
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
        int col = 1;
        bool ok = true;
        ok &= (sqlite3_bind_int(stmt, col++, i) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, quest->m_side) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, quest->m_type) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, quest->m_target_config_id) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, quest->m_max_count) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, quest->m_from) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, quest->m_min_level) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, quest->m_max_level) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, quest->m_required_skill_num) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, quest->m_required_skill_level) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, quest->m_time_limit) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, quest->m_assign_type) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, quest->m_reward_type[1]) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, quest->m_reward_amount[1]) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, quest->m_reward_type[2]) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, quest->m_reward_amount[2]) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, quest->m_reward_type[3]) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, quest->m_reward_amount[3]) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, quest->m_contribution) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, quest->m_contribution_limit) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, quest->m_response_mode) == SQLITE_OK);
        ok &= PrepareAndBindText(stmt, col++, quest->m_target_name);
        ok &= (sqlite3_bind_int(stmt, col++, quest->m_x) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, quest->m_y) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, quest->m_range) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, quest->m_quest_id) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, quest->m_req_contribution) == SQLITE_OK);

        if (!ok || sqlite3_step(stmt) != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            RollbackTransaction(db);
            return false;
        }
    }

    sqlite3_finalize(stmt);
    if (!CommitTransaction(db)) {
        RollbackTransaction(db);
        return false;
    }
    return true;
}

bool LoadQuestConfigs(sqlite3* db, CGame* game)
{
    if (db == nullptr || game == nullptr) {
        return false;
    }

    for(int i = 0; i < hb::server::config::MaxQuestType; i++) {
        delete game->m_quest_manager->m_quest_config_list[i];
        game->m_quest_manager->m_quest_config_list[i] = nullptr;
    }

    const char* sql =
        "SELECT quest_index, side, quest_type, target_config_id, max_count, quest_from, min_level, max_level,"
        " required_skill_num, required_skill_level, time_limit, assign_type,"
        " reward_type1, reward_amount1, reward_type2, reward_amount2, reward_type3, reward_amount3,"
        " contribution, contribution_limit, response_mode, target_name, target_x, target_y, target_range,"
        " quest_id, req_contribution"
        " FROM quest_configs ORDER BY quest_index;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = 0;
        int questIndex = sqlite3_column_int(stmt, col++);
        if (questIndex < 0 || questIndex >= hb::server::config::MaxQuestType) {
            continue;
        }

        CQuest* quest = new CQuest();
        quest->m_side = (char)sqlite3_column_int(stmt, col++);
        quest->m_type = sqlite3_column_int(stmt, col++);
        quest->m_target_config_id = sqlite3_column_int(stmt, col++);
        quest->m_max_count = sqlite3_column_int(stmt, col++);
        quest->m_from = sqlite3_column_int(stmt, col++);
        quest->m_min_level = sqlite3_column_int(stmt, col++);
        quest->m_max_level = sqlite3_column_int(stmt, col++);
        quest->m_required_skill_num = sqlite3_column_int(stmt, col++);
        quest->m_required_skill_level = sqlite3_column_int(stmt, col++);
        quest->m_time_limit = sqlite3_column_int(stmt, col++);
        quest->m_assign_type = sqlite3_column_int(stmt, col++);
        quest->m_reward_type[1] = sqlite3_column_int(stmt, col++);
        quest->m_reward_amount[1] = sqlite3_column_int(stmt, col++);
        quest->m_reward_type[2] = sqlite3_column_int(stmt, col++);
        quest->m_reward_amount[2] = sqlite3_column_int(stmt, col++);
        quest->m_reward_type[3] = sqlite3_column_int(stmt, col++);
        quest->m_reward_amount[3] = sqlite3_column_int(stmt, col++);
        quest->m_contribution = sqlite3_column_int(stmt, col++);
        quest->m_contribution_limit = sqlite3_column_int(stmt, col++);
        quest->m_response_mode = sqlite3_column_int(stmt, col++);
        CopyColumnText(stmt, col++, quest->m_target_name, sizeof(quest->m_target_name));
        quest->m_x = sqlite3_column_int(stmt, col++);
        quest->m_y = sqlite3_column_int(stmt, col++);
        quest->m_range = sqlite3_column_int(stmt, col++);
        quest->m_quest_id = sqlite3_column_int(stmt, col++);
        quest->m_req_contribution = sqlite3_column_int(stmt, col++);

        game->m_quest_manager->m_quest_config_list[questIndex] = quest;
    }

    sqlite3_finalize(stmt);
    return true;
}

bool SavePortionConfigs(sqlite3* db, const CGame* game)
{
    if (db == nullptr || game == nullptr) {
        return false;
    }

    if (!BeginTransaction(db)) {
        return false;
    }

    if (!ClearTable(db, "potion_configs") || !ClearTable(db, "crafting_configs")) {
        RollbackTransaction(db);
        return false;
    }

    const char* potionSql =
        "INSERT INTO potion_configs("
        " potion_id, name, array0, array1, array2, array3, array4, array5, array6,"
        " array7, array8, array9, array10, array11, skill_limit, difficulty"
        ") VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);";
    const char* craftingSql =
        "INSERT INTO crafting_configs("
        " crafting_id, name, array0, array1, array2, array3, array4, array5, array6,"
        " array7, array8, array9, array10, array11, skill_limit, difficulty"
        ") VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);";

    sqlite3_stmt* potionStmt = nullptr;
    sqlite3_stmt* craftingStmt = nullptr;
    if (sqlite3_prepare_v2(db, potionSql, -1, &potionStmt, nullptr) != SQLITE_OK ||
        sqlite3_prepare_v2(db, craftingSql, -1, &craftingStmt, nullptr) != SQLITE_OK) {
        if (potionStmt) sqlite3_finalize(potionStmt);
        if (craftingStmt) sqlite3_finalize(craftingStmt);
        RollbackTransaction(db);
        return false;
    }

    for(int i = 0; i < hb::server::config::MaxPortionTypes; i++) {
        if (game->m_crafting_manager->m_portion_config_list[i] != nullptr) {
            const CPortion* potion = game->m_crafting_manager->m_portion_config_list[i];
            sqlite3_reset(potionStmt);
            sqlite3_clear_bindings(potionStmt);
            int col = 1;
            bool ok = true;
            ok &= (sqlite3_bind_int(potionStmt, col++, i) == SQLITE_OK);
            ok &= PrepareAndBindText(potionStmt, col++, potion->m_name);
            for(int a = 0; a < 12; a++) {
                ok &= (sqlite3_bind_int(potionStmt, col++, potion->m_array[a]) == SQLITE_OK);
            }
            ok &= (sqlite3_bind_int(potionStmt, col++, potion->m_skill_limit) == SQLITE_OK);
            ok &= (sqlite3_bind_int(potionStmt, col++, potion->m_difficulty) == SQLITE_OK);
            if (!ok || sqlite3_step(potionStmt) != SQLITE_DONE) {
                sqlite3_finalize(potionStmt);
                sqlite3_finalize(craftingStmt);
                RollbackTransaction(db);
                return false;
            }
        }

        if (game->m_crafting_manager->m_crafting_config_list[i] != nullptr) {
            const CPortion* crafting = game->m_crafting_manager->m_crafting_config_list[i];
            sqlite3_reset(craftingStmt);
            sqlite3_clear_bindings(craftingStmt);
            int col = 1;
            bool ok = true;
            ok &= (sqlite3_bind_int(craftingStmt, col++, i) == SQLITE_OK);
            ok &= PrepareAndBindText(craftingStmt, col++, crafting->m_name);
            for(int a = 0; a < 12; a++) {
                ok &= (sqlite3_bind_int(craftingStmt, col++, crafting->m_array[a]) == SQLITE_OK);
            }
            ok &= (sqlite3_bind_int(craftingStmt, col++, crafting->m_skill_limit) == SQLITE_OK);
            ok &= (sqlite3_bind_int(craftingStmt, col++, crafting->m_difficulty) == SQLITE_OK);
            if (!ok || sqlite3_step(craftingStmt) != SQLITE_DONE) {
                sqlite3_finalize(potionStmt);
                sqlite3_finalize(craftingStmt);
                RollbackTransaction(db);
                return false;
            }
        }
    }

    sqlite3_finalize(potionStmt);
    sqlite3_finalize(craftingStmt);
    if (!CommitTransaction(db)) {
        RollbackTransaction(db);
        return false;
    }
    return true;
}

bool LoadPortionConfigs(sqlite3* db, CGame* game)
{
    if (db == nullptr || game == nullptr) {
        return false;
    }

    for(int i = 0; i < hb::server::config::MaxPortionTypes; i++) {
        delete game->m_crafting_manager->m_portion_config_list[i];
        game->m_crafting_manager->m_portion_config_list[i] = nullptr;
        delete game->m_crafting_manager->m_crafting_config_list[i];
        game->m_crafting_manager->m_crafting_config_list[i] = nullptr;
    }

    const char* potionSql =
        "SELECT potion_id, name, array0, array1, array2, array3, array4, array5, array6,"
        " array7, array8, array9, array10, array11, skill_limit, difficulty"
        " FROM potion_configs ORDER BY potion_id;";
    const char* craftingSql =
        "SELECT crafting_id, name, array0, array1, array2, array3, array4, array5, array6,"
        " array7, array8, array9, array10, array11, skill_limit, difficulty"
        " FROM crafting_configs ORDER BY crafting_id;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, potionSql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = 0;
        int potionId = sqlite3_column_int(stmt, col++);
        if (potionId < 0 || potionId >= hb::server::config::MaxPortionTypes) {
            continue;
        }
        CPortion* potion = new CPortion();
        CopyColumnText(stmt, col++, potion->m_name, sizeof(potion->m_name));
        for(int a = 0; a < 12; a++) {
            potion->m_array[a] = (short)sqlite3_column_int(stmt, col++);
        }
        potion->m_skill_limit = sqlite3_column_int(stmt, col++);
        potion->m_difficulty = sqlite3_column_int(stmt, col++);
        game->m_crafting_manager->m_portion_config_list[potionId] = potion;
    }

    sqlite3_finalize(stmt);
    if (sqlite3_prepare_v2(db, craftingSql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = 0;
        int craftingId = sqlite3_column_int(stmt, col++);
        if (craftingId < 0 || craftingId >= hb::server::config::MaxPortionTypes) {
            continue;
        }
        CPortion* crafting = new CPortion();
        CopyColumnText(stmt, col++, crafting->m_name, sizeof(crafting->m_name));
        for(int a = 0; a < 12; a++) {
            crafting->m_array[a] = (short)sqlite3_column_int(stmt, col++);
        }
        crafting->m_skill_limit = sqlite3_column_int(stmt, col++);
        crafting->m_difficulty = sqlite3_column_int(stmt, col++);
        game->m_crafting_manager->m_crafting_config_list[craftingId] = crafting;
    }

    sqlite3_finalize(stmt);
    return true;
}

bool SaveBuildItemConfigs(sqlite3* db, const CGame* game)
{
    if (db == nullptr || game == nullptr) {
        return false;
    }

    if (!BeginTransaction(db)) {
        return false;
    }

    if (!ClearTable(db, "builditem_configs")) {
        RollbackTransaction(db);
        return false;
    }

    const char* sql =
        "INSERT INTO builditem_configs("
        " build_id, name, skill_limit, material_id1, material_count1, material_value1,"
        " material_id2, material_count2, material_value2, material_id3, material_count3, material_value3,"
        " material_id4, material_count4, material_value4, material_id5, material_count5, material_value5,"
        " material_id6, material_count6, material_value6, average_value, max_skill, attribute, item_id"
        ") VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        RollbackTransaction(db);
        return false;
    }

    for(int i = 0; i < hb::shared::limits::MaxBuildItems; i++) {
        if (game->m_build_item_list[i] == nullptr) {
            continue;
        }

        const CBuildItem* build = game->m_build_item_list[i];
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
        int col = 1;
        bool ok = true;
        ok &= (sqlite3_bind_int(stmt, col++, i) == SQLITE_OK);
        ok &= PrepareAndBindText(stmt, col++, build->m_name);
        ok &= (sqlite3_bind_int(stmt, col++, build->m_skill_limit) == SQLITE_OK);
        for(int a = 0; a < 6; a++) {
            ok &= (sqlite3_bind_int(stmt, col++, build->m_material_item_id[a]) == SQLITE_OK);
            ok &= (sqlite3_bind_int(stmt, col++, build->m_material_item_count[a]) == SQLITE_OK);
            ok &= (sqlite3_bind_int(stmt, col++, build->m_material_item_value[a]) == SQLITE_OK);
        }
        ok &= (sqlite3_bind_int(stmt, col++, build->m_average_value) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, build->m_max_skill) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, build->m_attribute) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, build->m_item_id) == SQLITE_OK);

        if (!ok || sqlite3_step(stmt) != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            RollbackTransaction(db);
            return false;
        }
    }

    sqlite3_finalize(stmt);
    if (!CommitTransaction(db)) {
        RollbackTransaction(db);
        return false;
    }
    return true;
}

bool LoadBuildItemConfigs(sqlite3* db, CGame* game)
{
    if (db == nullptr || game == nullptr) {
        return false;
    }

    for(int i = 0; i < hb::shared::limits::MaxBuildItems; i++) {
        delete game->m_build_item_list[i];
        game->m_build_item_list[i] = nullptr;
    }

    const char* sql =
        "SELECT build_id, name, skill_limit, material_id1, material_count1, material_value1,"
        " material_id2, material_count2, material_value2, material_id3, material_count3, material_value3,"
        " material_id4, material_count4, material_value4, material_id5, material_count5, material_value5,"
        " material_id6, material_count6, material_value6, average_value, max_skill, attribute, item_id"
        " FROM builditem_configs ORDER BY build_id;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = 0;
        int buildId = sqlite3_column_int(stmt, col++);
        if (buildId < 0 || buildId >= hb::shared::limits::MaxBuildItems) {
            continue;
        }

        CBuildItem* build = new CBuildItem();
        CopyColumnText(stmt, col++, build->m_name, sizeof(build->m_name));
        build->m_skill_limit = sqlite3_column_int(stmt, col++);
        for(int a = 0; a < 6; a++) {
            build->m_material_item_id[a] = sqlite3_column_int(stmt, col++);
            build->m_material_item_count[a] = sqlite3_column_int(stmt, col++);
            build->m_material_item_value[a] = sqlite3_column_int(stmt, col++);
        }
        build->m_average_value = sqlite3_column_int(stmt, col++);
        build->m_max_skill = sqlite3_column_int(stmt, col++);
        build->m_attribute = (uint16_t)sqlite3_column_int(stmt, col++);
        int storedItemId = sqlite3_column_int(stmt, col++);

        // Use the stored item_id directly instead of looking up by name
        // (names in items table are now display names, not internal names)
        if (storedItemId > 0 && storedItemId < MaxItemTypes && game->m_item_config_list[storedItemId] != nullptr) {
            build->m_item_id = static_cast<short>(storedItemId);
        } else {
            // Fallback to name lookup for backwards compatibility
            CItem tempItem;
            if (game->m_item_manager->init_item_attr(&tempItem, build->m_name)) {
                build->m_item_id = tempItem.m_id_num;
            } else {
                char logMsg[256] = {};
                hb::logger::log("BuildItem '{}' has invalid item_id {}", build->m_name, storedItemId);
                delete build;
                continue; // Skip this build item instead of failing completely
            }
        }

        build->m_max_value = 0;
        for(int a = 0; a < 6; a++) {
            build->m_max_value += (build->m_material_item_value[a] * 100);
        }

        game->m_build_item_list[buildId] = build;
    }

    sqlite3_finalize(stmt);
    return true;
}

bool SaveCrusadeConfig(sqlite3* db, const CGame* game)
{
    if (db == nullptr || game == nullptr) {
        return false;
    }

    if (!BeginTransaction(db)) {
        return false;
    }

    if (!ClearTable(db, "crusade_structures")) {
        RollbackTransaction(db);
        return false;
    }

    const char* sql =
        "INSERT INTO crusade_structures("
        " structure_id, map_name, structure_type, pos_x, pos_y"
        ") VALUES(?,?,?,?,?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        RollbackTransaction(db);
        return false;
    }

    for(int i = 0; i < hb::shared::limits::MaxCrusadeStructures; i++) {
        const auto& entry = game->m_crusade_structures[i];
        if (entry.type == 0 || entry.map_name[0] == 0) {
            continue;
        }
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
        bool ok = true;
        ok &= (sqlite3_bind_int(stmt, 1, i) == SQLITE_OK);
        ok &= PrepareAndBindText(stmt, 2, entry.map_name);
        ok &= (sqlite3_bind_int(stmt, 3, entry.type) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, 4, entry.x) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, 5, entry.y) == SQLITE_OK);
        if (!ok || sqlite3_step(stmt) != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            RollbackTransaction(db);
            return false;
        }
    }

    sqlite3_finalize(stmt);
    if (!CommitTransaction(db)) {
        RollbackTransaction(db);
        return false;
    }
    return true;
}

bool LoadCrusadeConfig(sqlite3* db, CGame* game)
{
    if (db == nullptr || game == nullptr) {
        return false;
    }

    for(int i = 0; i < hb::shared::limits::MaxCrusadeStructures; i++) {
        std::memset(game->m_crusade_structures[i].map_name, 0, sizeof(game->m_crusade_structures[i].map_name));
        game->m_crusade_structures[i].type = 0;
        game->m_crusade_structures[i].x = 0;
        game->m_crusade_structures[i].y = 0;
    }

    const char* sql =
        "SELECT structure_id, map_name, structure_type, pos_x, pos_y"
        " FROM crusade_structures ORDER BY structure_id;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = 0;
        int structureId = sqlite3_column_int(stmt, col++);
        if (structureId < 0 || structureId >= hb::shared::limits::MaxCrusadeStructures) {
            continue;
        }
        CopyColumnText(stmt, col++, game->m_crusade_structures[structureId].map_name,
            sizeof(game->m_crusade_structures[structureId].map_name));
        game->m_crusade_structures[structureId].type = (char)sqlite3_column_int(stmt, col++);
        game->m_crusade_structures[structureId].x = sqlite3_column_int(stmt, col++);
        game->m_crusade_structures[structureId].y = sqlite3_column_int(stmt, col++);
    }

    sqlite3_finalize(stmt);
    return true;
}

bool SaveScheduleConfig(sqlite3* db, const CGame* game)
{
    if (db == nullptr || game == nullptr) {
        return false;
    }

    if (!BeginTransaction(db)) {
        return false;
    }

    if (!ClearTable(db, "event_schedule")) {
        RollbackTransaction(db);
        return false;
    }

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT INTO event_schedule(event_type, schedule_index, day, start_hour, start_minute, end_hour, end_minute, is_active) "
                      "VALUES(?, ?, ?, ?, ?, ?, ?, ?);";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        RollbackTransaction(db);
        return false;
    }

    bool success = true;

    // Save crusade schedules
    for(int i = 0; i < hb::server::config::MaxSchedule && success; i++) {
        if (game->m_crusade_war_schedule[i].day >= 0) {
            sqlite3_reset(stmt);
            sqlite3_clear_bindings(stmt);
            bool ok = true;
            ok &= PrepareAndBindText(stmt, 1, "crusade");
            ok &= (sqlite3_bind_int(stmt, 2, i) == SQLITE_OK);
            ok &= (sqlite3_bind_int(stmt, 3, game->m_crusade_war_schedule[i].day) == SQLITE_OK);
            ok &= (sqlite3_bind_int(stmt, 4, game->m_crusade_war_schedule[i].hour) == SQLITE_OK);
            ok &= (sqlite3_bind_int(stmt, 5, game->m_crusade_war_schedule[i].minute) == SQLITE_OK);
            ok &= (sqlite3_bind_null(stmt, 6) == SQLITE_OK);  // end_hour
            ok &= (sqlite3_bind_null(stmt, 7) == SQLITE_OK);  // end_minute
            ok &= (sqlite3_bind_int(stmt, 8, 0) == SQLITE_OK);  // is_active
            success = ok && (sqlite3_step(stmt) == SQLITE_DONE);
        }
    }

    // Save apocalypse schedules (start and end times in one row)
    for(int i = 0; i < hb::server::config::MaxApocalypse && success; i++) {
        if (game->m_apocalypse_schedule_start[i].day >= 0) {
            sqlite3_reset(stmt);
            sqlite3_clear_bindings(stmt);
            bool ok = true;
            ok &= PrepareAndBindText(stmt, 1, "apocalypse");
            ok &= (sqlite3_bind_int(stmt, 2, i) == SQLITE_OK);
            ok &= (sqlite3_bind_int(stmt, 3, game->m_apocalypse_schedule_start[i].day) == SQLITE_OK);
            ok &= (sqlite3_bind_int(stmt, 4, game->m_apocalypse_schedule_start[i].hour) == SQLITE_OK);
            ok &= (sqlite3_bind_int(stmt, 5, game->m_apocalypse_schedule_start[i].minute) == SQLITE_OK);
            // End time from the corresponding end schedule
            if (game->m_apocalypse_schedule_end[i].day >= 0) {
                ok &= (sqlite3_bind_int(stmt, 6, game->m_apocalypse_schedule_end[i].hour) == SQLITE_OK);
                ok &= (sqlite3_bind_int(stmt, 7, game->m_apocalypse_schedule_end[i].minute) == SQLITE_OK);
            } else {
                ok &= (sqlite3_bind_null(stmt, 6) == SQLITE_OK);
                ok &= (sqlite3_bind_null(stmt, 7) == SQLITE_OK);
            }
            ok &= (sqlite3_bind_int(stmt, 8, 0) == SQLITE_OK);  // is_active
            success = ok && (sqlite3_step(stmt) == SQLITE_DONE);
        }
    }

    // Save heldenian schedules (these have end times)
    for(int i = 0; i < hb::server::config::MaxHeldenian && success; i++) {
        if (game->m_heldenian_schedule[i].day >= 0) {
            sqlite3_reset(stmt);
            sqlite3_clear_bindings(stmt);
            bool ok = true;
            ok &= PrepareAndBindText(stmt, 1, "heldenian");
            ok &= (sqlite3_bind_int(stmt, 2, i) == SQLITE_OK);
            ok &= (sqlite3_bind_int(stmt, 3, game->m_heldenian_schedule[i].day) == SQLITE_OK);
            ok &= (sqlite3_bind_int(stmt, 4, game->m_heldenian_schedule[i].start_hour) == SQLITE_OK);
            ok &= (sqlite3_bind_int(stmt, 5, game->m_heldenian_schedule[i].start_minute) == SQLITE_OK);
            ok &= (sqlite3_bind_int(stmt, 6, game->m_heldenian_schedule[i].end_hour) == SQLITE_OK);
            ok &= (sqlite3_bind_int(stmt, 7, game->m_heldenian_schedule[i].end_minute) == SQLITE_OK);
            ok &= (sqlite3_bind_int(stmt, 8, 0) == SQLITE_OK);  // is_active
            success = ok && (sqlite3_step(stmt) == SQLITE_DONE);
        }
    }

    sqlite3_finalize(stmt);

    if (!success) {
        RollbackTransaction(db);
        return false;
    }

    if (!CommitTransaction(db)) {
        RollbackTransaction(db);
        return false;
    }
    return true;
}

bool LoadScheduleConfig(sqlite3* db, CGame* game)
{
    if (db == nullptr || game == nullptr) {
        return false;
    }

    // initialize all schedules to -1
    for(int i = 0; i < hb::server::config::MaxSchedule; i++) {
        game->m_crusade_war_schedule[i].day = -1;
        game->m_crusade_war_schedule[i].hour = -1;
        game->m_crusade_war_schedule[i].minute = -1;
    }
    for(int i = 0; i < hb::server::config::MaxApocalypse; i++) {
        game->m_apocalypse_schedule_start[i].day = -1;
        game->m_apocalypse_schedule_start[i].hour = -1;
        game->m_apocalypse_schedule_start[i].minute = -1;
        game->m_apocalypse_schedule_end[i].day = -1;
        game->m_apocalypse_schedule_end[i].hour = -1;
        game->m_apocalypse_schedule_end[i].minute = -1;
    }
    for(int i = 0; i < hb::server::config::MaxHeldenian; i++) {
        game->m_heldenian_schedule[i].day = -1;
        game->m_heldenian_schedule[i].start_hour = -1;
        game->m_heldenian_schedule[i].start_minute = -1;
        game->m_heldenian_schedule[i].end_hour = -1;
        game->m_heldenian_schedule[i].end_minute = -1;
    }

    // Load all events from unified table
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT event_type, schedule_index, day, start_hour, start_minute, end_hour, end_minute, is_active "
                      "FROM event_schedule ORDER BY event_type, schedule_index;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* eventType = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (eventType == nullptr) continue;

        int idx = sqlite3_column_int(stmt, 1);
        int day = sqlite3_column_int(stmt, 2);
        int startHour = sqlite3_column_int(stmt, 3);
        int startMinute = sqlite3_column_int(stmt, 4);
        int endHour = sqlite3_column_type(stmt, 5) != SQLITE_NULL ? sqlite3_column_int(stmt, 5) : -1;
        int endMinute = sqlite3_column_type(stmt, 6) != SQLITE_NULL ? sqlite3_column_int(stmt, 6) : -1;
        // is_active at column 7 - can be used for state tracking

        if (strcmp(eventType, "crusade") == 0) {
            if (idx >= 0 && idx < hb::server::config::MaxSchedule) {
                game->m_crusade_war_schedule[idx].day = day;
                game->m_crusade_war_schedule[idx].hour = startHour;
                game->m_crusade_war_schedule[idx].minute = startMinute;
            }
        }
        else if (strcmp(eventType, "apocalypse") == 0) {
            if (idx >= 0 && idx < hb::server::config::MaxApocalypse) {
                // start time
                game->m_apocalypse_schedule_start[idx].day = day;
                game->m_apocalypse_schedule_start[idx].hour = startHour;
                game->m_apocalypse_schedule_start[idx].minute = startMinute;
                // End time (same day as start)
                game->m_apocalypse_schedule_end[idx].day = day;
                game->m_apocalypse_schedule_end[idx].hour = endHour;
                game->m_apocalypse_schedule_end[idx].minute = endMinute;
            }
        }
        else if (strcmp(eventType, "heldenian") == 0) {
            if (idx >= 0 && idx < hb::server::config::MaxHeldenian) {
                game->m_heldenian_schedule[idx].day = day;
                game->m_heldenian_schedule[idx].start_hour = startHour;
                game->m_heldenian_schedule[idx].start_minute = startMinute;
                game->m_heldenian_schedule[idx].end_hour = endHour;
                game->m_heldenian_schedule[idx].end_minute = endMinute;
            }
        }
    }
    sqlite3_finalize(stmt);

    return true;
}

bool HasGameConfigRows(sqlite3* db, const char* tableName)
{
    if (db == nullptr || tableName == nullptr || tableName[0] == 0) {
        return false;
    }

    char sql[256] = {};
    std::snprintf(sql, sizeof(sql), "SELECT 1 FROM %s LIMIT 1;", tableName);
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    bool hasRow = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return hasRow;
}

void CloseGameConfigDatabase(sqlite3* db)
{
    if (db != nullptr) {
        sqlite3_close(db);
    }
}

bool LoadFormulas(sqlite3* db, hb::shared::formula_engine& engine)
{
    if (db == nullptr) return false;

    engine.clear();

    // Load formulas (expression-based, single table)
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, "SELECT formula_id, expression, description FROM formulas;",
        -1, &stmt, nullptr) != SQLITE_OK)
    {
        return false;
    }

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const unsigned char* id = sqlite3_column_text(stmt, 0);
        const unsigned char* expr = sqlite3_column_text(stmt, 1);
        const unsigned char* desc = sqlite3_column_text(stmt, 2);
        if (!id || !expr) continue;

        std::string sid = reinterpret_cast<const char*>(id);
        std::string sexpr = reinterpret_cast<const char*>(expr);
        std::string sdesc = desc ? reinterpret_cast<const char*>(desc) : "";

        if (!engine.add_formula(sid, sexpr, sdesc))
        {
            sqlite3_finalize(stmt);
            return false;
        }
        ++count;
    }
    sqlite3_finalize(stmt);

    // Load scaling profiles
    if (sqlite3_prepare_v2(db,
        "SELECT profile_id, bracket_min, bracket_max, multiplier FROM scaling_profiles ORDER BY profile_id, bracket_min;",
        -1, &stmt, nullptr) != SQLITE_OK)
    {
        return false;
    }

    std::unordered_map<std::string, hb::shared::scaling_profile> profile_map;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const unsigned char* pid = sqlite3_column_text(stmt, 0);
        if (!pid) continue;
        std::string profile_id = reinterpret_cast<const char*>(pid);

        hb::shared::scaling_bracket bracket;
        bracket.bracket_min = sqlite3_column_int(stmt, 1);
        bracket.bracket_max = sqlite3_column_int(stmt, 2);
        bracket.multiplier = sqlite3_column_double(stmt, 3);

        profile_map[profile_id].profile_id = profile_id;
        profile_map[profile_id].brackets.push_back(bracket);
    }
    sqlite3_finalize(stmt);

    for (auto& [id, profile] : profile_map)
        engine.add_scaling_profile(profile);

    return true;
}

