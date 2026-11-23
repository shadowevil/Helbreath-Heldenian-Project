// Game.h: interface for the CGame class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "NativeTypes.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include <cstring>
#include <memory>
#include <fstream>
#include <iostream>
#include <iosfwd>
#include <vector>
#include <sstream>
#include <array>
#include <format>
#include <unordered_map>

#include "GlobalDef.h"
#include "GameGeometry.h"
#include "IRenderer.h"
#include "RendererFactory.h"
#include "ISprite.h"
#include "ISpriteFactory.h"
#include "SpriteCollection.h"
#include "ITextRenderer.h"
#include "IBitmapFont.h"
#include "BitmapFontFactory.h"
#include "IInput.h"
#include "ASIOSocket.h"
#include "IOServicePool.h"
#include "SpriteID.h"
#include "Misc.h"
#include "ChatMsg.h"
#include "Effect.h"
#include "MapData.h"
#include "ActionID.h"
#include "ActionID_Client.h"
#include "NetMessages.h"
#include "ClientMessages.h"
#include "CharInfo.h"
#include "Item/Item.h"
#include "Magic.h"
#include "Skill.h"
#include "DynamicObjectID.h"
#include "BuildItem.h"
#include "DialogBoxManager.h"
#include "EffectManager.h"
#include "CursorTarget.h"
#include "Player.h"
#include "EntityRenderState.h"
#include "Camera.h"
#include "GameModeManager.h"
#include "GameTimer.h"
#include "GameConstants.h"
#include "Application.h"
#include "GameEvents.h"

// Overlay types for popup screens that render over base screens
enum class OverlayType {
	None = 0,
	Connecting,
	WaitingResponse,
	LogResMsg,
	QueryForceLogin,
	QueryDeleteCharacter
};

struct item_draw_ref
{
	hb::shared::sprite::ISprite* sprite;
	int16_t frame;
};

// Temporary storage for character creation screen data
struct char_creation_data
{
	std::string player_name;
	int8_t gender = 0, skin_col = 0, hair_style = 0, hair_col = 0, under_col = 0;
	int8_t stat_str = 10, stat_vit = 10, stat_dex = 10;
	int8_t stat_int = 10, stat_mag = 10, stat_chr = 10;
};

namespace hb { namespace net { struct packet_base; } }

class floating_text_manager;
class Screen_OnGame;

class CGame : public hb::shared::render::application
{
public:
	CGame(hb::shared::types::NativeInstance native_instance, int icon_resource_id);
	~CGame() override;

	// Active screen shortcut — set/cleared by GameModeManager on screen transitions
	IGameScreen* m_active_screen = nullptr;

	template<typename T>
	T* get_active_screen_as() { return GameModeManager::get_active_screen_as<T>(); }

	// Convenience accessor — routes through the active Screen_OnGame
	DialogBoxManager& get_dialog_box_manager();
	// Convenience accessor — routes through the active Screen_OnGame gameplay state
	Screen_OnGame* on_game();


	// Connect to the game server using stored connection info
	void connect_to_game_server();

	// --- application overrides ---
	bool on_initialize() override;
	bool on_start() override;
	void on_uninitialize() override;
	void on_run() override;
	void on_event(const hb::shared::render::event& e) override;
	bool on_native_message(uint32_t message, uintptr_t wparam, intptr_t lparam) override;
	void on_key_event(KeyCode key, bool pressed) override;
	bool on_text_input(hb::shared::types::NativeWindowHandle hwnd,
	                   uint32_t message, uintptr_t wparam, intptr_t lparam) override;
	// CLEROTH - AURAS
	void check_active_aura(short sX, short sY, uint32_t time, short owner_type);
	void check_active_aura2(short sX, short sY, uint32_t time, short owner_type);

	// Camera for viewport management
	CCamera m_Camera;

	void read_settings();
	void write_settings();

	void create_screen_shot();
	void crusade_war_result(int winner_side);
	void crusade_contribution_result(int war_contribution);
	void cannot_construct(int code);
	void set_top_msg(const char* string, unsigned char last_sec);
	void grand_magic_result(const char* map_name, int ares_crusade_points, int elv_crusade_points, int ares_industry_points, int elv_industry_points, int ares_crusade_casualties, int ares_industry_casualties, int elv_crusade_casualties, int elv_industry_casualties);
	void meteor_strike_coming(int code);

	void draw_new_dialog_box(char type, int sX, int sY, int frame, bool is_no_color_key = false, bool is_trans = false);
	void add_map_status_info(const char* data, bool is_last_data);
	void request_map_status(const char* map_name, int mode);
	// draw_dialog_boxs REMOVED — use get_dialog_box_manager().draw_all()
	std::string format_comma_number(uint64_t value);

	void response_panning_handler(char * data);
	void set_ilusion_effect(int owner_h);
	void noticement_handler(char * data);
	CItem* get_item_config(int item_id) const;
	item_draw_ref get_item_draw(int16_t display_id, int atlas_type, bool is_female);
	short find_item_id_by_name(const char* item_name);
	void load_game_msg_text_contents();
	const char* get_npc_config_name_by_id(short npcConfigId) const;
	short resolve_npc_type(short npcConfigId) const;

	void use_shortcut( int num );
	void draw_cursor();
	void on_update();   // Logic update: audio, timers, network, game state
	void on_render();   // render only: clear backbuffer -> draw -> flip

	void npc_talk_handler(char * packet_data);
	void set_camera_shaking_effect(short dist, int mul = 0);
	void clear_skill_using_status();
	bool check_ex_id(const char* name);
	bool check_local_chat_command(const char* pMsg);
	char get_official_map_name(const char* map_name, char* name);
	char get_hardcoded_map_index(const char* map_name, char* name);
	uint32_t get_level_exp(int level);
	void draw_version();
	bool is_item_on_hand();
	void dynamic_object_handler(char * data);
	bool check_item_by_type(hb::shared::item::ItemType type);
	void play_game_sound(char type, int num, int dist, long lPan = 0);  // Forwards to audio_manager
	void load_text_dlg_contents(int type);
	int  load_text_dlg_contents2(int type);
	void request_full_object_data(uint16_t object_id);
	void retrieve_item_handler(char * data);
	void civil_right_admission_handler(char * data);
	void request_teleport_and_wait_data();
	void point_command_handler(int indexX, int indexY, char item_id = -1);
	void add_event_list(const char* txt, char color = 0, bool dup_allow = true);
	// shift_guild_operation_list / put_guild_operation_list REMOVED — use DialogBox_GuildOperation
	void init_player_characteristics(char * data);
	void common_event_handler(char * data);
	// get_top_dialog_box_index, enable_dialog_box, disable_dialog_box REMOVED
	// — use get_dialog_box_manager().get_top_id(), enable_dialog_box(), disable_dialog_box()
	void init_item_list(char * packet_data);
	hb::shared::sprite::BoundRect draw_object_on_move_for_menu(int indexX, int indexY, int sX, int sY, bool trans, uint32_t time, bool draw_shadow = false);
	void chat_msg_handler(char * packet_data);
	void release_unused_sprites();
	void handle_key_up(KeyCode key);
	void change_game_mode(GameMode mode);
	void log_recv_msg_handler(char * data);
	void log_response_handler(char * packet_data);
	void on_log_socket_event();  // MODERNIZED: Polls socket instead of handling window messages
	void on_timer();
	void log_event_handler(char * data);
	void read_map_data(short pivot_x, short pivot_y, const char* packet_data);
	void motion_event_handler(char * packet_data);
	void init_data_response_handler(char * packet_data);
	void init_player_response_handler(char * data);
	void connection_establish_handler(char where);
	void motion_response_handler(char * packet_data);
	void game_recv_msg_handler(uint32_t msg_size, char * data);
	// Convenience accessor — routes through the active Screen_OnGame
	floating_text_manager& get_floating_text();
	// --- Packet transport API (caller constructs packet, CGame owns the wire) ---
	bool send_game_packet_impl(const hb::net::packet_base& pkt, size_t size, bool encrypt = true);

	template<typename PacketT>
	bool send_game_packet(const PacketT& pkt, bool encrypt = true)
	{
		return send_game_packet_impl(pkt, sizeof(PacketT), encrypt);
	}

	bool send_game_packet_raw(const char* data, uint32_t size, bool encrypt = true);
	bool send_chat_message(const char* text);

	void set_pending_login_packet_impl(const hb::net::packet_base& pkt, size_t size);

	template<typename PacketT>
	void set_pending_login_packet(const PacketT& pkt)
	{
		set_pending_login_packet_impl(pkt, sizeof(PacketT));
	}

	bool check_send_result(int result);
	void command_processor(short mouse_x, short mouse_y, short tile_x, short tile_y, char left_button, char right_button);
	bool process_left_click(short mouse_x, short mouse_y, short tile_x, short tile_y, uint32_t current_time, uint16_t& action_type);
	bool process_right_click(short mouse_x, short mouse_y, short tile_x, short tile_y, uint32_t current_time, uint16_t& action_type);
	void process_motion_commands(uint16_t action_type);
	void on_game_socket_event();  // MODERNIZED: Polls socket instead of handling window messages
	void handle_key_down(KeyCode key);
	// Platform specifics (passed from main, used in on_initialize)
	hb::shared::types::NativeInstance m_native_instance;
	int m_icon_resource_id;

	void reserve_fightzone_response_handler(char * data);
	void start_bgm();  // Forwards to audio_manager based on current location

	int has_hero_set(const hb::shared::entity::PlayerAppearance& appr, short OwnerType);
	void show_heldenian_victory(short side);

	//50Cent - Repair All
	short totalItemRepair;
	int totalPrice;
	struct
	{
		char index;
		short price;
	} m_repair_all[hb::shared::limits::MaxItems];

	bool item_drop_history(short item_id);

	GameTimer m_game_timer;




	class hb::shared::render::IRenderer* m_Renderer;  // Abstract renderer interface
	std::unique_ptr<hb::shared::sprite::ISpriteFactory> m_sprite_factory;  // Sprite factory for creating sprites
	hb::shared::sprite::SpriteCollection m_sprite;
	hb::shared::sprite::SpriteCollection m_tile_spr;
	hb::shared::sprite::SpriteCollection m_effect_sprites;
	hb::shared::sprite::SpriteCollection m_item_sprites;	// Atlas: [0]=equip, [1]=ground, [2]=pack
	hb::shared::sprite::SpriteCollection m_equip_sprites;	// Per-item equipment sprites (indexed via equip_sprite::index)

	// Player: owned by Screen_OnGame (static). Raw pointer for gameplay access.
	CPlayer* m_player = nullptr;

	// Session data (login/character selection — exists before player)
	std::string m_account_name;
	std::string m_account_password;
	std::string m_selected_char_name;
	int m_selected_char_level = 0;
	char_creation_data m_new_char;

	std::unique_ptr<CMapData> m_map_data;
	std::unique_ptr<hb::shared::net::IOServicePool> m_io_pool;  // 0 threads = manual poll mode for client
	std::unique_ptr<hb::shared::net::ASIOSocket> m_g_sock;
	std::unique_ptr<hb::shared::net::ASIOSocket> m_l_sock;
	std::unique_ptr<effect_manager> m_effect_manager;
	std::array<std::unique_ptr<CMagic>, hb::shared::limits::MaxMagicType> m_magic_cfg_list;
	std::array<std::unique_ptr<CSkill>, hb::shared::limits::MaxSkillType> m_skill_cfg_list;
	std::unique_ptr<CMsg> m_ex_id;

	std::array<std::unique_ptr<CCharInfo>, 4> m_char_list;
	std::array<std::unique_ptr<CMsg>, game_limits::max_game_msgs> m_game_msg_list;


	std::vector<char> m_pending_login_packet;
	uint32_t m_time;
	uint32_t m_cur_time;
	uint32_t m_check_conn_time, m_check_spr_time, m_check_chat_time;
	uint32_t m_check_connection_time;
	uint32_t m_restart_count_time;
	uint32_t m_observer_cam_time;
	uint32_t m_damaged_time;

	//v2.2
	uint32_t m_monster_event_time;
	short m_monster_id;
	short m_event_x, m_event_y;

	//v2.183 Hunter Mode - Moved to CPlayer

std::array<bool, hb::shared::limits::MaxItems> m_is_item_equipped{};
	std::array<bool, hb::shared::limits::MaxItems> m_is_item_disabled{};
	bool m_is_first_conn;
	bool m_is_server_changing = false;

	bool m_hide_local_cursor;
	bool m_mouse_initialized = false;

	bool m_force_disconn;

	uint32_t m_fps_time;
	int m_latency_ms;
	uint32_t m_last_net_msg_id;
	uint32_t m_last_net_msg_time;
	uint32_t m_last_net_msg_size;
	uint32_t m_last_net_recv_time;
	uint32_t m_last_npc_event_time;


	int m_total_char;
	short m_magic_short_cut;
	int m_accnt_year, m_accnt_month, m_accnt_day;
	int m_ip_year, m_ip_month, m_ip_day;

	short m_recent_short_cut;
	std::array<short, 6> m_short_cut{}; // Snoopy: 6 shortcuts

	int m_time_left_sec_account, m_time_left_sec_ip;
	int m_log_server_port, m_game_server_port;
	int m_block_year, m_block_month, m_block_day;
	int m_net_lag_count;
	std::array<short, hb::shared::item::DEF_MAXITEMEQUIPPOS> m_item_equipment_status{};
	short m_mcx, m_mcy;
	int   m_casting_magic_type;
	short m_vdl_x, m_vdl_y;

	uint16_t m_comm_object_id;
	uint16_t m_last_attack_target_id;
	uint16_t m_enter_game_type;
	char m_item_order[hb::shared::limits::MaxItems];
	static constexpr int AmountStringMaxLen = 20;
	std::string m_amount_string;
	int m_restart_count;

	// Overlay system state
	OverlayType m_active_overlay = OverlayType::None;
	char m_overlay_context;      // Which background screen (replaces m_msg[0] for overlay)
	char m_overlay_message;      // Message code (replaces m_msg[1] for overlay)
	uint32_t m_overlay_start_time;  // When overlay was shown

	char m_msg[200];
	std::string m_location;
	std::string m_cur_location;
	std::string m_mc_name;
	std::string m_map_name;
	std::string m_map_message;
	std::unordered_map<std::string, std::string> m_map_display_names;
	char m_map_index;
	char m_cur_focus, m_max_focus;
	char m_arrow_pressed;
	std::string m_log_server_addr;
	static constexpr int ChatMsgMaxLen = 64;
	std::string m_chat_msg;
	std::string m_backup_chat_msg;

	std::string m_world_server_name;
	direction m_menu_dir;
	char m_menu_dir_cnt, m_menu_frame;
	std::string m_name_ie;
	char m_loading;
	char m_discount;

	std::string m_status_map_name;
	std::string m_construct_map_name;
	std::string m_game_server_name; //  Gateway

	std::array<std::unique_ptr<CItem>, 5000> m_item_config_list;
	bool cache_process_item_config(char* data, uint32_t msg_size);
	bool cache_process_magic_config(char* data, uint32_t msg_size);
	bool cache_process_skill_config(char* data, uint32_t msg_size);
	bool cache_process_npc_config(char* data, uint32_t msg_size);
	bool cache_process_map_config(char* data, uint32_t msg_size);

	struct NpcConfig { short npcType = 0; std::string name; bool valid = false; };
	std::array<NpcConfig, hb::shared::limits::MaxNpcConfigs> m_npc_config_list{};   // indexed by npc_id
	int m_npc_configs_received = 0;
	int m_map_configs_received = 0;

	enum class ConfigRetryLevel : uint8_t { None = 0, CacheTried = 1, ServerRequested = 2, Failed = 3 };
	ConfigRetryLevel m_config_retry[5]{};  // indexed by ConfigCacheType (Items=0, Magic=1, Skills=2, Npcs=3, Maps=4)
	uint32_t m_config_request_time = 0;
	static constexpr uint32_t CONFIG_REQUEST_TIMEOUT_MS = 10000;

	bool m_init_data_ready = false;      // RESPONSE_INITDATA received, waiting for configs
	bool m_configs_ready = false;       // All configs loaded, safe to enter game

	bool ensure_config_loaded(int type);
	bool try_replay_cache_for_config(int type);
	void request_configs_from_server(bool items, bool magic, bool skills, bool npcs = false, bool maps = false);
	void check_configs_ready_and_enter_game();

	bool ensure_item_configs_loaded()  { return ensure_config_loaded(0); }
	bool ensure_magic_configs_loaded() { return ensure_config_loaded(1); }
	bool ensure_skill_configs_loaded() { return ensure_config_loaded(2); }
	bool ensure_npc_configs_loaded()   { return ensure_config_loaded(3); }

	hb::shared::geometry::GameRectangle m_player_rect, m_body_rect;

	short m_item_drop_id[25];
	bool m_item_drop;
	int  m_item_drop_cnt;

	std::string m_gate_map_name;
	bool m_illusion_mvt;

	// Entity render state (temporary state for currently rendered entity)
	CEntityRenderState m_entity_state;

	int   m_contribution_price;

	short m_max_stats;
	int m_max_level;
	int m_max_bank_items;
};

