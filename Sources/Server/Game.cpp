// Game.cpp: implementation of the CGame class.

#include "CommonTypes.h"
#include "Game.h"
#include "AdminLevel.h"
#include "StringCompat.h"
#include "TimeUtils.h"
#include <filesystem>
#include <iostream>
#include "LoginServer.h"
#include "EntityManager.h"
#include "FishingManager.h"
#include "MiningManager.h"
#include "AccountSqliteStore.h"
#include "GameConfigSqliteStore.h"
#include "MapInfoSqliteStore.h"
#include "sqlite3.h"
#include "Packet/SharedPackets.h"
#include "SHA256.h"
#include "SharedCalculations.h"
#include "BalanceConstants.h"
#include "Item/ItemAttributes.h"
#include "ObjectIDRange.h"
#include "DirectionHelpers.h"
#include "GameChatCommand.h"
#include "CraftingManager.h"
#include "QuestManager.h"
#include "GuildManager.h"
#include "DelayEventManager.h"
#include "DynamicObjectManager.h"
#include "LootManager.h"
#include "CombatManager.h"
#include "ItemManager.h"
#include "MagicManager.h"
#include "SkillManager.h"
#include "WarManager.h"
#include "StatusEffectManager.h"
#include "RegenManager.h"
#include "Log.h"
#include "ServerLogChannels.h"

using namespace hb::shared::net;

using hb::log_channel;
using namespace hb::server::net;
using namespace hb::server::config;
using namespace hb::server::npc;
using namespace hb::server::skill;
using namespace hb::server::msg;
namespace smap = hb::server::map;
namespace squest = hb::server::quest;
namespace sdelay = hb::server::delay_event;
namespace sock = hb::shared::net::socket;
namespace dynamic_object = hb::shared::dynamic_object;

using namespace hb::shared::action;
using namespace hb::shared::direction;

using namespace hb::shared::item;

class CDebugWindow* DbgWnd;

extern char G_cTxt[512];
extern char	G_cData50000[50000];

// extern void PutDebugMsg(char * str);	// 2002-09-09 #2


// Build a snapshot of a player's appearance with equipment item_id/display_id
// populated from their currently equipped items. Called at each broadcast point
// rather than stored, so it's always in sync with actual inventory.
hb::shared::entity::PlayerAppearance CGame::build_broadcast_appearance(int client_h)
{
	auto* client = m_client_list[client_h];
	auto appr = client->m_appearance;  // copy cosmetic + old type fields

	// Helper: populate item_id + display_id for one equip slot.
	// display_id comes from the item CONFIG template (m_item_config_list),
	// not the player's inventory item instance (which has default -1).
	// Writes through local copies to avoid packed struct ref/ptr issues on GCC.
	auto fill_slot = [&](EquipPos pos, int16_t& out_item_id, int16_t& out_display_id) {
		short slot_index = client->m_item_equipment_status[to_int(pos)];
		if (slot_index >= 0 && client->m_item_list[slot_index] != nullptr) {
			auto* item = client->m_item_list[slot_index];
			out_item_id = item->m_id_num;
			if (item->m_id_num > 0 && item->m_id_num < hb::server::config::MaxItemTypes
				&& m_item_config_list[item->m_id_num] != nullptr)
			{
				out_display_id = m_item_config_list[item->m_id_num]->m_display_id;
			}
		}
	};

	// Copy packed fields to locals, fill, then write back
	int16_t helm_id = appr.helm_item_id,     helm_disp = appr.helm_display_id;
	int16_t armor_id = appr.armor_item_id,   armor_disp = appr.armor_display_id;
	int16_t arm_id = appr.arm_item_id,       arm_disp = appr.arm_display_id;
	int16_t pants_id = appr.pants_item_id,   pants_disp = appr.pants_display_id;
	int16_t boots_id = appr.boots_item_id,   boots_disp = appr.boots_display_id;
	int16_t weapon_id = appr.weapon_item_id, weapon_disp = appr.weapon_display_id;
	int16_t shield_id = appr.shield_item_id, shield_disp = appr.shield_display_id;
	int16_t mantle_id = appr.mantle_item_id, mantle_disp = appr.mantle_display_id;

	fill_slot(EquipPos::Head,      helm_id,   helm_disp);
	fill_slot(EquipPos::Body,      armor_id,  armor_disp);
	fill_slot(EquipPos::FullBody,  armor_id,  armor_disp);
	fill_slot(EquipPos::Arms,      arm_id,    arm_disp);
	fill_slot(EquipPos::Leggings,     pants_id,  pants_disp);
	fill_slot(EquipPos::Boots,  boots_id,  boots_disp);
	fill_slot(EquipPos::RightHand, weapon_id, weapon_disp);
	fill_slot(EquipPos::TwoHand,   weapon_id, weapon_disp);
	fill_slot(EquipPos::LeftHand,  shield_id, shield_disp);
	fill_slot(EquipPos::Back,      mantle_id, mantle_disp);

	appr.helm_item_id = helm_id;       appr.helm_display_id = helm_disp;
	appr.armor_item_id = armor_id;     appr.armor_display_id = armor_disp;
	appr.arm_item_id = arm_id;         appr.arm_display_id = arm_disp;
	appr.pants_item_id = pants_id;     appr.pants_display_id = pants_disp;
	appr.boots_item_id = boots_id;     appr.boots_display_id = boots_disp;
	appr.weapon_item_id = weapon_id;   appr.weapon_display_id = weapon_disp;
	appr.shield_item_id = shield_id;   appr.shield_display_id = shield_disp;
	appr.mantle_item_id = mantle_id;   appr.mantle_display_id = mantle_disp;

	// Compute is_skirt from equipped pants
	short pants_slot = client->m_item_equipment_status[to_int(EquipPos::Leggings)];
	if (pants_slot >= 0 && client->m_item_list[pants_slot] != nullptr)
		appr.is_skirt = (client->m_item_list[pants_slot]->m_is_skirt != 0);

	return appr;
}

// Move location tables  auto-calculated from hb::shared::view::InitDataTilesX/Y.
// Each direction lists the (X,Y) tile offsets revealed when the player
// moves one step in that direction, terminated by -1.
int _tmp_iMoveLocX[9][hb::shared::view::MoveLocMaxEntries];
int _tmp_iMoveLocY[9][hb::shared::view::MoveLocMaxEntries];

static void BuildMoveLocTables()
{
	const int MAX_X = hb::shared::view::InitDataTilesX - 1;
	const int MAX_Y = hb::shared::view::InitDataTilesY - 1;

	// Zero-fill everything first
	memset(_tmp_iMoveLocX, 0, sizeof(_tmp_iMoveLocX));
	memset(_tmp_iMoveLocY, 0, sizeof(_tmp_iMoveLocY));

	// Helper: write an entry pair and advance the index
	auto put = [](int dir, int& idx, int x, int y) {
		_tmp_iMoveLocX[dir][idx] = x;
		_tmp_iMoveLocY[dir][idx] = y;
		idx++;
	};
	auto sentinel = [](int dir, int idx) {
		_tmp_iMoveLocX[dir][idx] = -1;
		_tmp_iMoveLocY[dir][idx] = -1;
	};

	int n;

	// Direction 0: unused
	sentinel(0, 0);

	// Direction 1 (North): top row, sweep X 0..MAX_X, Y=0
	n = 0;
	for(int x = 0; x <= MAX_X; x++) put(1, n, x, 0);
	sentinel(1, n);

	// Direction 2 (NE): top row X 0..MAX_X at Y=0, then right column Y=1..MAX_Y at X=MAX_X
	n = 0;
	for(int x = 0; x <= MAX_X; x++) put(2, n, x, 0);
	for(int y = 1; y <= MAX_Y; y++) put(2, n, MAX_X, y);
	sentinel(2, n);

	// Direction 3 (East): right column, sweep Y 0..MAX_Y, X=MAX_X
	n = 0;
	for(int y = 0; y <= MAX_Y; y++) put(3, n, MAX_X, y);
	sentinel(3, n);

	// Direction 4 (SE): right column Y 0..MAX_Y at X=MAX_X, then bottom row X=MAX_X-1..0 at Y=MAX_Y
	n = 0;
	for(int y = 0; y <= MAX_Y; y++) put(4, n, MAX_X, y);
	for(int x = MAX_X - 1; x >= 0; x--) put(4, n, x, MAX_Y);
	sentinel(4, n);

	// Direction 5 (South): bottom row, sweep X 0..MAX_X, Y=MAX_Y
	n = 0;
	for(int x = 0; x <= MAX_X; x++) put(5, n, x, MAX_Y);
	sentinel(5, n);

	// Direction 6 (SW): left column Y 0..MAX_Y at X=0, then bottom row X=1..MAX_X at Y=MAX_Y
	n = 0;
	for(int y = 0; y <= MAX_Y; y++) put(6, n, 0, y);
	for(int x = 1; x <= MAX_X; x++) put(6, n, x, MAX_Y);
	sentinel(6, n);

	// Direction 7 (West): left column, sweep Y 0..MAX_Y, X=0
	n = 0;
	for(int y = 0; y <= MAX_Y; y++) put(7, n, 0, y);
	sentinel(7, n);

	// Direction 8 (NW): top row X 0..MAX_X at Y=0, then left column Y=1..MAX_Y at X=0
	n = 0;
	for(int x = 0; x <= MAX_X; x++) put(8, n, x, 0);
	for(int y = 1; y <= MAX_Y; y++) put(8, n, 0, y);
	sentinel(8, n);
}

char _tmp_cTmpDirX[9] = { 0,0,1,1,1,0,-1,-1,-1 };
char _tmp_cTmpDirY[9] = { 0,-1,-1,0,1,1,1,0,-1 };

// Construction/Destruction

CGame::CGame()
{
	int x;

	BuildMoveLocTables();

	m_is_game_started = false;
	_lsock = 0;
	g_login = new LoginServer;

	// initialize configurable settings with defaults
	// Timing Settings (milliseconds)
	m_client_timeout = ClientTimeout;
	m_stamina_regen_interval = SpUpTime;
	m_poison_damage_interval = PoisonTime;
	m_health_regen_interval = HpUpTime;
	m_mana_regen_interval = MpUpTime;
	m_hunger_consume_interval = HungerTime;
	m_summon_creature_duration = SummonTime;
	m_autosave_interval = AutoSaveTime;
	m_lag_protection_interval = RagProtectionTime;

	// Character/Leveling Settings
	m_base_stat_value = 10;
	m_max_creation_stat_value = 4;
	m_creation_stat_points = 10;
	m_base_stat_total = m_base_stat_value * 6 + m_creation_stat_points;
	m_levelup_stat_gain = TotalLevelUpPoint;
	m_max_level = hb::shared::limits::PlayerMaxLevel;
	m_max_stat_value = 0; // Calculated after config load

	// Combat Settings
	m_minimum_hit_ratio = MinimumHitRatio;
	m_maximum_hit_ratio = MaximumHitRatio;

	// Gameplay Settings
	m_nighttime_duration = NightTime;
	m_grand_magic_mana_consumption = GmgManaConsumeUnit;
	m_max_construction_points = m_max_construction_points;
	m_max_summon_points = MaxSummonPoints;
	m_max_war_contribution = m_max_war_contribution;
	m_max_bank_items = 200; // Default soft cap

	m_is_drop_table_available = false;
	m_drop_tables.clear();

	for(int i = 0; i < MaxClients; i++)
		m_client_list[i] = 0;

	for(int i = 0; i < MaxMaps; i++)
		m_map_list[i] = 0;

	for(int i = 0; i < MaxItemTypes; i++)
		m_item_config_list[i] = 0;

	for(int i = 0; i < MaxNpcTypes; i++)
		m_npc_config_list[i] = 0;

	// initialize Entity Manager (MUST be before any entity operations)
	m_entity_manager = new CEntityManager();

	// get reference to EntityManager's entity array for backward compatibility
	// This allows existing code to access entities via m_npc_list
	m_npc_list = m_entity_manager->get_entity_array();
	m_entity_manager->set_map_list(m_map_list, MaxMaps);
	m_entity_manager->set_game(this);

	// initialize Gathering Managers
	m_fishing_manager = new FishingManager();
	m_fishing_manager->set_game(this);
	m_mining_manager = new MiningManager();
	m_mining_manager->set_game(this);
	m_crafting_manager = new CraftingManager();
	m_crafting_manager->set_game(this);
	m_quest_manager = new QuestManager();
	m_quest_manager->set_game(this);
	m_guild_manager = new GuildManager();
	m_guild_manager->set_game(this);
	m_delay_event_manager = new DelayEventManager();
	m_delay_event_manager->set_game(this);
	m_delay_event_manager->init_arrays();
	m_dynamic_object_manager = new DynamicObjectManager();
	m_dynamic_object_manager->set_game(this);
	m_dynamic_object_manager->init_arrays();
	m_loot_manager = new LootManager();
	m_loot_manager->set_game(this);
	m_combat_manager = new CombatManager();
	m_item_manager = new ItemManager();
	m_magic_manager = new MagicManager();
	m_skill_manager = new SkillManager();
	m_war_manager = new WarManager();
	m_status_effect_manager = new StatusEffectManager();
	m_regen_manager = new RegenManager();
	m_combat_manager->set_game(this);
	m_item_manager->set_game(this);
	m_magic_manager->set_game(this);
	m_skill_manager->set_game(this);
	m_war_manager->set_game(this);
	m_status_effect_manager->set_game(this);
	m_regen_manager->set_game(this);

	for(int i = 0; i < hb::shared::limits::MaxMagicType; i++)
		m_magic_config_list[i] = 0;

	for(int i = 0; i < hb::shared::limits::MaxSkillType; i++)
		m_skill_config_list[i] = 0;

	for(int i = 0; i < MaxNotifyMsgs; i++)
		m_notice_msg_list[i] = 0;

	//	/for(int i = 0; i < DEF_MAXTELEPORTTYPE; i++)
	//		m_pTeleportConfigList[i] = 0;

	for(int i = 0; i < hb::shared::limits::MaxBuildItems; i++)
		m_build_item_list[i] = 0;

	// New 06/05/2004
	for(int i = 0; i < MaxClients; i++) {
		m_party_info[i].total_members = 0;
		for (x = 0; x < hb::shared::limits::MaxPartyMembers; x++)
			m_party_info[i].index[x] = 0;
	}

	m_total_clients = 0;
	m_max_clients = 0;
	m_total_maps = 0;

	m_total_game_server_clients = 0;
	m_total_game_server_max_clients = 0;

	m_max_user_sys_time.hour = 0;
	m_max_user_sys_time.minute = 0;

	m_is_server_shutdown = false;
	m_shutdown_code = 0;

	m_middleland_map_index = -1;
	m_aresden_occupy_tiles = 0;
	m_elvine_occupy_tiles = 0;

	m_cur_msgs = 0;
	m_max_msgs = 0;

	m_city_status[1].crimes = 0;
	m_city_status[1].funds = 0;
	m_city_status[1].wins = 0;

	m_city_status[2].crimes = 0;
	m_city_status[2].funds = 0;
	m_city_status[2].wins = 0;

	m_auto_rebooting_count = 0;
	m_enemy_kill_mode = false;
	m_enemy_kill_adjust = 1;
	m_raid_time_monday = 0;
	m_raid_time_tuesday = 0;
	m_raid_time_wednesday = 0;
	m_raid_time_thursday = 0;
	m_raid_time_friday = 0;
	m_raid_time_saturday = 0;
	m_raid_time_sunday = 0;
	m_char_point_limit = 0;
	m_slate_success_rate = 0;
	m_force_recall_time = 0;
	m_primary_drop_rate = 1.0f;    // 1.0 = normal (10% base), 1.5 = 150%, etc.
	m_gold_drop_rate = 1.0f;       // 1.0 = normal (30% base), 1.5 = 150%, etc.
	m_secondary_drop_rate = 1.0f;  // 1.0 = normal (5% base), 1.5 = 150%, etc.

	//Show Debug hb::shared::render::Window
	//DbgWnd = new CDebugWindow();
	//DbgWnd->Startup();
	//DbgWnd->AddEventMsg("CGame Startup");
	// 2002-09-09 #1
	m_received_item_list = false;

	for(int i = 0; i < MaxClientLoginSock; i++)
		_lclients[i] = nullptr;

	m_party_manager = new class PartyManager(this);

}

CGame::~CGame()
{
	//DbgWnd->shutdown();
	//delete DbgWnd;

	for(int i = 0; i < MaxClientLoginSock; i++)
	{
		if (_lclients[i])
		{
			delete _lclients[i];
			_lclients[i] = nullptr;
		}
	}

	delete m_party_manager;

	// Cleanup Entity Manager
	if (m_entity_manager != NULL) {
		delete m_entity_manager;
		m_entity_manager = NULL;
	}

	// Cleanup Gathering Managers
	if (m_fishing_manager != nullptr) {
		delete m_fishing_manager;
		m_fishing_manager = nullptr;
	}
	if (m_mining_manager != nullptr) {
		delete m_mining_manager;
		m_mining_manager = nullptr;
	}
	if (m_crafting_manager != nullptr) {
		delete m_crafting_manager;
		m_crafting_manager = nullptr;
	}
	if (m_quest_manager != nullptr) {
		delete m_quest_manager;
		m_quest_manager = nullptr;
	}
	if (m_guild_manager != nullptr) {
		delete m_guild_manager;
		m_guild_manager = nullptr;
	}
	if (m_delay_event_manager != nullptr) {
		m_delay_event_manager->cleanup_arrays();
		delete m_delay_event_manager;
		m_delay_event_manager = nullptr;
	}
	if (m_dynamic_object_manager != nullptr) {
		m_dynamic_object_manager->cleanup_arrays();
		delete m_dynamic_object_manager;
		m_dynamic_object_manager = nullptr;
	}
	if (m_loot_manager != nullptr) {
		delete m_loot_manager;
		m_loot_manager = nullptr;
	}
	if (m_combat_manager != nullptr) {
		delete m_combat_manager;
	delete m_item_manager;
	delete m_magic_manager;
	delete m_skill_manager;
	delete m_war_manager;
	delete m_status_effect_manager;
		m_combat_manager = nullptr;
	}
}

bool CGame::accept_login(hb::shared::net::ASIOSocket* sock)
{
	if (m_is_game_started == false)
	{
		hb::logger::log("Connection closed (not initialized)");
	}
	else
	{
		for(int i = 0; i < MaxClientLoginSock; i++)
		{
			auto& p = _lclients[i];
			if (!p)
			{
				p = new LoginClient(G_pIOPool->get_context());
				sock->accept(p->sock);  // MODERNIZED: Removed WM_USER_BOT_ACCEPT message ID
				std::memset(p->ip, 0, sizeof(p->ip));
				p->sock->get_peer_address(p->ip);
				return true;
			}
		}
	}

	// MODERNIZED: Removed m_hWnd parameter
	auto tmp_sock = new hb::shared::net::ASIOSocket(G_pIOPool->get_context(), ServerSocketBlockLimit);
	sock->accept(tmp_sock);
	delete tmp_sock;

	return false;
}

bool CGame::accept(class hb::shared::net::ASIOSocket* x_sock)
{
	int totalip = 0, a;
	class hb::shared::net::ASIOSocket* tmp_sock;
	char i_pto_ban[21];
	FILE* file;
	bool close_conn = false;

	if (m_is_game_started == false)
	{
		// Fall through to CLOSE_ANYWAY
	}
	else
	{
		for(int i = 1; i < MaxClients; i++)
			if (m_client_list[i] == 0) {

				m_client_list[i] = new class CClient(G_pIOPool->get_context());
				add_client_short_cut(i);
				m_client_list[i]->m_sp_time = m_client_list[i]->m_mp_time =
					m_client_list[i]->m_hp_time = m_client_list[i]->m_auto_save_time =
					m_client_list[i]->m_time = m_client_list[i]->m_hunger_time = m_client_list[i]->m_exp_stock_time =
					m_client_list[i]->m_recent_attack_time = m_client_list[i]->m_auto_exp_time = m_client_list[i]->m_speed_hack_check_time =
					m_client_list[i]->m_afk_activity_time = GameClock::GetTimeMS();

				x_sock->accept(m_client_list[i]->m_socket);

				std::memset(m_client_list[i]->m_ip_address, 0, sizeof(m_client_list[i]->m_ip_address));
				m_client_list[i]->m_socket->get_peer_address(m_client_list[i]->m_ip_address);

				a = i;

				for(int v = 0; v < MaxBanned; v++)
				{
					if (strcmp(m_banned_list[v].banned_ip_address, m_client_list[i]->m_ip_address) == 0)
					{
						close_conn = true;
						break;
					}
				}
				if (!close_conn) {
					//centu: Anti-Downer
					for(int j = 0; j < MaxClients; j++) {
						if (m_client_list[j] != 0) {
							if (strcmp(m_client_list[j]->m_ip_address, m_client_list[i]->m_ip_address) == 0) totalip++;
						}
					}
					if (totalip > 9) {
						std::memset(i_pto_ban, 0, sizeof(i_pto_ban));
						strcpy(i_pto_ban, m_client_list[i]->m_ip_address);
						//opens cfg file
						file = fopen("gameconfigs/bannedlist.cfg", "a");
						//shows log
						hb::logger::log("Client {}: IP banned ({})", i, i_pto_ban);
						//modifys cfg file
						fprintf(file, "banned-ip = %s", i_pto_ban);
						fprintf(file, "\n");
						fclose(file);

						//updates bannedlist.cfg on the server
						for(int x = 0; x < MaxBanned; x++)
							if (strlen(m_banned_list[x].banned_ip_address) == 0)
								strcpy(m_banned_list[x].banned_ip_address, i_pto_ban);

						close_conn = true;
					}
				}

				if (close_conn) {
					delete m_client_list[a];
					m_client_list[a] = 0;
					remove_client_short_cut(a);
					return false;
				}

				hb::logger::log("Client {}: connected from {}", i, m_client_list[i]->m_ip_address);

				m_total_clients++;

				if (m_total_clients > m_max_clients) {
					m_max_clients = m_total_clients;
					//m_max_user_sys_time = hb::time::local_time::now();
					//std::snprintf(txt, sizeof(txt), "Maximum Players: %d", m_max_clients);
					//PutLogFileList(txt);
				}

				// m_client_list[client_h]->m_is_init_complete   .
				return true;
			}
	}

	tmp_sock = new class hb::shared::net::ASIOSocket(G_pIOPool->get_context(), ServerSocketBlockLimit);
	x_sock->accept(tmp_sock);
	delete tmp_sock;

	return false;
}

bool CGame::accept_login_from_async(asio::ip::tcp::socket&& peer)
{
	if (m_is_game_started == false) return false;

	for(int i = 0; i < MaxClientLoginSock; i++)
	{
		auto& p = _lclients[i];
		if (!p)
		{
			p = new LoginClient(G_pIOPool->get_context());
			p->sock->accept_from_socket(std::move(peer));
			std::memset(p->ip, 0, sizeof(p->ip));
			p->sock->get_peer_address(p->ip);
			return true;
		}
	}

	return false;
}

bool CGame::accept_from_async(asio::ip::tcp::socket&& peer)
{
	int totalip = 0, a;
	char i_pto_ban[21];
	FILE* file;
	bool close_conn = false;

	if (m_is_game_started == false) return false;

	for(int i = 1; i < MaxClients; i++)
		if (m_client_list[i] == 0) {

			m_client_list[i] = new class CClient(G_pIOPool->get_context());
			add_client_short_cut(i);
			m_client_list[i]->m_sp_time = m_client_list[i]->m_mp_time =
				m_client_list[i]->m_hp_time = m_client_list[i]->m_auto_save_time =
				m_client_list[i]->m_time = m_client_list[i]->m_hunger_time = m_client_list[i]->m_exp_stock_time =
				m_client_list[i]->m_recent_attack_time = m_client_list[i]->m_auto_exp_time = m_client_list[i]->m_speed_hack_check_time =
				m_client_list[i]->m_afk_activity_time = GameClock::GetTimeMS();

			m_client_list[i]->m_socket->accept_from_socket(std::move(peer));

			std::memset(m_client_list[i]->m_ip_address, 0, sizeof(m_client_list[i]->m_ip_address));
			m_client_list[i]->m_socket->get_peer_address(m_client_list[i]->m_ip_address);

			a = i;

			for(int v = 0; v < MaxBanned; v++)
			{
				if (strcmp(m_banned_list[v].banned_ip_address, m_client_list[i]->m_ip_address) == 0)
				{
					close_conn = true;
					break;
				}
			}

			if (!close_conn) {
				for(int j = 0; j < MaxClients; j++) {
					if (m_client_list[j] != 0) {
						if (strcmp(m_client_list[j]->m_ip_address, m_client_list[i]->m_ip_address) == 0) totalip++;
					}
				}
				if (totalip > 9) {
					std::memset(i_pto_ban, 0, sizeof(i_pto_ban));
					strcpy(i_pto_ban, m_client_list[i]->m_ip_address);
					file = fopen("gameconfigs/bannedlist.cfg", "a");
					hb::logger::log("Client {}: IP banned ({})", i, i_pto_ban);
					fprintf(file, "banned-ip = %s", i_pto_ban);
					fprintf(file, "\n");
					fclose(file);

					for(int x = 0; x < MaxBanned; x++)
						if (strlen(m_banned_list[x].banned_ip_address) == 0)
							strcpy(m_banned_list[x].banned_ip_address, i_pto_ban);

					close_conn = true;
				}
			}

			if (close_conn) {
				delete m_client_list[a];
				m_client_list[a] = 0;
				remove_client_short_cut(a);
				return false;
			}

			hb::logger::log("Client {}: connected from {}", i, m_client_list[i]->m_ip_address);

			m_total_clients++;

			if (m_total_clients > m_max_clients) {
				m_max_clients = m_total_clients;
			}

			// Set up async callbacks and start async read
			m_client_list[i]->m_socket->set_socket_index(i);
			{
				hb::shared::net::ASIOSocket* sock = m_client_list[i]->m_socket;
				m_client_list[i]->m_socket->set_callbacks(
					[this, sock](int idx, const char* data, size_t size, char key) {
						// Fast-track ping: respond immediately from I/O thread for accurate latency
						if (size >= sizeof(hb::net::PacketCommandCheckConnection)) {
							const auto* hdr = reinterpret_cast<const hb::net::PacketHeader*>(data);
							if (hdr->msg_id == MsgId::CommandCheckConnection) {
								const auto* ping = reinterpret_cast<const hb::net::PacketCommandCheckConnection*>(data);
								hb::net::PacketCommandCheckConnection resp{};
								resp.header.msg_id = MsgId::CommandCheckConnection;
								resp.header.msg_type = MsgType::Confirm;
								resp.time_ms = ping->time_ms;
								sock->send_msg(reinterpret_cast<char*>(&resp), sizeof(resp));
								// Still queue it so msg_process updates m_time and timeout tracking
							}
						}
						put_msg_queue(Source::Client, (char*)data, size, idx, key);
					},
					[](int idx, int err) {
						extern hb::shared::net::ConcurrentQueue<hb::shared::net::SocketErrorEvent> G_errorQueue;
						G_errorQueue.push(hb::shared::net::SocketErrorEvent{idx, err});
					}
				);
			}
			m_client_list[i]->m_socket->start_async_read();

			return true;
		}

	return false;
}

// MODERNIZED: No longer uses window messages, directly polls socket
void CGame::on_client_socket_event(int client_h)
{
	int ret;
	uint32_t time = GameClock::GetTimeMS();

	if (client_h <= 0) return;

	if (m_client_list[client_h] == 0) return;

	ret = m_client_list[client_h]->m_socket->Poll();  // MODERNIZED: Poll() instead of on_socket_event()

	switch (ret) {
	case sock::Event::ReadComplete:
		on_client_read(client_h);
		m_client_list[client_h]->m_time = GameClock::GetTimeMS();
		break;

	case sock::Event::Block:
		hb::logger::log("Client {}: socket blocked (send buffer full)", client_h);
		break;

	case sock::Event::ConfirmCodeNotMatch:
		hb::logger::log("Client {}: confirm code mismatch", client_h);
		delete_client(client_h, false, true);
		break;

	case sock::Event::MsgSizeTooLarge:
		hb::logger::log("Client {}: disconnected, message too large ({})", client_h, m_client_list[client_h]->m_ip_address);
		delete_client(client_h, true, true);
		break;

	case sock::Event::SocketError:
		hb::logger::log("Client {}: disconnected, socket error ({}) WSA={} last_msg=0x{:08X} age={}ms size={} char={}", client_h, m_client_list[client_h]->m_ip_address, m_client_list[client_h]->m_socket->m_WSAErr, m_client_list[client_h]->m_last_msg_id, time - m_client_list[client_h]->m_last_msg_time, m_client_list[client_h]->m_last_msg_size, m_client_list[client_h]->m_char_name);
		delete_client(client_h, true, true);
		break;

	case sock::Event::SocketClosed:
		hb::logger::log("Client {}: disconnected, socket closed ({}) WSA={} idle={}ms last_msg=0x{:08X} age={}ms size={} char={}", client_h, m_client_list[client_h]->m_ip_address, m_client_list[client_h]->m_socket->m_WSAErr, time - m_client_list[client_h]->m_time, m_client_list[client_h]->m_last_msg_id, time - m_client_list[client_h]->m_last_msg_time, m_client_list[client_h]->m_last_msg_size, m_client_list[client_h]->m_char_name);
		if ((time - m_client_list[client_h]->m_logout_hack_check) < 1000) {
			try
			{
				hb::logger::warn<log_channel::security>("Logout hack: IP={} player={}, disconnected within 10s of last damage", m_client_list[client_h]->m_ip_address, m_client_list[client_h]->m_char_name);
			}
			catch (...)
			{
			}
		}

		delete_client(client_h, true, true);
		break;

	case sock::Event::CriticalError:
		hb::logger::log("Client {}: disconnected, critical error ({}) WSA={} last_msg=0x{:08X} age={}ms size={} char={}", client_h, m_client_list[client_h]->m_ip_address, m_client_list[client_h]->m_socket->m_WSAErr, m_client_list[client_h]->m_last_msg_id, time - m_client_list[client_h]->m_last_msg_time, m_client_list[client_h]->m_last_msg_size, m_client_list[client_h]->m_char_name);
		delete_client(client_h, true, true);
		break;
	}
}

bool CGame::init()
{
	hb::time::local_time SysTime{};
	uint32_t time = GameClock::GetTimeMS();

	//CMisc::Temp();

	hb::logger::log("Initializing game server");

	for(int i = 0; i < MaxClients + 1; i++)
		m_client_shortcut[i] = 0;

	if (_lsock != 0)
		delete _lsock;

	for(int i = 0; i < MaxClients; i++)
		if (m_client_list[i] != 0) delete m_client_list[i];

	for(int i = 0; i < MaxNpcs; i++)
		if (m_npc_list[i] != 0) delete m_npc_list[i];

	for(int i = 0; i < MaxMaps; i++)
		if (m_map_list[i] != 0) delete m_map_list[i];

	for(int i = 0; i < MaxItemTypes; i++)
		if (m_item_config_list[i] != 0) delete m_item_config_list[i];

	for(int i = 0; i < MaxNpcTypes; i++)
		if (m_npc_config_list[i] != 0) delete m_npc_config_list[i];

	for(int i = 0; i < hb::shared::limits::MaxMagicType; i++)
		if (m_magic_config_list[i] != 0) delete m_magic_config_list[i];

	for(int i = 0; i < hb::shared::limits::MaxSkillType; i++)
		if (m_skill_config_list[i] != 0) delete m_skill_config_list[i];

	for(int i = 0; i < MaxNotifyMsgs; i++)
		if (m_notice_msg_list[i] != 0) delete m_notice_msg_list[i];

	//	for(int i = 0; i < DEF_MAXTELEPORTTYPE; i++)
	//	if (m_pTeleportConfigList[i] != 0) delete m_pTeleportConfigList[i];

	for(int i = 0; i < hb::shared::limits::MaxBuildItems; i++)
		if (m_build_item_list[i] != 0) delete m_build_item_list[i];

	for(int i = 0; i < MaxNpcTypes; i++)
		m_npc_construction_point[i] = 0;

	for(int i = 0; i < MaxSchedule; i++) {
		m_crusade_war_schedule[i].day = -1;
		m_crusade_war_schedule[i].hour = -1;
		m_crusade_war_schedule[i].minute = -1;
	}

	for(int i = 0; i < MaxApocalypse; i++) {
		m_apocalypse_schedule_start[i].day = -1;
		m_apocalypse_schedule_start[i].hour = -1;
		m_apocalypse_schedule_start[i].minute = -1;
	}

	for(int i = 0; i < MaxHeldenian; i++) {
		m_heldenian_schedule[i].day = -1;
		m_heldenian_schedule[i].start_hour = -1;
		m_heldenian_schedule[i].start_minute = -1;
		m_heldenian_schedule[i].end_hour = -1;
		m_heldenian_schedule[i].end_minute = -1;
	}

	for(int i = 0; i < MaxApocalypse; i++) {
		m_apocalypse_schedule_end[i].day = -1;
		m_apocalypse_schedule_end[i].hour = -1;
		m_apocalypse_schedule_end[i].minute = -1;
	}

	m_npc_construction_point[1] = 100;
	m_npc_construction_point[2] = 100;
	m_npc_construction_point[3] = 100;
	m_npc_construction_point[4] = 100;
	m_npc_construction_point[5] = 100;
	m_npc_construction_point[6] = 100;

	m_npc_construction_point[43] = 1000; // LWB
	m_npc_construction_point[44] = 2000; // GHK
	m_npc_construction_point[45] = 3000; // GHKABS
	m_npc_construction_point[46] = 2000;
	m_npc_construction_point[47] = 3000;
	m_npc_construction_point[51] = 1500; // Catapult

	m_is_game_started = false;

	_lsock = 0;

	for(int i = 0; i < MaxClients; i++)
		m_client_list[i] = 0;

	for(int i = 0; i < MaxMaps; i++)
		m_map_list[i] = 0;

	for(int i = 0; i < MaxItemTypes; i++)
		m_item_config_list[i] = 0;

	for(int i = 0; i < MaxNpcTypes; i++)
		m_npc_config_list[i] = 0;

	for(int i = 0; i < MaxNpcs; i++)
		m_npc_list[i] = 0;

	for(int i = 0; i < hb::shared::limits::MaxMagicType; i++)
		m_magic_config_list[i] = 0;

	for(int i = 0; i < hb::shared::limits::MaxSkillType; i++)
		m_skill_config_list[i] = 0;

	for(int i = 0; i < MaxNotifyMsgs; i++)
		m_notice_msg_list[i] = 0;

	//	for(int i = 0; i < DEF_MAXTELEPORTTYPE; i++)
	//		m_pTeleportConfigList[i] = 0;

	for(int i = 0; i < hb::shared::limits::MaxBuildItems; i++)
		m_build_item_list[i] = 0;

	for(int i = 0; i < hb::shared::limits::MaxCrusadeStructures; i++) {
		std::memset(m_crusade_structures[i].map_name, 0, sizeof(m_crusade_structures[i].map_name));
		m_crusade_structures[i].type = 0;
		m_crusade_structures[i].x = 0;
		m_crusade_structures[i].y = 0;
	}

	for(int i = 0; i < MaxBanned; i++) {
		std::memset(m_banned_list[i].banned_ip_address, 0, sizeof(m_banned_list[i].banned_ip_address));
	}

	for(int i = 0; i < hb::shared::limits::MaxCrusadeStructures; i++) {
		m_middle_crusade_structure_info[i].type = 0;
		m_middle_crusade_structure_info[i].side = 0;
		m_middle_crusade_structure_info[i].x = 0;
		m_middle_crusade_structure_info[i].y = 0;
	}
	m_total_middle_crusade_structures = 0;

	m_notice_data = 0;

	m_total_clients = 0;
	m_max_clients = 0;
	m_total_maps = 0;

	m_total_game_server_clients = 0;
	m_total_game_server_max_clients = 0;

	m_max_user_sys_time.hour = 0;
	m_max_user_sys_time.minute = 0;

	m_is_server_shutdown = false;
	m_shutdown_code = 0;

	m_middleland_map_index = -1;
	m_aresden_map_index = -1;
	m_elvine_map_index = -1;
	m_godh_map_index = -1;
	m_bt_field_map_index = -1;

	m_aresden_occupy_tiles = 0;
	m_elvine_occupy_tiles = 0;

	m_cur_msgs = 0;
	m_max_msgs = 0;

	m_city_status[1].crimes = 0;
	m_city_status[1].funds = 0;
	m_city_status[1].wins = 0;

	m_city_status[2].crimes = 0;
	m_city_status[2].funds = 0;
	m_city_status[2].wins = 0;

	m_strategic_status = 0;

	m_collected_mana[0] = 0;
	m_collected_mana[1] = 0;
	m_collected_mana[2] = 0;

	m_aresden_mana = 0;
	m_elvine_mana = 0;

	if (m_fishing_manager != nullptr) m_fishing_manager->m_fish_time = time;
	m_special_event_time = m_weather_time = m_equip_validation_time = m_game_time_1 =
		m_game_time_2 = m_game_time_3 = m_game_time_4 = m_game_time_5 = m_game_time_6 = time;

	m_is_special_event_time = false;

	SysTime = hb::time::local_time::now();

	// Load server_config.json (balance settings, timing, combat, etc.)
	server_config cfg;
	if (!load_server_config("server_config.json", cfg))
	{
		hb::logger::error("Cannot start server: server_config.json unavailable or invalid");
		return false;
	}
	apply_server_config(cfg);

	sqlite3* configDb = nullptr;
	std::string configDbPath;
	bool configDbCreated = false;
	hb::logger::log("Loading game data from gamedata.db");
	bool configDbReady = EnsureGameConfigDatabase(&configDb, configDbPath, &configDbCreated);
	if (!configDbReady) {
		hb::logger::error("Cannot start server: gamedata.db unavailable");
		return false;
	}
	if (configDbCreated) {
		hb::logger::error("Cannot start server: gamedata.db missing configuration data");
		CloseGameConfigDatabase(configDb);
		return false;
	}

	if (!LoadFormulas(configDb, m_formula_engine))
	{
		hb::logger::error("Cannot start server: formula data missing in gamedata.db");
		CloseGameConfigDatabase(configDb);
		return false;
	}

	auto vr = m_formula_engine.validate();
	if (!vr.success)
	{
		hb::logger::error("Formula validation failed. Press Enter to exit...");
		std::string dummy;
		std::getline(std::cin, dummy);
		CloseGameConfigDatabase(configDb);
		return false;
	}

	for (int i = 1; i < 1000; i++)
		m_level_exp_table[i] = get_level_exp(i);

	if (!LoadBannedListConfig(configDb, this)) {
		hb::logger::error("Cannot start server: banned list unavailable in gamedata.db");
		CloseGameConfigDatabase(configDb);
		return false;
	}

	LoadAdminConfig(configDb, this);
	LoadCommandPermissions(configDb, this);

	srand((unsigned)std::time(0));

	_lsock = new class hb::shared::net::ASIOSocket(G_pIOPool->get_context(), ServerSocketBlockLimit);
	_lsock->connect(m_login_listen_ip, m_login_listen_port);
	_lsock->init_buffer_size(hb::shared::limits::MsgBufferSize);

	m_on_exit_process = false;
	m_shutdown_start_time = 0;
	m_shutdown_delay_ms = 0;
	m_shutdown_next_milestone = 0;
	m_shutdown_message[0] = '\0';

	for(int i = 0; i <= 100; i++) {
		m_skill_progress_threshold[i] = m_skill_manager->calc_skill_ssn_point(i);
	}

	SysTime = hb::time::local_time::now();
	if (SysTime.minute >= m_nighttime_duration)
		m_day_or_night = 2;
	else m_day_or_night = 1;

	m_notice_data = 0;
	m_notice_data_size = 0;

	m_map_sector_info_time = time;
	m_map_sector_info_update_count = 0;

	m_crusade_count = 0;
	m_is_crusade_mode = false;
	m_is_apocalypse_mode = false;
	m_crusade_guid = 0;
	m_crusade_winner_side = 0;
	m_last_crusade_winner = 0;
	m_last_heldenian_winner = 0;
	m_last_crusade_date = -1;
	m_final_shutdown_count = 0;
	m_is_crusade_war_starter = false;
	m_is_apocalypse_starter = false;
	m_latest_crusade_day_of_week = -1;

	m_heldenian_initiated = false;
	m_heldenian_type = false;
	m_is_heldenian_mode = false;
	m_heldenian_running = false;
	m_heldenian_aresden_left_tower = 0;
	m_heldenian_mode_type = -1;
	m_last_heldenian_winner = -1;
	m_heldenian_aresden_left_tower = 0;
	m_heldenian_elvine_left_tower = 0;
	m_heldenian_aresden_dead = 0;
	m_heldenian_elvine_dead = 0;

	int msg_size = 0;
	m_is_item_available = false;
	if (HasGameConfigRows(configDb, "items")) {
		m_is_item_available = LoadItemConfigs(configDb, m_item_config_list, MaxItemTypes);
	}
	if (!m_is_item_available) {
		hb::logger::error("Cannot start server: item configs missing in gamedata.db");
		CloseGameConfigDatabase(configDb);
		return false;
	}

	// Load character creation items (optional — empty table means no starter items)
	if (HasGameConfigRows(configDb, "character_creation_items")) {
		LoadCreationItems(configDb, m_creation_items);
	}
	if (m_creation_items.empty()) {
		hb::logger::warn("No character creation items configured in gamedata.db");
	}

	// Load color palette (optional — migration script seeds the table)
	if (HasGameConfigRows(configDb, "color_palette")) {
		LoadColorPalette(configDb, m_color_palette);
	}
	if (m_color_palette.empty()) {
		hb::logger::warn("No color palette entries found in gamedata.db");
	}

	// Load attribute type multipliers (optional — migration script seeds the tables)
	if (HasGameConfigRows(configDb, "attribute_prefix_types")) {
		LoadAttributePrefixTypes(configDb, m_attribute_prefix_types);
	}
	if (HasGameConfigRows(configDb, "attribute_secondary_types")) {
		LoadAttributeSecondaryTypes(configDb, m_attribute_secondary_types);
	}
	build_multiplier_lookup();

	m_is_build_item_available = false;
	if (HasGameConfigRows(configDb, "builditem_configs")) {
		m_is_build_item_available = LoadBuildItemConfigs(configDb, this);
	}
	if (!m_is_build_item_available) {
		hb::logger::error("Cannot start server: build item configs missing in gamedata.db");
		CloseGameConfigDatabase(configDb);
		return false;
	}

	build_magic_manual_index();

	m_is_npc_available = false;
	if (HasGameConfigRows(configDb, "npc_configs")) {
		m_is_npc_available = LoadNpcConfigs(configDb, this);
	}
	if (!m_is_npc_available) {
		hb::logger::error("Cannot start server: NPC configs missing in gamedata.db");
		CloseGameConfigDatabase(configDb);
		return false;
	}

	m_is_drop_table_available = false;
	if (HasGameConfigRows(configDb, "drop_tables") && HasGameConfigRows(configDb, "drop_entries")) {
		m_is_drop_table_available = LoadDropTables(configDb, this);
	}
	if (!m_is_drop_table_available) {
		hb::logger::error("Cannot start server: drop tables missing in gamedata.db");
		CloseGameConfigDatabase(configDb);
		return false;
	}

	{
		int missingDrops = 0;
		for(int i = 0; i < MaxNpcTypes; i++) {
			const CNpc* npc = m_npc_config_list[i];
			if (npc == nullptr) {
				continue;
			}
			if (npc->m_drop_table_id == 0 &&
				(npc->m_exp_dice_max > 0 || npc->m_gold_dice_max > 0)) {
				hb::logger::warn("NPC missing drop table: {} (exp {}-{}, gold {}-{})", npc->m_npc_name, static_cast<unsigned int>(npc->m_exp_dice_min), static_cast<unsigned int>(npc->m_exp_dice_max), static_cast<unsigned int>(npc->m_gold_dice_min), static_cast<unsigned int>(npc->m_gold_dice_max));
				missingDrops++;
			}
		}
		if (missingDrops > 0) {
			hb::logger::warn("NPCs missing drop tables: {}", missingDrops);
		}
	}

	m_is_magic_available = false;
	if (HasGameConfigRows(configDb, "magic_configs")) {
		m_is_magic_available = LoadMagicConfigs(configDb, this);
	}
	if (!m_is_magic_available) {
		hb::logger::error("Cannot start server: magic configs missing in gamedata.db");
		CloseGameConfigDatabase(configDb);
		return false;
	}

	m_is_skill_available = false;
	if (HasGameConfigRows(configDb, "skill_configs")) {
		m_is_skill_available = LoadSkillConfigs(configDb, this);
	}
	if (!m_is_skill_available) {
		hb::logger::error("Cannot start server: skill configs missing in gamedata.db");
		CloseGameConfigDatabase(configDb);
		return false;
	}

	m_is_quest_available = false;
	if (HasGameConfigRows(configDb, "quest_configs")) {
		m_is_quest_available = LoadQuestConfigs(configDb, this);
	}
	if (!m_is_quest_available) {
		hb::logger::error("Cannot start server: quest configs missing in gamedata.db");
		CloseGameConfigDatabase(configDb);
		return false;
	}

	m_is_portion_available = false;
	if (HasGameConfigRows(configDb, "potion_configs") || HasGameConfigRows(configDb, "crafting_configs")) {
		m_is_portion_available = LoadPortionConfigs(configDb, this);
	}
	if (!m_is_portion_available) {
		hb::logger::error("Cannot start server: potion/crafting configs missing in gamedata.db");
		CloseGameConfigDatabase(configDb);
		return false;
	}

	// Log consolidated game data counts
	{
		int item_count = 0;
		for (int i = 0; i < MaxItemTypes; i++)
			if (m_item_config_list[i] != nullptr) item_count++;
		int build_item_count = 0;
		for (int i = 0; i < hb::shared::limits::MaxBuildItems; i++)
			if (m_build_item_list[i] != nullptr) build_item_count++;
		int npc_count = 0;
		for (int i = 0; i < MaxNpcTypes; i++)
			if (m_npc_config_list[i] != nullptr) npc_count++;
		int magic_count = 0;
		for (int i = 0; i < hb::shared::limits::MaxMagicType; i++)
			if (m_magic_config_list[i] != nullptr) magic_count++;
		int skill_count = 0;
		for (int i = 0; i < hb::shared::limits::MaxSkillType; i++)
			if (m_skill_config_list[i] != nullptr) skill_count++;
		int quest_count = 0;
		for (int i = 0; i < MaxQuestType; i++)
			if (m_quest_manager->m_quest_config_list[i] != nullptr) quest_count++;
		int potion_count = 0;
		for (int i = 0; i < MaxPortionTypes; i++)
			if (m_crafting_manager->m_portion_config_list[i] != nullptr) potion_count++;

		hb::logger::log("- {} items, {} build items", item_count, build_item_count);
		hb::logger::log("- {} NPCs, {} drop tables", npc_count, (int)m_drop_tables.size());
		hb::logger::log("- {} magic, {} skills", magic_count, skill_count);
		hb::logger::log("- {} quests, {} potions", quest_count, potion_count);
		hb::logger::log("- {} admins, {} command permission overrides", m_admin_count, (int)m_command_permissions.size());
	}

	// Load shop configurations (optional - server works without shops)
	m_is_shop_data_available = false;
	if (HasGameConfigRows(configDb, "npc_shop_mapping") || HasGameConfigRows(configDb, "shop_items")) {
		LoadShopConfigs(configDb, this);
	}
	if (!m_is_shop_data_available) {
		hb::logger::warn("Shop data not configured, NPCs will not have shop inventories");
	}

	// Load summon creature thresholds (optional)
	if (HasGameConfigRows(configDb, "summon_thresholds")) {
		LoadSummonThresholds(configDb, this);
	}
	if (m_summon_thresholds.empty()) {
		hb::logger::warn("Summon thresholds not configured, Summon spell will have no creature pool");
	}

	read_notify_msg_list_file("gameconfigs/notice.txt");
	m_notice_time = time;

	// Load active maps from gamedata.db (must be before map config loading below)
	hb::logger::log("Loading active maps from gamedata.db");
	if (!HasGameConfigRows(configDb, "active_maps") || !LoadActiveMaps(configDb, this)) {
		hb::logger::error("Cannot start server: active_maps missing in gamedata.db");
		CloseGameConfigDatabase(configDb);
		return false;
	}

	CloseGameConfigDatabase(configDb);

	// Load map configurations (display names, no-attack areas, static NPCs) from mapinfo.db
	// Must be after NPC configs are loaded from gamedata.db (spawn_map_npcs needs m_npc_config_list)
	{
		sqlite3* mapInfoDb = nullptr;
		std::string mapInfoDbPath;
		bool mapInfoDbCreated = false;

		if (!EnsureMapInfoDatabase(&mapInfoDb, mapInfoDbPath, &mapInfoDbCreated)) {
			hb::logger::error("Cannot start server: mapinfo.db not available");
			return false;
		}

		for (int i = 0; i < MaxMaps; i++)
		{
			if (m_map_list[i] != 0)
			{
				if (memcmp(m_map_list[i]->m_name, "fightzone", 9) == 0)
					m_map_list[i]->m_is_fight_zone = true;
				if (memcmp(m_map_list[i]->m_name, "icebound", 8) == 0)
					m_map_list[i]->m_is_snow_enabled = true;

				if (LoadMapConfig(mapInfoDb, m_map_list[i]->m_name, m_map_list[i])) {
					m_map_list[i]->setup_no_attack_area();
					spawn_map_npcs_from_database(mapInfoDb, i);
				}
				else {
					hb::logger::warn("- Failed to load map config for: {}", m_map_list[i]->m_name);
				}
			}
		}
		CloseMapInfoDatabase(mapInfoDb);
	}

	compute_config_hashes();

	return true;
}

void CGame::on_client_read(int client_h)
{
	char* data, key;
	size_t  msg_size;

	if (m_client_list[client_h] == 0) return;

	data = m_client_list[client_h]->m_socket->get_rcv_data_pointer(&msg_size, &key); // v1.4

	const auto* header = hb::net::PacketCast<hb::net::PacketHeader>(
		data, sizeof(hb::net::PacketHeader));
	if (header) {
		m_client_list[client_h]->m_last_msg_id = header->msg_id;
		m_client_list[client_h]->m_last_msg_time = GameClock::GetTimeMS();
		m_client_list[client_h]->m_last_msg_size = msg_size;

		// Fast-track ping responses: bypass the message queue (which only drains every 300ms)
		// so latency measurement reflects actual round-trip time
		if (header->msg_id == MsgId::CommandCheckConnection) {
			check_connection_handler(client_h, data);
			return;
		}
	}

	if (put_msg_queue(Source::Client, data, msg_size, client_h, key) == false) {
		hb::logger::error("Critical error in message queue");
	}
}

void CGame::client_motion_handler(int client_h, char* data)
{
	uint32_t client_time;
	uint16_t command, target_object_id = 0;
	short sX, sY, dX, dY, type;
	direction dir;
	int   ret, temp;

	if (m_client_list[client_h] == 0) return;
	if (m_client_list[client_h]->m_is_init_complete == false) return;
	if (m_client_list[client_h]->m_is_killed) return;

	const auto* base = hb::net::PacketCast<hb::net::PacketCommandMotionBase>(
		data, sizeof(hb::net::PacketCommandMotionBase));
	if (!base) return;
	command = base->header.msg_type;
	sX = base->x;
	sY = base->y;
	dir = static_cast<direction>(base->dir);
	dX = base->dx;
	dY = base->dy;
	type = base->type;

	if ((command == Type::Attack) || (command == Type::AttackMove)) { // v1.4
		const auto* pkt = hb::net::PacketCast<hb::net::PacketCommandMotionAttack>(
			data, sizeof(hb::net::PacketCommandMotionAttack));
		if (!pkt) return;
		target_object_id = pkt->target_id;
		client_time = pkt->time_ms;
	}
	else {
		const auto* pkt = hb::net::PacketCast<hb::net::PacketCommandMotionSimple>(
			data, sizeof(hb::net::PacketCommandMotionSimple));
		if (!pkt) return;
		client_time = pkt->time_ms;
	}

	switch (command) {
	case Type::stop:
		ret = client_motion_stop_handler(client_h, sX, sY, dir);
		if (ret == 1) {
			send_event_to_near_client_type_a(client_h, hb::shared::owner_class::Player, MsgId::EventMotion, Type::stop, 0, 0, 0);
		}
		else if (ret == 2) send_object_motion_reject_msg(client_h);
		break;

	case Type::Run:
		ret = client_motion_move_handler(client_h, sX, sY, dir, 1);
		if (ret == 1) {
			send_event_to_near_client_type_a(client_h, hb::shared::owner_class::Player, MsgId::EventMotion, Type::Run, 0, 0, 0);
		}
		else if (ret == 2) send_object_motion_reject_msg(client_h);
		if ((m_client_list[client_h] != 0) && (m_client_list[client_h]->m_hp <= 0)) m_combat_manager->client_killed_handler(client_h, 0, 0, 1); // v1.4
		// v2.171
		check_client_move_frequency(client_h, client_time);
		break;

	case Type::Move:
		ret = client_motion_move_handler(client_h, sX, sY, dir, 2);
		if (ret == 1) {
			send_event_to_near_client_type_a(client_h, hb::shared::owner_class::Player, MsgId::EventMotion, Type::Move, 0, 0, 0);
		}
		else if (ret == 2) send_object_motion_reject_msg(client_h);
		if ((m_client_list[client_h] != 0) && (m_client_list[client_h]->m_hp <= 0)) m_combat_manager->client_killed_handler(client_h, 0, 0, 1); // v1.4
		// v2.171
		check_client_move_frequency(client_h, client_time);
		break;

	case Type::DamageMove:
		ret = client_motion_move_handler(client_h, sX, sY, dir, 0);
		if (ret == 1) {
			send_event_to_near_client_type_a(client_h, hb::shared::owner_class::Player, MsgId::EventMotion, Type::DamageMove, m_client_list[client_h]->m_last_damage, 0, 0);
		}
		else if (ret == 2) send_object_motion_reject_msg(client_h);
		if ((m_client_list[client_h] != 0) && (m_client_list[client_h]->m_hp <= 0)) m_combat_manager->client_killed_handler(client_h, 0, 0, 1); // v1.4
		break;

	case Type::AttackMove:
		ret = client_motion_move_handler(client_h, sX, sY, dir, 0);
		if ((ret == 1) && (m_client_list[client_h] != 0)) {
			send_event_to_near_client_type_a(client_h, hb::shared::owner_class::Player, MsgId::EventMotion, Type::AttackMove, 0, 0, 0);
			client_motion_attack_handler(client_h, m_client_list[client_h]->m_x, m_client_list[client_h]->m_y, dX, dY, type, dir, target_object_id, client_time, false, true); // v1.4
		}
		else if (ret == 2) send_object_motion_reject_msg(client_h);
		if ((m_client_list[client_h] != 0) && (m_client_list[client_h]->m_hp <= 0)) m_combat_manager->client_killed_handler(client_h, 0, 0, 1); // v1.4
		// v2.171
		m_combat_manager->check_client_attack_frequency(client_h, client_time);
		break;

	case Type::Attack:
		m_combat_manager->check_attack_type(client_h, &type);
		ret = client_motion_attack_handler(client_h, sX, sY, dX, dY, type, dir, target_object_id, client_time); // v1.4
		if (ret == 1) {
			if (type >= 20) {
				m_client_list[client_h]->m_super_attack_left--;
				if (m_client_list[client_h]->m_super_attack_left < 0) m_client_list[client_h]->m_super_attack_left = 0;
				send_notify_msg(0, client_h, Notify::SuperAttackLeft, 0, 0, 0, 0);
			}

			send_event_to_near_client_type_a(client_h, hb::shared::owner_class::Player, MsgId::EventMotion, Type::Attack, dX, dY, type);
		}
		else if (ret == 2) send_object_motion_reject_msg(client_h);
		// v2.171
		m_combat_manager->check_client_attack_frequency(client_h, client_time);
		break;

	case Type::GetItem:
		ret = m_item_manager->client_motion_get_item_handler(client_h, sX, sY, dir);
		if (ret == 1) {
			send_event_to_near_client_type_a(client_h, hb::shared::owner_class::Player, MsgId::EventMotion, Type::GetItem, 0, 0, 0);
		}
		else if (ret == 2) send_object_motion_reject_msg(client_h);
		break;

	case Type::Magic:
		ret = m_magic_manager->client_motion_magic_handler(client_h, sX, sY, dir);
		if (ret == 1) {
			m_client_list[client_h]->m_magic_pause_time = true;
			temp = 10;
			send_event_to_near_client_type_a(client_h, hb::shared::owner_class::Player, MsgId::EventMotion, Type::Magic, dX, temp, 0);
			m_client_list[client_h]->m_spell_count++;
			m_magic_manager->check_client_magic_frequency(client_h, client_time);
		}
		else if (ret == 2) send_object_motion_reject_msg(client_h);
		break;

	default:
		break;
	}
}

int CGame::client_motion_move_handler(int client_h, short sX, short sY, direction dir, char move_type)
{
	char moveMapData[3000];
	class CTile* tile;
	uint32_t time;
	uint16_t object_id;
	short dX, dY, d_otype, top_item;
	int   ret, size, damage;
	bool  ret_ok, is_blocked = false;
	hb::net::PacketWriter writer;

	if (m_client_list[client_h] == 0) return 0;
	if ((dir <= 0) || (dir > 8))       return 0;
	if (m_client_list[client_h]->m_is_killed) return 0;
	if (m_client_list[client_h]->m_is_init_complete == false) return 0;

	if ((sX != m_client_list[client_h]->m_x) || (sY != m_client_list[client_h]->m_y)) return 2;

	//locobans
	time = GameClock::GetTimeMS();
	/*m_client_list[client_h]->m_last_action_time = time;
	if (move_type == 2) {
		if (m_client_list[client_h]->m_recent_walk_time > time) {
			m_client_list[client_h]->m_recent_walk_time = time;
			if (m_client_list[client_h]->m_v1 < 1) {
				if (m_client_list[client_h]->m_recent_walk_time < time) {
					m_client_list[client_h]->m_v1++;
				}
				else {
					is_blocked = true;
					m_client_list[client_h]->m_v1 = 0;
				}
			}
		m_client_list[client_h]->m_recent_walk_time = time;
		}
		if (is_blocked == false) m_client_list[client_h]->m_move_msg_recv_count++;
		if (m_client_list[client_h]->m_move_msg_recv_count >= 3) {
			if (m_client_list[client_h]->m_move_last_action_time != 0) {
				if ((time - m_client_list[client_h]->m_move_last_action_time) < (590)) {
					//sprintf(G_cTxt, "3.51 Walk Speeder: (%s) Player: (%s) walk difference: %d. Speed Hack?", m_client_list[client_h]->m_ip_address, m_client_list[client_h]->m_char_name, time - m_client_list[client_h]->m_move_last_action_time);
					//PutHackLogFileList(G_cTxt);
					is_blocked = true;
				}
			}
			m_client_list[client_h]->m_move_last_action_time = time;
			m_client_list[client_h]->m_move_msg_recv_count = 0;
		}
	}
	else if (move_type == 1) {
		if (m_client_list[client_h]->m_recent_run_time > time) {
			m_client_list[client_h]->m_recent_run_time = time;
			if (m_client_list[client_h]->m_v1 < 1) {
				if (m_client_list[client_h]->m_recent_run_time < time) {
					m_client_list[client_h]->m_v1++;
				}
				else {
					is_blocked = true;
					m_client_list[client_h]->m_v1 = 0;
				}
			}
		m_client_list[client_h]->m_recent_run_time = time;
		}
		if (is_blocked == false) m_client_list[client_h]->m_run_msg_recv_count++;
		if (m_client_list[client_h]->m_run_msg_recv_count >= 3) {
			if (m_client_list[client_h]->m_run_last_action_time != 0) {
				if ((time - m_client_list[client_h]->m_run_last_action_time) < (290)) {
					//sprintf(G_cTxt, "3.51 Run Speeder: (%s) Player: (%s) run difference: %d. Speed Hack?", m_client_list[client_h]->m_ip_address, m_client_list[client_h]->m_char_name, time - m_client_list[client_h]->m_run_last_action_time);
					//PutHackLogFileList(G_cTxt);
					is_blocked = true;
				}
			}
			m_client_list[client_h]->m_run_last_action_time	= time;
			m_client_list[client_h]->m_run_msg_recv_count = 0;
		}
	}*/

	int st_x, st_y;
	if (m_map_list[m_client_list[client_h]->m_map_index] != 0) {
		st_x = m_client_list[client_h]->m_x / 20;
		st_y = m_client_list[client_h]->m_y / 20;
		m_map_list[m_client_list[client_h]->m_map_index]->m_temp_sector_info[st_x][st_y].player_activity++;

		switch (m_client_list[client_h]->m_side) {
		case 0: m_map_list[m_client_list[client_h]->m_map_index]->m_temp_sector_info[st_x][st_y].neutral_activity++; break;
		case 1: m_map_list[m_client_list[client_h]->m_map_index]->m_temp_sector_info[st_x][st_y].aresden_activity++; break;
		case 2: m_map_list[m_client_list[client_h]->m_map_index]->m_temp_sector_info[st_x][st_y].elvine_activity++;  break;
		}
	}

	m_skill_manager->clear_skill_using_status(client_h);

	dX = m_client_list[client_h]->m_x;
	dY = m_client_list[client_h]->m_y;
	hb::shared::direction::ApplyOffset(dir, dX, dY);

	top_item = 0;
	ret_ok = m_map_list[m_client_list[client_h]->m_map_index]->get_moveable(dX, dY, &d_otype, &top_item);

	if (m_client_list[client_h]->m_magic_effect_status[hb::shared::magic::HoldObject] != 0)
		ret_ok = false;

	if ((ret_ok) && (is_blocked == false)) {
		if (m_client_list[client_h]->m_quest != 0) m_quest_manager->check_is_quest_completed(client_h);

		m_map_list[m_client_list[client_h]->m_map_index]->clear_owner(1, client_h, hb::shared::owner_class::Player, m_client_list[client_h]->m_x, m_client_list[client_h]->m_y);

		m_client_list[client_h]->m_x = dX;
		m_client_list[client_h]->m_y = dY;
		m_client_list[client_h]->m_dir = dir;

		m_map_list[m_client_list[client_h]->m_map_index]->set_owner((short)client_h,
			hb::shared::owner_class::Player,
			dX, dY);

		if (d_otype == dynamic_object::Spike) {
			if ((m_client_list[client_h]->m_is_neutral) && (!m_client_list[client_h]->m_appearance.is_walking)) {

			}
			else {
				damage = dice(2, 4);

				m_client_list[client_h]->m_hp -= damage;
			}
		}

		if (m_client_list[client_h]->m_hp <= 0) m_client_list[client_h]->m_hp = 0;

		writer.Reset();
		auto* pkt = writer.Append<hb::net::PacketResponseMotionMoveConfirm>();
		pkt->header.msg_id = MsgId::ResponseMotion;
		pkt->header.msg_type = Confirm::MoveConfirm;
		pkt->x = static_cast<std::int16_t>(dX - hb::shared::view::CenterX);
		pkt->y = static_cast<std::int16_t>(dY - hb::shared::view::CenterY);
		pkt->dir = static_cast<std::uint8_t>(dir);
		pkt->stamina_cost = 0;
		if (move_type == 1) {
			if (m_client_list[client_h]->m_sp > 0) {
				if (m_client_list[client_h]->m_time_left_firm_stamina == 0) {
					m_client_list[client_h]->m_sp--;
					pkt->stamina_cost = 1;
					send_notify_msg(0, client_h, Notify::Sp, 0, 0, 0, 0);
				}
			}
		}

		tile = (class CTile*)(m_map_list[m_client_list[client_h]->m_map_index]->m_tile + dX + dY * m_map_list[m_client_list[client_h]->m_map_index]->m_size_y);
		pkt->occupy_status = static_cast<std::uint8_t>(tile->m_occupy_status);
		pkt->hp = m_client_list[client_h]->m_hp;

		size = compose_move_map_data((short)(dX - hb::shared::view::CenterX), (short)(dY - hb::shared::view::CenterY), client_h, dir, moveMapData);
		writer.AppendBytes(moveMapData, static_cast<std::size_t>(size));

		ret = m_client_list[client_h]->m_socket->send_msg(writer.Data(), static_cast<int>(writer.size()));
		switch (ret) {
		case sock::Event::QueueFull:
		case sock::Event::SocketError:
		case sock::Event::CriticalError:
		case sock::Event::SocketClosed:
			delete_client(client_h, true, true);
			return 0;
		}
	}
	else {
		m_client_list[client_h]->m_is_move_blocked = true;

		m_client_list[client_h]->m_attack_last_action_time = 0;

		object_id = (uint16_t)client_h;
		writer.Reset();
		auto* pkt = writer.Append<hb::net::PacketResponseMotionMoveReject>();
		pkt->header.msg_id = MsgId::ResponseMotion;
		pkt->header.msg_type = Confirm::MoveReject;
		pkt->object_id = static_cast<std::uint16_t>(object_id);
		pkt->x = m_client_list[object_id]->m_x;
		pkt->y = m_client_list[object_id]->m_y;
		pkt->type = m_client_list[object_id]->m_type;
		pkt->dir = static_cast<std::uint8_t>(m_client_list[object_id]->m_dir);
		std::memcpy(pkt->name, m_client_list[object_id]->m_char_name, sizeof(pkt->name));
		pkt->appearance = build_broadcast_appearance(object_id);
		{
			auto pktStatus = m_client_list[object_id]->m_status;
			pktStatus.pk = (m_client_list[object_id]->m_player_kill_count != 0) ? 1 : 0;
			pktStatus.citizen = (m_client_list[object_id]->m_side != 0) ? 1 : 0;
			pktStatus.aresden = (m_client_list[object_id]->m_side == 1) ? 1 : 0;
			pktStatus.hunter = m_client_list[object_id]->m_is_player_civil ? 1 : 0;
			pktStatus.relationship = m_combat_manager->get_player_relationship(object_id, client_h);
			pkt->status = pktStatus;
		}
		pkt->padding = 0;

		ret = m_client_list[client_h]->m_socket->send_msg(writer.Data(), static_cast<int>(writer.size()));

		switch (ret) {
		case sock::Event::QueueFull:
		case sock::Event::SocketError:
		case sock::Event::CriticalError:
		case sock::Event::SocketClosed:
			delete_client(client_h, true, true);
			return 0;
		}
		// locobans
		send_event_to_near_client_type_a(client_h, hb::shared::owner_class::Player, MsgId::EventMotion, Type::NullAction, 0, 0, 0);
		return 0;
	}

	return 1;
}

void CGame::request_init_player_handler(int client_h, char* data, char key)
{
	
	char char_name[hb::shared::limits::CharNameLen], account_name[hb::shared::limits::AccountNameLen], account_password[hb::shared::limits::AccountPassLen], txt[120];
	bool is_observer_mode;

	if (m_client_list[client_h] == 0) return;
	if (m_client_list[client_h]->m_is_init_complete) return;

	std::memset(char_name, 0, sizeof(char_name));
	std::memset(account_name, 0, sizeof(account_name));
	std::memset(account_password, 0, sizeof(account_password));

	std::memset(m_client_list[client_h]->m_char_name, 0, sizeof(m_client_list[client_h]->m_char_name));
	std::memset(m_client_list[client_h]->m_account_name, 0, sizeof(m_client_list[client_h]->m_account_name));
	std::memset(m_client_list[client_h]->m_account_password, 0, sizeof(m_client_list[client_h]->m_account_password));

	const auto* req = hb::net::PacketCast<hb::net::PacketRequestInitPlayer>(
		data, sizeof(hb::net::PacketRequestInitPlayer));
	if (!req) return;

	memcpy(char_name, req->player, hb::shared::limits::CharNameLen - 1);

	std::memset(txt, 0, sizeof(txt)); // v1.4
	memcpy(txt, char_name, hb::shared::limits::CharNameLen - 1);
	std::memset(char_name, 0, sizeof(char_name));
	memcpy(char_name, txt, hb::shared::limits::CharNameLen - 1);

	//testcode
	if (strlen(txt) == 0) hb::logger::warn("request_init_player_handler: empty character name after copy");

	memcpy(account_name, req->account, hb::shared::limits::AccountNameLen - 1);

	std::memset(txt, 0, sizeof(txt)); // v1.4
	memcpy(txt, account_name, hb::shared::limits::AccountNameLen - 1);
	std::memset(account_name, 0, sizeof(account_name));
	memcpy(account_name, txt, hb::shared::limits::AccountNameLen - 1);

	// Lowercase account name to match how it was stored during account creation
	for (int ci = 0; ci < 10 && account_name[ci] != '\0'; ci++)
		account_name[ci] = static_cast<char>(::tolower(static_cast<unsigned char>(account_name[ci])));

	memcpy(account_password, req->password, hb::shared::limits::AccountPassLen - 1);

	std::memset(txt, 0, sizeof(txt)); // v1.4
	memcpy(txt, account_password, hb::shared::limits::AccountPassLen - 1);
	std::memset(account_password, 0, sizeof(account_password));
	memcpy(account_password, txt, hb::shared::limits::AccountPassLen - 1);

	is_observer_mode = (req->is_observer != 0);

	for(int i = 1; i < MaxClients; i++)
		if ((m_client_list[i] != 0) && (client_h != i) && (hb_strnicmp(m_client_list[i]->m_account_name, account_name, hb::shared::limits::AccountNameLen - 1) == 0)) {
			if (memcmp(m_client_list[i]->m_account_password, account_password, 10) == 0) {
				sprintf(G_cTxt, "<%d> Duplicate account player! Deleted with data save : CharName(%s) AccntName(%s) IP(%s)", i, m_client_list[i]->m_char_name, m_client_list[i]->m_account_name, m_client_list[i]->m_ip_address);
				hb::logger::log("{}", G_cTxt);
				//PutLogFileList(G_cTxt);
				delete_client(i, true, true, false);
			}
			else {
				memcpy(m_client_list[client_h]->m_char_name, char_name, hb::shared::limits::CharNameLen - 1);
				memcpy(m_client_list[client_h]->m_account_name, account_name, hb::shared::limits::AccountNameLen - 1);
				memcpy(m_client_list[client_h]->m_account_password, account_password, hb::shared::limits::AccountPassLen - 1);

				delete_client(client_h, false, false, false);
				return;
			}
		}

	for(int i = 1; i < MaxClients; i++)
		if ((m_client_list[i] != 0) && (client_h != i) && (hb_strnicmp(m_client_list[i]->m_char_name, char_name, hb::shared::limits::CharNameLen - 1) == 0)) {
			if (memcmp(m_client_list[i]->m_account_password, account_password, 10) == 0) {
				sprintf(G_cTxt, "<%d> Duplicate player! Deleted with data save : CharName(%s) IP(%s)", i, m_client_list[i]->m_char_name, m_client_list[i]->m_ip_address);
				hb::logger::log("{}", G_cTxt);
				//PutLogFileList(G_cTxt);
				delete_client(i, true, true, false);
			}
			else {
				memcpy(m_client_list[client_h]->m_char_name, char_name, hb::shared::limits::CharNameLen - 1);
				memcpy(m_client_list[client_h]->m_account_name, account_name, hb::shared::limits::AccountNameLen - 1);
				memcpy(m_client_list[client_h]->m_account_password, account_password, hb::shared::limits::AccountPassLen - 1);

				delete_client(client_h, false, false);
				return;
			}
		}

	memcpy(m_client_list[client_h]->m_char_name, char_name, hb::shared::limits::CharNameLen - 1);
	memcpy(m_client_list[client_h]->m_account_name, account_name, hb::shared::limits::AccountNameLen - 1);
	memcpy(m_client_list[client_h]->m_account_password, account_password, hb::shared::limits::AccountPassLen - 1);

	m_client_list[client_h]->m_is_observer_mode = is_observer_mode;

	// Admin validation
	m_client_list[client_h]->m_admin_index = -1;
	m_client_list[client_h]->m_admin_level = 0;
	m_client_list[client_h]->m_is_gm_mode = false;
	int admin_idx = find_admin_by_account(account_name);
	if (admin_idx != -1 && hb_stricmp(m_admin_list[admin_idx].m_char_name, char_name) == 0)
	{
		if (strcmp(m_admin_list[admin_idx].approved_ip, "0.0.0.0") == 0)
		{
			strncpy(m_admin_list[admin_idx].approved_ip, m_client_list[client_h]->m_ip_address, 20);
			m_admin_list[admin_idx].approved_ip[20] = '\0';
			hb::logger::log("Admin IP auto-set: account={} ip={}", account_name, m_client_list[client_h]->m_ip_address);

			sqlite3* configDb = nullptr;
			std::string dbPath;
			if (EnsureGameConfigDatabase(&configDb, dbPath, nullptr))
			{
				SaveAdminConfig(configDb, this);
				CloseGameConfigDatabase(configDb);
			}

			m_client_list[client_h]->m_admin_index = admin_idx;
			m_client_list[client_h]->m_admin_level = m_admin_list[admin_idx].m_admin_level;
		}
		else if (strcmp(m_admin_list[admin_idx].approved_ip, m_client_list[client_h]->m_ip_address) == 0)
		{
			m_client_list[client_h]->m_admin_index = admin_idx;
			m_client_list[client_h]->m_admin_level = m_admin_list[admin_idx].m_admin_level;
		}
		else
		{
			hb::logger::error("Admin IP mismatch for account {} (expected {}, got {})", account_name, m_admin_list[admin_idx].approved_ip, m_client_list[client_h]->m_ip_address);
			delete_client(client_h, false, false, false);
			return;
		}
	}

	init_player_data(client_h, 0, 0); //send_msg_to_ls(ServerMsgId::RequestPlayerData, client_h);
}

// 05/22/2004 - Hypnotoad - sends client to proper location after dieing
void CGame::request_init_data_handler(int client_h, char* data, char key, size_t msg_size)
{
	char player_name[hb::shared::limits::CharNameLen], txt[120];
	int summon_points;
	int total_item_a, total_item_b, size, ret, stats;
	hb::time::local_time SysTime{};
	hb::net::PacketWriter writer;
	char initMapData[hb::shared::limits::MsgBufferSize + 1];

	if (m_client_list[client_h] == 0) return;

	const auto* req = hb::net::PacketCast<hb::net::PacketRequestInitPlayer>(
		data, sizeof(hb::net::PacketRequestInitPlayer));
	if (!req) {
		return;
	}

	std::memset(player_name, 0, sizeof(player_name));
	memcpy(player_name, req->player, hb::shared::limits::CharNameLen - 1);

	std::memset(txt, 0, sizeof(txt)); // v1.4
	memcpy(txt, player_name, hb::shared::limits::CharNameLen - 1);
	std::memset(player_name, 0, sizeof(player_name));
	memcpy(player_name, txt, hb::shared::limits::CharNameLen - 1);

	if (hb_strnicmp(m_client_list[client_h]->m_char_name, player_name, hb::shared::limits::CharNameLen - 1) != 0) {
		delete_client(client_h, false, true);
		return;
	}

	// Send configs FIRST so the client has item/magic/skill definitions
	// before receiving player data that references them.
	std::string clientItemHash, clientMagicHash, clientSkillHash, clientNpcHash, clientMapHash, clientBalanceHash, clientColorPaletteHash, clientAttributeTypeHash;
	if (msg_size >= sizeof(hb::net::PacketRequestInitDataEx)) {
		const auto* exReq = reinterpret_cast<const hb::net::PacketRequestInitDataEx*>(data);
		clientItemHash = exReq->itemConfigHash;
		clientMagicHash = exReq->magicConfigHash;
		clientSkillHash = exReq->skillConfigHash;
		clientNpcHash = exReq->npcConfigHash;
		clientMapHash = exReq->mapConfigHash;
		clientBalanceHash = exReq->balanceConfigHash;
		clientColorPaletteHash = exReq->colorPaletteConfigHash;
		clientAttributeTypeHash = exReq->attributeTypeConfigHash;
	}

	bool item_cache_valid    = (!clientItemHash.empty() && clientItemHash == m_config_hash[0]);
	bool magic_cache_valid   = (!clientMagicHash.empty() && clientMagicHash == m_config_hash[1]);
	bool skill_cache_valid   = (!clientSkillHash.empty() && clientSkillHash == m_config_hash[2]);
	bool npc_cache_valid     = (!clientNpcHash.empty() && clientNpcHash == m_config_hash[3]);
	bool map_cache_valid     = (!clientMapHash.empty() && clientMapHash == m_config_hash[4]);
	bool balance_cache_valid = (!clientBalanceHash.empty() && clientBalanceHash == m_config_hash[5]);
	bool color_palette_cache_valid = (!clientColorPaletteHash.empty() && clientColorPaletteHash == m_config_hash[6]);
	bool attribute_type_cache_valid = (!clientAttributeTypeHash.empty() && clientAttributeTypeHash == m_config_hash[7]);

	{
		hb::net::PacketResponseConfigCacheStatus cacheStatus{};
		cacheStatus.header.msg_id = MSGID_RESPONSE_CONFIGCACHESTATUS;
		cacheStatus.header.msg_type = MsgType::Confirm;
		cacheStatus.itemCacheValid = item_cache_valid ? 1 : 0;
		cacheStatus.magicCacheValid = magic_cache_valid ? 1 : 0;
		cacheStatus.skillCacheValid = skill_cache_valid ? 1 : 0;
		cacheStatus.npcCacheValid = npc_cache_valid ? 1 : 0;
		cacheStatus.mapCacheValid = map_cache_valid ? 1 : 0;
		cacheStatus.balanceCacheValid = balance_cache_valid ? 1 : 0;
		cacheStatus.colorPaletteCacheValid = color_palette_cache_valid ? 1 : 0;
		cacheStatus.attributeTypeCacheValid = attribute_type_cache_valid ? 1 : 0;
		m_client_list[client_h]->m_socket->send_msg(
			reinterpret_cast<char*>(&cacheStatus), sizeof(cacheStatus));
	}

	if (!item_cache_valid)    m_item_manager->send_client_item_configs(client_h);
	if (!magic_cache_valid)   m_magic_manager->send_client_magic_configs(client_h);
	if (!skill_cache_valid)   m_skill_manager->send_client_skill_configs(client_h);
	if (!npc_cache_valid)     send_client_npc_configs(client_h);
	if (!map_cache_valid)     send_client_map_configs(client_h);
	if (!balance_cache_valid) send_client_balance_config(client_h);
	if (!color_palette_cache_valid) send_client_color_palette(client_h);
	if (!attribute_type_cache_valid) send_client_attribute_types(client_h);

	// Now send player data (configs are guaranteed loaded on client)
	writer.Reset();
	auto* char_pkt = writer.Append<hb::net::PacketResponsePlayerCharacterContents>();
	char_pkt->header.msg_id = MsgId::PlayerCharacterContents;
	char_pkt->header.msg_type = MsgType::Confirm;
	char_pkt->hp = m_client_list[client_h]->m_hp;
	char_pkt->mp = m_client_list[client_h]->m_mp;
	char_pkt->sp = m_client_list[client_h]->m_sp;
	char_pkt->ac = m_client_list[client_h]->m_defense_ratio;
	char_pkt->thac0 = m_client_list[client_h]->m_hit_ratio;
	char_pkt->level = m_client_list[client_h]->m_level;
	char_pkt->str = m_client_list[client_h]->m_str;
	char_pkt->intel = m_client_list[client_h]->m_int;
	char_pkt->vit = m_client_list[client_h]->m_vit;
	char_pkt->dex = m_client_list[client_h]->m_dex;
	char_pkt->mag = m_client_list[client_h]->m_mag;
	char_pkt->chr = m_client_list[client_h]->m_charisma;

	stats = (m_client_list[client_h]->m_str + m_client_list[client_h]->m_dex + m_client_list[client_h]->m_vit +
		m_client_list[client_h]->m_int + m_client_list[client_h]->m_mag + m_client_list[client_h]->m_charisma);

	m_client_list[client_h]->m_levelup_pool = (m_client_list[client_h]->m_level - 1) * m_levelup_stat_gain - (stats - m_base_stat_total);
	char_pkt->lu_point = static_cast<std::uint16_t>(m_client_list[client_h]->m_levelup_pool);
	char_pkt->lu_unused[0] = static_cast<std::uint8_t>(m_client_list[client_h]->m_var);
	char_pkt->lu_unused[1] = 0;
	char_pkt->lu_unused[2] = 0;
	char_pkt->lu_unused[3] = 0;
	char_pkt->lu_unused[4] = 0;
	char_pkt->exp = m_client_list[client_h]->m_exp;
	char_pkt->enemy_kills = m_client_list[client_h]->m_enemy_kill_count;
	char_pkt->pk_count = m_client_list[client_h]->m_player_kill_count;
	char_pkt->reward_gold = m_client_list[client_h]->m_reward_gold;
	std::memcpy(char_pkt->location, m_client_list[client_h]->m_location, sizeof(char_pkt->location));
	char_pkt->super_attack_left = static_cast<std::uint8_t>(m_client_list[client_h]->m_super_attack_left);
	char_pkt->max_stats = m_max_stat_value;
	char_pkt->max_level = m_max_level;
	char_pkt->max_bank_items = m_max_bank_items;

	//hbest
	m_client_list[client_h]->is_force_set = false;
	m_client_list[client_h]->m_party_id = 0;
	m_client_list[client_h]->m_party_status = PartyStatus::Null;
	m_client_list[client_h]->m_req_join_party_client_h = 0;

	ret = m_client_list[client_h]->m_socket->send_msg(writer.Data(), static_cast<int>(writer.size()));
	switch (ret) {
	case sock::Event::QueueFull:
	case sock::Event::SocketError:
	case sock::Event::CriticalError:
	case sock::Event::SocketClosed:
		delete_client(client_h, true, true);
		return;
	}

	writer.Reset();
	auto* item_header = writer.Append<hb::net::PacketResponseItemListHeader>();
	item_header->header.msg_id = MsgId::PlayerItemListContents;
	item_header->header.msg_type = MsgType::Confirm;

	total_item_a = 0;
	for(int i = 0; i < hb::shared::limits::MaxItems; i++)
		if (m_client_list[client_h]->m_item_list[i] != 0)
			total_item_a++;

	item_header->item_count = static_cast<std::uint8_t>(total_item_a);

	for(int i = 0; i < total_item_a; i++) {
		// ### ERROR POINT!!!
		if (m_client_list[client_h]->m_item_list[i] == 0) {
			sprintf(G_cTxt, "request_init_data_handler error: Client(%s) Item(%d)", m_client_list[client_h]->m_char_name, i);
			hb::logger::log<log_channel::events>("{}", G_cTxt);

			delete_client(client_h, false, true);
			return;
		}
		auto* entry = writer.Append<hb::net::PacketResponseItemListEntry>();
		std::memcpy(entry->name, m_client_list[client_h]->m_item_list[i]->m_name, sizeof(entry->name));
		entry->count = m_client_list[client_h]->m_item_list[i]->m_instance.count;
		entry->item_type = m_client_list[client_h]->m_item_list[i]->m_item_type;
		entry->equip_pos = m_client_list[client_h]->m_item_list[i]->m_equip_pos;
		entry->is_equipped = static_cast<std::uint8_t>(m_client_list[client_h]->m_is_item_equipped[i]);
		entry->level_limit = m_client_list[client_h]->m_item_list[i]->m_level_requirement;
		entry->gender_limit = m_client_list[client_h]->m_item_list[i]->m_gender_requirement;
		entry->cur_durability = m_client_list[client_h]->m_item_list[i]->m_instance.cur_durability;
		entry->weight = m_client_list[client_h]->m_item_list[i]->m_weight;
		entry->item_color = m_client_list[client_h]->m_item_list[i]->m_instance.item_color;
		entry->spec_value2 = static_cast<std::uint8_t>(m_client_list[client_h]->m_item_list[i]->m_instance.special_effect_value2);
		m_client_list[client_h]->m_item_list[i]->copy_attributes_to(*entry);
		entry->item_id = m_client_list[client_h]->m_item_list[i]->m_id_num;
		entry->max_durability = m_client_list[client_h]->m_item_list[i]->m_durability;
	}

	total_item_b = 0;
	for(int i = 0; i < hb::shared::limits::MaxBankItems; i++)
		if (m_client_list[client_h]->m_item_in_bank_list[i] != 0)
			total_item_b++;

	auto* bank_header = writer.Append<hb::net::PacketResponseBankItemListHeader>();
	bank_header->bank_item_count = static_cast<std::uint16_t>(total_item_b);

	for(int i = 0; i < total_item_b; i++) {
		if (m_client_list[client_h]->m_item_in_bank_list[i] == 0) {
			sprintf(G_cTxt, "request_init_data_handler error: Client(%s) Bank-Item(%d)", m_client_list[client_h]->m_char_name, i);
			hb::logger::log<log_channel::events>("{}", G_cTxt);

			delete_client(client_h, false, true);
			return;
		}
		auto* entry = writer.Append<hb::net::PacketResponseBankItemEntry>();
		std::memcpy(entry->name, m_client_list[client_h]->m_item_in_bank_list[i]->m_name, sizeof(entry->name));
		entry->count = m_client_list[client_h]->m_item_in_bank_list[i]->m_instance.count;
		entry->item_type = m_client_list[client_h]->m_item_in_bank_list[i]->m_item_type;
		entry->equip_pos = m_client_list[client_h]->m_item_in_bank_list[i]->m_equip_pos;
		entry->level_limit = m_client_list[client_h]->m_item_in_bank_list[i]->m_level_requirement;
		entry->gender_limit = m_client_list[client_h]->m_item_in_bank_list[i]->m_gender_requirement;
		entry->cur_durability = m_client_list[client_h]->m_item_in_bank_list[i]->m_instance.cur_durability;
		entry->weight = m_client_list[client_h]->m_item_in_bank_list[i]->m_weight;
		entry->item_color = m_client_list[client_h]->m_item_in_bank_list[i]->m_instance.item_color;
		entry->spec_value2 = static_cast<std::uint8_t>(m_client_list[client_h]->m_item_in_bank_list[i]->m_instance.special_effect_value2);
		m_client_list[client_h]->m_item_in_bank_list[i]->copy_attributes_to(*entry);
		entry->item_id = m_client_list[client_h]->m_item_in_bank_list[i]->m_id_num;
		entry->max_durability = m_client_list[client_h]->m_item_in_bank_list[i]->m_durability;
	}

	auto* mastery = writer.Append<hb::net::PacketResponseMasteryData>();
	std::memcpy(mastery->magic_mastery, m_client_list[client_h]->m_magic_mastery, hb::shared::limits::MaxMagicType);
	std::memcpy(mastery->skill_mastery, m_client_list[client_h]->m_skill_mastery, hb::shared::limits::MaxSkillType);

	ret = m_client_list[client_h]->m_socket->send_msg(writer.Data(), static_cast<int>(writer.size()));
	switch (ret) {
	case sock::Event::QueueFull:
	case sock::Event::SocketError:
	case sock::Event::CriticalError:
	case sock::Event::SocketClosed:
		delete_client(client_h, true, true);
		return;
	}

	// Send item positions right after item list so positions are applied
	// while items are still freshly initialized
	send_notify_msg(0, client_h, Notify::ItemPosList, 0, 0, 0, 0);

	writer.Reset();
	auto* init_header = writer.Append<hb::net::PacketResponseInitDataHeader>();
	init_header->header.msg_id = MsgId::ResponseInitData;
	init_header->header.msg_type = MsgType::Confirm;

	if (m_client_list[client_h]->m_is_observer_mode == false)
	{
		// When dest is -1,-1 (teleport to initial points), resolve to a random spawn first
		// so get_empty_position searches near a valid point instead of (-1,-1).
		// Use the destination map's location_name (not player's m_location) to ensure
		// randomization even for players with location "NONE".
		if (m_client_list[client_h]->m_x == -1 && m_client_list[client_h]->m_y == -1)
		{
			get_map_initial_point(m_client_list[client_h]->m_map_index,
				&m_client_list[client_h]->m_x, &m_client_list[client_h]->m_y,
				m_map_list[m_client_list[client_h]->m_map_index]->m_location_name);
		}
		get_empty_position(&m_client_list[client_h]->m_x, &m_client_list[client_h]->m_y, m_client_list[client_h]->m_map_index);
	}
	else get_map_initial_point(m_client_list[client_h]->m_map_index, &m_client_list[client_h]->m_x, &m_client_list[client_h]->m_y);

	init_header->player_object_id = static_cast<std::int16_t>(client_h);
	init_header->pivot_x = static_cast<std::int16_t>(m_client_list[client_h]->m_x - hb::shared::view::PlayerPivotOffsetX);
	init_header->pivot_y = static_cast<std::int16_t>(m_client_list[client_h]->m_y - hb::shared::view::PlayerPivotOffsetY);
	init_header->player_type = m_client_list[client_h]->m_type;
	init_header->appearance = build_broadcast_appearance(client_h);
	init_header->status = m_client_list[client_h]->m_status;
	std::memcpy(init_header->map_name, m_client_list[client_h]->m_map_name, sizeof(init_header->map_name));
	std::memcpy(init_header->cur_location, m_map_list[m_client_list[client_h]->m_map_index]->m_location_name, sizeof(init_header->cur_location));

	if (m_map_list[m_client_list[client_h]->m_map_index]->m_is_fixed_day_mode)
		init_header->sprite_alpha = 1;
	else init_header->sprite_alpha = m_day_or_night;

	if (m_map_list[m_client_list[client_h]->m_map_index]->m_is_fixed_day_mode)
		init_header->weather_status = 0;
	else init_header->weather_status = m_map_list[m_client_list[client_h]->m_map_index]->m_weather_status;

	init_header->contribution = m_client_list[client_h]->m_contribution;

	if (m_client_list[client_h]->m_is_observer_mode == false) {
		m_map_list[m_client_list[client_h]->m_map_index]->set_owner(client_h,
			hb::shared::owner_class::Player,
			m_client_list[client_h]->m_x,
			m_client_list[client_h]->m_y);
	}

	init_header->observer_mode = static_cast<std::uint8_t>(m_client_list[client_h]->m_is_observer_mode);
	init_header->rating = m_client_list[client_h]->m_rating;
	init_header->hp = m_client_list[client_h]->m_hp;
	init_header->discount = 0;

	size = compose_init_map_data(m_client_list[client_h]->m_x - hb::shared::view::CenterX, m_client_list[client_h]->m_y - hb::shared::view::CenterY, client_h, initMapData);
	writer.AppendBytes(initMapData, static_cast<std::size_t>(size));

	ret = m_client_list[client_h]->m_socket->send_msg(writer.Data(), static_cast<int>(writer.size()));
	switch (ret) {
	case sock::Event::QueueFull:
	case sock::Event::SocketError:
	case sock::Event::CriticalError:
	case sock::Event::SocketClosed:
		delete_client(client_h, true, true);
		return;
	}

	send_event_to_near_client_type_a(client_h, hb::shared::owner_class::Player, MsgId::EventLog, MsgType::Confirm, 0, 0, 0);

	// v2.13 
	if ((memcmp(m_client_list[client_h]->m_location, "are", 3) == 0) &&
		(memcmp(m_map_list[m_client_list[client_h]->m_map_index]->m_location_name, "elvine", 6) == 0)
		) {

		m_client_list[client_h]->m_war_begin_time = GameClock::GetTimeMS();
		m_client_list[client_h]->m_is_war_location = true;
		// v2.17 2002-7-15
		set_force_recall_time(client_h);
	}
	// v2.13 
	else if ((memcmp(m_client_list[client_h]->m_location, "elv", 3) == 0) &&
		(memcmp(m_map_list[m_client_list[client_h]->m_map_index]->m_location_name, "aresden", 7) == 0)
		) {

		m_client_list[client_h]->m_war_begin_time = GameClock::GetTimeMS();
		m_client_list[client_h]->m_is_war_location = true;

		// v2.17 2002-7-15
		set_force_recall_time(client_h);
	}
	else if (((memcmp(m_map_list[m_client_list[client_h]->m_map_index]->m_location_name, "arejail", 7) == 0) ||
		(memcmp(m_map_list[m_client_list[client_h]->m_map_index]->m_location_name, "elvjail", 7) == 0))
		) {
		m_client_list[client_h]->m_is_war_location = true;
		m_client_list[client_h]->m_war_begin_time = GameClock::GetTimeMS();

		// v2.17 2002-7-15 
		if (m_client_list[client_h]->m_time_left_force_recall == 0) {
			m_client_list[client_h]->m_time_left_force_recall = 20 * 5;
		}
		else if (m_client_list[client_h]->m_time_left_force_recall > 20 * 5) {
			m_client_list[client_h]->m_time_left_force_recall = 20 * 5;
		}

	}
	else
	{
		m_client_list[client_h]->m_is_war_location = false;
		// v1.42
		m_client_list[client_h]->m_time_left_force_recall = 0;
		// 06/11/2004
		set_force_recall_time(client_h);
	}

	// v2.17 2002-7-15
	//hbest...
	if ((m_client_list[client_h]->m_time_left_force_recall > 0) && (m_client_list[client_h]->m_is_war_location) && is_enemy_zone(client_h)) {
		send_notify_msg(0, client_h, Notify::ForceRecallTime, m_client_list[client_h]->m_time_left_force_recall, 0, 0, 0);
		//sprintf(G_cTxt,"(!) Game Server Force Recall Time  %d (%d)min", m_client_list[client_h]->m_time_left_force_recall, m_client_list[client_h]->m_time_left_force_recall/20) ;
		//PutLogList(G_cTxt) ;
	}

	if (m_client_list[client_h]->m_gizon_item_upgrade_left < 0) {
		m_client_list[client_h]->m_gizon_item_upgrade_left = 0;
	}

	// No entering enemy shops
	int mapside, mapside2;

	mapside = get_map_location_side(m_map_list[m_client_list[client_h]->m_map_index]->m_name);
	if (mapside > 3) mapside2 = mapside - 2;
	else mapside2 = mapside;
	m_client_list[client_h]->m_is_inside_own_town = false;
	if ((m_client_list[client_h]->m_side != mapside2) && (mapside != 0)) {
		if ((mapside <= 2)) {
			if (m_client_list[client_h]->m_side != 0) {
				m_client_list[client_h]->m_war_begin_time = GameClock::GetTimeMS();
				m_client_list[client_h]->m_is_war_location = true;
				m_client_list[client_h]->m_time_left_force_recall = 1;
				m_client_list[client_h]->m_is_inside_own_town = true;
			}
		}
	}
	else {
		{
			if (memcmp(m_map_list[m_client_list[client_h]->m_map_index]->m_location_name, "arejail", 7) == 0 ||
				memcmp(m_map_list[m_client_list[client_h]->m_map_index]->m_location_name, "elvjail", 7) == 0) {
				m_client_list[client_h]->m_is_war_location = true;
				m_client_list[client_h]->m_war_begin_time = GameClock::GetTimeMS();
				if (m_client_list[client_h]->m_time_left_force_recall == 0)
					m_client_list[client_h]->m_time_left_force_recall = 100;
				else if (m_client_list[client_h]->m_time_left_force_recall > 100)
					m_client_list[client_h]->m_time_left_force_recall = 100;
			}
		}
	}

	/*if ((m_client_list[client_h]->m_time_left_force_recall > 0) &&
		(m_client_list[client_h]->m_is_war_location )) {
		send_notify_msg(0, client_h, Notify::ForceRecallTime, m_client_list[client_h]->m_time_left_force_recall, 0, 0, 0);
	}*/

	send_notify_msg(0, client_h, Notify::SafeAttackMode, 0, 0, 0, 0);
	send_notify_msg(0, client_h, Notify::DownSkillIndexSet, m_client_list[client_h]->m_down_skill_index, 0, 0, 0);

	m_quest_manager->send_quest_contents(client_h);
	m_quest_manager->check_quest_environment(client_h);

	// v1.432
	if (m_client_list[client_h]->m_special_ability_time == 0) {
		send_notify_msg(0, client_h, Notify::SpecialAbilityEnabled, 0, 0, 0, 0);
	}

	// Crusade 
	if (m_is_crusade_mode) {
		if (m_client_list[client_h]->m_crusade_guid == 0) {
			m_client_list[client_h]->m_crusade_duty = 0;
			m_client_list[client_h]->m_construction_point = 0;
			m_client_list[client_h]->m_crusade_guid = m_crusade_guid;
		}
		else if (m_client_list[client_h]->m_crusade_guid != m_crusade_guid) {
			m_client_list[client_h]->m_crusade_duty = 0;
			m_client_list[client_h]->m_construction_point = 0;
			m_client_list[client_h]->m_war_contribution = 0;
			m_client_list[client_h]->m_crusade_guid = m_crusade_guid;
			send_notify_msg(0, client_h, Notify::Crusade, (uint32_t)m_is_crusade_mode, 0, 0, 0, -1);
		}
		m_client_list[client_h]->m_var = 1;
		send_notify_msg(0, client_h, Notify::Crusade, (uint32_t)m_is_crusade_mode, m_client_list[client_h]->m_crusade_duty, 0, 0);
	}
	else if (m_is_heldenian_mode) {
		summon_points = m_client_list[client_h]->m_charisma * 300;
		if (summon_points > m_max_summon_points) summon_points = m_max_summon_points;
		if (m_client_list[client_h]->m_heldenian_guid == 0) {
			m_client_list[client_h]->m_heldenian_guid = m_heldenian_guid;
			m_client_list[client_h]->m_construction_point = summon_points;
		}
		else if (m_client_list[client_h]->m_heldenian_guid != m_heldenian_guid) {
			m_client_list[client_h]->m_construction_point = summon_points;
			m_client_list[client_h]->m_war_contribution = 0;
			m_client_list[client_h]->m_heldenian_guid = m_heldenian_guid;
		}
		m_client_list[client_h]->m_var = 2;
		if (m_is_heldenian_mode) {
			send_notify_msg(0, client_h, Notify::Crusade, 0, 0, 0, 0);
			if (m_heldenian_initiated == false) {
				send_notify_msg(0, client_h, Notify::HeldenianStart, 0, 0, 0, 0);
			}
			send_notify_msg(0, client_h, Notify::ConstructionPoint, m_client_list[client_h]->m_construction_point, m_client_list[client_h]->m_war_contribution, 0, 0);
			m_war_manager->update_heldenian_status();
		}
	}
	else if ((m_client_list[client_h]->m_var == 1) && (m_client_list[client_h]->m_crusade_guid == m_crusade_guid)) {
		m_client_list[client_h]->m_crusade_duty = 0;
		m_client_list[client_h]->m_construction_point = 0;
	}
	else {
		if (m_client_list[client_h]->m_crusade_guid == m_crusade_guid) {
			if (m_client_list[client_h]->m_var == 1) {
				send_notify_msg(0, client_h, Notify::Crusade, (uint32_t)m_is_crusade_mode, 0, 0, 0, -1);
			}
		}
		else {
			send_notify_msg(0, client_h, Notify::Crusade, (uint32_t)m_is_crusade_mode, 0, 0, 0, -1);
			m_client_list[client_h]->m_crusade_guid = 0;
			m_client_list[client_h]->m_war_contribution = 0;
			m_client_list[client_h]->m_crusade_guid = 0;
		}
	}

	// v1.42
	if (memcmp(m_client_list[client_h]->m_map_name, "fightzone", 9) == 0) {
		sprintf(G_cTxt, "Char(%s)-Enter(%s) Observer(%d)", m_client_list[client_h]->m_char_name, m_client_list[client_h]->m_map_name, m_client_list[client_h]->m_is_observer_mode);
		hb::logger::log<log_channel::log_events>("{}", G_cTxt);
	}

	if (m_is_heldenian_mode) send_notify_msg(0, client_h, Notify::HeldenianTeleport, 0, 0, 0, 0, 0);
	if (m_heldenian_initiated) send_notify_msg(0, client_h, Notify::HeldenianStart, 0, 0, 0, 0, 0);

	// Crusade
	send_notify_msg(0, client_h, Notify::ConstructionPoint, m_client_list[client_h]->m_construction_point, m_client_list[client_h]->m_war_contribution, 1, 0);
	//Fix Sprite Bug
	//			send_event_to_near_client_type_a(client_h, hb::shared::owner_class::Player, MsgId::EventMotion, Type::NullAction, 0, 0, 0);
	//Gizon point lefT???
	send_notify_msg(0, client_h, Notify::GizonItemUpgradeLeft, m_client_list[client_h]->m_gizon_item_upgrade_left, 0, 0, 0);

	if ((m_is_apocalypse_mode) && (m_map_list[m_client_list[client_h]->m_map_index]->m_is_apocalypse_map)) {
		request_teleport_handler(client_h, "1   ");
	}

	if (m_is_apocalypse_mode) {
		send_notify_msg(0, client_h, Notify::ApocGateStartMsg, 0, 0, 0, 0, 0);
	}

	send_notify_msg(0, client_h, Notify::Hunger, m_client_list[client_h]->m_hunger_status, 0, 0, 0);
	send_notify_msg(0, client_h, Notify::SuperAttackLeft, 0, 0, 0, 0);

	request_noticement_handler(client_h); // send noticement when log in
}

bool CGame::send_client_npc_configs(int client_h)
{
	if (m_client_list[client_h] == 0) {
		return false;
	}

	constexpr size_t maxPacketSize = 7000;
	constexpr size_t headerSize = sizeof(hb::net::PacketNpcConfigHeader);
	constexpr size_t entrySize = sizeof(hb::net::PacketNpcConfigEntry);
	constexpr size_t maxEntriesPerPacket = (maxPacketSize - headerSize) / entrySize;

	// Count total NPCs
	int totalNpcs = 0;
	for(int i = 0; i < MaxNpcTypes; i++) {
		if (m_npc_config_list[i] != 0) {
			totalNpcs++;
		}
	}

	// Send NPCs in packets
	int npcsSent = 0;
	int packetIndex = 0;

	while (npcsSent < totalNpcs) {
		std::memset(G_cData50000, 0, sizeof(G_cData50000));

		auto* pktHeader = reinterpret_cast<hb::net::PacketNpcConfigHeader*>(G_cData50000);
		pktHeader->header.msg_id = MsgId::NpcConfigContents;
		pktHeader->header.msg_type = MsgType::Confirm;
		pktHeader->totalNpcs = static_cast<uint16_t>(totalNpcs);
		pktHeader->packetIndex = static_cast<uint16_t>(packetIndex);

		auto* entries = reinterpret_cast<hb::net::PacketNpcConfigEntry*>(G_cData50000 + headerSize);

		uint16_t entriesInPacket = 0;
		int skipped = 0;

		for(int i = 0; i < MaxNpcTypes && entriesInPacket < maxEntriesPerPacket; i++) {
			if (m_npc_config_list[i] == 0) {
				continue;
			}

			if (skipped < npcsSent) {
				skipped++;
				continue;
			}

			const CNpc* npc = m_npc_config_list[i];
			auto& entry = entries[entriesInPacket];

			entry.npcId = static_cast<int16_t>(i);
			entry.npcType = npc->m_type;
			std::memset(entry.name, 0, sizeof(entry.name));
			std::snprintf(entry.name, sizeof(entry.name), "%s", npc->m_npc_name);

			entriesInPacket++;
		}

		pktHeader->npcCount = entriesInPacket;
		size_t packetSize = headerSize + (entriesInPacket * entrySize);

		int ret = m_client_list[client_h]->m_socket->send_msg(G_cData50000, static_cast<int>(packetSize));
		switch (ret) {
		case sock::Event::QueueFull:
		case sock::Event::SocketError:
		case sock::Event::CriticalError:
		case sock::Event::SocketClosed:
			hb::logger::log("Failed to send NPC configs: Client({}) Packet({})", client_h, packetIndex);
			delete_client(client_h, true, true);
			delete m_client_list[client_h];
			m_client_list[client_h] = 0;
			return false;
		}

		npcsSent += entriesInPacket;
		packetIndex++;
	}

	return true;
}

bool CGame::send_client_map_configs(int client_h)
{
	if (m_client_list[client_h] == 0) {
		return false;
	}

	constexpr size_t maxPacketSize = 7000;
	constexpr size_t headerSize = sizeof(hb::net::PacketMapConfigHeader);
	constexpr size_t entrySize = sizeof(hb::net::PacketMapConfigEntry);
	constexpr size_t maxEntriesPerPacket = (maxPacketSize - headerSize) / entrySize;

	int totalMaps = 0;
	for (int i = 0; i < MaxMaps; i++) {
		if (m_map_list[i] != 0 && m_map_list[i]->m_display_name[0] != '\0') totalMaps++;
	}

	int mapsSent = 0;
	int packetIndex = 0;

	while (mapsSent < totalMaps) {
		std::memset(G_cData50000, 0, sizeof(G_cData50000));

		auto* pktHeader = reinterpret_cast<hb::net::PacketMapConfigHeader*>(G_cData50000);
		pktHeader->header.msg_id = MsgId::MapConfigContents;
		pktHeader->header.msg_type = MsgType::Confirm;
		pktHeader->totalMaps = static_cast<uint16_t>(totalMaps);
		pktHeader->packetIndex = static_cast<uint16_t>(packetIndex);

		auto* entries = reinterpret_cast<hb::net::PacketMapConfigEntry*>(G_cData50000 + headerSize);

		uint16_t entriesInPacket = 0;
		int skipped = 0;

		for (int i = 0; i < MaxMaps && entriesInPacket < maxEntriesPerPacket; i++) {
			if (m_map_list[i] == 0 || m_map_list[i]->m_display_name[0] == '\0') continue;
			if (skipped < mapsSent) { skipped++; continue; }

			auto& entry = entries[entriesInPacket];
			std::memset(&entry, 0, sizeof(entry));
			std::snprintf(entry.map_name, sizeof(entry.map_name), "%s", m_map_list[i]->m_name);
			std::snprintf(entry.display_name, sizeof(entry.display_name), "%s", m_map_list[i]->m_display_name);
			entriesInPacket++;
		}

		pktHeader->mapCount = entriesInPacket;
		size_t packetSize = headerSize + (entriesInPacket * entrySize);

		int ret = m_client_list[client_h]->m_socket->send_msg(G_cData50000, static_cast<int>(packetSize));
		switch (ret) {
		case sock::Event::QueueFull:
		case sock::Event::SocketError:
		case sock::Event::CriticalError:
		case sock::Event::SocketClosed:
			hb::logger::log("Failed to send map configs: Client({}) Packet({})", client_h, packetIndex);
			delete_client(client_h, true, true);
			delete m_client_list[client_h];
			m_client_list[client_h] = 0;
			return false;
		}

		mapsSent += entriesInPacket;
		packetIndex++;
	}

	return true;
}

void CGame::build_magic_manual_index()
{
	m_magic_to_manual_item.clear();
	for (int i = 0; i < MaxItemTypes; i++)
	{
		if (m_item_config_list[i] == nullptr) continue;
		if (m_item_config_list[i]->get_item_effect_type() != ItemEffectType::StudyMagic) continue;
		int magic_idx = m_item_config_list[i]->m_item_effect_value1;
		if (magic_idx >= 0 && magic_idx < hb::shared::limits::MaxMagicType)
			m_magic_to_manual_item[magic_idx] = m_item_config_list[i]->m_id_num;
	}
	hb::logger::log("Built magic-to-manual index: {} entries", m_magic_to_manual_item.size());
}

void CGame::compute_config_hashes()
{
	// Compute SHA256 for item configs
	{
		constexpr size_t headerSize = sizeof(hb::net::PacketItemConfigHeader);
		constexpr size_t entrySize = sizeof(hb::net::PacketItemConfigEntry);
		constexpr size_t maxEntriesPerPacket = (7000 - headerSize) / entrySize;

		std::vector<uint8_t> allData;
		int totalItems = 0;
		for(int i = 0; i < MaxItemTypes; i++) {
			if (m_item_config_list[i] != 0) totalItems++;
		}

		int itemsSent = 0;
		int packetIndex = 0;
		while (itemsSent < totalItems) {
			char buf[7000]{};
			auto* pktHeader = reinterpret_cast<hb::net::PacketItemConfigHeader*>(buf);
			pktHeader->header.msg_id = MsgId::ItemConfigContents;
			pktHeader->header.msg_type = MsgType::Confirm;
			pktHeader->totalItems = static_cast<uint16_t>(totalItems);
			pktHeader->packetIndex = static_cast<uint16_t>(packetIndex);

			auto* entries = reinterpret_cast<hb::net::PacketItemConfigEntry*>(buf + headerSize);
			uint16_t entriesInPacket = 0;
			int skipped = 0;

			for(int i = 0; i < MaxItemTypes && entriesInPacket < maxEntriesPerPacket; i++) {
				if (m_item_config_list[i] == 0) continue;
				if (skipped < itemsSent) { skipped++; continue; }

				const CItem* item = m_item_config_list[i];
				auto& entry = entries[entriesInPacket];
				entry.itemId = item->m_id_num;
				std::memset(entry.name, 0, sizeof(entry.name));
				std::snprintf(entry.name, sizeof(entry.name), "%s", item->m_name);
				entry.itemType = item->m_item_type;
				entry.itemSubType = item->m_item_sub_type;
				entry.equipPos = item->m_equip_pos;
				entry.weaponClass = item->m_weapon_class;
				entry.effectType = item->m_item_effect_type;
				entry.effectValue1 = item->m_item_effect_value1;
				entry.effectValue2 = item->m_item_effect_value2;
				entry.effectValue3 = item->m_item_effect_value3;
				entry.effectValue4 = item->m_item_effect_value4;
				entry.effectValue5 = item->m_item_effect_value5;
				entry.effectValue6 = item->m_item_effect_value6;
				entry.durability = item->m_durability;
				entry.specialEffect = item->m_special_effect;
				entry.sellPrice = item->m_sell_price;
				entry.weight = item->m_weight;
				entry.swingSpeed = item->m_swing_speed;
				entry.levelRequirement = item->m_level_requirement;
				entry.genderRequirement = item->m_gender_requirement;
				entry.specialEffectValue1 = item->m_special_effect_value1;
				entry.specialEffectValue2 = item->m_special_effect_value2;
				entry.relatedSkill = item->m_related_skill;
				entry.hideArmor = item->m_hide_armor;
				entry.isSkirt = item->m_is_skirt;
				entry.stackable = item->m_stackable;
				entry.isDyeable = item->m_is_dyeable;
				entry.setId = item->m_set_id;
				entry.itemColor = item->m_instance.item_color;
				entry.displayId = item->m_display_id;
				entriesInPacket++;
			}

			pktHeader->itemCount = entriesInPacket;
			size_t packetSize = headerSize + (entriesInPacket * entrySize);

			uint16_t len = static_cast<uint16_t>(packetSize);
			const uint8_t* lenBytes = reinterpret_cast<const uint8_t*>(&len);
			allData.push_back(lenBytes[0]);
			allData.push_back(lenBytes[1]);
			allData.insert(allData.end(), reinterpret_cast<uint8_t*>(buf), reinterpret_cast<uint8_t*>(buf) + packetSize);

			itemsSent += entriesInPacket;
			packetIndex++;
		}
		m_config_hash[0] = allData.empty() ? std::string{} : hb::shared::util::sha256(allData.data(), allData.size());
	}

	// Compute SHA256 for magic configs
	{
		constexpr size_t headerSize = sizeof(hb::net::PacketMagicConfigHeader);
		constexpr size_t entrySize = sizeof(hb::net::PacketMagicConfigEntry);
		constexpr size_t maxEntriesPerPacket = (7000 - headerSize) / entrySize;

		std::vector<uint8_t> allData;
		int totalMagics = 0;
		for(int i = 0; i < hb::shared::limits::MaxMagicType; i++) {
			if (m_magic_config_list[i] != 0) totalMagics++;
		}

		int magicsSent = 0;
		int packetIndex = 0;
		while (magicsSent < totalMagics) {
			char buf[7000]{};
			auto* pktHeader = reinterpret_cast<hb::net::PacketMagicConfigHeader*>(buf);
			pktHeader->header.msg_id = MsgId::MagicConfigContents;
			pktHeader->header.msg_type = MsgType::Confirm;
			pktHeader->totalMagics = static_cast<uint16_t>(totalMagics);
			pktHeader->packetIndex = static_cast<uint16_t>(packetIndex);

			auto* entries = reinterpret_cast<hb::net::PacketMagicConfigEntry*>(buf + headerSize);
			uint16_t entriesInPacket = 0;
			int skipped = 0;

			for(int i = 0; i < hb::shared::limits::MaxMagicType && entriesInPacket < maxEntriesPerPacket; i++) {
				if (m_magic_config_list[i] == 0) continue;
				if (skipped < magicsSent) { skipped++; continue; }

				const CMagic* magic = m_magic_config_list[i];
				auto& entry = entries[entriesInPacket];
				entry.magicId = static_cast<int16_t>(i);
				std::memset(entry.name, 0, sizeof(entry.name));
				std::snprintf(entry.name, sizeof(entry.name), "%s", magic->m_name);
				entry.manaCost = magic->m_value_1;
				entry.intLimit = magic->m_intelligence_limit;
				entry.goldCost = magic->m_gold_cost;
				entry.visible = (magic->m_gold_cost >= 0) ? 1 : 0;
				entry.magicType = magic->m_type;
				entry.aoeRadiusX = magic->m_value_2;
				entry.aoeRadiusY = magic->m_value_3;
				entry.dynamicPattern = magic->m_value_11;
				entry.dynamicRadius = magic->m_value_12;
				entriesInPacket++;
			}

			pktHeader->magicCount = entriesInPacket;
			size_t packetSize = headerSize + (entriesInPacket * entrySize);

			uint16_t len = static_cast<uint16_t>(packetSize);
			const uint8_t* lenBytes = reinterpret_cast<const uint8_t*>(&len);
			allData.push_back(lenBytes[0]);
			allData.push_back(lenBytes[1]);
			allData.insert(allData.end(), reinterpret_cast<uint8_t*>(buf), reinterpret_cast<uint8_t*>(buf) + packetSize);

			magicsSent += entriesInPacket;
			packetIndex++;
		}
		m_config_hash[1] = allData.empty() ? std::string{} : hb::shared::util::sha256(allData.data(), allData.size());
	}

	// Compute SHA256 for skill configs
	{
		constexpr size_t headerSize = sizeof(hb::net::PacketSkillConfigHeader);
		constexpr size_t entrySize = sizeof(hb::net::PacketSkillConfigEntry);
		constexpr size_t maxEntriesPerPacket = (7000 - headerSize) / entrySize;

		std::vector<uint8_t> allData;
		int totalSkills = 0;
		for(int i = 0; i < hb::shared::limits::MaxSkillType; i++) {
			if (m_skill_config_list[i] != 0) totalSkills++;
		}

		int skillsSent = 0;
		int packetIndex = 0;
		while (skillsSent < totalSkills) {
			char buf[7000]{};
			auto* pktHeader = reinterpret_cast<hb::net::PacketSkillConfigHeader*>(buf);
			pktHeader->header.msg_id = MsgId::SkillConfigContents;
			pktHeader->header.msg_type = MsgType::Confirm;
			pktHeader->totalSkills = static_cast<uint16_t>(totalSkills);
			pktHeader->packetIndex = static_cast<uint16_t>(packetIndex);

			auto* entries = reinterpret_cast<hb::net::PacketSkillConfigEntry*>(buf + headerSize);
			uint16_t entriesInPacket = 0;
			int skipped = 0;

			for(int i = 0; i < hb::shared::limits::MaxSkillType && entriesInPacket < maxEntriesPerPacket; i++) {
				if (m_skill_config_list[i] == 0) continue;
				if (skipped < skillsSent) { skipped++; continue; }

				const CSkill* skill = m_skill_config_list[i];
				auto& entry = entries[entriesInPacket];
				entry.skillId = static_cast<int16_t>(i);
				std::memset(entry.name, 0, sizeof(entry.name));
				std::snprintf(entry.name, sizeof(entry.name), "%s", skill->m_name);
				entry.useable = skill->m_is_useable ? 1 : 0;
				entry.useMethod = skill->m_use_method;
				entriesInPacket++;
			}

			pktHeader->skillCount = entriesInPacket;
			size_t packetSize = headerSize + (entriesInPacket * entrySize);

			uint16_t len = static_cast<uint16_t>(packetSize);
			const uint8_t* lenBytes = reinterpret_cast<const uint8_t*>(&len);
			allData.push_back(lenBytes[0]);
			allData.push_back(lenBytes[1]);
			allData.insert(allData.end(), reinterpret_cast<uint8_t*>(buf), reinterpret_cast<uint8_t*>(buf) + packetSize);

			skillsSent += entriesInPacket;
			packetIndex++;
		}
		m_config_hash[2] = allData.empty() ? std::string{} : hb::shared::util::sha256(allData.data(), allData.size());
	}

	// Compute SHA256 for NPC configs
	{
		constexpr size_t headerSize = sizeof(hb::net::PacketNpcConfigHeader);
		constexpr size_t entrySize = sizeof(hb::net::PacketNpcConfigEntry);
		constexpr size_t maxEntriesPerPacket = (7000 - headerSize) / entrySize;

		std::vector<uint8_t> allData;
		int totalNpcs = 0;
		for(int i = 0; i < MaxNpcTypes; i++) {
			if (m_npc_config_list[i] != 0) totalNpcs++;
		}

		int npcsSent = 0;
		int packetIndex = 0;
		while (npcsSent < totalNpcs) {
			char buf[7000]{};
			auto* pktHeader = reinterpret_cast<hb::net::PacketNpcConfigHeader*>(buf);
			pktHeader->header.msg_id = MsgId::NpcConfigContents;
			pktHeader->header.msg_type = MsgType::Confirm;
			pktHeader->totalNpcs = static_cast<uint16_t>(totalNpcs);
			pktHeader->packetIndex = static_cast<uint16_t>(packetIndex);

			auto* entries = reinterpret_cast<hb::net::PacketNpcConfigEntry*>(buf + headerSize);
			uint16_t entriesInPacket = 0;
			int skipped = 0;

			for(int i = 0; i < MaxNpcTypes && entriesInPacket < maxEntriesPerPacket; i++) {
				if (m_npc_config_list[i] == 0) continue;
				if (skipped < npcsSent) { skipped++; continue; }

				const CNpc* npc = m_npc_config_list[i];
				auto& entry = entries[entriesInPacket];
				entry.npcId = static_cast<int16_t>(i);
				entry.npcType = npc->m_type;
				std::memset(entry.name, 0, sizeof(entry.name));
				std::snprintf(entry.name, sizeof(entry.name), "%s", npc->m_npc_name);
				entriesInPacket++;
			}

			pktHeader->npcCount = entriesInPacket;
			size_t packetSize = headerSize + (entriesInPacket * entrySize);

			uint16_t len = static_cast<uint16_t>(packetSize);
			const uint8_t* lenBytes = reinterpret_cast<const uint8_t*>(&len);
			allData.push_back(lenBytes[0]);
			allData.push_back(lenBytes[1]);
			allData.insert(allData.end(), reinterpret_cast<uint8_t*>(buf), reinterpret_cast<uint8_t*>(buf) + packetSize);

			npcsSent += entriesInPacket;
			packetIndex++;
		}
		m_config_hash[3] = allData.empty() ? std::string{} : hb::shared::util::sha256(allData.data(), allData.size());
	}

	// Compute SHA256 for map display name configs
	{
		constexpr size_t headerSize = sizeof(hb::net::PacketMapConfigHeader);
		constexpr size_t entrySize = sizeof(hb::net::PacketMapConfigEntry);
		constexpr size_t maxEntriesPerPacket = (7000 - headerSize) / entrySize;

		std::vector<uint8_t> allData;
		int totalMaps = 0;
		for (int i = 0; i < MaxMaps; i++) {
			if (m_map_list[i] != 0 && m_map_list[i]->m_display_name[0] != '\0') totalMaps++;
		}

		int mapsSent = 0;
		int packetIndex = 0;
		while (mapsSent < totalMaps) {
			char buf[7000]{};
			auto* pktHeader = reinterpret_cast<hb::net::PacketMapConfigHeader*>(buf);
			pktHeader->header.msg_id = MsgId::MapConfigContents;
			pktHeader->header.msg_type = MsgType::Confirm;
			pktHeader->totalMaps = static_cast<uint16_t>(totalMaps);
			pktHeader->packetIndex = static_cast<uint16_t>(packetIndex);

			auto* entries = reinterpret_cast<hb::net::PacketMapConfigEntry*>(buf + headerSize);
			uint16_t entriesInPacket = 0;
			int skipped = 0;

			for (int i = 0; i < MaxMaps && entriesInPacket < maxEntriesPerPacket; i++) {
				if (m_map_list[i] == 0 || m_map_list[i]->m_display_name[0] == '\0') continue;
				if (skipped < mapsSent) { skipped++; continue; }

				auto& entry = entries[entriesInPacket];
				std::memset(&entry, 0, sizeof(entry));
				std::snprintf(entry.map_name, sizeof(entry.map_name), "%s", m_map_list[i]->m_name);
				std::snprintf(entry.display_name, sizeof(entry.display_name), "%s", m_map_list[i]->m_display_name);
				entriesInPacket++;
			}

			pktHeader->mapCount = entriesInPacket;
			size_t packetSize = headerSize + (entriesInPacket * entrySize);

			uint16_t len = static_cast<uint16_t>(packetSize);
			const uint8_t* lenBytes = reinterpret_cast<const uint8_t*>(&len);
			allData.push_back(lenBytes[0]);
			allData.push_back(lenBytes[1]);
			allData.insert(allData.end(), reinterpret_cast<uint8_t*>(buf), reinterpret_cast<uint8_t*>(buf) + packetSize);

			mapsSent += entriesInPacket;
			packetIndex++;
		}
		m_config_hash[4] = allData.empty() ? std::string{} : hb::shared::util::sha256(allData.data(), allData.size());
	}

	compute_balance_hash();
	compute_color_palette_hash();
	compute_attribute_types_hash();

	hb::logger::log("Config hashes computed:");
	hb::logger::log("- Items: {}", m_config_hash[0]);
	hb::logger::log("- Magic: {}", m_config_hash[1]);
	hb::logger::log("- Skills: {}", m_config_hash[2]);
	hb::logger::log("- Npcs: {}", m_config_hash[3]);
	hb::logger::log("- Maps: {}", m_config_hash[4]);
	hb::logger::log("- Balance: {}", m_config_hash[5]);
	hb::logger::log("- ColorPalette: {}", m_config_hash[6]);
	hb::logger::log("- AttributeTypes: {}", m_config_hash[7]);
}

void CGame::compute_balance_hash()
{
	auto serialized = m_formula_engine.serialize();
	if (serialized.empty())
	{
		m_config_hash[5].clear();
	}
	else
	{
		m_config_hash[5] = hb::shared::util::sha256(serialized.data(), serialized.size());
	}
}

bool CGame::send_client_balance_config(int client_h)
{
	if (m_client_list[client_h] == nullptr) return false;

	auto serialized = m_formula_engine.serialize();
	if (serialized.empty()) return false;

	// Send as a single packet: header + serialized data
	std::vector<char> buf(sizeof(hb::net::PacketHeader) + serialized.size(), 0);
	auto* header = reinterpret_cast<hb::net::PacketHeader*>(buf.data());
	header->msg_id = MsgId::BalanceConfigContents;
	header->msg_type = MsgType::Confirm;
	std::memcpy(buf.data() + sizeof(hb::net::PacketHeader), serialized.data(), serialized.size());

	m_client_list[client_h]->m_socket->send_msg(buf.data(), static_cast<int>(buf.size()));
	return true;
}

void CGame::compute_color_palette_hash()
{
	if (m_color_palette.empty())
	{
		m_config_hash[6].clear();
		return;
	}

	// Build the same packet data the client would receive, then hash it
	constexpr size_t headerSize = sizeof(hb::net::PacketColorPaletteConfigHeader);
	constexpr size_t entrySize = sizeof(hb::net::PacketColorPaletteConfigEntry);
	size_t totalColors = m_color_palette.size();

	std::vector<uint8_t> allData;
	size_t colorsSent = 0;
	uint16_t packetIndex = 0;
	constexpr size_t maxEntriesPerPacket = (7000 - headerSize) / entrySize;

	while (colorsSent < totalColors)
	{
		char buf[7000]{};
		auto* pktHeader = reinterpret_cast<hb::net::PacketColorPaletteConfigHeader*>(buf);
		pktHeader->header.msg_id = MsgId::ColorPaletteConfigContents;
		pktHeader->header.msg_type = MsgType::Confirm;
		pktHeader->totalColors = static_cast<uint16_t>(totalColors);
		pktHeader->packetIndex = packetIndex;

		auto* entries = reinterpret_cast<hb::net::PacketColorPaletteConfigEntry*>(buf + headerSize);
		uint16_t entriesInPacket = 0;

		for (size_t i = colorsSent; i < totalColors && entriesInPacket < maxEntriesPerPacket; i++)
		{
			entries[entriesInPacket].colorId = m_color_palette[i].color_id;
			entries[entriesInPacket].r = m_color_palette[i].r;
			entries[entriesInPacket].g = m_color_palette[i].g;
			entries[entriesInPacket].b = m_color_palette[i].b;
			entriesInPacket++;
		}

		pktHeader->colorCount = entriesInPacket;
		size_t packetSize = headerSize + (entriesInPacket * entrySize);
		allData.insert(allData.end(), buf, buf + packetSize);

		colorsSent += entriesInPacket;
		packetIndex++;
	}

	m_config_hash[6] = allData.empty() ? std::string{} : hb::shared::util::sha256(allData.data(), allData.size());
}

bool CGame::send_client_color_palette(int client_h)
{
	if (m_client_list[client_h] == nullptr) return false;
	if (m_color_palette.empty()) return false;

	constexpr size_t maxPacketSize = 7000;
	constexpr size_t headerSize = sizeof(hb::net::PacketColorPaletteConfigHeader);
	constexpr size_t entrySize = sizeof(hb::net::PacketColorPaletteConfigEntry);
	constexpr size_t maxEntriesPerPacket = (maxPacketSize - headerSize) / entrySize;

	size_t totalColors = m_color_palette.size();
	size_t colorsSent = 0;
	uint16_t packetIndex = 0;

	while (colorsSent < totalColors)
	{
		std::memset(G_cData50000, 0, sizeof(G_cData50000));

		auto* pktHeader = reinterpret_cast<hb::net::PacketColorPaletteConfigHeader*>(G_cData50000);
		pktHeader->header.msg_id = MsgId::ColorPaletteConfigContents;
		pktHeader->header.msg_type = MsgType::Confirm;
		pktHeader->totalColors = static_cast<uint16_t>(totalColors);
		pktHeader->packetIndex = packetIndex;

		auto* entries = reinterpret_cast<hb::net::PacketColorPaletteConfigEntry*>(G_cData50000 + headerSize);
		uint16_t entriesInPacket = 0;

		for (size_t i = colorsSent; i < totalColors && entriesInPacket < maxEntriesPerPacket; i++)
		{
			entries[entriesInPacket].colorId = m_color_palette[i].color_id;
			entries[entriesInPacket].r = m_color_palette[i].r;
			entries[entriesInPacket].g = m_color_palette[i].g;
			entries[entriesInPacket].b = m_color_palette[i].b;
			entriesInPacket++;
		}

		pktHeader->colorCount = entriesInPacket;
		size_t packetSize = headerSize + (entriesInPacket * entrySize);

		int ret = m_client_list[client_h]->m_socket->send_msg(G_cData50000, static_cast<int>(packetSize));
		switch (ret) {
		case sock::Event::QueueFull:
		case sock::Event::SocketError:
		case sock::Event::CriticalError:
		case sock::Event::SocketClosed:
			hb::logger::log("Failed to send color palette: Client({})", client_h);
			delete_client(client_h, true, true);
			delete m_client_list[client_h];
			m_client_list[client_h] = 0;
			return false;
		}

		colorsSent += entriesInPacket;
		packetIndex++;
	}

	return true;
}

void CGame::build_multiplier_lookup()
{
	// Default all to 1 (safe fallback)
	for (int i = 0; i < 16; i++)
	{
		m_prefix_multiplier[i] = 1;
		m_prefix_min_value[i] = 1;
		m_prefix_max_value[i] = 13;
		m_secondary_multiplier[i] = 1;
		m_secondary_min_value[i] = 1;
		m_secondary_max_value[i] = 13;
	}
	// None types get 0
	m_prefix_multiplier[0] = 0;
	m_prefix_min_value[0] = 0;
	m_prefix_max_value[0] = 0;
	m_secondary_multiplier[0] = 0;
	m_secondary_min_value[0] = 0;
	m_secondary_max_value[0] = 0;

	for (const auto& e : m_attribute_prefix_types)
	{
		if (e.prefix_id < 16)
		{
			m_prefix_multiplier[e.prefix_id] = e.multiplier;
			m_prefix_min_value[e.prefix_id] = e.min_value;
			m_prefix_max_value[e.prefix_id] = e.max_value;
		}
	}
	for (const auto& e : m_attribute_secondary_types)
	{
		if (e.secondary_id < 16)
		{
			m_secondary_multiplier[e.secondary_id] = e.multiplier;
			m_secondary_min_value[e.secondary_id] = e.min_value;
			m_secondary_max_value[e.secondary_id] = e.max_value;
		}
	}

	hb::logger::log("Attribute multiplier lookup built: {} prefix, {} secondary entries",
		(int)m_attribute_prefix_types.size(), (int)m_attribute_secondary_types.size());
}

void CGame::compute_attribute_types_hash()
{
	if (m_attribute_prefix_types.empty() && m_attribute_secondary_types.empty())
	{
		m_config_hash[7].clear();
		return;
	}

	constexpr size_t headerSize = sizeof(hb::net::PacketAttributeTypeConfigHeader);
	constexpr size_t prefixEntrySize = sizeof(hb::net::PacketAttributePrefixTypeEntry);
	constexpr size_t secondaryEntrySize = sizeof(hb::net::PacketAttributeSecondaryTypeEntry);

	std::vector<uint8_t> allData;

	// Build prefix packet data
	if (!m_attribute_prefix_types.empty())
	{
		char buf[7000]{};
		auto* pktHeader = reinterpret_cast<hb::net::PacketAttributeTypeConfigHeader*>(buf);
		pktHeader->header.msg_id = MsgId::AttributeTypeConfigContents;
		pktHeader->header.msg_type = MsgType::Confirm;
		pktHeader->totalEntries = static_cast<uint16_t>(m_attribute_prefix_types.size());
		pktHeader->packetIndex = 0;
		pktHeader->entryType = 0;

		auto* entries = reinterpret_cast<hb::net::PacketAttributePrefixTypeEntry*>(buf + headerSize);
		uint16_t count = 0;
		for (const auto& e : m_attribute_prefix_types)
		{
			entries[count].prefix_id = e.prefix_id;
			entries[count].multiplier = e.multiplier;
			count++;
		}
		pktHeader->entryCount = count;
		size_t packetSize = headerSize + (count * prefixEntrySize);
		allData.insert(allData.end(), buf, buf + packetSize);
	}

	// Build secondary packet data
	if (!m_attribute_secondary_types.empty())
	{
		char buf[7000]{};
		auto* pktHeader = reinterpret_cast<hb::net::PacketAttributeTypeConfigHeader*>(buf);
		pktHeader->header.msg_id = MsgId::AttributeTypeConfigContents;
		pktHeader->header.msg_type = MsgType::Confirm;
		pktHeader->totalEntries = static_cast<uint16_t>(m_attribute_secondary_types.size());
		pktHeader->packetIndex = 0;
		pktHeader->entryType = 1;

		auto* entries = reinterpret_cast<hb::net::PacketAttributeSecondaryTypeEntry*>(buf + headerSize);
		uint16_t count = 0;
		for (const auto& e : m_attribute_secondary_types)
		{
			entries[count].secondary_id = e.secondary_id;
			entries[count].multiplier = e.multiplier;
			count++;
		}
		pktHeader->entryCount = count;
		size_t packetSize = headerSize + (count * secondaryEntrySize);
		allData.insert(allData.end(), buf, buf + packetSize);
	}

	m_config_hash[7] = allData.empty() ? std::string{} : hb::shared::util::sha256(allData.data(), allData.size());
}

bool CGame::send_client_attribute_types(int client_h)
{
	if (m_client_list[client_h] == nullptr) return false;
	if (m_attribute_prefix_types.empty() && m_attribute_secondary_types.empty()) return false;

	constexpr size_t headerSize = sizeof(hb::net::PacketAttributeTypeConfigHeader);
	constexpr size_t prefixEntrySize = sizeof(hb::net::PacketAttributePrefixTypeEntry);
	constexpr size_t secondaryEntrySize = sizeof(hb::net::PacketAttributeSecondaryTypeEntry);

	// Send prefix entries
	if (!m_attribute_prefix_types.empty())
	{
		std::memset(G_cData50000, 0, sizeof(G_cData50000));

		auto* pktHeader = reinterpret_cast<hb::net::PacketAttributeTypeConfigHeader*>(G_cData50000);
		pktHeader->header.msg_id = MsgId::AttributeTypeConfigContents;
		pktHeader->header.msg_type = MsgType::Confirm;
		pktHeader->totalEntries = static_cast<uint16_t>(m_attribute_prefix_types.size());
		pktHeader->packetIndex = 0;
		pktHeader->entryType = 0;

		auto* entries = reinterpret_cast<hb::net::PacketAttributePrefixTypeEntry*>(G_cData50000 + headerSize);
		uint16_t count = 0;
		for (const auto& e : m_attribute_prefix_types)
		{
			entries[count].prefix_id = e.prefix_id;
			entries[count].multiplier = e.multiplier;
			count++;
		}
		pktHeader->entryCount = count;
		size_t packetSize = headerSize + (count * prefixEntrySize);

		int ret = m_client_list[client_h]->m_socket->send_msg(G_cData50000, static_cast<int>(packetSize));
		switch (ret) {
		case sock::Event::QueueFull:
		case sock::Event::SocketError:
		case sock::Event::CriticalError:
		case sock::Event::SocketClosed:
			hb::logger::log("Failed to send attribute prefix types: Client({})", client_h);
			delete_client(client_h, true, true);
			delete m_client_list[client_h];
			m_client_list[client_h] = 0;
			return false;
		}
	}

	// Send secondary entries
	if (!m_attribute_secondary_types.empty())
	{
		std::memset(G_cData50000, 0, sizeof(G_cData50000));

		auto* pktHeader = reinterpret_cast<hb::net::PacketAttributeTypeConfigHeader*>(G_cData50000);
		pktHeader->header.msg_id = MsgId::AttributeTypeConfigContents;
		pktHeader->header.msg_type = MsgType::Confirm;
		pktHeader->totalEntries = static_cast<uint16_t>(m_attribute_secondary_types.size());
		pktHeader->packetIndex = 0;
		pktHeader->entryType = 1;

		auto* entries = reinterpret_cast<hb::net::PacketAttributeSecondaryTypeEntry*>(G_cData50000 + headerSize);
		uint16_t count = 0;
		for (const auto& e : m_attribute_secondary_types)
		{
			entries[count].secondary_id = e.secondary_id;
			entries[count].multiplier = e.multiplier;
			count++;
		}
		pktHeader->entryCount = count;
		size_t packetSize = headerSize + (count * secondaryEntrySize);

		int ret = m_client_list[client_h]->m_socket->send_msg(G_cData50000, static_cast<int>(packetSize));
		switch (ret) {
		case sock::Event::QueueFull:
		case sock::Event::SocketError:
		case sock::Event::CriticalError:
		case sock::Event::SocketClosed:
			hb::logger::log("Failed to send attribute secondary types: Client({})", client_h);
			delete_client(client_h, true, true);
			delete m_client_list[client_h];
			m_client_list[client_h] = 0;
			return false;
		}
	}

	return true;
}

void CGame::reload_attribute_types()
{
	sqlite3* configDb = nullptr;
	std::string dbPath;
	bool created = false;
	if (!EnsureGameConfigDatabase(&configDb, dbPath, &created)) return;

	m_attribute_prefix_types.clear();
	m_attribute_secondary_types.clear();
	LoadAttributePrefixTypes(configDb, m_attribute_prefix_types);
	LoadAttributeSecondaryTypes(configDb, m_attribute_secondary_types);
	build_multiplier_lookup();
	compute_attribute_types_hash();
	CloseGameConfigDatabase(configDb);
}

void CGame::fill_player_map_object(hb::net::PacketMapDataObjectPlayer& obj, short owner_h, int viewer_h)
{
	auto* client = m_client_list[owner_h];
	obj.base.object_id = static_cast<uint16_t>(owner_h);
	obj.type = client->m_type;
	obj.dir = static_cast<uint8_t>(client->m_dir);
	obj.appearance = build_broadcast_appearance(owner_h);
	obj.status = client->m_status;
	obj.status.pk = (client->m_player_kill_count != 0) ? 1 : 0;
	obj.status.citizen = (client->m_side != 0) ? 1 : 0;
	obj.status.aresden = (client->m_side == 1) ? 1 : 0;
	obj.status.hunter = client->m_is_player_civil ? 1 : 0;
	obj.status.relationship = m_combat_manager->get_player_relationship(owner_h, viewer_h);
	std::memcpy(obj.name, client->m_char_name, sizeof(obj.name));
}

void CGame::fill_npc_map_object(hb::net::PacketMapDataObjectNpc& obj, short owner_h, int viewer_h)
{
	auto* npc = m_npc_list[owner_h];
	obj.base.object_id = static_cast<uint16_t>(owner_h + hb::shared::object_id::NpcMin);
	obj.config_id = npc->m_npc_config_id;
	obj.dir = static_cast<uint8_t>(npc->m_dir);
	obj.appearance = npc->m_appearance;
	obj.status = npc->m_status;
	obj.status.relationship = m_entity_manager->get_npc_relationship(owner_h, viewer_h);
	std::memcpy(obj.name, npc->m_name, sizeof(obj.name));
}

int CGame::compose_init_map_data(short sX, short sY, int client_h, char* data)
{
	int size, tile_exists;
	class CTile* tile;
	unsigned char ucHeader;
	char* cp;

	if (m_client_list[client_h] == 0) return 0;

	cp = data + sizeof(hb::net::PacketMapDataHeader);
	size = sizeof(hb::net::PacketMapDataHeader);
	tile_exists = 0;

	for(int iy = 0; iy < hb::shared::view::InitDataTilesY; iy++)
		for(int ix = 0; ix < hb::shared::view::InitDataTilesX; ix++) {

			if (((sX + ix) == 100) && ((sY + iy) == 100))
				sX = sX;

			tile = (class CTile*)(m_map_list[m_client_list[client_h]->m_map_index]->m_tile + (sX + ix) + (sY + iy) * m_map_list[m_client_list[client_h]->m_map_index]->m_size_y);

			//If player not same side and is invied (Beholder Hack)
			/*if ((m_client_list[tile->m_owner] != 0) && (tile->m_owner != client_h))
				if ((m_client_list[tile->m_owner]->m_side != 0) &&
					(m_client_list[tile->m_owner]->m_side != m_client_list[client_h]->m_side) &&
					(m_client_list[tile->m_owner]->m_status.invisibility)) {
					continue;
				}*/

			if ((tile->m_owner != 0) || (tile->m_dead_owner != 0) ||
				(tile->m_item[0] != 0) || (tile->m_dynamic_object_type != 0)) {
				tile_exists++;

				ucHeader = 0;
				if (tile->m_owner != 0) {
					if (tile->m_owner_class == hb::shared::owner_class::Player) {
						if (m_client_list[tile->m_owner] != 0) {
							if (m_client_list[tile->m_owner]->m_is_admin_invisible &&
								tile->m_owner != client_h &&
								m_client_list[client_h]->m_admin_level <= m_client_list[tile->m_owner]->m_admin_level)
							{
							}
							else
							{
								ucHeader = ucHeader | 0x01;
							}
						}
						else {
							std::snprintf(G_cTxt, sizeof(G_cTxt), "empty player handle: %d", tile->m_owner);
							tile->m_owner = 0;
						}
					}

					if (tile->m_owner_class == hb::shared::owner_class::Npc) {
						if (m_npc_list[tile->m_owner] != 0) ucHeader = ucHeader | 0x01;
						else tile->m_owner = 0;
					}
				}
				if (tile->m_dead_owner != 0) {
					if (tile->m_dead_owner_class == hb::shared::owner_class::Player) {
						if (m_client_list[tile->m_dead_owner] != 0) ucHeader = ucHeader | 0x02;
						else tile->m_dead_owner = 0;
					}
					if (tile->m_dead_owner_class == hb::shared::owner_class::Npc) {
						if (m_npc_list[tile->m_dead_owner] != 0) ucHeader = ucHeader | 0x02;
						else tile->m_dead_owner = 0;
					}
				}
				if (tile->m_item[0] != 0)				ucHeader = ucHeader | 0x04;
				if (tile->m_dynamic_object_type != 0)    ucHeader = ucHeader | 0x08;

				hb::net::PacketMapDataEntryHeader entryHeader{};
				entryHeader.x = static_cast<int16_t>(ix);
				entryHeader.y = static_cast<int16_t>(iy);
				entryHeader.flags = ucHeader;
				std::memcpy(cp, &entryHeader, sizeof(entryHeader));
				cp += sizeof(entryHeader);
				size += sizeof(entryHeader);

				if ((ucHeader & 0x01) != 0) {
					switch (tile->m_owner_class) {
					case hb::shared::owner_class::Player:
					{
						hb::net::PacketMapDataObjectPlayer obj{};
						fill_player_map_object(obj, tile->m_owner, client_h);
						if (m_client_list[tile->m_owner]->m_is_admin_invisible) {
							obj.status.invisibility = true;
							obj.status.gm_mode = true;
						}
						std::memcpy(cp, &obj, sizeof(obj));
						cp += sizeof(obj);
						size += sizeof(obj);
						break;
					}
					case hb::shared::owner_class::Npc:
					{
						hb::net::PacketMapDataObjectNpc obj{};
						fill_npc_map_object(obj, tile->m_owner, client_h);
						std::memcpy(cp, &obj, sizeof(obj));
						cp += sizeof(obj);
						size += sizeof(obj);
						break;
					}
					}
				}

				if ((ucHeader & 0x02) != 0) {
					switch (tile->m_dead_owner_class) {
					case hb::shared::owner_class::Player:
					{
						hb::net::PacketMapDataObjectPlayer obj{};
						fill_player_map_object(obj, tile->m_dead_owner, client_h);
						std::memcpy(cp, &obj, sizeof(obj));
						cp += sizeof(obj);
						size += sizeof(obj);
						break;
					}
					case hb::shared::owner_class::Npc:
					{
						hb::net::PacketMapDataObjectNpc obj{};
						fill_npc_map_object(obj, tile->m_dead_owner, client_h);
						std::memcpy(cp, &obj, sizeof(obj));
						cp += sizeof(obj);
						size += sizeof(obj);
						break;
					}
					}
				}

				if (tile->m_item[0] != 0) {
					hb::net::PacketMapDataItem itemObj{};
					itemObj.item_id = tile->m_item[0]->m_id_num;
					itemObj.color = tile->m_item[0]->m_instance.item_color;
					tile->m_item[0]->copy_attributes_to(itemObj);
					itemObj.count = CItem::count_to_v2(tile->m_item[0]->m_instance.count);
					std::memcpy(cp, &itemObj, sizeof(itemObj));
					cp += sizeof(itemObj);
					size += sizeof(itemObj);
				}

				if (tile->m_dynamic_object_type != 0) {
					hb::net::PacketMapDataDynamicObject dynObj{};
					dynObj.object_id = tile->m_dynamic_object_id;
					dynObj.type = tile->m_dynamic_object_type;
					std::memcpy(cp, &dynObj, sizeof(dynObj));
					cp += sizeof(dynObj);
					size += sizeof(dynObj);
				}
			} // Big if
		} // while(1)

	hb::net::PacketMapDataHeader header{};
	header.total = static_cast<int16_t>(tile_exists);
	std::memcpy(data, &header, sizeof(header));
	return size;
}

void CGame::delete_client(int client_h, bool save, bool notify, bool count_logout, bool force_close_conn)
{
	int ex_h;
	char tmp_map[30];

	if (m_client_list[client_h] == 0) return;
	if (m_client_list[client_h]->m_is_init_complete) {
		if (memcmp(m_client_list[client_h]->m_map_name, "fight", 5) == 0) {
			hb::logger::log<log_channel::log_events>("Player '{}' exited map '{}'", m_client_list[client_h]->m_char_name, m_client_list[client_h]->m_map_name);
		}

		if (m_client_list[client_h]->m_is_exchange_mode) {
			ex_h = m_client_list[client_h]->m_exchange_h;
			m_item_manager->clear_exchange_status(ex_h);
			m_item_manager->clear_exchange_status(client_h);
		}

		m_fishing_manager->release_fish_engagement(client_h);

		if (notify)
			send_event_to_near_client_type_a(client_h, hb::shared::owner_class::Player, MsgId::EventLog, MsgType::Reject, 0, 0, 0);

		m_combat_manager->remove_from_target(client_h, hb::shared::owner_class::Player);

		// Delete all summoned NPCs belonging to this player
		for (int i = 0; i < MaxNpcs; i++)
			if (m_npc_list[i] != 0) {
				if ((m_npc_list[i]->m_is_summoned) &&
					(m_npc_list[i]->m_follow_owner_index == client_h) &&
					(m_npc_list[i]->m_follow_owner_type == hb::shared::owner_class::Player)) {
					m_entity_manager->delete_entity(i);
				}
			}

		for(int i = 1; i < MaxClients; i++)
			if ((m_client_list[i] != 0) && (m_client_list[i]->m_whisper_player_index == client_h)) {
				m_client_list[i]->m_whisper_player_index = -1;
				send_notify_msg(0, i, Notify::WhisperModeOff, 0, 0, 0, m_client_list[client_h]->m_char_name);
			}

		m_map_list[m_client_list[client_h]->m_map_index]->clear_owner(2, client_h, hb::shared::owner_class::Player,
			m_client_list[client_h]->m_x,
			m_client_list[client_h]->m_y);

		m_delay_event_manager->remove_from_delay_event_list(client_h, hb::shared::owner_class::Player, 0);
	}

	if ((save) && (m_client_list[client_h]->m_is_on_server_change == false)) {

		if (m_client_list[client_h]->m_is_killed) {
			m_client_list[client_h]->m_x = -1;
			m_client_list[client_h]->m_y = -1;

			strcpy(tmp_map, m_client_list[client_h]->m_map_name);

			std::memset(m_client_list[client_h]->m_map_name, 0, sizeof(m_client_list[client_h]->m_map_name));

			if (m_client_list[client_h]->m_side == 0) {
				strcpy(m_client_list[client_h]->m_map_name, "default");
			}
			else {
				if (memcmp(m_client_list[client_h]->m_location, "are", 3) == 0) {
					if (m_is_crusade_mode) {
						if (m_client_list[client_h]->m_dead_penalty_time > 0) {
							std::memset(m_client_list[client_h]->m_locked_map_name, 0, sizeof(m_client_list[client_h]->m_locked_map_name));
							strcpy(m_client_list[client_h]->m_locked_map_name, "aresden");
							m_client_list[client_h]->m_locked_map_time = 60 * 5;
							m_client_list[client_h]->m_dead_penalty_time = 60 * 10;
						}
						else {
							m_client_list[client_h]->m_dead_penalty_time = 60 * 10;
						}
					}

					if (strcmp(tmp_map, "elvine") == 0) {
						strcpy(m_client_list[client_h]->m_locked_map_name, "elvjail");
						m_client_list[client_h]->m_locked_map_time = 60 * 3;
						memcpy(m_client_list[client_h]->m_map_name, "elvjail", 7);
					}
					else if (m_client_list[client_h]->m_level > 80)
						memcpy(m_client_list[client_h]->m_map_name, "resurr1", 7);
					else memcpy(m_client_list[client_h]->m_map_name, "arefarm", 7);
				}
				else {
					if (m_is_crusade_mode) {
						if (m_client_list[client_h]->m_dead_penalty_time > 0) {
							std::memset(m_client_list[client_h]->m_locked_map_name, 0, sizeof(m_client_list[client_h]->m_locked_map_name));
							strcpy(m_client_list[client_h]->m_locked_map_name, "elvine");
							m_client_list[client_h]->m_locked_map_time = 60 * 5;
							m_client_list[client_h]->m_dead_penalty_time = 60 * 10;
						}
						else {
							m_client_list[client_h]->m_dead_penalty_time = 60 * 10;
						}
					}
					if (strcmp(tmp_map, "aresden") == 0) {
						strcpy(m_client_list[client_h]->m_locked_map_name, "arejail");
						m_client_list[client_h]->m_locked_map_time = 60 * 3;
						memcpy(m_client_list[client_h]->m_map_name, "arejail", 7);

					}
					else if (m_client_list[client_h]->m_level > 80)
						memcpy(m_client_list[client_h]->m_map_name, "resurr2", 7);
					else memcpy(m_client_list[client_h]->m_map_name, "elvfarm", 7);
				}
			}
		}
		else if (force_close_conn) {
			std::memset(m_client_list[client_h]->m_map_name, 0, sizeof(m_client_list[client_h]->m_map_name));
			memcpy(m_client_list[client_h]->m_map_name, "bisle", 5);
			m_client_list[client_h]->m_x = -1;
			m_client_list[client_h]->m_y = -1;

			std::memset(m_client_list[client_h]->m_locked_map_name, 0, sizeof(m_client_list[client_h]->m_locked_map_name));
			strcpy(m_client_list[client_h]->m_locked_map_name, "bisle");
			m_client_list[client_h]->m_locked_map_time = 10 * 60;
		}

		if (m_client_list[client_h]->m_is_observer_mode) {
			std::memset(m_client_list[client_h]->m_map_name, 0, sizeof(m_client_list[client_h]->m_map_name));
			if (m_client_list[client_h]->m_side == 0) {
				switch (dice(1, 2)) {
				case 1:
					memcpy(m_client_list[client_h]->m_map_name, "aresden", 7);
					break;
				case 2:
					memcpy(m_client_list[client_h]->m_map_name, "elvine", 6);
					break;
				}
			}
			else {
				memcpy(m_client_list[client_h]->m_map_name, m_client_list[client_h]->m_location, 10);
			}
			m_client_list[client_h]->m_x = -1;
			m_client_list[client_h]->m_y = -1;
		}

		if (memcmp(m_client_list[client_h]->m_map_name, "fight", 5) == 0) {
			std::memset(m_client_list[client_h]->m_map_name, 0, sizeof(m_client_list[client_h]->m_map_name));
			if (m_client_list[client_h]->m_side == 0) {
				switch (dice(1, 2)) {
				case 1:
					memcpy(m_client_list[client_h]->m_map_name, "aresden", 7);
					break;
				case 2:
					memcpy(m_client_list[client_h]->m_map_name, "elvine", 6);
					break;
				}
			}
			else {
				memcpy(m_client_list[client_h]->m_map_name, m_client_list[client_h]->m_location, 10);
			}
			m_client_list[client_h]->m_x = -1;
			m_client_list[client_h]->m_y = -1;
		}

		if (m_client_list[client_h]->m_is_init_complete) {
			if (m_client_list[client_h]->m_party_id != 0) {
				hb::net::PartyOpPayload partyOp{};
				partyOp.op_type = 4;
				partyOp.client_h = static_cast<uint16_t>(client_h);
				std::memcpy(partyOp.name, m_client_list[client_h]->m_char_name, sizeof(partyOp.name));
				partyOp.party_id = static_cast<uint16_t>(m_client_list[client_h]->m_party_id);
				party_operation(reinterpret_cast<char*>(&partyOp));
			}
		}
		g_login->local_save_player_data(client_h);
	}
	else {
		if (m_client_list[client_h]->m_is_on_server_change == false) {
			if (m_client_list[client_h]->m_party_id != 0) {
				hb::net::PartyOpPayload partyOp{};
				partyOp.op_type = 4;
				partyOp.client_h = static_cast<uint16_t>(client_h);
				std::memcpy(partyOp.name, m_client_list[client_h]->m_char_name, sizeof(partyOp.name));
				partyOp.party_id = static_cast<uint16_t>(m_client_list[client_h]->m_party_id);
				party_operation(reinterpret_cast<char*>(&partyOp));
			}
		}
		else {
			if (m_client_list[client_h]->m_party_id != 0) {
				hb::net::PartyOpPayload partyOp{};
				partyOp.op_type = 7;
				partyOp.client_h = 0;
				std::memcpy(partyOp.name, m_client_list[client_h]->m_char_name, sizeof(partyOp.name));
				partyOp.party_id = static_cast<uint16_t>(m_client_list[client_h]->m_party_id);
				party_operation(reinterpret_cast<char*>(&partyOp));
			}
		}
	}

	if (m_client_list[client_h]->m_party_id != 0) {
		for(int i = 0; i < hb::shared::limits::MaxPartyMembers; i++)
			if (m_party_info[m_client_list[client_h]->m_party_id].index[i] == client_h) {
				m_party_info[m_client_list[client_h]->m_party_id].index[i] = 0;
				m_party_info[m_client_list[client_h]->m_party_id].total_members--;
				m_client_list[client_h]->m_party_id = 0;
				m_client_list[client_h]->m_party_status = PartyStatus::Null;
				m_client_list[client_h]->m_req_join_party_client_h = 0;
				hb::logger::log("Party {}: member {} removed (deleted), total={}", m_client_list[client_h]->m_party_id, client_h, m_party_info[m_client_list[client_h]->m_party_id].total_members);
				break;
			}

		for(int i = 0; i < hb::shared::limits::MaxPartyMembers - 1; i++)
			if ((m_party_info[m_client_list[client_h]->m_party_id].index[i] == 0) && (m_party_info[m_client_list[client_h]->m_party_id].index[i + 1] != 0)) {
				m_party_info[m_client_list[client_h]->m_party_id].index[i] = m_party_info[m_client_list[client_h]->m_party_id].index[i + 1];
				m_party_info[m_client_list[client_h]->m_party_id].index[i + 1] = 0;
			}
	}

	m_total_clients--;

	// Cancel async operations before freeing the socket
	if (m_client_list[client_h]->m_socket != 0)
		m_client_list[client_h]->m_socket->cancel_async();

	delete m_client_list[client_h];
	m_client_list[client_h] = 0;

	remove_client_short_cut(client_h);
}

void CGame::send_event_to_near_client_type_a(short owner_h, char owner_type, uint32_t msg_id, uint16_t msg_type, int v1, short v2, short v3)
{
	int ret, short_cut_index;
	bool flag;
	int range;
	short sX, sY;
	bool owner_send;

	if ((msg_id == MsgId::EventLog) || (msg_type == Type::Move) || (msg_type == Type::Run) ||
		(msg_type == Type::AttackMove) || (msg_type == Type::DamageMove) || (msg_type == Type::Dying))
		range = hb::shared::view::RangeBuffer;
	else range = 0;

	if (owner_type == hb::shared::owner_class::Player) {
		if (m_client_list[owner_h] == 0) return;

		sX = m_client_list[owner_h]->m_x;
		sY = m_client_list[owner_h]->m_y;

		switch (msg_type) {
		case Type::NullAction:
		case Type::Damage:
		case Type::Dying:
		case MsgType::Confirm:
			owner_send = true;
			break;
		default:
			owner_send = false;
			break;
		}

		hb::net::PacketEventMotionPlayer base_all{};
		base_all.header.msg_id = msg_id;
		base_all.header.msg_type = msg_type;
		base_all.object_id = static_cast<std::uint16_t>(owner_h);
		base_all.x = sX;
		base_all.y = sY;
		base_all.type = m_client_list[owner_h]->m_type;
		base_all.dir = static_cast<std::uint8_t>(m_client_list[owner_h]->m_dir);
		std::memcpy(base_all.name, m_client_list[owner_h]->m_char_name, sizeof(base_all.name));
		base_all.appearance = build_broadcast_appearance(owner_h);
		base_all.status = m_client_list[owner_h]->m_status;
		base_all.loc = 0;
		if (msg_type == Type::NullAction) {
			base_all.loc = m_client_list[owner_h]->m_is_killed ? 1 : 0;
		}

		hb::net::PacketEventMotionShort pkt_short{};
		pkt_short.header.msg_id = msg_id;
		pkt_short.header.msg_type = msg_type;
		pkt_short.object_id = static_cast<std::uint16_t>(owner_h + hb::shared::object_id::NearbyOffset);
		pkt_short.dir = static_cast<std::uint8_t>(m_client_list[owner_h]->m_dir);
		pkt_short.v1 = static_cast<std::int32_t>(v1);
		pkt_short.v2 = static_cast<std::uint8_t>(v2);

		hb::net::PacketEventMotionMove pkt_move{};
		pkt_move.header.msg_id = msg_id;
		pkt_move.header.msg_type = msg_type;
		pkt_move.object_id = static_cast<std::uint16_t>(owner_h + hb::shared::object_id::NearbyOffset);
		pkt_move.dir = static_cast<std::uint8_t>(m_client_list[owner_h]->m_dir);
		pkt_move.v1 = static_cast<std::int32_t>(v1);
		pkt_move.v2 = static_cast<std::uint8_t>(v2);
		pkt_move.x = sX;
		pkt_move.y = sY;

		hb::net::PacketEventMotionAttack pkt_attack{};
		pkt_attack.header.msg_id = msg_id;
		pkt_attack.header.msg_type = msg_type;
		pkt_attack.object_id = static_cast<std::uint16_t>(owner_h + hb::shared::object_id::NearbyOffset);
		pkt_attack.dir = static_cast<std::uint8_t>(m_client_list[owner_h]->m_dir);
		pkt_attack.v1 = static_cast<std::int8_t>(v1 - sX);
		pkt_attack.v2 = static_cast<std::int8_t>(v2 - sY);
		pkt_attack.v3 = static_cast<std::int16_t>(v3);

		hb::net::PacketEventMotionDirOnly pkt_dir{};
		pkt_dir.header.msg_id = msg_id;
		pkt_dir.header.msg_type = msg_type;
		pkt_dir.object_id = static_cast<std::uint16_t>(owner_h + hb::shared::object_id::NearbyOffset);
		pkt_dir.dir = static_cast<std::uint8_t>(m_client_list[owner_h]->m_dir);

		// Per-viewer status filtering is handled at packet build time

		const char key = static_cast<char>((rand() % 255) + 1);

		flag = true;
		short_cut_index = 0;

		while (flag) {
			int i = m_client_shortcut[short_cut_index];
			short_cut_index++;
			if (i == 0) flag = false;

			if ((flag) && (m_client_list[i] != 0) && (m_client_list[i]->m_is_init_complete))
				if ((m_client_list[i]->m_map_index == m_client_list[owner_h]->m_map_index) &&
					(m_client_list[i]->m_x >= m_client_list[owner_h]->m_x - hb::shared::view::RangeX - range) &&
					(m_client_list[i]->m_x <= m_client_list[owner_h]->m_x + hb::shared::view::RangeX + range) &&
					(m_client_list[i]->m_y >= m_client_list[owner_h]->m_y - hb::shared::view::RangeY - range) &&
					(m_client_list[i]->m_y <= m_client_list[owner_h]->m_y + hb::shared::view::RangeY + range)) {

					// Admin invisibility filtering: skip clients that shouldn't see this player
					if (m_client_list[owner_h]->m_is_admin_invisible && i != owner_h &&
						m_client_list[i]->m_admin_level <= m_client_list[owner_h]->m_admin_level)
					{
						// Don't send any packet to this client
					}
					else
					{

					auto pkt_all = base_all;
					{
						auto viewerStatus = m_client_list[owner_h]->m_status;
						viewerStatus.pk = (m_client_list[owner_h]->m_player_kill_count != 0) ? 1 : 0;
						viewerStatus.citizen = (m_client_list[owner_h]->m_side != 0) ? 1 : 0;
						viewerStatus.aresden = (m_client_list[owner_h]->m_side == 1) ? 1 : 0;
						viewerStatus.hunter = m_client_list[owner_h]->m_is_player_civil ? 1 : 0;
						viewerStatus.relationship = m_combat_manager->get_player_relationship(owner_h, i);
						if (m_client_list[owner_h]->m_side != m_client_list[i]->m_side && i != owner_h) {
							viewerStatus.poisoned = false;
							viewerStatus.illusion = false;
						}
						if (m_client_list[owner_h]->m_is_admin_invisible) {
							viewerStatus.invisibility = true;
							viewerStatus.gm_mode = true;
						}
						pkt_all.status = viewerStatus;
					}

					auto send_packet = [&](const void* packet, std::size_t size) -> int {
						return m_client_list[i]->m_socket->send_msg(reinterpret_cast<char*>(const_cast<void*>(packet)), static_cast<int>(size), key);
						};

					const bool is_near = (m_client_list[i]->m_x >= m_client_list[owner_h]->m_x - (hb::shared::view::CenterX - 1)) &&
						(m_client_list[i]->m_x <= m_client_list[owner_h]->m_x + (hb::shared::view::CenterX - 1)) &&
						(m_client_list[i]->m_y >= m_client_list[owner_h]->m_y - hb::shared::view::CenterY) &&
						(m_client_list[i]->m_y <= m_client_list[owner_h]->m_y + hb::shared::view::CenterY);

					if (is_near) {
						switch (msg_type) {
						case MsgType::Confirm:
						case MsgType::Reject:
						case Type::NullAction:
							if (owner_send)
								ret = send_packet(&pkt_all, sizeof(pkt_all));
							else if (i != owner_h)
								ret = send_packet(&pkt_all, sizeof(pkt_all));
							break;

						case Type::Attack:
						case Type::AttackMove:
							if (owner_send)
								ret = send_packet(&pkt_attack, sizeof(pkt_attack));
							else if (i != owner_h)
								ret = send_packet(&pkt_attack, sizeof(pkt_attack));
							break;

						case Type::Magic:
						case Type::Damage:
						case Type::DamageMove:
							if (owner_send)
								ret = send_packet(&pkt_short, sizeof(pkt_short));
							else if (i != owner_h)
								ret = send_packet(&pkt_short, sizeof(pkt_short));
							break;

						case Type::Dying:
							if (owner_send)
								ret = send_packet(&pkt_move, sizeof(pkt_move));
							else if (i != owner_h)
								ret = send_packet(&pkt_move, sizeof(pkt_move));
							break;

						default:
							if (owner_send)
								ret = send_packet(&pkt_dir, sizeof(pkt_dir));
							else if (i != owner_h)
								ret = send_packet(&pkt_dir, sizeof(pkt_dir));
							break;
						}
					}
					else {
						switch (msg_type) {
						case MsgType::Confirm:
						case MsgType::Reject:
						case Type::NullAction:
							if (owner_send)
								ret = send_packet(&pkt_all, sizeof(pkt_all));
							else if (i != owner_h)
								ret = send_packet(&pkt_all, sizeof(pkt_all));
							break;

						case Type::Attack:
						case Type::AttackMove:
							if (owner_send)
								ret = send_packet(&pkt_attack, sizeof(pkt_attack));
							else if (i != owner_h)
								ret = send_packet(&pkt_attack, sizeof(pkt_attack));
							break;

						case Type::Magic:
						case Type::Damage:
						case Type::DamageMove:
							if (owner_send)
								ret = send_packet(&pkt_short, sizeof(pkt_short));
							else if (i != owner_h)
								ret = send_packet(&pkt_short, sizeof(pkt_short));
							break;

						case Type::Dying:
							if (owner_send)
								ret = send_packet(&pkt_move, sizeof(pkt_move));
							else if (i != owner_h)
								ret = send_packet(&pkt_move, sizeof(pkt_move));
							break;

						default:
							if (owner_send)
								ret = send_packet(&pkt_all, sizeof(pkt_all));
							else if (i != owner_h)
								ret = send_packet(&pkt_all, sizeof(pkt_all));
							break;
						}

						if ((ret == sock::Event::QueueFull) || (ret == sock::Event::SocketError) ||
							(ret == sock::Event::SocketClosed) || (ret == sock::Event::CriticalError)) {
							static uint32_t s_dwLastNetWarn = 0;
							uint32_t now = GameClock::GetTimeMS();
							if (now - s_dwLastNetWarn > 5000) {
								hb::logger::log("send_event_to_near_client_type_a: client={} ret={} ownerType={} msgType=0x{:X}", i, ret, owner_type, msg_type);
								s_dwLastNetWarn = now;
							}
						}
					}
					} // end admin invis else
				}
		}
	}
	else {
		if (m_npc_list[owner_h] == 0) return;

		sX = m_npc_list[owner_h]->m_x;
		sY = m_npc_list[owner_h]->m_y;

		hb::net::PacketEventMotionNpc base_all{};
		base_all.header.msg_id = msg_id;
		base_all.header.msg_type = msg_type;
		base_all.object_id = static_cast<std::uint16_t>(owner_h + hb::shared::object_id::NpcMin);
		base_all.x = sX;
		base_all.y = sY;
		base_all.config_id = m_npc_list[owner_h]->m_npc_config_id;
		base_all.dir = static_cast<std::uint8_t>(m_npc_list[owner_h]->m_dir);
		std::memcpy(base_all.name, m_npc_list[owner_h]->m_name, sizeof(base_all.name));
		base_all.appearance = m_npc_list[owner_h]->m_appearance;
		base_all.status = m_npc_list[owner_h]->m_status;
		base_all.loc = 0;
		if (msg_type == Type::NullAction) {
			base_all.loc = m_npc_list[owner_h]->m_is_killed ? 1 : 0;
		}

		hb::net::PacketEventMotionShort pkt_short{};
		pkt_short.header.msg_id = msg_id;
		pkt_short.header.msg_type = msg_type;
		pkt_short.object_id = static_cast<std::uint16_t>(owner_h + 40000);
		pkt_short.dir = static_cast<std::uint8_t>(m_npc_list[owner_h]->m_dir);
		pkt_short.v1 = static_cast<std::int32_t>(v1);
		pkt_short.v2 = static_cast<std::uint8_t>(v2);

		hb::net::PacketEventMotionMove pkt_move{};
		pkt_move.header.msg_id = msg_id;
		pkt_move.header.msg_type = msg_type;
		pkt_move.object_id = static_cast<std::uint16_t>(owner_h + 40000);
		pkt_move.dir = static_cast<std::uint8_t>(m_npc_list[owner_h]->m_dir);
		pkt_move.v1 = static_cast<std::int32_t>(v1);
		pkt_move.v2 = static_cast<std::uint8_t>(v2);
		pkt_move.x = sX;
		pkt_move.y = sY;

		hb::net::PacketEventMotionAttack pkt_attack{};
		pkt_attack.header.msg_id = msg_id;
		pkt_attack.header.msg_type = msg_type;
		pkt_attack.object_id = static_cast<std::uint16_t>(owner_h + 40000);
		pkt_attack.dir = static_cast<std::uint8_t>(m_npc_list[owner_h]->m_dir);
		pkt_attack.v1 = static_cast<std::int8_t>(v1 - sX);
		pkt_attack.v2 = static_cast<std::int8_t>(v2 - sY);
		pkt_attack.v3 = static_cast<std::int16_t>(v3);

		hb::net::PacketEventMotionDirOnly pkt_dir{};
		pkt_dir.header.msg_id = msg_id;
		pkt_dir.header.msg_type = msg_type;
		pkt_dir.object_id = static_cast<std::uint16_t>(owner_h + 40000);
		pkt_dir.dir = static_cast<std::uint8_t>(m_npc_list[owner_h]->m_dir);

		const char key = static_cast<char>((rand() % 255) + 1);

		flag = true;
		short_cut_index = 0;

		while (flag) {

			int i = m_client_shortcut[short_cut_index];
			short_cut_index++;
			if (i == 0) flag = false;

			if ((flag) && (m_client_list[i] != 0))

				if ((m_client_list[i]->m_map_index == m_npc_list[owner_h]->m_map_index) &&
					(m_client_list[i]->m_x >= m_npc_list[owner_h]->m_x - hb::shared::view::RangeX - range) &&
					(m_client_list[i]->m_x <= m_npc_list[owner_h]->m_x + hb::shared::view::RangeX + range) &&
					(m_client_list[i]->m_y >= m_npc_list[owner_h]->m_y - hb::shared::view::RangeY - range) &&
					(m_client_list[i]->m_y <= m_npc_list[owner_h]->m_y + hb::shared::view::RangeY + range)) {

					auto pkt_all = base_all;
					pkt_all.status.relationship = m_entity_manager->get_npc_relationship(owner_h, i);

					auto send_packet = [&](const void* packet, std::size_t size) -> int {
						return m_client_list[i]->m_socket->send_msg(reinterpret_cast<char*>(const_cast<void*>(packet)), static_cast<int>(size), key);
						};

					const bool is_near = (m_client_list[i]->m_x >= m_npc_list[owner_h]->m_x - (hb::shared::view::CenterX - 1)) &&
						(m_client_list[i]->m_x <= m_npc_list[owner_h]->m_x + (hb::shared::view::CenterX - 1)) &&
						(m_client_list[i]->m_y >= m_npc_list[owner_h]->m_y - hb::shared::view::CenterY) &&
						(m_client_list[i]->m_y <= m_npc_list[owner_h]->m_y + hb::shared::view::CenterY);

					if (is_near) {
						switch (msg_type) {
						case MsgType::Confirm:
						case MsgType::Reject:
						case Type::NullAction:
							ret = send_packet(&pkt_all, sizeof(pkt_all));
							break;

						case Type::stop:
							ret = send_packet(&pkt_all, sizeof(pkt_all));
							break;

						case Type::Dying:
							ret = send_packet(&pkt_move, sizeof(pkt_move));
							break;

						case Type::Damage:
						case Type::DamageMove:
							ret = send_packet(&pkt_short, sizeof(pkt_short));
							break;

						case Type::Attack:
						case Type::AttackMove:
							ret = send_packet(&pkt_attack, sizeof(pkt_attack));
							break;

						case Type::Move:
						case Type::Run:
							// Send full position data to prevent desync for nearby players
							ret = send_packet(&pkt_all, sizeof(pkt_all));
							break;

						default:
							ret = send_packet(&pkt_dir, sizeof(pkt_dir));
							break;

						}

						if ((ret == sock::Event::QueueFull) || (ret == sock::Event::SocketError) ||
							(ret == sock::Event::SocketClosed) || (ret == sock::Event::CriticalError)) {
							static uint32_t s_dwLastNetWarnNpc = 0;
							uint32_t now = GameClock::GetTimeMS();
							if (now - s_dwLastNetWarnNpc > 5000) {
								hb::logger::log("send_event_to_near_client_type_a(NPC): client={} ret={} ownerType={} msgType=0x{:X}", i, ret, owner_type, msg_type);
								s_dwLastNetWarnNpc = now;
							}
						}
					}
					else {
						switch (msg_type) {
						case MsgType::Confirm:
						case MsgType::Reject:
						case Type::NullAction:
							ret = send_packet(&pkt_all, sizeof(pkt_all));
							break;

						case Type::Dying:
							ret = send_packet(&pkt_move, sizeof(pkt_move));
							break;

						case Type::Damage:
						case Type::DamageMove:
							ret = send_packet(&pkt_short, sizeof(pkt_short));
							break;

						case Type::Attack:
						case Type::AttackMove:
							ret = send_packet(&pkt_attack, sizeof(pkt_attack));
							break;

						default:
							ret = send_packet(&pkt_all, sizeof(pkt_all));
							break;

						} //Switch

						if ((ret == sock::Event::QueueFull) || (ret == sock::Event::SocketError) ||
							(ret == sock::Event::SocketClosed) || (ret == sock::Event::CriticalError)) {
							static uint32_t s_dwLastNetWarnNpcFar = 0;
							uint32_t now = GameClock::GetTimeMS();
							if (now - s_dwLastNetWarnNpcFar > 5000) {
								hb::logger::log("send_event_to_near_client_type_a(NPC-far): client={} ret={} ownerType={} msgType=0x{:X}", i, ret, owner_type, msg_type);
								s_dwLastNetWarnNpcFar = now;
							}
						}
					}
				}
		}
	} // else - NPC
}

int CGame::compose_move_map_data(short sX, short sY, int client_h, direction dir, char* data)
{
	int ix, iy, size, tile_exists, index;
	class CTile* tile;
	unsigned char ucHeader;
	char* cp;

	if (m_client_list[client_h] == 0) return 0;

	cp = data + sizeof(hb::net::PacketMapDataHeader);
	size = sizeof(hb::net::PacketMapDataHeader);
	tile_exists = 0;

	index = 0;

	while (1) {
		ix = _tmp_iMoveLocX[dir][index];
		iy = _tmp_iMoveLocY[dir][index];
		if ((ix == -1) || (iy == -1)) break;

		index++;

		tile = (class CTile*)(m_map_list[m_client_list[client_h]->m_map_index]->m_tile + (sX + ix) + (sY + iy) * m_map_list[m_client_list[client_h]->m_map_index]->m_size_y);

		//If player not same side and is invied (Beholder Hack)
		// there is another person on the tiles, and the owner is not the player
//xxxxxx
		/*if ((m_client_list[tile->m_owner] != 0) && (tile->m_owner != client_h))
			if ((m_client_list[tile->m_owner]->m_side != 0) &&
				(m_client_list[tile->m_owner]->m_side != m_client_list[client_h]->m_side) &&
				(m_client_list[tile->m_owner]->m_status.invisibility)) {
				continue;
			}*/

		if ((tile->m_owner != 0) || (tile->m_dead_owner != 0) ||
			(tile->m_item[0] != 0) || (tile->m_dynamic_object_type != 0)) {

			tile_exists++;

			ucHeader = 0;
			if (tile->m_owner != 0) {
				if (tile->m_owner_class == hb::shared::owner_class::Player) {
					if (m_client_list[tile->m_owner] != 0) {
						if (m_client_list[tile->m_owner]->m_is_admin_invisible &&
							tile->m_owner != client_h &&
							m_client_list[client_h]->m_admin_level <= m_client_list[tile->m_owner]->m_admin_level)
						{
						}
						else
						{
							ucHeader = ucHeader | 0x01;
						}
					}
					else tile->m_owner = 0;
				}
				if (tile->m_owner_class == hb::shared::owner_class::Npc) {
					if (m_npc_list[tile->m_owner] != 0) ucHeader = ucHeader | 0x01;
					else tile->m_owner = 0;
				}
			}
			if (tile->m_dead_owner != 0) {
				if (tile->m_dead_owner_class == hb::shared::owner_class::Player) {
					if (m_client_list[tile->m_dead_owner] != 0)	ucHeader = ucHeader | 0x02;
					else tile->m_dead_owner = 0;
				}
				if (tile->m_dead_owner_class == hb::shared::owner_class::Npc) {
					if (m_npc_list[tile->m_dead_owner] != 0) ucHeader = ucHeader | 0x02;
					else tile->m_dead_owner = 0;
				}
			}
			if (tile->m_item[0] != 0)				ucHeader = ucHeader | 0x04;
			if (tile->m_dynamic_object_type != 0)    ucHeader = ucHeader | 0x08;

			hb::net::PacketMapDataEntryHeader entryHeader{};
			entryHeader.x = static_cast<int16_t>(ix);
			entryHeader.y = static_cast<int16_t>(iy);
			entryHeader.flags = ucHeader;
			std::memcpy(cp, &entryHeader, sizeof(entryHeader));
			cp += sizeof(entryHeader);
			size += sizeof(entryHeader);

			if ((ucHeader & 0x01) != 0) {
				switch (tile->m_owner_class) {
				case hb::shared::owner_class::Player:
				{
					hb::net::PacketMapDataObjectPlayer obj{};
					fill_player_map_object(obj, tile->m_owner, client_h);
					if (m_client_list[client_h]->m_side != m_client_list[tile->m_owner]->m_side && client_h != tile->m_owner) {
						obj.status.poisoned = false;
						obj.status.illusion = false;
					}
					if (m_client_list[tile->m_owner]->m_is_admin_invisible) {
						obj.status.invisibility = true;
						obj.status.gm_mode = true;
					}
					std::memcpy(cp, &obj, sizeof(obj));
					cp += sizeof(obj);
					size += sizeof(obj);
					break;
				}
				case hb::shared::owner_class::Npc:
				{
					hb::net::PacketMapDataObjectNpc obj{};
					fill_npc_map_object(obj, tile->m_owner, client_h);
					std::memcpy(cp, &obj, sizeof(obj));
					cp += sizeof(obj);
					size += sizeof(obj);
					break;
				}
				}
			}

			if ((ucHeader & 0x02) != 0) {
				switch (tile->m_dead_owner_class) {
				case hb::shared::owner_class::Player:
				{
					hb::net::PacketMapDataObjectPlayer obj{};
					fill_player_map_object(obj, tile->m_dead_owner, client_h);
					if (m_client_list[client_h]->m_side != m_client_list[tile->m_dead_owner]->m_side && client_h != tile->m_dead_owner) {
						obj.status.poisoned = false;
						obj.status.illusion = false;
					}
					std::memcpy(cp, &obj, sizeof(obj));
					cp += sizeof(obj);
					size += sizeof(obj);
					break;
				}
				case hb::shared::owner_class::Npc:
				{
					hb::net::PacketMapDataObjectNpc obj{};
					fill_npc_map_object(obj, tile->m_dead_owner, client_h);
					std::memcpy(cp, &obj, sizeof(obj));
					cp += sizeof(obj);
					size += sizeof(obj);
					break;
				}
				}
			}

			if (tile->m_item[0] != 0) {
				hb::net::PacketMapDataItem itemObj{};
				itemObj.item_id = tile->m_item[0]->m_id_num;
				itemObj.color = tile->m_item[0]->m_instance.item_color;
				itemObj.custom_made = tile->m_item[0]->m_instance.custom_made ? 1 : 0;
				itemObj.prefix_type = static_cast<uint8_t>(tile->m_item[0]->m_instance.prefix_type);
				itemObj.prefix_value = tile->m_item[0]->m_instance.prefix_value;
				itemObj.secondary_type = static_cast<uint8_t>(tile->m_item[0]->m_instance.secondary_type);
				itemObj.secondary_value = tile->m_item[0]->m_instance.secondary_value;
				itemObj.enchant_bonus = tile->m_item[0]->m_instance.enchant_bonus;
				itemObj.count = CItem::count_to_v2(tile->m_item[0]->m_instance.count);
				std::memcpy(cp, &itemObj, sizeof(itemObj));
				cp += sizeof(itemObj);
				size += sizeof(itemObj);
			}

			if (tile->m_dynamic_object_type != 0) {
				hb::net::PacketMapDataDynamicObject dynObj{};
				dynObj.object_id = tile->m_dynamic_object_id;
				dynObj.type = tile->m_dynamic_object_type;
				std::memcpy(cp, &dynObj, sizeof(dynObj));
				cp += sizeof(dynObj);
				size += sizeof(dynObj);
			}

		} //(tile->m_owner != 0)
	} // end While(1)

	hb::net::PacketMapDataHeader header{};
	header.total = static_cast<int16_t>(tile_exists);
	std::memcpy(data, &header, sizeof(header));
	return size;
}

void CGame::check_client_response_time()
{
	int max_super_attack, value;
	uint32_t time;
	short item_index;
	static uint32_t s_dwLastIdleLog = 0;
	//locobans

	   /*
	   SysTime = hb::time::local_time::now();
	   switch (SysTime.day_of_week) {
	   case 1:	war_period = 30; break;
	   case 2:	war_period = 30; break;
	   case 3:	war_period = 60; break;
	   case 4:	war_period = 60*2;  break;
	   case 5:	war_period = 60*5;  break;
	   case 6:	war_period = 60*10; break;
	   case 0:	war_period = 60*20; break;
	   }
	   */

	time = GameClock::GetTimeMS();

	for(int i = 1; i < MaxClients; i++) {
		if (m_client_list[i] != 0) {

			if ((time - m_client_list[i]->m_time) > (uint32_t)m_client_timeout) {
				if (m_client_list[i]->m_is_init_complete) {
					//Testcode
					hb::logger::log("Client timeout: {}", m_client_list[i]->m_ip_address);

					delete_client(i, true, true);
				}
				else if ((time - m_client_list[i]->m_time) > (uint32_t)m_client_timeout) {
					delete_client(i, false, false);
				}
			}
			else if (m_client_list[i]->m_is_init_complete) {
				uint32_t idle = time - m_client_list[i]->m_time;
				if (idle > 5000 && (time - m_client_list[i]->m_last_msg_time) > 5000 &&
					(time - s_dwLastIdleLog) > 5000) {
					hb::logger::log("[NET] IDLE slot={} idle={}ms lastmsg=0x{:08X} lastage={}ms size={} char={} ip={}", i, idle, m_client_list[i]->m_last_msg_id, time - m_client_list[i]->m_last_msg_time, m_client_list[i]->m_last_msg_size, m_client_list[i]->m_char_name, m_client_list[i]->m_ip_address);
					s_dwLastIdleLog = time;
				}

				// AFK detection: 3 minutes of no meaningful activity
				constexpr uint32_t AFK_TIMEOUT_MS = 180000;
				bool was_afk = m_client_list[i]->m_status.afk;
				bool now_afk = (time - m_client_list[i]->m_afk_activity_time) > AFK_TIMEOUT_MS;
				if (was_afk != now_afk) {
					m_client_list[i]->m_status.afk = now_afk;
					send_event_to_near_client_type_a(i, hb::shared::owner_class::Player,
						MsgId::EventMotion, Type::NullAction, 0, 0, 0);
				}

				m_client_list[i]->m_time_left_rating--;
				if (m_client_list[i]->m_time_left_rating < 0) m_client_list[i]->m_time_left_rating = 0;

				m_regen_manager->process_client_tick(i, time);

				if (check_character_data(i) == false) {
					delete_client(i, true, true);
					break;
				}

				if ((m_map_list[m_client_list[i]->m_map_index]->m_is_fight_zone == false) &&
					((time - m_client_list[i]->m_auto_save_time) > (uint32_t)m_autosave_interval)) {
					g_login->local_save_player_data(i); //send_msg_to_ls(ServerMsgId::RequestSavePlayerData, i);
					m_client_list[i]->m_auto_save_time += m_autosave_interval;
					if (time - m_client_list[i]->m_auto_save_time > (uint32_t)m_autosave_interval) m_client_list[i]->m_auto_save_time = time;
				}

				// ExpStock
				if ((time - m_client_list[i]->m_exp_stock_time) > (uint32_t)ExpStockTime) {
					m_client_list[i]->m_exp_stock_time += ExpStockTime;
					if (time - m_client_list[i]->m_exp_stock_time > (uint32_t)ExpStockTime) m_client_list[i]->m_exp_stock_time = time;
					calc_exp_stock(i);
					m_item_manager->check_unique_item_equipment(i);
					m_war_manager->check_crusade_result_calculation(i);
					m_war_manager->check_heldenian_result_calculation(i);
				}

				// AutoExe
				if ((time - m_client_list[i]->m_auto_exp_time) > (uint32_t)AutoExpTime) {
					value = (m_client_list[i]->m_level / 2);
					if (value <= 0) value = 1;
					uint32_t value_dw = static_cast<uint32_t>(value);
					if (m_client_list[i]->m_auto_exp_amount < value_dw) {
						if ((m_client_list[i]->m_exp + value_dw) < m_client_list[i]->m_next_level_exp) {
							//m_client_list[i]->m_exp_stock += value;
							get_exp(i, value_dw, false);
							calc_exp_stock(i);
						}
					}

					m_client_list[i]->m_auto_exp_amount = 0;
					m_client_list[i]->m_auto_exp_time += AutoExpTime;
					if (time - m_client_list[i]->m_auto_exp_time > (uint32_t)AutoExpTime) m_client_list[i]->m_auto_exp_time = time;
				}

				// v1.432
				if (m_client_list[i]->m_special_ability_time == 3) {
					send_notify_msg(0, i, Notify::SpecialAbilityEnabled, 0, 0, 0, 0);
					// New 25/05/2004
					// After the time up, add magic back
					item_index = m_client_list[i]->m_item_equipment_status[to_int(EquipPos::RightHand)];
					if (item_index != -1) {
						if ((m_client_list[i]->m_item_list[item_index]->m_id_num == 865) || (m_client_list[i]->m_item_list[item_index]->m_id_num == 866)) {
							if ((m_client_list[i]->m_int + m_client_list[i]->m_angelic_int) > 99 && (m_client_list[i]->m_mag + m_client_list[i]->m_angelic_mag) > 99) {
								m_client_list[i]->m_magic_mastery[94] = true;
								send_notify_msg(0, i, Notify::StateChangeSuccess, 0, 0, 0, 0);
							}
						}
					}
				}
				m_client_list[i]->m_special_ability_time -= 3;
				if (m_client_list[i]->m_special_ability_time < 0) m_client_list[i]->m_special_ability_time = 0;

				// v1.432
				if (m_client_list[i]->m_is_special_ability_enabled) {
					uint32_t elapsedSec = (time - m_client_list[i]->m_special_ability_start_time) / 1000;
					if (elapsedSec > static_cast<uint32_t>(m_client_list[i]->m_special_ability_last_sec)) {
						send_notify_msg(0, i, Notify::SpecialAbilityStatus, 3, 0, 0, 0);
						m_client_list[i]->m_is_special_ability_enabled = false;
						m_client_list[i]->m_special_ability_time = SpecialAbilityTimeSec;
						m_client_list[i]->m_appearance.effect_type = 0;
						send_event_to_near_client_type_a(i, hb::shared::owner_class::Player, MsgId::EventMotion, Type::NullAction, 0, 0, 0);
					}
				}

				//Crusade
				m_client_list[i]->m_locked_map_time -= 3;
				if (m_client_list[i]->m_locked_map_time < 0) {
					m_client_list[i]->m_locked_map_time = 0;
					std::memset(m_client_list[i]->m_locked_map_name, 0, sizeof(m_client_list[i]->m_locked_map_name));
					strcpy(m_client_list[i]->m_locked_map_name, "NONE");
				}

				// v2.04
				m_client_list[i]->m_dead_penalty_time -= 3;
				if (m_client_list[i]->m_dead_penalty_time < 0) m_client_list[i]->m_dead_penalty_time = 0;

				// GMs with /gm on bypass all forced recalls and level restrictions
				if (!m_client_list[i]->m_is_gm_mode) {

				if ((m_client_list[i]->m_is_war_location) && is_enemy_zone(i)) {
					// Crusade
					if (m_is_crusade_mode == false)
						if (m_client_list[i]->m_is_inside_own_town == false)
							m_client_list[i]->m_time_left_force_recall--;

					if (m_client_list[i]->m_time_left_force_recall <= 0) {
						m_client_list[i]->m_time_left_force_recall = 0;
						m_client_list[i]->m_war_begin_time = time;
						m_client_list[i]->m_is_war_location = false;

						send_notify_msg(0, i, Notify::ToBeRecalled, 0, 0, 0, 0);
						request_teleport_handler(i, "1   ");
					}
				}

				if ((m_is_heldenian_mode) && (m_map_list[m_client_list[i]->m_map_index] != 0)) {
					if (m_war_manager->check_heldenian_map(i, m_bt_field_map_index, hb::shared::owner_class::Player) == 1) {
						m_status_effect_manager->set_hero_flag(i, hb::shared::owner_class::Player, true);
					}
					else {
						m_status_effect_manager->set_hero_flag(i, hb::shared::owner_class::Player, false);
					}
				}

				if (m_client_list[i] == 0) break;
				if (m_client_list[i]->m_skill_msg_recv_count >= 2) {
					// std::snprintf(G_cTxt, sizeof(G_cTxt), "(!)  (%s)  ", m_client_list[i]->m_char_name);
					//PutLogFileList(G_cTxt);
					delete_client(i, true, true);
				}
				else {
					m_client_list[i]->m_skill_msg_recv_count = 0;
				}

				if (m_client_list[i] == 0) break;
				//if (m_client_list[i]->m_level < m_map_list[m_client_list[i]->m_map_index]->m_level_requirement) {
				if ((m_client_list[i]->m_level < m_map_list[m_client_list[i]->m_map_index]->m_level_requirement)) {
					send_notify_msg(0, i, Notify::ToBeRecalled, 0, 0, 0, 0);
					request_teleport_handler(i, "0   ");
				}

				if (m_client_list[i] == 0) break;
				//if ( (m_map_list[m_client_list[i]->m_map_index]->m_upper_level_limit != 0) &&
				//	 (m_client_list[i]->m_level > m_map_list[m_client_list[i]->m_map_index]->m_upper_level_limit) ) {
				if ((m_map_list[m_client_list[i]->m_map_index]->m_upper_level_limit != 0) &&
					(m_client_list[i]->m_level > m_map_list[m_client_list[i]->m_map_index]->m_upper_level_limit)) {
					send_notify_msg(0, i, Notify::ToBeRecalled, 0, 0, 0, 0);
					if (m_client_list[i]->m_side == 1) {
						request_teleport_handler(i, "2   ", "aresden", -1, -1);
					}
					else if (m_client_list[i]->m_side == 2) {
						request_teleport_handler(i, "2   ", "elvine", -1, -1);
					}
				}

				if (m_client_list[i] == 0) break;
				if ((strcmp(m_client_list[i]->m_location, "elvine") != 0) &&
					(strcmp(m_client_list[i]->m_location, "elvhunter") != 0) &&
					(strcmp(m_client_list[i]->m_location, "arehunter") != 0) &&
					(strcmp(m_client_list[i]->m_location, "aresden") != 0) &&
					(m_client_list[i]->m_level >= 20)) {
					hb::logger::warn<log_channel::security>("Traveller hack: IP={} player={}, traveller exceeds level 19", m_client_list[i]->m_ip_address, m_client_list[i]->m_char_name);
					delete_client(i, true, true);
				}

				if (m_client_list[i] == 0) break;
				if ((m_map_list[m_client_list[i]->m_map_index]->m_is_apocalypse_map) &&
					(m_is_apocalypse_mode == false))
				{
					request_teleport_handler(i, "1   ");
				}

				//(m_is_crusade_mode )
				/*
				if (m_client_list[i] == 0) break;
				if (((memcmp(m_client_list[i]->m_location, "aresden",7) == 0) || (memcmp(m_client_list[i]->m_location, "elvine",6) == 0)) ) {
					mapside = get_map_location_side(m_map_list[m_client_list[i]->m_map_index]->m_name);
					if (mapside > 3) mapside2 = mapside - 2;
					else mapside2 = mapside;

					if ((m_client_list[i]->m_side != mapside2) && (mapside != 0)) {
						if ((mapside <= 2)) {
							if (m_client_list[i]->m_side != 0) {
								m_client_list[i]->m_war_begin_time = GameClock::GetTimeMS();
								m_client_list[i]->m_is_war_location = true;
								m_client_list[i]->m_time_left_force_recall = 1;

								request_teleport_handler(i, "1   ");
								send_notify_msg(0, i, Notify::ToBeRecalled, 0, 0, 0, 0);
							}
						}
					}
				}*/

				if (m_client_list[i] == 0) break;
				if (((memcmp(m_client_list[i]->m_location, "arehunter", 9) == 0) || (memcmp(m_client_list[i]->m_location, "elvhunter", 9) == 0)) &&
					((strcmp(m_map_list[m_client_list[i]->m_map_index]->m_name, "2ndmiddle") == 0) || (strcmp(m_map_list[m_client_list[i]->m_map_index]->m_name, "middleland") == 0))) {
					send_notify_msg(0, i, Notify::ToBeRecalled, 0, 0, 0, 0);
					request_teleport_handler(i, "1   ");
				}

				if (m_is_apocalypse_mode) {
					if (memcmp(m_map_list[m_client_list[i]->m_map_index]->m_name, "abaddon", 7) == 0)
						send_notify_msg(0, i, Notify::ApocGateOpen, 167, 169, 0, m_client_list[i]->m_map_name);
					else if (memcmp(m_map_list[m_client_list[i]->m_map_index]->m_name, "icebound", 8) == 0)
						send_notify_msg(0, i, Notify::ApocGateOpen, 89, 31, 0, m_client_list[i]->m_map_name);
				}

				if (m_client_list[i] == 0) break;
				if ((m_is_apocalypse_mode) &&
					(memcmp(m_map_list[m_client_list[i]->m_map_index]->m_name, "icebound", 8) == 0) &&
					((m_client_list[i]->m_x == 89 && m_client_list[i]->m_y == 31) ||
						(m_client_list[i]->m_x == 89 && m_client_list[i]->m_y == 32) ||
						(m_client_list[i]->m_x == 90 && m_client_list[i]->m_y == 31) ||
						(m_client_list[i]->m_x == 90 && m_client_list[i]->m_y == 32))) {
					request_teleport_handler(i, "2   ", "druncncity", -1, -1);
				}

				if (m_client_list[i] == 0) break;
				if ((memcmp(m_client_list[i]->m_location, "are", 3) == 0) &&
					(strcmp(m_map_list[m_client_list[i]->m_map_index]->m_name, "elvfarm") == 0)) {
					send_notify_msg(0, i, Notify::ToBeRecalled, 0, 0, 0, 0);
					request_teleport_handler(i, "0   ");
				}

				if (m_client_list[i] == 0) break;
				if ((memcmp(m_client_list[i]->m_location, "elv", 3) == 0) &&
					(strcmp(m_map_list[m_client_list[i]->m_map_index]->m_name, "arefarm") == 0)) {
					send_notify_msg(0, i, Notify::ToBeRecalled, 0, 0, 0, 0);
					request_teleport_handler(i, "0   ");
				}

				if (m_client_list[i] == 0) break;
				if ((strcmp(m_map_list[m_client_list[i]->m_map_index]->m_name, "middleland") == 0)
					&& (strcmp(m_client_list[i]->m_location, "NONE") == 0)) {
					send_notify_msg(0, i, Notify::ToBeRecalled, 0, 0, 0, 0);
					request_teleport_handler(i, "0   ");
				}

				if ((m_client_list[i]->m_in_recall_impossible_map)) {
					m_client_list[i]->m_time_left_force_recall--;
					if (m_client_list[i]->m_time_left_force_recall <= 0) {
						m_client_list[i]->m_time_left_force_recall = 0;
						m_client_list[i]->m_in_recall_impossible_map = false;
						send_notify_msg(0, i, Notify::ToBeRecalled, 0, 0, 0, 0);
						request_teleport_handler(i, "0   ");
					}
				}

				} // end GM bypass

				if (m_client_list[i] == 0) break;
				m_client_list[i]->m_super_attack_count++;
				if (m_client_list[i]->m_super_attack_count > 12) {

					m_client_list[i]->m_super_attack_count = 0;
					max_super_attack = (m_client_list[i]->m_level / 10);
					if (m_client_list[i]->m_super_attack_left < max_super_attack) m_client_list[i]->m_super_attack_left++;

					// v1.12
					send_notify_msg(0, i, Notify::SuperAttackLeft, 0, 0, 0, 0);
				}

				// v1.42
				m_client_list[i]->m_time_left_firm_stamina--;
				if (m_client_list[i]->m_time_left_firm_stamina < 0) m_client_list[i]->m_time_left_firm_stamina = 0;

				// Crusade
				if (m_client_list[i] == 0) break;
				if (m_client_list[i]->m_is_sending_map_status) m_war_manager->send_map_status(i);

				if (m_client_list[i]->m_construction_point > 0) {
					m_war_manager->check_commander_construction_point(i);
				}
			}
		}
	}
}

void CGame::response_player_data_handler(char* data, uint32_t size)
{
	char* cp, char_name[hb::shared::limits::CharNameLen];

	std::memset(char_name, 0, sizeof(char_name));
	const auto* header = hb::net::PacketCast<hb::net::PacketHeader>(data, sizeof(hb::net::PacketHeader));
	if (!header) return;
	cp = (char*)(data + sizeof(hb::net::PacketHeader));

	memcpy(char_name, cp, hb::shared::limits::CharNameLen);
	cp += hb::shared::limits::CharNameLen;

	for(int i = 1; i < MaxClients; i++)
		if (m_client_list[i] != 0) {
			if (hb_strnicmp(m_client_list[i]->m_char_name, char_name, hb::shared::limits::CharNameLen - 1) == 0) {
				switch (header->msg_type) {
				case LogResMsg::Confirm:
					init_player_data(i, data, size);
					break;

				case LogResMsg::Reject:
					hb::logger::log("Non-existent character '{}' data request, rejected", m_client_list[i]->m_char_name);
					//PutLogFileList(G_cTxt); // v1.4

					delete_client(i, false, false);
					break;

				default:
					break;
				}

				return;
			}
		}

	hb::logger::log("Non-existent player data from login server: {}", char_name);
}

bool CGame::load_player_data_from_db(int client_h)
{
	if (m_client_list[client_h] == 0) return false;

	sqlite3* db = nullptr;
	std::string dbPath;
	if (!EnsureAccountDatabase(m_client_list[client_h]->m_account_name, &db, dbPath)) {
		return false;
	}

	AccountDbCharacterState state = {};
	if (!LoadCharacterState(db, m_client_list[client_h]->m_char_name, state)) {
		CloseAccountDatabase(db);
		return false;
	}

	std::memset(m_client_list[client_h]->m_profile, 0, sizeof(m_client_list[client_h]->m_profile));
	std::snprintf(m_client_list[client_h]->m_profile, sizeof(m_client_list[client_h]->m_profile), "%s", state.profile);

	std::memset(m_client_list[client_h]->m_location, 0, sizeof(m_client_list[client_h]->m_location));
	std::snprintf(m_client_list[client_h]->m_location, sizeof(m_client_list[client_h]->m_location), "%s", state.location);

	std::memset(m_client_list[client_h]->m_map_name, 0, sizeof(m_client_list[client_h]->m_map_name));
	std::snprintf(m_client_list[client_h]->m_map_name, sizeof(m_client_list[client_h]->m_map_name), "%s", state.map_name);
	m_client_list[client_h]->m_map_index = -1;
	for(int i = 0; i < MaxMaps; i++) {
		if ((m_map_list[i] != 0) && (memcmp(m_map_list[i]->m_name, m_client_list[client_h]->m_map_name, 10) == 0)) {
			m_client_list[client_h]->m_map_index = (char)i;
			break;
		}
	}
	if (m_client_list[client_h]->m_map_index == -1) {
		hb::logger::log("Player '{}' tried to enter unknown map: {}", m_client_list[client_h]->m_char_name, m_client_list[client_h]->m_map_name);
		CloseAccountDatabase(db);
		return false;
	}

	m_client_list[client_h]->m_x = (short)state.map_x;
	m_client_list[client_h]->m_y = (short)state.map_y;
	m_client_list[client_h]->m_hp = state.hp;
	m_client_list[client_h]->m_mp = state.mp;
	m_client_list[client_h]->m_sp = state.sp;
	m_client_list[client_h]->m_level = state.level;
	m_client_list[client_h]->m_rating = state.rating;
	m_client_list[client_h]->m_str = state.str;
	m_client_list[client_h]->m_int = state.intl;
	m_client_list[client_h]->m_vit = state.vit;
	m_client_list[client_h]->m_dex = state.dex;
	m_client_list[client_h]->m_mag = state.mag;
	m_client_list[client_h]->m_charisma = state.chr;
	m_client_list[client_h]->m_luck = state.luck;
	m_client_list[client_h]->m_exp = state.exp;
	m_client_list[client_h]->m_levelup_pool = state.lu_pool;
	m_client_list[client_h]->m_enemy_kill_count = state.enemy_kill_count;
	m_client_list[client_h]->m_player_kill_count = state.pk_count;
	m_client_list[client_h]->m_reward_gold = state.reward_gold;
	m_client_list[client_h]->m_down_skill_index = state.down_skill_index;
	m_client_list[client_h]->m_char_id_num1 = (short)state.id_num1;
	m_client_list[client_h]->m_char_id_num2 = (short)state.id_num2;
	m_client_list[client_h]->m_char_id_num3 = (short)state.id_num3;
	m_client_list[client_h]->m_sex = (char)state.sex;
	m_client_list[client_h]->m_skin = (char)state.skin;
	m_client_list[client_h]->m_hair_style = (char)state.hair_style;
	m_client_list[client_h]->m_hair_color = (char)state.hair_color;
	m_client_list[client_h]->m_underwear = (char)state.underwear;
	m_client_list[client_h]->m_hunger_status = state.hunger_status;
	m_client_list[client_h]->m_time_left_rating = state.timeleft_rating;
	m_client_list[client_h]->m_time_left_force_recall = state.timeleft_force_recall;
	m_client_list[client_h]->m_time_left_firm_stamina = state.timeleft_firm_stamina;
	m_client_list[client_h]->m_penalty_block_year = state.penalty_block_year;
	m_client_list[client_h]->m_penalty_block_month = state.penalty_block_month;
	m_client_list[client_h]->m_penalty_block_day = state.penalty_block_day;
	m_client_list[client_h]->m_quest = state.quest_number;
	m_client_list[client_h]->m_quest_id = state.quest_id;
	m_client_list[client_h]->m_cur_quest_count = state.current_quest_count;
	m_client_list[client_h]->m_quest_reward_type = state.quest_reward_type;
	m_client_list[client_h]->m_quest_reward_amount = state.quest_reward_amount;
	m_client_list[client_h]->m_contribution = state.contribution;
	m_client_list[client_h]->m_war_contribution = state.war_contribution;
	m_client_list[client_h]->m_is_quest_completed = (state.quest_completed != 0);
	m_client_list[client_h]->m_special_event_id = state.special_event_id;
	m_client_list[client_h]->m_super_attack_left = state.super_attack_left;
	m_client_list[client_h]->m_special_ability_time = state.special_ability_time;
	std::memset(m_client_list[client_h]->m_locked_map_name, 0, sizeof(m_client_list[client_h]->m_locked_map_name));
	std::snprintf(m_client_list[client_h]->m_locked_map_name, sizeof(m_client_list[client_h]->m_locked_map_name), "%s", state.locked_map_name);
	m_client_list[client_h]->m_locked_map_time = state.locked_map_time;
	m_client_list[client_h]->m_crusade_duty = state.crusade_job;
	m_client_list[client_h]->m_crusade_guid = state.crusade_guid;
	m_client_list[client_h]->m_construction_point = state.construct_point;
	m_client_list[client_h]->m_dead_penalty_time = state.dead_penalty_time;
	m_client_list[client_h]->m_party_id = state.party_id;
	m_client_list[client_h]->m_gizon_item_upgrade_left = state.gizon_item_upgrade_left;
	m_client_list[client_h]->m_appearance = state.appearance;

	for(int i = 0; i < hb::shared::limits::MaxItems; i++) {
		if (m_client_list[client_h]->m_item_list[i] != 0) {
			delete m_client_list[client_h]->m_item_list[i];
			m_client_list[client_h]->m_item_list[i] = 0;
		}
		m_client_list[client_h]->m_item_pos_list[i].x = 40;
		m_client_list[client_h]->m_item_pos_list[i].y = 30;
		m_client_list[client_h]->m_is_item_equipped[i] = false;
	}

	for(int i = 0; i < hb::shared::limits::MaxBankItems; i++) {
		if (m_client_list[client_h]->m_item_in_bank_list[i] != 0) {
			delete m_client_list[client_h]->m_item_in_bank_list[i];
			m_client_list[client_h]->m_item_in_bank_list[i] = 0;
		}
	}

	std::vector<AccountDbIndexedValue> positionsX;
	std::vector<AccountDbIndexedValue> positionsY;
	LoadCharacterItemPositions(db, m_client_list[client_h]->m_char_name, positionsX, positionsY);
	for (size_t i = 0; i < positionsX.size(); i++) {
		int slot = positionsX[i].index;
		if (slot >= 0 && slot < hb::shared::limits::MaxItems) {
			m_client_list[client_h]->m_item_pos_list[slot].x = positionsX[i].value;
			m_client_list[client_h]->m_item_pos_list[slot].y = positionsY[i].value;
		}
	}

	std::vector<AccountDbItemRow> items;
	LoadCharacterItems(db, m_client_list[client_h]->m_char_name, items);
	for (const auto& item : items) {
		if (item.slot < 0 || item.slot >= hb::shared::limits::MaxItems) {
			continue;
		}
		if (m_client_list[client_h]->m_item_list[item.slot] != 0) {
			delete m_client_list[client_h]->m_item_list[item.slot];
		}
		m_client_list[client_h]->m_item_list[item.slot] = new CItem;
		if (m_item_manager->init_item_attr(m_client_list[client_h]->m_item_list[item.slot], item.item_id) == false) {
			delete m_client_list[client_h]->m_item_list[item.slot];
			m_client_list[client_h]->m_item_list[item.slot] = 0;
			continue;
		}
		m_client_list[client_h]->m_item_list[item.slot]->m_instance.count = item.count;
		m_client_list[client_h]->m_item_list[item.slot]->m_instance.touch_effect_type = item.touch_effect_type;
		m_client_list[client_h]->m_item_list[item.slot]->m_instance.touch_effect_value1 = item.touch_effect_value1;
		m_client_list[client_h]->m_item_list[item.slot]->m_instance.touch_effect_value2 = item.touch_effect_value2;
		m_client_list[client_h]->m_item_list[item.slot]->m_instance.touch_effect_value3 = item.touch_effect_value3;
		m_client_list[client_h]->m_item_list[item.slot]->m_instance.item_color = item.item_color;
		m_client_list[client_h]->m_item_list[item.slot]->m_instance.special_effect_value1 = item.spec_effect_value1;
		m_client_list[client_h]->m_item_list[item.slot]->m_instance.special_effect_value2 = item.spec_effect_value2;
		m_client_list[client_h]->m_item_list[item.slot]->m_instance.special_effect_value3 = item.spec_effect_value3;
		m_client_list[client_h]->m_item_list[item.slot]->m_instance.cur_durability = (short)item.cur_durability;
		m_client_list[client_h]->m_item_list[item.slot]->load_attributes_from(item);

		if (m_client_list[client_h]->m_item_list[item.slot]->m_instance.custom_made) {
			m_client_list[client_h]->m_item_list[item.slot]->m_durability = m_client_list[client_h]->m_item_list[item.slot]->m_instance.special_effect_value1;
		}
		m_item_manager->adjust_rare_item_value(m_client_list[client_h]->m_item_list[item.slot]);
		if (m_client_list[client_h]->m_item_list[item.slot]->m_instance.cur_durability > m_client_list[client_h]->m_item_list[item.slot]->m_durability) {
			m_client_list[client_h]->m_item_list[item.slot]->m_instance.cur_durability = m_client_list[client_h]->m_item_list[item.slot]->m_durability;
		}
		m_item_manager->check_and_convert_plus_weapon_item(client_h, item.slot);
	}

	std::vector<AccountDbBankItemRow> bankItems;
	LoadCharacterBankItems(db, m_client_list[client_h]->m_char_name, bankItems);
	for (const auto& item : bankItems) {
		if (item.slot < 0 || item.slot >= hb::shared::limits::MaxBankItems) {
			continue;
		}
		if (m_client_list[client_h]->m_item_in_bank_list[item.slot] != 0) {
			delete m_client_list[client_h]->m_item_in_bank_list[item.slot];
		}
		m_client_list[client_h]->m_item_in_bank_list[item.slot] = new CItem;
		if (m_item_manager->init_item_attr(m_client_list[client_h]->m_item_in_bank_list[item.slot], item.item_id) == false) {
			delete m_client_list[client_h]->m_item_in_bank_list[item.slot];
			m_client_list[client_h]->m_item_in_bank_list[item.slot] = 0;
			continue;
		}
		m_client_list[client_h]->m_item_in_bank_list[item.slot]->m_instance.count = item.count;
		m_client_list[client_h]->m_item_in_bank_list[item.slot]->m_instance.touch_effect_type = item.touch_effect_type;
		m_client_list[client_h]->m_item_in_bank_list[item.slot]->m_instance.touch_effect_value1 = item.touch_effect_value1;
		m_client_list[client_h]->m_item_in_bank_list[item.slot]->m_instance.touch_effect_value2 = item.touch_effect_value2;
		m_client_list[client_h]->m_item_in_bank_list[item.slot]->m_instance.touch_effect_value3 = item.touch_effect_value3;
		m_client_list[client_h]->m_item_in_bank_list[item.slot]->m_instance.item_color = item.item_color;
		m_client_list[client_h]->m_item_in_bank_list[item.slot]->m_instance.special_effect_value1 = item.spec_effect_value1;
		m_client_list[client_h]->m_item_in_bank_list[item.slot]->m_instance.special_effect_value2 = item.spec_effect_value2;
		m_client_list[client_h]->m_item_in_bank_list[item.slot]->m_instance.special_effect_value3 = item.spec_effect_value3;
		m_client_list[client_h]->m_item_in_bank_list[item.slot]->m_instance.cur_durability = (short)item.cur_durability;
		m_client_list[client_h]->m_item_in_bank_list[item.slot]->load_attributes_from(item);
		if (m_client_list[client_h]->m_item_in_bank_list[item.slot]->m_instance.custom_made) {
			m_client_list[client_h]->m_item_in_bank_list[item.slot]->m_durability = m_client_list[client_h]->m_item_in_bank_list[item.slot]->m_instance.special_effect_value1;
		}
		m_item_manager->adjust_rare_item_value(m_client_list[client_h]->m_item_in_bank_list[item.slot]);
		if (m_client_list[client_h]->m_item_in_bank_list[item.slot]->m_instance.cur_durability > m_client_list[client_h]->m_item_in_bank_list[item.slot]->m_durability) {
			m_client_list[client_h]->m_item_in_bank_list[item.slot]->m_instance.cur_durability = m_client_list[client_h]->m_item_in_bank_list[item.slot]->m_durability;
		}
	}

	std::vector<AccountDbIndexedValue> equips;
	LoadCharacterItemEquips(db, m_client_list[client_h]->m_char_name, equips);
	for (const auto& equip : equips) {
		if (equip.index >= 0 && equip.index < hb::shared::limits::MaxItems) {
			m_client_list[client_h]->m_is_item_equipped[equip.index] = (equip.value != 0);
		}
	}

	int packedIndex = 0;
	for(int i = 0; i < hb::shared::limits::MaxItems; i++) {
		if (m_client_list[client_h]->m_item_list[i] == 0) {
			continue;
		}
		if (i != packedIndex) {
			m_client_list[client_h]->m_item_list[packedIndex] = m_client_list[client_h]->m_item_list[i];
			m_client_list[client_h]->m_item_list[i] = 0;
			m_client_list[client_h]->m_item_pos_list[packedIndex] = m_client_list[client_h]->m_item_pos_list[i];
			m_client_list[client_h]->m_is_item_equipped[packedIndex] = m_client_list[client_h]->m_is_item_equipped[i];
		}
		packedIndex++;
	}
	for(int i = packedIndex; i < hb::shared::limits::MaxItems; i++) {
		m_client_list[client_h]->m_item_pos_list[i].x = 40;
		m_client_list[client_h]->m_item_pos_list[i].y = 30;
		m_client_list[client_h]->m_is_item_equipped[i] = false;
	}

	packedIndex = 0;
	for(int i = 0; i < hb::shared::limits::MaxBankItems; i++) {
		if (m_client_list[client_h]->m_item_in_bank_list[i] == 0) {
			continue;
		}
		if (i != packedIndex) {
			m_client_list[client_h]->m_item_in_bank_list[packedIndex] = m_client_list[client_h]->m_item_in_bank_list[i];
			m_client_list[client_h]->m_item_in_bank_list[i] = 0;
		}
		packedIndex++;
	}
	for(int i = packedIndex; i < hb::shared::limits::MaxBankItems; i++) {
		m_client_list[client_h]->m_item_in_bank_list[i] = 0;
	}

	for(int i = 0; i < DEF_MAXITEMEQUIPPOS; i++) {
		m_client_list[client_h]->m_item_equipment_status[i] = -1;
	}

	for(int i = 0; i < hb::shared::limits::MaxItems; i++) {
		if ((m_client_list[client_h]->m_item_list[i] != 0) && m_client_list[client_h]->m_is_item_equipped[i]) {
			if (m_client_list[client_h]->m_item_list[i]->get_item_type() == hb::shared::item::item_type::equipment) {
				if (m_item_manager->equip_item_handler(client_h, i) == false) {
					m_client_list[client_h]->m_is_item_equipped[i] = false;
				}
			}
			else {
				m_client_list[client_h]->m_is_item_equipped[i] = false;
			}
		}
	}

	for(int i = 0; i < hb::shared::limits::MaxMagicType; i++) {
		m_client_list[client_h]->m_magic_mastery[i] = 0;
	}
	std::vector<AccountDbIndexedValue> magicMastery;
	LoadCharacterMagicMastery(db, m_client_list[client_h]->m_char_name, magicMastery);
	for (const auto& entry : magicMastery) {
		if (entry.index >= 0 && entry.index < hb::shared::limits::MaxMagicType) {
			m_client_list[client_h]->m_magic_mastery[entry.index] = (char)entry.value;
		}
	}

	for(int i = 0; i < hb::shared::limits::MaxSkillType; i++) {
		m_client_list[client_h]->m_skill_mastery[i] = 0;
		m_client_list[client_h]->m_skill_progress[i] = 0;
	}
	std::vector<AccountDbIndexedValue> skillMastery;
	LoadCharacterSkillMastery(db, m_client_list[client_h]->m_char_name, skillMastery);
	for (const auto& entry : skillMastery) {
		if (entry.index >= 0 && entry.index < hb::shared::limits::MaxSkillType) {
			m_client_list[client_h]->m_skill_mastery[entry.index] = (unsigned char)entry.value;
		}
	}

	std::vector<AccountDbIndexedValue> skillSsn;
	LoadCharacterSkillSSN(db, m_client_list[client_h]->m_char_name, skillSsn);
	for (const auto& entry : skillSsn) {
		if (entry.index >= 0 && entry.index < hb::shared::limits::MaxSkillType) {
			m_client_list[client_h]->m_skill_progress[entry.index] = entry.value;
		}
	}

	short tmp_type = 0;
	if (m_client_list[client_h]->m_sex == 1) {
		tmp_type = 1;
	}
	else if (m_client_list[client_h]->m_sex == 2) {
		tmp_type = 4;
	}
	switch (m_client_list[client_h]->m_skin) {
	case 1:
		break;
	case 2:
		tmp_type += 1;
		break;
	case 3:
		tmp_type += 2;
		break;
	}
	m_client_list[client_h]->m_type = tmp_type;
	m_client_list[client_h]->m_appearance.hair_style = m_client_list[client_h]->m_hair_style;
	m_client_list[client_h]->m_appearance.hair_color = m_client_list[client_h]->m_hair_color;
	m_client_list[client_h]->m_appearance.underwear_type = m_client_list[client_h]->m_underwear;

	if (m_client_list[client_h]->m_char_id_num1 == 0) {
		int temp1 = 1;
		int temp2 = 1;
		for(int i = 0; i < 10; i++) {
			temp1 += m_client_list[client_h]->m_char_name[i];
			temp2 += abs(m_client_list[client_h]->m_char_name[i] ^ m_client_list[client_h]->m_char_name[i]);
		}
		m_client_list[client_h]->m_char_id_num1 = (short)GameClock::GetTimeMS();
		m_client_list[client_h]->m_char_id_num2 = (short)temp1;
		m_client_list[client_h]->m_char_id_num3 = (short)temp2;
	}

	m_client_list[client_h]->m_speed_hack_check_exp = m_client_list[client_h]->m_exp;
	if (memcmp(m_client_list[client_h]->m_location, "NONE", 4) == 0) {
		m_client_list[client_h]->m_is_neutral = true;
	}

	// Load block list
	m_client_list[client_h]->m_blocked_accounts.clear();
	m_client_list[client_h]->m_blocked_accounts_list.clear();
	m_client_list[client_h]->m_block_list_dirty = false;
	std::vector<std::pair<std::string, std::string>> blocks;
	if (LoadBlockList(db, blocks)) {
		for (const auto& entry : blocks) {
			m_client_list[client_h]->m_blocked_accounts.insert(entry.first);
			m_client_list[client_h]->m_blocked_accounts_list.push_back(entry);
		}
	}

	CloseAccountDatabase(db);
	return true;
}

void CGame::init_player_data(int client_h, char* data, uint32_t size)
{
	char quest_remain;
	int     ret, total_points;
	bool    ret_ok;

	if (m_client_list[client_h] == 0) return;
	if (m_client_list[client_h]->m_is_init_complete) return;

	// Log Server
	//cp = (char *)(data + hb::shared::net::MessageOffsetType + 2);

	//std::memset(name, 0, sizeof(name));
	//memcpy(name, cp, hb::shared::limits::CharNameLen - 1);
	//cp += 10;

	////m_client_list[client_h]->m_cAccountStatus = *cp;
	//cp++;

	//cGuildStatus = *cp;
	//cp++;

	m_client_list[client_h]->m_hit_ratio = 0;
	m_client_list[client_h]->m_defense_ratio = 0;
	m_client_list[client_h]->m_side = 0;

	ret_ok = load_player_data_from_db(client_h);
	if (ret_ok == false) {
		std::snprintf(G_cTxt, sizeof(G_cTxt), "(HACK?) Character(%s) data error!", m_client_list[client_h]->m_char_name);
		delete_client(client_h, false, true);
		return;
	}

	restore_player_characteristics(client_h);

	// Clamp stats to max_stat_value on login
	{
		int* stats[] = {
			&m_client_list[client_h]->m_str, &m_client_list[client_h]->m_int,
			&m_client_list[client_h]->m_vit, &m_client_list[client_h]->m_dex,
			&m_client_list[client_h]->m_mag, &m_client_list[client_h]->m_charisma
		};
		for (auto* stat : stats)
		{
			if (*stat > m_max_stat_value)
			{
				m_client_list[client_h]->m_levelup_pool += (*stat - m_max_stat_value);
				*stat = m_max_stat_value;
			}
		}
	}

	restore_player_rating(client_h);

	if ((m_client_list[client_h]->m_x == -1) && (m_client_list[client_h]->m_y == -1)) {
		get_map_initial_point(m_client_list[client_h]->m_map_index, &m_client_list[client_h]->m_x, &m_client_list[client_h]->m_y, m_client_list[client_h]->m_location);
	}

	// New 17/05/2004
	set_playing_status(client_h);
	// Set faction/identity status fields from player data
	m_client_list[client_h]->m_status.pk = (m_client_list[client_h]->m_player_kill_count != 0) ? 1 : 0;
	m_client_list[client_h]->m_status.citizen = (m_client_list[client_h]->m_side != 0) ? 1 : 0;
	m_client_list[client_h]->m_status.aresden = (m_client_list[client_h]->m_side == 1) ? 1 : 0;
	m_client_list[client_h]->m_status.hunter = m_client_list[client_h]->m_is_player_civil ? 1 : 0;

	if (m_client_list[client_h]->m_level > 100)
		if (m_client_list[client_h]->m_is_player_civil)
			force_change_play_mode(client_h, false);

	m_client_list[client_h]->m_next_level_exp = m_level_exp_table[m_client_list[client_h]->m_level + 1]; //get_level_exp(m_client_list[client_h]->m_level + 1);

	m_item_manager->calc_total_item_effect(client_h, -1, true); //false
	calc_total_weight(client_h);

	total_points = 0;
	for(int i = 0; i < hb::shared::limits::MaxSkillType; i++)
		total_points += m_client_list[client_h]->m_skill_mastery[i];
#ifndef TESTER_ONLY
	// Skill point validation — disabled in tester builds for tester menu "Max all skills"
	if ((total_points - 21 > MaxSkillPoints) ) {
		try
		{
			hb::logger::warn<log_channel::security>("Packet editing: IP={} player={}, exceeds allowed skill points ({})", m_client_list[client_h]->m_ip_address, m_client_list[client_h]->m_char_name, total_points);
			delete_client(client_h, true, true);
		}
		catch (...)
		{
		}
		return;
	}
#endif

	check_special_event(client_h);
	m_item_manager->validate_equipped_items(client_h);

	send_notify_msg(0, client_h, Notify::Hunger, m_client_list[client_h]->m_hunger_status, 0, 0, 0);

	if (m_client_list[client_h]->m_quest > 0) {
		// Validate quest number to prevent out-of-bounds access
		if(m_client_list[client_h]->m_quest >= hb::server::config::MaxQuestType || !m_quest_manager->m_quest_config_list[m_client_list[client_h]->m_quest])
		{
			m_client_list[client_h]->m_quest = 0;
			m_client_list[client_h]->m_cur_quest_count = 0;
			m_client_list[client_h]->m_quest_reward_amount = 0;
			m_client_list[client_h]->m_quest_reward_type = 0;
			m_client_list[client_h]->m_is_quest_completed = false;
		}
		else {
			quest_remain = (m_quest_manager->m_quest_config_list[m_client_list[client_h]->m_quest]->m_max_count - m_client_list[client_h]->m_cur_quest_count);
			send_notify_msg(0, client_h, Notify::QuestCounter, quest_remain, 0, 0, 0);
			m_quest_manager->check_is_quest_completed(client_h);
		}
	}

	if (m_client_list[client_h] == 0) {
		hb::logger::log("Client {}: init player data failed (socket error), disconnected", client_h);
		return;
	}

	hb::net::PacketResponseInitPlayer pkt{};
	pkt.header.msg_id = MsgId::ResponseInitPlayer;
	pkt.header.msg_type = MsgType::Confirm;

	ret = m_client_list[client_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	switch (ret) {
	case sock::Event::QueueFull:
	case sock::Event::SocketError:
	case sock::Event::CriticalError:
	case sock::Event::SocketClosed:
		// ## BUG POINT!!!
		hb::logger::log("Client {}: init player data socket error, disconnected", client_h);

		delete_client(client_h, false, true);
		return;
	}

	m_client_list[client_h]->m_is_init_complete = true;
}

void CGame::game_process()
{
	// MODERNIZED: Socket polling moved to EventLoop (wmain.cpp) for continuous responsiveness
	// This function now handles only game logic processing
	npc_process();
	msg_process();
	force_recall_process();
	m_delay_event_manager->delay_event_process();
}

// Helper function to normalize item name for comparison (removes spaces and underscores)
static void NormalizeItemName(const char* src, char* dst, size_t dstSize)
{
	size_t j = 0;
	for (size_t i = 0; src[i] && j < dstSize - 1; ++i) {
		if (src[i] != ' ' && src[i] != '_') {
			dst[j++] = src[i];
		}
	}
	dst[j] = '\0';
}

bool CGame::get_is_string_is_number(char* str)
{
	
	for(int i = 0; i < (int)strlen(str); i++)
		if ((str[i] != '-') && ((str[i] < (char)'0') || (str[i] > (char)'9'))) return false;

	return true;
}

int CGame::create_new_npc(int npc_config_id, char* name, char* map_name, short sClass, char sa, char move_type, int* offset_x, int* offset_y, char* waypoint_list, hb::shared::geometry::GameRectangle* area, int spot_mob_index, char change_side, bool hide_gen_mode, bool is_summoned, bool firm_berserk, bool is_master, bool bypass_mob_limit)
{
	if (m_entity_manager == 0)
		return false;

	return (m_entity_manager->create_entity(
		npc_config_id, name, map_name, sClass, sa, move_type,
		offset_x, offset_y, waypoint_list, area, spot_mob_index, change_side,
		hide_gen_mode, is_summoned, firm_berserk, is_master, bypass_mob_limit) > 0);
}

int CGame::spawn_map_npcs_from_database(sqlite3* db, int map_index)
{
	if (db == nullptr || m_map_list[map_index] == nullptr)
		return 0;

	const char* sql =
		"SELECT npc_config_id, move_type, waypoint_list, name_prefix"
		" FROM map_npcs WHERE map_name = ? COLLATE NOCASE;";

	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
		return 0;
	}

	sqlite3_bind_text(stmt, 1, m_map_list[map_index]->m_name, -1, SQLITE_STATIC);

	int npcCount = 0;
	char npc_waypoint_index[12];
	char name_prefix;
	char npc_move_type;
	char name[8];
	int naming_value;

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		// get NPC config ID directly
		int npc_config_id = sqlite3_column_int(stmt, 0);
		if (npc_config_id < 0 || npc_config_id >= MaxNpcTypes || m_npc_config_list[npc_config_id] == nullptr) {
			hb::logger::log("Invalid npc_config_id {} in map_npcs for map {}", npc_config_id, m_map_list[map_index]->m_name);
			continue;
		}

		// get move type
		npc_move_type = static_cast<char>(sqlite3_column_int(stmt, 1));

		// get waypoint list (comma-separated string like "0,0,0,0,0,0,0,0,0,0")
		std::memset(npc_waypoint_index, 0, sizeof(npc_waypoint_index));
		const char* waypointList = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
		if (waypointList != nullptr && strlen(waypointList) > 0) {
			char waypointCopy[64];
			std::snprintf(waypointCopy, sizeof(waypointCopy), "%s", waypointList);

			char* token = strtok(waypointCopy, ",");
			int wpIndex = 0;
			while (token != nullptr && wpIndex < 10) {
				npc_waypoint_index[wpIndex] = static_cast<char>(atoi(token));
				token = strtok(nullptr, ",");
				wpIndex++;
			}
		}

		// get name prefix
		const char* prefix = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
		name_prefix = (prefix != nullptr && strlen(prefix) > 0) ? prefix[0] : '_';

		// get a naming value for this NPC
		naming_value = m_map_list[map_index]->get_empty_naming_value();
		if (naming_value == -1) {
			continue;
		}

		// Construct the NPC instance name
		std::memset(name, 0, sizeof(name));
		std::snprintf(name, sizeof(name), "XX%d", naming_value);
		name[0] = name_prefix;
		name[1] = static_cast<char>(map_index + 65);

		// Spawn the NPC
		if (create_new_npc(npc_config_id, name, m_map_list[map_index]->m_name, 0, 0, npc_move_type, 0, 0, npc_waypoint_index, 0, 0, -1, false) == false) {
			m_map_list[map_index]->set_naming_value_empty(naming_value);
			hb::logger::warn("- Failed to spawn static NPC (config_id={}) for map: {}", npc_config_id, m_map_list[map_index]->m_name);
		}
		else {
			npcCount++;
		}
	}

	sqlite3_finalize(stmt);

	return npcCount;
}

void CGame::npc_process()
{
	if (m_entity_manager != 0)
		m_entity_manager->process_entities();
}

void CGame::broadcast_server_message(const char* message)
{
	if (message == nullptr || message[0] == '\0')
		return;

	// Build a chat packet: header(6) + x(2) + y(2) + name(10) + chat_type(1) + message + null
	char pkt[256];
	std::memset(pkt, 0, sizeof(pkt));

	size_t msgLen = std::strlen(message);
	if (msgLen > sizeof(pkt) - 22) msgLen = sizeof(pkt) - 22;

	auto& chatPkt = *reinterpret_cast<hb::net::PacketChatMsg*>(pkt);
	chatPkt.header.msg_id = MsgId::CommandChatMsg;
	chatPkt.header.msg_type = 0;
	chatPkt.reserved1 = 0;
	chatPkt.reserved2 = 0;
	std::memcpy(chatPkt.name, "Server", 6);
	chatPkt.chat_type = 10;

	// Message text follows immediately after the chat header
	constexpr size_t chat_header_size = sizeof(hb::net::PacketChatMsg);
	std::memcpy(pkt + chat_header_size, message, msgLen);
	pkt[chat_header_size + msgLen] = '\0';

	uint32_t size = static_cast<uint32_t>(chat_header_size + msgLen + 1);

	for(int i = 1; i < MaxClients; i++)
	{
		if (m_client_list[i] != nullptr && m_client_list[i]->m_is_init_complete)
			m_client_list[i]->m_socket->send_msg(pkt, size);
	}

	hb::logger::log<log_channel::chat>("[Broadcast] {}: {}", "Server", message);
}

bool CGame::is_blocked_by(int sender_h, int receiver_h) const
{
	if (m_client_list[sender_h] == nullptr || m_client_list[receiver_h] == nullptr)
		return false;
	return m_client_list[receiver_h]->m_blocked_accounts.count(
		m_client_list[sender_h]->m_account_name) > 0;
}

// 05/29/2004 - Hypnotoad - GM chat tweak
void CGame::chat_msg_handler(int client_h, char* data, size_t msg_size)
{
	int ret = sock::Event::CriticalError;
	char* cp = 0;
	uint8_t send_mode = 0;

	if (m_client_list[client_h] == 0) return;
	if (m_client_list[client_h]->m_is_init_complete == false) return;
	if (msg_size > 83 + 30) {
		hb::logger::debug<log_channel::chat>("[ChatMsg] Rejected from client {} — size {} exceeds max", client_h, msg_size);
		return;
	}

	auto* header = hb::net::PacketCast<hb::net::PacketHeader>(data, sizeof(hb::net::PacketHeader));
	if (!header) return;
	const auto* pkt = hb::net::PacketCast<hb::net::PacketCommandChatMsgHeader>(data, sizeof(hb::net::PacketCommandChatMsgHeader));
	if (!pkt) return;
	char* message = data + sizeof(hb::net::PacketCommandChatMsgHeader);

	hb::logger::debug<log_channel::chat>("[ChatMsg] client={} pkt->name='{}' expected='{}' msgSize={}",
		client_h, pkt->name, m_client_list[client_h]->m_char_name, msg_size);

	if (hb_strnicmp(pkt->name, m_client_list[client_h]->m_char_name, strlen(m_client_list[client_h]->m_char_name)) != 0) {
		hb::logger::debug<log_channel::chat>("[ChatMsg] Name mismatch — possible struct packing issue. PacketCommandChatMsgHeader size={}", sizeof(hb::net::PacketCommandChatMsgHeader));
		return;
	}

	if (m_client_list[client_h]->m_is_observer_mode) return;

	// v1.432-2
	int st_x, st_y;
	if (m_map_list[m_client_list[client_h]->m_map_index] != 0) {
		st_x = m_client_list[client_h]->m_x / 20;
		st_y = m_client_list[client_h]->m_y / 20;
		m_map_list[m_client_list[client_h]->m_map_index]->m_temp_sector_info[st_x][st_y].player_activity++;

		switch (m_client_list[client_h]->m_side) {
		case 0: m_map_list[m_client_list[client_h]->m_map_index]->m_temp_sector_info[st_x][st_y].neutral_activity++; break;
		case 1: m_map_list[m_client_list[client_h]->m_map_index]->m_temp_sector_info[st_x][st_y].aresden_activity++; break;
		case 2: m_map_list[m_client_list[client_h]->m_map_index]->m_temp_sector_info[st_x][st_y].elvine_activity++;  break;
		}
	}
	cp = message;

	switch (*cp) {
	case '@':
		*cp = 32;

		if ((m_client_list[client_h]->m_level > 1) &&
			(m_client_list[client_h]->m_sp >= 3)) {
			if (m_client_list[client_h]->m_time_left_firm_stamina == 0) {
				m_client_list[client_h]->m_sp -= 3;
				send_notify_msg(0, client_h, Notify::Sp, 0, 0, 0, 0);
			}
			send_mode = 1;
		}
		else send_mode = 0;
		break;

		// New 08/05/2004
		// Party chat
	case '$':
		*cp = 32;

		if (m_client_list[client_h]->m_sp >= 3) {
			if (m_client_list[client_h]->m_time_left_firm_stamina == 0) {
				m_client_list[client_h]->m_sp -= 3;
				send_notify_msg(0, client_h, Notify::Sp, 0, 0, 0, 0);
			}
			send_mode = 4;
		}
		else {
			send_mode = 0;
		}
		break;

	case '^':
		*cp = 32;

		// Guild chat removed
		send_mode = 0;

		// v1.4334
		if (m_client_list[client_h]->m_hp < 0) send_mode = 0;

		break;

	case '!':
		*cp = 32;

		if ((m_client_list[client_h]->m_level > 10) &&
			(m_client_list[client_h]->m_sp >= 5)) {
			if (m_client_list[client_h]->m_time_left_firm_stamina == 0) {
				m_client_list[client_h]->m_sp -= 5;
				send_notify_msg(0, client_h, Notify::Sp, 0, 0, 0, 0);
			}
			send_mode = 2;
		}
		else send_mode = 0;

		// v1.4334
		if (m_client_list[client_h]->m_hp <= 0) send_mode = 0;

		break;

	case '~':
		*cp = 32;
		if ((m_client_list[client_h]->m_level > 1) &&
			(m_client_list[client_h]->m_sp >= 3)) {
			if (m_client_list[client_h]->m_time_left_firm_stamina == 0) {
				m_client_list[client_h]->m_sp -= 3;
				send_notify_msg(0, client_h, Notify::Sp, 0, 0, 0, 0);
			}
			send_mode = 3;
		}
		else send_mode = 0;
		// v1.4334
		if (m_client_list[client_h]->m_hp <= 0) send_mode = 0;
		break;

	case '/':
		if (GameChatCommandManager::get().process_command(client_h, cp, msg_size - sizeof(hb::net::PacketCommandChatMsgHeader)))
			return;
		// Not a recognized command - fall through as normal chat
		break;
	}

	data[msg_size - 1] = 0;

	if ((m_client_list[client_h]->m_magic_effect_status[hb::shared::magic::Confuse] == 1) && (dice(1, 3) != 2)) {
		// Confuse Language
		cp = message;

		while (*cp != 0) {
			if ((cp[0] != 0) && (cp[0] != ' ') && (cp[1] != 0) && (cp[1] != ' ')) {
				switch (dice(1, 3)) {
				case 1:	memcpy(cp, "¿ö", 2); break;
				case 2:	memcpy(cp, "¿ì", 2); break;
				case 3:	memcpy(cp, "¿ù", 2); break;
				}
				cp += 2;
			}
			else cp++;
		}
	}

	cp = message;

	if ((send_mode == 0) && (m_client_list[client_h]->m_whisper_player_index != -1)) {
		send_mode = 20;

		if (*cp == '#') send_mode = 0;
	}

	// Refresh AFK timer for non-whisper chat (whispers don't break AFK)
	if (send_mode != 20) {
		m_client_list[client_h]->m_afk_activity_time = GameClock::GetTimeMS();
	}

	std::string chat_type = "Local";
	std::string player_name = m_client_list[client_h]->m_char_name;

	switch (send_mode)
	{
	case 0:  chat_type = "[Local]"; break;
	case 1:  chat_type = "[Local]"; break; // Guild chat removed, fall through to local
	case 2:  chat_type = "[Shout]"; break;
	case 3:  chat_type = "[Faction]"; break;
	case 4:  chat_type = "[Party]"; break;
	case 10: chat_type = "[GMBroadcast]"; break;
	case 20: chat_type = "[Whisper]"; break;
	}
	if (send_mode == 20)
	{
		std::string whispered_player = m_client_list[client_h]->m_whisper_player_name;
		hb::logger::log<log_channel::chat>("{} {}->{}:{}", chat_type, player_name, whispered_player, message);
	}
	else {
		hb::logger::log<log_channel::chat>("{} {}:{}", chat_type, player_name, message);
	}

	header->msg_type = (uint16_t)client_h;
	// Write chat send mode into the packet's chat_type field before relaying to clients
	data[offsetof(hb::net::PacketCommandChatMsgHeader, chat_type)] = send_mode;

	if (send_mode != 20) {
		for(int i = 1; i < MaxClients; i++)
			if (m_client_list[i] != 0) {
				if (is_blocked_by(client_h, i)) continue;
				switch (send_mode) {
				case 0:
					if (m_client_list[i]->m_is_init_complete == false) break;

					if ((m_client_list[i]->m_map_index == m_client_list[client_h]->m_map_index) &&
						(m_client_list[i]->m_x > m_client_list[client_h]->m_x - hb::shared::view::CenterX) &&
						(m_client_list[i]->m_x < m_client_list[client_h]->m_x + hb::shared::view::CenterX) &&
						(m_client_list[i]->m_y > m_client_list[client_h]->m_y - hb::shared::view::CenterY) &&
						(m_client_list[i]->m_y < m_client_list[client_h]->m_y + hb::shared::view::CenterY)) {

						// Crusade
						if (m_is_crusade_mode) {
							if ((m_client_list[client_h]->m_side != 0) && (m_client_list[i]->m_side != 0) &&
								(m_client_list[i]->m_side != m_client_list[client_h]->m_side)) {
							}
							else ret = m_client_list[i]->m_socket->send_msg(data, msg_size);
						}
						else ret = m_client_list[i]->m_socket->send_msg(data, msg_size);
					}
					break;

				case 1:
					// Guild chat removed
					break;

				case 2:
				case 10:
					// Crusade
					if (m_is_crusade_mode) {
						if ((m_client_list[client_h]->m_side != 0) && (m_client_list[i]->m_side != 0) &&
							(m_client_list[i]->m_side != m_client_list[client_h]->m_side)) {
						}
						else ret = m_client_list[i]->m_socket->send_msg(data, msg_size);
					}
					else ret = m_client_list[i]->m_socket->send_msg(data, msg_size);
					break;

				case 3:
					if (m_client_list[i]->m_is_init_complete == false) break;

					if ((m_client_list[i]->m_side == m_client_list[client_h]->m_side))
						ret = m_client_list[i]->m_socket->send_msg(data, msg_size);
					break;

				case 4:
					if (m_client_list[i]->m_is_init_complete == false) break;
					if ((m_client_list[i]->m_party_id != 0) && (m_client_list[i]->m_party_id == m_client_list[client_h]->m_party_id))
						ret = m_client_list[i]->m_socket->send_msg(data, msg_size);
					break;
				}

				switch (ret) {
				case sock::Event::QueueFull:
				case sock::Event::SocketError:
				case sock::Event::CriticalError:
				case sock::Event::SocketClosed:
					//delete_client(i, true, true);
					break;
				}
			}
	}
	else {
		// New 16/05/2004
		ret = m_client_list[client_h]->m_socket->send_msg(data, msg_size);
		{
			int whisperTarget = m_client_list[client_h]->m_whisper_player_index;
			if (m_client_list[whisperTarget] != 0 &&
				hb_stricmp(m_client_list[client_h]->m_whisper_player_name, m_client_list[whisperTarget]->m_char_name) == 0) {
				if (!is_blocked_by(client_h, whisperTarget)) {
					ret = m_client_list[whisperTarget]->m_socket->send_msg(data, msg_size);
				}
			}
		}

		switch (ret) {
		case sock::Event::QueueFull:
		case sock::Event::SocketError:
		case sock::Event::CriticalError:
		case sock::Event::SocketClosed:
			//delete_client(i, true, true);
			break;
		}
	}
}

void CGame::chat_msg_handler_gsm(int msg_type, int v1, char* name, char* data, size_t msg_size)
{
	int ret;
	char temp[256], send_mode = 0;

	std::memset(temp, 0, sizeof(temp));

	auto& chatPkt = *reinterpret_cast<hb::net::PacketChatMsg*>(temp);
	chatPkt.header.msg_id = MsgId::CommandChatMsg;
	chatPkt.header.msg_type = 0;
	chatPkt.reserved1 = 0;
	chatPkt.reserved2 = 0;
	std::memcpy(chatPkt.name, name, sizeof(chatPkt.name));
	chatPkt.chat_type = static_cast<uint8_t>(msg_type);

	char* cp = temp + sizeof(hb::net::PacketChatMsg);
	memcpy(cp, data, msg_size);
	cp += msg_size;

	switch (msg_type) {
	case 1:
		// Guild chat removed
		break;

	case 2:
	case 10:
		for(int i = 1; i < MaxClients; i++)
			if (m_client_list[i] != 0) {
				ret = m_client_list[i]->m_socket->send_msg(temp, msg_size + 22);
			}
		break;
	}
}

//  int CGame::client_motion_attack_handler(int client_h, short sX, short sY, short dX, short dY, short type, char dir, uint16_t target_object_id, bool response, bool is_dash)
//  description			:: controls player attack
//	return value		:: int
//  last updated		:: October 29, 2004; 8:06 PM; Hypnotoad
//  commentary			:: - contains attack hack detection
//						   - added checks for Firebow and Directionbow to see if player is m_is_inside_warehouse, m_is_inside_wizard_tower, m_is_inside_own_town 
//						   - added ability to attack moving object
//						   - fixed attack unmoving object
int CGame::client_motion_attack_handler(int client_h, short sX, short sY, short dX, short dY, short type, direction dir, uint16_t target_object_id, uint32_t client_time, bool response, bool is_dash)
{
	uint32_t time, exp;
	int     ret, tdX = 0, tdY = 0;
	short   owner, abs_x, abs_y;
	char    owner_type;
	bool    near_attack = false, var_AC = false;
	short item_index;
	int tX, tY, err, st_x, st_y;

	if (m_client_list[client_h] == 0) return 0;
	if ((dir <= 0) || (dir > 8))       return 0;
	if (m_client_list[client_h]->m_is_init_complete == false) return 0;
	if (m_client_list[client_h]->m_is_killed) return 0;

	time = GameClock::GetTimeMS();
	m_client_list[client_h]->m_last_action_time = time;
	m_client_list[client_h]->m_attack_msg_recv_count++;
	if (m_client_list[client_h]->m_attack_msg_recv_count >= 7) {
		if (m_client_list[client_h]->m_attack_last_action_time != 0) {
			// Compute expected time for 7 consecutive attacks from weapon speed and status.
			// Uses client time to avoid false positives from TCP buffering/network jitter.
			constexpr int BATCH_TOLERANCE_MS = 100;

			const auto& status = m_client_list[client_h]->m_status;
			int base_swing = hb::shared::calc::swing_time(status.attack_delay);
			int effective_swing = base_swing;
			if (status.frozen) effective_swing += hb::shared::balance::swing_frames * (hb::shared::balance::base_frame_time >> 2);
			if (status.haste)  effective_swing -= hb::shared::balance::swing_frames * static_cast<int>(hb::shared::balance::run_frame_time / 2.3);

			int singleSwingTime = effective_swing;
			int batchThreshold = 7 * singleSwingTime - BATCH_TOLERANCE_MS;
			if (batchThreshold < 2800) batchThreshold = 2800;

			uint32_t client_gap = client_time - m_client_list[client_h]->m_attack_last_action_time;
			if (client_gap < static_cast<uint32_t>(batchThreshold)) {
				hb::logger::warn<log_channel::security>("Batch swing hack: IP={} player={}, 7 attacks in {}ms (min={}ms)", m_client_list[client_h]->m_ip_address, m_client_list[client_h]->m_char_name, client_gap, batchThreshold);
				delete_client(client_h, true, true, true);
				return 0;
			}
		}
		m_client_list[client_h]->m_attack_last_action_time = client_time;
		m_client_list[client_h]->m_attack_msg_recv_count = 0;
	}

	if ((target_object_id != 0) && (type != 2)) {
		if (target_object_id < MaxClients) {
			if (m_client_list[target_object_id] != 0) {
				tdX = m_client_list[target_object_id]->m_x;
				tdY = m_client_list[target_object_id]->m_y;
			}
		}
		else if (hb::shared::object_id::IsNpcID(target_object_id) && (hb::shared::object_id::ToNpcIndex(target_object_id) < MaxNpcs)) {
			if (m_npc_list[hb::shared::object_id::ToNpcIndex(target_object_id)] != 0) {
				tdX = m_npc_list[hb::shared::object_id::ToNpcIndex(target_object_id)]->m_x;
				tdY = m_npc_list[hb::shared::object_id::ToNpcIndex(target_object_id)]->m_y;
			}
		}

		m_map_list[m_client_list[client_h]->m_map_index]->get_owner(&owner, &owner_type, dX, dY);
		if ((owner == hb::shared::object_id::ToNpcIndex(target_object_id)) && (m_npc_list[owner] != 0)) {
			tdX = m_npc_list[owner]->m_x;
			dX = tdX;
			tdY = m_npc_list[owner]->m_y;
			dY = tdY;
			near_attack = false;
			var_AC = true;
		}
		if (var_AC != true) {
			if ((tdX == dX) && (tdY == dY)) {
				near_attack = false;
			}
			else if ((abs(tdX - dX) <= 1) && (abs(tdY - dY) <= 1)) {
				dX = tdX;
				dY = tdY;
				near_attack = true;
			}
		}
	}

	if ((dX < 0) || (dX >= m_map_list[m_client_list[client_h]->m_map_index]->m_size_x) ||
		(dY < 0) || (dY >= m_map_list[m_client_list[client_h]->m_map_index]->m_size_y)) return 0;

	if ((sX != m_client_list[client_h]->m_x) || (sY != m_client_list[client_h]->m_y)) return 2;

	if (m_map_list[m_client_list[client_h]->m_map_index] != 0) {
		st_x = m_client_list[client_h]->m_x / 20;
		st_y = m_client_list[client_h]->m_y / 20;
		m_map_list[m_client_list[client_h]->m_map_index]->m_temp_sector_info[st_x][st_y].player_activity++;

		switch (m_client_list[client_h]->m_side) {
		case 0: m_map_list[m_client_list[client_h]->m_map_index]->m_temp_sector_info[st_x][st_y].neutral_activity++; break;
		case 1: m_map_list[m_client_list[client_h]->m_map_index]->m_temp_sector_info[st_x][st_y].aresden_activity++; break;
		case 2: m_map_list[m_client_list[client_h]->m_map_index]->m_temp_sector_info[st_x][st_y].elvine_activity++;  break;
		}
	}

	abs_x = abs(sX - dX);
	abs_y = abs(sY - dY);
	if ((type != 2) && (type < 20)) {
		if (var_AC == false) {
			item_index = m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::TwoHand)];
			if (item_index != -1) {
				if (m_client_list[client_h]->m_item_list[item_index] == 0) return 0;
				if (m_client_list[client_h]->m_item_list[item_index]->m_id_num == 845) {
					if ((abs_x > 4) || (abs_y > 4)) type = 0;
				}
				else {
					if ((abs_x > 1) || (abs_y > 1)) type = 0;
				}
			}
			else {
				if ((abs_x > 1) || (abs_y > 1)) type = 0;
			}
		}
		else {
			dir = CMisc::get_next_move_dir(sX, sY, dX, dY);
			if ((m_map_list[m_client_list[client_h]->m_map_index]->check_fly_space_available(
				sX, static_cast<char>(sY), dir, owner)))
				type = 0;
		}
	}
	else if (type >= 20) {
		short max_range;
		switch (type) {
		case 30: max_range = 5; break; // StormBlade critical
		case 22: max_range = 4; break; // Esterk
		case 23: max_range = 3; break; // Long Sword
		case 24: max_range = 2; break; // Axe
		case 26: max_range = 2; break; // Hammer
		case 27: max_range = 2; break; // Wand
		case 25: max_range = 0; break; // Bow - ranged, no melee limit
		default: max_range = 1; break; // Boxing (20), Dagger/SS (21)
		}
		if ((max_range > 0) && ((abs_x > max_range) || (abs_y > max_range))) type = 0;
	}

	m_skill_manager->clear_skill_using_status(client_h);
	m_map_list[m_client_list[client_h]->m_map_index]->clear_owner(0, client_h, hb::shared::owner_class::Player, sX, sY);
	m_map_list[m_client_list[client_h]->m_map_index]->set_owner(client_h, hb::shared::owner_class::Player, sX, sY);

	m_client_list[client_h]->m_dir = dir;

	exp = 0;
	m_map_list[m_client_list[client_h]->m_map_index]->get_owner(&owner, &owner_type, dX, dY);

	if (owner != 0) {
		if ((type != 0) && ((time - m_client_list[client_h]->m_recent_attack_time) > 100)) {
			if ((m_client_list[client_h]->m_is_processing_allowed == false) && (m_client_list[client_h]->m_is_inside_warehouse == false)
				&& (m_client_list[client_h]->m_is_inside_wizard_tower == false) && (m_client_list[client_h]->m_is_inside_own_town == false)) {

				uint32_t type1 = 0, type2, value1, value2;
				if (m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::RightHand)] != -1) {
					item_index = m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::RightHand)];
				}
				else if (m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::TwoHand)] != -1) {
					item_index = m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::TwoHand)];
				}
				else item_index = -1;

				if (item_index != -1 && m_client_list[client_h]->m_item_list[item_index] != 0) {
					if (m_client_list[client_h]->m_item_list[item_index]->m_instance.prefix_type != static_cast<uint8_t>(hb::shared::item::AttributePrefixType::None)) {
						type1 = static_cast<int>(m_client_list[client_h]->m_item_list[item_index]->m_instance.prefix_type);
						value1 = m_client_list[client_h]->m_item_list[item_index]->m_instance.prefix_value;
						type2 = static_cast<int>(m_client_list[client_h]->m_item_list[item_index]->m_instance.secondary_type);
						value2 = m_client_list[client_h]->m_item_list[item_index]->m_instance.secondary_value;
					}

					if (type1 == 2) {
						// Centuu - fix for poison
						switch (owner_type) {
						case hb::shared::owner_class::Player:
							if (!m_client_list[owner]->m_is_poisoned && !m_combat_manager->check_resisting_poison_success(owner, owner_type))
							{
								m_client_list[owner]->m_is_poisoned = true;
								m_client_list[owner]->m_poison_level = value1 * m_prefix_multiplier[2];
								m_client_list[owner]->m_poison_time = time;
								m_status_effect_manager->set_poison_flag(owner, owner_type, true);
								send_notify_msg(0, owner, Notify::MagicEffectOn, hb::shared::magic::Poison, m_client_list[owner]->m_poison_level, 0, 0);
							}
							break;
						case hb::shared::owner_class::Npc:
							break;
						}
					}
				}

				item_index = m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::TwoHand)];
				if (item_index != -1 && m_client_list[client_h]->m_item_list[item_index] != 0) {
					if (m_client_list[client_h]->m_item_list[item_index]->m_id_num == 874) { // Directional bow
						for(int i = 2; i < 10; i++) {
							err = 0;
							CMisc::GetPoint2(sX, sY, dX, dY, &tX, &tY, &err, i);
							m_map_list[m_client_list[client_h]->m_map_index]->get_owner(&owner, &owner_type, tX, tY);
							exp += m_combat_manager->calculate_attack_effect(owner, owner_type, client_h, hb::shared::owner_class::Player, tX, tY, type, near_attack, is_dash, true);
							if ((abs(tdX - dX) <= 1) && (abs(tdY - dY) <= 1)) {
								m_map_list[m_client_list[client_h]->m_map_index]->get_owner(&owner, &owner_type, dX, dY);
								exp += m_combat_manager->calculate_attack_effect(owner, owner_type, client_h, hb::shared::owner_class::Player, dX, dY, type, near_attack, is_dash, false);
							}
						}
					}
					else if (m_client_list[client_h]->m_item_list[item_index]->m_id_num == 873) { // Firebow
						if (m_client_list[client_h]->m_appearance.is_walking) {
							if (m_heldenian_initiated != 1) {
								m_dynamic_object_manager->add_dynamic_object_list(client_h, hb::shared::owner_class::PlayerIndirect, dynamic_object::Fire3, m_client_list[client_h]->m_map_index, dX, dY, (dice(1, 7) + 3) * 1000, 8);
							}
							exp += m_combat_manager->calculate_attack_effect(owner, owner_type, client_h, hb::shared::owner_class::Player, dX, dY, type, near_attack, is_dash, false);
						}
					}
					else {
						exp += m_combat_manager->calculate_attack_effect(owner, owner_type, client_h, hb::shared::owner_class::Player, dX, dY, type, near_attack, is_dash, false);
					}
				}
				else {
					exp += m_combat_manager->calculate_attack_effect(owner, owner_type, client_h, hb::shared::owner_class::Player, dX, dY, type, near_attack, is_dash, false);
				}
			}
			else {
				exp += m_combat_manager->calculate_attack_effect(owner, owner_type, client_h, hb::shared::owner_class::Player, dX, dY, type, near_attack, is_dash, false);
			}
			if (m_client_list[client_h] == 0) return 0;
			m_client_list[client_h]->m_recent_attack_time = time;
		}
	}
	else m_mining_manager->check_mining_action(client_h, dX, dY);

	if (exp != 0) {
		get_exp(client_h, exp, true);
	}

	if (response) {
		hb::net::PacketResponseMotionHeader pkt{};
		pkt.header.msg_id = MsgId::ResponseMotion;
		pkt.header.msg_type = Confirm::MotionAttackConfirm;
		ret = m_client_list[client_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
		switch (ret) {
		case sock::Event::QueueFull:
		case sock::Event::SocketError:
		case sock::Event::CriticalError:
		case sock::Event::SocketClosed:
			delete_client(client_h, true, true);
			return 0;
		}
	}

	return 1;
}

direction CGame::get_next_move_dir(short sX, short sY, short dstX, short dstY, char map_index, char turn, int* error_acc)
{
	direction dir, tmp_dir;
	int   aX, aY, dX, dY;
	int   res_x, res_y;

	if ((sX == dstX) && (sY == dstY)) return direction{};

	dX = sX;
	dY = sY;

	if ((abs(dX - dstX) <= 1) && (abs(dY - dstY) <= 1)) {
		res_x = dstX;
		res_y = dstY;
	}
	else CMisc::GetPoint(dX, dY, dstX, dstY, &res_x, &res_y, error_acc);

	dir = CMisc::get_next_move_dir(dX, dY, res_x, res_y);

	if (turn == 0)
		for(int i = dir; i <= dir + 7; i++) {
			tmp_dir = static_cast<direction>(i);
			if (tmp_dir > 8) tmp_dir = static_cast<direction>(tmp_dir - 8);
			aX = _tmp_cTmpDirX[tmp_dir];
			aY = _tmp_cTmpDirY[tmp_dir];
			if (m_map_list[map_index]->get_moveable(dX + aX, dY + aY)) return tmp_dir;
		}

	if (turn == 1)
		for(int i = dir; i >= dir - 7; i--) {
			tmp_dir = static_cast<direction>(i);
			if (tmp_dir < 1) tmp_dir = static_cast<direction>(tmp_dir + 8);
			aX = _tmp_cTmpDirX[tmp_dir];
			aY = _tmp_cTmpDirY[tmp_dir];
			if (m_map_list[map_index]->get_moveable(dX + aX, dY + aY)) return tmp_dir;
		}

	return direction{};
}

char _tmp_cEmptyPosX[] = { 0, 1, 1, 0, -1, -1, -1, 0 ,1, 2, 2, 2, 2, 1, 0, -1, -2, -2, -2, -2, -2, -1, 0, 1, 2 };
char _tmp_cEmptyPosY[] = { 0, 0, 1, 1, 1, 0, -1, -1, -1, -1, 0, 1, 2, 2, 2, 2, 2, 1, 0, -1, -2, -2, -2, -2, -2 };

bool CGame::get_empty_position(short* pX, short* pY, char map_index)
{
	
	short sX, sY;

	for(int i = 0; i < 25; i++)
		if ((m_map_list[map_index]->get_moveable(*pX + _tmp_cEmptyPosX[i], *pY + _tmp_cEmptyPosY[i])) &&
			(m_map_list[map_index]->get_is_teleport(*pX + _tmp_cEmptyPosX[i], *pY + _tmp_cEmptyPosY[i]) == false)) {
			sX = *pX + _tmp_cEmptyPosX[i];
			sY = *pY + _tmp_cEmptyPosY[i];
			*pX = sX;
			*pY = sY;
			return true;
		}

	get_map_initial_point(map_index, &sX, &sY);
	*pX = sX;
	*pY = sY;

	return false;
}

void CGame::msg_process()
{
	char* data, cFrom, key;
	size_t    msg_size;
	int      client_h;
	uint32_t time = GameClock::GetTimeMS();

	std::memset(m_msg_buffer, 0, hb::shared::limits::MsgBufferSize + 1);
	data = (char*)m_msg_buffer;

	m_cur_msgs = 0;
	while (get_msg_queue(&cFrom, data, &msg_size, &client_h, &key)) {

		//v1.31
		m_cur_msgs++;
		if (m_cur_msgs > m_max_msgs) m_max_msgs = m_cur_msgs;

		switch (cFrom) {
		case Source::Client: {
			if (m_client_list[client_h] == nullptr) break;

			// Update activity tracking (async reads bypass on_client_socket_event)
			m_client_list[client_h]->m_time = time;

			const auto* header = hb::net::PacketCast<hb::net::PacketHeader>(
				data, sizeof(hb::net::PacketHeader));
			if (!header) break;

			m_client_list[client_h]->m_last_msg_id = header->msg_id;
			m_client_list[client_h]->m_last_msg_time = time;
			m_client_list[client_h]->m_last_msg_size = msg_size;

			// Update AFK activity timer for all messages except keepalive and chat
			// (chat is handled separately in chat_msg_handler to exclude whispers)
			if (header->msg_id != MsgId::CommandCheckConnection && header->msg_id != MsgId::CommandChatMsg) {
				m_client_list[client_h]->m_afk_activity_time = time;
			}

			switch (header->msg_id) {

			case MsgId::RequestAngel: // Angels by Snoopy...
				get_angel_handler(client_h, data, msg_size);
				break;

			case MsgId::RequestResurrectYes:
				request_resurrect_player(client_h, true);
				break;

			case MsgId::RequestResurrectNo:
				request_resurrect_player(client_h, false);
				break;

			case MsgId::RequestSellItemList:
				m_item_manager->request_sell_item_list_handler(client_h, data);
				break;

			case MsgId::RequestRestart:
				request_restart_handler(client_h);
				break;

			case MsgId::RequestPanning:
				request_panning_map_data_request(client_h, data);
				break;

			case MsgId::RequestNoticement:
				//request_noticement_handler(client_h, data);
				break;

			case MsgId::RequestSetItemPos:
				m_item_manager->set_item_pos(client_h, data);
				break;

			case MsgId::request_full_object_data:
				request_full_object_data(client_h, data);
				break;

			case MsgId::RequestRetrieveItem:
				m_item_manager->request_retrieve_item_handler(client_h, data);
				break;

			case MsgId::RequestCivilRight:
				request_civil_right_handler(client_h, data);
				break;

			case MsgId::RequestTeleport:
				request_teleport_handler(client_h, data);
				break;

			case MsgId::RequestTeleportAuth:
				request_teleport_auth_handler(client_h, data);
				break;

			case MsgId::RequestInitPlayer:
				request_init_player_handler(client_h, data, key);
				break;

			case MsgId::RequestInitData:
				if (m_client_list[client_h] == nullptr) break;
				if (m_client_list[client_h]->m_is_client_connected) {
					hb::logger::error("Client '{}' connection closed, sniffer suspected", m_client_list[client_h]->m_char_name);
					m_delay_event_manager->remove_from_delay_event_list(client_h, hb::shared::owner_class::Player, 0);
					g_login->local_save_player_data(client_h); //send_msg_to_ls(ServerMsgId::RequestSavePlayerDataLogout, client_h, false);
					if ((time - m_game_time_2) > 3000) { // 3 segs
						m_client_list[client_h]->m_is_client_connected = false;
						delete_client(client_h, true, true, true, true);
					}
					break;
				}
				else {
					m_client_list[client_h]->m_is_client_connected = true;
					request_init_data_handler(client_h, data, key, msg_size);
				}
				break;

			case MsgId::CommandCommon:
				client_common_handler(client_h, data);
				break;

			case MsgId::CommandMotion:
				client_motion_handler(client_h, data);
				break;

			case MsgId::CommandCheckConnection:
				// Ping response already sent from I/O thread for accurate latency.
				// Still run handler for speedhack detection (skip duplicate send).
				check_connection_handler(client_h, data, true);
				break;

			case MsgId::CommandChatMsg:
				chat_msg_handler(client_h, data, msg_size);
				break;

			case MsgId::LevelUpSettings:
				level_up_settings_handler(client_h, data, msg_size);
				break;

			case MsgId::StateChangePoint:
				state_change_handler(client_h, data, msg_size);
				break;

			case ServerMsgId::request_heldenian_teleport:
				m_war_manager->request_heldenian_teleport(client_h, data, msg_size);
				break;

			case ServerMsgId::RequestCityHallTeleport:
				if (memcmp(m_client_list[client_h]->m_location, "aresden", 7) == 0) {
					request_teleport_handler(client_h, "2   ", "dglv2", 263, 258);
				}
				else if (memcmp(m_client_list[client_h]->m_location, "elvine", 6) == 0) {
					request_teleport_handler(client_h, "2   ", "dglv2", 209, 258);
				}
				break;

			case MSGID_REQUEST_SHOP_CONTENTS:
				request_shop_contents_handler(client_h, data);
				break;

			case MsgId::RequestConfigData:
			{
				const auto* reqPkt = hb::net::PacketCast<hb::net::PacketRequestConfigData>(
					data, sizeof(hb::net::PacketRequestConfigData));
				if (!reqPkt) break;
				if (time - m_client_list[client_h]->m_last_config_request_time < 30000) break;
				m_client_list[client_h]->m_last_config_request_time = time;
				if (reqPkt->requestItems)   m_item_manager->send_client_item_configs(client_h);
				if (reqPkt->requestMagic)   m_magic_manager->send_client_magic_configs(client_h);
				if (reqPkt->requestSkills)  m_skill_manager->send_client_skill_configs(client_h);
				if (reqPkt->requestNpcs)    send_client_npc_configs(client_h);
				if (reqPkt->requestMaps)    send_client_map_configs(client_h);
				if (reqPkt->requestBalance) send_client_balance_config(client_h);
				if (reqPkt->requestColorPalette) send_client_color_palette(client_h);
				if (reqPkt->requestAttributeTypes) send_client_attribute_types(client_h);
			}
			break;

			default:
				if (m_client_list[client_h] != 0)  // Snoopy: Anti-crash check !
				{
					std::snprintf(G_cTxt, sizeof(G_cTxt), "Unknown message received: (0x%.8X) PC(%s) - (Delayed). \tIP(%s)"
						, header->msg_id
						, m_client_list[client_h]->m_char_name
						, m_client_list[client_h]->m_ip_address);
					//DelayedDeleteClient(client_h, true, true, true, true);
				}
				else
				{
				}
				hb::logger::warn<log_channel::security>("Unknown message: 0x{:X} (no player)", header->msg_id);
				hb::logger::warn<log_channel::security>("{}", m_msg_buffer);
				break;
			}
			break;
		}

		case Source::LogServer: {
			const auto* header = hb::net::PacketCast<hb::net::PacketHeader>(
				data, sizeof(hb::net::PacketHeader));
			if (!header) break;

			switch (header->msg_id) {
			case MsgId::RequestCreateNewAccount:
				g_login->create_new_account(client_h, data);
				break;
			case MsgId::request_login:
				g_login->request_login(client_h, data);
				break;
			case MsgId::RequestCreateNewCharacter: //message from client
				g_login->response_character(client_h, data);
				break;
			case MsgId::RequestDeleteCharacter:
				g_login->delete_character(client_h, data);
				break;
			case MsgId::RequestChangePassword:
				g_login->change_password(client_h, data);
				break;
			case MsgId::request_enter_game:
				g_login->request_enter_game(client_h, data);
				break;
			default:
				hb::logger::log("Unknown login message: 0x{:X}, disconnecting client", header->msg_id);
				break;
			}
			delete_login_client(client_h);
		}
								  break;
		}

	}
}

bool CGame::put_msg_queue(char cFrom, char* data, size_t msg_size, int index, char key)
{
	return m_msgQueue.push(cFrom, data, msg_size, index, key);
}

bool CGame::get_msg_queue(char* pFrom, char* data, size_t* msg_size, int* index, char* key)
{
	return m_msgQueue.pop(pFrom, data, msg_size, index, key);
}

void CGame::client_common_handler(int client_h, char* data)
{
	uint16_t command;
	short sX, sY;
	int v1, v2, v3, v4;
	direction dir;
	const char* string;

	if (m_client_list[client_h] == 0) return;
	if (m_client_list[client_h]->m_is_init_complete == false) return;
	if (m_client_list[client_h]->m_is_killed) return;

	const auto* req = hb::net::PacketCast<hb::net::PacketCommandCommonWithString>(
		data, sizeof(hb::net::PacketCommandCommonWithString));
	if (!req) return;
	command = req->base.header.msg_type;
	sX = req->base.x;
	sY = req->base.y;
	dir = static_cast<direction>(req->base.dir);
	v1 = req->v1;
	v2 = req->v2;
	v3 = req->v3;
	string = req->text;
	v4 = req->v4;
	switch (command) {

		//50Cent - Repair All
	case CommonType::ReqRepairAll:
		m_item_manager->request_repair_all_items_handler(client_h);
		break;
	case CommonType::ReqRepairAllDelete:
		m_item_manager->request_repair_all_items_delete_handler(client_h, v1);
		break;
	case CommonType::ReqRepairAllConfirm:
		m_item_manager->request_repair_all_items_confirm_handler(client_h);
		break;

		// Crafting
	case CommonType::CraftItem:
		m_crafting_manager->req_create_crafting_handler(client_h, data);
		break;

		// New 15/05/2004
	case CommonType::ReqCreateSlate:
		m_item_manager->req_create_slate_handler(client_h, data);
		break;

		// 2.06 - by KLKS
	case CommonType::RequestHuntMode:
		//DbgWnd->AddEventMsg("RECV -> Source::Client -> MsgId::CommandCommon -> MsgId::RequestCivilRight");
		request_change_play_mode(client_h);
		break;

	case CommonType::SummonWarUnit:
		//DbgWnd->AddEventMsg("RECV -> Source::Client -> MsgId::CommandCommon -> CommonType::SummonWarUnit");
		m_war_manager->request_summon_war_unit_handler(client_h, sX, sY, v1, v2, v3);
		break;

	case CommonType::RequestHelp:
		//DbgWnd->AddEventMsg("RECV -> Source::Client -> MsgId::CommandCommon -> CommonType::RequestHelp");
		request_help_handler(client_h);
		break;

	case CommonType::RequestMapStatus:
		//DbgWnd->AddEventMsg("RECV -> Source::Client -> MsgId::CommandCommon -> CommonType::RequestMapStatus");
		m_war_manager->map_status_handler(client_h, v1, string);
		break;

	case CommonType::RequestSelectCrusadeDuty:
		//DbgWnd->AddEventMsg("RECV -> Source::Client -> MsgId::CommandCommon -> CommonType::RequestSelectCrusadeDuty");
		m_war_manager->select_crusade_duty_handler(client_h, v1);
		break;

	case CommonType::RequestCancelQuest:
		//DbgWnd->AddEventMsg("RECV -> Source::Client -> MsgId::CommandCommon -> CommonType::RequestCancelQuest");
		m_quest_manager->cancel_quest_handler(client_h);
		break;

	case CommonType::RequestActivateSpecAbility:
		//DbgWnd->AddEventMsg("RECV -> Source::Client -> MsgId::CommandCommon -> CommonType::RequestActivateSpecAbility");
		activate_special_ability_handler(client_h);
		break;

	case CommonType::RequestJoinParty:
		//DbgWnd->AddEventMsg("RECV -> Source::Client -> MsgId::CommandCommon -> CommonType::RequestJoinParty");
		join_party_handler(client_h, v1, string);
		break;

	case CommonType::GetMagicAbility:
		//DbgWnd->AddEventMsg("RECV -> Source::Client -> MsgId::CommandCommon -> CommonType::GetMagicAbility");
		m_magic_manager->get_magic_ability_handler(client_h);
		break;

	case CommonType::BuildItem:
		//DbgWnd->AddEventMsg("RECV -> Source::Client -> MsgId::CommandCommon -> CommonType::BuildItem");
		m_item_manager->build_item_handler(client_h, data);
		break;

	case CommonType::QuestAccepted:
		//DbgWnd->AddEventMsg("RECV -> Source::Client -> MsgId::CommandCommon -> CommonType::QuestAccepted");
		m_quest_manager->quest_accepted_handler(client_h);
		break;

	case CommonType::cancel_exchange_item:
		//DbgWnd->AddEventMsg("RECV -> Source::Client -> MsgId::CommandCommon -> CommonType::cancel_exchange_item");
		m_item_manager->cancel_exchange_item(client_h);
		break;

	case CommonType::confirm_exchange_item:
		//DbgWnd->AddEventMsg("RECV -> Source::Client -> MsgId::CommandCommon -> CommonType::confirm_exchange_item");
		m_item_manager->confirm_exchange_item(client_h);
		break;

	case CommonType::set_exchange_item:
		//DbgWnd->AddEventMsg("RECV -> Source::Client -> MsgId::CommandCommon -> CommonType::set_exchange_item");
		m_item_manager->set_exchange_item(client_h, v1, v2);
		break;

	case CommonType::ReqGetHeroMantle:
		//DbgWnd->AddEventMsg("RECV -> Source::Client -> MsgId::CommandCommon -> CommonType::ReqGetHeroMantle");
		m_item_manager->get_hero_mantle_handler(client_h, v1, string);
		break;

	case CommonType::ReqGetOccupyFlag:
		//DbgWnd->AddEventMsg("RECV -> Source::Client -> MsgId::CommandCommon -> CommonType::ReqGetOccupyFlag");
		m_war_manager->get_occupy_flag_handler(client_h);
		break;

	case CommonType::ReqSetDownSkillIndex:
		//DbgWnd->AddEventMsg("RECV -> Source::Client -> MsgId::CommandCommon -> CommonType::ReqSetDownSkillIndex");
		m_skill_manager->set_down_skill_index_handler(client_h, v1);
		break;

	case CommonType::TalkToNpc:
		//DbgWnd->AddEventMsg("RECV -> Source::Client -> MsgId::CommandCommon -> CommonType::TalkToNpc");
		// works for client, but for npc it returns middleland
		// if ((m_map_list[m_npc_list[v1]->m_map_index]->m_name) != (m_map_list[m_client_list[client_h]->m_map_index]->m_name)) break;
		m_quest_manager->npc_talk_handler(client_h, v1);
		break;

	case CommonType::ReqCreatePortion:
		//DbgWnd->AddEventMsg("RECV -> Source::Client -> MsgId::CommandCommon -> CommonType::ReqCreatePortion");
		m_crafting_manager->req_create_portion_handler(client_h, data);
		break;

	case CommonType::ReqGetFishThisTime:
		//DbgWnd->AddEventMsg("RECV -> Source::Client -> MsgId::CommandCommon -> CommonType::ReqGetFishThisTime");
		m_fishing_manager->req_get_fish_this_time_handler(client_h);
		break;

	case CommonType::ReqRepairItemConfirm:
		//DbgWnd->AddEventMsg("RECV -> Source::Client -> MsgId::CommandCommon -> CommonType::ReqRepairItemConfirm");
		m_item_manager->req_repair_item_cofirm_handler(client_h, v1, string);
		break;

	case CommonType::ReqRepairItem:
		//DbgWnd->AddEventMsg("RECV -> Source::Client -> MsgId::CommandCommon -> CommonType::ReqRepairItem");
		m_item_manager->req_repair_item_handler(client_h, v1, v2, string);
		break;

	case CommonType::ReqSellItemConfirm:
		//DbgWnd->AddEventMsg("RECV -> Source::Client -> MsgId::CommandCommon -> CommonType::ReqSellItemConfirm");
		m_item_manager->req_sell_item_confirm_handler(client_h, v1, v2, string);
		break;

	case CommonType::ReqSellItem:
		//DbgWnd->AddEventMsg("RECV -> Source::Client -> MsgId::CommandCommon -> CommonType::ReqSellItem");
		m_item_manager->req_sell_item_handler(client_h, v1, v2, v3, string);
		break;

	case CommonType::ReqUseSkill:
		//DbgWnd->AddEventMsg("RECV -> Source::Client -> MsgId::CommandCommon -> CommonType::ReqUseSkill");
		m_skill_manager->use_skill_handler(client_h, v1, v2, v3);
		break;

	case CommonType::ReqUseItem:
		//DbgWnd->AddEventMsg("RECV -> Source::Client -> MsgId::CommandCommon -> CommonType::ReqUseItem");
		m_item_manager->use_item_handler(client_h, v1, v2, v3, v4);
		break;

	case CommonType::ReqGetRewardMoney:
		//DbgWnd->AddEventMsg("RECV -> Source::Client -> MsgId::CommandCommon -> CommonType::ReqGetRewardMoney");
		m_loot_manager->get_reward_money_handler(client_h);
		break;

	case CommonType::ItemDrop:
		//DbgWnd->AddEventMsg("RECV -> Source::Client -> MsgId::CommandCommon -> CommonType::ItemDrop");
		m_item_manager->drop_item_handler(client_h, v1, v2, string, true);
		break;

	case CommonType::equip_item:
		//DbgWnd->AddEventMsg("RECV -> Source::Client -> MsgId::CommandCommon -> CommonType::equip_item");
		m_item_manager->equip_item_handler(client_h, v1);
		break;

	case CommonType::ReqPurchaseItem:
		//DbgWnd->AddEventMsg("RECV -> Source::Client -> MsgId::CommandCommon -> CommonType::ReqPurchaseItem");
		m_item_manager->request_purchase_item_handler(client_h, string, v1, v2);
		break;

	case CommonType::ReqStudyMagic:
		//DbgWnd->AddEventMsg("RECV -> Source::Client -> MsgId::CommandCommon -> CommonType::ReqStudyMagic");
		m_magic_manager->request_study_magic_handler(client_h, string);
		break;

	case CommonType::ReqTrainSkill:
		//DbgWnd->AddEventMsg("RECV -> Source::Client -> MsgId::CommandCommon -> CommonType::ReqTrainSkill");
		//RequestTrainSkillHandler(client_h, string);
		break;

	case CommonType::GiveItemToChar:
		//DbgWnd->AddEventMsg("RECV -> Source::Client -> MsgId::CommandCommon -> CommonType::GiveItemToChar");
		m_item_manager->give_item_handler(client_h, dir, v1, v2, v3, v4, string);
		break;

	case CommonType::ExchangeItemToChar:
		//DbgWnd->AddEventMsg("RECV -> Source::Client -> MsgId::CommandCommon -> CommonType::ExchangeItemToChar");
		m_item_manager->exchange_item_handler(client_h, dir, v1, v2, v3, v4, string);
		break;

	case CommonType::ReleaseItem:
		//DbgWnd->AddEventMsg("RECV -> Source::Client -> MsgId::CommandCommon -> CommonType::ReleaseItem");
		m_item_manager->release_item_handler(client_h, v1, true);
		break;

	case CommonType::ToggleCombatMode:
		//DbgWnd->AddEventMsg("RECV -> Source::Client -> MsgId::CommandCommon -> CommonType::ToggleCombatMode");
		toggle_combat_mode_handler(client_h);
		break;

	case CommonType::Magic:
	{
		//DbgWnd->AddEventMsg("RECV -> Source::Client -> MsgId::CommandCommon -> CommonType::Magic");
		// Parse as PacketCommandCommonWithTime to get target object ID from time_ms field
		const auto* magicReq = hb::net::PacketCast<hb::net::PacketCommandCommonWithTime>(
			data, sizeof(hb::net::PacketCommandCommonWithTime));
		uint16_t targetObjectID = magicReq ? static_cast<uint16_t>(magicReq->time_ms) : 0;
		m_magic_manager->player_magic_handler(client_h, v1, v2, (v3 - 100), false, 0, targetObjectID);
	}
	break;

	case CommonType::ToggleSafeAttackMode:
		//DbgWnd->AddEventMsg("RECV -> Source::Client -> MsgId::CommandCommon -> CommonType::ToggleSafeAttackMode");
		toggle_safe_attack_mode_handler(client_h);
		break;

		// Upgrade Item
	case CommonType::UpgradeItem:
		//DbgWnd->AddEventMsg("RECV -> Source::Client -> MsgId::CommandCommon -> CommonType::UpgradeItem");
		m_item_manager->request_item_upgrade_handler(client_h, v1);
		break;

	case CommonType::RequestAcceptJoinParty:
		//DbgWnd->AddEventMsg("RECV -> Source::Client -> MsgId::CommandCommon -> CommonType::RequestAcceptJoinParty");
		request_accept_join_party_handler(client_h, v1);
		break;

#ifdef TESTER_ONLY
	// TESTER MENU — all tester handlers
	case CommonType::TesterAction:
	{
		// No permission check — tester menu is available to all players

		int action_id = v1;
		switch (action_id)
		{
		case 0: // Reset stats
		{
			auto* p = m_client_list[client_h];

			// Reset stats
			p->m_str = m_base_stat_value;
			p->m_int = m_base_stat_value;
			p->m_vit = m_base_stat_value;
			p->m_dex = m_base_stat_value;
			p->m_mag = m_base_stat_value;
			p->m_charisma = m_base_stat_value;
			p->m_levelup_pool = (p->m_level - 1) * m_levelup_stat_gain
				- (m_base_stat_value * 6 - m_base_stat_total);
			if (p->m_levelup_pool < 0) p->m_levelup_pool = 0;

			send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0,
				"Stats have been reset. Some spells may require higher Intelligence to cast.");
			send_notify_msg(0, client_h, Notify::ForceMasteryRefresh, 0, 0, 0, 0);
			calc_total_weight(client_h);

			// Unequip items the player no longer meets requirements for
			m_item_manager->validate_equipped_items(client_h);
			// Clamp HP/MP/SP to new maximums
			p->m_hp = get_max_hp(client_h);
			p->m_mp = get_max_mp(client_h);
			p->m_sp = get_max_sp(client_h);
			send_notify_msg(0, client_h, Notify::Sp, 0, 0, 0, 0);
			// LevelUp refreshes all stats on client, Exp refreshes XP bar
			send_notify_msg(0, client_h, Notify::LevelUp, 0, 0, 0, 0);
			send_notify_msg(0, client_h, Notify::Exp, 0, 0, 0, 0);
			send_notify_msg(0, client_h, Notify::LevelUpPoints, 0, 0, 0, 0);
			hb::logger::log<log_channel::commands>("[TesterMenu] '{}' reset stats (lu_pool={})",
				p->m_char_name, p->m_levelup_pool);
			break;
		}
		case 1: // Add 100 contribution
		{
			m_client_list[client_h]->m_contribution += 100;
			send_notify_msg(0, client_h, Notify::Contribution,
				m_client_list[client_h]->m_contribution, 0, 0, 0);
			hb::logger::log<log_channel::commands>("[TesterMenu] '{}' added 100 contribution (now {})",
				m_client_list[client_h]->m_char_name, m_client_list[client_h]->m_contribution);
			break;
		}
		case 2: // Add 100 majestics
		{
			m_client_list[client_h]->m_gizon_item_upgrade_left += 100;
			send_notify_msg(0, client_h, Notify::GizonItemUpgradeLeft,
				m_client_list[client_h]->m_gizon_item_upgrade_left, 0, 0, 0);
			hb::logger::log<log_channel::commands>("[TesterMenu] '{}' added 100 majestics (now {})",
				m_client_list[client_h]->m_char_name, m_client_list[client_h]->m_gizon_item_upgrade_left);
			break;
		}
		case 3: // Add 100 eks
		{
			m_client_list[client_h]->m_enemy_kill_count += 100;
			send_notify_msg(0, client_h, Notify::EnemyKills,
				m_client_list[client_h]->m_enemy_kill_count, 0, 0, 0);
			hb::logger::log<log_channel::commands>("[TesterMenu] '{}' added 100 eks (now {})",
				m_client_list[client_h]->m_char_name, m_client_list[client_h]->m_enemy_kill_count);
			break;
		}
		case 4: // Add 1m gold
		{
			CItem* gold_item = new CItem();
			if (m_item_manager->init_item_attr(gold_item, hb::shared::item::ItemId::Gold))
			{
				gold_item->m_instance.count = 1000000;
				int erase_req = 0;
				if (m_item_manager->add_client_item_list(client_h, gold_item, &erase_req))
				{
					m_item_manager->send_item_notify_msg(client_h, Notify::ItemObtained, gold_item, 0);
				}
				else
				{
					delete gold_item;
					send_notify_msg(0, client_h, Notify::CannotCarryMoreItem, 0, 0, 0, 0);
				}
			}
			else
			{
				delete gold_item;
			}
			hb::logger::log<log_channel::commands>("[TesterMenu] '{}' added 1m gold",
				m_client_list[client_h]->m_char_name);
			break;
		}
		case 5: // Add 100 crits
		{
			m_client_list[client_h]->m_super_attack_left += 100;
			send_notify_msg(0, client_h, Notify::SuperAttackLeft, 0, 0, 0, 0);
			hb::logger::log<log_channel::commands>("[TesterMenu] '{}' added 100 crits (now {})",
				m_client_list[client_h]->m_char_name, m_client_list[client_h]->m_super_attack_left);
			break;
		}
		case 6: // Max all skills (only valid skill slots)
		{
			for (int i = 0; i < hb::shared::limits::MaxSkillType; i++)
			{
				if (m_skill_config_list[i] != 0)
					m_client_list[client_h]->m_skill_mastery[i] = 100;
				else
					m_client_list[client_h]->m_skill_mastery[i] = 0;
				send_notify_msg(0, client_h, Notify::Skill, i, m_client_list[client_h]->m_skill_mastery[i], 0, 0);
			}
			hb::logger::log<log_channel::commands>("[TesterMenu] '{}' maxed all skills",
				m_client_list[client_h]->m_char_name);
			break;
		}
		case 7: // Set level
		{
			int target_level = std::clamp(static_cast<int>(v2), 1, m_max_level);

			// Traveller anti-hack kicks players at level >= 20 if not in a faction city.
			// Teleport them to their faction city first, or clamp if no faction.
			if (target_level >= 20)
			{
				bool is_traveller =
					(strcmp(m_client_list[client_h]->m_location, "elvine") != 0) &&
					(strcmp(m_client_list[client_h]->m_location, "elvhunter") != 0) &&
					(strcmp(m_client_list[client_h]->m_location, "arehunter") != 0) &&
					(strcmp(m_client_list[client_h]->m_location, "aresden") != 0);

				if (is_traveller)
				{
					if (m_client_list[client_h]->m_side == 1)
						gm_teleport_to(client_h, "aresden", -1, -1);
					else if (m_client_list[client_h]->m_side == 2)
						gm_teleport_to(client_h, "elvine", -1, -1);
					else
						target_level = 19;
					if (m_client_list[client_h] == nullptr) break;
				}
			}

			m_client_list[client_h]->m_level = target_level;
			m_client_list[client_h]->m_exp = m_level_exp_table[target_level];
			m_client_list[client_h]->m_next_level_exp = m_level_exp_table[target_level + 1];

			// Recalculate levelup pool: (level-1)*3 total points, minus points already spent
			int total_stats = m_client_list[client_h]->m_str + m_client_list[client_h]->m_int
				+ m_client_list[client_h]->m_vit + m_client_list[client_h]->m_dex
				+ m_client_list[client_h]->m_mag + m_client_list[client_h]->m_charisma;
			m_client_list[client_h]->m_levelup_pool = (target_level - 1) * m_levelup_stat_gain - (total_stats - m_base_stat_total);
			if (m_client_list[client_h]->m_levelup_pool < 0)
				m_client_list[client_h]->m_levelup_pool = 0;

			// Clamp HP/MP/SP — check_character_data() kicks if HP > max
			m_client_list[client_h]->m_hp = std::min(m_client_list[client_h]->m_hp, get_max_hp(client_h));
			m_client_list[client_h]->m_mp = std::min(m_client_list[client_h]->m_mp, get_max_mp(client_h));
			m_client_list[client_h]->m_sp = std::min(m_client_list[client_h]->m_sp, get_max_sp(client_h));
			send_notify_msg(0, client_h, Notify::Sp, 0, 0, 0, 0);

			// Match natural level-up notification order (check_level_up)
			send_notify_msg(0, client_h, Notify::SuperAttackLeft, 0, 0, 0, 0);
			send_notify_msg(0, client_h, Notify::LevelUp, 0, 0, 0, 0);
			send_notify_msg(0, client_h, Notify::Exp, 0, 0, 0, 0);
			send_notify_msg(0, client_h, Notify::LevelUpPoints, 0, 0, 0, 0);
			hb::logger::log<log_channel::commands>("[TesterMenu] '{}' set level to {} (lu_pool={})",
				m_client_list[client_h]->m_char_name, target_level, m_client_list[client_h]->m_levelup_pool);
			break;
		}
		case 9: // Teleport to map
		{
			if (string == nullptr || string[0] == '\0') break;

			if (!gm_teleport_to(client_h, string, -1, -1))
			{
				send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0, "Teleport failed — map not found.");
			}
			else
			{
				hb::logger::log<log_channel::commands>("[TesterMenu] '{}' teleported to '{}'",
					m_client_list[client_h]->m_char_name, string);
			}
			break;
		}
		default:
			break;
		}
		break;
	}

	case CommonType::TesterMapList:
	{
		if (m_client_list[client_h] == nullptr) break;

		hb::net::PacketNotifyTesterMapListResult result{};
		result.header.msg_id = MsgId::Notify;
		result.header.msg_type = Notify::TesterMapListResult;
		result.count = 0;

		for (int i = 0; i < hb::server::config::MaxMaps && result.count < 100; i++)
		{
			if (m_map_list[i] == nullptr) continue;
			auto& entry = result.entries[result.count];
			std::memset(entry.name, 0, sizeof(entry.name));
			std::memcpy(entry.name, m_map_list[i]->m_name, 10);
			result.count++;
		}

		m_client_list[client_h]->m_socket->send_msg(
			reinterpret_cast<char*>(&result), sizeof(result));
		hb::logger::log<log_channel::commands>("[TesterMenu] '{}' requested map list ({} maps)",
			m_client_list[client_h]->m_char_name, static_cast<int>(result.count));
		break;
	}

	case CommonType::TesterItemSearch:
	{
		if (m_client_list[client_h] == nullptr) break;

		// Empty search = return first 50 items; otherwise filter by substring
		bool has_filter = (string != nullptr && string[0] != '\0');
		char search_lower[64]{};
		if (has_filter)
		{
			std::snprintf(search_lower, sizeof(search_lower), "%s", string);
			for (int i = 0; search_lower[i]; i++)
				search_lower[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(search_lower[i])));
		}

		hb::net::PacketNotifyTesterItemSearchResult result{};
		result.header.msg_id = MsgId::Notify;
		result.header.msg_type = Notify::TesterItemSearchResult;
		result.count = 0;

		for (int i = 0; i < hb::server::config::MaxItemTypes && result.count < 50; i++)
		{
			if (m_item_config_list[i] == nullptr) continue;

			if (has_filter)
			{
				char name_lower[64]{};
				std::snprintf(name_lower, sizeof(name_lower), "%s", m_item_config_list[i]->m_name);
				for (int j = 0; name_lower[j]; j++)
					name_lower[j] = static_cast<char>(std::tolower(static_cast<unsigned char>(name_lower[j])));

				if (std::strstr(name_lower, search_lower) == nullptr)
					continue;
			}

			auto& entry = result.entries[result.count];
			entry.item_id = static_cast<int16_t>(i);
			entry.effect_type = m_item_config_list[i]->m_item_effect_type;
			std::memset(entry.name, 0, sizeof(entry.name));
			std::snprintf(entry.name, sizeof(entry.name), "%s", m_item_config_list[i]->m_name);
			result.count++;
		}

		m_client_list[client_h]->m_socket->send_msg(
			reinterpret_cast<char*>(&result), sizeof(result));
		hb::logger::log<log_channel::commands>("[TesterMenu] '{}' searched items '{}' ({} results)",
			m_client_list[client_h]->m_char_name, has_filter ? string : "(all)", static_cast<int>(result.count));
		break;
	}

	case CommonType::TesterCreateItem:
	{
		if (m_client_list[client_h] == nullptr) break;

		int item_id = static_cast<int>(v1);
		uint32_t attribute = static_cast<uint32_t>(v2);
		int count = std::clamp(static_cast<int>(v3), 1, 10);

		if (item_id < 0 || item_id >= hb::server::config::MaxItemTypes
			|| m_item_config_list[item_id] == nullptr)
		{
			send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0, "Invalid item ID.");
			break;
		}

		// Unpack legacy bitmask from tester command into individual fields
		auto prefix_type = static_cast<hb::shared::item::AttributePrefixType>((attribute >> 20) & 0x0F);
		uint8_t prefix_value = static_cast<uint8_t>((attribute >> 16) & 0x0F);
		auto secondary_type = static_cast<hb::shared::item::SecondaryEffectType>((attribute >> 12) & 0x0F);
		uint8_t secondary_value = static_cast<uint8_t>((attribute >> 8) & 0x0F);
		uint8_t enchant_bonus = static_cast<uint8_t>((attribute >> 28) & 0x0F);
		bool custom_made = (attribute & 0x00000001) != 0;

		int created = 0;
		for (int i = 0; i < count; i++)
		{
			CItem* item = new CItem();
			if (!m_item_manager->init_item_attr(item, item_id))
			{
				delete item;
				continue;
			}

			item->m_instance.custom_made = custom_made ? 1 : 0;
			item->m_instance.prefix_type = static_cast<uint8_t>(prefix_type);
			item->m_instance.prefix_value = prefix_value;
			item->m_instance.secondary_type = static_cast<uint8_t>(secondary_type);
			item->m_instance.secondary_value = secondary_value;
			item->m_instance.enchant_bonus = enchant_bonus;
			m_item_manager->adjust_rare_item_value(item);

			// Set item color based on prefix type — unified palette weapon indices (16-21)
			switch (static_cast<hb::shared::item::AttributePrefixType>(item->m_instance.prefix_type))
			{
			case hb::shared::item::AttributePrefixType::Agile:      item->m_instance.item_color = 16; break;
			case hb::shared::item::AttributePrefixType::Light:       item->m_instance.item_color = 16; break;
			case hb::shared::item::AttributePrefixType::Strong:      item->m_instance.item_color = 16; break;
			case hb::shared::item::AttributePrefixType::Poisoning:   item->m_instance.item_color = 17; break;
			case hb::shared::item::AttributePrefixType::Critical:    item->m_instance.item_color = 18; break;
			case hb::shared::item::AttributePrefixType::Special:     item->m_instance.item_color = 18; break;
			case hb::shared::item::AttributePrefixType::Sharp:       item->m_instance.item_color = 19; break;
			case hb::shared::item::AttributePrefixType::Righteous:   item->m_instance.item_color = 20; break;
			case hb::shared::item::AttributePrefixType::Ancient:     item->m_instance.item_color = 21; break;
			default: break;
			}

			int erase_req = 0;
			if (m_item_manager->add_client_item_list(client_h, item, &erase_req))
			{
				m_item_manager->send_item_notify_msg(client_h, Notify::ItemObtained, item, 0);
				created++;
			}
			else
			{
				delete item;
				send_notify_msg(0, client_h, Notify::CannotCarryMoreItem, 0, 0, 0, 0);
				break;
			}
		}

		if (created > 0)
		{
			char buf[128];
			std::snprintf(buf, sizeof(buf), "Created %dx %s (ID: %d)", created, m_item_config_list[item_id]->m_name, item_id);
			send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0, buf);
		}
		else
		{
			send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0, "Failed to create item.");
		}

		hb::logger::log<log_channel::commands>("[TesterMenu] '{}' created {}x item ID {} attr=0x{:08X}",
			m_client_list[client_h]->m_char_name, created, item_id, attribute);
		break;
	}
#endif // TESTER_ONLY

	default:
		hb::logger::log("Unknown message: 0x{:X}", command);
		break;
	}
}

// New 07/05/2004

//  int CGame::client_motion_get_item_handler(int client_h, short sX, short sY, char dir)
//  description			:: check if player is dropping item or picking up item
//  last updated		:: October 29, 2004; 7:12 PM; Hypnotoad
//	return value		:: int

void CGame::send_event_to_near_client_type_b(uint32_t msg_id, uint16_t msg_type, char map_index, short sX, short sY, short v1, short v2, short v3, short v4)
{
	int ret, short_cut_index;
	bool flag;

	// OPTIMIZATION FIX #2: Early exit if no clients online
	if (m_client_shortcut[0] == 0) return;

	// OPTIMIZATION FIX #2: Pre-check if any clients are in range before building packet
	bool has_nearby_clients = false;
	short_cut_index = 0;
	while (m_client_shortcut[short_cut_index] != 0) {
		int i = m_client_shortcut[short_cut_index];
		if ((m_client_list[i] != 0) &&
			(m_client_list[i]->m_map_index == map_index) &&
			(m_client_list[i]->m_x >= sX - hb::shared::view::CenterX) &&
			(m_client_list[i]->m_x <= sX + hb::shared::view::CenterX) &&
			(m_client_list[i]->m_y >= sY - (hb::shared::view::CenterY + 1)) &&
			(m_client_list[i]->m_y <= sY + (hb::shared::view::CenterY + 1))) {
			has_nearby_clients = true;
			break;
		}
		short_cut_index++;
	}

	// TEMPORARILY DISABLED FOR TESTING - Early exit if no clients in range
	if (false && !has_nearby_clients) return;

	hb::net::PacketEventNearTypeBShort pkt{};
	pkt.header.msg_id = msg_id;
	pkt.header.msg_type = msg_type;
	pkt.x = sX;
	pkt.y = sY;
	pkt.v1 = v1;
	pkt.v2 = v2;
	pkt.v3 = v3;
	pkt.v4 = v4;

	//for(int i = 1; i < MaxClients; i++)
	flag = true;
	short_cut_index = 0;
	while (flag) {
		// MaxClients 
		int i = m_client_shortcut[short_cut_index];
		short_cut_index++;
		if (i == 0) flag = false;

		if ((flag) && (m_client_list[i] != 0)) {
			if ((m_client_list[i]->m_map_index == map_index) &&
				(m_client_list[i]->m_x >= sX - hb::shared::view::CenterX) &&
				(m_client_list[i]->m_x <= sX + hb::shared::view::CenterX) &&
				(m_client_list[i]->m_y >= sY - (hb::shared::view::CenterY + 1)) &&
				(m_client_list[i]->m_y <= sY + (hb::shared::view::CenterY + 1))) {

				ret = m_client_list[i]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
			}
		}
	}
}

void CGame::send_event_to_near_client_type_b(uint32_t msg_id, uint16_t msg_type, char map_index, short sX, short sY, short v1, short v2, short v3, uint32_t v4)
{
	int ret, short_cut_index;
	bool flag;

	hb::net::PacketEventNearTypeBDword pkt{};
	pkt.header.msg_id = msg_id;
	pkt.header.msg_type = msg_type;
	pkt.x = sX;
	pkt.y = sY;
	pkt.v1 = v1;
	pkt.v2 = v2;
	pkt.v3 = v3;
	pkt.v4 = v4;

	//for(int i = 1; i < MaxClients; i++)
	flag = true;
	short_cut_index = 0;
	while (flag) {
		// MaxClients 
		int i = m_client_shortcut[short_cut_index];
		short_cut_index++;
		if (i == 0) flag = false;

		if ((flag) && (m_client_list[i] != 0)) {
			if ((m_client_list[i]->m_map_index == map_index) &&
				(m_client_list[i]->m_x >= sX - hb::shared::view::CenterX) &&
				(m_client_list[i]->m_x <= sX + hb::shared::view::CenterX) &&
				(m_client_list[i]->m_y >= sY - (hb::shared::view::CenterY + 1)) &&
				(m_client_list[i]->m_y <= sY + (hb::shared::view::CenterY + 1))) {

				ret = m_client_list[i]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
			}
		}
	}
}

void CGame::send_ground_item_event(uint16_t msg_type, char map_index, short sX, short sY, const CItem* item)
{
	int short_cut_index;
	bool flag;

	hb::net::PacketEventGroundItem pkt{};
	pkt.header.msg_id = MsgId::EventCommon;
	pkt.header.msg_type = msg_type;
	pkt.x = sX;
	pkt.y = sY;

	if (item != nullptr)
	{
		pkt.item_id = item->m_id_num;
		pkt.count = CItem::count_to_v2(item->m_instance.count);
		pkt.item_color = item->m_instance.item_color;
		pkt.touch_effect_type = item->m_instance.touch_effect_type;
		pkt.touch_effect_value1 = item->m_instance.touch_effect_value1;
		pkt.touch_effect_value2 = item->m_instance.touch_effect_value2;
		pkt.touch_effect_value3 = item->m_instance.touch_effect_value3;
		pkt.special_effect_value1 = item->m_instance.special_effect_value1;
		pkt.special_effect_value2 = item->m_instance.special_effect_value2;
		pkt.special_effect_value3 = item->m_instance.special_effect_value3;
		pkt.cur_durability = item->m_instance.cur_durability;
		item->copy_attributes_to(pkt);
	}

	flag = true;
	short_cut_index = 0;
	while (flag)
	{
		int i = m_client_shortcut[short_cut_index];
		short_cut_index++;
		if (i == 0) flag = false;

		if ((flag) && (m_client_list[i] != 0))
		{
			if ((m_client_list[i]->m_map_index == map_index) &&
				(m_client_list[i]->m_x >= sX - hb::shared::view::CenterX) &&
				(m_client_list[i]->m_x <= sX + hb::shared::view::CenterX) &&
				(m_client_list[i]->m_y >= sY - (hb::shared::view::CenterY + 1)) &&
				(m_client_list[i]->m_y <= sY + (hb::shared::view::CenterY + 1)))
			{
				m_client_list[i]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
			}
		}
	}
}

//  int CGame::client_motion_stop_handler(int client_h, short sX, short sY, char dir)
//  description			:: checks if player is stopped
//  last updated		:: October 29, 2004; 6:46 PM; Hypnotoad
//	return value		:: int
int CGame::client_motion_stop_handler(int client_h, short sX, short sY, direction dir)
{
	int     ret;
	short   owner_h;
	char    owner_type;

	if (m_client_list[client_h] == 0) return 0;
	if ((dir <= 0) || (dir > 8))       return 0;
	if (m_client_list[client_h]->m_is_killed) return 0;
	if (m_client_list[client_h]->m_is_init_complete == false) return 0;

	if ((sX != m_client_list[client_h]->m_x) || (sY != m_client_list[client_h]->m_y)) return 2;

	if (m_client_list[client_h]->m_skill_using_status[19]) {
		m_map_list[m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, sX, sY);
		if (owner_h != 0) {
			delete_client(client_h, true, true);
			return 0;
		}
	}

	m_skill_manager->clear_skill_using_status(client_h);

	int st_x, st_y;
	if (m_map_list[m_client_list[client_h]->m_map_index] != 0) {
		st_x = m_client_list[client_h]->m_x / 20;
		st_y = m_client_list[client_h]->m_y / 20;
		m_map_list[m_client_list[client_h]->m_map_index]->m_temp_sector_info[st_x][st_y].player_activity++;

		switch (m_client_list[client_h]->m_side) {
		case 0: m_map_list[m_client_list[client_h]->m_map_index]->m_temp_sector_info[st_x][st_y].neutral_activity++; break;
		case 1: m_map_list[m_client_list[client_h]->m_map_index]->m_temp_sector_info[st_x][st_y].aresden_activity++; break;
		case 2: m_map_list[m_client_list[client_h]->m_map_index]->m_temp_sector_info[st_x][st_y].elvine_activity++;  break;
		}
	}

	m_client_list[client_h]->m_dir = dir;

	m_map_list[m_client_list[client_h]->m_map_index]->clear_owner(0, client_h, hb::shared::owner_class::Player, sX, sY);
	m_map_list[m_client_list[client_h]->m_map_index]->set_owner(client_h, hb::shared::owner_class::Player, sX, sY);

	{
		hb::net::PacketResponseMotionHeader pkt{};
		pkt.header.msg_id = MsgId::ResponseMotion;
		pkt.header.msg_type = Confirm::MotionConfirm;
		ret = m_client_list[client_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}
	switch (ret) {
	case sock::Event::QueueFull:
	case sock::Event::SocketError:
	case sock::Event::CriticalError:
	case sock::Event::SocketClosed:
		delete_client(client_h, true, true);
		return 0;
	}

	return 1;
}

// Dedicated attribute change notify — replaces send_notify_msg for Notify::ItemAttributeChange
void CGame::send_item_attribute_change(int client_h, int item_index, CItem* item, uint32_t spec_value1, uint32_t spec_value2)
{
	if (m_client_list[client_h] == nullptr) return;

	hb::net::PacketNotifyItemAttributeChange pkt{};
	pkt.header.msg_id = MsgId::Notify;
	pkt.header.msg_type = Notify::ItemAttributeChange;
	pkt.item_index = static_cast<int16_t>(item_index);
	item->copy_attributes_to(pkt);
	pkt.spec_value1 = spec_value1;
	pkt.spec_value2 = spec_value2;
	m_client_list[client_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
}

// Dedicated gizon item change notify — replaces send_notify_msg for Notify::GizoneItemChange
void CGame::send_gizon_item_change(int client_h, int item_index, CItem* item)
{
	if (m_client_list[client_h] == nullptr) return;

	hb::net::PacketNotifyGizonItemChange pkt{};
	pkt.header.msg_id = MsgId::Notify;
	pkt.header.msg_type = Notify::GizoneItemChange;
	pkt.item_index = static_cast<int16_t>(item_index);
	pkt.item_type = item->m_item_type;
	pkt.cur_durability = item->m_instance.cur_durability;
	std::memcpy(pkt.item_name, item->m_name, sizeof(pkt.item_name));
	pkt.item_color = item->m_instance.item_color;
	pkt.spec_value2 = static_cast<uint8_t>(item->m_instance.special_effect_value2);
	item->copy_attributes_to(pkt);
	pkt.item_id = item->m_id_num;
	m_client_list[client_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
}

// Dedicated exchange item notify — replaces send_notify_msg for exchange item types
void CGame::send_exchange_item_notify(int from_h, int to_h, uint16_t msg_type, int item_index, CItem* item, int amount)
{
	if (m_client_list[to_h] == nullptr) return;

	hb::net::PacketNotifyExchangeItem pkt{};
	pkt.header.msg_id = MsgId::Notify;
	pkt.header.msg_type = msg_type;
	pkt.dir = static_cast<int16_t>(item_index);
	pkt.amount = static_cast<int32_t>(amount);
	pkt.color = item->m_instance.item_color;
	pkt.cur_durability = item->m_instance.cur_durability;
	pkt.max_durability = item->m_durability;
	pkt.performance = static_cast<int16_t>(item->m_instance.special_effect_value2 + 100);
	std::memcpy(pkt.item_name, item->m_name, sizeof(pkt.item_name));
	std::memcpy(pkt.char_name, m_client_list[from_h]->m_char_name, sizeof(pkt.char_name));
	item->copy_attributes_to(pkt);
	pkt.item_id = item->m_id_num;
	m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
}

// 05/29/2004 - Hypnotoad - Purchase Dicount updated to take charisma into consideration

void CGame::send_notify_msg(int from_h, int to_h, uint16_t msg_type, uint32_t v1, uint64_t v2, uint32_t v3, const char* string, uint32_t v4, uint32_t v5, uint32_t v6, uint32_t v7, uint32_t v8, uint32_t v9, const char* string2)
{
	int ret = 0;

	if (m_client_list[to_h] == 0) return;

	// !!! v1, v2, v3 DWORD .
	switch (msg_type) {
	case Notify::CurDurability:
	{
		hb::net::PacketNotifyCurDurability pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.item_index = static_cast<int32_t>(v1);
		pkt.cur_durability = static_cast<int32_t>(v2);
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
		break;
	}
	case Notify::HeldenianCount:
	{
		hb::net::PacketNotifyHeldenianCount pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.aresden_tower_left = static_cast<int16_t>(v1);
		pkt.elvine_tower_left = static_cast<int16_t>(v2);
		pkt.aresden_flags = static_cast<int16_t>(v3);
		pkt.elvine_flags = static_cast<int16_t>(v4);
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
		break;
	}

	case Notify::NoMoreAgriculture:
	case Notify::AgricultureSkillLimit:
	case Notify::AgricultureNoArea:
	{
		hb::net::PacketNotifyEmpty pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}
	break;

	// New 18/05/2004
	case Notify::SpawnEvent:
	{
		hb::net::PacketNotifySpawnEvent pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.monster_id = static_cast<uint8_t>(v3);
		pkt.x = static_cast<int16_t>(v1);
		pkt.y = static_cast<int16_t>(v2);
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
		break;
	}

	case Notify::QuestCounter:
	{
		hb::net::PacketNotifyQuestCounter pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.current_count = static_cast<int32_t>(v1);
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
		break;
	}

	case Notify::ApocGateClose:
	case Notify::ApocGateOpen:
	{
		hb::net::PacketNotifyApocGateOpen pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.gate_x = static_cast<int32_t>(v1);
		pkt.gate_y = static_cast<int32_t>(v2);
		if (string != 0) {
			memcpy(pkt.map_name, string, sizeof(pkt.map_name));
		}
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
		break;
	}

	case Notify::AbaddonKilled:
	{
		hb::net::PacketNotifyAbaddonKilled pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		memcpy(pkt.killer_name, m_client_list[from_h]->m_char_name, sizeof(pkt.killer_name));
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
		break;
	}

	case Notify::ApocForceRecallPlayers:
	case Notify::ApocGateStartMsg:
	case Notify::ApocGateEndMsg:
	case Notify::NoRecall:
	case Notify::TeleportApproved:
	{
		hb::net::PacketNotifyEmpty pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}
	break;

	case Notify::LevelUpPoints:
	{
		hb::net::PacketNotifyLevelUpPoints pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.lu_point = static_cast<uint16_t>(m_client_list[to_h]->m_levelup_pool);
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}
	break;

	case Notify::ForceRecallTime:
	{
		hb::net::PacketNotifyForceRecallTime pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.seconds_left = static_cast<uint16_t>(v1);
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}
	break;

	// New 16/05/2004
	//0xB4E2, 0xBEB
	case Notify::MonsterCount:
	case Notify::SlateStatus:
		if (msg_type == Notify::MonsterCount) {
			hb::net::PacketNotifyMonsterCount pkt{};
			pkt.header.msg_id = MsgId::Notify;
			pkt.header.msg_type = msg_type;
			pkt.count = static_cast<int16_t>(v1);
			ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
		}
		else {
			hb::net::PacketNotifySimpleShort pkt{};
			pkt.header.msg_id = MsgId::Notify;
			pkt.header.msg_type = msg_type;
			pkt.value = static_cast<int16_t>(v1);
			ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
		}
		break;

		//0x0BE5, 0x0BE7, 0x0BE8, 0x0BEA
	case Notify::Unknown0BE8:
	case Notify::HeldenianTeleport:
	case Notify::HeldenianEnd:
	case Notify::ResurrectPlayer:
	case Notify::SlateExp:
	case Notify::SlateMana:
	case Notify::SlateInvincible:
	{
		hb::net::PacketNotifyEmpty pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}
	break;

	case Notify::SlateCreateFail:
	{
		hb::net::PacketNotifyEmpty pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}
	break;

	case Notify::SlateCreateSuccess:
	{
		hb::net::PacketNotifySimpleInt pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.value = static_cast<int32_t>(v1);
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}
	break;

	// New 07/05/2004
	// Party Notify Msg's
	case Notify::Party:
		switch (v1) {
		case 4:
		case 6:
		{
			hb::net::PacketNotifyPartyName pkt{};
			pkt.header.msg_id = MsgId::Notify;
			pkt.header.msg_type = msg_type;
			pkt.type = static_cast<int16_t>(v1);
			pkt.v2 = static_cast<int16_t>(v2);
			pkt.v3 = static_cast<int16_t>(v3);
			if (string != 0) {
				memcpy(pkt.name, string, sizeof(pkt.name));
			}
			ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
			break;
		}
		case 5:
		{
			hb::net::PacketWriter writer;
			writer.Reserve(sizeof(hb::net::PacketNotifyPartyList) + (v3 * 11));

			auto* pkt = writer.Append<hb::net::PacketNotifyPartyList>();
			pkt->header.msg_id = MsgId::Notify;
			pkt->header.msg_type = msg_type;
			pkt->type = static_cast<int16_t>(v1);
			pkt->v2 = static_cast<int16_t>(v2);
			pkt->count = static_cast<int16_t>(v3);

			if (string != 0 && v3 > 0) {
				writer.AppendBytes(string, v3 * 11);
			}

			ret = m_client_list[to_h]->m_socket->send_msg(writer.Data(), static_cast<int>(writer.size()));
			break;
		}
		default:
		{
			hb::net::PacketNotifyPartyBasic pkt{};
			pkt.header.msg_id = MsgId::Notify;
			pkt.header.msg_type = msg_type;
			pkt.type = static_cast<int16_t>(v1);
			pkt.v2 = static_cast<int16_t>(v2);
			pkt.v3 = static_cast<int16_t>(v3);
			pkt.v4 = static_cast<int16_t>(v4);
			ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
			break;
		}
		}
		break;

	// New 06/05/2004
	// Upgrade Notify Msg's
	case Notify::ItemUpgradeFail:
	{
		hb::net::PacketNotifyItemUpgradeFail pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.reason = static_cast<int16_t>(v1);
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}
	break;

	// NOTE: Notify::ItemAttributeChange now uses send_item_attribute_change() directly
	// NOTE: Notify::GizoneItemChange now uses send_gizon_item_change() directly
	case Notify::GizonItemUpgradeLeft:
	{
		hb::net::PacketNotifyItemAttributeChange pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.item_index = static_cast<int16_t>(v1);
		pkt.spec_value1 = static_cast<uint32_t>(v3);
		pkt.spec_value2 = static_cast<uint32_t>(v4);
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}
	break;

	// 2.06 - by KLKS
	case Notify::ChangePlayMode:
	{
		hb::net::PacketNotifyChangePlayMode pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		if (string != 0) {
			memcpy(pkt.location, string, sizeof(pkt.location));
		}
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
		break;
	}

	case Notify::TcLoc:
	{
		hb::net::PacketNotifyTCLoc pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.dest_x = static_cast<int16_t>(v1);
		pkt.dest_y = static_cast<int16_t>(v2);
		if (string != 0) {
			memcpy(pkt.teleport_map, string, sizeof(pkt.teleport_map));
		}
		pkt.construct_x = static_cast<int16_t>(v4);
		pkt.construct_y = static_cast<int16_t>(v5);
		if (string2 != 0) {
			memcpy(pkt.construct_map, string2, sizeof(pkt.construct_map));
		}
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}
	break;

	//New 11/05/2004
	case Notify::grand_magic_result:
	{
		hb::net::PacketWriter writer;
		writer.Reserve(sizeof(hb::net::PacketNotifyGrandMagicResultHeader) + (v9 * 2));

		auto* pkt = writer.Append<hb::net::PacketNotifyGrandMagicResultHeader>();
		pkt->header.msg_id = MsgId::Notify;
		pkt->header.msg_type = msg_type;
		pkt->crashed_structures = static_cast<uint16_t>(v1);
		pkt->structure_damage = static_cast<uint16_t>(v2);
		pkt->casualities = static_cast<uint16_t>(v3);
		if (string != 0) {
			memcpy(pkt->map_name, string, sizeof(pkt->map_name));
		}
		pkt->active_structure = static_cast<uint16_t>(v4);
		pkt->value_count = static_cast<uint16_t>(v9);

		if (v9 > 0 && string2 != 0) {
			writer.AppendBytes(string2 + 2, v9 * 2);
		}

		ret = m_client_list[to_h]->m_socket->send_msg(writer.Data(), static_cast<int>(writer.size()));
	}
	break;

	case Notify::MapStatusNext:
	{
		hb::net::PacketWriter writer;
		writer.Reserve(sizeof(hb::net::PacketHeader) + v1);

		auto* pkt = writer.Append<hb::net::PacketHeader>();
		pkt->msg_id = MsgId::Notify;
		pkt->msg_type = msg_type;

		if (string != 0 && v1 > 0) {
			writer.AppendBytes(string, v1);
		}

		ret = m_client_list[to_h]->m_socket->send_msg(writer.Data(), static_cast<int>(writer.size()));
	}
	break;

	case Notify::MapStatusLast:
	{
		hb::net::PacketWriter writer;
		writer.Reserve(sizeof(hb::net::PacketHeader) + v1);

		auto* pkt = writer.Append<hb::net::PacketHeader>();
		pkt->msg_id = MsgId::Notify;
		pkt->msg_type = msg_type;

		if (string != 0 && v1 > 0) {
			writer.AppendBytes(string, v1);
		}

		ret = m_client_list[to_h]->m_socket->send_msg(writer.Data(), static_cast<int>(writer.size()));
	}
	break;

	case Notify::LockedMap:
	{
		hb::net::PacketNotifyLockedMap pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.seconds_left = static_cast<int16_t>(v1);
		if (string != 0) {
			memcpy(pkt.map_name, string, sizeof(pkt.map_name));
		}
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}
	break;

	case Notify::BuildItemSuccess:
	case Notify::BuildItemFail:
	{
		hb::net::PacketNotifyBuildItemResult pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		if (v1 >= 0) {
			pkt.item_id = static_cast<int16_t>(v1);
		}
		else {
			pkt.item_id = static_cast<int16_t>(v1 + 10000);
		}
		pkt.item_count = static_cast<int16_t>(v2);
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}
	break;

	case Notify::Help:
	case Notify::QuestReward:
	{
		hb::net::PacketNotifyQuestReward pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.who = static_cast<int16_t>(v1);
		pkt.flag = static_cast<int16_t>(v2);
		pkt.amount = static_cast<int32_t>(v3);
		if (string != 0) {
			memcpy(pkt.reward_name, string, sizeof(pkt.reward_name));
		}
		pkt.contribution = static_cast<int32_t>(v4);
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}
	break;

	case Notify::cannot_construct:
	{
		hb::net::PacketNotifyCannotConstruct pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.reason = static_cast<int16_t>(v1);
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
		break;
	}
	case Notify::meteor_strike_coming:
	{
		hb::net::PacketNotifyMeteorStrikeComing pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.phase = static_cast<int16_t>(v1);
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
		break;
	}
	case Notify::ObserverMode:
	{
		hb::net::PacketNotifyObserverMode pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.enabled = static_cast<int16_t>(v1);
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
		break;
	}
	case Notify::SpellInterrupted:
	{
		hb::net::PacketNotifyEmpty pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
		break;
	}
	case Notify::MeteorStrikeHit:
	case Notify::HelpFailed:
	case Notify::SpecialAbilityEnabled:
	case Notify::ForceDisconn:
	case Notify::QuestCompleted:
	case Notify::QuestAborted:
		if (msg_type == Notify::ForceDisconn) {
			hb::net::PacketNotifyForceDisconn pkt{};
			pkt.header.msg_id = MsgId::Notify;
			pkt.header.msg_type = msg_type;
			pkt.seconds = static_cast<uint16_t>(v1);
			ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
		}
		else {
			hb::net::PacketNotifySimpleShort pkt{};
			pkt.header.msg_id = MsgId::Notify;
			pkt.header.msg_type = msg_type;
			pkt.value = static_cast<int16_t>(v1);
			ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
		}
		break;

	case Notify::QuestContents:
	{
		hb::net::PacketNotifyQuestContents pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.who = static_cast<int16_t>(v1);
		pkt.quest_type = static_cast<int16_t>(v2);
		pkt.contribution = static_cast<int16_t>(v3);
		pkt.target_config_id = static_cast<int16_t>(v4);
		pkt.target_count = static_cast<int16_t>(v5);
		pkt.x = static_cast<int16_t>(v6);
		pkt.y = static_cast<int16_t>(v7);
		pkt.range = static_cast<int16_t>(v8);
		pkt.is_completed = static_cast<int16_t>(v9);
		if (string2 != 0) {
			memcpy(pkt.target_name, string2, sizeof(pkt.target_name));
		}
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
		break;
	}

	case Notify::EnergySphereCreated:
	{
		hb::net::PacketNotifyEnergySphereCreated pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.x = static_cast<int16_t>(v1);
		pkt.y = static_cast<int16_t>(v2);
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}
	break;
	case Notify::ItemColorChange:
	{
		hb::net::PacketNotifyItemColorChange pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.item_index = static_cast<int16_t>(v1);
		pkt.item_color = static_cast<int16_t>(v2);
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
		break;
	}

	case Notify::NoMoreCrusadeStructure:
	case Notify::ExchangeItemComplete:
	case Notify::cancel_exchange_item:
	{
		hb::net::PacketNotifyEmpty pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}
	break;

	// NOTE: Notify::set_exchange_item now uses send_exchange_item_notify() directly
	// NOTE: Notify::OpenExchangeWindow now uses send_exchange_item_notify() directly

	case Notify::NotFlagSpot:
	{
		hb::net::PacketNotifyEmpty pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}
	break;

	case Notify::ItemPosList:
	{
		hb::net::PacketNotifyItemPosList pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		for(int i = 0; i < hb::shared::limits::MaxItems; i++) {
			pkt.positions[i * 2] = static_cast<int16_t>(m_client_list[to_h]->m_item_pos_list[i].x);
			pkt.positions[i * 2 + 1] = static_cast<int16_t>(m_client_list[to_h]->m_item_pos_list[i].y);
		}
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}
	break;

	case Notify::EnemyKills:
	{
		hb::net::PacketNotifyEnemyKills pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.count = static_cast<int32_t>(v1);
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}
	break;

#ifdef TESTER_ONLY
	case Notify::Contribution:
	{
		hb::net::PacketNotifySimpleInt pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.value = static_cast<int32_t>(v1);
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}
	break;
#endif // TESTER_ONLY

	case Notify::Crusade:
	{
		hb::net::PacketNotifyCrusade pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.crusade_mode = static_cast<int32_t>(v1);
		pkt.crusade_duty = static_cast<int32_t>(v2);
		pkt.v3 = static_cast<int32_t>(v3);
		pkt.v4 = static_cast<int32_t>(v4);
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}
	break;

	case Notify::ConstructionPoint:
	{
		hb::net::PacketNotifyConstructionPoint pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.construction_point = static_cast<int16_t>(v1);
		pkt.war_contribution = static_cast<int16_t>(v2);
		pkt.notify_type = static_cast<int16_t>(v3);
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}
	break;

	case Notify::SpecialAbilityStatus:
	{
		hb::net::PacketNotifySpecialAbilityStatus pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.status_type = static_cast<int16_t>(v1);
		pkt.ability_type = static_cast<int16_t>(v2);
		pkt.seconds_left = static_cast<int16_t>(v3);
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}
	break;

	case Notify::DamageMove:
	{
		hb::net::PacketNotifyDamageMove pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.dir = static_cast<uint8_t>(v1);
		pkt.amount = static_cast<int32_t>(v2);
		pkt.weapon = static_cast<uint8_t>(v3);
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}
	break;

	case Notify::DownSkillIndexSet:
	case Notify::ResponseCreateNewParty:
		if (msg_type == Notify::ResponseCreateNewParty) {
			hb::net::PacketNotifyResponseCreateNewParty pkt{};
			pkt.header.msg_id = MsgId::Notify;
			pkt.header.msg_type = msg_type;
			pkt.result = static_cast<int16_t>(v1);
			ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
		}
		else {
			hb::net::PacketNotifyDownSkillIndexSet pkt{};
			pkt.header.msg_id = MsgId::Notify;
			pkt.header.msg_type = msg_type;
			pkt.skill_index = static_cast<uint16_t>(v1);
			ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
		}
		break;

	case Notify::HeldenianStart:
	case Notify::NpcTalk:
	{
		hb::net::PacketNotifyNpcTalk pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.type = static_cast<int16_t>(v1);
		pkt.response = static_cast<int16_t>(v2);
		pkt.amount = static_cast<int16_t>(v3);
		pkt.contribution = static_cast<int16_t>(v4);
		pkt.target_config_id = static_cast<int16_t>(v5);
		pkt.target_count = static_cast<int16_t>(v6);
		pkt.x = static_cast<int16_t>(v7);
		pkt.y = static_cast<int16_t>(v8);
		pkt.range = static_cast<int16_t>(v9);
		if (string != 0) {
			memcpy(pkt.reward_name, string, sizeof(pkt.reward_name));
		}
		if (string2 != 0) {
			memcpy(pkt.target_name, string2, sizeof(pkt.target_name));
		}
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}
	break;

	// Crafting
	case Notify::CraftingFail:		//reversed by Snoopy: 0x0BF1:
	{
		hb::net::PacketNotifyCraftingFail pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.reason = static_cast<int32_t>(v1);
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
		break;
	}

	case Notify::CraftingSuccess:		//reversed by Snoopy: 0x0BF0
	case Notify::PortionSuccess:
	case Notify::LowPortionSkill:
	case Notify::PortionFail:
	case Notify::NoMatchingPortion:
	{
		hb::net::PacketNotifyEmpty pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}
	break;

	case Notify::SuperAttackLeft:
	{
		hb::net::PacketNotifySuperAttackLeft pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.left = static_cast<int16_t>(m_client_list[to_h]->m_super_attack_left);
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}
	break;

	case Notify::SafeAttackMode:
	{
		hb::net::PacketNotifySafeAttackMode pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.enabled = m_client_list[to_h]->m_is_safe_attack_mode ? 1 : 0;
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}
	break;

	case Notify::QueryJoinParty:
	case Notify::IpAccountInfo:
		if (msg_type == Notify::QueryJoinParty) {
			hb::net::PacketWriter writer;
			writer.Reserve(sizeof(hb::net::PacketHeader) + 11);

			auto* pkt = writer.Append<hb::net::PacketHeader>();
			pkt->msg_id = MsgId::Notify;
			pkt->msg_type = msg_type;

			std::size_t name_len = 0;
			if (string != 0) {
				name_len = std::strlen(string);
				if (name_len > 10) {
					name_len = 10;
				}
				writer.AppendBytes(string, name_len);
			}
			writer.AppendBytes("", 1);
			ret = m_client_list[to_h]->m_socket->send_msg(writer.Data(), static_cast<int>(writer.size()));
		}
		else {
			hb::net::PacketWriter writer;
			writer.Reserve(sizeof(hb::net::PacketHeader) + 510);

			auto* pkt = writer.Append<hb::net::PacketHeader>();
			pkt->msg_id = MsgId::Notify;
			pkt->msg_type = msg_type;

			std::size_t text_len = 0;
			if (string != 0) {
				text_len = std::strlen(string);
				if (text_len >= 509) {
					text_len = 509;
				}
				writer.AppendBytes(string, text_len);
			}
			writer.AppendBytes("", 1);
			ret = m_client_list[to_h]->m_socket->send_msg(writer.Data(), static_cast<int>(writer.size()));
		}
		break;

	case Notify::RewardGold:
	{
		hb::net::PacketNotifyRewardGold pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.gold = static_cast<uint32_t>(m_client_list[to_h]->m_reward_gold);
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}
	break;

	case Notify::ServerShutdown:
	{
		hb::net::PacketNotifyServerShutdown pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.mode = static_cast<uint8_t>(v1);
		pkt.seconds = static_cast<uint16_t>(v2);
		if (string != nullptr)
		{
			std::strncpy(pkt.message, string, sizeof(pkt.message) - 1);
			pkt.message[sizeof(pkt.message) - 1] = '\0';
		}
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}
	break;

	case Notify::GlobalAttackMode:
	case Notify::WhetherChange:
		if (msg_type == Notify::GlobalAttackMode) {
			hb::net::PacketNotifyGlobalAttackMode pkt{};
			pkt.header.msg_id = MsgId::Notify;
			pkt.header.msg_type = msg_type;
			pkt.mode = static_cast<uint8_t>(v1);
			ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
		}
		else {
			hb::net::PacketNotifyWhetherChange pkt{};
			pkt.header.msg_id = MsgId::Notify;
			pkt.header.msg_type = msg_type;
			pkt.status = static_cast<uint8_t>(v1);
			ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
		}
		break;

	case Notify::FishCanceled:
	case Notify::FishSuccess:
	case Notify::FishFail:
		if (msg_type == Notify::FishCanceled) {
			hb::net::PacketNotifyFishCanceled pkt{};
			pkt.header.msg_id = MsgId::Notify;
			pkt.header.msg_type = msg_type;
			pkt.reason = static_cast<uint16_t>(v1);
			ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
		}
		else {
			hb::net::PacketNotifySimpleShort pkt{};
			pkt.header.msg_id = MsgId::Notify;
			pkt.header.msg_type = msg_type;
			pkt.value = static_cast<int16_t>(v1);
			ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
		}
		break;

	case Notify::DebugMsg:
	{
		hb::net::PacketNotifySimpleShort pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.value = static_cast<int16_t>(v1);
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}
	break;

	case Notify::FishChance:
	{
		hb::net::PacketNotifyFishChance pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.chance = static_cast<uint16_t>(v1);
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}
	break;

	case Notify::EnergySphereGoalIn:
	case Notify::EventFishMode:
		if (msg_type == Notify::EnergySphereGoalIn) {
			hb::net::PacketNotifyEnergySphereGoalIn pkt{};
			pkt.header.msg_id = MsgId::Notify;
			pkt.header.msg_type = msg_type;
			pkt.result = static_cast<int16_t>(v1);
			pkt.side = static_cast<int16_t>(v2);
			pkt.goal = static_cast<int16_t>(v3);
			if (string != 0) {
				memcpy(pkt.name, string, sizeof(pkt.name));
			}
			ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
		}
		else {
			hb::net::PacketNotifyEventFishMode pkt{};
			pkt.header.msg_id = MsgId::Notify;
			pkt.header.msg_type = msg_type;
			pkt.price = static_cast<uint16_t>(v1);
			if (string != 0) {
				memcpy(pkt.name, string, sizeof(pkt.name));
			}
			ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
		}
		break;

	case Notify::NoticeMsg:
	{
		char buf[sizeof(hb::net::PacketHeader) + 256]{};
		auto* pkt = reinterpret_cast<hb::net::PacketNotifyNoticeMsg*>(buf);
		pkt->header.msg_id = MsgId::Notify;
		pkt->header.msg_type = msg_type;
		std::size_t msg_len = 0;
		if (string != 0) {
			msg_len = std::strlen(string);
			if (msg_len > 255) msg_len = 255;
			memcpy(pkt->text, string, msg_len);
		}
		pkt->text[msg_len] = '\0';
		ret = m_client_list[to_h]->m_socket->send_msg(buf,
			static_cast<int>(sizeof(hb::net::PacketHeader) + msg_len + 1));
		break;
	}

	case Notify::StatusText:
	{
		hb::net::PacketNotifyStatusText pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		if (string != nullptr) {
			strncpy(pkt.text, string, sizeof(pkt.text) - 1);
			pkt.text[sizeof(pkt.text) - 1] = '\0';
		}
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
		break;
	}

	case Notify::CannotRating:
	{
		hb::net::PacketNotifyCannotRating pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.time_left = static_cast<uint16_t>(v1);
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}
	break;

	case Notify::RatingPlayer:
	{
		hb::net::PacketNotifyRatingPlayer pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.result = static_cast<uint8_t>(v1);
		if (string != 0) {
			memcpy(pkt.name, string, sizeof(pkt.name));
		}
		pkt.rating = m_client_list[to_h]->m_rating;
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}
	break;

	case Notify::TimeChange:
	{
		hb::net::PacketNotifyTimeChange pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.sprite_alpha = static_cast<uint8_t>(v1);
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
		break;
	}

	case Notify::ToBeRecalled:
	{
		hb::net::PacketNotifyEmpty pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}
	break;

	case Notify::Hunger:
	{
		hb::net::PacketNotifyHunger pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.hunger = static_cast<uint8_t>(v1);
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
		break;
	}

	case Notify::PlayerProfile:
	{
		hb::net::PacketNotifyPlayerProfile pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		std::size_t send_len = 0;
		if (string != 0) {
			send_len = std::strlen(string);
			if (send_len >= sizeof(pkt.text)) {
				send_len = sizeof(pkt.text) - 1;
			}
			std::size_t copy_len = send_len;
			if (copy_len > 100) {
				copy_len = 100;
			}
			memcpy(pkt.text, string, copy_len);
		}
		pkt.text[send_len] = '\0';
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt),
			static_cast<int>(sizeof(hb::net::PacketHeader) + send_len + 1));
		break;
	}

	// New 10/05/2004 Changed
	case Notify::WhisperModeOn:
	case Notify::WhisperModeOff:
	case Notify::PlayerNotOnGame:
	{
		hb::net::PacketNotifyPlayerNotOnGame pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		if (string != 0) {
			memcpy(pkt.name, string, sizeof(pkt.name));
		}
		std::memset(pkt.filler, ' ', sizeof(pkt.filler));
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}
	break;

	// New 15/05/2004 Changed
	case Notify::PlayerOnGame:
	{
		hb::net::PacketNotifyPlayerOnGame pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		if (string != 0) {
			memcpy(pkt.name, string, sizeof(pkt.name));
		}
		if (string != 0 && string[0] != 0 && string2 != 0) {
			memcpy(pkt.map_name, string2, sizeof(pkt.map_name));
		}
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}
	break;

	// New 06/05/2004
	case Notify::ItemSold:
	case Notify::ItemRepaired:
	{
		hb::net::PacketNotifyItemRepaired pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.item_id = static_cast<uint32_t>(v1);
		pkt.life = static_cast<uint32_t>(v2);
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
		break;
	}

	// New 06/05/2004
	case Notify::RepairItemPrice:
	case Notify::SellItemPrice:
	{
		if (msg_type == Notify::RepairItemPrice) {
			hb::net::PacketNotifyRepairItemPrice pkt{};
			pkt.header.msg_id = MsgId::Notify;
			pkt.header.msg_type = msg_type;
			pkt.v1 = static_cast<uint32_t>(v1);
			pkt.v2 = static_cast<uint32_t>(v2);
			pkt.v3 = static_cast<uint32_t>(v3);
			pkt.v4 = static_cast<uint32_t>(v4);
			if (string != 0) {
				memcpy(pkt.item_name, string, sizeof(pkt.item_name));
			}
			ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
		}
		else {
			hb::net::PacketNotifySellItemPrice pkt{};
			pkt.header.msg_id = MsgId::Notify;
			pkt.header.msg_type = msg_type;
			pkt.v1 = static_cast<uint32_t>(v1);
			pkt.v2 = static_cast<uint32_t>(v2);
			pkt.v3 = static_cast<uint32_t>(v3);
			pkt.v4 = static_cast<uint32_t>(v4);
			if (string != 0) {
				memcpy(pkt.item_name, string, sizeof(pkt.item_name));
			}
			ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
		}
		break;
	}

	case Notify::CannotRepairItem:
	case Notify::CannotSellItem:
		if (msg_type == Notify::CannotRepairItem) {
			hb::net::PacketNotifyCannotRepairItem pkt{};
			pkt.header.msg_id = MsgId::Notify;
			pkt.header.msg_type = msg_type;
			pkt.item_index = static_cast<uint16_t>(v1);
			pkt.reason = static_cast<uint16_t>(v2);
			if (string != 0) {
				memcpy(pkt.name, string, sizeof(pkt.name));
			}
			ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
		}
		else {
			hb::net::PacketNotifyCannotSellItem pkt{};
			pkt.header.msg_id = MsgId::Notify;
			pkt.header.msg_type = msg_type;
			pkt.item_index = static_cast<uint16_t>(v1);
			pkt.reason = static_cast<uint16_t>(v2);
			if (string != 0) {
				memcpy(pkt.name, string, sizeof(pkt.name));
			}
			ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
		}
		break;

	case Notify::ShowMap:
	{
		hb::net::PacketNotifyShowMap pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.map_id = static_cast<uint16_t>(v1);
		pkt.map_type = static_cast<uint16_t>(v2);
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}
	break;

	case Notify::SkillUsingEnd:
	{
		hb::net::PacketNotifySkillUsingEnd pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.result = static_cast<uint16_t>(v1);
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
		break;
	}

	case Notify::TotalUsers:
	{
		hb::net::PacketNotifyTotalUsers pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.total = static_cast<uint16_t>(m_total_game_server_clients);
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
		break;
	}

	case Notify::MagicEffectOff:
	case Notify::MagicEffectOn:
	{
		hb::net::PacketNotifyMagicEffect pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.magic_type = static_cast<uint16_t>(v1);
		pkt.effect = static_cast<uint32_t>(v2);
		pkt.owner = static_cast<uint32_t>(v3);
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}
	break;

	case Notify::CannotItemToBank:
	{
		hb::net::PacketNotifyEmpty pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}
	break;

	case Notify::ServerChange:
	{
		hb::net::PacketNotifyServerChange pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		memcpy(pkt.map_name, m_client_list[to_h]->m_map_name, sizeof(pkt.map_name));
		memcpy(pkt.log_server_addr, m_login_listen_ip, sizeof(pkt.log_server_addr));
		pkt.log_server_port = m_login_listen_port;
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}
	break;

	case Notify::Skill:
	{
		hb::net::PacketNotifySkill pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.skill_index = static_cast<uint16_t>(v1);
		pkt.skill_value = static_cast<uint16_t>(v2);
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
		break;
	}

	case Notify::set_item_count:
	{
		hb::net::PacketNotifySetItemCount pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.item_index = static_cast<uint16_t>(v1);
		pkt.count = v2;
		pkt.notify = static_cast<uint8_t>(v3);
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}
	break;

	case Notify::ItemDepletedEraseItem:
	{
		hb::net::PacketNotifyItemDepletedEraseItem pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.item_index = static_cast<uint16_t>(v1);
		pkt.use_result = static_cast<uint16_t>(v2);
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
		break;
	}

	case Notify::DropItemFinCountChanged:
	{
		hb::net::PacketNotifyDropItemFinCountChanged pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.item_index = static_cast<uint16_t>(v1);
		pkt.amount = static_cast<int32_t>(v2);
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
		break;
	}

	case Notify::DropItemFinEraseItem:
	{
		hb::net::PacketNotifyDropItemFinEraseItem pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.item_index = static_cast<uint16_t>(v1);
		pkt.amount = static_cast<int32_t>(v2);
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
		break;
	}

	case Notify::CannotGiveItem:
	case Notify::GiveItemFinCountChanged:
		if (msg_type == Notify::GiveItemFinCountChanged) {
			hb::net::PacketNotifyGiveItemFinCountChanged pkt{};
			pkt.header.msg_id = MsgId::Notify;
			pkt.header.msg_type = msg_type;
			pkt.item_index = static_cast<uint16_t>(v1);
			pkt.amount = static_cast<int32_t>(v2);
			if (string != 0) {
				memcpy(pkt.name, string, sizeof(pkt.name));
			}
			ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
		}
		else {
			hb::net::PacketNotifyCannotGiveItem pkt{};
			pkt.header.msg_id = MsgId::Notify;
			pkt.header.msg_type = msg_type;
			pkt.item_index = static_cast<uint16_t>(v1);
			pkt.amount = static_cast<int32_t>(v2);
			if (string != 0) {
				memcpy(pkt.name, string, sizeof(pkt.name));
			}
			ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
		}
		break;

	case Notify::GiveItemFinEraseItem:
	{
		hb::net::PacketNotifyGiveItemFinEraseItem pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.item_index = static_cast<uint16_t>(v1);
		pkt.amount = static_cast<int32_t>(v2);
		if (string != 0) {
			memcpy(pkt.name, string, sizeof(pkt.name));
		}
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
		break;
	}

	case Notify::EnemyKillReward:
	{
		hb::net::PacketNotifyEnemyKillReward pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.exp = static_cast<uint32_t>(m_client_list[to_h]->m_exp);
		pkt.kill_count = static_cast<uint32_t>(m_client_list[to_h]->m_enemy_kill_count);
		memcpy(pkt.killer_name, m_client_list[v1]->m_char_name, sizeof(pkt.killer_name));
		pkt.war_contribution = static_cast<int16_t>(m_client_list[to_h]->m_war_contribution);
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}
	break;

	case Notify::PkCaptured:
	{
		hb::net::PacketNotifyPKcaptured pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.pk_count = static_cast<uint16_t>(v1);
		pkt.victim_pk_count = static_cast<uint16_t>(v2);
		if (string != 0) {
			memcpy(pkt.victim_name, string, sizeof(pkt.victim_name));
		}
		pkt.reward_gold = static_cast<uint32_t>(m_client_list[to_h]->m_reward_gold);
		pkt.exp = static_cast<uint32_t>(m_client_list[to_h]->m_exp);
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}
	break;

	case Notify::PkPenalty:
	{
		hb::net::PacketNotifyPKpenalty pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.exp = static_cast<uint32_t>(m_client_list[to_h]->m_exp);
		pkt.str = static_cast<uint32_t>(m_client_list[to_h]->m_str);
		pkt.vit = static_cast<uint32_t>(m_client_list[to_h]->m_vit);
		pkt.dex = static_cast<uint32_t>(m_client_list[to_h]->m_dex);
		pkt.intel = static_cast<uint32_t>(m_client_list[to_h]->m_int);
		pkt.mag = static_cast<uint32_t>(m_client_list[to_h]->m_mag);
		pkt.chr = static_cast<uint32_t>(m_client_list[to_h]->m_charisma);
		pkt.pk_count = static_cast<uint32_t>(m_client_list[to_h]->m_player_kill_count);
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}
	break;

	case Notify::RepairAllPrices:
	{
		hb::net::PacketNotifyRepairAllPricesHeader header{};
		header.header.msg_id = MsgId::Notify;
		header.header.msg_type = msg_type;
		int total = m_client_list[to_h]->total_item_repair;
		if (total < 0) total = 0;
		header.total = static_cast<int16_t>(total);

		hb::net::PacketWriter writer;
		writer.Reserve(sizeof(hb::net::PacketNotifyRepairAllPricesHeader) +
			(total * sizeof(hb::net::PacketNotifyRepairAllPricesEntry)));
		writer.AppendBytes(&header, sizeof(header));

		for(int i = 0; i < total; i++) {
			hb::net::PacketNotifyRepairAllPricesEntry entry{};
			entry.index = static_cast<uint8_t>(m_client_list[to_h]->m_repair_all[i].index);
			entry.price = static_cast<int16_t>(m_client_list[to_h]->m_repair_all[i].price);
			writer.AppendBytes(&entry, sizeof(entry));
		}

		ret = m_client_list[to_h]->m_socket->send_msg(writer.Data(), static_cast<int>(writer.size()));
	}
	break;

	case Notify::TravelerLimitedLevel:
	case Notify::LimitedLevel:
	{
		hb::net::PacketNotifySimpleInt pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.value = static_cast<int32_t>(m_client_list[to_h]->m_exp);
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
		break;
	}

	case Notify::ItemReleased:
	{
		hb::net::PacketNotifyItemReleased pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.equip_pos = static_cast<int16_t>(v1);
		pkt.item_index = static_cast<int16_t>(v2);
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
		break;
	}
	case Notify::ItemDurabilityEnd:
	{
		hb::net::PacketNotifyItemDurabilityEnd pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.equip_pos = static_cast<int16_t>(v1);
		pkt.item_index = static_cast<int16_t>(v2);
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
		break;
	}

	case Notify::Killed:
	{
		hb::net::PacketNotifyKilled pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		if (string != 0) {
			memcpy(pkt.attacker_name, string, sizeof(pkt.attacker_name));
		}
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}
	break;

	case Notify::Exp:
	{
		hb::net::PacketNotifyExp pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.exp = static_cast<uint32_t>(m_client_list[to_h]->m_exp);
		pkt.rating = static_cast<int32_t>(m_client_list[to_h]->m_rating);
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
		break;
	}

	case Notify::Hp:
	{
		hb::net::PacketNotifyHP pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.hp = static_cast<uint32_t>(m_client_list[to_h]->m_hp);
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
		break;
	}

	case Notify::Mp:
	{
		hb::net::PacketNotifyMP pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.mp = static_cast<uint32_t>(m_client_list[to_h]->m_mp);
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
		break;
	}

	case Notify::Sp:
	{
		hb::net::PacketNotifySP pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.sp = static_cast<uint32_t>(m_client_list[to_h]->m_sp);
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
		break;
	}

	case Notify::Charisma:
	{
		hb::net::PacketNotifyCharisma pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.charisma = static_cast<uint32_t>(m_client_list[to_h]->m_charisma);
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}
	break;

	// State change failures
	case Notify::StateChangeFailed:
	case Notify::SettingFailed:
	{
		hb::net::PacketNotifyEmpty pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}
	break;

	case Notify::ForceMasteryRefresh:
	case Notify::StateChangeSuccess:	// 2003-04-14     .. wtf korean junk
	{
		
		hb::net::PacketNotifyStateChangeSuccess pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;

		for(int i = 0; i < hb::shared::limits::MaxMagicType; i++) {
			pkt.magic_mastery[i] = static_cast<uint8_t>(m_client_list[to_h]->m_magic_mastery[i]);
		}

		for(int i = 0; i < hb::shared::limits::MaxSkillType; i++) {
			pkt.skill_mastery[i] = static_cast<uint8_t>(m_client_list[to_h]->m_skill_mastery[i]);
		}

		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}
	break;

	case Notify::ForceStatRefresh:
	case Notify::SettingSuccess:
	case Notify::LevelUp:
	{
		hb::net::PacketNotifyLevelUp pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.level = m_client_list[to_h]->m_level;
		pkt.str = m_client_list[to_h]->m_str;
		pkt.vit = m_client_list[to_h]->m_vit;
		pkt.dex = m_client_list[to_h]->m_dex;
		pkt.intel = m_client_list[to_h]->m_int;
		pkt.mag = m_client_list[to_h]->m_mag;
		pkt.chr = m_client_list[to_h]->m_charisma;
		pkt.attack_delay = m_client_list[to_h]->m_status.attack_delay;
		ret = m_client_list[to_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
		break;
	}

	}

	switch (ret) {
	case sock::Event::QueueFull:
	case sock::Event::SocketError:
	case sock::Event::CriticalError:
	case sock::Event::SocketClosed:
		// . Time Out  .
		//delete_client(to_h, true, true);
		return;
	}
}

int CGame::get_npc_config_id_by_name(const char* npc_name) const
{
	if (npc_name == nullptr || npc_name[0] == 0) return -1;
	for (int i = 0; i < MaxNpcTypes; i++) {
		if (m_npc_config_list[i] != 0) {
			if (memcmp(npc_name, m_npc_config_list[i]->m_npc_name, hb::shared::limits::NpcNameLen - 1) == 0) {
				return i;
			}
		}
	}
	return -1;
}

bool CGame::init_npc_attr(class CNpc* npc, int npc_config_id, short sClass, char sa)
{
	int temp;
	double v1, v2, v3;

	if (npc_config_id < 0 || npc_config_id >= MaxNpcTypes || m_npc_config_list[npc_config_id] == 0) {
		return false;
	}

	{
		int config_idx = npc_config_id;
				npc->m_npc_config_id = static_cast<short>(npc_config_id);

				std::memset(npc->m_npc_name, 0, sizeof(npc->m_npc_name));
				memcpy(npc->m_npc_name, m_npc_config_list[config_idx]->m_npc_name, hb::shared::limits::NpcNameLen - 1);

				npc->m_type = m_npc_config_list[config_idx]->m_type;

				int hp_range = m_npc_config_list[config_idx]->m_hp_max - m_npc_config_list[config_idx]->m_hp_min;
				if (hp_range > 0)
					npc->m_hp = dice(1, hp_range) + m_npc_config_list[config_idx]->m_hp_min;
				else
					npc->m_hp = m_npc_config_list[config_idx]->m_hp_min;
				if (npc->m_hp <= 0) npc->m_hp = 1;

				//50Cent - HP Bar
				npc->m_max_hp = npc->m_hp;

				npc->m_exp_dice_min = m_npc_config_list[config_idx]->m_exp_dice_min;
				npc->m_exp_dice_max = m_npc_config_list[config_idx]->m_exp_dice_max;
				npc->m_gold_dice_min = m_npc_config_list[config_idx]->m_gold_dice_min;
				npc->m_gold_dice_max = m_npc_config_list[config_idx]->m_gold_dice_max;
				npc->m_drop_table_id = m_npc_config_list[config_idx]->m_drop_table_id;
				npc->m_exp = (dice(1, (m_npc_config_list[config_idx]->m_exp_dice_max - m_npc_config_list[config_idx]->m_exp_dice_min)) + m_npc_config_list[config_idx]->m_exp_dice_min);

				npc->m_hp_min = m_npc_config_list[config_idx]->m_hp_min;
				npc->m_hp_max = m_npc_config_list[config_idx]->m_hp_max;
				npc->m_hold_resist = m_npc_config_list[config_idx]->m_hold_resist;
				npc->m_defense_ratio = m_npc_config_list[config_idx]->m_defense_ratio;
				npc->m_hit_ratio = m_npc_config_list[config_idx]->m_hit_ratio;
				npc->m_min_bravery = m_npc_config_list[config_idx]->m_min_bravery;
				npc->m_min_damage = m_npc_config_list[config_idx]->m_min_damage;
				npc->m_max_damage = m_npc_config_list[config_idx]->m_max_damage;
				npc->m_size = m_npc_config_list[config_idx]->m_size;
				npc->m_side = m_npc_config_list[config_idx]->m_side;
				npc->m_action_limit = m_npc_config_list[config_idx]->m_action_limit;
				npc->m_action_time = m_npc_config_list[config_idx]->m_action_time;
				npc->m_regen_time = m_npc_config_list[config_idx]->m_regen_time;
				npc->m_resist_magic = m_npc_config_list[config_idx]->m_resist_magic;
				npc->m_magic_level = m_npc_config_list[config_idx]->m_magic_level;
				npc->m_max_mana = m_npc_config_list[config_idx]->m_max_mana; // v1.4
				npc->m_mana = m_npc_config_list[config_idx]->m_max_mana;
				npc->m_chat_msg_presence = m_npc_config_list[config_idx]->m_chat_msg_presence;
				npc->m_day_of_week_limit = m_npc_config_list[config_idx]->m_day_of_week_limit;
				npc->m_target_search_range = m_npc_config_list[config_idx]->m_target_search_range;

				switch (sClass) {
				case 43:
				case 44:
				case 45:
				case 46:
				case 47:
					npc->m_attack_strategy = AttackAI::Normal;
					break;

				default:
					npc->m_attack_strategy = dice(1, 10);
					break;
				}

				npc->m_ai_level = dice(1, 3);
				npc->m_abs_damage = m_npc_config_list[config_idx]->m_abs_damage;
				npc->m_magic_hit_ratio = m_npc_config_list[config_idx]->m_magic_hit_ratio;
				npc->m_attack_range = m_npc_config_list[config_idx]->m_attack_range;
				npc->m_special_ability = sa;
				npc->m_build_count = m_npc_config_list[config_idx]->m_min_bravery;

				switch (npc->m_special_ability) {
				case 1:
					v2 = (double)npc->m_exp;
					v3 = 25.0f / 100.0f;
					v1 = v2 * v3;
					npc->m_exp += (uint32_t)v1;
					break;

				case 2:
					v2 = (double)npc->m_exp;
					v3 = 30.0f / 100.0f;
					v1 = v2 * v3;
					npc->m_exp += (uint32_t)v1;
					break;

				case 3: // Absorbing Physical Damage
					if (npc->m_abs_damage > 0) {
						npc->m_special_ability = 0;
						sa = 0;
					}
					else {
						temp = 20 + dice(1, 60);
						npc->m_abs_damage -= temp;
						if (npc->m_abs_damage < -90) npc->m_abs_damage = -90;
					}

					v2 = (double)npc->m_exp;
					v3 = (double)abs(npc->m_abs_damage) / 100.0f;
					v1 = v2 * v3;
					npc->m_exp += (uint32_t)v1;
					break;

				case 4: // Absorbing Magical Damage
					if (npc->m_abs_damage < 0) {
						npc->m_special_ability = 0;
						sa = 0;
					}
					else {
						temp = 20 + dice(1, 60);
						npc->m_abs_damage += temp;
						if (npc->m_abs_damage > 90) npc->m_abs_damage = 90;
					}

					v2 = (double)npc->m_exp;
					v3 = (double)(npc->m_abs_damage) / 100.0f;
					v1 = v2 * v3;
					npc->m_exp += (uint32_t)v1;
					break;

				case 5:
					v2 = (double)npc->m_exp;
					v3 = 15.0f / 100.0f;
					v1 = v2 * v3;
					npc->m_exp += (uint32_t)v1;
					break;

				case 6:
				case 7:
					v2 = (double)npc->m_exp;
					v3 = 20.0f / 100.0f;
					v1 = v2 * v3;
					npc->m_exp += (uint32_t)v1;
					break;

				case 8:
					v2 = (double)npc->m_exp;
					v3 = 25.0f / 100.0f;
					v1 = v2 * v3;
					npc->m_exp += (uint32_t)v1;
					break;
				}

				npc->m_no_die_remain_exp = (npc->m_exp) - (npc->m_exp / 3);

				npc->m_status.angel_percent = static_cast<uint8_t>(sa);

				npc->m_status.attack_delay = static_cast<uint8_t>(sClass);

		return true;
	}
}

/*********************************************************************************************************************
**  int CGame::dice(int iThrow, int range)																		**
**  description			:: produces a random number between the throw and range										**
**  last updated		:: November 20, 2004; 10:24 PM; Hypnotoad													**
**	return value		:: int																						**
**********************************************************************************************************************/
uint32_t CGame::dice(uint32_t iThrow, uint32_t range)
{
	uint32_t ret;

	if (range <= 0) return 0;
	ret = 0;
	for (uint32_t i = 1; i <= iThrow; i++) {
		ret += (rand() % range) + 1;
	}
	return ret;
}

void CGame::toggle_combat_mode_handler(int client_h)
{
	if (m_client_list[client_h] == 0) return;
	if (m_client_list[client_h]->m_is_init_complete == false) return;
	if (m_client_list[client_h]->m_is_killed) return;
	if (m_client_list[client_h]->m_skill_using_status[19]) return;

	m_client_list[client_h]->m_is_attack_mode_change = true; // v2.172

	if (!m_client_list[client_h]->m_appearance.is_walking) {
		m_client_list[client_h]->m_appearance.is_walking = true;
	}
	else {
		m_client_list[client_h]->m_appearance.is_walking = false;
	}

	send_event_to_near_client_type_a(client_h, hb::shared::owner_class::Player, MsgId::EventMotion, Type::NullAction, 0, 0, 0);

}

//  int CGame::client_motion_magic_handler(int client_h, short sX, short sY, char dir)
//  description			:: checks if player is casting magic
//  last updated		:: October 29, 2004; 6:51 PM; Hypnotoad
//	return value		:: int

/*********************************************************************************************************************
**  void CGame::player_magic_handler(int client_h, int dX, int dY, short type, bool item_effect, int v1)			**
**  description			:: handles all magic related items/spells													**
**  last updated		:: November 22, 2004; 5:45 PM; Hypnotoad													**
**	return value		:: void																						**
**  commentary			::	-	added 3.51 casting detection														**
**							-	updated it so civilians can only cast certain spells on players and vice versa		**
**							-	fixed bug causing spell to be cast when mana is below required amount				**
**********************************************************************************************************************/

void CGame::request_teleport_handler(int client_h, const char* data, const char* map_name, int dX, int dY)
{
	char temp_map_name[21];
	char dest_map_name[11], map_index, quest_remain;
	direction dir;
	short sX, sY, summon_points;
	int ret, size, dest_x, dest_y, ex_h, map_side;
	bool    ret_ok, is_locked_map_notify;
	hb::time::local_time SysTime{};

	if (m_client_list[client_h] == 0) return;
	if (m_client_list[client_h]->m_is_init_complete == false) return;
	if (m_client_list[client_h]->m_is_killed) return;
	if (m_client_list[client_h]->m_is_on_waiting_process) return;
	if ((m_map_list[m_client_list[client_h]->m_map_index]->m_is_recall_impossible) &&
		(m_client_list[client_h]->m_is_killed == false) && (m_is_apocalypse_mode) && (m_client_list[client_h]->m_hp > 0)) {
		send_notify_msg(0, client_h, Notify::NoRecall, 0, 0, 0, 0);
		return;
	}
	if (!m_client_list[client_h]->m_is_gm_mode) {
		if ((memcmp(m_client_list[client_h]->m_location, "elvine", 6) == 0)
			&& (m_client_list[client_h]->m_time_left_force_recall > 0)
			&& (memcmp(m_map_list[m_client_list[client_h]->m_map_index]->m_location_name, "aresden", 7) == 0)
			&& ((data[0] == '1') || (data[0] == '3'))
			&& (m_is_crusade_mode == false)) return;

		if ((memcmp(m_client_list[client_h]->m_location, "aresden", 7) == 0)
			&& (m_client_list[client_h]->m_time_left_force_recall > 0)
			&& (memcmp(m_map_list[m_client_list[client_h]->m_map_index]->m_location_name, "elvine", 6) == 0)
			&& ((data[0] == '1') || (data[0] == '3'))
			&& (m_is_crusade_mode == false)) return;
	}

	is_locked_map_notify = false;

	if (m_client_list[client_h]->m_is_exchange_mode) {
		ex_h = m_client_list[client_h]->m_exchange_h;
		m_item_manager->clear_exchange_status(ex_h);
		m_item_manager->clear_exchange_status(client_h);
	}

	m_combat_manager->remove_from_target(client_h, hb::shared::owner_class::Player);

	// Delete all summoned NPCs belonging to this player
	for (int i = 0; i < MaxNpcs; i++)
		if (m_npc_list[i] != 0) {
			if ((m_npc_list[i]->m_is_summoned) &&
				(m_npc_list[i]->m_follow_owner_index == client_h) &&
				(m_npc_list[i]->m_follow_owner_type == hb::shared::owner_class::Player)) {
				m_entity_manager->delete_entity(i);
			}
		}

	m_map_list[m_client_list[client_h]->m_map_index]->clear_owner(13, client_h, hb::shared::owner_class::Player,
		m_client_list[client_h]->m_x,
		m_client_list[client_h]->m_y);

	send_event_to_near_client_type_a(client_h, hb::shared::owner_class::Player, MsgId::EventLog, MsgType::Reject, 0, 0, 0);

	sX = m_client_list[client_h]->m_x;
	sY = m_client_list[client_h]->m_y;

	std::memset(dest_map_name, 0, sizeof(dest_map_name));
	ret_ok = m_map_list[m_client_list[client_h]->m_map_index]->search_teleport_dest(sX, sY, dest_map_name, &dest_x, &dest_y, &dir);

	// For client-initiated tile teleports: if the player slightly overshot the
	// teleport tile (walked through it), check the 8 adjacent tiles as a fallback.
	// Force recall calls pass data[0] as '0'-'3'; client packets do not.
	if (!ret_ok && map_name == 0 && !(data[0] >= '0' && data[0] <= '3')) {
		static const short adj_dx[] = { 1, 1, 0, -1, -1, -1, 0, 1 };
		static const short adj_dy[] = { 0, 1, 1, 1, 0, -1, -1, -1 };
		for (int adj = 0; adj < 8; adj++) {
			std::memset(dest_map_name, 0, sizeof(dest_map_name));
			ret_ok = m_map_list[m_client_list[client_h]->m_map_index]->search_teleport_dest(
				sX + adj_dx[adj], sY + adj_dy[adj], dest_map_name, &dest_x, &dest_y, &dir);
			if (ret_ok) break;
		}
	}

	// Crusade
	if ((strcmp(m_client_list[client_h]->m_locked_map_name, "NONE") != 0) && (m_client_list[client_h]->m_locked_map_time > 0)) {
		map_side = get_map_location_side(dest_map_name);
		if (map_side > 3) map_side -= 2; // New 18/05/2004
		if ((map_side != 0) && (m_client_list[client_h]->m_side == map_side)) {
		}
		else {
			dest_x = -1;
			dest_y = -1;
			is_locked_map_notify = true;
			std::memset(dest_map_name, 0, sizeof(dest_map_name));
			strcpy(dest_map_name, m_client_list[client_h]->m_locked_map_name);
		}
	}

	if ((ret_ok) && (map_name == 0)) {
		bool map_found = false;
		for(int i = 0; i < MaxMaps; i++)
			if (m_map_list[i] != 0) {
				if (memcmp(m_map_list[i]->m_name, dest_map_name, 10) == 0) {
					m_client_list[client_h]->m_x = dest_x;
					m_client_list[client_h]->m_y = dest_y;
					m_client_list[client_h]->m_dir = dir;
					m_client_list[client_h]->m_map_index = i;
					std::memset(m_client_list[client_h]->m_map_name, 0, sizeof(m_client_list[client_h]->m_map_name));
					memcpy(m_client_list[client_h]->m_map_name, m_map_list[i]->m_name, 10);
					map_found = true;
					break;
				}
			}

		if (!map_found) {
			m_client_list[client_h]->m_x = dest_x;
			m_client_list[client_h]->m_y = dest_y;
			m_client_list[client_h]->m_dir = dir;
			std::memset(m_client_list[client_h]->m_map_name, 0, sizeof(m_client_list[client_h]->m_map_name));
			memcpy(m_client_list[client_h]->m_map_name, dest_map_name, 10);

			// New 18/05/2004
			send_notify_msg(0, client_h, Notify::MagicEffectOff, hb::shared::magic::Confuse,
				m_client_list[client_h]->m_magic_effect_status[hb::shared::magic::Confuse], 0, 0);
			m_item_manager->set_slate_flag(client_h, SlateClearNotify, false);

			// send_msg_to_ls(ServerMsgId::RequestSavePlayerDataReply, client_h, false);  // !   .
			m_client_list[client_h]->m_is_on_server_change = true;
			m_client_list[client_h]->m_is_on_waiting_process = true;
			return;
		}
	}
	else {
		switch (data[0]) {
		case '0':
		{
			// Forced Recall.
			std::memset(temp_map_name, 0, sizeof(temp_map_name));
			if (memcmp(m_client_list[client_h]->m_location, "NONE", 4) == 0) {
				strcpy(temp_map_name, "default");
			}
			else if (memcmp(m_client_list[client_h]->m_location, "arehunter", 9) == 0) {
				strcpy(temp_map_name, "arefarm");
			}
			else if (memcmp(m_client_list[client_h]->m_location, "elvhunter", 9) == 0) {
				strcpy(temp_map_name, "elvfarm");
			}
			else strcpy(temp_map_name, m_client_list[client_h]->m_location);

			// Crusade
			if ((strcmp(m_client_list[client_h]->m_locked_map_name, "NONE") != 0) && (m_client_list[client_h]->m_locked_map_time > 0)) {
				is_locked_map_notify = true;
				std::memset(temp_map_name, 0, sizeof(temp_map_name));
				strcpy(temp_map_name, m_client_list[client_h]->m_locked_map_name);
			}

			bool map_found = false;
			for(int i = 0; i < MaxMaps; i++)
				if (m_map_list[i] != 0) {
					if (memcmp(m_map_list[i]->m_name, temp_map_name, 10) == 0) {
						get_map_initial_point(i, &m_client_list[client_h]->m_x, &m_client_list[client_h]->m_y, m_client_list[client_h]->m_location);

						m_client_list[client_h]->m_map_index = i;
						std::memset(m_client_list[client_h]->m_map_name, 0, sizeof(m_client_list[client_h]->m_map_name));
						memcpy(m_client_list[client_h]->m_map_name, temp_map_name, 10);
						map_found = true;
						break;
					}
				}

			if (!map_found) {
				m_client_list[client_h]->m_x = -1;
				m_client_list[client_h]->m_y = -1;	  // -1 InitialPoint .

				std::memset(m_client_list[client_h]->m_map_name, 0, sizeof(m_client_list[client_h]->m_map_name));
				memcpy(m_client_list[client_h]->m_map_name, temp_map_name, 10);

				// New 18/05/2004
				send_notify_msg(0, client_h, Notify::MagicEffectOff, hb::shared::magic::Confuse,
					m_client_list[client_h]->m_magic_effect_status[hb::shared::magic::Confuse], 0, 0);
				m_item_manager->set_slate_flag(client_h, SlateClearNotify, false);

				// send_msg_to_ls(ServerMsgId::RequestSavePlayerDataReply, client_h, false); // !   .

				m_client_list[client_h]->m_is_on_server_change = true;
				m_client_list[client_h]->m_is_on_waiting_process = true;
				return;
			}
			break;
		}

		case '1':
		{
			// Recall.     .
			// if (memcmp(m_map_list[ m_client_list[client_h]->m_map_index ]->m_name, "resurr", 6) == 0) return;

			std::memset(temp_map_name, 0, sizeof(temp_map_name));
			if (memcmp(m_client_list[client_h]->m_location, "NONE", 4) == 0) {
				strcpy(temp_map_name, "default");
			}
			else {
				if (m_client_list[client_h]->m_level > 80)
					if (memcmp(m_client_list[client_h]->m_location, "are", 3) == 0)
						strcpy(temp_map_name, "aresden");
					else strcpy(temp_map_name, "elvine");
				else {
					if (memcmp(m_client_list[client_h]->m_location, "are", 3) == 0)
						strcpy(temp_map_name, "arefarm");
					else strcpy(temp_map_name, "elvfarm");
				}
			}
			// Crusade
			if ((strcmp(m_client_list[client_h]->m_locked_map_name, "NONE") != 0) && (m_client_list[client_h]->m_locked_map_time > 0)) {
				is_locked_map_notify = true;
				std::memset(temp_map_name, 0, sizeof(temp_map_name));
				strcpy(temp_map_name, m_client_list[client_h]->m_locked_map_name);
			}

			bool map_found = false;
			for(int i = 0; i < MaxMaps; i++)
				if (m_map_list[i] != 0) {
					if (memcmp(m_map_list[i]->m_name, temp_map_name, 10) == 0) {

						get_map_initial_point(i, &m_client_list[client_h]->m_x, &m_client_list[client_h]->m_y, m_client_list[client_h]->m_location);

						m_client_list[client_h]->m_map_index = i;
						std::memset(m_client_list[client_h]->m_map_name, 0, sizeof(m_client_list[client_h]->m_map_name));
						memcpy(m_client_list[client_h]->m_map_name, m_map_list[i]->m_name, 10);
						map_found = true;
						break;
					}
				}

			if (!map_found) {
				m_client_list[client_h]->m_x = -1;
				m_client_list[client_h]->m_y = -1;	  // -1 InitialPoint .

				std::memset(m_client_list[client_h]->m_map_name, 0, sizeof(m_client_list[client_h]->m_map_name));
				memcpy(m_client_list[client_h]->m_map_name, temp_map_name, 10);

				// New 18/05/2004
				send_notify_msg(0, client_h, Notify::MagicEffectOff, hb::shared::magic::Confuse,
					m_client_list[client_h]->m_magic_effect_status[hb::shared::magic::Confuse], 0, 0);
				m_item_manager->set_slate_flag(client_h, SlateClearNotify, false);

				// send_msg_to_ls(ServerMsgId::RequestSavePlayerDataReply, client_h, false); // !   .
				m_client_list[client_h]->m_is_on_server_change = true;
				m_client_list[client_h]->m_is_on_waiting_process = true;
				return;
			}
			break;
		}

		case '2':

			// Crusade
			if ((strcmp(m_client_list[client_h]->m_locked_map_name, "NONE") != 0) && (m_client_list[client_h]->m_locked_map_time > 0)) {
				dX = -1;
				dY = -1;
				is_locked_map_notify = true;
				std::memset(temp_map_name, 0, sizeof(temp_map_name));
				strcpy(temp_map_name, m_client_list[client_h]->m_locked_map_name);
			}
			else {
				std::memset(temp_map_name, 0, sizeof(temp_map_name));
				strcpy(temp_map_name, map_name);
			}

			map_index = get_map_index(temp_map_name);
			if (map_index == -1) {
				m_client_list[client_h]->m_x = dX; // -1;	  //   .
				m_client_list[client_h]->m_y = dY; // -1;	  // -1 InitialPoint .

				std::memset(m_client_list[client_h]->m_map_name, 0, sizeof(m_client_list[client_h]->m_map_name));
				memcpy(m_client_list[client_h]->m_map_name, temp_map_name, 10);

				// New 18/05/2004
				send_notify_msg(0, client_h, Notify::MagicEffectOff, hb::shared::magic::Confuse,
					m_client_list[client_h]->m_magic_effect_status[hb::shared::magic::Confuse], 0, 0);
				m_item_manager->set_slate_flag(client_h, SlateClearNotify, false);

				// send_msg_to_ls(ServerMsgId::RequestSavePlayerDataReply, client_h, false); // !   .
				m_client_list[client_h]->m_is_on_server_change = true;
				m_client_list[client_h]->m_is_on_waiting_process = true;
				return;
			}

			m_client_list[client_h]->m_x = dX;
			m_client_list[client_h]->m_y = dY;
			m_client_list[client_h]->m_map_index = map_index;

			std::memset(m_client_list[client_h]->m_map_name, 0, sizeof(m_client_list[client_h]->m_map_name));
			memcpy(m_client_list[client_h]->m_map_name, m_map_list[map_index]->m_name, 10);
			break;
		}
	}

	// New 17/05/2004
	set_playing_status(client_h);
	// Set faction/identity status fields from player data
	m_client_list[client_h]->m_status.pk = (m_client_list[client_h]->m_player_kill_count != 0) ? 1 : 0;
	m_client_list[client_h]->m_status.citizen = (m_client_list[client_h]->m_side != 0) ? 1 : 0;
	m_client_list[client_h]->m_status.aresden = (m_client_list[client_h]->m_side == 1) ? 1 : 0;
	m_client_list[client_h]->m_status.hunter = m_client_list[client_h]->m_is_player_civil ? 1 : 0;

	// Crusade
	if (is_locked_map_notify) send_notify_msg(0, client_h, Notify::LockedMap, m_client_list[client_h]->m_locked_map_time, 0, 0, m_client_list[client_h]->m_locked_map_name);

	hb::net::PacketWriter writer;
	char initMapData[hb::shared::limits::MsgBufferSize + 1];

	writer.Reset();
	auto* init_header = writer.Append<hb::net::PacketResponseInitDataHeader>();
	init_header->header.msg_id = MsgId::ResponseInitData;
	init_header->header.msg_type = MsgType::Confirm;

	if (m_client_list[client_h]->m_is_observer_mode == false)
	{
		// When dest is -1,-1 (teleport to initial points), resolve to a random spawn first
		// so get_empty_position searches near a valid point instead of (-1,-1).
		// Use the destination map's location_name (not player's m_location) to ensure
		// randomization even for players with location "NONE".
		if (m_client_list[client_h]->m_x == -1 && m_client_list[client_h]->m_y == -1)
		{
			get_map_initial_point(m_client_list[client_h]->m_map_index,
				&m_client_list[client_h]->m_x, &m_client_list[client_h]->m_y,
				m_map_list[m_client_list[client_h]->m_map_index]->m_location_name);
		}
		get_empty_position(&m_client_list[client_h]->m_x, &m_client_list[client_h]->m_y, m_client_list[client_h]->m_map_index);
	}
	else get_map_initial_point(m_client_list[client_h]->m_map_index, &m_client_list[client_h]->m_x, &m_client_list[client_h]->m_y);

	init_header->player_object_id = static_cast<std::int16_t>(client_h);
	init_header->pivot_x = static_cast<std::int16_t>(m_client_list[client_h]->m_x - hb::shared::view::PlayerPivotOffsetX);
	init_header->pivot_y = static_cast<std::int16_t>(m_client_list[client_h]->m_y - hb::shared::view::PlayerPivotOffsetY);
	init_header->player_type = m_client_list[client_h]->m_type;
	init_header->appearance = build_broadcast_appearance(client_h);
	init_header->status = m_client_list[client_h]->m_status;
	std::memcpy(init_header->map_name, m_client_list[client_h]->m_map_name, sizeof(init_header->map_name));
	std::memcpy(init_header->cur_location, m_map_list[m_client_list[client_h]->m_map_index]->m_location_name, sizeof(init_header->cur_location));

	if (m_map_list[m_client_list[client_h]->m_map_index]->m_is_fixed_day_mode)
		init_header->sprite_alpha = 1;
	else init_header->sprite_alpha = m_day_or_night;

	if (m_map_list[m_client_list[client_h]->m_map_index]->m_is_fixed_day_mode)
		init_header->weather_status = 0;
	else init_header->weather_status = m_map_list[m_client_list[client_h]->m_map_index]->m_weather_status;

	init_header->contribution = m_client_list[client_h]->m_contribution;

	if (m_client_list[client_h]->m_is_observer_mode == false) {
		m_map_list[m_client_list[client_h]->m_map_index]->set_owner(client_h,
			hb::shared::owner_class::Player,
			m_client_list[client_h]->m_x,
			m_client_list[client_h]->m_y);
	}

	init_header->observer_mode = static_cast<std::uint8_t>(m_client_list[client_h]->m_is_observer_mode);
	init_header->rating = m_client_list[client_h]->m_rating;
	init_header->hp = m_client_list[client_h]->m_hp;
	init_header->discount = 0;

	size = compose_init_map_data(m_client_list[client_h]->m_x - hb::shared::view::CenterX, m_client_list[client_h]->m_y - hb::shared::view::CenterY, client_h, initMapData);
	writer.AppendBytes(initMapData, static_cast<std::size_t>(size));

	ret = m_client_list[client_h]->m_socket->send_msg(writer.Data(), static_cast<int>(writer.size()));
	switch (ret) {
	case sock::Event::QueueFull:
	case sock::Event::SocketError:
	case sock::Event::CriticalError:
	case sock::Event::SocketClosed:
		delete_client(client_h, true, true);
		return;
	}

	send_event_to_near_client_type_a(client_h, hb::shared::owner_class::Player, MsgId::EventLog, MsgType::Confirm, 0, 0, 0);

	if ((memcmp(m_client_list[client_h]->m_location, "are", 3) == 0) &&
		(memcmp(m_map_list[m_client_list[client_h]->m_map_index]->m_location_name, "elvine", 6) == 0)) {

		m_client_list[client_h]->m_war_begin_time = GameClock::GetTimeMS();
		m_client_list[client_h]->m_is_war_location = true;
		// New 17/05/2004
		check_force_recall_time(client_h);
	}
	else if ((memcmp(m_client_list[client_h]->m_location, "elv", 3) == 0) &&
		(memcmp(m_map_list[m_client_list[client_h]->m_map_index]->m_location_name, "aresden", 7) == 0)) {

		m_client_list[client_h]->m_war_begin_time = GameClock::GetTimeMS();
		m_client_list[client_h]->m_is_war_location = true;

		// New 17/05/2004
		check_force_recall_time(client_h);
	}
	else {
		m_client_list[client_h]->m_is_war_location = false;
		m_client_list[client_h]->m_time_left_force_recall = 0;
		set_force_recall_time(client_h);
	}

	// No entering enemy shops
	int mapside, mapside2;

	mapside = get_map_location_side(m_map_list[m_client_list[client_h]->m_map_index]->m_name);
	if (mapside > 3) mapside2 = mapside - 2;
	else mapside2 = mapside;
	m_client_list[client_h]->m_is_inside_own_town = false;
	if ((m_client_list[client_h]->m_side != mapside2) && (mapside != 0)) {
		if ((mapside <= 2)) {
			if (m_client_list[client_h]->m_side != 0) {
				m_client_list[client_h]->m_war_begin_time = GameClock::GetTimeMS();
				m_client_list[client_h]->m_is_war_location = true;
				m_client_list[client_h]->m_time_left_force_recall = 1;
				m_client_list[client_h]->m_is_inside_own_town = true;
			}
		}
	}
	else {
		{
			if (memcmp(m_map_list[m_client_list[client_h]->m_map_index]->m_location_name, "arejail", 7) == 0 ||
				memcmp(m_map_list[m_client_list[client_h]->m_map_index]->m_location_name, "elvjail", 7) == 0) {
				m_client_list[client_h]->m_is_war_location = true;
				m_client_list[client_h]->m_war_begin_time = GameClock::GetTimeMS();
				if (m_client_list[client_h]->m_time_left_force_recall == 0)
					m_client_list[client_h]->m_time_left_force_recall = 100;
				else if (m_client_list[client_h]->m_time_left_force_recall > 100)
					m_client_list[client_h]->m_time_left_force_recall = 100;
			}
		}
	}

	// . v1.1
	send_notify_msg(0, client_h, Notify::SafeAttackMode, 0, 0, 0, 0);
	// v1.3
	send_notify_msg(0, client_h, Notify::DownSkillIndexSet, m_client_list[client_h]->m_down_skill_index, 0, 0, 0);
	// V1.3
	send_notify_msg(0, client_h, Notify::ItemPosList, 0, 0, 0, 0);
	// v1.4 
	m_quest_manager->send_quest_contents(client_h);
	m_quest_manager->check_quest_environment(client_h);

	// v1.432
	if (m_client_list[client_h]->m_special_ability_time == 0)
		send_notify_msg(0, client_h, Notify::SpecialAbilityEnabled, 0, 0, 0, 0);

	if (m_is_crusade_mode) {
		if (m_client_list[client_h]->m_crusade_guid == 0) {
			m_client_list[client_h]->m_crusade_duty = 0;
			m_client_list[client_h]->m_construction_point = 0;
			m_client_list[client_h]->m_crusade_guid = m_crusade_guid;
		}
		else if (m_client_list[client_h]->m_crusade_guid != m_crusade_guid) {
			m_client_list[client_h]->m_crusade_duty = 0;
			m_client_list[client_h]->m_construction_point = 0;
			m_client_list[client_h]->m_war_contribution = 0;
			m_client_list[client_h]->m_crusade_guid = m_crusade_guid;
			// ? GUID .  .
			send_notify_msg(0, client_h, Notify::Crusade, (uint32_t)m_is_crusade_mode, 0, 0, 0, -1);
		}
		m_client_list[client_h]->m_var = 1;
		send_notify_msg(0, client_h, Notify::Crusade, (uint32_t)m_is_crusade_mode, m_client_list[client_h]->m_crusade_duty, 0, 0);
	}
	else if (m_is_heldenian_mode) {
		summon_points = m_client_list[client_h]->m_charisma * 300;
		if (summon_points > m_max_summon_points) summon_points = m_max_summon_points;
		if (m_client_list[client_h]->m_heldenian_guid == 0) {
			m_client_list[client_h]->m_heldenian_guid = m_heldenian_guid;
			m_client_list[client_h]->m_construction_point = summon_points;
		}
		else if (m_client_list[client_h]->m_heldenian_guid != m_heldenian_guid) {
			m_client_list[client_h]->m_construction_point = summon_points;
			m_client_list[client_h]->m_war_contribution = 0;
			m_client_list[client_h]->m_heldenian_guid = m_heldenian_guid;
		}
		m_client_list[client_h]->m_var = 2;
		if (m_is_heldenian_mode) {
			send_notify_msg(0, client_h, Notify::HeldenianTeleport, 0, 0, 0, 0);
		}
		if (m_heldenian_initiated) {
			send_notify_msg(0, client_h, Notify::HeldenianStart, 0, 0, 0, 0);
		}
		send_notify_msg(0, client_h, Notify::ConstructionPoint, m_client_list[client_h]->m_construction_point, m_client_list[client_h]->m_war_contribution, 0, 0);
		m_war_manager->update_heldenian_status();
	}
	else if ((m_client_list[client_h]->m_var == 1) && (m_client_list[client_h]->m_crusade_guid == m_crusade_guid)) {
		m_client_list[client_h]->m_crusade_duty = 0;
		m_client_list[client_h]->m_construction_point = 0;
	}
	else {
		if (m_client_list[client_h]->m_crusade_guid == m_crusade_guid) {
			if (m_client_list[client_h]->m_var == 1) {
				send_notify_msg(0, client_h, Notify::Crusade, (uint32_t)m_is_crusade_mode, 0, 0, 0, -1);
			}
		}
		else {
			m_client_list[client_h]->m_crusade_guid = 0;
			m_client_list[client_h]->m_war_contribution = 0;
			m_client_list[client_h]->m_crusade_guid = 0;
		}
	}

	// v1.42
	// 2002-7-4
	if (memcmp(m_client_list[client_h]->m_map_name, "fight", 5) == 0) {
		hb::logger::log<log_channel::log_events>("Player '{}' entered map '{}' (observer={})", m_client_list[client_h]->m_char_name, m_client_list[client_h]->m_map_name, m_client_list[client_h]->m_is_observer_mode);
	}

	// Crusade
	send_notify_msg(0, client_h, Notify::ConstructionPoint, m_client_list[client_h]->m_construction_point, m_client_list[client_h]->m_war_contribution, 1, 0);

	// v2.15
	send_notify_msg(0, client_h, Notify::GizonItemUpgradeLeft, m_client_list[client_h]->m_gizon_item_upgrade_left, 0, 0, 0);

	if (m_is_heldenian_mode) {
		send_notify_msg(0, client_h, Notify::HeldenianTeleport, 0, 0, 0, 0);
		if (m_heldenian_initiated) {
			send_notify_msg(0, client_h, Notify::HeldenianStart, 0, 0, 0, 0);
		}
		else {
			m_war_manager->update_heldenian_status();
		}
	}

	if (m_client_list[client_h]->m_quest != 0) {
		quest_remain = (m_quest_manager->m_quest_config_list[m_client_list[client_h]->m_quest]->m_max_count - m_client_list[client_h]->m_cur_quest_count);
		send_notify_msg(0, client_h, Notify::QuestCounter, quest_remain, 0, 0, 0);
		m_quest_manager->check_is_quest_completed(client_h);
	}

	send_notify_msg(0, client_h, Notify::Hunger, m_client_list[client_h]->m_hunger_status, 0, 0, 0);
	send_notify_msg(0, client_h, Notify::SuperAttackLeft, 0, 0, 0, 0);
}

void CGame::release_follow_mode(short owner_h, char owner_type)
{
	

	for(int i = 0; i < MaxNpcs; i++)
		if ((i != owner_h) && (m_npc_list[i] != 0)) {
			if ((m_npc_list[i]->m_move_type == MoveType::Follow) &&
				(m_npc_list[i]->m_follow_owner_index == owner_h) &&
				(m_npc_list[i]->m_follow_owner_type == owner_type)) {

				m_npc_list[i]->m_move_type = MoveType::RandomWaypoint;
			}
		}
}

void CGame::quit()
{
	



	if (_lsock != 0) delete _lsock;

	for(int i = 0; i < MaxClients; i++)
		if (m_client_list[i] != 0) delete m_client_list[i];

	for(int i = 0; i < MaxNpcs; i++)
		if (m_npc_list[i] != 0) delete m_npc_list[i];

	for(int i = 0; i < MaxMaps; i++)
		if (m_map_list[i] != 0) delete m_map_list[i];

	for(int i = 0; i < MaxItemTypes; i++)
		if (m_item_config_list[i] != 0) delete m_item_config_list[i];

	for(int i = 0; i < MaxNpcTypes; i++)
		if (m_npc_config_list[i] != 0) delete m_npc_config_list[i];

	for(int i = 0; i < hb::shared::limits::MaxMagicType; i++)
		if (m_magic_config_list[i] != 0) delete m_magic_config_list[i];

	for(int i = 0; i < hb::shared::limits::MaxSkillType; i++)
		if (m_skill_config_list[i] != 0) delete m_skill_config_list[i];

	for(int i = 0; i < MaxNotifyMsgs; i++)
		if (m_notice_msg_list[i] != 0) delete m_notice_msg_list[i];

	//	for(int i = 0; i < DEF_MAXTELEPORTTYPE; i++)
	//	if (m_pTeleportConfigList[i] != 0) delete m_pTeleportConfigList[i];

	for(int i = 0; i < hb::shared::limits::MaxBuildItems; i++)
		if (m_build_item_list[i] != 0) delete m_build_item_list[i];

	if (m_notice_data != 0) delete m_notice_data;

}

uint32_t CGame::get_level_exp(int level)
{
	return hb::shared::calc::level_exp(m_formula_engine, hb::shared::calc::level{(double)level});
}

/*****************************************************************
**---------------------------FUNCTION---------------------------**
**             void Game::CheckLevelUp(int client_h)            **
**-------------------------DESCRIPTION--------------------------**
** Level-Up                                                     **
**  - Level +1                                                  **
**  - +3 Level Up Points                                        **
**  - Reset Next Level EXP                                      **
**  - Civilian Level Limit                                      **
**      Player mode switches to Combatant                       **
**      when the limit is reached                               **
**  - Majestic Points +1                                        **
**  - Reset Next Level EXP                                      **
**------------------------CREATION DATE-------------------------**
**                January 30, 2007; 3:06 PM; Dax                **
*****************************************************************/
bool CGame::check_level_up(int client_h)
{

	if (m_client_list[client_h] == 0) return false;

	while (m_client_list[client_h]->m_exp >= m_client_list[client_h]->m_next_level_exp)
	{
		if (m_client_list[client_h]->m_level < m_max_level)
		{
			// Traveler cap — block level-up past 19 for unjoineded characters
			if (m_client_list[client_h]->m_level + 1 > 19
				&& memcmp(m_client_list[client_h]->m_location, "NONE", 4) == 0)
			{
				m_client_list[client_h]->m_exp = m_client_list[client_h]->m_next_level_exp - 1;
				send_notify_msg(0, client_h, Notify::TravelerLimitedLevel, 0, 0, 0, 0);
				break;
			}

			// Carry over remainder exp into the next level
			m_client_list[client_h]->m_exp -= m_client_list[client_h]->m_next_level_exp;

			m_client_list[client_h]->m_level++;
			m_client_list[client_h]->m_levelup_pool += m_levelup_stat_gain;

			if (m_client_list[client_h]->m_str > CharPointLimit)      m_client_list[client_h]->m_str = CharPointLimit;
			if (m_client_list[client_h]->m_dex > CharPointLimit)      m_client_list[client_h]->m_dex = CharPointLimit;
			if (m_client_list[client_h]->m_vit > CharPointLimit)      m_client_list[client_h]->m_vit = CharPointLimit;
			if (m_client_list[client_h]->m_int > CharPointLimit)      m_client_list[client_h]->m_int = CharPointLimit;
			if (m_client_list[client_h]->m_mag > CharPointLimit)      m_client_list[client_h]->m_mag = CharPointLimit;
			if (m_client_list[client_h]->m_charisma > CharPointLimit) m_client_list[client_h]->m_charisma = CharPointLimit;

			// New 17/05/2004
			if (m_client_list[client_h]->m_level > 100)
				if (m_client_list[client_h]->m_is_player_civil)
					force_change_play_mode(client_h, true);

			send_notify_msg(0, client_h, Notify::SuperAttackLeft, 0, 0, 0, 0);
			send_notify_msg(0, client_h, Notify::LevelUp, 0, 0, 0, 0);
			send_notify_msg(0, client_h, Notify::LevelUpPoints, 0, 0, 0, 0);

			m_client_list[client_h]->m_next_level_exp = m_level_exp_table[m_client_list[client_h]->m_level + 1];

			m_item_manager->calc_total_item_effect(client_h, -1, false);
		}
		else {
			// Majestic — carry over remainder, award upgrade point
			m_client_list[client_h]->m_exp -= m_client_list[client_h]->m_next_level_exp;
			m_client_list[client_h]->m_gizon_item_upgrade_left++;

			m_client_list[client_h]->m_next_level_exp = m_level_exp_table[m_max_level + 1];

			send_notify_msg(0, client_h, Notify::GizonItemUpgradeLeft, m_client_list[client_h]->m_gizon_item_upgrade_left, 1, 0, 0);
		}
	}

	return false;
}
// 2003-04-14      ...
void CGame::state_change_handler(int client_h, char* data, size_t msg_size)
{
	if (m_client_list[client_h] == 0) return;
	if (m_client_list[client_h]->m_is_init_complete == false) return;
	if (m_client_list[client_h]->m_gizon_item_upgrade_left <= 0) return;

	const auto* pkt = hb::net::PacketCast<hb::net::PacketRequestStateChange>(
		data, sizeof(hb::net::PacketRequestStateChange));
	if (!pkt) return;

	int16_t str = pkt->str;
	int16_t vit = pkt->vit;
	int16_t dex = pkt->dex;
	int16_t cInt = pkt->intel;
	int16_t mag = pkt->mag;
	int16_t cChar = pkt->chr;

	// All reduction values must be >= 0
	if (str < 0 || vit < 0 || dex < 0 || cInt < 0 || mag < 0 || cChar < 0)
	{
		send_notify_msg(0, client_h, Notify::StateChangeFailed, 0, 0, 0, 0);
		return;
	}

	// Total must be a positive multiple of 3 (TotalLevelUpPoint per majestic point)
	int total_reduction = str + vit + dex + cInt + mag + cChar;
	if (total_reduction <= 0 || (total_reduction % TotalLevelUpPoint) != 0)
	{
		send_notify_msg(0, client_h, Notify::StateChangeFailed, 0, 0, 0, 0);
		return;
	}

	int majestic_cost = total_reduction / m_levelup_stat_gain;
	if (majestic_cost > m_client_list[client_h]->m_gizon_item_upgrade_left)
	{
		send_notify_msg(0, client_h, Notify::StateChangeFailed, 0, 0, 0, 0);
		return;
	}

	// Stats must equal the max-level formula (all points fully allocated)
	int old_str = m_client_list[client_h]->m_str;
	int old_vit = m_client_list[client_h]->m_vit;
	int old_dex = m_client_list[client_h]->m_dex;
	int old_int = m_client_list[client_h]->m_int;
	int old_mag = m_client_list[client_h]->m_mag;
	int old_char = m_client_list[client_h]->m_charisma;

	int expected_stats = (m_max_level - 1) * m_levelup_stat_gain
		+ m_base_stat_total;
	if (old_str + old_vit + old_dex + old_int + old_mag + old_char != expected_stats)
		return;

	// Each stat must stay >= base_stat_value and <= CharPointLimit after reduction
	if ((old_str - str < m_base_stat_value) || (old_str - str > CharPointLimit)) { send_notify_msg(0, client_h, Notify::StateChangeFailed, 0, 0, 0, 0); return; }
	if ((old_vit - vit < m_base_stat_value) || (old_vit - vit > CharPointLimit)) { send_notify_msg(0, client_h, Notify::StateChangeFailed, 0, 0, 0, 0); return; }
	if ((old_dex - dex < m_base_stat_value) || (old_dex - dex > CharPointLimit)) { send_notify_msg(0, client_h, Notify::StateChangeFailed, 0, 0, 0, 0); return; }
	if ((old_int - cInt < m_base_stat_value) || (old_int - cInt > CharPointLimit)) { send_notify_msg(0, client_h, Notify::StateChangeFailed, 0, 0, 0, 0); return; }
	if ((old_mag - mag < m_base_stat_value) || (old_mag - mag > CharPointLimit)) { send_notify_msg(0, client_h, Notify::StateChangeFailed, 0, 0, 0, 0); return; }
	if ((old_char - cChar < m_base_stat_value) || (old_char - cChar > CharPointLimit)) { send_notify_msg(0, client_h, Notify::StateChangeFailed, 0, 0, 0, 0); return; }

	hb::logger::log("Majestic stat upgrade: player={} cost={} STR={} VIT={} DEX={} INT={} MAG={} CHR={}", m_client_list[client_h]->m_char_name, majestic_cost, str, vit, dex, cInt, mag, cChar);

	// Apply reductions
	m_client_list[client_h]->m_gizon_item_upgrade_left -= majestic_cost;
	m_client_list[client_h]->m_levelup_pool += total_reduction;

	m_client_list[client_h]->m_str -= str;
	m_client_list[client_h]->m_vit -= vit;
	m_client_list[client_h]->m_dex -= dex;
	m_client_list[client_h]->m_int -= cInt;
	m_client_list[client_h]->m_mag -= mag;
	m_client_list[client_h]->m_charisma -= cChar;

	// Recalculate derived stats
	m_client_list[client_h]->m_hp = get_max_hp(client_h);
	m_client_list[client_h]->m_mp = get_max_mp(client_h);
	m_client_list[client_h]->m_sp = get_max_sp(client_h);
	send_notify_msg(0, client_h, Notify::Sp, 0, 0, 0, 0);

	send_notify_msg(0, client_h, Notify::LevelUpPoints, 0, 0, 0, 0);
	send_notify_msg(0, client_h, Notify::StateChangeSuccess, 0, 0, 0, 0);
}

void CGame::request_teleport_auth_handler(int client_h, const char* data)
{
	using namespace hb::shared::net;
	using namespace hb::server::config;

	// Basic player state validation (same checks as request_teleport_handler)
	if (m_client_list[client_h] == 0) return;
	if (m_client_list[client_h]->m_is_init_complete == false) {
		send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0, "Teleport not available");
		return;
	}
	if (m_client_list[client_h]->m_is_killed) {
		send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0, "Teleport not available");
		return;
	}
	if (m_client_list[client_h]->m_is_on_waiting_process) {
		send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0, "Teleport not available");
		return;
	}

	// Apocalypse recall-impossible map check
	if ((m_map_list[m_client_list[client_h]->m_map_index]->m_is_recall_impossible) &&
		(m_client_list[client_h]->m_is_killed == false) && (m_is_apocalypse_mode) && (m_client_list[client_h]->m_hp > 0)) {
		send_notify_msg(0, client_h, Notify::NoRecall, 0, 0, 0, 0);
		return;
	}

	// Crusade force-recall restrictions (enemy city) — GMs bypass
	if (!m_client_list[client_h]->m_is_gm_mode) {
		if ((memcmp(m_client_list[client_h]->m_location, "elvine", 6) == 0)
			&& (m_client_list[client_h]->m_time_left_force_recall > 0)
			&& (memcmp(m_map_list[m_client_list[client_h]->m_map_index]->m_location_name, "aresden", 7) == 0)
			&& (m_is_crusade_mode == false)) {
			send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0, "Teleport not available");
			return;
		}
		if ((memcmp(m_client_list[client_h]->m_location, "aresden", 7) == 0)
			&& (m_client_list[client_h]->m_time_left_force_recall > 0)
			&& (memcmp(m_map_list[m_client_list[client_h]->m_map_index]->m_location_name, "elvine", 6) == 0)
			&& (m_is_crusade_mode == false)) {
			send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0, "Teleport not available");
			return;
		}
	}

	// Check teleport tile exists at current position
	short sX = m_client_list[client_h]->m_x;
	short sY = m_client_list[client_h]->m_y;
	char dest_map_name[11]{};
	int dest_x, dest_y;
	direction dir;
	bool ret_ok = m_map_list[m_client_list[client_h]->m_map_index]->search_teleport_dest(sX, sY, dest_map_name, &dest_x, &dest_y, &dir);
	// 1-tile margin: if the player walked through a teleport tile, check adjacent tiles
	if (!ret_ok) {
		static const short adj_dx[] = { 1, 1, 0, -1, -1, -1, 0, 1 };
		static const short adj_dy[] = { 0, 1, 1, 1, 0, -1, -1, -1 };
		for (int adj = 0; adj < 8; adj++) {
			std::memset(dest_map_name, 0, sizeof(dest_map_name));
			ret_ok = m_map_list[m_client_list[client_h]->m_map_index]->search_teleport_dest(
				sX + adj_dx[adj], sY + adj_dy[adj], dest_map_name, &dest_x, &dest_y, &dir);
			if (ret_ok) break;
		}
	}
	if (!ret_ok) {
		send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0, "No teleport at this location");
		return;
	}

	// Crusade locked-map override — redirect destination
	if ((strcmp(m_client_list[client_h]->m_locked_map_name, "NONE") != 0) && (m_client_list[client_h]->m_locked_map_time > 0)) {
		int map_side = get_map_location_side(dest_map_name);
		if (map_side > 3) map_side -= 2;
		if (!((map_side != 0) && (m_client_list[client_h]->m_side == map_side))) {
			std::memset(dest_map_name, 0, sizeof(dest_map_name));
			strcpy(dest_map_name, m_client_list[client_h]->m_locked_map_name);
		}
	}

	// Look up destination map on this server
	int dest_map_index = -1;
	for (int i = 0; i < MaxMaps; i++) {
		if (m_map_list[i] != 0 && memcmp(m_map_list[i]->m_name, dest_map_name, 10) == 0) {
			dest_map_index = i;
			break;
		}
	}

	// Destination map not on this server — still valid (server change), approve
	if (dest_map_index >= 0 && !m_client_list[client_h]->m_is_gm_mode) {
		// Check level limits on destination map (GMs bypass)
		if (m_client_list[client_h]->m_level < m_map_list[dest_map_index]->m_level_requirement) {
			char msg[128];
			std::snprintf(msg, sizeof(msg), "You must be at least level %d to enter this area.",
				m_map_list[dest_map_index]->m_level_requirement);
			send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0, msg);
			return;
		}
		if ((m_map_list[dest_map_index]->m_upper_level_limit != 0) &&
			(m_client_list[client_h]->m_level > m_map_list[dest_map_index]->m_upper_level_limit)) {
			char msg[128];
			std::snprintf(msg, sizeof(msg), "Your level is too high to enter this area. (Max level: %d)",
				m_map_list[dest_map_index]->m_upper_level_limit);
			send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0, msg);
			return;
		}
	}

	// All checks pass — approve teleport
	send_notify_msg(0, client_h, Notify::TeleportApproved, 0, 0, 0, 0);
}

// 2003-04-21     ...
//  bool CGame::check_magic_int(int client_h)  //another retarded korean function
// desc		 ::     ... ...
// return value ::  true   // ....dumbass koreans
//  date		 :: 2003-04-21

void CGame::level_up_settings_handler(int client_h, char* data, size_t msg_size)
{
	int total_setting = 0;

	int16_t str, vit, dex, cInt, mag, cChar;

	if (m_client_list[client_h] == 0) return;
	if (m_client_list[client_h]->m_is_init_complete == false) return;
	if (m_client_list[client_h]->m_levelup_pool <= 0)
	{
		send_notify_msg(0, client_h, Notify::SettingFailed, 0, 0, 0, 0);
		return;
	}

	const auto* req = hb::net::PacketCast<hb::net::PacketRequestLevelUpSettings>(data, sizeof(hb::net::PacketRequestLevelUpSettings));
	if (!req) return;

	str = req->str;
	vit = req->vit;
	dex = req->dex;
	cInt = req->intel;
	mag = req->mag;
	cChar = req->chr;

	if ((str + vit + dex + cInt + mag + cChar) > m_client_list[client_h]->m_levelup_pool)
	{
		send_notify_msg(0, client_h, Notify::SettingFailed, 0, 0, 0, 0);
		return;
	}

	// Check if adding points would exceed the stat limit or be negative
	if ((m_client_list[client_h]->m_str + str > CharPointLimit) || (str < 0))
		return;

	if ((m_client_list[client_h]->m_dex + dex > CharPointLimit) || (dex < 0))
		return;

	if ((m_client_list[client_h]->m_int + cInt > CharPointLimit) || (cInt < 0))
		return;

	if ((m_client_list[client_h]->m_vit + vit > CharPointLimit) || (vit < 0))
		return;

	if ((m_client_list[client_h]->m_mag + mag > CharPointLimit) || (mag < 0))
		return;

	if ((m_client_list[client_h]->m_charisma + cChar > CharPointLimit) || (cChar < 0))
		return;

	total_setting = m_client_list[client_h]->m_str + m_client_list[client_h]->m_dex + m_client_list[client_h]->m_vit +
		m_client_list[client_h]->m_int + m_client_list[client_h]->m_mag + m_client_list[client_h]->m_charisma;

	// (  +   >   ) ..  ..      ..
	if (total_setting + m_client_list[client_h]->m_levelup_pool > ((m_client_list[client_h]->m_level - 1) * m_levelup_stat_gain + m_base_stat_total))
	{
		m_client_list[client_h]->m_levelup_pool = (m_client_list[client_h]->m_level - 1) * m_levelup_stat_gain + m_base_stat_total - total_setting;

		if (m_client_list[client_h]->m_levelup_pool < 0)
			m_client_list[client_h]->m_levelup_pool = 0;
		send_notify_msg(0, client_h, Notify::LevelUpPoints, 0, 0, 0, 0);
		send_notify_msg(0, client_h, Notify::SettingFailed, 0, 0, 0, 0);
		return;
	}

	// (  +    D >   )  ..
	if (total_setting + (str + vit + dex + cInt + mag + cChar) > ((m_client_list[client_h]->m_level - 1) * m_levelup_stat_gain + m_base_stat_total))
	{
		send_notify_msg(0, client_h, Notify::SettingFailed, 0, 0, 0, 0);
		return;
	}

	m_client_list[client_h]->m_levelup_pool = m_client_list[client_h]->m_levelup_pool - (str + vit + dex + cInt + mag + cChar);

	m_client_list[client_h]->m_str += str;
	m_client_list[client_h]->m_vit += vit;
	m_client_list[client_h]->m_dex += dex;
	m_client_list[client_h]->m_int += cInt;
	m_client_list[client_h]->m_mag += mag;
	m_client_list[client_h]->m_charisma += cChar;

	// Recalculate item effects and weapon swing speed after stat changes
	m_item_manager->calc_total_item_effect(client_h, -1, false);

	// Recalculate weapon swing speed (m_status.attack_delay)
	short weapon_index = m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::RightHand)];
	if (weapon_index == -1)
		weapon_index = m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::TwoHand)];
	if (weapon_index != -1 && m_client_list[client_h]->m_item_list[weapon_index] != nullptr)
	{
		m_client_list[client_h]->m_status.attack_delay = static_cast<uint8_t>(hb::shared::calc::attack_delay(
			m_client_list[client_h]->m_item_list[weapon_index]->m_swing_speed,
			m_client_list[client_h]->m_str,
			m_client_list[client_h]->m_angelic_str));
	}

	send_notify_msg(0, client_h, Notify::LevelUpPoints, 0, 0, 0, 0);
	send_notify_msg(0, client_h, Notify::SettingSuccess, 0, 0, 0, 0);

}

bool CGame::check_limited_user(int client_h)
{
	if (m_client_list[client_h] == 0) return false;

	// Safety net — if a traveler somehow reached level 20+, clamp exp
	if (memcmp(m_client_list[client_h]->m_location, "NONE", 4) == 0
		&& m_client_list[client_h]->m_level >= 20)
	{
		m_client_list[client_h]->m_exp = 0;
		send_notify_msg(0, client_h, Notify::TravelerLimitedLevel, 0, 0, 0, 0);
		return true;
	}

	return false;
}

void CGame::request_civil_right_handler(int client_h, char* data)
{
	uint16_t result;
	int  ret;

	if (m_client_list[client_h] == 0) return;
	if (m_client_list[client_h]->m_is_init_complete == false) return;

	if (memcmp(m_client_list[client_h]->m_location, "NONE", 4) != 0) result = 0;
	else result = 1;

	if (m_client_list[client_h]->m_level < 5) result = 0;

	if (result == 1) {
		std::memset(m_client_list[client_h]->m_location, 0, sizeof(m_client_list[client_h]->m_location));
		strcpy(m_client_list[client_h]->m_location, m_map_list[m_client_list[client_h]->m_map_index]->m_location_name);
	}

	// Side
	if (memcmp(m_client_list[client_h]->m_location, "are", 3) == 0)
		m_client_list[client_h]->m_side = 1;

	if (memcmp(m_client_list[client_h]->m_location, "elv", 3) == 0)
		m_client_list[client_h]->m_side = 2;

	hb::net::PacketResponseCivilRight pkt{};
	pkt.header.msg_id = MsgId::ResponseCivilRight;
	pkt.header.msg_type = result;
	std::memset(pkt.location, 0, sizeof(pkt.location));
	memcpy(pkt.location, m_client_list[client_h]->m_location, sizeof(pkt.location));

	ret = m_client_list[client_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	switch (ret) {
	case sock::Event::QueueFull:
	case sock::Event::SocketError:
	case sock::Event::CriticalError:
	case sock::Event::SocketClosed:
		delete_client(client_h, true, true);
		return;
	}
	send_event_to_near_client_type_a(client_h, hb::shared::owner_class::Player, MsgId::EventMotion, Type::NullAction, 0, 0, 0);
}

// 05/21/2004 - Hypnotoad - send player to jail

// 05/17/2004 - Hypnotoad - register pk log
// 05/22/2004 - Hypnotoad - register in pk log
// 05/29/2004 - Hypnotoad - Limits some items from not dropping

int CGame::calc_max_load(int client_h)
{
	if (m_client_list[client_h] == 0) return 0;

	return hb::shared::calc::max_load(m_formula_engine,
		hb::shared::calc::str{(double)m_client_list[client_h]->m_str},
		hb::shared::calc::angelic_str{(double)m_client_list[client_h]->m_angelic_str},
		hb::shared::calc::level{(double)m_client_list[client_h]->m_level})
		* hb::shared::balance::weight_units_per_stone;
}

void CGame::request_full_object_data(int client_h, char* data)
{
	uint16_t object_id;
	int ret;
	uint32_t time;

	if (m_client_list[client_h] == 0) return;
	if (m_client_list[client_h]->m_is_init_complete == false) return;

	const auto* header = hb::net::PacketCast<hb::net::PacketHeader>(data, sizeof(hb::net::PacketHeader));
	if (!header) return;
	object_id = header->msg_type;
	time = GameClock::GetTimeMS();

	if ((object_id != m_client_list[client_h]->m_last_full_object_id) ||
		(time - m_client_list[client_h]->m_last_full_object_time) > 1000) {
		m_client_list[client_h]->m_last_full_object_id = object_id;
		m_client_list[client_h]->m_last_full_object_time = time;
	}

	if (hb::shared::object_id::is_player_id(object_id)) {
		if ((object_id == 0) || (object_id >= MaxClients)) return;
		if (m_client_list[object_id] == 0) return;

		hb::net::PacketEventMotionPlayer pkt{};
		pkt.header.msg_id = MsgId::EventMotion;
		pkt.header.msg_type = Type::stop;
		pkt.object_id = object_id;
		pkt.x = m_client_list[object_id]->m_x;
		pkt.y = m_client_list[object_id]->m_y;
		pkt.type = m_client_list[object_id]->m_type;
		pkt.dir = static_cast<uint8_t>(m_client_list[object_id]->m_dir);
		memcpy(pkt.name, m_client_list[object_id]->m_char_name, sizeof(pkt.name));
		pkt.appearance = build_broadcast_appearance(object_id);

		{
			auto pktStatus = m_client_list[object_id]->m_status;
			pktStatus.pk = (m_client_list[object_id]->m_player_kill_count != 0) ? 1 : 0;
			pktStatus.citizen = (m_client_list[object_id]->m_side != 0) ? 1 : 0;
			pktStatus.aresden = (m_client_list[object_id]->m_side == 1) ? 1 : 0;
			pktStatus.hunter = m_client_list[object_id]->m_is_player_civil ? 1 : 0;
			pktStatus.relationship = m_combat_manager->get_player_relationship(object_id, client_h);
			pkt.status = pktStatus;
		}
		pkt.loc = m_client_list[object_id]->m_is_killed ? 1 : 0;

		ret = m_client_list[client_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}
	else {
		uint16_t npcIdx = hb::shared::object_id::ToNpcIndex(object_id);
		if ((npcIdx == 0) || (npcIdx >= MaxNpcs)) return;
		if (m_npc_list[npcIdx] == 0) return;

		const uint16_t objectId = object_id;
		object_id = npcIdx;

		hb::net::PacketEventMotionNpc pkt{};
		pkt.header.msg_id = MsgId::EventMotion;
		pkt.header.msg_type = Type::stop;
		pkt.object_id = objectId;
		pkt.x = m_npc_list[object_id]->m_x;
		pkt.y = m_npc_list[object_id]->m_y;
		pkt.config_id = m_npc_list[object_id]->m_npc_config_id;
		pkt.dir = static_cast<uint8_t>(m_npc_list[object_id]->m_dir);
		memcpy(pkt.name, m_npc_list[object_id]->m_name, sizeof(pkt.name));
		pkt.appearance = m_npc_list[object_id]->m_appearance;

		pkt.status = m_npc_list[object_id]->m_status;
		pkt.status.relationship = m_entity_manager->get_npc_relationship(object_id, client_h);
		pkt.loc = m_npc_list[object_id]->m_is_killed ? 1 : 0;

		ret = m_client_list[client_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}

	switch (ret) {
	case sock::Event::QueueFull:
	case sock::Event::SocketError:
	case sock::Event::CriticalError:
	case sock::Event::SocketClosed:
		delete_client(client_h, true, true);
		return;
	}
}

int CGame::get_follower_number(short owner_h, char owner_type)
{
	int total;

	total = 0;

	for(int i = 1; i < MaxNpcs; i++)
		if ((m_npc_list[i] != 0) && (m_npc_list[i]->m_move_type == MoveType::Follow)) {

			if ((m_npc_list[i]->m_follow_owner_index == owner_h) && (m_npc_list[i]->m_follow_owner_type == owner_type))
				total++;
		}

	return total;
}

void CGame::send_object_motion_reject_msg(int client_h)
{
	int     ret;

	m_client_list[client_h]->m_is_move_blocked = true; // v2.171

	// Send motion reject response.
	hb::net::PacketResponseMotionReject pkt{};
	pkt.header.msg_id = MsgId::ResponseMotion;
	pkt.header.msg_type = Confirm::MotionReject;
	pkt.x = m_client_list[client_h]->m_x;
	pkt.y = m_client_list[client_h]->m_y;
	ret = m_client_list[client_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	switch (ret) {
	case sock::Event::QueueFull:
	case sock::Event::SocketError:
	case sock::Event::CriticalError:
	case sock::Event::SocketClosed:
		// Socket error while sending motion reject.
		delete_client(client_h, true, true);
		return;
	}
	return;
}

int CGame::calc_total_weight(int client_h)
{
	int weight;
	short item_index;

	if (m_client_list[client_h] == 0) return 0;

	m_client_list[client_h]->m_alter_item_drop_index = -1;
	for (item_index = 0; item_index < hb::shared::limits::MaxItems; item_index++)
		if (m_client_list[client_h]->m_item_list[item_index] != 0) {
			switch (m_client_list[client_h]->m_item_list[item_index]->get_item_effect_type()) {
			case ItemEffectType::AlterItemDrop:
				if (m_client_list[client_h]->m_item_list[item_index]->m_instance.cur_durability > 0) {
					m_client_list[client_h]->m_alter_item_drop_index = item_index;
				}
				break;
			}
		}

	weight = 0;
	for(int i = 0; i < hb::shared::limits::MaxItems; i++)
		if (m_client_list[client_h]->m_item_list[i] != 0) {

			weight += m_item_manager->get_item_weight(m_client_list[client_h]->m_item_list[i], static_cast<int>(m_client_list[client_h]->m_item_list[i]->m_instance.count));
		}

	m_client_list[client_h]->m_cur_weight_load = weight;

	return weight;
}

void CGame::check_and_notify_player_connection(int client_h, char* pMsg, uint32_t size)
{
	char   seps[] = "= \t\r\n";
	char* token, name[hb::shared::limits::CharNameLen], buff[256], player_location[120];
	

	if (m_client_list[client_h] == 0) return;
	if (size <= 0) return;

	std::memset(player_location, 0, sizeof(player_location));
	std::memset(name, 0, sizeof(name));
	std::memset(buff, 0, sizeof(buff));
	memcpy(buff, pMsg, size);

	token = strtok(buff, seps);
	token = strtok(NULL, seps);

	if (token == 0) {
		return;
	}

	if (strlen(token) > hb::shared::limits::CharNameLen - 1)
		memcpy(name, token, hb::shared::limits::CharNameLen - 1);
	else memcpy(name, token, strlen(token));

	// name     .
	for(int i = 1; i < MaxClients; i++)
		if ((m_client_list[i] != 0) && (hb_strnicmp(name, m_client_list[i]->m_char_name, hb::shared::limits::CharNameLen - 1) == 0)) {
			send_notify_msg(0, client_h, Notify::PlayerOnGame, 0, 0, 0, m_client_list[i]->m_char_name, 0, 0, 0, 0, 0, 0, player_location);

			return;
		}

}

void CGame::set_player_profile(int client_h, char* pMsg, size_t msg_size)
{
	char temp[256];
	

	if (m_client_list[client_h] == 0) return;
	if ((msg_size - 7) <= 0) return;

	std::memset(temp, 0, sizeof(temp));
	memcpy(temp, (pMsg + 7), msg_size - 7);

	for(int i = 0; i < 256; i++)
		if (temp[i] == ' ') temp[i] = '_';

	temp[255] = 0;

	std::memset(m_client_list[client_h]->m_profile, 0, sizeof(m_client_list[client_h]->m_profile));
	strcpy(m_client_list[client_h]->m_profile, temp);
}

void CGame::get_player_profile(int client_h, char* pMsg, size_t msg_size)
{
	char   seps[] = "= \t\r\n";
	char* token, name[hb::shared::limits::CharNameLen], buff[256], buff2[500];
	

	if (m_client_list[client_h] == 0) return;
	if ((msg_size) <= 0) return;

	std::memset(name, 0, sizeof(name));
	std::memset(buff, 0, sizeof(buff));
	memcpy(buff, pMsg, msg_size);

	token = strtok(buff, seps);
	token = strtok(NULL, seps);

	if (token != 0) {
		// token
		if (strlen(token) > hb::shared::limits::CharNameLen - 1)
			memcpy(name, token, hb::shared::limits::CharNameLen - 1);
		else memcpy(name, token, strlen(token));

		for(int i = 1; i < MaxClients; i++)
			if ((m_client_list[i] != 0) && (hb_strnicmp(m_client_list[i]->m_char_name, name, hb::shared::limits::CharNameLen - 1) == 0)) {

				std::memset(buff2, 0, sizeof(buff2));
				std::snprintf(buff2, sizeof(buff2), "%s Profile: %s", name, m_client_list[i]->m_profile);
				send_notify_msg(0, client_h, Notify::PlayerProfile, 0, 0, 0, buff2);

				return;
			}
		send_notify_msg(0, client_h, Notify::PlayerNotOnGame, 0, 0, 0, name);
	}

	return;
}

void CGame::restore_player_characteristics(int client_h)
{
	int str, dex, iInt, vit, mag, charisma;
	int original_point, cur_point, verify_point, to_be_restored_point;
	int max, iA, iB;
	bool flag;
	return;
	if (m_client_list[client_h] == 0) return;

	str = m_client_list[client_h]->m_str;
	dex = m_client_list[client_h]->m_dex;
	iInt = m_client_list[client_h]->m_int;
	vit = m_client_list[client_h]->m_vit;
	mag = m_client_list[client_h]->m_mag;
	charisma = m_client_list[client_h]->m_charisma;

	cur_point = m_client_list[client_h]->m_str + m_client_list[client_h]->m_int +
		m_client_list[client_h]->m_vit + m_client_list[client_h]->m_dex +
		m_client_list[client_h]->m_mag + m_client_list[client_h]->m_charisma;

	original_point = (m_client_list[client_h]->m_level - 1) * m_levelup_stat_gain + m_base_stat_total;

	to_be_restored_point = original_point - cur_point;

	if (to_be_restored_point == 0) return;

	if (to_be_restored_point > 0) {
		// to_be_restored_point   .
		while (1) {
			flag = false;

			if ((to_be_restored_point > 0) && (m_client_list[client_h]->m_str < m_base_stat_value)) {
				m_client_list[client_h]->m_str++;
				to_be_restored_point--;
				flag = true;
			}
			if ((to_be_restored_point > 0) && (m_client_list[client_h]->m_mag < m_base_stat_value)) {
				m_client_list[client_h]->m_mag++;
				to_be_restored_point--;
				flag = true;
			}
			if ((to_be_restored_point > 0) && (m_client_list[client_h]->m_int < m_base_stat_value)) {
				m_client_list[client_h]->m_int++;
				to_be_restored_point--;
				flag = true;
			}
			if ((to_be_restored_point > 0) && (m_client_list[client_h]->m_dex < m_base_stat_value)) {
				m_client_list[client_h]->m_dex++;
				to_be_restored_point--;
				flag = true;
			}
			if ((to_be_restored_point > 0) && (m_client_list[client_h]->m_vit < m_base_stat_value)) {
				m_client_list[client_h]->m_vit++;
				to_be_restored_point--;
				flag = true;
			}
			if ((to_be_restored_point > 0) && (m_client_list[client_h]->m_charisma < m_base_stat_value)) {
				m_client_list[client_h]->m_charisma++;
				to_be_restored_point--;
				flag = true;
			}

			if (flag == false)          break;
			if (to_be_restored_point <= 0) break;
		}

		// max, Str max/2   .
		max = m_client_list[client_h]->m_skill_mastery[5];

		if (m_client_list[client_h]->m_str < (max / 2)) {

			while (1) {
				if ((to_be_restored_point > 0) && (m_client_list[client_h]->m_str < (max / 2))) {
					m_client_list[client_h]->m_str++;
					to_be_restored_point--;
				}

				if (m_client_list[client_h]->m_str == (max / 2)) break;
				if (to_be_restored_point <= 0) break;
			}
		}

		// max, Dex max/2   .
		iA = m_client_list[client_h]->m_skill_mastery[7];
		iB = m_client_list[client_h]->m_skill_mastery[8];
		if (iA > iB)
			max = iA;
		else max = iB;
		iA = m_client_list[client_h]->m_skill_mastery[9];
		if (iA > max) max = iA;
		iA = m_client_list[client_h]->m_skill_mastery[6];
		if (iA > max) max = iA;

		if (m_client_list[client_h]->m_dex < (max / 2)) {

			while (1) {
				if ((to_be_restored_point > 0) && (m_client_list[client_h]->m_dex < (max / 2))) {
					m_client_list[client_h]->m_dex++;
					to_be_restored_point--;
				}

				if (m_client_list[client_h]->m_dex == (max / 2)) break;
				if (to_be_restored_point <= 0) break;
			}
		}

		// max, Int max/2   .
		max = m_client_list[client_h]->m_skill_mastery[19];

		if (m_client_list[client_h]->m_int < (max / 2)) {

			while (1) {
				if ((to_be_restored_point > 0) && (m_client_list[client_h]->m_int < (max / 2))) {
					m_client_list[client_h]->m_int++;
					to_be_restored_point--;
				}

				if (m_client_list[client_h]->m_int == (max / 2)) break;
				if (to_be_restored_point <= 0) break;
			}
		}

		// max, Mag max/2   .
		iA = m_client_list[client_h]->m_skill_mastery[3];
		iB = m_client_list[client_h]->m_skill_mastery[4];
		if (iA > iB)
			max = iA;
		else max = iB;

		if (m_client_list[client_h]->m_mag < (max / 2)) {

			while (1) {
				if ((to_be_restored_point > 0) && (m_client_list[client_h]->m_mag < (max / 2))) {
					m_client_list[client_h]->m_mag++;
					to_be_restored_point--;
				}

				if (m_client_list[client_h]->m_mag == (max / 2)) break;
				if (to_be_restored_point <= 0) break;
			}
		}

		while (to_be_restored_point != 0) {
			switch (dice(1, 6)) {
			case 1:
				if (m_client_list[client_h]->m_str < CharPointLimit) {
					m_client_list[client_h]->m_str++;
					to_be_restored_point--;
				}
				break;
			case 2:
				if (m_client_list[client_h]->m_vit < CharPointLimit) {
					m_client_list[client_h]->m_vit++;
					to_be_restored_point--;
				}
				break;
			case 3:
				if (m_client_list[client_h]->m_dex < CharPointLimit) {
					m_client_list[client_h]->m_dex++;
					to_be_restored_point--;
				}
				break;
			case 4:
				if (m_client_list[client_h]->m_mag < CharPointLimit) {
					m_client_list[client_h]->m_mag++;
					to_be_restored_point--;
				}
				break;
			case 5:
				if (m_client_list[client_h]->m_int < CharPointLimit) {
					m_client_list[client_h]->m_int++;
					to_be_restored_point--;
				}
				break;
			case 6:
				if (m_client_list[client_h]->m_charisma < CharPointLimit) {
					m_client_list[client_h]->m_charisma++;
					to_be_restored_point--;
				}
				break;
			}
		}

		verify_point = m_client_list[client_h]->m_str + m_client_list[client_h]->m_int +
			m_client_list[client_h]->m_vit + m_client_list[client_h]->m_dex +
			m_client_list[client_h]->m_mag + m_client_list[client_h]->m_charisma;

		if (verify_point != original_point) {
			hb::logger::log("Stat restoration (minor) failed: player={} ({}/{})", m_client_list[client_h]->m_char_name, verify_point, original_point);

			m_client_list[client_h]->m_str = str;
			m_client_list[client_h]->m_dex = dex;
			m_client_list[client_h]->m_int = iInt;
			m_client_list[client_h]->m_vit = vit;
			m_client_list[client_h]->m_mag = mag;
			m_client_list[client_h]->m_charisma = charisma;
		}
		else {
			hb::logger::log("Stat restoration (minor) succeeded: player={} ({}/{})", m_client_list[client_h]->m_char_name, verify_point, original_point);
		}
	}
	else {
		// .   . to_be_restored_point !

		while (1) {
			flag = false;
			if (m_client_list[client_h]->m_str > CharPointLimit) {
				flag = true;
				m_client_list[client_h]->m_str--;
				to_be_restored_point++;
			}

			if (m_client_list[client_h]->m_dex > CharPointLimit) {
				flag = true;
				m_client_list[client_h]->m_dex--;
				to_be_restored_point++;
			}

			if (m_client_list[client_h]->m_vit > CharPointLimit) {
				flag = true;
				m_client_list[client_h]->m_vit--;
				to_be_restored_point++;
			}

			if (m_client_list[client_h]->m_int > CharPointLimit) {
				flag = true;
				m_client_list[client_h]->m_int--;
				to_be_restored_point++;
			}

			if (m_client_list[client_h]->m_mag > CharPointLimit) {
				flag = true;
				m_client_list[client_h]->m_mag--;
				to_be_restored_point++;
			}

			if (m_client_list[client_h]->m_charisma > CharPointLimit) {
				flag = true;
				m_client_list[client_h]->m_charisma--;
				to_be_restored_point++;
			}

			if (flag == false)	break;
			if (to_be_restored_point >= 0) break;
		}

		if (to_be_restored_point < 0) {
			while (to_be_restored_point != 0) {
				switch (dice(1, 6)) {
				case 1:
					if (m_client_list[client_h]->m_str > m_base_stat_value) {
						m_client_list[client_h]->m_str--;
						to_be_restored_point++;
					}
					break;
				case 2:
					if (m_client_list[client_h]->m_vit > m_base_stat_value) {
						m_client_list[client_h]->m_vit--;
						to_be_restored_point++;
					}
					break;
				case 3:
					if (m_client_list[client_h]->m_dex > m_base_stat_value) {
						m_client_list[client_h]->m_dex--;
						to_be_restored_point++;
					}
					break;
				case 4:
					if (m_client_list[client_h]->m_mag > m_base_stat_value) {
						m_client_list[client_h]->m_mag--;
						to_be_restored_point++;
					}
					break;
				case 5:
					if (m_client_list[client_h]->m_int > m_base_stat_value) {
						m_client_list[client_h]->m_int--;
						to_be_restored_point++;
					}
					break;
				case 6:
					if (m_client_list[client_h]->m_charisma > m_base_stat_value) {
						m_client_list[client_h]->m_charisma--;
						to_be_restored_point++;
					}
					break;
				}
			}
		}
		else {
			while (to_be_restored_point != 0) {
				switch (dice(1, 6)) {
				case 1:
					if (m_client_list[client_h]->m_str < CharPointLimit) {
						m_client_list[client_h]->m_str++;
						to_be_restored_point--;
					}
					break;
				case 2:
					if (m_client_list[client_h]->m_vit < CharPointLimit) {
						m_client_list[client_h]->m_vit++;
						to_be_restored_point--;
					}
					break;
				case 3:
					if (m_client_list[client_h]->m_dex < CharPointLimit) {
						m_client_list[client_h]->m_dex++;
						to_be_restored_point--;
					}
					break;
				case 4:
					if (m_client_list[client_h]->m_mag < CharPointLimit) {
						m_client_list[client_h]->m_mag++;
						to_be_restored_point--;
					}
					break;
				case 5:
					if (m_client_list[client_h]->m_int < CharPointLimit) {
						m_client_list[client_h]->m_int++;
						to_be_restored_point--;
					}
					break;
				case 6:
					if (m_client_list[client_h]->m_charisma < CharPointLimit) {
						m_client_list[client_h]->m_charisma++;
						to_be_restored_point--;
					}
					break;
				}
			}
		}

		verify_point = m_client_list[client_h]->m_str + m_client_list[client_h]->m_int +
			m_client_list[client_h]->m_vit + m_client_list[client_h]->m_dex +
			m_client_list[client_h]->m_mag + m_client_list[client_h]->m_charisma;

		if (verify_point != original_point) {
			hb::logger::log("Stat restoration (overflow) failed: player={} ({}/{})", m_client_list[client_h]->m_char_name, verify_point, original_point);

		}
		else {
			hb::logger::log("Stat restoration (overflow) succeeded: player={} ({}/{})", m_client_list[client_h]->m_char_name, verify_point, original_point);
		}
	}
}

int CGame::get_player_number_on_spot(short dX, short dY, char map_index, char range)
{
	int sum = 0;
	short owner_h;
	char  owner_type;

	for(int ix = dX - range; ix <= dX + range; ix++)
		for(int iy = dY - range; iy <= dY + range; iy++) {
			m_map_list[map_index]->get_owner(&owner_h, &owner_type, ix, iy);
			if ((owner_h != 0) && (owner_type == hb::shared::owner_class::Player))
				sum++;
		}

	return sum;
}

void CGame::check_day_or_night_mode()
{
	hb::time::local_time SysTime{};
	char prev_mode;

	// DEBUG: Force night mode for testing light effects
	// Set to 0 to use normal day/night cycle, 1 for forced day, 2 for forced night
	constexpr int DEBUG_FORCE_TIME = 0;

	prev_mode = m_day_or_night;

	SysTime = hb::time::local_time::now();
	if (SysTime.minute >= m_nighttime_duration)
		m_day_or_night = 2;
	else m_day_or_night = 1;

	if (prev_mode != m_day_or_night) {
		for(int i = 1; i < MaxClients; i++)
			if ((m_client_list[i] != 0) && (m_client_list[i]->m_is_init_complete)) {
				if ((m_client_list[i]->m_map_index >= 0) &&
					(m_map_list[m_client_list[i]->m_map_index] != 0) &&
					(m_map_list[m_client_list[i]->m_map_index]->m_is_fixed_day_mode == false))
					send_notify_msg(0, i, Notify::TimeChange, m_day_or_night, 0, 0, 0);
			}
	}
}

void CGame::set_player_reputation(int client_h, char* pMsg, char value, size_t msg_size)
{
	char   seps[] = "= \t\r\n";
	char* token, name[hb::shared::limits::CharNameLen], buff[256];
	

	if (m_client_list[client_h] == 0) return;
	if ((msg_size) <= 0) return;
	if (m_client_list[client_h]->m_level < 40) return;

	if ((m_client_list[client_h]->m_time_left_rating != 0) || (m_client_list[client_h]->m_player_kill_count != 0)) {
		send_notify_msg(0, client_h, Notify::CannotRating, m_client_list[client_h]->m_time_left_rating, 0, 0, 0);
		return;
	}
	else if (memcmp(m_client_list[client_h]->m_location, "NONE", 4) == 0) {
		send_notify_msg(0, client_h, Notify::CannotRating, 0, 0, 0, 0);
		return;
	}

	std::memset(name, 0, sizeof(name));
	std::memset(buff, 0, sizeof(buff));
	memcpy(buff, pMsg, msg_size);

	token = strtok(buff, seps);
	token = strtok(NULL, seps);

	if (token != 0) {
		// token
		if (strlen(token) > hb::shared::limits::CharNameLen - 1)
			memcpy(name, token, hb::shared::limits::CharNameLen - 1);
		else memcpy(name, token, strlen(token));

		for(int i = 1; i < MaxClients; i++)
			if ((m_client_list[i] != 0) && (hb_strnicmp(m_client_list[i]->m_char_name, name, hb::shared::limits::CharNameLen - 1) == 0)) {

				if (i != client_h) {
					if (value == 0)
						m_client_list[i]->m_rating--;
					else if (value == 1)
						m_client_list[i]->m_rating++;

					if (m_client_list[i]->m_rating > 500)  m_client_list[i]->m_rating = 500;
					if (m_client_list[i]->m_rating < -500) m_client_list[i]->m_rating = -500;
					m_client_list[client_h]->m_time_left_rating = 20 * 60;

					send_notify_msg(0, i, Notify::RatingPlayer, value, 0, 0, name);
					send_notify_msg(0, client_h, Notify::RatingPlayer, value, 0, 0, name);

					return;
				}
			}
		send_notify_msg(0, client_h, Notify::PlayerNotOnGame, 0, 0, 0, name);
	}

	return;
}

bool CGame::read_notify_msg_list_file(const char* fn)
{
	FILE* file;
	uint32_t  file_size;
	char* cp, * token, read_mode;
	char seps[] = "=\t\n;";

	read_mode = 0;
	m_total_notice_msg = 0;

	std::error_code ec;
	auto fsize = std::filesystem::file_size(fn, ec);
	file_size = ec ? 0 : static_cast<uint32_t>(fsize);

	file = fopen(fn, "rt");
	if (file == 0) {
		hb::logger::warn("Notify message list file not found");
		return false;
	}
	else {
		hb::logger::log("Reading notify message list file");
		cp = new char[file_size + 2];
		std::memset(cp, 0, file_size + 2);
		if (fread(cp, file_size, 1, file) != 1)
			hb::logger::warn("Short read on notify message list file");

		token = strtok(cp, seps);
		while (token != 0) {

			if (read_mode != 0) {
				switch (read_mode) {
				case 1:
					for(int i = 0; i < MaxNotifyMsgs; i++)
						if (m_notice_msg_list[i] == 0) {
							m_notice_msg_list[i] = new class CMsg;
							m_notice_msg_list[i]->put(0, token, strlen(token), 0, 0);
							m_total_notice_msg++;
							break;
						}

					read_mode = 0;
					break;
				}
			}
			else {
				if (memcmp(token, "notify_msg", 10) == 0) read_mode = 1;
			}

			token = strtok(NULL, seps);
		}

		delete cp;
	}
	if (file != 0) fclose(file);

	return true;
}
void CGame::notice_handler()
{
	char  temp, buffer[1000], key;
	size_t size = 0;
	uint32_t time = GameClock::GetTimeMS();
	int msg_index, temp_int;

	if (m_total_notice_msg <= 1) return;

	if ((time - m_notice_time) > NoticeTime) {
		m_notice_time += NoticeTime;
		if (time - m_notice_time > NoticeTime) m_notice_time = time;
		do {
			msg_index = dice(1, m_total_notice_msg) - 1;
		} while (msg_index == m_prev_send_notice_msg);

		m_prev_send_notice_msg = msg_index;

		std::memset(buffer, 0, sizeof(buffer));
		if (m_notice_msg_list[msg_index] != 0) {
			m_notice_msg_list[msg_index]->get(&temp, buffer, &size, &temp_int, &key);
		}

		for(int i = 1; i < MaxClients; i++)
			if (m_client_list[i] != 0) {
				send_notify_msg(0, i, Notify::NoticeMsg, 0, 0, 0, buffer);
			}
	}
}

void CGame::response_save_player_data_reply_handler(char* data, size_t msg_size)
{
	char* cp, char_name[hb::shared::limits::CharNameLen];

	std::memset(char_name, 0, sizeof(char_name));

	const auto* header = hb::net::PacketCast<hb::net::PacketHeader>(data, sizeof(hb::net::PacketHeader));
	if (!header) return;
	cp = (char*)(data + sizeof(hb::net::PacketHeader));
	memcpy(char_name, cp, hb::shared::limits::CharNameLen - 1);

	for(int i = 0; i < MaxClients; i++)
		if (m_client_list[i] != 0) {
			if (hb_strnicmp(m_client_list[i]->m_char_name, char_name, hb::shared::limits::CharNameLen - 1) == 0) {
				send_notify_msg(0, i, Notify::ServerChange, 0, 0, 0, 0);
			}
		}
}

void CGame::calc_exp_stock(int client_h)
{
	bool is_level_up;
	CItem* item;

	if (m_client_list[client_h] == 0) return;
	if (m_client_list[client_h]->m_is_init_complete == false) return;
	if (m_client_list[client_h]->m_exp_stock <= 0) return;
	//if ((m_client_list[client_h]->m_level >= m_max_level) && (m_client_list[client_h]->m_exp >= m_level_exp_table[m_max_level])) return;

	if (m_map_list[m_client_list[client_h]->m_map_index]->m_type == smap::MapType::NoPenaltyNoReward) {
		m_client_list[client_h]->m_exp_stock = 0;
		return;
	}

	m_client_list[client_h]->m_exp += m_client_list[client_h]->m_exp_stock;
	m_client_list[client_h]->m_auto_exp_amount += m_client_list[client_h]->m_exp_stock;
	m_client_list[client_h]->m_exp_stock = 0;

	is_level_up = check_level_up(client_h);

	if (check_limited_user(client_h) == false) {
		send_notify_msg(0, client_h, Notify::Exp, 0, 0, 0, 0);
	}

	if ((is_level_up) && (m_client_list[client_h]->m_level <= 5)) {
		// Gold .  1~5 100 Gold .
		item = new CItem;
		if (m_item_manager->init_item_attr(item, hb::shared::item::ItemId::Gold) == false) {
			delete item;
			return;
		}
		else item->m_instance.count = (uint32_t)100000;
		m_item_manager->add_item(client_h, item, 0);
	}

	if ((is_level_up) && (m_client_list[client_h]->m_level > 5) && (m_client_list[client_h]->m_level <= 20)) {
		// Gold .  5~20 300 Gold .
		item = new CItem;
		if (m_item_manager->init_item_attr(item, hb::shared::item::ItemId::Gold) == false) {
			delete item;
			return;
		}
		else item->m_instance.count = (uint32_t)100000;
		m_item_manager->add_item(client_h, item, 0);
	}
}

void CGame::restore_player_rating(int client_h)
{
	if (m_client_list[client_h] == 0) return;

	if (m_client_list[client_h]->m_rating < -10000) m_client_list[client_h]->m_rating = 0;
	if (m_client_list[client_h]->m_rating > 10000) m_client_list[client_h]->m_rating = 0;
}

int CGame::get_exp_level(uint32_t exp)
{
	

	for(int i = 1; i < 1000; i++)
		if ((m_level_exp_table[i] <= exp) && (m_level_exp_table[i + 1] > exp)) return i;

	return 0;
}

int CGame::calc_player_num(char map_index, short dX, short dY, char radius)
{
	int ret;
	class CTile* tile;

	if ((map_index < 0) || (map_index > MaxMaps)) return 0;
	if (m_map_list[map_index] == 0) return 0;

	ret = 0;
	for(int ix = dX - radius; ix <= dX + radius; ix++)
		for(int iy = dY - radius; iy <= dY + radius; iy++) {
			if ((ix < 0) || (ix >= m_map_list[map_index]->m_size_x) ||
				(iy < 0) || (iy >= m_map_list[map_index]->m_size_y)) {
			}
			else {
				tile = (class CTile*)(m_map_list[map_index]->m_tile + ix + iy * m_map_list[map_index]->m_size_y);
				if ((tile->m_owner != 0) && (tile->m_owner_class == hb::shared::owner_class::Player))
					ret++;
			}
		}

	return ret;
}

void CGame::weather_processor()
{
	char prev_mode;
	int j;
	uint32_t time;

	time = GameClock::GetTimeMS();

	for(int i = 0; i < MaxMaps; i++) {
		if ((m_map_list[i] != 0) && (m_map_list[i]->m_is_fixed_day_mode == false)) {
			prev_mode = m_map_list[i]->m_weather_status;
			if (m_map_list[i]->m_weather_status != 0) {
				if ((time - m_map_list[i]->m_weather_start_time) > m_map_list[i]->m_weather_duration)
					m_map_list[i]->m_weather_status = 0;
			}
			else {
				if (dice(1, 300) == 13) {
					m_map_list[i]->m_weather_status = static_cast<char>(dice(1, 3)); //This looks better or else we only get snow :(
					//m_map_list[i]->m_weather_status = dice(1,3)+3; <- This original code looks fucked
					m_map_list[i]->m_weather_start_time = time;
					m_map_list[i]->m_weather_duration = 60000 * 3 + 60000 * dice(1, 7);
				}
			}

			if (m_map_list[i]->m_is_snow_enabled) {
				m_map_list[i]->m_weather_status = static_cast<char>(dice(1, 3) + 3);
				m_map_list[i]->m_weather_start_time = time;
				m_map_list[i]->m_weather_duration = 60000 * 3 + 60000 * dice(1, 7);
			}

			if (prev_mode != m_map_list[i]->m_weather_status) {
				for (j = 1; j < MaxClients; j++)
					if ((m_client_list[j] != 0) && (m_client_list[j]->m_is_init_complete) && (m_client_list[j]->m_map_index == i))
						send_notify_msg(0, j, Notify::WhetherChange, m_map_list[i]->m_weather_status, 0, 0, 0);
			}
		}
	} //for Loop
}

/*********************************************************************************************************************
**  int CGame::get_weather_magic_bonus_effect(short type, char wheather_status)										**
**  description			:: checks for a weather bonus when magic is cast											**
**  last updated		:: November 20, 2004; 10:34 PM; Hypnotoad													**
**	return value		:: int																						**
*********************************************************************************************************************/

int CGame::get_map_index(char* map_name)
{
	int map_index;
	char tmp_name[256];

	std::memset(tmp_name, 0, sizeof(tmp_name));
	strcpy(tmp_name, map_name);

	map_index = -1;
	for(int i = 0; i < MaxMaps; i++)
		if (m_map_list[i] != 0) {
			if (memcmp(m_map_list[i]->m_name, map_name, 10) == 0)
				map_index = i;
		}

	return map_index;
}

int CGame::force_player_disconnect(int num)
{
	int cnt;

	cnt = 0;
	for(int i = 1; i < MaxClients; i++)
		if (m_client_list[i] != 0) {
			if (m_client_list[i]->m_is_init_complete)
				delete_client(i, true, true);
			else delete_client(i, false, false);
			cnt++;
			if (cnt >= num) break;
		}

	return cnt;
}

int CGame::save_all_players()
{
	int count = 0;
	for (int i = 1; i < MaxClients; i++)
	{
		if (m_client_list[i] != nullptr && m_client_list[i]->m_is_init_complete)
		{
			g_login->local_save_player_data(i);
			count++;
		}
	}
	return count;
}

void CGame::special_event_handler()
{
	uint32_t time;

	time = GameClock::GetTimeMS();

	if ((time - m_special_event_time) < SpecialEventTime) return; // SpecialEventTime
	m_special_event_time += SpecialEventTime;
	if (time - m_special_event_time > SpecialEventTime) m_special_event_time = time;
	m_is_special_event_time = true;

	switch (dice(1, 180)) {
	case 98: m_special_event_type = 2; break; // 30 1 1/30
	default: m_special_event_type = 1; break;
	}
}

void CGame::toggle_safe_attack_mode_handler(int client_h) //v1.1
{
	if (m_client_list[client_h] == 0) return;
	if (m_client_list[client_h]->m_is_init_complete == false) return;
	if (m_client_list[client_h]->m_is_killed) return;

	if (m_client_list[client_h]->m_is_safe_attack_mode)
		m_client_list[client_h]->m_is_safe_attack_mode = false;
	else m_client_list[client_h]->m_is_safe_attack_mode = true;

	send_notify_msg(0, client_h, Notify::SafeAttackMode, 0, 0, 0, 0);
}

void CGame::force_disconnect_account(char* account_name, uint16_t count)
{
	

	for(int i = 1; i < MaxClients; i++)
		if ((m_client_list[i] != 0) && (hb_strnicmp(m_client_list[i]->m_account_name, account_name, hb::shared::limits::AccountNameLen - 1) == 0)) {
			hb::logger::log("Client {}: force disconnect char={} account={} count={}", i, m_client_list[i]->m_char_name, m_client_list[i]->m_account_name, count);

			//delete_client(i, true, true);

			//v1.4312
			send_notify_msg(0, i, Notify::ForceDisconn, count, 0, 0, 0);
		}
}

bool CGame::on_close()
{
	if (m_is_server_shutdown == false)
	{
#ifdef _WIN32
		if (MessageBox(0, "Player data not saved! shutdown server now?", m_realm_name, MB_ICONEXCLAMATION | MB_YESNO) == IDYES) return true;
		return false;
#else
		return false;
#endif
	}
	else return true;

	return false;
}

// 05/24/2004 - Hypnotoad - Hammer and Wand train to 100% fixed

//Hero Code by Zabuza

// New 14/05/2004

int CGame::get_max_hp(int client_h)
{
	if (m_client_list[client_h] == 0) return 0;

	int ret = hb::shared::calc::max_hp(m_formula_engine,
		hb::shared::calc::vit{(double)m_client_list[client_h]->m_vit},
		hb::shared::calc::level{(double)m_client_list[client_h]->m_level},
		hb::shared::calc::str{(double)m_client_list[client_h]->m_str},
		hb::shared::calc::angelic_str{(double)m_client_list[client_h]->m_angelic_str});

	// Apply side effect reduction if active
	if (m_client_list[client_h]->m_side_effect_max_hp_down != 0)
		ret = ret - (ret / m_client_list[client_h]->m_side_effect_max_hp_down);

	return ret;
}

int CGame::get_max_mp(int client_h)
{
	if (m_client_list[client_h] == 0) return 0;

	return hb::shared::calc::max_mp(m_formula_engine,
		hb::shared::calc::mag{(double)m_client_list[client_h]->m_mag},
		hb::shared::calc::angelic_mag{(double)m_client_list[client_h]->m_angelic_mag},
		hb::shared::calc::level{(double)m_client_list[client_h]->m_level},
		hb::shared::calc::intel{(double)m_client_list[client_h]->m_int},
		hb::shared::calc::angelic_int{(double)m_client_list[client_h]->m_angelic_int});
}

int CGame::get_max_sp(int client_h)
{
	if (m_client_list[client_h] == 0) return 0;

	return hb::shared::calc::max_sp(m_formula_engine,
		hb::shared::calc::str{(double)m_client_list[client_h]->m_str},
		hb::shared::calc::angelic_str{(double)m_client_list[client_h]->m_angelic_str},
		hb::shared::calc::level{(double)m_client_list[client_h]->m_level});
}

void CGame::get_map_initial_point(int map_index, short* pX, short* pY, char* player_location)
{
	int total_point = 0;

	hb::shared::geometry::GamePoint list[smap::MaxInitialPoint];

	if (m_map_list[map_index] == 0)
		return;
	for (int i = 0; i < smap::MaxInitialPoint; i++)
		if (m_map_list[map_index]->m_initial_point[i].x != -1) {
			list[total_point].x = m_map_list[map_index]->m_initial_point[i].x;
			list[total_point].y = m_map_list[map_index]->m_initial_point[i].y;
			total_point++;
		}
	if (total_point == 0) return;
	int index = 0;
	if ((player_location != 0) && (memcmp(player_location, "NONE", 4) != 0))
		index = dice(1, total_point) - 1;

	*pX = static_cast<short>(list[index].x);
	*pY = static_cast<short>(list[index].y);
}

// MODERNIZED: New function that polls login client socket instead of handling window messages
void CGame::on_login_client_socket_event(int login_client_h)
{
	int ret;

	if (login_client_h < 0 || login_client_h >= MaxClientLoginSock) return;

	auto p = _lclients[login_client_h];
	if (p == 0) return;

	ret = p->sock->Poll();

	switch (ret) {
	case sock::Event::UnsentDataSendComplete:
		break;
	case sock::Event::ConnectionEstablish:
		break;

	case sock::Event::ReadComplete:
		on_client_login_read(login_client_h);
		break;

	case sock::Event::Block:
		break;

	case sock::Event::ConfirmCodeNotMatch:
		hb::logger::log("Client {}: login confirm code mismatch", login_client_h);
		delete_login_client(login_client_h);
		break;
	case sock::Event::MsgSizeTooLarge:
	case sock::Event::SocketError:
	case sock::Event::SocketClosed:
		delete_login_client(login_client_h);
		break;
	}
}


LoginClient::~LoginClient()
{
	if (sock)
		delete sock;
}

void CGame::on_client_login_read(int h)
{
	char* data, key;
	size_t  msg_size;

	if (_lclients[h] == 0) return;

	data = _lclients[h]->sock->get_rcv_data_pointer(&msg_size, &key);

	if (put_msg_queue(Source::LogServer, data, msg_size, h, key) == false) {
		hb::logger::error("Critical error in message queue");
	}
}

void CGame::delete_login_client(int h)
{
	if (!_lclients[h])
		return;

	_lclients[h]->timeout_tm = GameClock::GetTimeMS();
	_lclients_disconn.push_back(_lclients[h]);
	//delete _lclients[h];
	_lclients[h] = nullptr;
}

// 3.51 - 05/17/2004 - Hypnotoad/[KLKS] - Monster Special Abilities
char CGame::get_special_ability(int kind_sa)
{
	char sa;

	switch (kind_sa) {
	case 1:
		// Slime, Orc, Orge, WereWolf, YB-, Rabbit, Mountain-Giant, Stalker, Hellclaw, 
		// Wyvern, Fire-Wyvern, Barlog, Tentocle, Centaurus, Giant-Lizard, Minotaurus,
		// Abaddon, Claw-Turtle, Giant-Cray-Fish, Giant-Plant, MasterMage-Orc, Nizie,
		// Tigerworm
		switch (dice(1, 2)) {
		case 1: sa = 3; break; // Anti-Physical Damage
		case 2: sa = 4; break; // Anti-Magic Damage
		}
		break;

	case 2:
		// Giant-Ant, Cat, Giant-Frog, 
		switch (dice(1, 3)) {
		case 1: sa = 3; break; // Anti-Physical Damage
		case 2: sa = 4; break; // Anti-Magic Damage
		case 3: sa = 5; break; // Poisonous
		}
		break;

	case 3:
		// Zombie, Scorpion, Amphis, Troll, Dark-Elf
		switch (dice(1, 4)) {
		case 1: sa = 3; break; // Anti-Physical Damage
		case 2: sa = 4; break; // Anti-Magic Damage
		case 3: sa = 5; break; // Poisonous
		case 4: sa = 6; break; // Critical Poisonous
		}
		break;

	case 4:
		// no linked Npc
		switch (dice(1, 3)) {
		case 1: sa = 3; break; // Anti-Physical Damage
		case 2: sa = 4; break; // Anti-Magic Damage
		case 3: sa = 7; break; // Explosive
		}
		break;

	case 5:
		// Stone-Golem, Clay-Golem, Beholder, Cannibal-Plant, Rudolph, DireBoar
		switch (dice(1, 4)) {
		case 1: sa = 3; break; // Anti-Physical Damage
		case 2: sa = 4; break; // Anti-Magic Damage
		case 3: sa = 7; break; // Explosive
		case 4: sa = 8; break; // Critical-Explosive
		}
		break;

	case 6:
		// no linked Npc
		switch (dice(1, 3)) {
		case 1: sa = 3; break; // Anti-Physical Damage
		case 2: sa = 4; break; // Anti-Magic Damage
		case 3: sa = 5; break; // Poisonous
		}
		break;

	case 7:
		// Orc-Mage, Unicorn
		switch (dice(1, 3)) {
		case 1: sa = 1; break; // Clairvoyant
		case 2: sa = 2; break; // Distruction of Magic Protection
		case 3: sa = 4; break; // Anti-Magic Damage
		}
		break;

	case 8:
		// Frost, Ice-Golem, Ettin, Gagoyle, Demon, Liche, Hellbound, Cyclops, 
		// Skeleton
		switch (dice(1, 5)) {
		case 1: sa = 1; break; // Clairvoyant
		case 2: sa = 2; break; // Distruction of Magic Protection
		case 3: sa = 4; break; // Anti-Magic Damage
		case 4: sa = 3; break; // Anti-Physical Damage
		case 5: sa = 8; break; // Critical-Explosive
		}
		break;

	case 9:
		// no linked Npc
		sa = static_cast<char>(dice(1, 8)); // All abilities available
		break;
	}

	return sa;
}

void CGame::check_special_event(int client_h)
{
	CItem* item;
	char  item_name[hb::shared::limits::ItemNameLen];
	int   erase_req;

	if (m_client_list[client_h] == 0) return;

	if (m_client_list[client_h]->m_special_event_id == 200081) {

		if (m_client_list[client_h]->m_level < 11) {
			m_client_list[client_h]->m_special_event_id = 0;
			return;
		}

		std::memset(item_name, 0, sizeof(item_name));
		strcpy(item_name, "MemorialRing");

		item = new CItem;
		if (m_item_manager->init_item_attr(item, item_name) == false) {
			delete item;
		}
		else {
			if (m_item_manager->add_client_item_list(client_h, item, &erase_req)) {
				if (m_client_list[client_h]->m_cur_weight_load < 0) m_client_list[client_h]->m_cur_weight_load = 0;

				// testcode  .
				hb::logger::log<log_channel::events>("get MemorialRing : Char({})", m_client_list[client_h]->m_char_name);

				item->set_touch_effect_type(TouchEffectType::UniqueOwner);
				item->m_instance.touch_effect_value1 = m_client_list[client_h]->m_char_id_num1;
				item->m_instance.touch_effect_value2 = m_client_list[client_h]->m_char_id_num2;
				item->m_instance.touch_effect_value3 = m_client_list[client_h]->m_char_id_num3;
				item->m_instance.item_color = 14;

				m_client_list[client_h]->m_special_event_id = 0;
			}
		}
	}
}

void CGame::request_noticement_handler(int client_h, char* data)
{
	char* cp;
	int ret, client_size;

	if (m_client_list[client_h] == 0) return;
	if (m_notice_data_size < 10) return;

	const auto* pkt = hb::net::PacketCast<hb::net::PacketRequestNoticement>(
		data, sizeof(hb::net::PacketRequestNoticement));
	if (!pkt) return;
	client_size = pkt->value;

	if (client_size != m_notice_data_size) {
		cp = new char[m_notice_data_size + 2 + sizeof(hb::net::PacketHeader)];
		std::memset(cp, 0, m_notice_data_size + 2 + sizeof(hb::net::PacketHeader));
		memcpy((cp + sizeof(hb::net::PacketHeader)), m_notice_data, m_notice_data_size);

		{
			auto* header = reinterpret_cast<hb::net::PacketResponseNoticementHeader*>(cp);
			header->header.msg_id = MsgId::ResponseNoticement;
			header->header.msg_type = MsgType::Reject;
		}

		ret = m_client_list[client_h]->m_socket->send_msg(cp, m_notice_data_size + 2 + sizeof(hb::net::PacketHeader));

		delete cp;
	}
	else {
		hb::net::PacketResponseNoticementHeader pkt{};
		pkt.header.msg_id = MsgId::ResponseNoticement;
		pkt.header.msg_type = MsgType::Confirm;
		ret = m_client_list[client_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}
}

void CGame::request_noticement_handler(int client_h)
{
	if (m_client_list[client_h] == 0) return;

	FILE* noti_file = fopen("gameconfigs/noticement.txt", "rb");
	if (!noti_file) return;
	fseek(noti_file, 0, SEEK_END);
	uint32_t file_size = static_cast<uint32_t>(ftell(noti_file));
	fseek(noti_file, 0, SEEK_SET);

	std::memset(G_cData50000, 0, sizeof(G_cData50000));

	if (fread(G_cData50000 + sizeof(hb::net::PacketHeader), 1, file_size, noti_file) != file_size)
		hb::logger::warn("Short read on noticement file");
	fclose(noti_file);

	{
		auto* header = reinterpret_cast<hb::net::PacketResponseNoticementHeader*>(G_cData50000);
		header->header.msg_id = MsgId::ResponseNoticement;
		header->header.msg_type = MsgType::Confirm;
	}

	int ret = m_client_list[client_h]->m_socket->send_msg(G_cData50000, file_size + 2 + sizeof(hb::net::PacketHeader));

	switch (ret) {
	case sock::Event::QueueFull:
	case sock::Event::SocketError:
	case sock::Event::CriticalError:
	case sock::Event::SocketClosed:
		return;
	}
}

void CGame::request_check_account_password_handler(char* data, size_t msg_size)
{
	int level;
	char account_name[11], account_password[hb::shared::limits::AccountPassLen];

	const auto* header = hb::net::PacketCast<hb::net::PacketHeader>(data, sizeof(hb::net::PacketHeader));
	if (!header) return;

	const auto& payload = *reinterpret_cast<const hb::net::TestLogPayload*>(data + sizeof(hb::net::PacketHeader));

	std::memset(account_name, 0, sizeof(account_name));
	std::memset(account_password, 0, sizeof(account_password));

	std::memcpy(account_name, payload.account_name, hb::shared::limits::AccountNameLen - 1);
	std::memcpy(account_password, payload.account_password, hb::shared::limits::AccountPassLen - 1);
	level = payload.level;

	for(int i = 0; i < MaxClients; i++)
		if ((m_client_list[i] != 0) && (hb_stricmp(m_client_list[i]->m_account_name, account_name) == 0)) {
			if ((strcmp(m_client_list[i]->m_account_password, account_password) != 0) || (m_client_list[i]->m_level != level)) {
				hb::logger::log("Account '{}' level {}: password or level mismatch, disconnecting", account_name, level);
				delete_client(i, false, true);
				return;
			}
		}
}

int CGame::request_panning_map_data_request(int client_h, char* data)
{
	direction dir;
	char mapData[3000];
	short dX, dY;
	int   ret, size;

	if (m_client_list[client_h] == 0) return 0;
	if (m_client_list[client_h]->m_is_observer_mode == false) return 0;
	if (m_client_list[client_h]->m_is_killed) return 0;
	if (m_client_list[client_h]->m_is_init_complete == false) return 0;

	dX = m_client_list[client_h]->m_x;
	dY = m_client_list[client_h]->m_y;

	const auto* req = hb::net::PacketCast<hb::net::PacketRequestPanning>(data, sizeof(hb::net::PacketRequestPanning));
	if (!req) return 0;
	dir = static_cast<direction>(req->dir);
	if ((dir <= 0) || (dir > 8)) return 0;

	hb::shared::direction::ApplyOffset(dir, dX, dY);

	m_client_list[client_h]->m_x = dX;
	m_client_list[client_h]->m_y = dY;
	m_client_list[client_h]->m_dir = dir;

	size = compose_move_map_data((short)(dX - hb::shared::view::CenterX), (short)(dY - hb::shared::view::CenterY), client_h, dir, mapData);

	hb::net::PacketWriter writer;
	writer.Reserve(sizeof(hb::net::PacketResponsePanningHeader) + size);

	auto* pkt = writer.Append<hb::net::PacketResponsePanningHeader>();
	pkt->header.msg_id = MsgId::ResponsePanning;
	pkt->header.msg_type = Confirm::MoveConfirm;
	pkt->x = static_cast<int16_t>(dX - hb::shared::view::CenterX);
	pkt->y = static_cast<int16_t>(dY - hb::shared::view::CenterY);
	pkt->dir = static_cast<uint8_t>(dir);

	writer.AppendBytes(mapData, size);

	ret = m_client_list[client_h]->m_socket->send_msg(writer.Data(), static_cast<int>(writer.size()));
	switch (ret) {
	case sock::Event::QueueFull:
	case sock::Event::SocketError:
	case sock::Event::CriticalError:
	case sock::Event::SocketClosed:
		delete_client(client_h, true, true);
		return 0;
	}

	return 1;
}

void CGame::request_restart_handler(int client_h)
{
	char  tmp_map[32];

	if (m_client_list[client_h] == 0) return;

	if (m_client_list[client_h]->m_is_killed) {

		strcpy(tmp_map, m_client_list[client_h]->m_map_name);
		std::memset(m_client_list[client_h]->m_map_name, 0, sizeof(m_client_list[client_h]->m_map_name));

		if (strcmp(m_client_list[client_h]->m_location, "NONE") == 0) {
			// default .
			strcpy(m_client_list[client_h]->m_map_name, "default");
		}
		else {
			if ((strcmp(m_client_list[client_h]->m_location, "aresden") == 0) || (strcmp(m_client_list[client_h]->m_location, "arehunter") == 0)) {
				if (m_is_crusade_mode) {
					if (m_client_list[client_h]->m_dead_penalty_time > 0) {
						std::memset(m_client_list[client_h]->m_locked_map_name, 0, sizeof(m_client_list[client_h]->m_locked_map_name));
						strcpy(m_client_list[client_h]->m_locked_map_name, "aresden");
						m_client_list[client_h]->m_locked_map_time = 60 * 5;
						m_client_list[client_h]->m_dead_penalty_time = 60 * 10; // v2.04
					}
					else {
						memcpy(m_client_list[client_h]->m_map_name, "resurr1", 7);
						m_client_list[client_h]->m_dead_penalty_time = 60 * 10;
					}
				}
				// v2.16 2002-5-31
				if (strcmp(tmp_map, "elvine") == 0) {
					memcpy(m_client_list[client_h]->m_map_name, "elvjail", 7);
					strcpy(m_client_list[client_h]->m_locked_map_name, "elvjail");
					m_client_list[client_h]->m_locked_map_time = 60 * 3;
				}
				else if (m_client_list[client_h]->m_level > 80)
					memcpy(m_client_list[client_h]->m_map_name, "resurr1", 7);
				else memcpy(m_client_list[client_h]->m_map_name, "arefarm", 7);
			}
			else {
				if (m_is_crusade_mode) {
					if (m_client_list[client_h]->m_dead_penalty_time > 0) {
						std::memset(m_client_list[client_h]->m_locked_map_name, 0, sizeof(m_client_list[client_h]->m_locked_map_name));
						strcpy(m_client_list[client_h]->m_locked_map_name, "elvine");
						m_client_list[client_h]->m_locked_map_time = 60 * 5;
						m_client_list[client_h]->m_dead_penalty_time = 60 * 10; // v2.04
					}
					else {
						memcpy(m_client_list[client_h]->m_map_name, "resurr2", 7);
						m_client_list[client_h]->m_dead_penalty_time = 60 * 10;
					}
				}
				if (strcmp(tmp_map, "aresden") == 0) {
					memcpy(m_client_list[client_h]->m_map_name, "arejail", 7);
					strcpy(m_client_list[client_h]->m_locked_map_name, "arejail");
					m_client_list[client_h]->m_locked_map_time = 60 * 3;

				}
				else if (m_client_list[client_h]->m_level > 80)
					memcpy(m_client_list[client_h]->m_map_name, "resurr2", 7);
				else memcpy(m_client_list[client_h]->m_map_name, "elvfarm", 7);
			}
		}

		m_client_list[client_h]->m_is_killed = false;
		m_client_list[client_h]->m_hp = get_max_hp(client_h);
		m_client_list[client_h]->m_hunger_status = 100;

		std::memset(tmp_map, 0, sizeof(tmp_map));
		strcpy(tmp_map, m_client_list[client_h]->m_map_name);
		// !!! request_teleport_handler m_map_name
		request_teleport_handler(client_h, "2   ", tmp_map, -1, -1);
	}
}

void CGame::request_shop_contents_handler(int client_h, char* data)
{
	if (m_client_list[client_h] == 0) return;
	if (!m_is_shop_data_available) {
		// No shop data configured
		return;
	}

	const auto* req = hb::net::PacketCast<hb::net::PacketShopRequest>(data, sizeof(hb::net::PacketShopRequest));
	if (!req) return;

	int16_t npc_config_id = req->npcConfigId;

	// Look up shop ID for this NPC config ID
	auto mappingIt = m_npc_shop_mappings.find(static_cast<int>(npc_config_id));
	if (mappingIt == m_npc_shop_mappings.end()) {
		// No shop configured for this NPC config ID
		hb::logger::log("Shop request for NPC config {} - no shop mapping found", npc_config_id);
		return;
	}

	int shop_id = mappingIt->second;

	// get shop data
	auto shopIt = m_shop_data.find(shop_id);
	if (shopIt == m_shop_data.end() || shopIt->second.item_ids.empty()) {
		// Shop exists in mapping but has no items
		hb::logger::log("Shop request for NPC config {}, shop {} - no items found", npc_config_id, shop_id);
		return;
	}

	const ShopData& shop = shopIt->second;
	uint16_t itemCount = static_cast<uint16_t>(shop.item_ids.size());
	if (itemCount > hb::net::MAX_SHOP_ITEMS) {
		itemCount = hb::net::MAX_SHOP_ITEMS;
	}

	// Build response packet
	// Header + array of int16_t item IDs
	size_t packetSize = sizeof(hb::net::PacketShopResponseHeader) + (itemCount * sizeof(int16_t));
	char* resp_data = new char[packetSize];
	std::memset(resp_data, 0, packetSize);

	auto* resp = reinterpret_cast<hb::net::PacketShopResponseHeader*>(resp_data);
	resp->header.msg_id = MSGID_RESPONSE_SHOP_CONTENTS;
	resp->header.msg_type = MsgType::Confirm;
	resp->npcConfigId = npc_config_id;
	resp->shopId = static_cast<int16_t>(shop_id);
	resp->itemCount = itemCount;

	// Copy item IDs after header
	int16_t* item_ids = reinterpret_cast<int16_t*>(resp_data + sizeof(hb::net::PacketShopResponseHeader));
	for (uint16_t i = 0; i < itemCount; i++) {
		item_ids[i] = shop.item_ids[i];
	}

	// Send to client
	m_client_list[client_h]->m_socket->send_msg(resp_data, static_cast<uint32_t>(packetSize));

	hb::logger::log("Sent shop contents: NPC config {}, shop {}, {} items", npc_config_id, shop_id, itemCount);

	delete[] resp_data;
}

void CGame::join_party_handler(int client_h, int v1, const char* member_name)
{
	char data[120]{};

	if (m_client_list[client_h] == 0) return;

	switch (v1) {
	case 0:
		request_delete_party_handler(client_h);
		break;

	case 1:
		//testcode
		hb::logger::log("Party join request: player={}({}) party={} status={} target={} name={}", m_client_list[client_h]->m_char_name, client_h, m_client_list[client_h]->m_party_id, m_client_list[client_h]->m_party_status, m_client_list[client_h]->m_req_join_party_client_h, m_client_list[client_h]->m_req_join_party_name);

		if ((m_client_list[client_h]->m_party_id != 0) || (m_client_list[client_h]->m_party_status != PartyStatus::Null)) {
			send_notify_msg(0, client_h, Notify::Party, 7, 0, 0, 0);
			m_client_list[client_h]->m_req_join_party_client_h = 0;
			std::memset(m_client_list[client_h]->m_req_join_party_name, 0, sizeof(m_client_list[client_h]->m_req_join_party_name));
			m_client_list[client_h]->m_party_status = PartyStatus::Null;
			//testcode
			hb::logger::log("Party join rejected (reason 1)");
			return;
		}

		for(int i = 1; i < MaxClients; i++)
			if ((m_client_list[i] != 0) && (hb_stricmp(m_client_list[i]->m_char_name, member_name) == 0)) {
				if (m_client_list[i]->m_appearance.is_walking) {
					send_notify_msg(0, client_h, Notify::Party, 7, 0, 0, 0);
					//testcode
					hb::logger::log("Party join rejected (reason 2)");
				}
				else if (m_client_list[i]->m_side != m_client_list[client_h]->m_side) {
					send_notify_msg(0, client_h, Notify::Party, 7, 0, 0, 0);
					//testcode
					hb::logger::log("Party join rejected (reason 3)");
				}
				else if (m_client_list[i]->m_party_status == PartyStatus::Processing) {
					send_notify_msg(0, client_h, Notify::Party, 7, 0, 0, 0);
					//testcode
					hb::logger::log("Party join rejected (reason 4)");
					//testcode
					hb::logger::log("Party join rejected: client={} party={} name={}", i, m_client_list[i]->m_party_id, m_client_list[i]->m_req_join_party_name);

					m_client_list[client_h]->m_req_join_party_client_h = 0;
					std::memset(m_client_list[client_h]->m_req_join_party_name, 0, sizeof(m_client_list[client_h]->m_req_join_party_name));
					m_client_list[client_h]->m_party_status = PartyStatus::Null;
				}
				else {
					m_client_list[i]->m_req_join_party_client_h = client_h;
					std::memset(m_client_list[i]->m_req_join_party_name, 0, sizeof(m_client_list[i]->m_req_join_party_name));
					strcpy(m_client_list[i]->m_req_join_party_name, m_client_list[client_h]->m_char_name);
					send_notify_msg(0, i, Notify::QueryJoinParty, 0, 0, 0, m_client_list[client_h]->m_char_name);

					m_client_list[client_h]->m_req_join_party_client_h = i;
					std::memset(m_client_list[client_h]->m_req_join_party_name, 0, sizeof(m_client_list[client_h]->m_req_join_party_name));
					strcpy(m_client_list[client_h]->m_req_join_party_name, m_client_list[i]->m_char_name);
					m_client_list[client_h]->m_party_status = PartyStatus::Processing;
				}
				return;
			}
		break;

	case 2:
		if (m_client_list[client_h]->m_party_status == PartyStatus::Confirm) {
			std::memset(data, 0, sizeof(data));
			hb::net::PartyOpPayloadWithId partyOp{};
			partyOp.msg_id = ServerMsgId::party_operation;
			partyOp.op_type = 6;
			partyOp.client_h = static_cast<uint16_t>(client_h);
			std::memcpy(partyOp.name, m_client_list[client_h]->m_char_name, sizeof(partyOp.name));
			partyOp.party_id = static_cast<uint16_t>(m_client_list[client_h]->m_party_id);
			std::memcpy(data, &partyOp, sizeof(partyOp));
			party_operation(data);
		}
		break;
	}
}

void CGame::activate_special_ability_handler(int client_h)
{
	uint32_t time = GameClock::GetTimeMS();

	if (m_client_list[client_h] == 0) return;
	if (m_client_list[client_h]->m_special_ability_time != 0) return;
	if (m_client_list[client_h]->m_special_ability_type == 0) return;
	if (m_client_list[client_h]->m_is_special_ability_enabled) return;

	m_client_list[client_h]->m_is_special_ability_enabled = true;
	m_client_list[client_h]->m_special_ability_start_time = time;

	m_client_list[client_h]->m_special_ability_time = SpecialAbilityTimeSec;

	switch (m_client_list[client_h]->m_special_ability_type) {
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
		m_client_list[client_h]->m_appearance.effect_type = 1;
		break;
	case 50:
	case 51:
	case 52:
	case 53:
	case 54:
		m_client_list[client_h]->m_appearance.effect_type = 2;
		break;
	}

	send_notify_msg(0, client_h, Notify::SpecialAbilityStatus, 1, m_client_list[client_h]->m_special_ability_type, m_client_list[client_h]->m_special_ability_last_sec, 0);
	send_event_to_near_client_type_a(client_h, hb::shared::owner_class::Player, MsgId::EventMotion, Type::NullAction, 0, 0, 0);
}

void CGame::update_map_sector_info()
{
	int max_neutral_activity, max_aresden_activity, max_elvine_activity, max_monster_activity, max_player_activity;

	for(int i = 0; i < MaxMaps; i++)
		if (m_map_list[i] != 0) {

			max_neutral_activity = max_aresden_activity = max_elvine_activity = max_monster_activity = max_player_activity = 0;
			m_map_list[i]->m_top_neutral_sector_x = m_map_list[i]->m_top_neutral_sector_y = m_map_list[i]->m_top_aresden_sector_x = m_map_list[i]->m_top_aresden_sector_y = 0;
			m_map_list[i]->m_top_elvine_sector_x = m_map_list[i]->m_top_elvine_sector_y = m_map_list[i]->m_top_monster_sector_x = m_map_list[i]->m_top_monster_sector_y = 0;
			m_map_list[i]->m_top_player_sector_x = m_map_list[i]->m_top_player_sector_y = 0;

			// TempSectorInfo   SectorInfo   TempSectorInfo .
			for(int ix = 0; ix < smap::MaxSectors; ix++)
				for(int iy = 0; iy < smap::MaxSectors; iy++) {
					if (m_map_list[i]->m_temp_sector_info[ix][iy].neutral_activity > max_neutral_activity) {
						max_neutral_activity = m_map_list[i]->m_temp_sector_info[ix][iy].neutral_activity;
						m_map_list[i]->m_top_neutral_sector_x = ix;
						m_map_list[i]->m_top_neutral_sector_y = iy;
					}

					if (m_map_list[i]->m_temp_sector_info[ix][iy].aresden_activity > max_aresden_activity) {
						max_aresden_activity = m_map_list[i]->m_temp_sector_info[ix][iy].aresden_activity;
						m_map_list[i]->m_top_aresden_sector_x = ix;
						m_map_list[i]->m_top_aresden_sector_y = iy;
					}

					if (m_map_list[i]->m_temp_sector_info[ix][iy].elvine_activity > max_elvine_activity) {
						max_elvine_activity = m_map_list[i]->m_temp_sector_info[ix][iy].elvine_activity;
						m_map_list[i]->m_top_elvine_sector_x = ix;
						m_map_list[i]->m_top_elvine_sector_y = iy;
					}

					if (m_map_list[i]->m_temp_sector_info[ix][iy].monster_activity > max_monster_activity) {
						max_monster_activity = m_map_list[i]->m_temp_sector_info[ix][iy].monster_activity;
						m_map_list[i]->m_top_monster_sector_x = ix;
						m_map_list[i]->m_top_monster_sector_y = iy;
					}

					if (m_map_list[i]->m_temp_sector_info[ix][iy].player_activity > max_player_activity) {
						max_player_activity = m_map_list[i]->m_temp_sector_info[ix][iy].player_activity;
						m_map_list[i]->m_top_player_sector_x = ix;
						m_map_list[i]->m_top_player_sector_y = iy;
					}
				}

			// TempSectorInfo .
			m_map_list[i]->clear_temp_sector_info();

			// Sector Info
			if (m_map_list[i]->m_top_neutral_sector_x > 0) m_map_list[i]->m_sector_info[m_map_list[i]->m_top_neutral_sector_x][m_map_list[i]->m_top_neutral_sector_y].neutral_activity++;
			if (m_map_list[i]->m_top_aresden_sector_x > 0) m_map_list[i]->m_sector_info[m_map_list[i]->m_top_aresden_sector_x][m_map_list[i]->m_top_aresden_sector_y].aresden_activity++;
			if (m_map_list[i]->m_top_elvine_sector_x > 0) m_map_list[i]->m_sector_info[m_map_list[i]->m_top_elvine_sector_x][m_map_list[i]->m_top_elvine_sector_y].elvine_activity++;
			if (m_map_list[i]->m_top_monster_sector_x > 0) m_map_list[i]->m_sector_info[m_map_list[i]->m_top_monster_sector_x][m_map_list[i]->m_top_monster_sector_y].monster_activity++;
			if (m_map_list[i]->m_top_player_sector_x > 0) m_map_list[i]->m_sector_info[m_map_list[i]->m_top_player_sector_x][m_map_list[i]->m_top_player_sector_y].player_activity++;
		}
}

void CGame::aging_map_sector_info()
{
	for(int i = 0; i < MaxMaps; i++)
		if (m_map_list[i] != 0) {
			for(int ix = 0; ix < smap::MaxSectors; ix++)
				for(int iy = 0; iy < smap::MaxSectors; iy++) {
					m_map_list[i]->m_sector_info[ix][iy].neutral_activity--;
					m_map_list[i]->m_sector_info[ix][iy].aresden_activity--;
					m_map_list[i]->m_sector_info[ix][iy].elvine_activity--;
					m_map_list[i]->m_sector_info[ix][iy].monster_activity--;
					m_map_list[i]->m_sector_info[ix][iy].player_activity--;

					if (m_map_list[i]->m_sector_info[ix][iy].neutral_activity < 0) m_map_list[i]->m_sector_info[ix][iy].neutral_activity = 0;
					if (m_map_list[i]->m_sector_info[ix][iy].aresden_activity < 0) m_map_list[i]->m_sector_info[ix][iy].aresden_activity = 0;
					if (m_map_list[i]->m_sector_info[ix][iy].elvine_activity < 0) m_map_list[i]->m_sector_info[ix][iy].elvine_activity = 0;
					if (m_map_list[i]->m_sector_info[ix][iy].monster_activity < 0) m_map_list[i]->m_sector_info[ix][iy].monster_activity = 0;
					if (m_map_list[i]->m_sector_info[ix][iy].player_activity < 0) m_map_list[i]->m_sector_info[ix][iy].player_activity = 0;
				}
		}
}

// New 14/05/2004 Changed

void CGame::check_connection_handler(int client_h, char* data, bool already_responded)
{
	uint32_t time_rcv, time, time_gap_client, time_gap_server;

	if (m_client_list[client_h] == 0) return;

	time = GameClock::GetTimeMS();
	const auto* req = hb::net::PacketCast<hb::net::PacketCommandCheckConnection>(data, sizeof(hb::net::PacketCommandCheckConnection));
	if (!req) return;
	time_rcv = req->time_ms;

	if (m_client_list[client_h]->m_initial_check_time_received == 0) {
		m_client_list[client_h]->m_initial_check_time_received = time_rcv;
		m_client_list[client_h]->m_initial_check_time = time;
	}
	else {
		time_gap_client = (time_rcv - m_client_list[client_h]->m_initial_check_time_received);
		time_gap_server = (time - m_client_list[client_h]->m_initial_check_time);

		if (time_gap_client < time_gap_server) return;
		if ((time_gap_client - time_gap_server) >= (uint32_t)m_client_timeout) {
			delete_client(client_h, true, true);
			return;
		}
	}

	if (!already_responded) {
		hb::net::PacketCommandCheckConnection resp{};
		resp.header.msg_id = MsgId::CommandCheckConnection;
		resp.header.msg_type = MsgType::Confirm;
		resp.time_ms = time_rcv;
		m_client_list[client_h]->m_socket->send_msg(reinterpret_cast<char*>(&resp), sizeof(resp));
	}

	// Client version check — warn outdated clients every 5 minutes
	if (req->client_major != 0 || req->client_minor != 0 || req->client_patch != 0 || req->client_build != 0) {
		uint64_t client_ver = static_cast<uint64_t>(req->client_major) * 10000000
			+ static_cast<uint64_t>(req->client_minor) * 100000
			+ static_cast<uint64_t>(req->client_patch) * 1000
			+ req->client_build;
		uint64_t expected_ver = static_cast<uint64_t>(hb::version::client::major) * 10000000
			+ static_cast<uint64_t>(hb::version::client::minor) * 100000
			+ static_cast<uint64_t>(hb::version::client::patch) * 1000
			+ hb::version::client::build_number;
		if (client_ver < expected_ver) {
			if (time - m_client_list[client_h]->m_last_version_warning_time >= 300000) {
				m_client_list[client_h]->m_last_version_warning_time = time;
				char msg[128];
				std::snprintf(msg, sizeof(msg),
					"Your client (%d.%d.%d.%d) is outdated. Expected: %d.%d.%d.%d. Please relaunch to update.",
					req->client_major, req->client_minor, req->client_patch, req->client_build,
					hb::version::client::major, hb::version::client::minor,
					hb::version::client::patch, hb::version::client::build_number);
				send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0, msg);
			}
		}
	}
}

void CGame::request_help_handler(int client_h)
{
	

	if (m_client_list[client_h] == 0) return;
	if (m_client_list[client_h]->m_crusade_duty != 1) return;

	// Guild system removed - help request finds any commander on the same side
	for(int i = 1; i < MaxClients; i++)
		if ((m_client_list[i] != 0) &&
			(m_client_list[i]->m_crusade_duty == 3) && (m_client_list[i]->m_side == m_client_list[client_h]->m_side)) {
			send_notify_msg(0, i, Notify::Help, m_client_list[client_h]->m_x, m_client_list[client_h]->m_y, m_client_list[client_h]->m_hp, m_client_list[client_h]->m_char_name);
			return;
		}

	send_notify_msg(0, client_h, Notify::HelpFailed, 0, 0, 0, 0);
}

bool CGame::add_client_short_cut(int client_h)
{
	

	for(int i = 0; i < MaxClients; i++)
		if (m_client_shortcut[i] == client_h) return false;

	for(int i = 0; i < MaxClients; i++)
		if (m_client_shortcut[i] == 0) {
			m_client_shortcut[i] = client_h;
			return true;
		}

	return false;
}

void CGame::remove_client_short_cut(int client_h)
{
	

	for(int i = 0; i < MaxClients + 1; i++)
		if (m_client_shortcut[i] == client_h) {
			m_client_shortcut[i] = 0;
			break;
		}

	//m_client_shortcut[i] = m_client_shortcut[m_total_clients+1];
	//m_client_shortcut[m_total_clients+1] = 0;
	for(int i = 0; i < MaxClients; i++)
		if ((m_client_shortcut[i] == 0) && (m_client_shortcut[i + 1] != 0)) {
			m_client_shortcut[i] = m_client_shortcut[i + 1];
			m_client_shortcut[i + 1] = 0;
		}
}

int CGame::get_map_location_side(char* map_name)
{

	if (strcmp(map_name, "aresden") == 0) return 3;
	if (strcmp(map_name, "elvine") == 0) return 4;
	if (strcmp(map_name, "arebrk11") == 0) return 3;
	if (strcmp(map_name, "elvbrk11") == 0) return 4;

	if (strcmp(map_name, "cityhall_1") == 0) return 1;
	if (strcmp(map_name, "cityhall_2") == 0) return 2;
	if (strcmp(map_name, "cath_1") == 0) return 1;
	if (strcmp(map_name, "cath_2") == 0) return 2;
	if (strcmp(map_name, "gshop_1") == 0) return 1;
	if (strcmp(map_name, "gshop_2") == 0) return 2;
	if (strcmp(map_name, "bsmith_1") == 0) return 1;
	if (strcmp(map_name, "bsmith_2") == 0) return 2;
	if (strcmp(map_name, "wrhus_1") == 0) return 1;
	if (strcmp(map_name, "wrhus_2") == 0) return 2;
	if (strcmp(map_name, "gldhall_1") == 0) return 1;
	if (strcmp(map_name, "gldhall_2") == 0) return 2;
	if (strcmp(map_name, "wzdtwr_1") == 0) return 1;
	if (strcmp(map_name, "wzdtwr_2") == 0) return 2;
	if (strcmp(map_name, "arefarm") == 0) return 1;
	if (strcmp(map_name, "elvfarm") == 0) return 2;
	if (strcmp(map_name, "arewrhus") == 0) return 1;
	if (strcmp(map_name, "elvwrhus") == 0) return 2;
	if (strcmp(map_name, "cmdhall_1") == 0) return 1;
	if (strcmp(map_name, "Cmdhall_2") == 0) return 2;

	return 0;
}

//ArchAngel Function

void CGame::request_change_play_mode(int client_h)
{

	if (m_client_list[client_h] == 0) return;
	if (m_client_list[client_h]->m_player_kill_count > 0) return;
	if (m_client_list[client_h]->m_is_init_complete == false) return;

	if (memcmp(m_client_list[client_h]->m_map_name, "cityhall", 8) != 0) return;

	if (m_client_list[client_h]->m_level < 100 ||
		m_client_list[client_h]->m_is_player_civil) {
		if (memcmp(m_client_list[client_h]->m_location, "aresden", 7) == 0) strcpy(m_client_list[client_h]->m_location, "arehunter");
		else if (memcmp(m_client_list[client_h]->m_location, "elvine", 6) == 0) strcpy(m_client_list[client_h]->m_location, "elvhunter");
		else if (memcmp(m_client_list[client_h]->m_location, "arehunter", 9) == 0) strcpy(m_client_list[client_h]->m_location, "aresden");
		else if (memcmp(m_client_list[client_h]->m_location, "elvhunter", 9) == 0) strcpy(m_client_list[client_h]->m_location, "elvine");

		if (m_client_list[client_h]->m_is_player_civil)
			m_client_list[client_h]->m_is_player_civil = false;
		else m_client_list[client_h]->m_is_player_civil = true;

		send_notify_msg(0, client_h, Notify::ChangePlayMode, 0, 0, 0, m_client_list[client_h]->m_location);
		send_event_to_near_client_type_a(client_h, hb::shared::owner_class::Player, MsgId::EventMotion, 100, 0, 0, 0);
	}

	g_login->local_save_player_data(client_h);
}

/*********************************************************************************************************************
**  void CGame::set_invisibility_flag(short owner_h, char owner_type, bool status)									**
**  description			:: changes the status of the player to show invisibility aura								**
**  last updated		:: November 20, 2004; 9:30 PM; Hypnotoad													**
**	return value		:: void																						**
*********************************************************************************************************************/

/*********************************************************************************************************************
**  void CGame::set_inhibition_casting_flag(short owner_h, char owner_type, bool status)								**
**  description			:: changes the status of the player to show inhibit casting aura							**
**  last updated		:: November 20, 2004; 9:33 PM; Hypnotoad													**
**	return value		:: void																						**
*********************************************************************************************************************/

/*********************************************************************************************************************
**  void void CGame::set_berserk_flag(short owner_h, char owner_type, bool status)									**
**  description			:: changes the status of the player to show berserk aura									**
**  last updated		:: November 20, 2004; 9:35 PM; Hypnotoad													**
**	return value		:: void																						**
*********************************************************************************************************************/

/*********************************************************************************************************************
**  void void CGame::set_ice_flag(short owner_h, char owner_type, bool status)										**
**  description			:: changes the status of the player to show frozen aura										**
**  last updated		:: November 20, 2004; 9:35 PM; Hypnotoad													**
**	return value		:: void																						**
*********************************************************************************************************************/

/*********************************************************************************************************************
**  void void CGame::set_poison_flag(short owner_h, char owner_type, bool status)									**
**  description			:: changes the status of the player to show poison aura										**
**  last updated		:: November 20, 2004; 9:36 PM; Hypnotoad													**
**	return value		:: void																						**
*********************************************************************************************************************/

/*********************************************************************************************************************
**  void void CGame::set_illusion_flag(short owner_h, char owner_type, bool status)									**
**  description			:: changes the status of the player to show illusion aura									**
**  last updated		:: November 20, 2004; 9:36 PM; Hypnotoad													**
**	return value		:: void																						**
*********************************************************************************************************************/

/*********************************************************************************************************************
**  void void CGame::set_hero_flag(short owner_h, char owner_type, bool status)										**
**  description			:: changes the status of the player to show hero item aura									**
**  last updated		:: November 20, 2004; 9:37 PM; Hypnotoad													**
**	return value		:: void																						**
*********************************************************************************************************************/
int CGame::find_admin_by_account(const char* account_name)
{
	if (account_name == nullptr) return -1;
	for (int i = 0; i < m_admin_count; i++) {
		if (hb_stricmp(m_admin_list[i].m_account_name, account_name) == 0)
			return i;
	}
	return -1;
}

int CGame::find_admin_by_char_name(const char* charName)
{
	if (charName == nullptr) return -1;
	for (int i = 0; i < m_admin_count; i++) {
		if (hb_stricmp(m_admin_list[i].m_char_name, charName) == 0)
			return i;
	}
	return -1;
}

bool CGame::is_client_admin(int client_h)
{
	if (client_h <= 0 || client_h >= MaxClients) return false;
	if (m_client_list[client_h] == nullptr) return false;
	return m_client_list[client_h]->m_admin_index != -1;
}

int CGame::get_command_required_level(const char* cmdName) const
{
	if (cmdName == nullptr) return hb::shared::admin::Administrator;
	auto it = m_command_permissions.find(cmdName);
	if (it != m_command_permissions.end())
		return it->second.admin_level;
	return hb::shared::admin::Administrator;
}

int CGame::find_client_by_name(const char* name) const
{
	if (name == nullptr) return 0;
	for (int i = 1; i < MaxClients; i++)
	{
		if (m_client_list[i] != nullptr && m_client_list[i]->m_is_init_complete)
		{
			if (hb_strnicmp(m_client_list[i]->m_char_name, name, hb::shared::limits::CharNameLen - 1) == 0)
				return i;
		}
	}
	return 0;
}

bool CGame::gm_teleport_to(int client_h, const char* dest_map, short dest_x, short dest_y)
{
	if (m_client_list[client_h] == nullptr) return false;
	if (dest_map == nullptr) return false;

	// Find destination map index
	int dest_map_index = -1;
	for (int i = 0; i < MaxMaps; i++)
	{
		if (m_map_list[i] != nullptr && memcmp(m_map_list[i]->m_name, dest_map, 10) == 0)
		{
			dest_map_index = i;
			break;
		}
	}
	if (dest_map_index == -1) return false;

	// Remove from current location
	m_combat_manager->remove_from_target(client_h, hb::shared::owner_class::Player);
	m_map_list[m_client_list[client_h]->m_map_index]->clear_owner(13, client_h, hb::shared::owner_class::Player,
		m_client_list[client_h]->m_x, m_client_list[client_h]->m_y);
	send_event_to_near_client_type_a(client_h, hb::shared::owner_class::Player, MsgId::EventLog, MsgType::Reject, 0, 0, 0);

	// Update position and map
	m_client_list[client_h]->m_x = dest_x;
	m_client_list[client_h]->m_y = dest_y;
	m_client_list[client_h]->m_map_index = static_cast<char>(dest_map_index);
	std::memset(m_client_list[client_h]->m_map_name, 0, sizeof(m_client_list[client_h]->m_map_name));
	memcpy(m_client_list[client_h]->m_map_name, m_map_list[dest_map_index]->m_name, 10);

	// Always send full INITDATA — same pattern as request_teleport_handler RTH_NEXTSTEP.
	// Even same-map teleports need INITDATA so the client reinitializes its view.
	set_playing_status(client_h);
	// Set faction/identity status fields from player data
	m_client_list[client_h]->m_status.pk = (m_client_list[client_h]->m_player_kill_count != 0) ? 1 : 0;
	m_client_list[client_h]->m_status.citizen = (m_client_list[client_h]->m_side != 0) ? 1 : 0;
	m_client_list[client_h]->m_status.aresden = (m_client_list[client_h]->m_side == 1) ? 1 : 0;
	m_client_list[client_h]->m_status.hunter = m_client_list[client_h]->m_is_player_civil ? 1 : 0;

	hb::net::PacketWriter writer;
	char initMapData[hb::shared::limits::MsgBufferSize + 1];

	writer.Reset();
	auto* init_header = writer.Append<hb::net::PacketResponseInitDataHeader>();
	init_header->header.msg_id = MsgId::ResponseInitData;
	init_header->header.msg_type = MsgType::Confirm;

	get_empty_position(&m_client_list[client_h]->m_x, &m_client_list[client_h]->m_y, m_client_list[client_h]->m_map_index);

	init_header->player_object_id = static_cast<std::int16_t>(client_h);
	init_header->pivot_x = static_cast<std::int16_t>(m_client_list[client_h]->m_x - hb::shared::view::PlayerPivotOffsetX);
	init_header->pivot_y = static_cast<std::int16_t>(m_client_list[client_h]->m_y - hb::shared::view::PlayerPivotOffsetY);
	init_header->player_type = m_client_list[client_h]->m_type;
	init_header->appearance = build_broadcast_appearance(client_h);
	init_header->status = m_client_list[client_h]->m_status;
	std::memcpy(init_header->map_name, m_client_list[client_h]->m_map_name, sizeof(init_header->map_name));
	std::memcpy(init_header->cur_location, m_map_list[m_client_list[client_h]->m_map_index]->m_location_name, sizeof(init_header->cur_location));

	if (m_map_list[m_client_list[client_h]->m_map_index]->m_is_fixed_day_mode)
		init_header->sprite_alpha = 1;
	else init_header->sprite_alpha = m_day_or_night;

	if (m_map_list[m_client_list[client_h]->m_map_index]->m_is_fixed_day_mode)
		init_header->weather_status = 0;
	else init_header->weather_status = m_map_list[m_client_list[client_h]->m_map_index]->m_weather_status;

	init_header->contribution = m_client_list[client_h]->m_contribution;

	m_map_list[m_client_list[client_h]->m_map_index]->set_owner(client_h,
		hb::shared::owner_class::Player,
		m_client_list[client_h]->m_x,
		m_client_list[client_h]->m_y);

	init_header->observer_mode = static_cast<std::uint8_t>(m_client_list[client_h]->m_is_observer_mode);
	init_header->rating = m_client_list[client_h]->m_rating;
	init_header->hp = m_client_list[client_h]->m_hp;
	init_header->discount = 0;

	int size = compose_init_map_data(m_client_list[client_h]->m_x - hb::shared::view::CenterX, m_client_list[client_h]->m_y - hb::shared::view::CenterY, client_h, initMapData);
	writer.AppendBytes(initMapData, static_cast<std::size_t>(size));

	int ret = m_client_list[client_h]->m_socket->send_msg(writer.Data(), static_cast<int>(writer.size()));
	if (ret == sock::Event::QueueFull || ret == sock::Event::SocketError ||
		ret == sock::Event::CriticalError || ret == sock::Event::SocketClosed)
	{
		delete_client(client_h, true, true);
		return false;
	}

	send_event_to_near_client_type_a(client_h, hb::shared::owner_class::Player, MsgId::EventLog, MsgType::Confirm, 0, 0, 0);

	return true;
}

/*********************************************************************************************************************
**  void void CGame::set_defense_shield_flag(short owner_h, char owner_type, bool status)								**
**  description			:: changes the status of the player to show defense aura									**
**  last updated		:: November 20, 2004; 9:37 PM; Hypnotoad													**
**	return value		:: void																						**
*********************************************************************************************************************/

/*********************************************************************************************************************
**  void void CGame::set_magic_protection_flag(short owner_h, char owner_type, bool status)							**
**  description			:: changes the status of the player to show magic protect aura								**
**  last updated		:: November 20, 2004; 9:38 PM; Hypnotoad													**
**	return value		:: void																						**
*********************************************************************************************************************/

/*********************************************************************************************************************
**  void void CGame::set_protection_from_arrow_flag(short owner_h, char owner_type, bool status)						**
**  description			:: changes the status of the player to show arrow protect aura								**
**  last updated		:: November 20, 2004; 9:39 PM; Hypnotoad													**
**	return value		:: void																						**
*********************************************************************************************************************/

/*********************************************************************************************************************
**  void void CGame::set_illusion_movement_flag(short owner_h, char owner_type, bool status)							**
**  description			:: changes the status of the player to show illusion movement aura							**
**  last updated		:: November 20, 2004; 9:39 PM; Hypnotoad													**
**	return value		:: void																						**
*********************************************************************************************************************/

// New 07/05/2004
// Item Logging

void CGame::get_exp(int client_h, uint32_t exp, bool is_attacker_own)
{
	double v1, v2, v3;
	int iH;
	uint32_t time = GameClock::GetTimeMS(), unit_value;
	int total_party_members;

	if (m_client_list[client_h] == 0) return;
	if (exp <= 0) return;

	if (m_client_list[client_h]->m_level <= 80) {
		v1 = (double)(80 - m_client_list[client_h]->m_level);
		v2 = v1 * 0.025f;
		v3 = (double)exp;
		v1 = (v2 + 1.025f) * v3;
		exp = (uint32_t)v1;
	}
	else { //Lower exp
		if ((m_client_list[client_h]->m_level >= 80) && ((strcmp(m_map_list[m_client_list[client_h]->m_map_index]->m_name, "aresdend1") == 0) || (strcmp(m_map_list[m_client_list[client_h]->m_map_index]->m_name, "elvined1") == 0))) {
			exp = (exp / 10);
		}
		else if ((strcmp(m_map_list[m_client_list[client_h]->m_map_index]->m_name, "aresdend1") == 0) || (strcmp(m_map_list[m_client_list[client_h]->m_map_index]->m_name, "elvined1") == 0)) {
			exp = (exp * 1 / 4);
		}
	}

	//Check for party status, else give exp to player
	if ((m_client_list[client_h]->m_party_id != 0) && (m_client_list[client_h]->m_party_status == PartyStatus::Confirm)) {
		//Only divide exp if >= 1 person 
		if (m_party_info[m_client_list[client_h]->m_party_id].total_members > 0) {

			//Calc total ppl in party
			total_party_members = 0;
			for(int i = 0; i < m_party_info[m_client_list[client_h]->m_party_id].total_members; i++) {
				iH = m_party_info[m_client_list[client_h]->m_party_id].index[i];
				if ((m_client_list[iH] != 0) && (m_client_list[iH]->m_hp > 0)) {
					//Newly added, Only players on same map get exp :}
					if ((strlen(m_map_list[m_client_list[iH]->m_map_index]->m_name)) == (strlen(m_map_list[m_client_list[client_h]->m_map_index]->m_name))) {
						if (memcmp(m_map_list[m_client_list[iH]->m_map_index]->m_name, m_map_list[m_client_list[client_h]->m_map_index]->m_name, strlen(m_map_list[m_client_list[client_h]->m_map_index]->m_name)) == 0) {
							total_party_members++;
						}
					}
				}
			}

			//Check for party bug
			if (total_party_members > hb::shared::limits::MaxPartyMembers) {
				hb::logger::error<log_channel::events>("Party bug: member count {} exceeds limit", total_party_members);
				total_party_members = hb::shared::limits::MaxPartyMembers;
			}

			//Figure out how much exp a player gets
			v1 = (double)exp;
			v2 = v1;

			if (total_party_members > 1)
			{
				v2 = (v1 + (v1 * (double)(total_party_members / hb::shared::limits::MaxPartyMembers))) / (double)total_party_members;
			}

			v3 = v2 + 5.0e-1;
			unit_value = (uint32_t)v3;

			//Divide exp among party members
			for(int i = 0; i < total_party_members; i++) {

				iH = m_party_info[m_client_list[client_h]->m_party_id].index[i];
				if ((m_client_list[iH] != 0) && (m_client_list[iH]->m_skill_using_status[19] != 1) && (m_client_list[iH]->m_hp > 0)) { // Is player alive ??
					if (m_client_list[iH]->m_status.slate_exp)  unit_value *= 3;
					m_client_list[iH]->m_exp_stock += unit_value;
				}
			}
		}
	}
	else {
		if ((m_client_list[client_h] != 0) && (m_client_list[client_h]->m_skill_using_status[19] != 1) && (m_client_list[client_h]->m_hp > 0)) { // Is player alive ??
			if (m_client_list[client_h]->m_status.slate_exp)  exp *= 3;
			m_client_list[client_h]->m_exp_stock += exp;
		}
	}
}

// New 12/05/2004

// New 16/05/2004

// New 18/05/2004
void CGame::set_playing_status(int client_h)
{
	char map_name[11], location[11];

	if (m_client_list[client_h] == 0) return;

	std::memset(map_name, 0, sizeof(map_name));
	std::memset(location, 0, sizeof(location));

	strcpy(location, m_client_list[client_h]->m_location);
	strcpy(map_name, m_map_list[m_client_list[client_h]->m_map_index]->m_name);

	m_client_list[client_h]->m_side = 0;
	m_client_list[client_h]->m_is_own_location = false;
	m_client_list[client_h]->m_is_player_civil = false;

	if (memcmp(location, map_name, 3) == 0) {
		m_client_list[client_h]->m_is_own_location = true;
	}

	if (memcmp(location, "are", 3) == 0)
		m_client_list[client_h]->m_side = 1;
	else if (memcmp(location, "elv", 3) == 0)
		m_client_list[client_h]->m_side = 2;
	else {
		if (strcmp(map_name, "elvine") == 0 || strcmp(map_name, "aresden") == 0) {
			m_client_list[client_h]->m_is_own_location = true;
		}
		m_client_list[client_h]->m_is_neutral = true;
	}

	if (memcmp(location, "arehunter", 9) == 0 || memcmp(location, "elvhunter", 9) == 0) {
		m_client_list[client_h]->m_is_player_civil = true;
	}

	if (memcmp(m_client_list[client_h]->m_map_name, "bisle", 5) == 0) {
		m_client_list[client_h]->m_is_player_civil = false;
	}

	if (memcmp(m_client_list[client_h]->m_map_name, "bsmith", 6) == 0 ||
		memcmp(m_client_list[client_h]->m_map_name, "gldhall", 7) == 0 ||
		memcmp(m_client_list[client_h]->m_map_name, "gshop", 5) == 0)
		m_client_list[client_h]->m_is_processing_allowed = true;
	else
		m_client_list[client_h]->m_is_processing_allowed = false;

	if (memcmp(m_client_list[client_h]->m_map_name, "wrhus", 5) == 0 ||
		memcmp(m_client_list[client_h]->m_map_name, "arewrhus", 8) == 0 ||
		memcmp(m_client_list[client_h]->m_map_name, "elvwrhus", 8) == 0)
		m_client_list[client_h]->m_is_inside_warehouse = true;
	else
		m_client_list[client_h]->m_is_inside_warehouse = false;

	if (memcmp(m_client_list[client_h]->m_map_name, "wzdtwr", 6) == 0)
		m_client_list[client_h]->m_is_inside_wizard_tower = true;
	else
		m_client_list[client_h]->m_is_inside_wizard_tower = false;
}

void CGame::force_change_play_mode(int client_h, bool notify)
{
	if (m_client_list[client_h] == 0) return;

	if (memcmp(m_client_list[client_h]->m_location, "arehunter", 9) == 0)
		strcpy(m_client_list[client_h]->m_location, "aresden");
	else if (memcmp(m_client_list[client_h]->m_location, "elvhunter", 9) == 0)
		strcpy(m_client_list[client_h]->m_location, "elvine");

	if (m_client_list[client_h]->m_is_player_civil)
		m_client_list[client_h]->m_is_player_civil = false;

	if (notify) {
		send_notify_msg(0, client_h, Notify::ChangePlayMode, 0, 0, 0, m_client_list[client_h]->m_location);
		send_event_to_near_client_type_a(client_h, hb::shared::owner_class::Player, MsgId::EventMotion, Type::NullAction, 0, 0, 0);
	}
}

void CGame::show_version(int client_h)
{
	char ver_message[256];

	std::memset(ver_message, 0, sizeof(ver_message));
	std::snprintf(ver_message, sizeof(ver_message), "Helbreath %s", hb::version::server::display_version);
	show_client_msg(client_h, ver_message);

}

// v2.14 05/22/2004 - Hypnotoad - adds pk log
void CGame::request_resurrect_player(int client_h, bool resurrect)
{
	short sX, sY;
	char buff[100];

	if (m_client_list[client_h] == 0) return;

	sX = m_client_list[client_h]->m_x;
	sY = m_client_list[client_h]->m_y;

	if (resurrect == false) {
		m_client_list[client_h]->m_is_being_resurrected = false;
		return;
	}

	if (m_client_list[client_h]->m_is_being_resurrected == false) {
		try
		{
			std::snprintf(buff, sizeof(buff), "(!!!) Player(%s) Tried To Use Resurrection Hack", m_client_list[client_h]->m_char_name);
			hb::logger::warn<log_channel::security>("{}", G_cTxt);
			delete_client(client_h, true, true, true, true);
		}
		catch (...)
		{
		}
		return;
	}

	hb::logger::log("Resurrect Player! {}", m_client_list[client_h]->m_char_name);

	m_client_list[client_h]->m_is_killed = false;
	// Player's HP becomes half of the Max HP. 
	m_client_list[client_h]->m_hp = get_max_hp(client_h) / 2;
	// Player's MP
	m_client_list[client_h]->m_mp = get_max_mp(client_h);
	// Player's SP
	m_client_list[client_h]->m_sp = get_max_sp(client_h);
	send_notify_msg(0, client_h, Notify::Sp, 0, 0, 0, 0);
	// Player's Hunger
	m_client_list[client_h]->m_hunger_status = 100;

	m_client_list[client_h]->m_is_being_resurrected = false;

	// !!! request_teleport_handler m_map_name
	request_teleport_handler(client_h, "2   ", m_client_list[client_h]->m_map_name, m_client_list[client_h]->m_x, m_client_list[client_h]->m_y);
}

bool CGame::check_client_move_frequency(int client_h, uint32_t client_time)
{
	uint32_t time_gap;

	if (m_client_list[client_h] == 0) return false;

	if (m_client_list[client_h]->m_move_freq_time == 0)
		m_client_list[client_h]->m_move_freq_time = client_time;
	else {
		if (m_client_list[client_h]->m_is_move_blocked) {
			m_client_list[client_h]->m_move_freq_time = 0;
			m_client_list[client_h]->m_is_move_blocked = false;
			return false;
		}

		if (m_client_list[client_h]->m_is_attack_mode_change) {
			m_client_list[client_h]->m_move_freq_time = 0;
			m_client_list[client_h]->m_is_attack_mode_change = false;
			return false;
		}

		time_gap = client_time - m_client_list[client_h]->m_move_freq_time;
		m_client_list[client_h]->m_move_freq_time = client_time;

		if ((time_gap < 200) && (time_gap >= 0)) {
			try
			{
				hb::logger::warn<log_channel::security>("Speed hack: IP={} player={}, running too fast", m_client_list[client_h]->m_ip_address, m_client_list[client_h]->m_char_name);
				delete_client(client_h, true, true);
			}
			catch (...)
			{
			}
			return false;
		}

		// testcode
		// std::snprintf(G_cTxt, sizeof(G_cTxt), "Move: %d", time_gap);
		// PutLogList(G_cTxt);
	}

	return false;
}

void CGame::on_timer(char type)
{
	uint32_t time;

	time = GameClock::GetTimeMS();

	// MODERNIZED: game_process moved to EventLoop to prevent blocking socket polling
	// on_timer now only handles periodic events (check_client_response_time, weather, etc.)
	// game_process() is called directly in EventLoop every 300ms

	if ((time - m_game_time_2) > 1000) {
		check_client_response_time();
		check_day_or_night_mode();
		m_game_time_2 += 1000;
		if (time - m_game_time_2 > 1000) m_game_time_2 = time;
		// v1.41

		// v1.41
		if (m_is_game_started == false) {
			on_start_game_signal();
			m_is_game_started = true;

			// initialize EntityManager now that maps are loaded
			if (m_entity_manager != NULL) {
				// EntityManager owns the entity array now, just set maps and game reference
				m_entity_manager->set_map_list(m_map_list, MaxMaps);
				m_entity_manager->set_game(this);

	// initialize Gathering Managers
	m_fishing_manager = new FishingManager();
	m_fishing_manager->set_game(this);
	m_mining_manager = new MiningManager();
	m_mining_manager->set_game(this);
	m_crafting_manager = new CraftingManager();
	m_crafting_manager->set_game(this);
	m_quest_manager = new QuestManager();
	m_quest_manager->set_game(this);
	m_guild_manager = new GuildManager();
	m_guild_manager->set_game(this);
	m_delay_event_manager = new DelayEventManager();
	m_delay_event_manager->set_game(this);
	m_delay_event_manager->init_arrays();
	m_dynamic_object_manager = new DynamicObjectManager();
	m_dynamic_object_manager->set_game(this);
	m_dynamic_object_manager->init_arrays();
	m_loot_manager = new LootManager();
	m_loot_manager->set_game(this);
	m_combat_manager = new CombatManager();
	m_item_manager = new ItemManager();
	m_magic_manager = new MagicManager();
	m_skill_manager = new SkillManager();
	m_war_manager = new WarManager();
	m_status_effect_manager = new StatusEffectManager();
	m_combat_manager->set_game(this);
	m_item_manager->set_game(this);
	m_magic_manager->set_game(this);
	m_skill_manager->set_game(this);
	m_war_manager->set_game(this);
	m_status_effect_manager->set_game(this);
			}

			// initialize Gathering Managers
			if (m_fishing_manager != nullptr) {
				m_fishing_manager->set_game(this);
			}
			if (m_mining_manager != nullptr) {
				m_mining_manager->set_game(this);
			}
			if (m_crafting_manager != nullptr) {
				m_crafting_manager->set_game(this);
			}
			if (m_quest_manager != nullptr) {
				m_quest_manager->set_game(this);
			}
			if (m_guild_manager != nullptr) {
				m_guild_manager->set_game(this);
			}
			if (m_delay_event_manager != nullptr) {
				m_delay_event_manager->set_game(this);
				m_delay_event_manager->cleanup_arrays();
				m_delay_event_manager->init_arrays();
			}
			if (m_dynamic_object_manager != nullptr) {
				m_dynamic_object_manager->set_game(this);
				m_dynamic_object_manager->cleanup_arrays();
				m_dynamic_object_manager->init_arrays();
			}
			if (m_loot_manager != nullptr) {
				m_loot_manager->set_game(this);
			}
			if (m_combat_manager != nullptr) {
				m_combat_manager->set_game(this);
	m_item_manager->set_game(this);
	m_magic_manager->set_game(this);
	m_skill_manager->set_game(this);
	m_war_manager->set_game(this);
	m_status_effect_manager->set_game(this);
			}
		}
	}
	if ((time - m_game_time_6) > 1000) {
		m_delay_event_manager->delay_event_processor();
		m_game_time_6 += 1000;
		if (time - m_game_time_6 > 1000) m_game_time_6 = time;

		// v2.05
		if (m_final_shutdown_count != 0) {
			m_final_shutdown_count--;
			hb::logger::log("Final shutdown countdown: {}", m_final_shutdown_count);
			if (m_final_shutdown_count <= 1) {
				G_bRunning = false;
				return;

			}
		}
	}

	if ((time - m_game_time_3) > 1000) {
		m_war_manager->sync_middleland_map_info();
		m_dynamic_object_manager->check_dynamic_object_list();
		m_dynamic_object_manager->dynamic_object_effect_processor();
		notice_handler();
		special_event_handler();
		m_war_manager->energy_sphere_processor();
		m_game_time_3 += 1000;
		if (time - m_game_time_3 > 1000) m_game_time_3 = time;
	}

	if ((time - m_game_time_4) > 600) {
		// Use EntityManager for spawn generation
		if (m_entity_manager != NULL)
			m_entity_manager->process_spawns();

		m_game_time_4 += 600;
		if (time - m_game_time_4 > 600) m_game_time_4 = time;
	}

	if ((time - m_game_time_5) > 1000 * 60 * 3) {

		m_game_time_5 += 1000 * 60 * 3;
		if (time - m_game_time_5 > 1000 * 60 * 3) m_game_time_5 = time;

		srand((unsigned)std::time(0));
	}

	if ((time - m_fishing_manager->m_fish_time) > 5000) {
		m_fishing_manager->fish_processor();
		m_fishing_manager->fish_generator();
		m_war_manager->send_collected_mana();
		m_war_manager->crusade_war_starter();
		//ApocalypseStarter();
		m_war_manager->apocalypse_ender();
		m_fishing_manager->m_fish_time += 5000;
		if (time - m_fishing_manager->m_fish_time > 5000) m_fishing_manager->m_fish_time = time;
	}

	if ((time - m_weather_time) > 1000 * 20) {
		weather_processor();
		m_weather_time += 1000 * 20;
		if (time - m_weather_time > 1000 * 20) m_weather_time = time;
	}

	// Periodic equipment validation — catches cascading invalidations
	if ((time - m_equip_validation_time) > 10000)
	{
		for (int i = 1; i < MaxClients; i++)
		{
			if (m_client_list[i] == nullptr) continue;
			if (!m_client_list[i]->m_is_init_complete) continue;
			m_item_manager->validate_equipped_items(i);
		}
		m_equip_validation_time += 10000;
		if (time - m_equip_validation_time > 10000) m_equip_validation_time = time;
	}

	if ((m_heldenian_running) && (m_is_heldenian_mode)) {
		m_war_manager->set_heldenian_mode();
	}
	// Scheduled shutdown: send milestone notifications, then begin disconnect
	if (m_shutdown_start_time != 0 && !m_on_exit_process)
	{
		uint32_t elapsed_ms = time - m_shutdown_start_time;

		if (elapsed_ms >= m_shutdown_delay_ms)
		{
			// Delay elapsed — save and begin disconnect sequence
			hb::logger::log("Scheduled shutdown delay elapsed, beginning disconnect sequence...");
			save_all_players();
			m_on_exit_process = true;
			m_exit_process_time = time;
			m_shutdown_start_time = 0;
		}
		else
		{
			// Check if we've crossed a milestone
			uint32_t remaining_ms = m_shutdown_delay_ms - elapsed_ms;
			int remaining_sec = static_cast<int>(remaining_ms / 1000);

			if (m_shutdown_next_milestone < static_cast<int>(m_shutdown_milestones.size()))
			{
				int milestone = m_shutdown_milestones[m_shutdown_next_milestone];
				if (remaining_sec <= milestone)
				{
					hb::logger::log("Shutdown countdown: {} seconds remaining", milestone);
					for (int i = 1; i < MaxClients; i++)
					{
						if (m_client_list[i] != nullptr && m_client_list[i]->m_is_init_complete)
							send_notify_msg(0, i, Notify::ServerShutdown, 1, milestone, 0,
								m_shutdown_message[0] != '\0' ? m_shutdown_message : nullptr);
					}
					m_shutdown_next_milestone++;
				}
			}
		}
	}

	if ((m_is_server_shutdown == false) && (m_on_exit_process) && ((time - m_exit_process_time) > 1000 * 2)) {
		if (force_player_disconnect(15) == 0) {
			hb::logger::log("Server shutdown complete, all players disconnected");
			m_is_server_shutdown = true;

			if ((m_shutdown_code == 3) || (m_shutdown_code == 4)) {
				hb::logger::error<log_channel::events>("Auto-rebooting server");
				init();
				m_auto_rebooting_count++;
			}
			else {
				if (m_final_shutdown_count == 0)	m_final_shutdown_count = 20;
			}
		}
		m_exit_process_time = time;
	}

	if ((time - m_map_sector_info_time) > 1000 * 10) {
		m_map_sector_info_time += 1000 * 10;
		if (time - m_map_sector_info_time > 1000 * 10) m_map_sector_info_time = time;
		update_map_sector_info();

		m_mining_manager->mineral_generator();

		m_map_sector_info_update_count++;
		if (m_map_sector_info_update_count >= 5) {
			aging_map_sector_info();
			m_map_sector_info_update_count = 0;
		}
	}
}

void CGame::on_start_game_signal()
{
	bool loadedSchedules = false;
	sqlite3* configDb = nullptr;
	std::string configDbPath;
	bool configDbCreated = false;
	bool configDbReady = EnsureGameConfigDatabase(&configDb, configDbPath, &configDbCreated);
	if (configDbReady && !configDbCreated) {
		bool hasCrusade = HasGameConfigRows(configDb, "crusade_structures");
		bool hasSchedule = HasGameConfigRows(configDb, "event_schedule");
		if (hasCrusade && hasSchedule) {
			if (LoadCrusadeConfig(configDb, this) && LoadScheduleConfig(configDb, this)) {
				loadedSchedules = true;
			}
		}
	}

	if (!loadedSchedules) {
		hb::logger::error("Crusade/schedule configs missing in gamedata.db");
	}

	m_war_manager->link_strike_point_map_index();

	if (configDb != nullptr) {
		CloseGameConfigDatabase(configDb);
	}

	m_war_manager->read_crusade_guid_file("GameData/CrusadeGUID.txt");
	m_war_manager->read_apocalypse_guid_file("GameData/ApocalypseGUID.txt");
	m_war_manager->read_heldenian_guid_file("GameData/HeldenianGUID.txt");

	hb::logger::log("Game server activated");

	auto stats = CountAccountStats();
	hb::logger::log("- Max level: {}, Max stat: {}, Stat/level: {}", m_max_level, m_max_stat_value, m_levelup_stat_gain);
	hb::logger::log("- Accounts: {}, Characters: {}", stats.accounts, stats.characters);
	for (const auto& [name, count] : stats.over_limit)
		hb::logger::warn("- Account '{}' has {} characters (limit: {})", name, count, hb::shared::limits::MaxCharactersPerAccount);

}

// New 12/05/2004 Changed

//HBest force recall start code

//HBest force recall code

void CGame::set_force_recall_time(int client_h)
{
	int iTL_ = 0;
	hb::time::local_time SysTime{};

	if (m_client_list[client_h] == 0) return;

	if (m_client_list[client_h]->m_time_left_force_recall == 0) {
		// war_period .

		if (m_force_recall_time > 0) {
			m_client_list[client_h]->m_time_left_force_recall = 20 * m_force_recall_time;
		}
		else {
			SysTime = hb::time::local_time::now();
			switch (SysTime.day_of_week) {
			case 1:	m_client_list[client_h]->m_time_left_force_recall = 20 * m_raid_time_monday; break;  // 3 2002-09-10 #1
			case 2:	m_client_list[client_h]->m_time_left_force_recall = 20 * m_raid_time_tuesday; break;
			case 3:	m_client_list[client_h]->m_time_left_force_recall = 20 * m_raid_time_wednesday; break;
			case 4:	m_client_list[client_h]->m_time_left_force_recall = 20 * m_raid_time_thursday; break;
			case 5:	m_client_list[client_h]->m_time_left_force_recall = 20 * m_raid_time_friday; break;
			case 6:	m_client_list[client_h]->m_time_left_force_recall = 20 * m_raid_time_saturday; break;
			case 0:	m_client_list[client_h]->m_time_left_force_recall = 20 * m_raid_time_sunday; break;
			}
		}
	}
	else { // if (m_client_list[client_h]->m_time_left_force_recall == 0) 
		if (m_force_recall_time > 0) {
			iTL_ = 20 * m_force_recall_time;
		}
		else {

			SysTime = hb::time::local_time::now();
			switch (SysTime.day_of_week) {
			case 1:	iTL_ = 20 * m_raid_time_monday; break;  // 3 2002-09-10 #1
			case 2:	iTL_ = 20 * m_raid_time_tuesday; break;
			case 3:	iTL_ = 20 * m_raid_time_wednesday; break;
			case 4:	iTL_ = 20 * m_raid_time_thursday; break;
			case 5:	iTL_ = 20 * m_raid_time_friday; break;
			case 6:	iTL_ = 20 * m_raid_time_saturday; break;
			case 0:	iTL_ = 20 * m_raid_time_sunday; break;
			}
		}

		if (m_client_list[client_h]->m_time_left_force_recall > iTL_)
			m_client_list[client_h]->m_time_left_force_recall = 1;

	}

	m_client_list[client_h]->m_is_war_location = true;
	return;
}

void CGame::check_force_recall_time(int client_h)
{
	hb::time::local_time SysTime{};
	int iTL_;

	if (m_client_list[client_h] == 0) return;

	if (m_client_list[client_h]->m_time_left_force_recall == 0) {
		// has admin set a recall time ??
		if (m_force_recall_time > 0) {
			m_client_list[client_h]->m_time_left_force_recall = m_force_recall_time * 20;
		}
		// use standard recall time calculations
		else {
			SysTime = hb::time::local_time::now();
			switch (SysTime.day_of_week) {
			case 1:	m_client_list[client_h]->m_time_left_force_recall = 20 * m_raid_time_monday; break;  // 3 2002-09-10 #1
			case 2:	m_client_list[client_h]->m_time_left_force_recall = 20 * m_raid_time_tuesday; break;
			case 3:	m_client_list[client_h]->m_time_left_force_recall = 20 * m_raid_time_wednesday; break;
			case 4:	m_client_list[client_h]->m_time_left_force_recall = 20 * m_raid_time_thursday; break;
			case 5:	m_client_list[client_h]->m_time_left_force_recall = 20 * m_raid_time_friday; break;
			case 6:	m_client_list[client_h]->m_time_left_force_recall = 20 * m_raid_time_saturday; break;
			case 0:	m_client_list[client_h]->m_time_left_force_recall = 20 * m_raid_time_sunday; break;
			}
		}
	}
	else {
		// has admin set a recall time ??
		if (m_force_recall_time > 0) {
			iTL_ = m_force_recall_time * 20;
		}
		// use standard recall time calculations
		else {
			SysTime = hb::time::local_time::now();
			switch (SysTime.day_of_week) {
			case 1:	iTL_ = 20 * m_raid_time_monday; break;  // 3 2002-09-10 #1
			case 2:	iTL_ = 20 * m_raid_time_tuesday; break;
			case 3:	iTL_ = 20 * m_raid_time_wednesday; break;
			case 4:	iTL_ = 20 * m_raid_time_thursday; break;
			case 5:	iTL_ = 20 * m_raid_time_friday; break;
			case 6:	iTL_ = 20 * m_raid_time_saturday; break;
			case 0:	iTL_ = 20 * m_raid_time_sunday; break;
			}
		}
		if (m_client_list[client_h]->m_time_left_force_recall > iTL_)
			m_client_list[client_h]->m_time_left_force_recall = iTL_;
	}

	m_client_list[client_h]->m_is_war_location = true;
	return;

}

int ITEMSPREAD_FIEXD_COORD[25][2] =
{
	{ 0,  0},
	{ 1,  0},
	{ 1,  1},
	{ 0,  1},
	{-1,  1},
	{-1,  0},
	{-1, -1},
	{ 0, -1},
	{ 1, -1},
	{ 2, -1},
	{ 2,  0},
	{ 2,  1},
	{ 2,  2},
	{ 1,  2},
	{ 0,  2},
	{-1,  2},
	{-2,  2},
	{-2,  1},
	{-2,  0},
	{-2, -1},
	{-2, -2},
	{-1, -2},
	{ 0, -2},
	{ 1, -2},
	{ 2, -2},
};

//New Changed 11/05/2004

bool CGame::register_map(char* name)
{
	
	char tmp_name[11];

	std::memset(tmp_name, 0, sizeof(tmp_name));
	strcpy(tmp_name, name);
	for(int i = 0; i < MaxMaps; i++)
		if ((m_map_list[i] != 0) && (memcmp(m_map_list[i]->m_name, tmp_name, 10) == 0)) {
			hb::logger::error("Map already installed: {}", tmp_name);
			return false;
		}

	for(int i = 0; i < MaxMaps; i++)
		if (m_map_list[i] == 0) {
			m_map_list[i] = new class CMap(this);
			if (m_map_list[i]->init(name) == false) {
				hb::logger::error("Map data load failed: {}", name);
				return false;
			};

			if ((m_middleland_map_index == -1) && (strcmp("middleland", name) == 0))
				m_middleland_map_index = i;

			if ((m_aresden_map_index == -1) && (strcmp("aresden", name) == 0))
				m_aresden_map_index = i;

			if ((m_elvine_map_index == -1) && (strcmp("elvine", name) == 0))
				m_elvine_map_index = i;

			if ((m_bt_field_map_index == -1) && (strcmp("btfield", name) == 0))
				m_bt_field_map_index = i;

			if ((m_godh_map_index == -1) && (strcmp("godh", name) == 0))
				m_godh_map_index = i;

			m_total_maps++;
			return true;
		}

	hb::logger::error("Map cannot be added (no space): {}", name);
	return false;
}

//New Changed 11/05/2004

void CGame::show_client_msg(int client_h, char* pMsg)
{
	char temp[256];
	std::memset(temp, 0, sizeof(temp));

	auto& chatPkt = *reinterpret_cast<hb::net::PacketChatMsg*>(temp);
	chatPkt.header.msg_id = MsgId::CommandChatMsg;
	chatPkt.header.msg_type = 0;
	chatPkt.reserved1 = 0;
	chatPkt.reserved2 = 0;
	std::memcpy(chatPkt.name, "HGServer", 8);
	chatPkt.chat_type = 10;

	size_t msg_size = strlen(pMsg);
	if (msg_size > 50) msg_size = 50;
	memcpy(temp + sizeof(hb::net::PacketChatMsg), pMsg, msg_size);

	m_client_list[client_h]->m_socket->send_msg(temp, msg_size + sizeof(hb::net::PacketChatMsg));
}

/*
at the end of client connection have a true switch
at the start of client move handler check if the switch is true
if it is not true add 1 warning, if the warning reaches 3
delete client and log him, if the true switch
*/
//and when a client walks into a map with dynamic portal
//[KLKS] - [Pretty Good Coders] says:
//u gotta inform it
//[KLKS] - [Pretty Good Coders] says:
//or else they wont see it

/*void CGame::OpenApocalypseGate(int client_h)
{
	if (m_client_list[client_h] == 0) return;

	//m_map_list[m_client_list[client_h]->m_map_index]->m_total_alive_object;
	send_notify_msg(0, client_h, Notify::ApocGateOpen, 95, 31, 0, m_client_list[client_h]->m_map_name);
}*/

void CGame::global_update_configs(char config_type)
{
	local_update_configs(config_type);
}

void CGame::local_update_configs(char config_type)
{
	sqlite3* configDb = nullptr;
	std::string configDbPath;
	bool configDbCreated = false;
	if (!EnsureGameConfigDatabase(&configDb, configDbPath, &configDbCreated) || configDbCreated) {
		hb::logger::error("gamedata.db unavailable, cannot reload configs");
		return;
	}

	bool ok = false;
	if (config_type == 3) {
		ok = LoadBannedListConfig(configDb, this);
		if (ok) hb::logger::log("BannedList updated successfully!");
		else hb::logger::error("BannedList reload failed!");
	}
	CloseGameConfigDatabase(configDb);
}

void CGame::reload_npc_configs()
{
	sqlite3* configDb = nullptr;
	std::string configDbPath;
	bool configDbCreated = false;
	if (!EnsureGameConfigDatabase(&configDb, configDbPath, &configDbCreated) || configDbCreated)
	{
		hb::logger::log("NPC config reload failed: gamedata.db unavailable");
		return;
	}

	for(int i = 0; i < MaxNpcTypes; i++)
	{
		if (m_npc_config_list[i] != 0)
		{
			delete m_npc_config_list[i];
			m_npc_config_list[i] = 0;
		}
	}

	if (!LoadNpcConfigs(configDb, this))
	{
		hb::logger::log("NPC config reload failed");
		CloseGameConfigDatabase(configDb);
		return;
	}

	CloseGameConfigDatabase(configDb);
	compute_config_hashes();
	hb::logger::log("NPC configs reloaded successfully");
}

void CGame::reload_shop_configs()
{
	sqlite3* configDb = nullptr;
	std::string configDbPath;
	bool configDbCreated = false;
	if (!EnsureGameConfigDatabase(&configDb, configDbPath, &configDbCreated) || configDbCreated)
	{
		hb::logger::log("Shop config reload failed: gamedata.db unavailable");
		return;
	}

	m_npc_shop_mappings.clear();
	m_shop_data.clear();
	m_is_shop_data_available = false;

	if (HasGameConfigRows(configDb, "npc_shop_mapping") || HasGameConfigRows(configDb, "shop_items"))
	{
		LoadShopConfigs(configDb, this);
	}

	CloseGameConfigDatabase(configDb);

	if (m_is_shop_data_available)
		hb::logger::log("Shop configs reloaded successfully ({} shops, {} NPC config mappings)",
			m_shop_data.size(), m_npc_shop_mappings.size());
	else
		hb::logger::log("Shop configs reloaded (no shop data found)");
}

void CGame::send_config_reload_notification(bool items, bool magic, bool skills, bool npcs, bool balance, bool colors, bool attribute_types)
{
	hb::net::PacketNotifyConfigReload pkt{};
	pkt.header.msg_id = MsgId::NotifyConfigReload;
	pkt.header.msg_type = MsgType::Confirm;
	pkt.reloadItems = items ? 1 : 0;
	pkt.reloadMagic = magic ? 1 : 0;
	pkt.reloadSkills = skills ? 1 : 0;
	pkt.reloadNpcs = npcs ? 1 : 0;
	pkt.reloadBalance = balance ? 1 : 0;
	pkt.reloadColorPalette = colors ? 1 : 0;
	pkt.reloadAttributeTypes = attribute_types ? 1 : 0;

	for(int i = 1; i < MaxClients; i++)
	{
		if (m_client_list[i] != 0 && m_client_list[i]->m_is_init_complete)
			m_client_list[i]->m_socket->send_msg((char*)&pkt, sizeof(pkt));
	}
}

void CGame::push_config_reload_to_clients(bool items, bool magic, bool skills, bool npcs, bool balance, bool colors, bool attribute_types)
{
	int count = 0;
	for(int i = 1; i < MaxClients; i++)
	{
		if (m_client_list[i] != 0 && m_client_list[i]->m_is_init_complete)
		{
			if (items)   m_item_manager->send_client_item_configs(i);
			if (magic)   m_magic_manager->send_client_magic_configs(i);
			if (skills)  m_skill_manager->send_client_skill_configs(i);
			if (npcs)    send_client_npc_configs(i);
			if (balance) send_client_balance_config(i);
			if (colors)  send_client_color_palette(i);
			if (attribute_types) send_client_attribute_types(i);
			count++;
		}
	}

	hb::logger::log("Config reload pushed to {} client(s)", count);
}

void CGame::reload_color_palette()
{
	sqlite3* configDb = nullptr;
	std::string dbPath;
	bool created = false;
	if (!EnsureGameConfigDatabase(&configDb, dbPath, &created)) return;

	m_color_palette.clear();
	LoadColorPalette(configDb, m_color_palette);
	compute_color_palette_hash();
	CloseGameConfigDatabase(configDb);
}

/*void CGame::ApocalypseStarter()
{
 hb::time::local_time SysTime{};
 

	if (m_is_apocalypse_mode ) return;
	if (m_is_apocalypse_starter == false) return;

	SysTime = hb::time::local_time::now();

	for(int i = 0; i < MaxApocalypse; i++)
	if	((m_apocalypse_schedule_start[i].day == SysTime.day_of_week) &&
		(m_apocalypse_schedule_start[i].hour == SysTime.hour) &&
		(m_apocalypse_schedule_start[i].minute == SysTime.minute)) {
			PutLogList("(!) Automated apocalypse is initiated!");
			GlobalStartApocalypseMode();
			return;
	}
}*/

// New 06/05/2004
// Party Code
void CGame::request_create_party_handler(int client_h)
{
	char data[120]{};

	if (m_client_list[client_h] == 0) return;
	if (m_client_list[client_h]->m_is_init_complete == false) return;

	if (m_client_list[client_h]->m_party_status != PartyStatus::Null) {
		return;
	}

	m_client_list[client_h]->m_party_status = PartyStatus::Processing;

	hb::net::PartyOpCreateRequest req{};
	req.op_type = 1;
	req.client_h = static_cast<uint16_t>(client_h);
	std::memcpy(req.name, m_client_list[client_h]->m_char_name, sizeof(req.name));
	std::memcpy(data, &req, sizeof(req));

	party_operation(data);

	hb::logger::log("Party create request: client={}", client_h);
}

// Last Updated October 28, 2004 - 3.51 translation
void CGame::party_operation_result_handler(char* data)
{
	char name[12];
	int client_h, party_id, total;

	uint16_t opType = *reinterpret_cast<uint16_t*>(data);

	switch (opType) {
	case 1: {
		const auto& res = *reinterpret_cast<const hb::net::PartyOpResultWithStatus*>(data);
		client_h = static_cast<int>(res.client_h);
		std::memset(name, 0, sizeof(name));
		std::memcpy(name, res.name, sizeof(res.name));
		party_id = static_cast<int>(res.party_id);

		party_operation_result_create(client_h, name, res.result, party_id);

		hb::logger::log("Party created: client={} party={}", client_h, party_id);
		break;
	}

	case 2: {
		const auto& res = *reinterpret_cast<const hb::net::PartyOpResultDelete*>(data);
		party_id = static_cast<int>(res.party_id);

		party_operation_result_delete(party_id);

		hb::logger::log("Party deleted: party={}", party_id);
		break;
	}

	case 3: {
		const auto& res = *reinterpret_cast<const hb::net::PartyOpCreateRequest*>(data);
		client_h = static_cast<int>(res.client_h);
		std::memset(name, 0, sizeof(name));
		std::memcpy(name, res.name, sizeof(res.name));

		if ((client_h < 0) && (client_h > MaxClients)) return;
		if (m_client_list[client_h] == 0) return;
		if (hb_stricmp(m_client_list[client_h]->m_char_name, name) != 0) return;

		for (int i = 0; i < hb::shared::limits::MaxPartyMembers; i++)
			if (m_party_info[m_client_list[client_h]->m_party_id].index[i] == client_h) {
				m_party_info[m_client_list[client_h]->m_party_id].index[i] = 0;
				m_party_info[m_client_list[client_h]->m_party_id].total_members--;

				hb::logger::log("Party {}: member {} left, total={}", m_client_list[client_h]->m_party_id, client_h, m_party_info[m_client_list[client_h]->m_party_id].total_members);
				break;
			}

		for (int i = 0; i < hb::shared::limits::MaxPartyMembers - 1; i++)
			if ((m_party_info[m_client_list[client_h]->m_party_id].index[i] == 0) && (m_party_info[m_client_list[client_h]->m_party_id].index[i + 1] != 0)) {
				m_party_info[m_client_list[client_h]->m_party_id].index[i] = m_party_info[m_client_list[client_h]->m_party_id].index[i + 1];
				m_party_info[m_client_list[client_h]->m_party_id].index[i + 1] = 0;
			}

		m_client_list[client_h]->m_party_id = 0;
		m_client_list[client_h]->m_party_status = PartyStatus::Null;

		hb::logger::log("Party status 0: {}", m_client_list[client_h]->m_char_name);

		send_notify_msg(0, client_h, Notify::Party, 8, 0, 0, 0);
		break;
	}

	case 4: {
		const auto& res = *reinterpret_cast<const hb::net::PartyOpResultWithStatus*>(data);
		client_h = static_cast<int>(res.client_h);
		std::memset(name, 0, sizeof(name));
		std::memcpy(name, res.name, sizeof(res.name));
		party_id = static_cast<int>(res.party_id);

		party_operation_result_join(client_h, name, res.result, party_id);

		hb::logger::log("Party joined: client={} party={}", client_h, party_id);
		break;
	}

	case 5: {
		const auto& res = *reinterpret_cast<const hb::net::PartyOpResultInfoHeader*>(data);
		client_h = static_cast<int>(res.client_h);
		std::memset(name, 0, sizeof(name));
		std::memcpy(name, res.name, sizeof(res.name));
		total = static_cast<int>(res.total);

		char* memberList = data + sizeof(hb::net::PartyOpResultInfoHeader);
		party_operation_result_info(client_h, name, total, memberList);

		hb::logger::log("Party info: client={} total={}", client_h, total);
		break;
	}

	case 6: {
		const auto& res = *reinterpret_cast<const hb::net::PartyOpResultWithStatus*>(data);
		client_h = static_cast<int>(res.client_h);
		std::memset(name, 0, sizeof(name));
		std::memcpy(name, res.name, sizeof(res.name));
		party_id = static_cast<int>(res.party_id);

		party_operation_result_dismiss(client_h, name, res.result, party_id);

		hb::logger::log("Party dismissed: client={} party={}", client_h, party_id);
		break;
	}
	}
}

void CGame::party_operation_result_create(int client_h, char* name, int result, int party_id)
{
	char data[120];
	

	if (m_client_list[client_h] == 0) return;
	if (hb_stricmp(m_client_list[client_h]->m_char_name, name) != 0) return;

	switch (result) {
	case 0:
		if (m_client_list[client_h]->m_party_status != PartyStatus::Processing) return;
		if (hb_stricmp(m_client_list[client_h]->m_char_name, name) != 0) return;

		m_client_list[client_h]->m_party_id = 0;
		m_client_list[client_h]->m_party_status = PartyStatus::Null;
		m_client_list[client_h]->m_req_join_party_client_h = 0;
		send_notify_msg(0, client_h, Notify::Party, 1, 0, 0, 0);
		break;

	case 1:
		if (m_client_list[client_h]->m_party_status != PartyStatus::Processing) return;
		if (hb_stricmp(m_client_list[client_h]->m_char_name, name) != 0) return;

		m_client_list[client_h]->m_party_id = party_id;
		m_client_list[client_h]->m_party_status = PartyStatus::Confirm;
		send_notify_msg(0, client_h, Notify::Party, 1, 1, 0, 0);

		for(int i = 0; i < hb::shared::limits::MaxPartyMembers; i++)
			if (m_party_info[m_client_list[client_h]->m_party_id].index[i] == 0) {
				m_party_info[m_client_list[client_h]->m_party_id].index[i] = client_h;
				m_party_info[m_client_list[client_h]->m_party_id].total_members++;
				//testcode
				hb::logger::log("Party {}: member {} added, total={}", m_client_list[client_h]->m_party_id, client_h, m_party_info[m_client_list[client_h]->m_party_id].total_members);
				break;
			}

		if ((m_client_list[client_h]->m_req_join_party_client_h != 0) && (strlen(m_client_list[client_h]->m_req_join_party_name) != 0)) {
			std::memset(data, 0, sizeof(data));
			hb::net::PartyOpPayload partyOp{};
			partyOp.op_type = 3;
			partyOp.client_h = static_cast<uint16_t>(m_client_list[client_h]->m_req_join_party_client_h);
			std::memcpy(partyOp.name, m_client_list[client_h]->m_req_join_party_name, sizeof(partyOp.name));
			partyOp.party_id = static_cast<uint16_t>(m_client_list[client_h]->m_party_id);
			std::memcpy(data, &partyOp, sizeof(partyOp));
			party_operation(data);
			m_client_list[client_h]->m_req_join_party_client_h = 0;
			std::memset(m_client_list[client_h]->m_req_join_party_name, 0, sizeof(m_client_list[client_h]->m_req_join_party_name));
		}
		break;
	}
}

// Last Updated October 28, 2004 - 3.51 translation
void CGame::party_operation_result_join(int client_h, char* name, int result, int party_id)
{
	

	if (m_client_list[client_h] == 0) return;

	switch (result) {
	case 0:
		if (m_client_list[client_h]->m_party_status != PartyStatus::Processing) return;
		if (hb_stricmp(m_client_list[client_h]->m_char_name, name) != 0) return;

		m_client_list[client_h]->m_party_id = 0;
		m_client_list[client_h]->m_party_status = PartyStatus::Null;
		send_notify_msg(0, client_h, Notify::Party, 4, 0, 0, name);

		m_client_list[client_h]->m_req_join_party_client_h = 0;
		std::memset(m_client_list[client_h]->m_req_join_party_name, 0, sizeof(m_client_list[client_h]->m_req_join_party_name));
		break;

	case 1:
		if (m_client_list[client_h]->m_party_status != PartyStatus::Processing) return;
		if (hb_stricmp(m_client_list[client_h]->m_char_name, name) != 0) return;

		m_client_list[client_h]->m_party_id = party_id;
		m_client_list[client_h]->m_party_status = PartyStatus::Confirm;
		send_notify_msg(0, client_h, Notify::Party, 4, 1, 0, name);

		m_client_list[client_h]->m_req_join_party_client_h = 0;
		std::memset(m_client_list[client_h]->m_req_join_party_name, 0, sizeof(m_client_list[client_h]->m_req_join_party_name));

		for(int i = 0; i < hb::shared::limits::MaxPartyMembers; i++)
			if (m_party_info[m_client_list[client_h]->m_party_id].index[i] == 0) {
				m_party_info[m_client_list[client_h]->m_party_id].index[i] = client_h;
				m_party_info[m_client_list[client_h]->m_party_id].total_members++;

				hb::logger::log("PartyID:{} member:{} In(Join) Total:{}", m_client_list[client_h]->m_party_id, client_h, m_party_info[m_client_list[client_h]->m_party_id].total_members);
				break;
			}

		for(int i = 1; i < MaxClients; i++)
			if ((i != client_h) && (m_client_list[i] != 0) && (m_client_list[i]->m_party_id != 0) && (m_client_list[i]->m_party_id == party_id)) {
				send_notify_msg(0, i, Notify::Party, 4, 1, 0, name);
			}
		break;
	}
}

void CGame::party_operation_result_dismiss(int client_h, char* name, int result, int party_id)
{
	
	// client_h     .

	switch (result) {
	case 0:
		break;

	case 1:
		if (client_h == 0) {
			// client_h  NULL        .
			for(int i = 1; i < MaxClients; i++)
				if ((m_client_list[i] != 0) && (hb_stricmp(m_client_list[i]->m_char_name, name) == 0)) {
					client_h = i;
					break;
				}

			for(int i = 0; i < hb::shared::limits::MaxPartyMembers; i++)
				if (m_party_info[party_id].index[i] == client_h) {
					m_party_info[party_id].index[i] = 0;
					m_party_info[party_id].total_members--;
					//testcode
					hb::logger::log("PartyID:{} member:{} Out Total:{}", party_id, client_h, m_party_info[party_id].total_members);
					break;
				}

			for(int i = 0; i < hb::shared::limits::MaxPartyMembers - 1; i++)
				if ((m_party_info[party_id].index[i] == 0) && (m_party_info[party_id].index[i + 1] != 0)) {
					m_party_info[party_id].index[i] = m_party_info[party_id].index[i + 1];
					m_party_info[party_id].index[i + 1] = 0;
				}

			if (m_client_list[client_h] != 0) {
				m_client_list[client_h]->m_party_id = 0;
				m_client_list[client_h]->m_party_status = PartyStatus::Null;
				m_client_list[client_h]->m_req_join_party_client_h = 0;
			}

			for(int i = 1; i < MaxClients; i++)
				if ((m_client_list[i] != 0) && (m_client_list[i]->m_party_id != 0) && (m_client_list[i]->m_party_id == party_id)) {
					send_notify_msg(0, i, Notify::Party, 6, 1, 0, name);
				}
			return;
		}

		if ((m_client_list[client_h] != 0) && (m_client_list[client_h]->m_party_status != PartyStatus::Processing)) return;
		if ((m_client_list[client_h] != 0) && (hb_stricmp(m_client_list[client_h]->m_char_name, name) != 0)) return;

		for(int i = 1; i < MaxClients; i++)
			if ((m_client_list[i] != 0) && (m_client_list[i]->m_party_id != 0) && (m_client_list[i]->m_party_id == party_id)) {
				send_notify_msg(0, i, Notify::Party, 6, 1, 0, name);
			}

		for(int i = 0; i < hb::shared::limits::MaxPartyMembers; i++)
			if (m_party_info[party_id].index[i] == client_h) {
				m_party_info[party_id].index[i] = 0;
				m_party_info[party_id].total_members--;
				//testcode
				hb::logger::log("PartyID:{} member:{} Out Total:{}", party_id, client_h, m_party_info[party_id].total_members);
				break;
			}

		for(int i = 0; i < hb::shared::limits::MaxPartyMembers - 1; i++)
			if ((m_party_info[party_id].index[i] == 0) && (m_party_info[party_id].index[i + 1] != 0)) {
				m_party_info[party_id].index[i] = m_party_info[party_id].index[i + 1];
				m_party_info[party_id].index[i + 1] = 0;
			}

		if (m_client_list[client_h] != 0) {
			m_client_list[client_h]->m_party_id = 0;
			m_client_list[client_h]->m_party_status = PartyStatus::Null;
			m_client_list[client_h]->m_req_join_party_client_h = 0;
		}
		break;
	}
}

void CGame::party_operation_result_delete(int party_id)
{
	

	for(int i = 0; i < hb::shared::limits::MaxPartyMembers; i++)
	{
		m_party_info[party_id].index[i] = 0;
		m_party_info[party_id].total_members = 0;
	}

	for(int i = 1; i < MaxClients; i++)
		if ((m_client_list[i] != 0) && (m_client_list[i]->m_party_id == party_id)) {
			send_notify_msg(0, i, Notify::Party, 2, 0, 0, 0);
			m_client_list[i]->m_party_id = 0;
			m_client_list[i]->m_party_status = PartyStatus::Null;
			m_client_list[i]->m_req_join_party_client_h = 0;
			//testcode
			hb::logger::log("Notify delete party: {}", i);
		}
}

void CGame::request_join_party_handler(int client_h, char* data, size_t msg_size)
{
	char   seps[] = "= \t\r\n";
	char* token, buff[256], msg_data[120], name[12];

	if (m_client_list[client_h] == 0) return;
	if (m_client_list[client_h]->m_party_status != PartyStatus::Null) return;
	if ((msg_size) <= 0) return;

	std::memset(buff, 0, sizeof(buff));
	memcpy(buff, data, msg_size);

	token = strtok(buff, seps);

	token = strtok(NULL, seps);
	if (token != 0) {
		std::memset(name, 0, sizeof(name));
		strcpy(name, token);
	}
	else return;

	for (int i = 1; i < MaxClients; i++)
		if ((m_client_list[i] != 0) && (hb_stricmp(m_client_list[i]->m_char_name, name) == 0)) {
			if ((m_client_list[i]->m_party_id == 0) || (m_client_list[i]->m_party_status != PartyStatus::Confirm)) {
				return;
			}

			std::memset(msg_data, 0, sizeof(msg_data));
			hb::net::PartyOpPayload partyOp{};
			partyOp.op_type = 3;
			partyOp.client_h = static_cast<uint16_t>(client_h);
			std::memcpy(partyOp.name, m_client_list[client_h]->m_char_name, sizeof(partyOp.name));
			partyOp.party_id = static_cast<uint16_t>(m_client_list[i]->m_party_id);
			std::memcpy(msg_data, &partyOp, sizeof(partyOp));
			party_operation(msg_data);
			return;
		}

	send_notify_msg(0, client_h, Notify::PlayerNotOnGame, 0, 0, 0, name);
}

void CGame::request_dismiss_party_handler(int client_h)
{
	char data[120]{};

	if (m_client_list[client_h] == 0) return;
	if (m_client_list[client_h]->m_party_status != PartyStatus::Confirm) return;

	hb::net::PartyOpPayload partyOp{};
	partyOp.op_type = 4;
	partyOp.client_h = static_cast<uint16_t>(client_h);
	std::memcpy(partyOp.name, m_client_list[client_h]->m_char_name, sizeof(partyOp.name));
	partyOp.party_id = static_cast<uint16_t>(m_client_list[client_h]->m_party_id);
	std::memcpy(data, &partyOp, sizeof(partyOp));
	party_operation(data);

	m_client_list[client_h]->m_party_status = PartyStatus::Processing;
}

void CGame::get_party_info_handler(int client_h)
{
	char data[120]{};

	if (m_client_list[client_h] == 0) return;
	if (m_client_list[client_h]->m_party_status != PartyStatus::Confirm) return;

	hb::net::PartyOpPayload partyOp{};
	partyOp.op_type = 5;
	partyOp.client_h = static_cast<uint16_t>(client_h);
	std::memcpy(partyOp.name, m_client_list[client_h]->m_char_name, sizeof(partyOp.name));
	partyOp.party_id = static_cast<uint16_t>(m_client_list[client_h]->m_party_id);
	std::memcpy(data, &partyOp, sizeof(partyOp));
	party_operation(data);
}

void CGame::party_operation_result_info(int client_h, char* name, int total, char* name_list)
{
	if (m_client_list[client_h] == 0) return;
	if (hb_stricmp(m_client_list[client_h]->m_char_name, name) != 0) return;
	if (m_client_list[client_h]->m_party_status != PartyStatus::Confirm) return;

	send_notify_msg(0, client_h, Notify::Party, 5, 1, total, name_list);
}

void CGame::request_delete_party_handler(int client_h)
{
	char data[120]{};

	if (m_client_list[client_h] == 0) return;
	if (m_client_list[client_h]->m_party_id != 0) {
		hb::net::PartyOpPayload partyOp{};
		partyOp.op_type = 4;
		partyOp.client_h = static_cast<uint16_t>(client_h);
		std::memcpy(partyOp.name, m_client_list[client_h]->m_char_name, sizeof(partyOp.name));
		partyOp.party_id = static_cast<uint16_t>(m_client_list[client_h]->m_party_id);
		std::memcpy(data, &partyOp, sizeof(partyOp));
		party_operation(data);
		m_client_list[client_h]->m_party_status = PartyStatus::Processing;
	}
}

void CGame::request_accept_join_party_handler(int client_h, int result)
{
	char data[120]{};
	int iH;

	if (m_client_list[client_h] == 0) return;

	switch (result) {
	case 0:
		iH = m_client_list[client_h]->m_req_join_party_client_h;
		if (m_client_list[iH] == 0) {
			return;
		}
		if (hb_stricmp(m_client_list[iH]->m_char_name, m_client_list[client_h]->m_req_join_party_name) != 0) {
			return;
		}
		if (m_client_list[iH]->m_party_status != PartyStatus::Processing) {
			return;
		}
		if ((m_client_list[iH]->m_req_join_party_client_h != client_h) || (hb_stricmp(m_client_list[iH]->m_req_join_party_name, m_client_list[client_h]->m_char_name) != 0)) {
			return;
		}

		send_notify_msg(0, iH, Notify::Party, 7, 0, 0, 0);
		//testcode
		hb::logger::log("Party join reject(3) ClientH:{} ID:{}", iH, m_client_list[iH]->m_party_id);

		m_client_list[iH]->m_party_id = 0;
		m_client_list[iH]->m_party_status = PartyStatus::Null;
		m_client_list[iH]->m_req_join_party_client_h = 0;
		std::memset(m_client_list[iH]->m_req_join_party_name, 0, sizeof(m_client_list[iH]->m_req_join_party_name));

		m_client_list[client_h]->m_req_join_party_client_h = 0;
		std::memset(m_client_list[client_h]->m_req_join_party_name, 0, sizeof(m_client_list[client_h]->m_req_join_party_name));
		break;

	case 1:
		if ((m_client_list[client_h]->m_party_status == PartyStatus::Confirm) && (m_client_list[client_h]->m_party_id != 0)) {
			iH = m_client_list[client_h]->m_req_join_party_client_h;
			if (m_client_list[iH] == 0) {
				return;
			}
			if (hb_stricmp(m_client_list[iH]->m_char_name, m_client_list[client_h]->m_req_join_party_name) != 0) {
				return;
			}
			if (m_client_list[iH]->m_party_status != PartyStatus::Processing) {
				return;
			}
			if ((m_client_list[iH]->m_req_join_party_client_h != client_h) || (hb_stricmp(m_client_list[iH]->m_req_join_party_name, m_client_list[client_h]->m_char_name) != 0)) {
				return;
			}

			std::memset(data, 0, sizeof(data));
			hb::net::PartyOpPayload partyOp{};
			partyOp.op_type = 3;
			partyOp.client_h = static_cast<uint16_t>(m_client_list[client_h]->m_req_join_party_client_h);
			std::memcpy(partyOp.name, m_client_list[client_h]->m_req_join_party_name, sizeof(partyOp.name));
			partyOp.party_id = static_cast<uint16_t>(m_client_list[client_h]->m_party_id);
			std::memcpy(data, &partyOp, sizeof(partyOp));
			party_operation(data);
		}
		else {
			iH = m_client_list[client_h]->m_req_join_party_client_h;
			if (m_client_list[iH] == 0) {
				return;
			}
			if (hb_stricmp(m_client_list[iH]->m_char_name, m_client_list[client_h]->m_req_join_party_name) != 0) {
				return;
			}
			if (m_client_list[iH]->m_party_status != PartyStatus::Processing) {
				return;
			}
			if ((m_client_list[iH]->m_req_join_party_client_h != client_h) || (hb_stricmp(m_client_list[iH]->m_req_join_party_name, m_client_list[client_h]->m_char_name) != 0)) {
				return;
			}

			if (m_client_list[client_h]->m_party_status == PartyStatus::Null) {
				request_create_party_handler(client_h);
			}
			else {
			}
		}
		break;

	case 2:
		if ((m_client_list[client_h]->m_party_id != 0) && (m_client_list[client_h]->m_party_status == PartyStatus::Confirm)) {
			request_dismiss_party_handler(client_h);
		}
		else {
			iH = m_client_list[client_h]->m_req_join_party_client_h;

			// NULL  .
			if ((m_client_list[iH] != 0) && (m_client_list[iH]->m_req_join_party_client_h == client_h) &&
				(hb_stricmp(m_client_list[iH]->m_req_join_party_name, m_client_list[client_h]->m_char_name) == 0)) {
				m_client_list[iH]->m_req_join_party_client_h = 0;
				std::memset(m_client_list[iH]->m_req_join_party_name, 0, sizeof(m_client_list[iH]->m_req_join_party_name));
			}

			m_client_list[client_h]->m_party_id = 0;
			m_client_list[client_h]->m_party_status = PartyStatus::Null;
			m_client_list[client_h]->m_req_join_party_client_h = 0;
			std::memset(m_client_list[client_h]->m_req_join_party_name, 0, sizeof(m_client_list[client_h]->m_req_join_party_name));
		}
		break;
	}
}

void CGame::party_operation(char* data)
{
	char name[12], msg_data[120]{};

	// Read the incoming payload as PartyOpPayload (superset of all request types)
	const auto& req = *reinterpret_cast<const hb::net::PartyOpPayload*>(data);
	uint16_t request_type = req.op_type;
	int gsch = static_cast<int>(req.client_h);
	std::memset(name, 0, sizeof(name));
	std::memcpy(name, req.name, sizeof(req.name));
	int party_id = static_cast<int>(req.party_id);

	hb::logger::log("Party Operation Type: {} Name: {} PartyID:{}", request_type, name, party_id);

	switch (request_type) {
	case 1: {
		party_id = m_party_manager->create_new_party_id(name);

		hb::net::PartyOpResultWithStatus result{};
		result.op_type = 1;
		result.result = static_cast<uint8_t>(party_id != 0 ? 1 : 0);
		result.client_h = static_cast<uint16_t>(gsch);
		std::memcpy(result.name, name, sizeof(result.name));
		result.party_id = static_cast<uint16_t>(party_id);
		std::memcpy(msg_data, &result, sizeof(result));

		party_operation_result_handler(msg_data);
		break;
	}

	case 2:
		break;

	case 3: {
		int ret = m_party_manager->add_member(party_id, name);

		hb::net::PartyOpResultWithStatus result{};
		result.op_type = 4;
		result.result = static_cast<uint8_t>(ret);
		result.client_h = static_cast<uint16_t>(gsch);
		std::memcpy(result.name, name, sizeof(result.name));
		result.party_id = static_cast<uint16_t>(party_id);
		std::memcpy(msg_data, &result, sizeof(result));

		party_operation_result_handler(msg_data);
		break;
	}

	case 4: {
		int ret = m_party_manager->remove_member(party_id, name);

		hb::net::PartyOpResultWithStatus result{};
		result.op_type = 6;
		result.result = static_cast<uint8_t>(ret);
		result.client_h = static_cast<uint16_t>(gsch);
		std::memcpy(result.name, name, sizeof(result.name));
		result.party_id = static_cast<uint16_t>(party_id);
		std::memcpy(msg_data, &result, sizeof(result));

		party_operation_result_handler(msg_data);
		break;
	}

	case 5:
		m_party_manager->check_party_member(gsch, party_id, name);
		break;

	case 6:
		m_party_manager->get_party_info(gsch, name, party_id);
		break;

	case 7:
		m_party_manager->set_server_change_status(name, party_id);
		break;
	}
}

/*void CGame::CalculateEnduranceDecrement(short target_h, short attacker_h, char target_type, int armor_type)
{
 short item_index;
 int down_value = 1, hammer_chance = 100;

	if (m_client_list[target_h] == 0) return;

	if ((target_type == hb::shared::owner_class::Player) && (m_client_list[attacker_h] != 0 )) {
		if ((target_type == hb::shared::owner_class::Player) && (m_client_list[target_h]->m_side != m_client_list[attacker_h]->m_side)) {
			switch (m_client_list[attacker_h]->m_using_weapon_skill) {
				case 14:
					if (m_client_list[attacker_h]->get_equipped_weapon_class() == hb::shared::item::weapon_class::hammer) {
						item_index = m_client_list[attacker_h]->m_item_equipment_status[to_int(EquipPos::TwoHand)];
						if ((item_index != -1) && (m_client_list[attacker_h]->m_item_list[item_index] != 0)) {
							if (m_client_list[attacker_h]->m_item_list[item_index]->m_id_num == 761) { // BattleHammer
								down_value = 30;
								break;
							}
							if (m_client_list[attacker_h]->m_item_list[item_index]->m_id_num == 762) { // GiantBattleHammer
								down_value = 35;
								break;
							}
							if (m_client_list[attacker_h]->m_item_list[item_index]->m_id_num == 843) { // BarbarianHammer
								down_value = 30;
								break;
							}
							if (m_client_list[attacker_h]->m_item_list[item_index]->m_id_num == 745) { // BarbarianHammer
								down_value = 30;
								break;
							}
						}
					}
					else {
						down_value = 20; break;
					}
				case 10: down_value = 3; break;
				default: down_value = 1; break;
				}

				if (m_client_list[target_h]->m_is_special_ability_enabled ) {
					switch (m_client_list[target_h]->m_special_ability_type)
						case 52: down_value = 0; hammer_chance = 0;
				}
			}
		}

		if ((m_client_list[target_h]->m_side != 0) && (m_client_list[target_h]->m_item_list[armor_type]->m_instance.cur_durability > 0)) {
				m_client_list[target_h]->m_item_list[armor_type]->m_instance.cur_durability -= down_value;
		}

		if ((m_client_list[target_h]->m_item_list[armor_type]->m_instance.cur_durability <= 0) || (m_client_list[target_h]->m_item_list[armor_type]->m_instance.cur_durability > 64000)) {
			m_client_list[target_h]->m_item_list[armor_type]->m_instance.cur_durability = 0;
			send_notify_msg(0, target_h, Notify::ItemDurabilityEnd, m_client_list[target_h]->m_item_list[armor_type]->m_equip_pos, armor_type, 0, 0);
			m_item_manager->release_item_handler(target_h, armor_type, true);
			return;
		}

	/*try
	{
		if (m_client_list[attacker_h] != 0) {
			if (target_type == hb::shared::owner_class::Player) {
				item_index = m_client_list[attacker_h]->m_item_equipment_status[to_int(EquipPos::TwoHand)];
				if ((item_index != -1) && (m_client_list[attacker_h]->m_item_list[item_index] != 0)) {
					if ((m_client_list[attacker_h]->m_item_list[item_index]->m_id_num == 617) || (m_client_list[attacker_h]->m_item_list[item_index]->m_id_num == 618) || (m_client_list[attacker_h]->m_item_list[item_index]->m_id_num == 619) || (m_client_list[attacker_h]->m_item_list[item_index]->m_id_num == 873) || (m_client_list[attacker_h]->m_item_list[item_index]->m_id_num == 874) || (m_client_list[attacker_h]->m_item_list[item_index]->m_id_num == 75) || (m_client_list[attacker_h]->m_item_list[item_index]->m_id_num == 76)) {
						m_client_list[attacker_h]->m_using_weapon_skill = 6;
						return;
					}
				}
			}
		}*/

		/*if (m_client_list[attacker_h] != 0) {
		if (target_type == hb::shared::owner_class::Player) {
		if ((m_client_list[attacker_h]->m_using_weapon_skill == 14) && (hammer_chance == 100)) {
			if (m_client_list[target_h]->m_item_list[armor_type]->m_durability < 2000) {
				hammer_chance = dice(6, (m_client_list[target_h]->m_item_list[armor_type]->m_durability - m_client_list[target_h]->m_item_list[armor_type]->m_instance.cur_durability));
			}
			else {
				hammer_chance = dice(4, (m_client_list[target_h]->m_item_list[armor_type]->m_durability - m_client_list[target_h]->m_item_list[armor_type]->m_instance.cur_durability));
			}

			if (m_client_list[attacker_h]->get_equipped_weapon_class() == hb::shared::item::weapon_class::hammer) {
				item_index = m_client_list[attacker_h]->m_item_equipment_status[to_int(EquipPos::TwoHand)];
				if ((item_index != -1) && (m_client_list[attacker_h]->m_item_list[item_index] != 0)) {
					if (m_client_list[attacker_h]->m_item_list[item_index]->m_id_num == 761) { // BattleHammer
						hammer_chance = hammer_chance/2;
					}
					if (m_client_list[attacker_h]->m_item_list[item_index]->m_id_num == 762) { // GiantBattleHammer
						hammer_chance = ((hammer_chance*10)/9);
					}
					if (m_client_list[attacker_h]->m_item_list[item_index]->m_id_num == 843) { // GiantBattleHammer
						hammer_chance = ((hammer_chance*10)/9);
					}
					if (m_client_list[attacker_h]->m_item_list[item_index]->m_id_num == 745) { // GiantBattleHammer
						hammer_chance = ((hammer_chance*10)/9);
					}
				}
			}
			if ((m_client_list[target_h]->m_item_list[armor_type]->m_id_num == 622) || (m_client_list[target_h]->m_item_list[armor_type]->m_id_num == 621)) {
				hammer_chance = 0;
			}
			if (m_client_list[target_h]->m_item_list[armor_type]->m_instance.cur_durability < hammer_chance) {
				std::snprintf(G_cTxt, sizeof(G_cTxt), "(hammer_chance (%d), target armor endurance (%d)!", hammer_chance, m_client_list[target_h]->m_item_list[armor_type]->m_instance.cur_durability);
				PutLogList(G_cTxt);
				m_item_manager->release_item_handler(target_h, armor_type, true);
				send_notify_msg(0, target_h, Notify::ItemReleased, m_client_list[target_h]->m_item_list[armor_type]->m_equip_pos, armor_type, 0, 0);
				return;
			}
		}
		}
		}
	//catch(...)
	{

	}
}*/

// October 19, 2004 - 3.51 translated

// October 19, 2004 - 3.51 translated

// October 19, 2004 - 3.51 translated

// October 19, 2004 - 3.51 translated

// October 19, 2004 - 3.51 translated

// October 19,2004 - 3.51 translated

/*
void CGame::StormBringer(int client_h, short dX, short dY)
{
	char owner_type;
	short owner, appr2, attacker_weapon;
	int  damage, temp, v1, v2, v3;

	//ArchAngel Fix

	if (m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::RightHand)] != -1) {
		m_map_list[m_client_list[client_h]->m_map_index]->get_owner(&owner, &owner_type, dX, dY);

		temp = m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::RightHand)];
		appr2 = (short)((m_client_list[client_h]->m_appearance.is_walking) >> 12);

		if (m_client_list[client_h]->m_item_list[temp]->m_id_num == hb::shared::item::ItemId::StormBringer){

			switch (owner_type) {
			case hb::shared::owner_class::Player:
				if (appr2 != 0) {
					v1 = m_client_list[client_h]->m_attack_dice_throw_l;
					v2 = m_client_list[client_h]->m_attack_dice_range_l;
					v3 = m_client_list[client_h]->m_attack_bonus_l;

					if (m_client_list[client_h]->m_magic_effect_status[ hb::shared::magic::Berserk ] != 0){
						damage = dice(v1*2,v2*2)+v3;
					}
					else{
						damage = dice(v1,v2)+v3;
					}

					m_client_list[owner]->m_hp -= damage;
					if (m_client_list[owner]->m_hp <= 0){
						attacker_weapon = 1;
						m_client_list[owner]->m_hp = 0;

						m_client_list[owner]->m_is_killed = true;
						m_client_list[owner]->m_last_damage = damage;
						send_notify_msg(0, owner, Notify::Hp, 0, 0, 0, 0);
						send_event_to_near_client_type_a(owner, hb::shared::owner_class::Player, MsgId::EventMotion, Type::Dying, damage, attacker_weapon, 0);
						m_map_list[m_client_list[owner]->m_map_index]->clear_owner(14, owner, hb::shared::owner_class::Player, m_client_list[owner]->m_x, m_client_list[owner]->m_y);
						m_map_list[m_client_list[owner]->m_map_index]->set_dead_owner(owner, hb::shared::owner_class::Player, m_client_list[owner]->m_x, m_client_list[owner]->m_y);
					}
					else{
						send_notify_msg(0, owner, Notify::Hp, 0, 0, 0, 0);
						send_event_to_near_client_type_a(owner, hb::shared::owner_class::Player, MsgId::EventMotion, Type::Damage, damage, 0, 0);
					}
				}
				break;

			case hb::shared::owner_class::Npc:
				if (appr2 != 0) {
					if (m_npc_list[owner]->m_size == 0){
						v1 = m_client_list[client_h]->m_attack_dice_throw_sm;
						v2 = m_client_list[client_h]->m_attack_dice_range_sm;
						v3 = m_client_list[client_h]->m_attack_bonus_sm;
					}
					else if (m_npc_list[owner]->m_size == 1){
						v1 = m_client_list[client_h]->m_attack_dice_throw_l;
						v2 = m_client_list[client_h]->m_attack_dice_range_l;
						v3 = m_client_list[client_h]->m_attack_bonus_l;
					}

					if (m_client_list[client_h]->m_magic_effect_status[ hb::shared::magic::Berserk ] != 0){
						damage = dice(v1*2,v2*2)+v3;
					}
					else{
						damage = dice(v1,v2)+v3;
					}

					m_npc_list[owner]->m_hp -= damage;
					if (m_npc_list[owner]->m_hp <= 0){
						attacker_weapon = 1;
						m_npc_list[owner]->m_hp = 0;

						m_npc_list[owner]->m_behavior_turn_count = 0;
						m_npc_list[owner]->m_behavior = Behavior::Dead;
						m_npc_list[owner]->m_dead_time = GameClock::GetTimeMS();
						send_event_to_near_client_type_a(owner, hb::shared::owner_class::Npc, MsgId::EventMotion, Type::Dying, damage, attacker_weapon, 0);
						m_map_list[m_npc_list[owner]->m_map_index]->clear_owner(10, owner, hb::shared::owner_class::Npc, m_npc_list[owner]->m_x, m_npc_list[owner]->m_y);
						m_map_list[m_npc_list[owner]->m_map_index]->set_dead_owner(owner, hb::shared::owner_class::Npc, m_npc_list[owner]->m_x, m_npc_list[owner]->m_y);
					}
					else{
						send_event_to_near_client_type_a(owner, hb::shared::owner_class::Npc, MsgId::EventMotion, Type::Damage, damage, 0, 0);
					}
				}
				break;
			}
		}
	}
}*/

bool CGame::check_character_data(int client_h)
{
	

	if ((m_client_list[client_h]->m_str > CharPointLimit) || (m_client_list[client_h]->m_vit > CharPointLimit) || (m_client_list[client_h]->m_dex > CharPointLimit) ||
		(m_client_list[client_h]->m_mag > CharPointLimit) || (m_client_list[client_h]->m_int > CharPointLimit) || (m_client_list[client_h]->m_charisma > CharPointLimit)) {
		try
		{
			hb::logger::warn<log_channel::security>("Packet Editing: ({}) Player: ({}) stat points are greater then server accepts.", m_client_list[client_h]->m_ip_address, m_client_list[client_h]->m_char_name);
			return false;
		}
		catch (...)
		{

		}
	}

	if ((m_client_list[client_h]->m_level > m_max_level) ) {
		try
		{
			hb::logger::warn<log_channel::security>("Packet Editing: ({}) Player: ({}) level above max server level.", m_client_list[client_h]->m_ip_address, m_client_list[client_h]->m_char_name);
			return false;
		}
		catch (...)
		{

		}
	}

	if (m_client_list[client_h]->m_exp < 0) {
		try
		{
			hb::logger::warn<log_channel::security>("Packet Editing: ({}) Player: ({}) experience is below 0 - (Exp:{}).", m_client_list[client_h]->m_ip_address, m_client_list[client_h]->m_char_name, m_client_list[client_h]->m_exp);
			return false;
		}
		catch (...)
		{

		}
	}

	if ((m_client_list[client_h]->m_hp > get_max_hp(client_h)) ) {
		try
		{
			if (m_client_list[client_h]->m_item_list[(m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::RightHand)])] != 0) {
				if ((m_client_list[client_h]->m_item_list[(m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::RightHand)])]->m_id_num == 492) || (m_client_list[client_h]->m_item_list[(m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::RightHand)])]->m_id_num == 491)) {
					if (m_client_list[client_h]->m_hp > (4 * (get_max_hp(client_h) / 5))) {

					}
				}
			}
			else if (m_client_list[client_h]->m_item_list[(m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::TwoHand)])] != 0) {
				if ((m_client_list[client_h]->m_item_list[(m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::TwoHand)])]->m_id_num == 490)) {
					if (m_client_list[client_h]->m_hp > (4 * (get_max_hp(client_h) / 5))) {

					}
				}
			}
			else {
				hb::logger::warn<log_channel::security>("Packet Editing: ({}) Player: ({}) HP: current/maximum ({}/{}).", m_client_list[client_h]->m_ip_address, m_client_list[client_h]->m_char_name, m_client_list[client_h]->m_hp, get_max_hp(client_h));
				return false;
			}
		}
		catch (...)
		{

		}
	}

	if ((m_client_list[client_h]->m_mp > get_max_mp(client_h)) ) {
		try
		{
			hb::logger::warn<log_channel::security>("Packet Editing: ({}) Player: ({}) MP: current/maximum ({}/{}).", m_client_list[client_h]->m_ip_address, m_client_list[client_h]->m_char_name, m_client_list[client_h]->m_mp, get_max_mp(client_h));
			return false;
		}
		catch (...)
		{

		}
	}

	if ((m_client_list[client_h]->m_sp > get_max_sp(client_h)) ) {
		try
		{
			hb::logger::warn<log_channel::security>("Packet Editing: ({}) Player: ({}) SP: current/maximum ({}/{}).", m_client_list[client_h]->m_ip_address, m_client_list[client_h]->m_char_name, m_client_list[client_h]->m_sp, get_max_sp(client_h));
			return false;
		}
		catch (...)
		{

		}
	}

	try
	{
		for(int i = 0; i < MaxBanned; i++) {
			if (strlen(m_banned_list[i].banned_ip_address) == 0) break; //No more GM's on list
			if ((strlen(m_banned_list[i].banned_ip_address)) == (strlen(m_client_list[client_h]->m_ip_address))) {
				if (memcmp(m_banned_list[i].banned_ip_address, m_client_list[client_h]->m_ip_address, strlen(m_client_list[client_h]->m_ip_address)) == 0) {
					hb::logger::log("Client Rejected: Banned: ({})", m_client_list[client_h]->m_ip_address);
					return false;
				}
				else {

				}
			}
		}
	}
	catch (...)
	{

	}

	return true;
}

void CGame::force_recall_process() {
	
	int map_side = 0;

	uint32_t time;

	time = GameClock::GetTimeMS();

	for(int i = 1; i < MaxClients; i++) {
		if (m_client_list[i] != 0) {
			if (m_client_list[i]->m_is_init_complete) {
				//force recall in enemy buidlings at crusade
				map_side = get_map_location_side(m_map_list[m_client_list[i]->m_map_index]->m_name);
				if ((memcmp(m_client_list[i]->m_location, "are", 3) == 0) && (map_side == 2) && (m_is_crusade_mode)) {
					request_teleport_handler(i, "2   ", "aresden", -1, -1);
				}
				if ((memcmp(m_client_list[i]->m_location, "elv", 3) == 0) && (map_side == 1) && (m_is_crusade_mode)) {
					request_teleport_handler(i, "2   ", "elvine", -1, -1);
				}

				//remove mim in building
				if ((memcmp(m_map_list[m_client_list[i]->m_map_index]->m_name, "wrhus", 5) == 0)
					|| (strcmp(m_map_list[m_client_list[i]->m_map_index]->m_name, "gshop_1") == 0)
					|| (strcmp(m_map_list[m_client_list[i]->m_map_index]->m_name, "bsmith_1") == 0)
					|| (strcmp(m_map_list[m_client_list[i]->m_map_index]->m_name, "cath_1") == 0)
					|| (strcmp(m_map_list[m_client_list[i]->m_map_index]->m_name, "cmdhall_1") == 0)
					|| (strcmp(m_map_list[m_client_list[i]->m_map_index]->m_name, "cityhall_1") == 0)
					|| (strcmp(m_map_list[m_client_list[i]->m_map_index]->m_name, "gshop_2") == 0)
					|| (strcmp(m_map_list[m_client_list[i]->m_map_index]->m_name, "bsmith_2") == 0)
					|| (strcmp(m_map_list[m_client_list[i]->m_map_index]->m_name, "cath_2") == 0)
					|| (strcmp(m_map_list[m_client_list[i]->m_map_index]->m_name, "cmdhall_2") == 0)
					|| (strcmp(m_map_list[m_client_list[i]->m_map_index]->m_name, "cityhall_2") == 0)
					|| (memcmp(m_map_list[m_client_list[i]->m_map_index]->m_name, "wzdtwr", 6) == 0)
					|| (memcmp(m_map_list[m_client_list[i]->m_map_index]->m_name, "gldhall", 7) == 0))
				{
					//m_status_effect_manager->set_illusion_flag(i, hb::shared::owner_class::Player, false);
					if (m_client_list[i]->m_status.illusion_movement) {
						m_status_effect_manager->set_illusion_movement_flag(i, hb::shared::owner_class::Player, false);
						m_delay_event_manager->remove_from_delay_event_list(i, hb::shared::owner_class::Player, hb::shared::magic::Confuse);
						m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Confuse, time + 2, i, hb::shared::owner_class::Player, 0, 0, 0, 4, 0, 0);
					}
				}
			}
			//check gizon errors
			if (m_client_list[i]->m_level < m_max_level) {
				if (m_client_list[i]->m_gizon_item_upgrade_left > 0) {
					m_client_list[i]->m_gizon_item_upgrade_left = 0;
				}
			}
		}
	}
}

//in stat change, check skillpoints

bool CGame::is_enemy_zone(int i) {
	if (memcmp(m_client_list[i]->m_location, "elv", 3) == 0) {
		if ((strcmp(m_map_list[m_client_list[i]->m_map_index]->m_location_name, "aresden") == 0) || (strcmp(m_map_list[m_client_list[i]->m_map_index]->m_location_name, "aresdend1") == 0) || (strcmp(m_map_list[m_client_list[i]->m_map_index]->m_location_name, "areuni") == 0) || (strcmp(m_map_list[m_client_list[i]->m_map_index]->m_location_name, "huntzone2") == 0) || (strcmp(m_map_list[m_client_list[i]->m_map_index]->m_location_name, "huntzone4") == 0)) {
			return true;
		}
	}
	else if (memcmp(m_client_list[i]->m_location, "are", 3) == 0) {
		if ((strcmp(m_map_list[m_client_list[i]->m_map_index]->m_location_name, "elvine") == 0) || (strcmp(m_map_list[m_client_list[i]->m_map_index]->m_location_name, "elvined1") == 0) || (strcmp(m_map_list[m_client_list[i]->m_map_index]->m_location_name, "elvuni") == 0) || (strcmp(m_map_list[m_client_list[i]->m_map_index]->m_location_name, "huntzone1") == 0) || (strcmp(m_map_list[m_client_list[i]->m_map_index]->m_location_name, "huntzone3") == 0)) {
			return true;
		}
	}
	return false;
}

void CGame::lotery_handler(int client_h)
{
	CItem* item;
	int     item_id;
	if (m_client_list[client_h] == 0) return;
	switch (dice(1, 17)) {
	case 1:item_id = 656; break; // XelimaStone
	case 2:item_id = 657; break; // MerienStone
	case 3:item_id = 650; break; // ZemstoneOfSacrifice
	case 4:item_id = 881; break; // ArmorDye(Indigo)
	case 5:item_id = 882; break; // ArmorDye(CrimsonRed)
	case 6:item_id = 883; break; // ArmorDye(Gold)
	case 7:item_id = 884; break; // ArmorDye(Aqua)
	case 8:item_id = 885; break; // ArmorDye(Pink)
	case 9:item_id = 886; break; // ArmorDye(Violet)
	case 10:item_id = 887; break; // ArmorDye(Blue)
	case 11:item_id = 888; break; // ArmorDye(Khaki)
	case 12:item_id = 889; break; // ArmorDye(Yellow)
	case 13:item_id = 890; break; // ArmorDye(Red)
	case 14:item_id = 971; break; // ArmorDye(Green)
	case 15:item_id = 972; break; // ArmorDye(Black)
	case 16:item_id = 973; break; // ArmorDye(Knight)
	case 17:item_id = 970; break; // CritCandy
	}

	//chance
	if (dice(1, 120) <= 3) item_id = 650;//ZemstoneOfSacrifice
	//chance

	item = new CItem;
	if (m_item_manager->init_item_attr(item, item_id) == false) {
		delete item;
	}
	else {
		m_map_list[m_client_list[client_h]->m_map_index]->set_item(m_client_list[client_h]->m_x,
			m_client_list[client_h]->m_y, item);
		send_ground_item_event(CommonType::ItemDrop, m_client_list[client_h]->m_map_index,
			m_client_list[client_h]->m_x, m_client_list[client_h]->m_y, item);
	}

}

//Angel Code By SlammeR(I dont know if it works)
/*void CGame::GetAngelMantleHandler(int client_h,int item_id,char * string)
{
 int   i, num, ret, erase_req;
 char  * cp, data[256], item_name[hb::shared::limits::ItemNameLen];
 CItem * item;
 uint32_t * dwp;
 short * sp;
 uint16_t  * wp;

	if (m_client_list[client_h] == 0) return;
	if (m_client_list[client_h]->m_gizon_item_upgrade_left < 5) return;
	if (m_client_list[client_h]->m_side == 0) return;
	if (m_item_manager->get_item_space_left(client_h) == 0) {
		m_item_manager->send_item_notify_msg(client_h,	Notify::CannotCarryMoreItem, 0, 0);
		return;
	}

	//Prevents a crash if item dosent exist
	if (m_item_config_list[item_id] == 0)  return;

	switch(item_id) {
	//Angels
	case 908: //AngelicPendant(STR)
	case 909: //AngelicPendant(DEX)
		case 910: //AngelicPendant(INT)
		case 911: //AngelicPendant(MAG)
		if(m_client_list[client_h]->m_gizon_item_upgrade_left<5) return;
		m_client_list[client_h]->m_gizon_item_upgrade_left -= 5;
		break;

  default:
	 return;
	 break;
  }

  std::memset(item_name, 0, sizeof(item_name));
  memcpy(item_name,m_item_config_list[item_id]->m_name, hb::shared::limits::ItemNameLen - 1);

  num = 1;
  for(int i = 1; i <= num; i++)
  {
	 item = new CItem;
	 if (m_item_manager->init_item_attr(item, item_name) == false)
	 {
		delete item;
	 }
	 else {

		if (m_item_manager->add_client_item_list(client_h, item, &erase_req) ) {
		   if (m_client_list[client_h]->m_cur_weight_load < 0) m_client_list[client_h]->m_cur_weight_load = 0;

		   std::snprintf(G_cTxt, sizeof(G_cTxt), "(*) get Angel : Char(%s) Player-Majestic-Points(%d) Angel Obtained(%s)", m_client_list[client_h]->m_char_name, m_client_list[client_h]->m_gizon_item_upgrade_left, item_name);
		   PutLogFileList(G_cTxt);

		   item->set_touch_effect_type(TouchEffectType::UniqueOwner);
		   item->m_instance.touch_effect_value1 = m_client_list[client_h]->m_char_id_num1;
		   item->m_instance.touch_effect_value2 = m_client_list[client_h]->m_char_id_num2;
		   item->m_instance.touch_effect_value3 = m_client_list[client_h]->m_char_id_num3;

		   ret = m_item_manager->send_item_notify_msg(client_h, Notify::ItemObtained, item, 0);

		   if (erase_req == 1) delete item;

		   calc_total_weight(client_h);

		   switch (ret) {
		   case sock::Event::QueueFull:
		   case sock::Event::SocketError:
		   case sock::Event::CriticalError:
		   case sock::Event::SocketClosed:
			  delete_client(client_h, true, true);
			  return;
		   }

		   send_notify_msg(0, client_h, Notify::GizonItemUpgradeLeft, m_client_list[client_h]->m_gizon_item_upgrade_left, 0, 0, 0);
		}
		else
		{
		   delete item;

		   calc_total_weight(client_h);

	   ret = m_item_manager->send_item_notify_msg(client_h, Notify::CannotCarryMoreItem, 0, 0);

		   switch (ret) {
		   case sock::Event::QueueFull:
		   case sock::Event::SocketError:
		   case sock::Event::CriticalError:
		   case sock::Event::SocketClosed:

			  delete_client(client_h, true, true);
			  return;
		   }
		}
	 }
   }
}*/

/*int CGame::angel_equip(int client_h)
{
 int temp;
 CItem * angel_temp;
	temp = m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::LeftFinger)];
	angel_temp = m_client_list[client_h]->m_item_list[temp];
	if ((temp != -1) && (angel_temp != 0)) {
		if(angel_temp->m_id_num >= 908){ //AngelicPendant(STR)
				if(angel_temp->m_id_num >= 909){ //AngelicPendant(DEX)
				if(angel_temp->m_id_num >= 910){ //AngelicPendant(INT)
				if(angel_temp->m_id_num >= 911){ //AngelicPendant(MAG)

				return angel_temp->m_id_num;
			} else {
				return 0;
				}
				}
				}
		}
	}
}*/

/*void CGame::CheckAngelUnequip(int client_h,int angel_id)
{
 int temp;
 CItem * angel_temp;

	temp = m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::LeftFinger)];
	angel_temp = m_client_list[client_h]->m_item_list[temp];
	if ((temp != -1) && (angel_temp->m_id_num != angel_id)) {
		angel_temp->m_id_num = angel_id;
	}

}*/

/*********************************************************************************************************************
**  bool CGame::set_angel_flag(short owner_h, char owner_type, int status, temp)		Snoopy			**
** description	  :: Sets the staus to send or not Angels to every client							**
*********************************************************************************************************************/

/*********************************************************************************************************************
**  bool CGame::get_angel_handler(int client_h, char * data, size_t msg_size)										**
** description	  :: Reversed and coded by Snoopy																	**
*********************************************************************************************************************/
void CGame::get_angel_handler(int client_h, char* data, size_t msg_size)
{
	int   angel, item_id;
	CItem* item;
	int   ret, erase_req;
	if (m_client_list[client_h] == 0)					 return;
	if (m_client_list[client_h]->m_is_init_complete == false) return;
	if (m_item_manager->get_item_space_left(client_h) == 0)
	{
		m_item_manager->send_item_notify_msg(client_h, Notify::CannotCarryMoreItem, 0, 0);
		return;
	}
	const auto* req = hb::net::PacketCast<hb::net::PacketRequestAngel>(data, sizeof(hb::net::PacketRequestAngel));
	if (!req) return;
	angel = req->angel_id;

	if (m_client_list[client_h]->m_gizon_item_upgrade_left < 5) return;

	switch (angel) {
	case 1: item_id = hb::shared::item::ItemId::AngelicPendantSTR; break;
	case 2: item_id = hb::shared::item::ItemId::AngelicPendantDEX; break;
	case 3: item_id = hb::shared::item::ItemId::AngelicPendantINT; break;
	case 4: item_id = hb::shared::item::ItemId::AngelicPendantMAG; break;
	default:
		hb::logger::log("NPC craft request for invalid item");
		return;
	}

	hb::logger::log("PC({}) requesting Angel ({}, ItemID:{}). {}({} {})", m_client_list[client_h]->m_char_name, angel, item_id, m_client_list[client_h]->m_map_name, m_client_list[client_h]->m_x, m_client_list[client_h]->m_y);

	item = new CItem;
	if ((m_item_manager->init_item_attr(item, item_id)))
	{
		m_client_list[client_h]->m_gizon_item_upgrade_left -= 5;

		item->set_touch_effect_type(TouchEffectType::UniqueOwner);
		item->m_instance.touch_effect_value1 = m_client_list[client_h]->m_char_id_num1;
		item->m_instance.touch_effect_value2 = m_client_list[client_h]->m_char_id_num2;
		item->m_instance.touch_effect_value3 = m_client_list[client_h]->m_char_id_num3;
		if (m_item_manager->add_client_item_list(client_h, item, &erase_req))
		{
			if (m_client_list[client_h]->m_cur_weight_load < 0) m_client_list[client_h]->m_cur_weight_load = 0;

			hb::logger::log<log_channel::events>("get Angel : Char({}) Player-Majestic-Points({}) Angel Obtained(ID:{})", m_client_list[client_h]->m_char_name, m_client_list[client_h]->m_gizon_item_upgrade_left, item_id);

			ret = m_item_manager->send_item_notify_msg(client_h, Notify::ItemObtained, item, 0);

			calc_total_weight(client_h);

			switch (ret) {
			case sock::Event::QueueFull:
			case sock::Event::SocketError:
			case sock::Event::CriticalError:
			case sock::Event::SocketClosed:
				delete_client(client_h, true, true);
				return;
			}

			send_notify_msg(0, client_h, Notify::GizonItemUpgradeLeft, m_client_list[client_h]->m_gizon_item_upgrade_left, 0, 0, 0);
		}
		else
		{
			delete item;

			calc_total_weight(client_h);

			ret = m_item_manager->send_item_notify_msg(client_h, Notify::CannotCarryMoreItem, 0, 0);

			switch (ret) {
			case sock::Event::QueueFull:
			case sock::Event::SocketError:
			case sock::Event::CriticalError:
			case sock::Event::SocketClosed:
				delete_client(client_h, true, true);
				return;
			}
		}
	}
	else
	{
		hb::logger::log("get_angel_handler: init_item_attr failed for ItemID {}. Item not found in config.", item_id);
		delete item;
	}
}

//50Cent - Repair All

void CGame::apply_server_config(const server_config& cfg)
{
	m_server_config = cfg;

	// Drop rates
	m_primary_drop_rate = cfg.drop_rates.primary;
	m_gold_drop_rate = cfg.drop_rates.gold;
	m_secondary_drop_rate = cfg.drop_rates.secondary;
	m_rep_drop_modifier = static_cast<char>(cfg.drop_rates.rep_modifier);

	// Timing
	m_client_timeout = cfg.timing.client_timeout_ms;
	m_stamina_regen_interval = cfg.timing.stamina_regen_ms;
	m_poison_damage_interval = cfg.timing.poison_damage_ms;
	m_health_regen_interval = cfg.timing.health_regen_ms;
	m_mana_regen_interval = cfg.timing.mana_regen_ms;
	m_hunger_consume_interval = cfg.timing.hunger_consume_ms;
	m_summon_creature_duration = cfg.timing.summon_duration_ms;
	m_autosave_interval = cfg.timing.autosave_ms;
	m_lag_protection_interval = cfg.timing.lag_protection_ms;

	// Combat
	m_enemy_kill_mode = (cfg.combat.enemy_kill_mode == "deathmatch");
	m_enemy_kill_adjust = cfg.combat.enemy_kill_adjust;
	m_slate_success_rate = static_cast<short>(cfg.combat.slate_success_rate);
	m_minimum_hit_ratio = cfg.combat.min_hit_ratio;
	m_maximum_hit_ratio = cfg.combat.max_hit_ratio;

	// Character
	m_base_stat_value = cfg.character.base_stat_value;
	m_max_creation_stat_value = cfg.character.max_creation_stat_value;
	m_creation_stat_points = cfg.character.creation_stat_points;
	m_base_stat_total = m_base_stat_value * 6 + m_creation_stat_points;
	m_levelup_stat_gain = cfg.character.levelup_stat_gain;
	m_max_level = cfg.character.max_level;
	m_max_stat_value = cfg.character.max_stat_value;

	// Gameplay
	m_nighttime_duration = cfg.gameplay.nighttime_duration;
	m_grand_magic_mana_consumption = cfg.gameplay.grand_magic_mana_cost;
	m_max_construction_points = cfg.gameplay.max_construction_points;
	m_max_summon_points = cfg.gameplay.max_summon_points;
	m_max_war_contribution = cfg.gameplay.max_war_contribution;
	m_max_bank_items = cfg.gameplay.max_bank_items;

	// Raid schedule
	m_raid_time_monday = cfg.raid_schedule.monday;
	m_raid_time_tuesday = cfg.raid_schedule.tuesday;
	m_raid_time_wednesday = cfg.raid_schedule.wednesday;
	m_raid_time_thursday = cfg.raid_schedule.thursday;
	m_raid_time_friday = cfg.raid_schedule.friday;
	m_raid_time_saturday = cfg.raid_schedule.saturday;
	m_raid_time_sunday = cfg.raid_schedule.sunday;

	// Realm
	std::snprintf(m_realm_name, sizeof(m_realm_name), "%s", cfg.realm.name.c_str());
	std::snprintf(m_login_listen_ip, sizeof(m_login_listen_ip), "%s", cfg.realm.login_listen_ip.c_str());
	m_login_listen_port = cfg.realm.login_listen_port;
	std::snprintf(m_game_listen_ip, sizeof(m_game_listen_ip), "%s", cfg.realm.game_listen_ip.c_str());
	m_game_listen_port = cfg.realm.game_listen_port;
	if (!cfg.realm.game_connection_ip.empty())
	{
		std::snprintf(m_game_connection_ip, sizeof(m_game_connection_ip), "%s", cfg.realm.game_connection_ip.c_str());
		m_game_connection_port = cfg.realm.game_connection_port;
	}
	else
	{
		m_game_connection_ip[0] = '\0';
		m_game_connection_port = 0;
	}
}

void CGame::enforce_max_level(int new_max)
{
	int base = m_base_stat_value;
	int gain = m_levelup_stat_gain;
	int base_total = m_base_stat_total;
	int online_count = 0;

	// Online characters
	for (int i = 1; i < hb::server::config::MaxClients; i++)
	{
		if (m_client_list[i] == nullptr) continue;
		if (!m_client_list[i]->m_is_init_complete) continue;
		if (m_client_list[i]->m_level <= new_max) continue;

		m_client_list[i]->m_level = new_max;
		m_client_list[i]->m_str = base;
		m_client_list[i]->m_int = base;
		m_client_list[i]->m_vit = base;
		m_client_list[i]->m_dex = base;
		m_client_list[i]->m_mag = base;
		m_client_list[i]->m_charisma = base;
		m_client_list[i]->m_levelup_pool = (new_max - 1) * gain - (base * 6 - base_total);

		reprocess_online_player(i);
		g_login->local_save_player_data(i);
		online_count++;
	}

	if (online_count > 0)
		hb::logger::log("enforce_max_level: clamped {} online character(s) to level {}", online_count, new_max);

	// Offline characters — iterate all account DBs
	int offline_count = 0;
	std::error_code ec;
	for (const auto& entry : std::filesystem::directory_iterator("accounts", ec))
	{
		if (!entry.is_regular_file() || entry.path().extension() != ".db")
			continue;

		std::string db_path = entry.path().string();
		sqlite3* db = nullptr;
		if (sqlite3_open(db_path.c_str(), &db) != SQLITE_OK)
		{
			if (db) sqlite3_close(db);
			continue;
		}

		const char* sql =
			"UPDATE characters SET "
			"level = ?1, str = ?2, intl = ?2, vit = ?2, dex = ?2, mag = ?2, chr = ?2, "
			"lu_pool = (?1 - 1) * ?3 - (?2 * 6 - ?4) "
			"WHERE level > ?1";

		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK)
		{
			sqlite3_bind_int(stmt, 1, new_max);
			sqlite3_bind_int(stmt, 2, base);
			sqlite3_bind_int(stmt, 3, gain);
			sqlite3_bind_int(stmt, 4, base_total);
			sqlite3_step(stmt);
			offline_count += sqlite3_changes(db);
			sqlite3_finalize(stmt);
		}

		sqlite3_close(db);
	}

	if (offline_count > 0)
		hb::logger::log("enforce_max_level: clamped {} offline character(s) to level {}", offline_count, new_max);
}

void CGame::enforce_max_stat_value(int new_max)
{
	int online_count = 0;

	// Online characters
	for (int i = 1; i < hb::server::config::MaxClients; i++)
	{
		if (m_client_list[i] == nullptr) continue;
		if (!m_client_list[i]->m_is_init_complete) continue;

		bool clamped = false;
		int* stats[] = {
			&m_client_list[i]->m_str, &m_client_list[i]->m_int,
			&m_client_list[i]->m_vit, &m_client_list[i]->m_dex,
			&m_client_list[i]->m_mag, &m_client_list[i]->m_charisma
		};

		for (auto* stat : stats)
		{
			if (*stat > new_max)
			{
				m_client_list[i]->m_levelup_pool += (*stat - new_max);
				*stat = new_max;
				clamped = true;
			}
		}

		if (clamped)
		{
			reprocess_online_player(i);
			g_login->local_save_player_data(i);
			online_count++;
		}
	}

	if (online_count > 0)
		hb::logger::log("enforce_max_stat_value: clamped {} online character(s) to max stat {}", online_count, new_max);

	// Offline characters — iterate all account DBs
	int offline_count = 0;
	std::error_code ec;
	for (const auto& entry : std::filesystem::directory_iterator("accounts", ec))
	{
		if (!entry.is_regular_file() || entry.path().extension() != ".db")
			continue;

		std::string db_path = entry.path().string();
		sqlite3* db = nullptr;
		if (sqlite3_open(db_path.c_str(), &db) != SQLITE_OK)
		{
			if (db) sqlite3_close(db);
			continue;
		}

		const char* sql =
			"UPDATE characters SET "
			"lu_pool = lu_pool "
			"+ CASE WHEN str > ?1 THEN str - ?1 ELSE 0 END "
			"+ CASE WHEN intl > ?1 THEN intl - ?1 ELSE 0 END "
			"+ CASE WHEN vit > ?1 THEN vit - ?1 ELSE 0 END "
			"+ CASE WHEN dex > ?1 THEN dex - ?1 ELSE 0 END "
			"+ CASE WHEN mag > ?1 THEN mag - ?1 ELSE 0 END "
			"+ CASE WHEN chr > ?1 THEN chr - ?1 ELSE 0 END, "
			"str = MIN(str, ?1), intl = MIN(intl, ?1), vit = MIN(vit, ?1), "
			"dex = MIN(dex, ?1), mag = MIN(mag, ?1), chr = MIN(chr, ?1) "
			"WHERE str > ?1 OR intl > ?1 OR vit > ?1 OR dex > ?1 OR mag > ?1 OR chr > ?1";

		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK)
		{
			sqlite3_bind_int(stmt, 1, new_max);
			sqlite3_step(stmt);
			offline_count += sqlite3_changes(db);
			sqlite3_finalize(stmt);
		}

		sqlite3_close(db);
	}

	if (offline_count > 0)
		hb::logger::log("enforce_max_stat_value: clamped {} offline character(s) to max stat {}", offline_count, new_max);
}

void CGame::enforce_base_stat_value()
{
	int base = m_base_stat_value;
	int gain = m_levelup_stat_gain;
	int base_total = m_base_stat_total;
	int online_count = 0;

	// Online characters — reset all stats to base, recalculate levelup pool
	for (int i = 1; i < hb::server::config::MaxClients; i++)
	{
		if (m_client_list[i] == nullptr) continue;
		if (!m_client_list[i]->m_is_init_complete) continue;

		m_client_list[i]->m_str = base;
		m_client_list[i]->m_int = base;
		m_client_list[i]->m_vit = base;
		m_client_list[i]->m_dex = base;
		m_client_list[i]->m_mag = base;
		m_client_list[i]->m_charisma = base;
		m_client_list[i]->m_levelup_pool = (m_client_list[i]->m_level - 1) * gain - (base * 6 - base_total);
		if (m_client_list[i]->m_levelup_pool < 0) m_client_list[i]->m_levelup_pool = 0;

		reprocess_online_player(i);
		g_login->local_save_player_data(i);
		online_count++;
	}

	if (online_count > 0)
		hb::logger::log("enforce_base_stat_value: reset {} online character(s) to base stat {}", online_count, base);

	// Offline characters — iterate all account DBs
	int offline_count = 0;
	std::error_code ec;
	for (const auto& entry : std::filesystem::directory_iterator("accounts", ec))
	{
		if (!entry.is_regular_file() || entry.path().extension() != ".db")
			continue;

		std::string db_path = entry.path().string();
		sqlite3* db = nullptr;
		if (sqlite3_open(db_path.c_str(), &db) != SQLITE_OK)
		{
			if (db) sqlite3_close(db);
			continue;
		}

		const char* sql =
			"UPDATE characters SET "
			"str = ?1, intl = ?1, vit = ?1, dex = ?1, mag = ?1, chr = ?1, "
			"lu_pool = MAX(0, (level - 1) * ?2 - (?1 * 6 - ?3))";

		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK)
		{
			sqlite3_bind_int(stmt, 1, base);
			sqlite3_bind_int(stmt, 2, gain);
			sqlite3_bind_int(stmt, 3, base_total);
			sqlite3_step(stmt);
			offline_count += sqlite3_changes(db);
			sqlite3_finalize(stmt);
		}

		sqlite3_close(db);
	}

	if (offline_count > 0)
		hb::logger::log("enforce_base_stat_value: reset {} offline character(s) to base stat {}", offline_count, base);
}

void CGame::reprocess_online_player(int client_h)
{
	auto* p = m_client_list[client_h];
	if (p == nullptr) return;

	// Recalculate item bonuses with current stats
	m_item_manager->calc_total_item_effect(client_h, -1, false);

	// Unequip items the player no longer meets requirements for
	m_item_manager->validate_equipped_items(client_h);

	// Recompute level-up pool from current stats
	int stats = p->m_str + p->m_dex + p->m_vit + p->m_int + p->m_mag + p->m_charisma;
	p->m_levelup_pool = (p->m_level - 1) * m_levelup_stat_gain - (stats - m_base_stat_total);
	if (p->m_levelup_pool < 0) p->m_levelup_pool = 0;

	// Clamp HP/MP/SP to new maximums
	int max_hp = get_max_hp(client_h);
	int max_mp = get_max_mp(client_h);
	int max_sp = get_max_sp(client_h);
	if (p->m_hp > max_hp) p->m_hp = max_hp;
	if (p->m_mp > max_mp) p->m_mp = max_mp;
	if (p->m_sp > max_sp) p->m_sp = max_sp;

	// Refresh mastery arrays on client (magic + skill) without triggering majestic path
	send_notify_msg(0, client_h, Notify::ForceMasteryRefresh, 0, 0, 0, 0);

	// Force-update client-side stats, level, and pool
	send_notify_msg(0, client_h, Notify::ForceStatRefresh, 0, 0, 0, 0);
	send_notify_msg(0, client_h, Notify::LevelUpPoints, 0, 0, 0, 0);

	// Notify the player
	send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0,
		"Server config updated. Your character has been adjusted.");
}

bool CGame::reload_server_config()
{
	server_config cfg;
	if (!load_server_config("server_config.json", cfg))
	{
		hb::logger::error("Failed to reload server_config.json");
		return false;
	}

	// Check if realm settings were modified — these require a restart
	const auto& cur = m_server_config.realm;
	const auto& fresh = cfg.realm;
	bool realm_changed = false;

	if (cur.name != fresh.name || cur.login_listen_ip != fresh.login_listen_ip ||
		cur.login_listen_port != fresh.login_listen_port || cur.game_listen_ip != fresh.game_listen_ip ||
		cur.game_listen_port != fresh.game_listen_port || cur.game_connection_ip != fresh.game_connection_ip ||
		cur.game_connection_port != fresh.game_connection_port)
	{
		realm_changed = true;
	}

	// Preserve current realm — socket bindings can't change at runtime
	cfg.realm = m_server_config.realm;

	// Capture old limits before applying to detect changes
	int old_max_level = m_max_level;
	int old_max_stat = m_max_stat_value;
	int old_base_stat = m_base_stat_value;

	apply_server_config(cfg);

	// Enforce new limits on existing characters
	// Base stat change requires full reset (level first if needed, then base, then stat cap)
	if (m_base_stat_value != old_base_stat)
		enforce_base_stat_value();
	else if (m_max_level < old_max_level)
		enforce_max_level(m_max_level);
	if (m_max_stat_value < old_max_stat)
		enforce_max_stat_value(m_max_stat_value);

	if (realm_changed)
		hb::logger::warn("Realm settings were modified in server_config.json but will not take effect until server restart");

	send_server_config_update();

	hb::logger::log("server_config.json reloaded successfully");
	return true;
}

void CGame::send_server_config_update()
{
	hb::net::PacketServerConfigUpdate pkt{};
	pkt.header.msg_id = MsgId::ServerConfigUpdate;
	pkt.header.msg_type = MsgType::Confirm;
	pkt.max_stats = static_cast<int16_t>(m_max_stat_value);
	pkt.max_level = static_cast<int32_t>(m_max_level);
	pkt.max_bank_items = static_cast<int16_t>(m_max_bank_items);
	pkt.base_stat_value = static_cast<int16_t>(m_base_stat_value);
	pkt.max_creation_stat_value = static_cast<int16_t>(m_max_creation_stat_value);
	pkt.creation_stat_points = static_cast<int16_t>(m_creation_stat_points);

	int count = 0;
	for (int i = 1; i < MaxClients; i++)
	{
		if (m_client_list[i] == nullptr) continue;
		if (!m_client_list[i]->m_is_init_complete) continue;
		m_client_list[i]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
		count++;
	}

	hb::logger::log("Server config update pushed to {} client(s)", count);
}

bool CGame::reload_formulas()
{
	sqlite3* db = nullptr;
	std::string dbPath;
	bool created = false;
	if (!EnsureGameConfigDatabase(&db, dbPath, &created))
	{
		hb::logger::error("Failed to open gamedata.db for formula reload");
		return false;
	}

	// Load into temporary engine, validate before swapping
	hb::shared::formula_engine temp_engine;
	bool ok = LoadFormulas(db, temp_engine);
	CloseGameConfigDatabase(db);

	if (!ok)
	{
		hb::logger::error("Formula reload failed (load error)");
		return false;
	}

	auto vr = temp_engine.validate();
	if (!vr.success)
	{
		hb::logger::error("Formula reload failed (validation error) — keeping current formulas");
		return false;
	}

	m_formula_engine = std::move(temp_engine);
	compute_balance_hash();

	// Clamp all online players' resources to new maximums so stale values
	// don't trigger the hack detection in check_character_data()
	for (int i = 1; i < hb::server::config::MaxClients; i++)
	{
		if (m_client_list[i] == nullptr) continue;
		if (!m_client_list[i]->m_is_init_complete) continue;

		int max_hp = get_max_hp(i);
		int max_mp = get_max_mp(i);
		int max_sp = get_max_sp(i);

		if (m_client_list[i]->m_hp > max_hp) m_client_list[i]->m_hp = max_hp;
		if (m_client_list[i]->m_mp > max_mp) m_client_list[i]->m_mp = max_mp;
		if (m_client_list[i]->m_sp > max_sp) m_client_list[i]->m_sp = max_sp;
	}

	hb::logger::log("Formulas reloaded from gamedata.db (balance hash updated)");
	return true;
}
