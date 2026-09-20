// Game.h: interface for the CGame class.

#pragma once

#include "CommonTypes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <memory.h>
#include <vector>
#include <map>
#include <unordered_map>
#include <string>
#include "TimeUtils.h"

namespace hb::shared::net { class IOServicePool; }
extern hb::shared::net::IOServicePool* G_pIOPool;
extern bool G_bRunning;

#include "ASIOSocket.h"
#include "Client.h"
#include "Npc.h"
#include "Map.h"
#include "ActionID_Server.h"
#include "NetMessages.h"
#include "Packet/PacketMap.h"
#include "ServerMessages.h"
#include "Misc.h"
#include "NetworkMsg.h"
#include "Magic.h"
#include "Skill.h"
#include "DynamicObject.h"
#include "DelayEvent.h"
#include "version_info.h"
#include "Fish.h"
#include "DynamicObject.h"
#include "DynamicObjectID.h"
#include "Portion.h"
#include "Mineral.h"
#include "Quest.h"
#include "BuildItem.h"
#include "TeleportLoc.h"
#include "GlobalDef.h"
#include "TempNpcItem.h"
#include "PartyManager.h"
#include "IOServicePool.h"
#include "ConcurrentMsgQueue.h"
#include "ServerConfig.h"
#include "FormulaEngine.h"
#include "GameConfigSqliteStore.h"

namespace hb::server::config
{
// Infrastructure limits
constexpr int MaxMaps                   = 100;
constexpr int MaxNpcTypes               = 200;
constexpr int ServerSocketBlockLimit    = 300;
constexpr int MaxBanned                 = 500;
constexpr int MaxAdmins                 = 50;
constexpr int MaxNpcItems               = 1000;
constexpr int MaxClients                = 2000;
constexpr int MaxClientLoginSock        = 2000;
constexpr int MaxNpcs                   = 15000;
constexpr int MaxItemTypes              = 5000;
constexpr int MaxDynamicObjects         = 60000;
constexpr int MaxDelayEvents            = 60000;
constexpr int MaxNotifyMsgs             = 300;
constexpr int MaxSkillPoints            = 700;
constexpr int MaxRewardGold             = 99999999;

// Timing constants (milliseconds)
constexpr int ClientTimeout             = 30000;
constexpr int SpUpTime                  = 10000;
constexpr int PoisonTime                = 12000;
constexpr int HpUpTime                  = 15000;
constexpr int MpUpTime                  = 20000;
constexpr int HungerTime                = 60000;
constexpr int NoticeTime                = 80000;
constexpr int SummonTime                = 300000;
constexpr int AutoSaveTime              = 600000;
constexpr int ExpStockTime              = 1000 * 10;
constexpr int AutoExpTime               = 1000 * 60 * 6;
constexpr int NightTime                 = 30;
constexpr int RagProtectionTime         = 7000;

// Game config
constexpr int MsgQueueSize              = 100000;
constexpr int TotalLevelUpPoint         = 3;
constexpr int SsnLimitMultiplyValue     = 2;
constexpr int CharPointLimit            = 1000;
} // namespace hb::server::config

namespace hb::server::config
{
// Attack AI types
namespace AttackAI
{
	enum : int
	{
		Normal          = 1,
		ExchangeAttack  = 2,
		TwoByOneAttack  = 3,
	};
}

// Resource limits
constexpr int MaxFishs                  = 200;
constexpr int MaxMinerals               = 200;
constexpr int MaxEngagingFish           = 30;
constexpr int MaxPortionTypes           = 500;
constexpr int MaxQuestType              = 200;

// Game limits
constexpr int MaxConstructNum           = 10;
constexpr int MaxSchedule               = 10;
constexpr int MaxApocalypse             = 7;
constexpr int MaxHeldenian              = 10;
// Combat constants
constexpr int MinimumHitRatio           = 15;
constexpr int MaximumHitRatio           = 99;
constexpr int SpecialEventTime          = 300000;

// Crusade constants
constexpr int GmgManaConsumeUnit        = 15;
constexpr int MaxConstructionPoint      = 30000;
constexpr int MaxSummonPoints           = 30000;
constexpr int MaxWarContribution        = 200000;
} // namespace hb::server::config

// ItemLog values moved to ServerMessages.h -> hb::server::net::ItemLogAction

#define NO_MSGSPEEDCHECK

struct DropEntry
{
	int item_id;
	int weight;
	int min_count;
	int max_count;
};

struct DropTable
{
	int id;
	char name[64];
	char description[128];
	std::vector<DropEntry> tierEntries[3];
	int total_weight[3];
};

// Shop system structures
struct NpcShopMapping
{
	int npc_config_id;               // NPC config ID from npc_configs table
	int shop_id;                     // Which shop inventory to use
	char description[64];           // For documentation
};

struct ShopData
{
	int shop_id;
	std::vector<int16_t> item_ids;   // List of item IDs available in this shop
};

struct summon_threshold_entry
{
	int min_mastery;
	int npc_id;
	int weight;
};

template <typename T>
static bool In(const T& value, std::initializer_list<T> values) {
	return std::any_of(values.begin(), values.end(),
		[&value](const T& x) { return x == value; });
}

template <typename T>
static bool NotIn(const T& value, std::initializer_list<T> values) {
	return !In(value, values);
}


template <typename T, class = typename enable_if<!is_pointer<T>::value>::type >
static void push(char*& cp, T value) {
	auto p = (T*)cp;
	*p = (T)value;
	cp += sizeof(T);
}

template <typename T, class = typename enable_if<!is_pointer<T>::value>::type >
static void pop(char*& cp, T& v) {
	T* p = (T*)cp;
	v = *p;
	cp += sizeof(T);
}

static void push(char*& dest, const char* src, uint32_t len) {
	memcpy(dest, src, len);
	dest += len;
}

static void push(char*& dest, const char* src) {

	strcpy(dest, src);
	dest += strlen(src) + 1;
}

static void push(char*& dest, const string& str) {
	strcpy(dest, str.c_str());
	dest += str.length() + 1;
}

static void pop(char*& src, char* dest, uint32_t len) {
	memcpy(dest, src, len);
	src += len;
}
static void pop(char*& src, char* dest) {

	size_t len = strlen(src) + 1;
	memcpy(dest, src, len);
	src += len;
}

static void pop(char*& src, string& str) {
	str = src;
	src += str.length() + 1;
}


struct LoginClient
{
	LoginClient(asio::io_context& ctx)
	{
		sock = 0;
		sock = new class hb::shared::net::ASIOSocket(ctx, hb::server::config::ClientSocketBlockLimit);
		sock->init_buffer_size(hb::shared::limits::MsgBufferSize);
		timeout_tm = 0;
	}

	uint32_t timeout_tm;
	~LoginClient();
	hb::shared::net::ASIOSocket* sock;
	char ip[21];
};

struct AdminEntry {
	char m_account_name[hb::shared::limits::AccountNameLen];
	char m_char_name[hb::shared::limits::CharNameLen];
	char approved_ip[21];
	int m_admin_level = 1;
};

struct CommandPermission {
	int admin_level = 1000;
	std::string description;
};

class CGame
{
public:


	void request_noticement_handler(int client_h);
	bool send_client_npc_configs(int client_h);
	bool send_client_map_configs(int client_h);

	LoginClient* _lclients[hb::server::config::MaxClientLoginSock];

	bool accept_login(hb::shared::net::ASIOSocket* sock);
	bool accept_from_async(asio::ip::tcp::socket&& peer);
	bool accept_login_from_async(asio::ip::tcp::socket&& peer);

	void party_operation(char* data);


	bool check_character_data(int client_h);
	//bool _bDecodeNpcItemConfigFileContents(char * data, size_t msg_size);
	void global_update_configs(char config_type);
	void local_update_configs(char config_type);

	void reload_npc_configs();
	void reload_shop_configs();
	void send_config_reload_notification(bool items, bool magic, bool skills, bool npcs, bool balance = false, bool colors = false, bool attribute_types = false);
	void push_config_reload_to_clients(bool items, bool magic, bool skills, bool npcs, bool balance = false, bool colors = false, bool attribute_types = false);
	void reload_color_palette();
	void reload_attribute_types();
	void apply_server_config(const server_config& cfg);
	bool reload_server_config();
	void send_server_config_update();
	bool reload_formulas();

	void enforce_max_level(int new_max);
	void enforce_max_stat_value(int new_max);
	void enforce_base_stat_value();
	void reprocess_online_player(int client_h);


	
	


	
	
	// Lists

	// Crusade


	// Acidx commands
	


	void set_force_recall_time(int client_h);


	//  bool bReadTeleportConfigFile(char * fn);
	//	void RequestTeleportD2Handler(int client_h, char * data);
	
	// Hack Checks
	bool check_client_move_frequency(int client_h, uint32_t client_time);

	// bool bCheckClientInvisibility(short client_h);

	//Hypnotoad functions

	void request_change_play_mode(int client_h);
	
	void state_change_handler(int client_h, char * data, size_t msg_size);
	
	

	int  get_map_location_side(char * map_name);
	void chat_msg_handler_gsm(int msg_type, int v1, char * name, char * data, size_t msg_size);
	void remove_client_short_cut(int client_h);
	bool add_client_short_cut(int client_h);

	void request_help_handler(int client_h);
	
	void check_connection_handler(int client_h, char *data, bool already_responded = false);

	void aging_map_sector_info();
	void update_map_sector_info();
	void activate_special_ability_handler(int client_h);
	void join_party_handler(int client_h, int v1, const char * member_name);
	void request_shop_contents_handler(int client_h, char * data);
	void request_restart_handler(int client_h);
	int request_panning_map_data_request(int client_h, char * data);
	void request_check_account_password_handler(char * data, size_t msg_size);
	void request_noticement_handler(int client_h, char * data);

	void check_special_event(int client_h);
	char get_special_ability(int kind_sa);
	void get_map_initial_point(int map_index, short * pX, short * pY, char * player_location = 0);
	int  get_max_hp(int client_h);
	int  get_max_mp(int client_h);
	int  get_max_sp(int client_h);
	
	

	
	bool on_close();
	void force_disconnect_account(char * account_name, uint16_t count);
	void toggle_safe_attack_mode_handler(int client_h);
	void special_event_handler();
	
	int force_player_disconnect(int num);
	int save_all_players();
	int get_map_index(char * map_name);
	void weather_processor();
	int calc_player_num(char map_index, short dX, short dY, char radius);
	int get_exp_level(uint32_t exp);
	void restore_player_rating(int client_h);
	void calc_exp_stock(int client_h);
	void response_save_player_data_reply_handler(char * data, size_t msg_size);
	void notice_handler();
	bool read_notify_msg_list_file(const char * fn);
	void set_player_reputation(int client_h, char * pMsg, char value, size_t msg_size);
	void check_day_or_night_mode();
	int get_player_number_on_spot(short dX, short dY, char map_index, char range);
	void restore_player_characteristics(int client_h);
	void get_player_profile(int client_h, char * pMsg, size_t msg_size);
	void set_player_profile(int client_h, char * pMsg, size_t msg_size);
	void check_and_notify_player_connection(int client_h, char * pMsg, uint32_t size);
	int calc_total_weight(int client_h);
	void send_object_motion_reject_msg(int client_h);
	int get_follower_number(short owner_h, char owner_type);
	void request_full_object_data(int client_h, char * data);
	int calc_max_load(int client_h);
	void request_civil_right_handler(int client_h, char * data);
	bool check_limited_user(int client_h);
	void level_up_settings_handler(int client_h, char * data, size_t msg_size);
	bool check_level_up(int client_h);
	uint32_t get_level_exp(int level);
	void quit();
	void release_follow_mode(short owner_h, char owner_type);
	void request_teleport_handler(int client_h, const char * data, const char * map_name = 0, int dX = -1, int dY = -1);
	void request_teleport_auth_handler(int client_h, const char * data);
	void toggle_combat_mode_handler(int client_h);
	void on_start_game_signal();
	uint32_t dice(uint32_t iThrow, uint32_t range);
	bool init_npc_attr(class CNpc * npc, int npc_config_id, short sClass, char sa);
	int get_npc_config_id_by_name(const char * npc_name) const;
	void send_notify_msg(int from_h, int to_h, uint16_t msg_type, uint32_t v1, uint64_t v2, uint32_t v3, const char * string, uint32_t v4 = 0, uint32_t v5 = 0, uint32_t v6 = 0, uint32_t v7 = 0, uint32_t v8 = 0, uint32_t v9 = 0, const char * string2 = 0);
	void send_item_attribute_change(int client_h, int item_index, CItem* item, uint32_t spec_value1 = 0, uint32_t spec_value2 = 0);
	void send_gizon_item_change(int client_h, int item_index, CItem* item);
	void send_exchange_item_notify(int from_h, int to_h, uint16_t msg_type, int item_index, CItem* item, int amount);

	void broadcast_server_message(const char* message);
	int  client_motion_stop_handler(int client_h, short sX, short sY, direction dir);

	
	void client_common_handler(int client_h, char * data);
	bool get_msg_queue(char * pFrom, char * data, size_t* msg_size, int * index, char * key);
	void msg_process();
	bool put_msg_queue(char cFrom, char * data, size_t msg_size, int index, char key);
	//int  calculate_attack_effect(short target_h, char target_type, short attacker_h, char attacker_type, int tdX, int tdY, int attack_mode, bool near_attack = false);
	bool get_empty_position(short * pX, short * pY, char map_index);
	direction get_next_move_dir(short sX, short sY, short dstX, short dstY, char map_index, char turn, int * error_acc);
	int  client_motion_attack_handler(int client_h, short sX, short sY, short dX, short dY, short type, direction dir, uint16_t target_object_id, uint32_t client_time, bool response = true, bool is_dash = false);
	void chat_msg_handler(int client_h, char * data, size_t msg_size);
	bool is_blocked_by(int sender_h, int receiver_h) const;
	void npc_process();
	int create_new_npc(int npc_config_id, char * name, char * map_name, short sClass, char sa, char move_type, int * offset_x, int * offset_y, char * waypoint_list, hb::shared::geometry::GameRectangle * area, int spot_mob_index, char change_side, bool hide_gen_mode, bool is_summoned = false, bool firm_berserk = false, bool is_master = false, bool bypass_mob_limit = false);
	//bool create_new_npc(char * npc_name, char * name, char * map_name, short sX, short sY);
	int spawn_map_npcs_from_database(struct sqlite3* db, int map_index);
	bool get_is_string_is_number(char * str);
	void game_process();
	void init_player_data(int client_h, char * data, uint32_t size);
	void response_player_data_handler(char * data, uint32_t size);
	void check_client_response_time();
	void on_timer(char type);
	int compose_move_map_data(short sX, short sY, int client_h, direction dir, char * data);
	void send_event_to_near_client_type_b(uint32_t msg_id, uint16_t msg_type, char map_index, short sX, short sY, short v1, short v2, short v3, short v4 = 0);
	void send_event_to_near_client_type_b(uint32_t msg_id, uint16_t msg_type, char map_index, short sX, short sY, short v1, short v2, short v3, uint32_t v4 = 0);
	void send_ground_item_event(uint16_t msg_type, char map_index, short sX, short sY, const CItem* item);
	void send_event_to_near_client_type_a(short owner_h, char owner_type, uint32_t msg_id, uint16_t msg_type, int v1, short v2, short v3);
	void delete_client(int client_h, bool save, bool notify, bool count_logout = true, bool force_close_conn = false);
	int  compose_init_map_data(short sX, short sY, int client_h, char * data);
	void fill_player_map_object(hb::net::PacketMapDataObjectPlayer& obj, short owner_h, int viewer_h);
	void fill_npc_map_object(hb::net::PacketMapDataObjectNpc& obj, short owner_h, int viewer_h);
	void request_init_data_handler(int client_h, char * data, char key, size_t msg_size = 0);
	void request_init_player_handler(int client_h, char * data, char key);
	int client_motion_move_handler(int client_h, short sX, short sY, direction dir, char move_type);
	void client_motion_handler(int client_h, char * data);
	void on_client_read(int client_h);
	bool init();
	void on_client_socket_event(int client_h);  // MODERNIZED: Polls socket instead of handling window messages
	bool accept(class hb::shared::net::ASIOSocket * x_sock);

	// New 06/05/2004
	// Upgrades

	
	
	//Party Codes
	void request_create_party_handler(int client_h);
	void party_operation_result_handler(char *data);
	void party_operation_result_create(int client_h, char *name, int result, int party_id);
	void party_operation_result_join(int client_h, char *name, int result, int party_id);
	void party_operation_result_dismiss(int client_h, char *name, int result, int party_id);
	void party_operation_result_delete(int party_id);
	void request_join_party_handler(int client_h, char *data, size_t msg_size);
	void request_dismiss_party_handler(int client_h);
	void get_party_info_handler(int client_h);
	void party_operation_result_info(int client_h, char * name, int total, char *name_list);
	void request_delete_party_handler(int client_h);
	void request_accept_join_party_handler(int client_h, int result);
	void get_exp(int client_h, uint32_t exp, bool is_attacker_own = false);

	// New 07/05/2004
	// Guild Codes

	// Item Logs


	// PK Logs

	//HBest code
	void force_recall_process();
	bool is_enemy_zone(int i);

	CGame();
	virtual ~CGame();

	// Realm configuration (from server_config.json)
	char m_realm_name[32];
	char m_login_listen_ip[16];
	int  m_login_listen_port;
	char m_game_listen_ip[16];
	int  m_game_listen_port;
	char m_game_connection_ip[16];   // Optional - for future login->game server connection
	int  m_game_connection_port;     // Optional - for future login->game server connection

//private:
	bool load_player_data_from_db(int client_h);
	bool register_map(char * name);

	class CClient * m_client_list[hb::server::config::MaxClients];
	class CNpc   ** m_npc_list;  // Pointer to EntityManager's entity array (for backward compatibility)
	class CMap    * m_map_list[hb::server::config::MaxMaps];
	class CNpcItem * m_temp_npc_item[hb::server::config::MaxNpcItems];

	class CEntityManager * m_entity_manager;  // Entity spawn/despawn manager
	class FishingManager * m_fishing_manager;  // Fish spawning/catching manager
	class MiningManager * m_mining_manager;    // Mineral spawning/mining manager
	class CraftingManager * m_crafting_manager; // Potion/crafting recipe manager
	class QuestManager * m_quest_manager;       // Quest assignment/progress manager
	class GuildManager * m_guild_manager;       // Guild operations manager
	class DelayEventManager * m_delay_event_manager; // Delay event processor
	class DynamicObjectManager * m_dynamic_object_manager; // Dynamic object manager
	class LootManager * m_loot_manager; // Kill rewards and penalties
	class CombatManager * m_combat_manager; // Combat calculations and effects
	class ItemManager * m_item_manager; // Item management
	class MagicManager * m_magic_manager; // Magic/spell casting
	class SkillManager * m_skill_manager; // Skill mastery/profession
	class WarManager * m_war_manager; // Crusade/Heldenian/Apocalypse/FightZone
	class StatusEffectManager * m_status_effect_manager; // Status effect flags
	class RegenManager * m_regen_manager; // Player HP/MP/SP regen, hunger, poison

	hb::shared::net::ConcurrentMsgQueue m_msgQueue;
	int             m_total_maps;
	bool			m_is_game_started;
	bool			m_is_item_available, m_is_build_item_available, m_is_npc_available, m_is_magic_available;
	bool			m_is_skill_available, m_is_portion_available, m_is_quest_available, m_is_teleport_available;
	bool			m_is_drop_table_available;
	std::map<int, DropTable> m_drop_tables;

	// Shop system - server sends shop contents to client by item IDs
	bool m_is_shop_data_available;
	std::map<int, int> m_npc_shop_mappings;        // npc_config_id  shop_id
	std::map<int, ShopData> m_shop_data;          // shop_id  ShopData

	// Summon Creature thresholds (loaded from gamedata.db)
	std::vector<summon_threshold_entry> m_summon_thresholds;

	// Character creation items (loaded from gamedata.db)
	std::vector<creation_item_entry> m_creation_items;
	CItem   * m_item_config_list[hb::server::config::MaxItemTypes];
	class CNpc    * m_npc_config_list[hb::server::config::MaxNpcTypes];
	class CMagic  * m_magic_config_list[hb::shared::limits::MaxMagicType];
	class CSkill  * m_skill_config_list[hb::shared::limits::MaxSkillType];
	std::unordered_map<int, int> m_magic_to_manual_item; // magic_index → item_id
	void build_magic_manual_index();
	//class CTeleport * m_pTeleportConfigList[DEF_MAXTELEPORTTYPE];

	std::string m_config_hash[8];
	void compute_config_hashes();
	void compute_balance_hash();
	void compute_color_palette_hash();
	void compute_attribute_types_hash();
	bool send_client_balance_config(int client_h);
	bool send_client_color_palette(int client_h);
	bool send_client_attribute_types(int client_h);

	std::vector<color_palette_entry> m_color_palette;
	std::vector<attribute_prefix_type_entry> m_attribute_prefix_types;
	std::vector<attribute_secondary_type_entry> m_attribute_secondary_types;
	uint8_t m_prefix_multiplier[16]{};
	uint8_t m_prefix_min_value[16]{};
	uint8_t m_prefix_max_value[16]{};
	uint8_t m_secondary_multiplier[16]{};
	uint8_t m_secondary_min_value[16]{};
	uint8_t m_secondary_max_value[16]{};
	void build_multiplier_lookup();

	class hb::shared::net::ASIOSocket* _lsock;

	class PartyManager* m_party_manager;

	void on_client_login_read(int h);
	void on_login_client_socket_event(int login_client_h);  // MODERNIZED: Polls login client socket instead of handling window messages
	void delete_login_client(int h);

	std::vector<LoginClient*> _lclients_disconn;

	char            m_msg_buffer[hb::shared::limits::MsgBufferSize+1];

	int   m_total_clients, m_max_clients, m_total_game_server_clients, m_total_game_server_max_clients;
	int   m_total_bots, m_max_bots, m_total_game_server_bots, m_total_game_server_max_bots;
	hb::time::local_time m_max_user_sys_time{};

	bool  m_on_exit_process;
	uint32_t m_exit_process_time;

	uint32_t m_shutdown_start_time;          // When scheduled shutdown was requested (0 = not scheduled)
	uint32_t m_shutdown_delay_ms;            // Total delay in ms from m_shutdown_start_time
	std::vector<int> m_shutdown_milestones;  // Countdown display points in seconds (descending)
	int m_shutdown_next_milestone;           // Index into m_shutdown_milestones
	char m_shutdown_message[128];            // Custom message for noticement dialog

	uint32_t m_weather_time, m_game_time_1, m_game_time_2, m_game_time_3, m_game_time_4, m_game_time_5, m_game_time_6;
	uint32_t m_equip_validation_time = 0;
	
	// Crusade Schedule
	bool m_is_crusade_war_starter;
	bool m_is_apocalypse_starter;
	int m_latest_crusade_day_of_week;

	char  m_day_or_night;
 	int   m_skill_progress_threshold[102];

	class CMsg * m_notice_msg_list[hb::server::config::MaxNotifyMsgs];
	int   m_total_notice_msg, m_prev_send_notice_msg;
	uint32_t m_notice_time, m_special_event_time;
	bool  m_is_special_event_time;
	char  m_special_event_type;

	uint32_t m_level_exp_table[1000];	//New 22/10/04


	bool  m_is_server_shutdown;
	char  m_shutdown_code;

	int   m_middleland_map_index; 
	int   m_aresden_map_index;
	int	  m_elvine_map_index;
	int   m_bt_field_map_index;
	int   m_godh_map_index;
	int   m_aresden_occupy_tiles;
	int   m_elvine_occupy_tiles;
	int   m_cur_msgs, m_max_msgs;

	struct {
		int64_t funds;
		int64_t crimes;
		int64_t wins;

	} m_city_status[3];
	
	int	  m_strategic_status;
	
	int   m_auto_rebooting_count;

	class CBuildItem * m_build_item_list[hb::shared::limits::MaxBuildItems];

	char * m_notice_data;
	uint32_t  m_notice_data_size;

	uint32_t  m_map_sector_info_time;
	int    m_map_sector_info_update_count;

	// Crusade
	int	   m_crusade_count;	
	bool   m_is_crusade_mode;		
	bool   m_is_apocalypse_mode;
	struct {
		char map_name[11];
		char type;
		int  x, y;

	} m_crusade_structures[hb::shared::limits::MaxCrusadeStructures];

	
	int m_collected_mana[3];
	int m_aresden_mana, m_elvine_mana;

	int m_last_crusade_winner; 	// New 13/05/2004
	struct {
		int crashed_structure_num;
		int structure_damage_amount;
		int casualties;
	} m_meteor_strike_result;

	struct {
		char type;
		char side;
		short x, y;
	} m_middle_crusade_structure_info[hb::shared::limits::MaxCrusadeStructures];

	struct {
		char banned_ip_address[21];
	} m_banned_list[hb::server::config::MaxBanned];

	AdminEntry m_admin_list[hb::server::config::MaxAdmins];
	int m_admin_count = 0;

	int find_admin_by_account(const char* account_name);
	int find_admin_by_char_name(const char* charName);
	bool is_client_admin(int client_h);

	std::unordered_map<std::string, CommandPermission> m_command_permissions;
	int get_command_required_level(const char* cmdName) const;

	// GM command helpers
	int find_client_by_name(const char* name) const;
	bool gm_teleport_to(int client_h, const char* dest_map, short dest_x, short dest_y);

	// Crusade Scheduler
	struct {
		int day;
		int hour;
		int minute;
	} m_crusade_war_schedule[hb::server::config::MaxSchedule];

	struct {
		int day;
		int hour;
		int minute;
	} m_apocalypse_schedule_start[hb::server::config::MaxApocalypse];

	struct {
		int day;
		int start_hour;
		int start_minute;
		int end_hour;
		int end_minute;
	} m_heldenian_schedule[hb::server::config::MaxHeldenian];

	struct {
		int day;
		int hour;
		int minute;
	} m_apocalypse_schedule_end[hb::server::config::MaxApocalypse];

	int m_total_middle_crusade_structures;
 
	int m_client_shortcut[hb::server::config::MaxClients+1];

	int m_npc_construction_point[hb::server::config::MaxNpcTypes];
	uint32_t m_crusade_guid;
	short m_last_crusade_date;
	int   m_crusade_winner_side;

	struct  {
		int total_members;
		int index[9];
	}m_party_info[hb::server::config::MaxClients];

	// 09/26/2004
	short m_slate_success_rate;

	// 17/05/2004
	short m_force_recall_time;

	// 22/05/2004 - Drop rate multipliers (1.0 = normal, 1.5 = 150%, 0.5 = 50%)
	float m_primary_drop_rate;    // Primary item drops (base 10%)
	float m_gold_drop_rate;       // Gold drops (base 30%)
	float m_secondary_drop_rate;  // Bonus/secondary drops (base 5%)

	// 25/05/2004
	int m_final_shutdown_count;

	// New 06/07/2004
	bool m_enemy_kill_mode;
	int m_enemy_kill_adjust;

	// Configurable Raid Time 
	short m_raid_time_monday; 
	short m_raid_time_tuesday; 
	short m_raid_time_wednesday; 
	short m_raid_time_thursday; 
	short m_raid_time_friday; 
	short m_raid_time_saturday; 
	short m_raid_time_sunday; 

	bool m_manual_time;

	// Apocalypse
	bool	m_is_apocalypse_mode_legacy;
	bool	m_is_heldenian_mode;
	bool	m_is_heldenian_teleport;
	char	m_heldenian_type;

	uint32_t m_apocalypse_guid;
	
	// Slate exploit
	int m_char_point_limit;

	// Limit Checks
	bool m_allow_100_all_skill;
	char m_rep_drop_modifier;

	// ============================================================================
	// Configurable Settings (loaded from server_config.json)
	// ============================================================================

	server_config m_server_config;
	hb::shared::formula_engine m_formula_engine;

	// Timing Settings (milliseconds)
	int m_client_timeout;           // client-timeout-ms
	int m_stamina_regen_interval;    // stamina-regen-interval
	int m_poison_damage_interval;    // poison-damage-interval
	int m_health_regen_interval;     // health-regen-interval
	int m_mana_regen_interval;       // mana-regen-interval
	int m_hunger_consume_interval;   // hunger-consume-interval
	int m_summon_creature_duration;  // summon-creature-duration
	int m_autosave_interval;        // autosave-interval
	int m_lag_protection_interval;   // lag-protection-interval

	// Character/Leveling Settings
	int m_base_stat_value;           // base-stat-value
	int m_max_creation_stat_value;   // max-creation-stat-value
	int m_creation_stat_points;      // creation-stat-points
	int m_base_stat_total;           // computed: m_base_stat_value * 6 + m_creation_stat_points
	int m_levelup_stat_gain;         // levelup-stat-gain
	int m_max_level;                // max-level (renamed from max-player-level)
	int m_max_stat_value;            // calculated: base + creation + (levelup * max_level) + 16

	// Combat Settings
	int m_minimum_hit_ratio;         // minimum-hit-ratio
	int m_maximum_hit_ratio;         // maximum-hit-ratio

	// Gameplay Settings
	int m_nighttime_duration;       // nighttime-duration
	int m_grand_magic_mana_consumption; // grand-magic-mana-consumption
	int m_max_construction_points;   // maximum-construction-points
	int m_max_summon_points;         // maximum-summon-points
	int m_max_war_contribution;      // maximum-war-contribution
	int m_max_bank_items;            // max-bank-items

	// ============================================================================

	bool var_89C, var_8A0;
	char m_heldenian_victory_type, m_last_heldenian_winner, m_heldenian_mode_type;
	int m_heldenian_aresden_dead, m_heldenian_elvine_dead, var_A38, var_88C;
	int m_heldenian_aresden_left_tower, m_heldenian_elvine_left_tower;
	uint32_t m_heldenian_guid, m_heldenian_start_hour, m_heldenian_start_minute, m_heldenian_start_time, m_heldenian_finish_time;
	bool m_received_item_list;
	bool m_heldenian_initiated;
	bool m_heldenian_running;

private:

public:

	void check_force_recall_time(int client_h);
	void set_playing_status(int client_h);
	void force_change_play_mode(int client_h, bool notify);
	hb::shared::entity::PlayerAppearance build_broadcast_appearance(int client_h);

	void show_version(int client_h);
	void show_client_msg(int client_h, char* pMsg);
	void request_resurrect_player(int client_h, bool resurrect);
	void lotery_handler(int client_h);
	
	/*void GetAngelMantleHandler(int client_h,int item_id,char * string);
	void CheckAngelUnequip(int client_h, int angel_id);
	int angel_equip(int client_h);*/

	void get_angel_handler(int client_h, char* data, size_t msg_size);

	void request_enchant_upgrade_handler(int client, uint32_t type, uint32_t lvl, int enchant_type);
	int get_required_level_for_upgrade(uint32_t value);

	//50Cent - Repair All

};
