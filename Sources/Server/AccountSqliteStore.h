#pragma once

#include <cstddef>
#include <string>
#include <vector>
#include "Appearance.h"

struct sqlite3;

struct AccountDbAccountData
{
    char name[11];
    char password_hash[65];
    char password_salt[33];
    char email[52];
    char created_at[32];
    char password_changed_at[32];
    char last_ip[32];
};

struct AccountDbCharacterData
{
    char account_name[11];
    char character_name[11];
    char created_at[32];
    hb::shared::entity::PlayerAppearance appearance;
    int level;
    uint32_t exp;
    char map_name[11];
    int map_x;
    int map_y;
    int hp;
    int mp;
    int sp;
    int str;
    int vit;
    int dex;
    int intl;
    int mag;
    int chr;
    int gender;
    int skin;
    int hair_style;
    int hair_color;
    int underwear;
};

struct AccountDbCharacterSummary
{
    char character_name[11];
    hb::shared::entity::PlayerAppearance appearance;
    uint16_t sex;
    uint16_t skin;
    uint16_t level;
    uint32_t exp;
    char map_name[11];
};

struct AccountDbCharacterState
{
    char account_name[11];
    char character_name[11];
    char profile[256];
    char location[11];
    char map_name[11];
    int map_x;
    int map_y;
    int hp;
    int mp;
    int sp;
    int level;
    int rating;
    int str;
    int intl;
    int vit;
    int dex;
    int mag;
    int chr;
    int luck;
    uint32_t exp;
    int lu_pool;
    int enemy_kill_count;
    int pk_count;
    uint32_t reward_gold;
    int down_skill_index;
    int id_num1;
    int id_num2;
    int id_num3;
    int sex;
    int skin;
    int hair_style;
    int hair_color;
    int underwear;
    int hunger_status;
    int timeleft_rating;
    int timeleft_force_recall;
    int timeleft_firm_stamina;
    int penalty_block_year;
    int penalty_block_month;
    int penalty_block_day;
    int quest_number;
    int quest_id;
    int current_quest_count;
    int quest_reward_type;
    int quest_reward_amount;
    int contribution;
    int war_contribution;
    int quest_completed;
    int special_event_id;
    int super_attack_left;
    int special_ability_time;
    char locked_map_name[11];
    int locked_map_time;
    int crusade_job;
    uint32_t crusade_guid;
    int construct_point;
    int dead_penalty_time;
    int party_id;
    int gizon_item_upgrade_left;
    hb::shared::entity::PlayerAppearance appearance;
};

struct AccountDbItemRow
{
    int slot;
    int item_id;
    int64_t count;
    int touch_effect_type;
    int touch_effect_value1;
    int touch_effect_value2;
    int touch_effect_value3;
    int item_color;
    int spec_effect_value1;
    int spec_effect_value2;
    int spec_effect_value3;
    int cur_durability;
    int custom_made;
    int prefix_type;
    int prefix_value;
    int secondary_type;
    int secondary_value;
    int enchant_bonus;
    int pos_x;
    int pos_y;
    int is_equipped;
};

struct AccountDbBankItemRow
{
    int slot;
    int item_id;
    int64_t count;
    int touch_effect_type;
    int touch_effect_value1;
    int touch_effect_value2;
    int touch_effect_value3;
    int item_color;
    int spec_effect_value1;
    int spec_effect_value2;
    int spec_effect_value3;
    int cur_durability;
    int custom_made;
    int prefix_type;
    int prefix_value;
    int secondary_type;
    int secondary_value;
    int enchant_bonus;
};

struct AccountDbIndexedValue
{
    int index;
    int value;
};

struct AccountDbEquippedItem
{
    int item_id;
    int item_color;
};

class CClient;

bool EnsureAccountDatabase(const char* account_name, sqlite3** outDb, std::string& outPath);
bool LoadAccountRecord(sqlite3* db, const char* account_name, AccountDbAccountData& outData);
bool UpdateAccountPassword(sqlite3* db, const char* account_name, const char* passwordHash, const char* passwordSalt);
bool ListCharacterSummaries(sqlite3* db, const char* account_name, std::vector<AccountDbCharacterSummary>& outChars);
bool LoadCharacterState(sqlite3* db, const char* character_name, AccountDbCharacterState& outState);
bool LoadCharacterItems(sqlite3* db, const char* character_name, std::vector<AccountDbItemRow>& outItems);
bool LoadCharacterBankItems(sqlite3* db, const char* character_name, std::vector<AccountDbBankItemRow>& outItems);
bool LoadCharacterItemPositions(sqlite3* db, const char* character_name, std::vector<AccountDbIndexedValue>& outPositionsX, std::vector<AccountDbIndexedValue>& outPositionsY);
bool LoadCharacterItemEquips(sqlite3* db, const char* character_name, std::vector<AccountDbIndexedValue>& outEquips);
bool LoadCharacterMagicMastery(sqlite3* db, const char* character_name, std::vector<AccountDbIndexedValue>& outMastery);
bool LoadCharacterSkillMastery(sqlite3* db, const char* character_name, std::vector<AccountDbIndexedValue>& outMastery);
bool LoadCharacterSkillSSN(sqlite3* db, const char* character_name, std::vector<AccountDbIndexedValue>& outValues);
bool LoadEquippedItemAppearances(sqlite3* db, const char* character_name, std::vector<AccountDbEquippedItem>& outItems);
bool InsertCharacterState(sqlite3* db, const AccountDbCharacterState& state);
bool InsertCharacterItems(sqlite3* db, const char* character_name, const std::vector<AccountDbItemRow>& items);
bool InsertCharacterBankItems(sqlite3* db, const char* character_name, const std::vector<AccountDbBankItemRow>& items);
bool InsertCharacterItemPositions(sqlite3* db, const char* character_name, const std::vector<AccountDbIndexedValue>& positionsX, const std::vector<AccountDbIndexedValue>& positionsY);
bool InsertCharacterItemEquips(sqlite3* db, const char* character_name, const std::vector<AccountDbIndexedValue>& equips);
bool InsertCharacterMagicMastery(sqlite3* db, const char* character_name, const std::vector<AccountDbIndexedValue>& mastery);
bool InsertCharacterSkillMastery(sqlite3* db, const char* character_name, const std::vector<AccountDbIndexedValue>& mastery);
bool InsertCharacterSkillSSN(sqlite3* db, const char* character_name, const std::vector<AccountDbIndexedValue>& values);
bool InsertAccountRecord(sqlite3* db, const AccountDbAccountData& data);
bool InsertCharacterRecord(sqlite3* db, const AccountDbCharacterData& data);
bool SaveCharacterSnapshot(sqlite3* db, const CClient* client);
bool DeleteCharacterData(sqlite3* db, const char* character_name);
void CloseAccountDatabase(sqlite3* db);

// Block list
bool LoadBlockList(sqlite3* db, std::vector<std::pair<std::string, std::string>>& outBlocks);
bool SaveBlockList(sqlite3* db, const std::vector<std::pair<std::string, std::string>>& blocks);
bool ResolveCharacterToAccount(const char* character_name, char* outAccountName, size_t accountNameSize);

// Global name checks - scan all account databases
bool CharacterNameExistsGlobally(const char* character_name);
bool AccountNameExists(const char* account_name);

// Global counts - scan all account databases
struct account_stats
{
    int accounts;
    int characters;
    std::vector<std::pair<std::string, int>> over_limit; // accounts with >4 characters
};
account_stats CountAccountStats();
