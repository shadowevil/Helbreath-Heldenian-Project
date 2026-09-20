#include "AccountSqliteStore.h"

#include <filesystem>
#include <cstdio>
#include <cctype>
#include <cstring>
#include <map>
#include <string>

#include "Client.h"
#include "NetConstants.h"
#include "sqlite3.h"
#include "Log.h"
#include "TimeUtils.h"

namespace
{
    static void LowercaseInPlace(char* buf, size_t len)
    {
        for (size_t i = 0; i < len && buf[i] != '\0'; i++)
            buf[i] = static_cast<char>(::tolower(static_cast<unsigned char>(buf[i])));
    }
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

    bool PrepareAndBindText(sqlite3_stmt** stmt, int idx, const char* value)
    {
        return sqlite3_bind_text(*stmt, idx, value, -1, SQLITE_TRANSIENT) == SQLITE_OK;
    }

    bool PrepareAndBindText(sqlite3_stmt* stmt, int idx, const char* value)
    {
        return sqlite3_bind_text(stmt, idx, value, -1, SQLITE_TRANSIENT) == SQLITE_OK;
    }

    bool ColumnExists(sqlite3* db, const char* tableName, const char* columnName)
    {
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

    bool AddColumnIfMissing(sqlite3* db, const char* tableName, const char* columnName, const char* columnDef)
    {
        if (ColumnExists(db, tableName, columnName)) {
            return true;
        }

        char sql[512] = {};
        std::snprintf(sql, sizeof(sql), "ALTER TABLE %s ADD COLUMN %s %s;", tableName, columnName, columnDef);
        return ExecSql(db, sql);
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

    // Load item name to ID mapping from gamedata.db
    bool LoadItemNameMapping(std::map<std::string, int>& mapping)
{
    sqlite3* configDb = nullptr;
    if (sqlite3_open("gamedata.db", &configDb) != SQLITE_OK) {
        sqlite3_close(configDb);
        return false;
    }

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT item_id, name FROM items";
    if (sqlite3_prepare_v2(configDb, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        sqlite3_close(configDb);
        return false;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int item_id = sqlite3_column_int(stmt, 0);
        const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        if (name) {
            mapping[name] = item_id;
        }
    }

    sqlite3_finalize(stmt);
    sqlite3_close(configDb);
    return !mapping.empty();
}

// Migrate character_items and character_bank_items from item_name to item_id
static bool MigrateItemNamesToIds(sqlite3* db)
{
    // Check if migration is needed (old schema has item_name, new has item_id)
    bool hasItemName = ColumnExists(db, "character_items", "item_name");
    bool hasItemId = ColumnExists(db, "character_items", "item_id");

    if (!hasItemName || hasItemId) {
        // Already migrated or fresh database
        return true;
    }

    hb::logger::error("SQLite: migrating item storage from names to IDs");

    // Load item mapping
    std::map<std::string, int> itemMapping;
    if (!LoadItemNameMapping(itemMapping)) {
        hb::logger::error("SQLite: failed to load item mapping from gamedata.db");
        return false;
    }

    // Begin transaction
    if (!ExecSql(db, "BEGIN TRANSACTION;")) {
        return false;
    }

    // Create new character_items table
    const char* createItemsSql =
        "CREATE TABLE character_items_new ("
        " character_name TEXT NOT NULL,"
        " slot INTEGER NOT NULL,"
        " item_id INTEGER NOT NULL,"
        " count INTEGER NOT NULL,"
        " touch_effect_type INTEGER NOT NULL,"
        " touch_effect_value1 INTEGER NOT NULL,"
        " touch_effect_value2 INTEGER NOT NULL,"
        " touch_effect_value3 INTEGER NOT NULL,"
        " item_color INTEGER NOT NULL,"
        " spec_effect_value1 INTEGER NOT NULL,"
        " spec_effect_value2 INTEGER NOT NULL,"
        " spec_effect_value3 INTEGER NOT NULL,"
        " cur_durability INTEGER NOT NULL,"
        " custom_made INTEGER NOT NULL DEFAULT 0,"
        " prefix_type INTEGER NOT NULL DEFAULT 0,"
        " prefix_value INTEGER NOT NULL DEFAULT 0,"
        " secondary_type INTEGER NOT NULL DEFAULT 0,"
        " secondary_value INTEGER NOT NULL DEFAULT 0,"
        " enchant_bonus INTEGER NOT NULL DEFAULT 0,"
        " pos_x INTEGER NOT NULL,"
        " pos_y INTEGER NOT NULL,"
        " is_equipped INTEGER NOT NULL,"
        " PRIMARY KEY(character_name, slot),"
        " FOREIGN KEY(character_name) REFERENCES characters(character_name) ON DELETE CASCADE"
        ");";

    if (!ExecSql(db, createItemsSql)) {
        ExecSql(db, "ROLLBACK;");
        return false;
    }

    // Copy and convert character_items
    sqlite3_stmt* readStmt = nullptr;
    const char* readSql =
        "SELECT character_name, slot, item_name, count, touch_effect_type,"
        " touch_effect_value1, touch_effect_value2, touch_effect_value3,"
        " item_color, spec_effect_value1, spec_effect_value2, spec_effect_value3,"
        " cur_durability, custom_made, prefix_type, prefix_value,"
        " secondary_type, secondary_value, enchant_bonus,"
        " pos_x, pos_y, is_equipped FROM character_items";

    if (sqlite3_prepare_v2(db, readSql, -1, &readStmt, nullptr) != SQLITE_OK) {
        ExecSql(db, "ROLLBACK;");
        return false;
    }

    sqlite3_stmt* writeStmt = nullptr;
    const char* writeSql =
        "INSERT INTO character_items_new VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)";

    if (sqlite3_prepare_v2(db, writeSql, -1, &writeStmt, nullptr) != SQLITE_OK) {
        sqlite3_finalize(readStmt);
        ExecSql(db, "ROLLBACK;");
        return false;
    }

    int migratedItems = 0;
    int skippedItems = 0;

    while (sqlite3_step(readStmt) == SQLITE_ROW) {
        const char* charName = reinterpret_cast<const char*>(sqlite3_column_text(readStmt, 0));
        int slot = sqlite3_column_int(readStmt, 1);
        const char* itemName = reinterpret_cast<const char*>(sqlite3_column_text(readStmt, 2));

        if (!itemName || itemMapping.find(itemName) == itemMapping.end()) {
            skippedItems++;
            continue;
        }

        int item_id = itemMapping[itemName];

        sqlite3_reset(writeStmt);
        sqlite3_clear_bindings(writeStmt);

        int col = 1;
        sqlite3_bind_text(writeStmt, col++, charName, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(writeStmt, col++, slot);
        sqlite3_bind_int(writeStmt, col++, item_id);
        for(int i = 3; i <= 21; i++) {
            sqlite3_bind_int(writeStmt, col++, sqlite3_column_int(readStmt, i));
        }

        if (sqlite3_step(writeStmt) != SQLITE_DONE) {
            skippedItems++;
        } else {
            migratedItems++;
        }
    }

    sqlite3_finalize(readStmt);
    sqlite3_finalize(writeStmt);

    // Create new character_bank_items table
    const char* createBankSql =
        "CREATE TABLE character_bank_items_new ("
        " character_name TEXT NOT NULL,"
        " slot INTEGER NOT NULL,"
        " item_id INTEGER NOT NULL,"
        " count INTEGER NOT NULL,"
        " touch_effect_type INTEGER NOT NULL,"
        " touch_effect_value1 INTEGER NOT NULL,"
        " touch_effect_value2 INTEGER NOT NULL,"
        " touch_effect_value3 INTEGER NOT NULL,"
        " item_color INTEGER NOT NULL,"
        " spec_effect_value1 INTEGER NOT NULL,"
        " spec_effect_value2 INTEGER NOT NULL,"
        " spec_effect_value3 INTEGER NOT NULL,"
        " cur_durability INTEGER NOT NULL,"
        " custom_made INTEGER NOT NULL DEFAULT 0,"
        " prefix_type INTEGER NOT NULL DEFAULT 0,"
        " prefix_value INTEGER NOT NULL DEFAULT 0,"
        " secondary_type INTEGER NOT NULL DEFAULT 0,"
        " secondary_value INTEGER NOT NULL DEFAULT 0,"
        " enchant_bonus INTEGER NOT NULL DEFAULT 0,"
        " PRIMARY KEY(character_name, slot),"
        " FOREIGN KEY(character_name) REFERENCES characters(character_name) ON DELETE CASCADE"
        ");";

    if (!ExecSql(db, createBankSql)) {
        ExecSql(db, "ROLLBACK;");
        return false;
    }

    // Copy and convert character_bank_items
    readSql =
        "SELECT character_name, slot, item_name, count, touch_effect_type,"
        " touch_effect_value1, touch_effect_value2, touch_effect_value3,"
        " item_color, spec_effect_value1, spec_effect_value2, spec_effect_value3,"
        " cur_durability, custom_made, prefix_type, prefix_value,"
        " secondary_type, secondary_value, enchant_bonus FROM character_bank_items";

    if (sqlite3_prepare_v2(db, readSql, -1, &readStmt, nullptr) != SQLITE_OK) {
        ExecSql(db, "ROLLBACK;");
        return false;
    }

    writeSql = "INSERT INTO character_bank_items_new VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)";

    if (sqlite3_prepare_v2(db, writeSql, -1, &writeStmt, nullptr) != SQLITE_OK) {
        sqlite3_finalize(readStmt);
        ExecSql(db, "ROLLBACK;");
        return false;
    }

    int migratedBankItems = 0;
    int skippedBankItems = 0;

    while (sqlite3_step(readStmt) == SQLITE_ROW) {
        const char* charName = reinterpret_cast<const char*>(sqlite3_column_text(readStmt, 0));
        int slot = sqlite3_column_int(readStmt, 1);
        const char* itemName = reinterpret_cast<const char*>(sqlite3_column_text(readStmt, 2));

        if (!itemName || itemMapping.find(itemName) == itemMapping.end()) {
            skippedBankItems++;
            continue;
        }

        int item_id = itemMapping[itemName];

        sqlite3_reset(writeStmt);
        sqlite3_clear_bindings(writeStmt);

        int col = 1;
        sqlite3_bind_text(writeStmt, col++, charName, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(writeStmt, col++, slot);
        sqlite3_bind_int(writeStmt, col++, item_id);
        for(int i = 3; i <= 13; i++) {
            sqlite3_bind_int(writeStmt, col++, sqlite3_column_int(readStmt, i));
        }

        if (sqlite3_step(writeStmt) != SQLITE_DONE) {
            skippedBankItems++;
        } else {
            migratedBankItems++;
        }
    }

    sqlite3_finalize(readStmt);
    sqlite3_finalize(writeStmt);

    // Drop old tables and rename new ones
    if (!ExecSql(db, "DROP TABLE character_items;") ||
        !ExecSql(db, "DROP TABLE character_bank_items;") ||
        !ExecSql(db, "ALTER TABLE character_items_new RENAME TO character_items;") ||
        !ExecSql(db, "ALTER TABLE character_bank_items_new RENAME TO character_bank_items;")) {
        ExecSql(db, "ROLLBACK;");
        return false;
    }

    // Commit transaction
    if (!ExecSql(db, "COMMIT;")) {
        return false;
    }

    hb::logger::error("SQLite: migration complete, {} items, {} bank items migrated ({}/{} skipped)", migratedItems, migratedBankItems, skippedItems, skippedBankItems);

    return true;
}

} // end anonymous namespace

bool EnsureAccountDatabase(const char* account_name, sqlite3** outDb, std::string& outPath)
{
    if (outDb == nullptr || account_name == nullptr || account_name[0] == 0) {
        return false;
    }

    std::filesystem::create_directories("accounts");

    char lowerName[64] = {};
    std::strncpy(lowerName, account_name, sizeof(lowerName) - 1);
    LowercaseInPlace(lowerName, sizeof(lowerName));
    char dbPath[260] = {};
    std::snprintf(dbPath, sizeof(dbPath), "accounts/%s.db", lowerName);
    outPath = dbPath;

    sqlite3* db = nullptr;
    int rc = sqlite3_open(dbPath, &db);
    if (rc != SQLITE_OK) {
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
        "INSERT OR REPLACE INTO meta(key, value) VALUES('schema_version','6');"
        "CREATE TABLE IF NOT EXISTS accounts ("
        " account_name TEXT PRIMARY KEY,"
        " password_hash TEXT NOT NULL,"
        " password_salt TEXT NOT NULL,"
        " email TEXT NOT NULL,"
        " created_at TEXT NOT NULL,"
        " password_changed_at TEXT NOT NULL,"
        " last_ip TEXT NOT NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS characters ("
        " character_name TEXT PRIMARY KEY,"
        " account_name TEXT NOT NULL,"
        " created_at TEXT NOT NULL,"
        " underwear_type INTEGER NOT NULL DEFAULT 0,"
        " hair_color INTEGER NOT NULL DEFAULT 0,"
        " hair_style INTEGER NOT NULL DEFAULT 0,"
        " skin_color INTEGER NOT NULL DEFAULT 0,"
        " level INTEGER NOT NULL,"
        " exp INTEGER NOT NULL,"
        " map_name TEXT NOT NULL,"
        " map_x INTEGER NOT NULL,"
        " map_y INTEGER NOT NULL,"
        " hp INTEGER NOT NULL,"
        " mp INTEGER NOT NULL,"
        " sp INTEGER NOT NULL,"
        " str INTEGER NOT NULL,"
        " vit INTEGER NOT NULL,"
        " dex INTEGER NOT NULL,"
        " intl INTEGER NOT NULL,"
        " mag INTEGER NOT NULL,"
        " chr INTEGER NOT NULL,"
        " gender INTEGER NOT NULL,"
        " skin INTEGER NOT NULL,"
        " hairstyle INTEGER NOT NULL,"
        " haircolor INTEGER NOT NULL,"
        " underwear INTEGER NOT NULL,"
        " FOREIGN KEY(account_name) REFERENCES accounts(account_name) ON DELETE CASCADE"
        ");"
        "CREATE TABLE IF NOT EXISTS character_items ("
        " character_name TEXT NOT NULL,"
        " slot INTEGER NOT NULL,"
        " item_id INTEGER NOT NULL,"
        " count INTEGER NOT NULL,"
        " touch_effect_type INTEGER NOT NULL,"
        " touch_effect_value1 INTEGER NOT NULL,"
        " touch_effect_value2 INTEGER NOT NULL,"
        " touch_effect_value3 INTEGER NOT NULL,"
        " item_color INTEGER NOT NULL,"
        " spec_effect_value1 INTEGER NOT NULL,"
        " spec_effect_value2 INTEGER NOT NULL,"
        " spec_effect_value3 INTEGER NOT NULL,"
        " cur_durability INTEGER NOT NULL,"
        " custom_made INTEGER NOT NULL DEFAULT 0,"
        " prefix_type INTEGER NOT NULL DEFAULT 0,"
        " prefix_value INTEGER NOT NULL DEFAULT 0,"
        " secondary_type INTEGER NOT NULL DEFAULT 0,"
        " secondary_value INTEGER NOT NULL DEFAULT 0,"
        " enchant_bonus INTEGER NOT NULL DEFAULT 0,"
        " pos_x INTEGER NOT NULL,"
        " pos_y INTEGER NOT NULL,"
        " is_equipped INTEGER NOT NULL,"
        " PRIMARY KEY(character_name, slot),"
        " FOREIGN KEY(character_name) REFERENCES characters(character_name) ON DELETE CASCADE"
        ");"
        "CREATE TABLE IF NOT EXISTS character_bank_items ("
        " character_name TEXT NOT NULL,"
        " slot INTEGER NOT NULL,"
        " item_id INTEGER NOT NULL,"
        " count INTEGER NOT NULL,"
        " touch_effect_type INTEGER NOT NULL,"
        " touch_effect_value1 INTEGER NOT NULL,"
        " touch_effect_value2 INTEGER NOT NULL,"
        " touch_effect_value3 INTEGER NOT NULL,"
        " item_color INTEGER NOT NULL,"
        " spec_effect_value1 INTEGER NOT NULL,"
        " spec_effect_value2 INTEGER NOT NULL,"
        " spec_effect_value3 INTEGER NOT NULL,"
        " cur_durability INTEGER NOT NULL,"
        " custom_made INTEGER NOT NULL DEFAULT 0,"
        " prefix_type INTEGER NOT NULL DEFAULT 0,"
        " prefix_value INTEGER NOT NULL DEFAULT 0,"
        " secondary_type INTEGER NOT NULL DEFAULT 0,"
        " secondary_value INTEGER NOT NULL DEFAULT 0,"
        " enchant_bonus INTEGER NOT NULL DEFAULT 0,"
        " PRIMARY KEY(character_name, slot),"
        " FOREIGN KEY(character_name) REFERENCES characters(character_name) ON DELETE CASCADE"
        ");"
        "CREATE TABLE IF NOT EXISTS character_item_positions ("
        " character_name TEXT NOT NULL,"
        " slot INTEGER NOT NULL,"
        " pos_x INTEGER NOT NULL,"
        " pos_y INTEGER NOT NULL,"
        " PRIMARY KEY(character_name, slot),"
        " FOREIGN KEY(character_name) REFERENCES characters(character_name) ON DELETE CASCADE"
        ");"
        "CREATE TABLE IF NOT EXISTS character_item_equips ("
        " character_name TEXT NOT NULL,"
        " slot INTEGER NOT NULL,"
        " is_equipped INTEGER NOT NULL,"
        " PRIMARY KEY(character_name, slot),"
        " FOREIGN KEY(character_name) REFERENCES characters(character_name) ON DELETE CASCADE"
        ");"
        "CREATE TABLE IF NOT EXISTS character_magic_mastery ("
        " character_name TEXT NOT NULL,"
        " magic_index INTEGER NOT NULL,"
        " mastery_value INTEGER NOT NULL,"
        " PRIMARY KEY(character_name, magic_index),"
        " FOREIGN KEY(character_name) REFERENCES characters(character_name) ON DELETE CASCADE"
        ");"
        "CREATE TABLE IF NOT EXISTS character_skill_mastery ("
        " character_name TEXT NOT NULL,"
        " skill_index INTEGER NOT NULL,"
        " mastery_value INTEGER NOT NULL,"
        " PRIMARY KEY(character_name, skill_index),"
        " FOREIGN KEY(character_name) REFERENCES characters(character_name) ON DELETE CASCADE"
        ");"
        "CREATE TABLE IF NOT EXISTS character_skill_ssn ("
        " character_name TEXT NOT NULL,"
        " skill_index INTEGER NOT NULL,"
        " ssn_value INTEGER NOT NULL,"
        " PRIMARY KEY(character_name, skill_index),"
        " FOREIGN KEY(character_name) REFERENCES characters(character_name) ON DELETE CASCADE"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_characters_account ON characters(account_name);"
        "CREATE TABLE IF NOT EXISTS block_list ("
        " blocked_account_name TEXT NOT NULL,"
        " blocked_character_name TEXT NOT NULL,"
        " PRIMARY KEY(blocked_account_name)"
        ");"
        "COMMIT;";

    if (!ExecSql(db, schemaSql)) {
        sqlite3_close(db);
        return false;
    }

    if (!AddColumnIfMissing(db, "characters", "profile", "TEXT NOT NULL DEFAULT ''") ||
        !AddColumnIfMissing(db, "characters", "location", "TEXT NOT NULL DEFAULT ''") ||
        !AddColumnIfMissing(db, "characters", "rating", "INTEGER NOT NULL DEFAULT 0") ||
        !AddColumnIfMissing(db, "characters", "luck", "INTEGER NOT NULL DEFAULT 0") ||
        !AddColumnIfMissing(db, "characters", "lu_pool", "INTEGER NOT NULL DEFAULT 0") ||
        !AddColumnIfMissing(db, "characters", "enemy_kill_count", "INTEGER NOT NULL DEFAULT 0") ||
        !AddColumnIfMissing(db, "characters", "pk_count", "INTEGER NOT NULL DEFAULT 0") ||
        !AddColumnIfMissing(db, "characters", "reward_gold", "INTEGER NOT NULL DEFAULT 0") ||
        !AddColumnIfMissing(db, "characters", "downskill_index", "INTEGER NOT NULL DEFAULT -1") ||
        !AddColumnIfMissing(db, "characters", "id_num1", "INTEGER NOT NULL DEFAULT 0") ||
        !AddColumnIfMissing(db, "characters", "id_num2", "INTEGER NOT NULL DEFAULT 0") ||
        !AddColumnIfMissing(db, "characters", "id_num3", "INTEGER NOT NULL DEFAULT 0") ||
        !AddColumnIfMissing(db, "characters", "hunger_status", "INTEGER NOT NULL DEFAULT 100") ||
        !AddColumnIfMissing(db, "characters", "timeleft_rating", "INTEGER NOT NULL DEFAULT 0") ||
        !AddColumnIfMissing(db, "characters", "timeleft_force_recall", "INTEGER NOT NULL DEFAULT 0") ||
        !AddColumnIfMissing(db, "characters", "timeleft_firm_staminar", "INTEGER NOT NULL DEFAULT 0") ||
        !AddColumnIfMissing(db, "characters", "penalty_block_year", "INTEGER NOT NULL DEFAULT 0") ||
        !AddColumnIfMissing(db, "characters", "penalty_block_month", "INTEGER NOT NULL DEFAULT 0") ||
        !AddColumnIfMissing(db, "characters", "penalty_block_day", "INTEGER NOT NULL DEFAULT 0") ||
        !AddColumnIfMissing(db, "characters", "quest_number", "INTEGER NOT NULL DEFAULT 0") ||
        !AddColumnIfMissing(db, "characters", "quest_id", "INTEGER NOT NULL DEFAULT 0") ||
        !AddColumnIfMissing(db, "characters", "current_quest_count", "INTEGER NOT NULL DEFAULT 0") ||
        !AddColumnIfMissing(db, "characters", "quest_reward_type", "INTEGER NOT NULL DEFAULT 0") ||
        !AddColumnIfMissing(db, "characters", "quest_reward_amount", "INTEGER NOT NULL DEFAULT 0") ||
        !AddColumnIfMissing(db, "characters", "contribution", "INTEGER NOT NULL DEFAULT 0") ||
        !AddColumnIfMissing(db, "characters", "war_contribution", "INTEGER NOT NULL DEFAULT 0") ||
        !AddColumnIfMissing(db, "characters", "quest_completed", "INTEGER NOT NULL DEFAULT 0") ||
        !AddColumnIfMissing(db, "characters", "special_event_id", "INTEGER NOT NULL DEFAULT 0") ||
        !AddColumnIfMissing(db, "characters", "super_attack_left", "INTEGER NOT NULL DEFAULT 0") ||
        !AddColumnIfMissing(db, "characters", "special_ability_time", "INTEGER NOT NULL DEFAULT 0") ||
        !AddColumnIfMissing(db, "characters", "locked_map_name", "TEXT NOT NULL DEFAULT ''") ||
        !AddColumnIfMissing(db, "characters", "locked_map_time", "INTEGER NOT NULL DEFAULT 0") ||
        !AddColumnIfMissing(db, "characters", "crusade_job", "INTEGER NOT NULL DEFAULT 0") ||
        !AddColumnIfMissing(db, "characters", "crusade_guid", "INTEGER NOT NULL DEFAULT 0") ||
        !AddColumnIfMissing(db, "characters", "construct_point", "INTEGER NOT NULL DEFAULT 0") ||
        !AddColumnIfMissing(db, "characters", "dead_penalty_time", "INTEGER NOT NULL DEFAULT 0") ||
        !AddColumnIfMissing(db, "characters", "party_id", "INTEGER NOT NULL DEFAULT 0") ||
        !AddColumnIfMissing(db, "characters", "gizon_item_upgrade_left", "INTEGER NOT NULL DEFAULT 0")) {
        sqlite3_close(db);
        return false;
    }

    // Migrate old item_name schema to item_id if needed
    if (!MigrateItemNamesToIds(db)) {
        sqlite3_close(db);
        return false;
    }

    // Migrate account_name to lowercase (accounts created before lowercase enforcement)
    // Must disable FK checks temporarily since accounts is referenced by characters
    ExecSql(db, "PRAGMA foreign_keys = OFF;");
    ExecSql(db, "UPDATE accounts SET account_name = LOWER(account_name) WHERE account_name != LOWER(account_name);");
    ExecSql(db, "UPDATE characters SET account_name = LOWER(account_name) WHERE account_name != LOWER(account_name);");
    ExecSql(db, "PRAGMA foreign_keys = ON;");

    *outDb = db;
    return true;
}

bool LoadAccountRecord(sqlite3* db, const char* account_name, AccountDbAccountData& outData)
{
    if (db == nullptr || account_name == nullptr) {
        return false;
    }

    const char* sql =
        "SELECT account_name, password_hash, password_salt, email, created_at, password_changed_at, last_ip "
        "FROM accounts WHERE account_name = ? COLLATE NOCASE;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    PrepareAndBindText(&stmt, 1, account_name);
    bool ok = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        std::memset(&outData, 0, sizeof(outData));
        CopyColumnText(stmt, 0, outData.name, sizeof(outData.name));
        CopyColumnText(stmt, 1, outData.password_hash, sizeof(outData.password_hash));
        CopyColumnText(stmt, 2, outData.password_salt, sizeof(outData.password_salt));
        CopyColumnText(stmt, 3, outData.email, sizeof(outData.email));
        CopyColumnText(stmt, 4, outData.created_at, sizeof(outData.created_at));
        CopyColumnText(stmt, 5, outData.password_changed_at, sizeof(outData.password_changed_at));
        CopyColumnText(stmt, 6, outData.last_ip, sizeof(outData.last_ip));
        ok = true;
    }

    sqlite3_finalize(stmt);
    return ok;
}

bool UpdateAccountPassword(sqlite3* db, const char* account_name, const char* passwordHash, const char* passwordSalt)
{
    if (db == nullptr || account_name == nullptr || passwordHash == nullptr || passwordSalt == nullptr) {
        return false;
    }

    hb::time::local_time sysTime{};
    sysTime = hb::time::local_time::now();
    char timestamp[32] = {};
    hb::time::format_timestamp(sysTime, timestamp, sizeof(timestamp));

    const char* sql =
        "UPDATE accounts SET password_hash = ?, password_salt = ?, password_changed_at = ? WHERE account_name = ? COLLATE NOCASE;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    bool ok = true;
    ok &= PrepareAndBindText(&stmt, 1, passwordHash);
    ok &= PrepareAndBindText(&stmt, 2, passwordSalt);
    ok &= PrepareAndBindText(&stmt, 3, timestamp);
    ok &= PrepareAndBindText(&stmt, 4, account_name);

    if (ok) {
        ok = sqlite3_step(stmt) == SQLITE_DONE;
    }

    sqlite3_finalize(stmt);
    return ok;
}

bool ListCharacterSummaries(sqlite3* db, const char* account_name, std::vector<AccountDbCharacterSummary>& outChars)
{
    if (db == nullptr || account_name == nullptr) {
        return false;
    }

    const char* sql =
        "SELECT character_name, underwear_type, hair_color, hair_style, skin_color, gender, skin, level, exp, map_name "
        "FROM characters WHERE account_name = ? COLLATE NOCASE ORDER BY character_name;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    PrepareAndBindText(&stmt, 1, account_name);
    outChars.clear();
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        AccountDbCharacterSummary row = {};
        CopyColumnText(stmt, 0, row.character_name, sizeof(row.character_name));
        row.appearance.underwear_type = static_cast<uint8_t>(sqlite3_column_int(stmt, 1));
        row.appearance.hair_color = static_cast<uint8_t>(sqlite3_column_int(stmt, 2));
        row.appearance.hair_style = static_cast<uint8_t>(sqlite3_column_int(stmt, 3));
        row.appearance.skin_color = static_cast<uint8_t>(sqlite3_column_int(stmt, 4));
        row.sex = static_cast<uint16_t>(sqlite3_column_int(stmt, 5));
        row.skin = static_cast<uint16_t>(sqlite3_column_int(stmt, 6));
        row.level = static_cast<uint16_t>(sqlite3_column_int(stmt, 7));
        row.exp = static_cast<uint32_t>(sqlite3_column_int(stmt, 8));
        CopyColumnText(stmt, 9, row.map_name, sizeof(row.map_name));
        outChars.push_back(row);
    }

    sqlite3_finalize(stmt);
    return true;
}

bool LoadCharacterState(sqlite3* db, const char* character_name, AccountDbCharacterState& outState)
{
    if (db == nullptr || character_name == nullptr) {
        return false;
    }

    const char* sql =
        "SELECT account_name, character_name, profile, location, "
        "map_name, map_x, map_y, hp, mp, sp, level, rating, str, intl, vit, dex, mag, chr, luck, exp, "
        "lu_pool, enemy_kill_count, pk_count, reward_gold, downskill_index, id_num1, id_num2, id_num3, "
        "gender, skin, hairstyle, haircolor, underwear, hunger_status, timeleft_rating, "
        "timeleft_force_recall, timeleft_firm_staminar, penalty_block_year, "
        "penalty_block_month, penalty_block_day, quest_number, quest_id, current_quest_count, "
        "quest_reward_type, quest_reward_amount, contribution, war_contribution, quest_completed, "
        "special_event_id, super_attack_left, "
        "special_ability_time, locked_map_name, locked_map_time, crusade_job, crusade_guid, "
        "construct_point, dead_penalty_time, party_id, gizon_item_upgrade_left, "
        "underwear_type, hair_color, hair_style, skin_color "
        "FROM characters WHERE character_name = ? COLLATE NOCASE;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    PrepareAndBindText(&stmt, 1, character_name);
    bool ok = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        std::memset(&outState, 0, sizeof(outState));
        int col = 0;
        CopyColumnText(stmt, col++, outState.account_name, sizeof(outState.account_name));
        CopyColumnText(stmt, col++, outState.character_name, sizeof(outState.character_name));
        CopyColumnText(stmt, col++, outState.profile, sizeof(outState.profile));
        CopyColumnText(stmt, col++, outState.location, sizeof(outState.location));
        CopyColumnText(stmt, col++, outState.map_name, sizeof(outState.map_name));
        outState.map_x = sqlite3_column_int(stmt, col++);
        outState.map_y = sqlite3_column_int(stmt, col++);
        outState.hp = sqlite3_column_int(stmt, col++);
        outState.mp = sqlite3_column_int(stmt, col++);
        outState.sp = sqlite3_column_int(stmt, col++);
        outState.level = sqlite3_column_int(stmt, col++);
        outState.rating = sqlite3_column_int(stmt, col++);
        outState.str = sqlite3_column_int(stmt, col++);
        outState.intl = sqlite3_column_int(stmt, col++);
        outState.vit = sqlite3_column_int(stmt, col++);
        outState.dex = sqlite3_column_int(stmt, col++);
        outState.mag = sqlite3_column_int(stmt, col++);
        outState.chr = sqlite3_column_int(stmt, col++);
        outState.luck = sqlite3_column_int(stmt, col++);
        outState.exp = static_cast<uint32_t>(sqlite3_column_int(stmt, col++));
        outState.lu_pool = sqlite3_column_int(stmt, col++);
        outState.enemy_kill_count = sqlite3_column_int(stmt, col++);
        outState.pk_count = sqlite3_column_int(stmt, col++);
        outState.reward_gold = static_cast<uint32_t>(sqlite3_column_int(stmt, col++));
        outState.down_skill_index = sqlite3_column_int(stmt, col++);
        outState.id_num1 = sqlite3_column_int(stmt, col++);
        outState.id_num2 = sqlite3_column_int(stmt, col++);
        outState.id_num3 = sqlite3_column_int(stmt, col++);
        outState.sex = sqlite3_column_int(stmt, col++);
        outState.skin = sqlite3_column_int(stmt, col++);
        outState.hair_style = sqlite3_column_int(stmt, col++);
        outState.hair_color = sqlite3_column_int(stmt, col++);
        outState.underwear = sqlite3_column_int(stmt, col++);
        outState.hunger_status = sqlite3_column_int(stmt, col++);
        outState.timeleft_rating = sqlite3_column_int(stmt, col++);
        outState.timeleft_force_recall = sqlite3_column_int(stmt, col++);
        outState.timeleft_firm_stamina = sqlite3_column_int(stmt, col++);
        outState.penalty_block_year = sqlite3_column_int(stmt, col++);
        outState.penalty_block_month = sqlite3_column_int(stmt, col++);
        outState.penalty_block_day = sqlite3_column_int(stmt, col++);
        outState.quest_number = sqlite3_column_int(stmt, col++);
        outState.quest_id = sqlite3_column_int(stmt, col++);
        outState.current_quest_count = sqlite3_column_int(stmt, col++);
        outState.quest_reward_type = sqlite3_column_int(stmt, col++);
        outState.quest_reward_amount = sqlite3_column_int(stmt, col++);
        outState.contribution = sqlite3_column_int(stmt, col++);
        outState.war_contribution = sqlite3_column_int(stmt, col++);
        outState.quest_completed = sqlite3_column_int(stmt, col++);
        outState.special_event_id = sqlite3_column_int(stmt, col++);
        outState.super_attack_left = sqlite3_column_int(stmt, col++);
        outState.special_ability_time = sqlite3_column_int(stmt, col++);
        CopyColumnText(stmt, col++, outState.locked_map_name, sizeof(outState.locked_map_name));
        outState.locked_map_time = sqlite3_column_int(stmt, col++);
        outState.crusade_job = sqlite3_column_int(stmt, col++);
        outState.crusade_guid = static_cast<uint32_t>(sqlite3_column_int(stmt, col++));
        outState.construct_point = sqlite3_column_int(stmt, col++);
        outState.dead_penalty_time = sqlite3_column_int(stmt, col++);
        outState.party_id = sqlite3_column_int(stmt, col++);
        outState.gizon_item_upgrade_left = sqlite3_column_int(stmt, col++);
        outState.appearance.underwear_type = static_cast<uint8_t>(sqlite3_column_int(stmt, col++));
        outState.appearance.hair_color = static_cast<uint8_t>(sqlite3_column_int(stmt, col++));
        outState.appearance.hair_style = static_cast<uint8_t>(sqlite3_column_int(stmt, col++));
        outState.appearance.skin_color = static_cast<uint8_t>(sqlite3_column_int(stmt, col++));
        ok = true;
    }

    sqlite3_finalize(stmt);
    return ok;
}

bool LoadCharacterItems(sqlite3* db, const char* character_name, std::vector<AccountDbItemRow>& outItems)
{
    if (db == nullptr || character_name == nullptr) {
        return false;
    }

    const char* sql =
        "SELECT slot, item_id, count, touch_effect_type, touch_effect_value1, touch_effect_value2, "
        "touch_effect_value3, item_color, spec_effect_value1, spec_effect_value2, spec_effect_value3, "
        "cur_durability, custom_made, prefix_type, prefix_value, secondary_type, secondary_value, "
        "enchant_bonus, pos_x, pos_y, is_equipped "
        "FROM character_items WHERE character_name = ? COLLATE NOCASE ORDER BY slot;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    PrepareAndBindText(&stmt, 1, character_name);
    outItems.clear();
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        AccountDbItemRow row = {};
        int col = 0;
        row.slot = sqlite3_column_int(stmt, col++);
        row.item_id = sqlite3_column_int(stmt, col++);
        row.count = sqlite3_column_int64(stmt, col++);
        row.touch_effect_type = sqlite3_column_int(stmt, col++);
        row.touch_effect_value1 = sqlite3_column_int(stmt, col++);
        row.touch_effect_value2 = sqlite3_column_int(stmt, col++);
        row.touch_effect_value3 = sqlite3_column_int(stmt, col++);
        row.item_color = sqlite3_column_int(stmt, col++);
        row.spec_effect_value1 = sqlite3_column_int(stmt, col++);
        row.spec_effect_value2 = sqlite3_column_int(stmt, col++);
        row.spec_effect_value3 = sqlite3_column_int(stmt, col++);
        row.cur_durability = sqlite3_column_int(stmt, col++);
        row.custom_made = sqlite3_column_int(stmt, col++);
        row.prefix_type = sqlite3_column_int(stmt, col++);
        row.prefix_value = sqlite3_column_int(stmt, col++);
        row.secondary_type = sqlite3_column_int(stmt, col++);
        row.secondary_value = sqlite3_column_int(stmt, col++);
        row.enchant_bonus = sqlite3_column_int(stmt, col++);
        row.pos_x = sqlite3_column_int(stmt, col++);
        row.pos_y = sqlite3_column_int(stmt, col++);
        row.is_equipped = sqlite3_column_int(stmt, col++);
        outItems.push_back(row);
    }

    sqlite3_finalize(stmt);
    return true;
}

bool LoadCharacterBankItems(sqlite3* db, const char* character_name, std::vector<AccountDbBankItemRow>& outItems)
{
    if (db == nullptr || character_name == nullptr) {
        return false;
    }

    const char* sql =
        "SELECT slot, item_id, count, touch_effect_type, touch_effect_value1, touch_effect_value2, "
        "touch_effect_value3, item_color, spec_effect_value1, spec_effect_value2, spec_effect_value3, "
        "cur_durability, custom_made, prefix_type, prefix_value, secondary_type, secondary_value, "
        "enchant_bonus "
        "FROM character_bank_items WHERE character_name = ? COLLATE NOCASE ORDER BY slot;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    PrepareAndBindText(&stmt, 1, character_name);
    outItems.clear();
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        AccountDbBankItemRow row = {};
        int col = 0;
        row.slot = sqlite3_column_int(stmt, col++);
        row.item_id = sqlite3_column_int(stmt, col++);
        row.count = sqlite3_column_int64(stmt, col++);
        row.touch_effect_type = sqlite3_column_int(stmt, col++);
        row.touch_effect_value1 = sqlite3_column_int(stmt, col++);
        row.touch_effect_value2 = sqlite3_column_int(stmt, col++);
        row.touch_effect_value3 = sqlite3_column_int(stmt, col++);
        row.item_color = sqlite3_column_int(stmt, col++);
        row.spec_effect_value1 = sqlite3_column_int(stmt, col++);
        row.spec_effect_value2 = sqlite3_column_int(stmt, col++);
        row.spec_effect_value3 = sqlite3_column_int(stmt, col++);
        row.cur_durability = sqlite3_column_int(stmt, col++);
        row.custom_made = sqlite3_column_int(stmt, col++);
        row.prefix_type = sqlite3_column_int(stmt, col++);
        row.prefix_value = sqlite3_column_int(stmt, col++);
        row.secondary_type = sqlite3_column_int(stmt, col++);
        row.secondary_value = sqlite3_column_int(stmt, col++);
        row.enchant_bonus = sqlite3_column_int(stmt, col++);
        outItems.push_back(row);
    }

    sqlite3_finalize(stmt);
    return true;
}

bool LoadCharacterItemPositions(sqlite3* db, const char* character_name, std::vector<AccountDbIndexedValue>& outPositionsX, std::vector<AccountDbIndexedValue>& outPositionsY)
{
    if (db == nullptr || character_name == nullptr) {
        return false;
    }

    const char* sql =
        "SELECT slot, pos_x, pos_y FROM character_item_positions WHERE character_name = ? COLLATE NOCASE ORDER BY slot;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    PrepareAndBindText(&stmt, 1, character_name);
    outPositionsX.clear();
    outPositionsY.clear();
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        AccountDbIndexedValue pos_x = {};
        AccountDbIndexedValue pos_y = {};
        pos_x.index = sqlite3_column_int(stmt, 0);
        pos_x.value = sqlite3_column_int(stmt, 1);
        pos_y.index = pos_x.index;
        pos_y.value = sqlite3_column_int(stmt, 2);
        outPositionsX.push_back(pos_x);
        outPositionsY.push_back(pos_y);
    }

    sqlite3_finalize(stmt);
    return true;
}

bool LoadCharacterItemEquips(sqlite3* db, const char* character_name, std::vector<AccountDbIndexedValue>& outEquips)
{
    if (db == nullptr || character_name == nullptr) {
        return false;
    }

    const char* sql =
        "SELECT slot, is_equipped FROM character_item_equips WHERE character_name = ? COLLATE NOCASE ORDER BY slot;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    PrepareAndBindText(&stmt, 1, character_name);
    outEquips.clear();
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        AccountDbIndexedValue equip = {};
        equip.index = sqlite3_column_int(stmt, 0);
        equip.value = sqlite3_column_int(stmt, 1);
        outEquips.push_back(equip);
    }

    sqlite3_finalize(stmt);
    return true;
}

bool LoadCharacterMagicMastery(sqlite3* db, const char* character_name, std::vector<AccountDbIndexedValue>& outMastery)
{
    if (db == nullptr || character_name == nullptr) {
        return false;
    }

    const char* sql =
        "SELECT magic_index, mastery_value FROM character_magic_mastery WHERE character_name = ? COLLATE NOCASE ORDER BY magic_index;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    PrepareAndBindText(&stmt, 1, character_name);
    outMastery.clear();
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        AccountDbIndexedValue row = {};
        row.index = sqlite3_column_int(stmt, 0);
        row.value = sqlite3_column_int(stmt, 1);
        outMastery.push_back(row);
    }

    sqlite3_finalize(stmt);
    return true;
}

bool LoadCharacterSkillMastery(sqlite3* db, const char* character_name, std::vector<AccountDbIndexedValue>& outMastery)
{
    if (db == nullptr || character_name == nullptr) {
        return false;
    }

    const char* sql =
        "SELECT skill_index, mastery_value FROM character_skill_mastery WHERE character_name = ? COLLATE NOCASE ORDER BY skill_index;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    PrepareAndBindText(&stmt, 1, character_name);
    outMastery.clear();
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        AccountDbIndexedValue row = {};
        row.index = sqlite3_column_int(stmt, 0);
        row.value = sqlite3_column_int(stmt, 1);
        outMastery.push_back(row);
    }

    sqlite3_finalize(stmt);
    return true;
}

bool LoadCharacterSkillSSN(sqlite3* db, const char* character_name, std::vector<AccountDbIndexedValue>& outValues)
{
    if (db == nullptr || character_name == nullptr) {
        return false;
    }

    const char* sql =
        "SELECT skill_index, ssn_value FROM character_skill_ssn WHERE character_name = ? COLLATE NOCASE ORDER BY skill_index;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    PrepareAndBindText(&stmt, 1, character_name);
    outValues.clear();
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        AccountDbIndexedValue row = {};
        row.index = sqlite3_column_int(stmt, 0);
        row.value = sqlite3_column_int(stmt, 1);
        outValues.push_back(row);
    }

    sqlite3_finalize(stmt);
    return true;
}

bool LoadEquippedItemAppearances(sqlite3* db, const char* character_name, std::vector<AccountDbEquippedItem>& outItems)
{
    if (db == nullptr || character_name == nullptr) {
        return false;
    }

    const char* sql = "SELECT item_id, item_color FROM character_items WHERE character_name = ? COLLATE NOCASE AND is_equipped = 1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    PrepareAndBindText(&stmt, 1, character_name);
    outItems.clear();
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        AccountDbEquippedItem item = {};
        item.item_id = sqlite3_column_int(stmt, 0);
        item.item_color = sqlite3_column_int(stmt, 1);
        outItems.push_back(item);
    }

    sqlite3_finalize(stmt);
    return true;
}

bool InsertCharacterState(sqlite3* db, const AccountDbCharacterState& state)
{
    if (db == nullptr) {
        return false;
    }

    hb::time::local_time sysTime{};
    sysTime = hb::time::local_time::now();
    char timestamp[32] = {};
    hb::time::format_timestamp(sysTime, timestamp, sizeof(timestamp));

    const char* sql =
        "INSERT INTO characters("
        " account_name, character_name, created_at, profile, location, "
        " map_name, map_x, map_y, hp, mp, sp, level, rating, str, intl, vit, dex, mag, chr, luck, exp, "
        " lu_pool, enemy_kill_count, pk_count, reward_gold, downskill_index, id_num1, id_num2, id_num3, "
        " gender, skin, hairstyle, haircolor, underwear, hunger_status, timeleft_rating, "
        " timeleft_force_recall, timeleft_firm_staminar, penalty_block_year, "
        " penalty_block_month, penalty_block_day, quest_number, quest_id, current_quest_count, "
        " quest_reward_type, quest_reward_amount, contribution, war_contribution, quest_completed, "
        " special_event_id, super_attack_left, "
        " special_ability_time, locked_map_name, locked_map_time, crusade_job, crusade_guid, "
        " construct_point, dead_penalty_time, party_id, gizon_item_upgrade_left,"
        " underwear_type, hair_color, hair_style, skin_color"
        ") VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    int col = 1;
    bool ok = true;
    ok &= PrepareAndBindText(stmt, col++, state.account_name);
    ok &= PrepareAndBindText(stmt, col++, state.character_name);
    ok &= PrepareAndBindText(stmt, col++, timestamp);
    ok &= PrepareAndBindText(stmt, col++, state.profile);
    ok &= PrepareAndBindText(stmt, col++, state.location);
    ok &= PrepareAndBindText(stmt, col++, state.map_name);
    ok &= (sqlite3_bind_int(stmt, col++, state.map_x) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, col++, state.map_y) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, col++, state.hp) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, col++, state.mp) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, col++, state.sp) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, col++, state.level) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, col++, state.rating) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, col++, state.str) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, col++, state.intl) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, col++, state.vit) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, col++, state.dex) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, col++, state.mag) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, col++, state.chr) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, col++, state.luck) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, col++, static_cast<int>(state.exp)) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, col++, state.lu_pool) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, col++, state.enemy_kill_count) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, col++, state.pk_count) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, col++, static_cast<int>(state.reward_gold)) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, col++, state.down_skill_index) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, col++, state.id_num1) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, col++, state.id_num2) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, col++, state.id_num3) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, col++, state.sex) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, col++, state.skin) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, col++, state.hair_style) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, col++, state.hair_color) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, col++, state.underwear) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, col++, state.hunger_status) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, col++, state.timeleft_rating) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, col++, state.timeleft_force_recall) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, col++, state.timeleft_firm_stamina) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, col++, state.penalty_block_year) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, col++, state.penalty_block_month) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, col++, state.penalty_block_day) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, col++, state.quest_number) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, col++, state.quest_id) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, col++, state.current_quest_count) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, col++, state.quest_reward_type) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, col++, state.quest_reward_amount) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, col++, state.contribution) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, col++, state.war_contribution) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, col++, state.quest_completed) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, col++, state.special_event_id) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, col++, state.super_attack_left) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, col++, state.special_ability_time) == SQLITE_OK);
    ok &= PrepareAndBindText(stmt, col++, state.locked_map_name);
    ok &= (sqlite3_bind_int(stmt, col++, state.locked_map_time) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, col++, state.crusade_job) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, col++, static_cast<int>(state.crusade_guid)) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, col++, state.construct_point) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, col++, state.dead_penalty_time) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, col++, state.party_id) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, col++, state.gizon_item_upgrade_left) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, col++, state.appearance.underwear_type) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, col++, state.appearance.hair_color) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, col++, state.appearance.hair_style) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, col++, state.appearance.skin_color) == SQLITE_OK);

    if (ok) {
        ok = sqlite3_step(stmt) == SQLITE_DONE;
    }

    sqlite3_finalize(stmt);
    return ok;
}

bool InsertCharacterItems(sqlite3* db, const char* character_name, const std::vector<AccountDbItemRow>& items)
{
    if (db == nullptr || character_name == nullptr) {
        return false;
    }

    const char* sql =
        "INSERT INTO character_items("
        " character_name, slot, item_id, count, touch_effect_type, touch_effect_value1, touch_effect_value2,"
        " touch_effect_value3, item_color, spec_effect_value1, spec_effect_value2, spec_effect_value3,"
        " cur_durability, custom_made, prefix_type, prefix_value, secondary_type, secondary_value,"
        " enchant_bonus, pos_x, pos_y, is_equipped"
        ") VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    for (const auto& item : items) {
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
        int col = 1;
        bool ok = true;
        ok &= PrepareAndBindText(stmt, col++, character_name);
        ok &= (sqlite3_bind_int(stmt, col++, item.slot) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, item.item_id) == SQLITE_OK);
        ok &= (sqlite3_bind_int64(stmt, col++, item.count) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, item.touch_effect_type) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, item.touch_effect_value1) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, item.touch_effect_value2) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, item.touch_effect_value3) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, item.item_color) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, item.spec_effect_value1) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, item.spec_effect_value2) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, item.spec_effect_value3) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, item.cur_durability) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, item.custom_made) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, item.prefix_type) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, item.prefix_value) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, item.secondary_type) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, item.secondary_value) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, item.enchant_bonus) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, item.pos_x) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, item.pos_y) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, item.is_equipped) == SQLITE_OK);
        if (!ok || sqlite3_step(stmt) != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            return false;
        }
    }

    sqlite3_finalize(stmt);
    return true;
}

bool InsertCharacterBankItems(sqlite3* db, const char* character_name, const std::vector<AccountDbBankItemRow>& items)
{
    if (db == nullptr || character_name == nullptr) {
        return false;
    }

    const char* sql =
        "INSERT INTO character_bank_items("
        " character_name, slot, item_id, count, touch_effect_type, touch_effect_value1, touch_effect_value2,"
        " touch_effect_value3, item_color, spec_effect_value1, spec_effect_value2, spec_effect_value3,"
        " cur_durability, custom_made, prefix_type, prefix_value, secondary_type, secondary_value,"
        " enchant_bonus"
        ") VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    for (const auto& item : items) {
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
        int col = 1;
        bool ok = true;
        ok &= PrepareAndBindText(stmt, col++, character_name);
        ok &= (sqlite3_bind_int(stmt, col++, item.slot) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, item.item_id) == SQLITE_OK);
        ok &= (sqlite3_bind_int64(stmt, col++, item.count) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, item.touch_effect_type) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, item.touch_effect_value1) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, item.touch_effect_value2) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, item.touch_effect_value3) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, item.item_color) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, item.spec_effect_value1) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, item.spec_effect_value2) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, item.spec_effect_value3) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, item.cur_durability) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, item.custom_made) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, item.prefix_type) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, item.prefix_value) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, item.secondary_type) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, item.secondary_value) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, item.enchant_bonus) == SQLITE_OK);
        if (!ok || sqlite3_step(stmt) != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            return false;
        }
    }

    sqlite3_finalize(stmt);
    return true;
}

bool InsertCharacterItemPositions(sqlite3* db, const char* character_name, const std::vector<AccountDbIndexedValue>& positionsX, const std::vector<AccountDbIndexedValue>& positionsY)
{
    if (db == nullptr || character_name == nullptr || positionsX.size() != positionsY.size()) {
        return false;
    }

    const char* sql =
        "INSERT INTO character_item_positions(character_name, slot, pos_x, pos_y) VALUES(?,?,?,?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    for (size_t i = 0; i < positionsX.size(); i++) {
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
        bool ok = true;
        ok &= PrepareAndBindText(stmt, 1, character_name);
        ok &= (sqlite3_bind_int(stmt, 2, positionsX[i].index) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, 3, positionsX[i].value) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, 4, positionsY[i].value) == SQLITE_OK);
        if (!ok || sqlite3_step(stmt) != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            return false;
        }
    }

    sqlite3_finalize(stmt);
    return true;
}

bool InsertCharacterItemEquips(sqlite3* db, const char* character_name, const std::vector<AccountDbIndexedValue>& equips)
{
    if (db == nullptr || character_name == nullptr) {
        return false;
    }

    const char* sql =
        "INSERT INTO character_item_equips(character_name, slot, is_equipped) VALUES(?,?,?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    for (const auto& equip : equips) {
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
        bool ok = true;
        ok &= PrepareAndBindText(stmt, 1, character_name);
        ok &= (sqlite3_bind_int(stmt, 2, equip.index) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, 3, equip.value) == SQLITE_OK);
        if (!ok || sqlite3_step(stmt) != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            return false;
        }
    }

    sqlite3_finalize(stmt);
    return true;
}

bool InsertCharacterMagicMastery(sqlite3* db, const char* character_name, const std::vector<AccountDbIndexedValue>& mastery)
{
    if (db == nullptr || character_name == nullptr) {
        return false;
    }

    const char* sql =
        "INSERT INTO character_magic_mastery(character_name, magic_index, mastery_value) VALUES(?,?,?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    for (const auto& entry : mastery) {
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
        bool ok = true;
        ok &= PrepareAndBindText(stmt, 1, character_name);
        ok &= (sqlite3_bind_int(stmt, 2, entry.index) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, 3, entry.value) == SQLITE_OK);
        if (!ok || sqlite3_step(stmt) != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            return false;
        }
    }

    sqlite3_finalize(stmt);
    return true;
}

bool InsertCharacterSkillMastery(sqlite3* db, const char* character_name, const std::vector<AccountDbIndexedValue>& mastery)
{
    if (db == nullptr || character_name == nullptr) {
        return false;
    }

    const char* sql =
        "INSERT INTO character_skill_mastery(character_name, skill_index, mastery_value) VALUES(?,?,?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    for (const auto& entry : mastery) {
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
        bool ok = true;
        ok &= PrepareAndBindText(stmt, 1, character_name);
        ok &= (sqlite3_bind_int(stmt, 2, entry.index) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, 3, entry.value) == SQLITE_OK);
        if (!ok || sqlite3_step(stmt) != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            return false;
        }
    }

    sqlite3_finalize(stmt);
    return true;
}

bool InsertCharacterSkillSSN(sqlite3* db, const char* character_name, const std::vector<AccountDbIndexedValue>& values)
{
    if (db == nullptr || character_name == nullptr) {
        return false;
    }

    const char* sql =
        "INSERT INTO character_skill_ssn(character_name, skill_index, ssn_value) VALUES(?,?,?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    for (const auto& entry : values) {
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
        bool ok = true;
        ok &= PrepareAndBindText(stmt, 1, character_name);
        ok &= (sqlite3_bind_int(stmt, 2, entry.index) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, 3, entry.value) == SQLITE_OK);
        if (!ok || sqlite3_step(stmt) != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            return false;
        }
    }

    sqlite3_finalize(stmt);
    return true;
}

bool InsertAccountRecord(sqlite3* db, const AccountDbAccountData& data)
{
    if (db == nullptr) {
        return false;
    }

    const char* sql =
        "INSERT INTO accounts("
        " account_name, password_hash, password_salt, email, created_at, password_changed_at, last_ip"
        ") VALUES(?,?,?,?,?,?,?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        char logMsg[512] = {};
        hb::logger::error("SQLite: account insert prepare failed: {}", sqlite3_errmsg(db));
        return false;
    }

    bool ok =
        PrepareAndBindText(&stmt, 1, data.name) &&
        PrepareAndBindText(&stmt, 2, data.password_hash) &&
        PrepareAndBindText(&stmt, 3, data.password_salt) &&
        PrepareAndBindText(&stmt, 4, data.email) &&
        PrepareAndBindText(&stmt, 5, data.created_at) &&
        PrepareAndBindText(&stmt, 6, data.password_changed_at) &&
        PrepareAndBindText(&stmt, 7, data.last_ip);

    if (ok) {
        ok = sqlite3_step(stmt) == SQLITE_DONE;
    }

    if (!ok) {
        char logMsg[512] = {};
        hb::logger::error("SQLite: account insert failed: {}", sqlite3_errmsg(db));
    }

    sqlite3_finalize(stmt);
    return ok;
}

bool InsertCharacterRecord(sqlite3* db, const AccountDbCharacterData& data)
{
    if (db == nullptr) {
        return false;
    }

    const char* sql =
        "INSERT INTO characters("
        " character_name, account_name, created_at,"
        " underwear_type, hair_color, hair_style, skin_color,"
        " level, exp, map_name, map_x, map_y, hp, mp, sp, str, vit, dex, intl, mag, chr,"
        " gender, skin, hairstyle, haircolor, underwear"
        ") VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        char logMsg[512] = {};
        hb::logger::error("SQLite: character insert prepare failed: {}", sqlite3_errmsg(db));
        return false;
    }

    int idx = 1;
    bool ok = true;
    ok &= PrepareAndBindText(&stmt, idx++, data.character_name);
    ok &= PrepareAndBindText(&stmt, idx++, data.account_name);
    ok &= PrepareAndBindText(&stmt, idx++, data.created_at);
    ok &= (sqlite3_bind_int(stmt, idx++, data.appearance.underwear_type) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, data.appearance.hair_color) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, data.appearance.hair_style) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, data.appearance.skin_color) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, data.level) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, static_cast<int>(data.exp)) == SQLITE_OK);
    ok &= PrepareAndBindText(&stmt, idx++, data.map_name);
    ok &= (sqlite3_bind_int(stmt, idx++, data.map_x) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, data.map_y) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, data.hp) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, data.mp) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, data.sp) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, data.str) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, data.vit) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, data.dex) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, data.intl) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, data.mag) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, data.chr) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, data.gender) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, data.skin) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, data.hair_style) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, data.hair_color) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, data.underwear) == SQLITE_OK);

    if (ok) {
        ok = sqlite3_step(stmt) == SQLITE_DONE;
    }

    if (!ok) {
        char logMsg[512] = {};
        hb::logger::error("SQLite: character insert failed: {}", sqlite3_errmsg(db));
    }

    sqlite3_finalize(stmt);
    return ok;
}

bool DeleteCharacterData(sqlite3* db, const char* character_name)
{
    if (db == nullptr || character_name == nullptr) {
        return false;
    }

    const char* sql = "DELETE FROM characters WHERE character_name = ? COLLATE NOCASE;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    PrepareAndBindText(&stmt, 1, character_name);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

void CloseAccountDatabase(sqlite3* db)
{
    if (db != nullptr) {
        sqlite3_close(db);
    }
}

bool SaveCharacterSnapshot(sqlite3* db, const CClient* client)
{
    if (db == nullptr || client == nullptr) {
        hb::logger::error("SQLite: save snapshot failed, null db or client");
        return false;
    }

    // Helper: capture the real SQLite error before ROLLBACK clears it
    auto FailAndRollback = [&](const char* stage) {
        char logMsg[512] = {};
        hb::logger::error("SQLite: save snapshot failed at [{}]: {}", stage, sqlite3_errmsg(db));
        ExecSql(db, "ROLLBACK;");
    };

    if (!ExecSql(db, "BEGIN;")) {
        hb::logger::error("SQLite: save snapshot BEGIN failed");
        return false;
    }

    hb::time::local_time sysTime{};
    sysTime = hb::time::local_time::now();
    char timestamp[32] = {};
    hb::time::format_timestamp(sysTime, timestamp, sizeof(timestamp));

    const char* upsertSql =
        "INSERT OR REPLACE INTO characters("
        " account_name, character_name, created_at, profile, location,"
        " map_name, map_x, map_y, hp, mp, sp, level, rating, str, intl, vit, dex, mag, chr, luck, exp,"
        " lu_pool, enemy_kill_count, pk_count, reward_gold, downskill_index, id_num1, id_num2, id_num3,"
        " gender, skin, hairstyle, haircolor, underwear, hunger_status, timeleft_rating,"
        " timeleft_force_recall, timeleft_firm_staminar, penalty_block_year,"
        " penalty_block_month, penalty_block_day, quest_number, quest_id, current_quest_count,"
        " quest_reward_type, quest_reward_amount, contribution, war_contribution, quest_completed,"
        " special_event_id, super_attack_left,"
        " special_ability_time, locked_map_name, locked_map_time, crusade_job, crusade_guid,"
        " construct_point, dead_penalty_time, party_id, gizon_item_upgrade_left,"
        " underwear_type, hair_color, hair_style, skin_color"
        ") VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, upsertSql, -1, &stmt, nullptr) != SQLITE_OK) {
        char logMsg[512] = {};
        hb::logger::error("SQLite: save snapshot upsert prepare failed: {}", sqlite3_errmsg(db));
        ExecSql(db, "ROLLBACK;");
        return false;
    }

    int idx = 1;
    bool ok = true;
    ok &= PrepareAndBindText(stmt, idx++, client->m_account_name);
    ok &= PrepareAndBindText(stmt, idx++, client->m_char_name);
    ok &= PrepareAndBindText(stmt, idx++, timestamp);
    ok &= PrepareAndBindText(stmt, idx++, client->m_profile);
    ok &= PrepareAndBindText(stmt, idx++, client->m_location);
    ok &= PrepareAndBindText(stmt, idx++, client->m_map_name);
    ok &= (sqlite3_bind_int(stmt, idx++, client->m_x) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, client->m_y) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, client->m_hp) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, client->m_mp) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, client->m_sp) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, client->m_level) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, client->m_rating) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, client->m_str) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, client->m_int) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, client->m_vit) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, client->m_dex) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, client->m_mag) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, client->m_charisma) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, client->m_luck) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, static_cast<int>(client->m_exp)) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, client->m_levelup_pool) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, client->m_enemy_kill_count) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, client->m_player_kill_count) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, static_cast<int>(client->m_reward_gold)) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, client->m_down_skill_index) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, client->m_char_id_num1) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, client->m_char_id_num2) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, client->m_char_id_num3) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, client->m_sex) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, client->m_skin) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, client->m_hair_style) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, client->m_hair_color) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, client->m_underwear) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, client->m_hunger_status) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, client->m_time_left_rating) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, client->m_time_left_force_recall) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, client->m_time_left_firm_stamina) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, client->m_penalty_block_year) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, client->m_penalty_block_month) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, client->m_penalty_block_day) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, client->m_quest) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, client->m_quest_id) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, client->m_cur_quest_count) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, client->m_quest_reward_type) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, client->m_quest_reward_amount) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, client->m_contribution) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, client->m_war_contribution) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, client->m_is_quest_completed ? 1 : 0) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, client->m_special_event_id) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, client->m_super_attack_left) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, client->m_special_ability_time) == SQLITE_OK);
    ok &= PrepareAndBindText(stmt, idx++, client->m_locked_map_name);
    ok &= (sqlite3_bind_int(stmt, idx++, client->m_locked_map_time) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, client->m_crusade_duty) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, static_cast<int>(client->m_crusade_guid)) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, client->m_construction_point) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, client->m_dead_penalty_time) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, client->m_party_id) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, client->m_gizon_item_upgrade_left) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, client->m_appearance.underwear_type) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, client->m_appearance.hair_color) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, client->m_appearance.hair_style) == SQLITE_OK);
    ok &= (sqlite3_bind_int(stmt, idx++, client->m_appearance.skin_color) == SQLITE_OK);

    if (!ok) {
        char logMsg[512] = {};
        hb::logger::error("SQLite: save snapshot upsert bind failed at idx {}: {}", idx - 1, sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        ExecSql(db, "ROLLBACK;");
        return false;
    }

    int stepRc = sqlite3_step(stmt);
    if (stepRc != SQLITE_DONE) {
        char logMsg[512] = {};
        hb::logger::error("SQLite: save snapshot upsert step failed (rc={}): {}", stepRc, sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        ExecSql(db, "ROLLBACK;");
        return false;
    }
    sqlite3_finalize(stmt);

    const char* deleteItemsSql = "DELETE FROM character_items WHERE character_name = ? COLLATE NOCASE;";
    const char* deleteBankSql = "DELETE FROM character_bank_items WHERE character_name = ? COLLATE NOCASE;";
    const char* deletePosSql = "DELETE FROM character_item_positions WHERE character_name = ? COLLATE NOCASE;";
    const char* deleteEquipSql = "DELETE FROM character_item_equips WHERE character_name = ? COLLATE NOCASE;";
    const char* deleteMagicSql = "DELETE FROM character_magic_mastery WHERE character_name = ? COLLATE NOCASE;";
    const char* deleteSkillSql = "DELETE FROM character_skill_mastery WHERE character_name = ? COLLATE NOCASE;";
    const char* deleteSsnSql = "DELETE FROM character_skill_ssn WHERE character_name = ? COLLATE NOCASE;";

    const char* deleteStatements[] = {
        deleteItemsSql, deleteBankSql, deletePosSql, deleteEquipSql, deleteMagicSql, deleteSkillSql, deleteSsnSql
    };

    for (const char* deleteSql : deleteStatements) {
        if (sqlite3_prepare_v2(db, deleteSql, -1, &stmt, nullptr) != SQLITE_OK) {
            char logMsg[512] = {};
            hb::logger::error("SQLite: save snapshot delete prepare failed: {} | SQL: {}", sqlite3_errmsg(db), deleteSql);
            ExecSql(db, "ROLLBACK;");
            return false;
        }
        PrepareAndBindText(stmt, 1, client->m_char_name);
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        if (rc != SQLITE_DONE) {
            char logMsg[512] = {};
            hb::logger::error("SQLite: save snapshot delete step failed (rc={}): {} | SQL: {}", rc, sqlite3_errmsg(db), deleteSql);
            ExecSql(db, "ROLLBACK;");
            return false;
        }
    }

    const char* insertItemSql =
        "INSERT INTO character_items("
        " character_name, slot, item_id, count, touch_effect_type, touch_effect_value1, touch_effect_value2,"
        " touch_effect_value3, item_color, spec_effect_value1, spec_effect_value2, spec_effect_value3,"
        " cur_durability, custom_made, prefix_type, prefix_value, secondary_type, secondary_value,"
        " enchant_bonus, pos_x, pos_y, is_equipped"
        ") VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);";

    if (sqlite3_prepare_v2(db, insertItemSql, -1, &stmt, nullptr) != SQLITE_OK) {
        char logMsg[512] = {};
        hb::logger::error("SQLite: save snapshot item insert prepare failed: {}", sqlite3_errmsg(db));
        ExecSql(db, "ROLLBACK;");
        return false;
    }

    for(int i = 0; i < hb::shared::limits::MaxItems; i++) {
        if (client->m_item_list[i] == nullptr) {
            continue;
        }
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
        int col = 1;
        ok = true;
        ok &= PrepareAndBindText(stmt, col++, client->m_char_name);
        ok &= (sqlite3_bind_int(stmt, col++, i) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, client->m_item_list[i]->m_id_num) == SQLITE_OK);
        ok &= (sqlite3_bind_int64(stmt, col++, static_cast<int64_t>(client->m_item_list[i]->m_instance.count)) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, client->m_item_list[i]->m_instance.touch_effect_type) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, client->m_item_list[i]->m_instance.touch_effect_value1) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, client->m_item_list[i]->m_instance.touch_effect_value2) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, client->m_item_list[i]->m_instance.touch_effect_value3) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, client->m_item_list[i]->m_instance.item_color) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, client->m_item_list[i]->m_instance.special_effect_value1) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, client->m_item_list[i]->m_instance.special_effect_value2) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, client->m_item_list[i]->m_instance.special_effect_value3) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, client->m_item_list[i]->m_instance.cur_durability) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, client->m_item_list[i]->m_instance.custom_made ? 1 : 0) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, static_cast<int>(client->m_item_list[i]->m_instance.prefix_type)) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, client->m_item_list[i]->m_instance.prefix_value) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, static_cast<int>(client->m_item_list[i]->m_instance.secondary_type)) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, client->m_item_list[i]->m_instance.secondary_value) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, client->m_item_list[i]->m_instance.enchant_bonus) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, client->m_item_pos_list[i].x) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, client->m_item_pos_list[i].y) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, client->m_is_item_equipped[i] ? 1 : 0) == SQLITE_OK);

        if (ok) {
            int rc = sqlite3_step(stmt);
            ok = rc == SQLITE_DONE;
            if (!ok) {
                char logMsg[512] = {};
                hb::logger::error("SQLite: save snapshot item[{}] step failed (rc={}): {}", i, rc, sqlite3_errmsg(db));
            }
        } else {
            char logMsg[256] = {};
            hb::logger::error("SQLite: save snapshot item[{}] bind failed", i);
        }
        if (!ok) {
            sqlite3_finalize(stmt);
            ExecSql(db, "ROLLBACK;");
            return false;
        }
    }
    sqlite3_finalize(stmt);

    const char* insertBankSql =
        "INSERT INTO character_bank_items("
        " character_name, slot, item_id, count, touch_effect_type, touch_effect_value1, touch_effect_value2,"
        " touch_effect_value3, item_color, spec_effect_value1, spec_effect_value2, spec_effect_value3,"
        " cur_durability, custom_made, prefix_type, prefix_value, secondary_type, secondary_value,"
        " enchant_bonus"
        ") VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);";

    if (sqlite3_prepare_v2(db, insertBankSql, -1, &stmt, nullptr) != SQLITE_OK) {
        FailAndRollback("bank_items prepare");
        return false;
    }

    for(int i = 0; i < hb::shared::limits::MaxBankItems; i++) {
        if (client->m_item_in_bank_list[i] == nullptr) {
            continue;
        }
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
        int col = 1;
        ok = true;
        ok &= PrepareAndBindText(stmt, col++, client->m_char_name);
        ok &= (sqlite3_bind_int(stmt, col++, i) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, client->m_item_in_bank_list[i]->m_id_num) == SQLITE_OK);
        ok &= (sqlite3_bind_int64(stmt, col++, static_cast<int64_t>(client->m_item_in_bank_list[i]->m_instance.count)) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, client->m_item_in_bank_list[i]->m_instance.touch_effect_type) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, client->m_item_in_bank_list[i]->m_instance.touch_effect_value1) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, client->m_item_in_bank_list[i]->m_instance.touch_effect_value2) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, client->m_item_in_bank_list[i]->m_instance.touch_effect_value3) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, client->m_item_in_bank_list[i]->m_instance.item_color) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, client->m_item_in_bank_list[i]->m_instance.special_effect_value1) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, client->m_item_in_bank_list[i]->m_instance.special_effect_value2) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, client->m_item_in_bank_list[i]->m_instance.special_effect_value3) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, client->m_item_in_bank_list[i]->m_instance.cur_durability) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, client->m_item_in_bank_list[i]->m_instance.custom_made ? 1 : 0) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, static_cast<int>(client->m_item_in_bank_list[i]->m_instance.prefix_type)) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, client->m_item_in_bank_list[i]->m_instance.prefix_value) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, static_cast<int>(client->m_item_in_bank_list[i]->m_instance.secondary_type)) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, client->m_item_in_bank_list[i]->m_instance.secondary_value) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, col++, client->m_item_in_bank_list[i]->m_instance.enchant_bonus) == SQLITE_OK);

        if (ok) {
            ok = sqlite3_step(stmt) == SQLITE_DONE;
        }
        if (!ok) {
            sqlite3_finalize(stmt);
            FailAndRollback("bank_items step/bind");
            return false;
        }
    }
    sqlite3_finalize(stmt);

    const char* insertPosSql =
        "INSERT INTO character_item_positions(character_name, slot, pos_x, pos_y)"
        " VALUES(?,?,?,?);";
    if (sqlite3_prepare_v2(db, insertPosSql, -1, &stmt, nullptr) != SQLITE_OK) {
        FailAndRollback("item_positions prepare");
        return false;
    }
    for(int i = 0; i < hb::shared::limits::MaxItems; i++) {
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
        ok = true;
        ok &= PrepareAndBindText(stmt, 1, client->m_char_name);
        ok &= (sqlite3_bind_int(stmt, 2, i) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, 3, client->m_item_pos_list[i].x) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, 4, client->m_item_pos_list[i].y) == SQLITE_OK);
        if (ok) {
            ok = sqlite3_step(stmt) == SQLITE_DONE;
        }
        if (!ok) {
            sqlite3_finalize(stmt);
            FailAndRollback("item_positions step/bind");
            return false;
        }
    }
    sqlite3_finalize(stmt);

    const char* insertEquipSql =
        "INSERT INTO character_item_equips(character_name, slot, is_equipped)"
        " VALUES(?,?,?);";
    if (sqlite3_prepare_v2(db, insertEquipSql, -1, &stmt, nullptr) != SQLITE_OK) {
        FailAndRollback("item_equips prepare");
        return false;
    }
    for(int i = 0; i < hb::shared::limits::MaxItems; i++) {
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
        ok = true;
        ok &= PrepareAndBindText(stmt, 1, client->m_char_name);
        ok &= (sqlite3_bind_int(stmt, 2, i) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, 3, client->m_is_item_equipped[i] ? 1 : 0) == SQLITE_OK);
        if (ok) {
            ok = sqlite3_step(stmt) == SQLITE_DONE;
        }
        if (!ok) {
            sqlite3_finalize(stmt);
            FailAndRollback("item_equips step/bind");
            return false;
        }
    }
    sqlite3_finalize(stmt);

    const char* insertMagicSql =
        "INSERT INTO character_magic_mastery(character_name, magic_index, mastery_value)"
        " VALUES(?,?,?);";
    if (sqlite3_prepare_v2(db, insertMagicSql, -1, &stmt, nullptr) != SQLITE_OK) {
        FailAndRollback("magic_mastery prepare");
        return false;
    }
    for(int i = 0; i < hb::shared::limits::MaxMagicType; i++) {
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
        ok = true;
        ok &= PrepareAndBindText(stmt, 1, client->m_char_name);
        ok &= (sqlite3_bind_int(stmt, 2, i) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, 3, client->m_magic_mastery[i]) == SQLITE_OK);
        if (ok) {
            ok = sqlite3_step(stmt) == SQLITE_DONE;
        }
        if (!ok) {
            sqlite3_finalize(stmt);
            FailAndRollback("magic_mastery step/bind");
            return false;
        }
    }
    sqlite3_finalize(stmt);

    const char* insertSkillSql =
        "INSERT INTO character_skill_mastery(character_name, skill_index, mastery_value)"
        " VALUES(?,?,?);";
    if (sqlite3_prepare_v2(db, insertSkillSql, -1, &stmt, nullptr) != SQLITE_OK) {
        FailAndRollback("skill_mastery prepare");
        return false;
    }
    for(int i = 0; i < hb::shared::limits::MaxSkillType; i++) {
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
        ok = true;
        ok &= PrepareAndBindText(stmt, 1, client->m_char_name);
        ok &= (sqlite3_bind_int(stmt, 2, i) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, 3, client->m_skill_mastery[i]) == SQLITE_OK);
        if (ok) {
            ok = sqlite3_step(stmt) == SQLITE_DONE;
        }
        if (!ok) {
            sqlite3_finalize(stmt);
            FailAndRollback("skill_mastery step/bind");
            return false;
        }
    }
    sqlite3_finalize(stmt);

    const char* insertSsnSql =
        "INSERT INTO character_skill_ssn(character_name, skill_index, ssn_value)"
        " VALUES(?,?,?);";
    if (sqlite3_prepare_v2(db, insertSsnSql, -1, &stmt, nullptr) != SQLITE_OK) {
        FailAndRollback("skill_ssn prepare");
        return false;
    }
    for(int i = 0; i < hb::shared::limits::MaxSkillType; i++) {
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
        ok = true;
        ok &= PrepareAndBindText(stmt, 1, client->m_char_name);
        ok &= (sqlite3_bind_int(stmt, 2, i) == SQLITE_OK);
        ok &= (sqlite3_bind_int(stmt, 3, client->m_skill_progress[i]) == SQLITE_OK);
        if (ok) {
            ok = sqlite3_step(stmt) == SQLITE_DONE;
        }
        if (!ok) {
            sqlite3_finalize(stmt);
            FailAndRollback("skill_ssn step/bind");
            return false;
        }
    }
    sqlite3_finalize(stmt);

    if (!ExecSql(db, "COMMIT;")) {
        FailAndRollback("COMMIT");
        return false;
    }
    return true;
}

bool CharacterNameExistsGlobally(const char* character_name)
{
    if (character_name == nullptr || character_name[0] == '\0') {
        return false;
    }

    std::error_code ec;
    bool found = false;

    for (const auto& entry : std::filesystem::directory_iterator("accounts", ec)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".db")
            continue;

        std::string dbPath = entry.path().string();

        sqlite3* db = nullptr;
        if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
            if (db) sqlite3_close(db);
            continue;
        }

        const char* sql = "SELECT 1 FROM characters WHERE character_name = ? COLLATE NOCASE LIMIT 1";
        sqlite3_stmt* stmt = nullptr;

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            if (sqlite3_bind_text(stmt, 1, character_name, -1, SQLITE_TRANSIENT) == SQLITE_OK) {
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    found = true;
                }
            }
            sqlite3_finalize(stmt);
        }

        sqlite3_close(db);
        if (found) break;
    }

    return found;
}

bool AccountNameExists(const char* account_name)
{
    if (account_name == nullptr || account_name[0] == '\0') {
        return false;
    }

    std::error_code ec;
    bool found = false;

    for (const auto& entry : std::filesystem::directory_iterator("accounts", ec)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".db")
            continue;

        std::string dbPath = entry.path().string();

        sqlite3* db = nullptr;
        if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
            if (db) sqlite3_close(db);
            continue;
        }

        const char* sql = "SELECT 1 FROM accounts WHERE account_name = ? COLLATE NOCASE LIMIT 1";
        sqlite3_stmt* stmt = nullptr;

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            if (sqlite3_bind_text(stmt, 1, account_name, -1, SQLITE_TRANSIENT) == SQLITE_OK) {
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    found = true;
                }
            }
            sqlite3_finalize(stmt);
        }

        sqlite3_close(db);
        if (found) break;
    }

    return found;
}

bool LoadBlockList(sqlite3* db, std::vector<std::pair<std::string, std::string>>& outBlocks)
{
    outBlocks.clear();
    const char* sql = "SELECT blocked_account_name, blocked_character_name FROM block_list";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char* account_name = (const char*)sqlite3_column_text(stmt, 0);
        const char* charName = (const char*)sqlite3_column_text(stmt, 1);
        if (account_name && charName)
            outBlocks.push_back(std::make_pair(std::string(account_name), std::string(charName)));
    }

    sqlite3_finalize(stmt);
    return true;
}

bool SaveBlockList(sqlite3* db, const std::vector<std::pair<std::string, std::string>>& blocks)
{
    if (!ExecSql(db, "DELETE FROM block_list"))
        return false;

    const char* sql = "INSERT INTO block_list (blocked_account_name, blocked_character_name) VALUES (?, ?)";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    for (const auto& entry : blocks)
    {
        sqlite3_reset(stmt);
        sqlite3_bind_text(stmt, 1, entry.first.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, entry.second.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) != SQLITE_DONE)
        {
            sqlite3_finalize(stmt);
            return false;
        }
    }

    sqlite3_finalize(stmt);
    return true;
}

bool ResolveCharacterToAccount(const char* character_name, char* outAccountName, size_t accountNameSize)
{
    if (character_name == nullptr || outAccountName == nullptr || accountNameSize == 0)
        return false;

    std::error_code ec;
    bool found = false;

    for (const auto& entry : std::filesystem::directory_iterator("accounts", ec)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".db")
            continue;

        std::string dbPathStr = entry.path().string();
        const char* dbPath = dbPathStr.c_str();

        sqlite3* db = nullptr;
        if (sqlite3_open(dbPath, &db) != SQLITE_OK)
        {
            if (db) sqlite3_close(db);
            continue;
        }

        const char* sql = "SELECT account_name FROM characters WHERE character_name = ? COLLATE NOCASE LIMIT 1";
        sqlite3_stmt* stmt = nullptr;

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK)
        {
            if (sqlite3_bind_text(stmt, 1, character_name, -1, SQLITE_TRANSIENT) == SQLITE_OK)
            {
                if (sqlite3_step(stmt) == SQLITE_ROW)
                {
                    const char* acctName = (const char*)sqlite3_column_text(stmt, 0);
                    if (acctName)
                    {
                        std::strncpy(outAccountName, acctName, accountNameSize - 1);
                        outAccountName[accountNameSize - 1] = '\0';
                        found = true;
                    }
                }
            }
            sqlite3_finalize(stmt);
        }

        sqlite3_close(db);

        if (found)
            break;

    }

    return found;
}

account_stats CountAccountStats()
{
    account_stats stats{0, 0};
    std::error_code ec;

    for (const auto& entry : std::filesystem::directory_iterator("accounts", ec)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".db")
            continue;

        stats.accounts++;

        sqlite3* db = nullptr;
        if (sqlite3_open(entry.path().string().c_str(), &db) != SQLITE_OK) {
            if (db) sqlite3_close(db);
            continue;
        }

        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM characters", -1, &stmt, nullptr) == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                int count = sqlite3_column_int(stmt, 0);
                stats.characters += count;
                if (count > hb::shared::limits::MaxCharactersPerAccount) {
                    std::string name = entry.path().stem().string();
                    stats.over_limit.emplace_back(name, count);
                }
            }
            sqlite3_finalize(stmt);
        }

        sqlite3_close(db);
    }

    return stats;
}
