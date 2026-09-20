#include "Game.h"
#include "GameConstants.h"
#include "ObjectIDRange.h"
#include "CommonTypes.h"
#include "RenderHelpers.h"
#include "GameFonts.h"
#include "TextFieldRenderer.h"
#include "TextLibExt.h"
#include "Benchmark.h"
#include "performance_monitor.h"
#include "lan_eng.h"
#include "Packet/SharedPackets.h"
#include "PacketSendHelpers.h"

#include "SharedCalculations.h"
#include "BalanceConstants.h"
#include "Log.h"
#include "ClientLogChannels.h"
#include "ItemSpriteMetadata.h"
#include "Item/ItemInstanceData.h"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>

#include "platform_headers.h"
#include "embedded_icon.h"

#ifdef _WIN32
#include <windowsx.h>
#endif

// hb::shared::render::Renderer
#include "RendererFactory.h"
#include "SpriteLoader.h"

// Manager singletons
#include "ConfigManager.h"
#include "AudioManager.h"
#include "WeatherManager.h"
#include "ChatManager.h"
#include "ItemNameFormatter.h"
#include "CombatSystem.h"
#include "InventoryManager.h"
#include "MagicCastingSystem.h"
#include "BuildItemManager.h"
#include "ShopManager.h"
#include "TeleportManager.h"
#include "TextInputManager.h"
#include "EventListManager.h"
#include "ChatCommandManager.h"
#include "LocalCacheManager.h"

// DialogBox system
#include "IDialogBox.h"

// Entity render state
#include "EntityRenderState.h"

// Cursor targeting
#include "CursorTarget.h"

// Screen system
#include "IGameScreen.h"
#include "Screen_OnGame.h"
#include "Screen_MainMenu.h"
#include "Screen_Login.h"
#include "Screen_SelectCharacter.h"
#include "Screen_CreateNewCharacter.h"
#include "Screen_CreateAccount.h"
#include "Screen_Quit.h"
#include "Screen_Loading.h"
#include "Screen_Splash.h"
#include "Screen_Test.h"
#include "Screen_TestPrimitives.h"
#include "IInput.h"

// Overlay system
#include "Overlay_Connecting.h"
#include "Overlay_WaitingResponse.h"
#include "Overlay_QueryForceLogin.h"
#include "Overlay_QueryDeleteCharacter.h"
#include "Overlay_LogResMsg.h"
#include "Overlay_ChangePassword.h"
#include "Overlay_VersionNotMatch.h"
#include "Overlay_ConnectionLost.h"
#include "Overlay_Msg.h"
#include "Overlay_WaitInitData.h"

// Dialog boxes
#include "DialogBox_CityHallMenu.h"
#include "DialogBox_NpcTalk.h"
#include "DialogBox_Skill.h"
#include "DialogBox_Bank.h"
#include "DialogBox_Magic.h"
#include "DialogBox_SellList.h"
#include "DialogBox_Party.h"
#include "DialogBox_ItemDropAmount.h"
#include "DialogBox_Slates.h"
#include "DialogBox_NpcActionQuery.h"
#include "DialogBox_Manufacture.h"


using namespace hb::shared::net;
using namespace hb::client::net;
namespace sock = hb::shared::net::socket;
using namespace hb::shared::action;
using namespace hb::shared::direction;

using namespace hb::shared::item;
using namespace hb::client::config;
using namespace hb::client::sprite_id;

namespace NetworkMessageHandlers {
	void HandleServerConfigUpdate(CGame* game, char* data);
}

// Drawing order arrays moved to RenderHelpers.cpp (declared extern in RenderHelpers.h)

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

void CGame::read_settings()
{
	// initialize and load settings from JSON file via config_manager
	config_manager::get().initialize();
	config_manager::get().load();

	// Copy values to CGame member variables
	m_magic_short_cut = config_manager::get().get_magic_shortcut();
	m_recent_short_cut = config_manager::get().get_recent_shortcut();
	for (int i = 0; i < 5; i++)
	{
		m_short_cut[i] = config_manager::get().get_shortcut(i);
	}
	m_short_cut[5] = -1; // 6th slot unused

	// Audio settings loaded into audio_manager
	audio_manager::get().set_master_volume(config_manager::get().get_master_volume());
	audio_manager::get().set_sound_volume(config_manager::get().get_sound_volume());
	audio_manager::get().set_music_volume(config_manager::get().get_music_volume());
	audio_manager::get().set_ambient_volume(config_manager::get().get_ambient_volume());
	audio_manager::get().set_ui_volume(config_manager::get().get_ui_volume());
	audio_manager::get().set_master_enabled(config_manager::get().is_master_enabled());
	audio_manager::get().set_sound_enabled(config_manager::get().is_sound_enabled());
	audio_manager::get().set_music_enabled(config_manager::get().is_music_enabled());
	audio_manager::get().set_ambient_enabled(config_manager::get().is_ambient_enabled());
	audio_manager::get().set_ui_enabled(config_manager::get().is_ui_enabled());
}

void CGame::write_settings()
{
	// Copy CGame member variables to config_manager
	config_manager::get().set_magic_shortcut(m_magic_short_cut);
	for (int i = 0; i < 5; i++)
	{
		config_manager::get().set_shortcut(i, m_short_cut[i]);
	}

	// Audio settings from audio_manager
	config_manager::get().set_master_volume(audio_manager::get().get_master_volume());
	config_manager::get().set_sound_volume(audio_manager::get().get_sound_volume());
	config_manager::get().set_music_volume(audio_manager::get().get_music_volume());
	config_manager::get().set_ambient_volume(audio_manager::get().get_ambient_volume());
	config_manager::get().set_ui_volume(audio_manager::get().get_ui_volume());
	config_manager::get().set_master_enabled(audio_manager::get().is_master_enabled());
	config_manager::get().set_sound_enabled(audio_manager::get().is_sound_enabled());
	config_manager::get().set_music_enabled(audio_manager::get().is_music_enabled());
	config_manager::get().set_ambient_enabled(audio_manager::get().is_ambient_enabled());
	config_manager::get().set_ui_enabled(audio_manager::get().is_ui_enabled());

	// save to JSON file
	config_manager::get().save();
}

CGame::CGame(hb::shared::types::NativeInstance native_instance, int icon_resource_id)
	: m_native_instance(native_instance)
	, m_icon_resource_id(icon_resource_id)
{
	m_io_pool = std::make_unique<hb::shared::net::IOServicePool>(0);  // 0 threads = manual poll mode
	m_Renderer = nullptr;
	read_settings();

	m_max_stats = 0;
	m_max_level = hb::shared::limits::PlayerMaxLevel;
	m_max_bank_items = 200; // Default soft cap, server overrides
	m_loading = 0;
	m_is_first_conn = true;
	m_item_drop_cnt = 0;
	std::fill(std::begin(m_item_drop_id), std::end(m_item_drop_id), short{ 0 });
	m_item_drop = false;

	// Initialize attribute multiplier fallbacks (hardcoded defaults matching original values)
	std::memset(m_prefix_multiplier, 0, sizeof(m_prefix_multiplier));
	std::memset(m_secondary_multiplier, 0, sizeof(m_secondary_multiplier));
	m_prefix_multiplier[1] = 1; m_prefix_multiplier[2] = 5; m_prefix_multiplier[6] = 4;
	m_prefix_multiplier[7] = 1; m_prefix_multiplier[8] = 7; m_prefix_multiplier[9] = 1;
	m_prefix_multiplier[10] = 3; m_prefix_multiplier[11] = 1; m_prefix_multiplier[12] = 1;
	for (int i = 1; i <= 7; i++) m_secondary_multiplier[i] = 7;
	m_secondary_multiplier[8] = 3; m_secondary_multiplier[9] = 3; m_secondary_multiplier[10] = 1;
	m_secondary_multiplier[11] = 10; m_secondary_multiplier[12] = 10;

	combat_system::get().set_game(*this);
	inventory_manager::get().set_game(this);
	magic_casting_system::get().set_game(this);
	build_item_manager::get().set_game(this);
	shop_manager::get().set_game(this);
	teleport_manager::get().set_game(this);
	event_list_manager::get().set_game(this);
	// Camera is initialized via its default constructor (calls reset())
	m_vdl_x = 0;
	m_vdl_y = 0;
	m_comm_object_id = 0;
	m_last_attack_target_id = 0;
	m_enter_game_type = 0;

	// initialize Managers (Networking v4) - using make_unique
	m_effect_manager = std::make_unique<effect_manager>(this);
	// All pointer arrays (std::array<std::unique_ptr<T>, N>) default to nullptr
	// Dialog box order initialization

	// Previous cursor status tracking removed
	CursorTarget::reset_selection_click_time();

	// m_pItemForSaleList defaults to nullptr (std::array<std::unique_ptr<T>, N>)

	// Dialog boxes now self-initialize via set_default_rect() in their constructors
	// Dialogs without classes (GiveItem) are initialized in DialogBoxManager::init_defaults()

	m_time_left_sec_account = 0;
	m_time_left_sec_ip = 0;

	// initialize char arrays that were previously zero-initialized by HEAP_ZERO_MEMORY
	std::memset(m_msg, 0, sizeof(m_msg));
	// m_chat_msg and m_amount_string are std::string (auto-initialized)
}

CGame::~CGame() = default;

// on_initialize: Pre-realize initialization (config, window params, data loading)
// Called by application::initialize() BEFORE the OS window exists.
bool CGame::on_initialize()
{
	// load config (was in Wmain.cpp before)
	config_manager::get().initialize();
	config_manager::get().load();

	hb::logger::initialize("logs");

	hb::shared::render::ResolutionConfig::initialize(
		config_manager::get().get_window_width(),
		config_manager::get().get_window_height()
	);

	// Configure window params via staged setters (no OS window yet)
	auto* window = get_window();
	window->set_title(std::format("Helbreath {}", hb::version::client::display_version).c_str());
	window->set_size(config_manager::get().get_window_width(),
		config_manager::get().get_window_height());
	window->set_borderless(config_manager::get().is_borderless_enabled());
	window->set_mouse_capture_enabled(config_manager::get().is_mouse_capture_enabled());
	window->set_native_instance(m_native_instance);
	window->set_icon_resource_id(m_icon_resource_id);

	// initialize game systems that don't need the OS window
	m_time = GameClock::get_time_ms();

	audio_manager::get().initialize();
	ChatCommandManager::get().initialize(this);
	GameModeManager::initialize(this);
#ifdef _DEBUG
	GameModeManager::set_screen<Screen_Loading>();
#else
	GameModeManager::set_screen<Screen_Splash>();
#endif
	m_hide_local_cursor = false;

	m_log_server_addr = DEF_SERVER_IP;
	m_log_server_port = DEF_SERVER_PORT;
	m_game_server_port = DEF_GSERVER_PORT;

	m_map_data = std::make_unique<CMapData>(this);

	m_menu_dir = direction::southeast;
	m_menu_dir_cnt = 0;
	m_menu_frame = 0;

	load_game_msg_text_contents();
	m_world_server_name = NAME_WORLDNAME1;

	weather_manager::get().initialize();
	ChatManager::get().initialize();
	item_name_formatter::get().set_item_configs(m_item_config_list);
	item_name_formatter::get().set_multipliers(m_prefix_multiplier, m_secondary_multiplier);
	LocalCacheManager::get().initialize();

	return true;
}

// on_start: Post-realize initialization (renderer, resources, display settings)
// Called by application::run() AFTER the OS window is created.
bool CGame::on_start()
{
	// Set window icon from embedded pixel data (cross-platform — on Windows the .rc
	// resource handles the exe icon, but this sets it for the SFML window on Linux/macOS).
	get_window()->set_icon(hb::embedded_icon::width, hb::embedded_icon::height, hb::embedded_icon::pixels);

	FrameTiming::initialize();

	// Create and initialize the renderer
	if (!hb::shared::render::Renderer::Set(hb::shared::render::RendererType::SFML))
	{
		hb::shared::render::Window::show_error("ERROR", "Failed to create renderer!");
		return false;
	}

	m_Renderer = hb::shared::render::Renderer::get();
	if (m_Renderer->init(hb::shared::render::Window::get_handle()) == false)
	{
		hb::shared::render::Window::show_error("ERROR", "Failed to init renderer!");
		return false;
	}

	// Preload the default font for text rendering
	if (hb::shared::text::GetTextRenderer())
	{
		if (!hb::shared::text::GetTextRenderer()->LoadFontFromFile("fonts/default.ttf"))
		{
			// Font load failed — game will use fallback rendering
		}
	}

	// initialize sprite factory and register it globally
	m_sprite_factory.reset(hb::shared::render::CreateSpriteFactory(m_Renderer));
	hb::shared::sprite::Sprites::set_factory(m_sprite_factory.get());

	// push display settings from config_manager to engine
	hb::shared::render::Window::get()->set_vsync_enabled(config_manager::get().is_vsync_enabled());
	hb::shared::render::Window::get()->set_framerate_limit(config_manager::get().get_fps_limit());
	hb::shared::render::Window::get()->set_fullscreen_stretch(config_manager::get().is_fullscreen_stretch_enabled());
	hb::shared::render::Window::get()->set_background_fps_limit(
		config_manager::get().is_background_fps_throttle_enabled() ? 5 : 0);
	if (hb::shared::render::Renderer::get())
		hb::shared::render::Renderer::get()->set_fullscreen_stretch(config_manager::get().is_fullscreen_stretch_enabled());

#ifdef _DEBUG
	FrameTiming::set_profiling_enabled(true);
#endif

	return true;
}

// on_run: Called every main loop iteration by application::run()
// Runs logic update unconditionally, then frame-limited render.
void CGame::on_run()
{
	on_update();

	FrameTiming::begin_frame();
	on_render();
	FrameTiming::end_frame();
}

void CGame::on_uninitialize()
{
	write_settings();
	change_game_mode(GameMode::Null);

	// Shutdown manager singletons — they outlive CGame (static storage),
	// so their destructors run too late for orderly cleanup.
	weather_manager::get().shutdown();
	ChatManager::get().shutdown();
	audio_manager::get().shutdown();
	config_manager::get().shutdown();

	// Release SFML graphics resources before renderer/window destruction.
	// SpriteCollections hold SFML textures that reference the RenderWindow;
	// the factory holds a renderer pointer. Both must go before Destroy().
	m_sprite.clear();
	m_tile_spr.clear();
	m_effect_sprites.clear();
	hb::shared::sprite::Sprites::set_factory(nullptr);
	m_sprite_factory.reset();

	// Destroy renderer BEFORE Window::destroy() runs in application::run().
	// The renderer holds SFML resources that reference the RenderWindow —
	// destroying it after the window is gone causes a crash on close.
	hb::shared::render::Renderer::Destroy();
	m_Renderer = nullptr;

	// All remaining members (unique_ptr arrays, sockets, managers, etc.)
	// are cleaned up by CGame's destructor via RAII.
}

// on_event: Discrete event handler — window lifecycle events routed by application base class
void CGame::on_event(const hb::shared::render::event& e)
{
	switch (e.id)
	{
	case hb::shared::render::event_id::closed:
		// Window close requested (user clicked X or Alt+F4)
		if ((GameModeManager::get_mode() == GameMode::MainGame) && (m_force_disconn == false))
		{
			// In main game, start logout countdown instead of closing immediately
#ifdef _DEBUG
			if (on_game()->m_logout_count == -1 || on_game()->m_logout_count > 2)
			{
				on_game()->m_logout_count = 1;
				on_game()->m_logout_count_time = GameClock::get_time_ms();
			}
#else
			if (on_game()->m_logout_count == -1 || on_game()->m_logout_count > 11)
			{
				on_game()->m_logout_count = 11;
				on_game()->m_logout_count_time = GameClock::get_time_ms();
			}
#endif
		}
		else if (GameModeManager::get_mode() == GameMode::MainMenu)
		{
			change_game_mode(GameMode::Quit);
		}
		else
		{
			request_quit();
		}
		break;

	case hb::shared::render::event_id::focus_gained:
		if (hb::shared::input::get())
			hb::shared::input::get()->set_window_active(true);
		// Note: change_display_mode removed — SFML handles OpenGL context
		// reactivation automatically on FocusGained (see SFMLWindow::process_messages).
		// The old DirectDraw renderer needed surface restoration here, but SFML does not.
		break;

	case hb::shared::render::event_id::focus_lost:
		if (hb::shared::input::get())
			hb::shared::input::get()->set_window_active(false);
		break;

	default:
		break;
	}
}

// on_key_event: Game-specific key handling hook (called by application alongside IInput routing)
// Filters out modifier keys and routes to handle_key_down/handle_key_up
// NOTE: These are named handle_key_down/up (not on_key_down/up) to avoid
// overriding the virtual application::on_key_down/up from IWindowEventHandler,
// which routes key events to the IInput edge-detect system.
void CGame::on_key_event(KeyCode key, bool pressed)
{
	// Skip modifier keys — handled purely through IInput polling
	if (key == KeyCode::Shift || key == KeyCode::Control || key == KeyCode::Alt ||
		key == KeyCode::LShift || key == KeyCode::RShift ||
		key == KeyCode::LControl || key == KeyCode::RControl ||
		key == KeyCode::LAlt || key == KeyCode::RAlt)
		return;

	if (pressed)
	{
		if (key != KeyCode::Enter)
			handle_key_down(key);
	}
	else
	{
		// Enter is handled purely through IInput, not handle_key_up
		if (key != KeyCode::Enter)
			handle_key_up(key);
	}
}

// on_native_message: Platform-specific message handling
bool CGame::on_native_message(uint32_t message, uintptr_t wparam, intptr_t lparam)
{
#ifdef _WIN32
	switch (message)
	{
	case WM_SETCURSOR:
		if (hb::shared::render::Window::get())
			hb::shared::render::Window::get()->set_mouse_cursor_visible(false);
		return true;

	case WM_SETFOCUS:
		if (hb::shared::input::get())
			hb::shared::input::get()->set_window_active(true);
		return true;

	case WM_KILLFOCUS:
		if (hb::shared::input::get())
			hb::shared::input::get()->set_window_active(false);
		return true;

	case WM_LBUTTONDBLCLK:
		if (hb::shared::input::get())
		{
			hb::shared::input::get()->on_mouse_move(GET_X_LPARAM(static_cast<LPARAM>(lparam)), GET_Y_LPARAM(static_cast<LPARAM>(lparam)));
			hb::shared::input::get()->on_mouse_down(hb::shared::input::MouseButton::Left);
		}
		return true;
	}
#endif
	return false;
}

// on_text_input: Text/IME input handling
bool CGame::on_text_input(hb::shared::types::NativeWindowHandle hwnd,
	uint32_t message, uintptr_t wparam, intptr_t lparam)
{
	if (message != 0x0102 /*WM_CHAR*/)
		return false;

	uint32_t codepoint = static_cast<uint32_t>(wparam);

	// Let the active screen handle screen-specific text input behavior
	IGameScreen* screen = GameModeManager::get_active_screen();
	if (screen) screen->on_text_input(codepoint);

	// All chars go through engine input system for CControls consumption
	hb::shared::input::on_text_char(codepoint);
	return true;
}

// UpdateScreen and DrawScreen removed - all modes now handled by Screen/Overlay system via GameModeManager

// draw_cursor: Centralized cursor drawing at end of frame
// Called at the end of DrawScreen() to ensure cursor is always on top
void CGame::draw_cursor()
{
	// Check if cursor should be hidden
	if (m_hide_local_cursor)
		return;

	// get mouse position
	int mouse_x = hb::shared::input::get_mouse_x();
	int mouse_y = hb::shared::input::get_mouse_y();

	// Track mouse initialization — (0,0) is valid in windowed mode
	if (!m_mouse_initialized)
	{
		if (mouse_x == 0 && mouse_y == 0) return;
		m_mouse_initialized = true;
	}

	// Determine cursor type based on game mode from manager (source of truth)
	cursor_type cursor = cursor_type::arrow;

	switch (GameModeManager::get_mode()) {
	case GameMode::MainGame:
		// In-game uses context-sensitive cursor from CursorTarget
		if (on_game()->m_is_observer_mode) {
			// Observer mode shows a small crosshair instead of cursor sprite
			m_Renderer->draw_pixel(mouse_x, mouse_y, hb::shared::render::Color::White());
			m_Renderer->draw_pixel(mouse_x + 1, mouse_y, hb::shared::render::Color::White());
			m_Renderer->draw_pixel(mouse_x - 1, mouse_y, hb::shared::render::Color::White());
			m_Renderer->draw_pixel(mouse_x, mouse_y + 1, hb::shared::render::Color::White());
			m_Renderer->draw_pixel(mouse_x, mouse_y - 1, hb::shared::render::Color::White());
			return;
		}
		cursor = CursorTarget::get_cursor_type();
		break;

	case GameMode::Connecting:
	case GameMode::WaitingResponse:
	case GameMode::WaitingInitData:
		cursor = cursor_type::hourglass;
		break;

	default:
		cursor = cursor_type::arrow;
		break;
	}

	// draw the cursor sprite
	if (m_sprite[MouseCursor])
	{
		int frame = static_cast<int>(cursor);

		// Item targeting cursor: ignore baked pivot offset and center on
		// the mouse point so the grow/shrink animation scales symmetrically.
		if (cursor >= cursor_type::item_target && cursor <= cursor_type::item_target_3)
		{
			auto rect = m_sprite[MouseCursor]->GetFrameRect(frame);
			hb::shared::sprite::DrawParams params;
			params.m_ignore_pivot = true;
			m_sprite[MouseCursor]->draw(
				mouse_x - rect.width / 2,
				mouse_y - rect.height / 2,
				frame, params);
		}
		else
		{
			m_sprite[MouseCursor]->draw(mouse_x, mouse_y, frame);
		}
	}
}



// on_update: Logic update — runs every iteration, decoupled from frame rate
// Handles: audio, timers, network, game state transitions
void CGame::on_update()
{
	audio_manager::get().update();

	// Process timer and network events (must happen before any update logic)
	if (m_game_timer.check_and_reset()) {
		on_timer();
	}
	on_game_socket_event();
	on_log_socket_event();

	// update game mode transition state (fade in/out progress)
	GameModeManager::update();

	// update teleport pre-auth fade state machine
	teleport_manager::get().update();

	// Swap holiday tile variants if month changed
	update_holiday_tiles();

	// update game screens/overlays
	FrameTiming::begin_profile(ProfileStage::update);
	GameModeManager::update_screens();
	FrameTiming::end_profile(ProfileStage::update);

}

void CGame::update_holiday_tiles()
{
	bool is_xmas = weather_manager::get().is_xmas();

	if (is_xmas != m_holiday_tiles_active)
	{
		for (int i = 0; i < 16; i++)
		{
			m_tile_spr.swap_indices(406 + i, hb::client::sprite_id::HolidayTileOffset + 406 + i);
		}
		m_holiday_tiles_active = is_xmas;
	}
}

// on_render: render only — gated by engine frame limiting
// Handles: clear backbuffer -> draw -> fade overlay -> cursor -> flip
void CGame::on_render()
{
	// ============== render Phase ==============
	FrameTiming::begin_profile(ProfileStage::ClearBuffer);
	m_Renderer->begin_frame();
	FrameTiming::end_profile(ProfileStage::ClearBuffer);

	// Engine frame limiter decided to skip this frame — return early
	// Prevents wasted draw calls and keeps behavior identical across all screens
	if (!m_Renderer->was_frame_presented())
		return;

	// Mark frame as rendered so FrameTiming only accumulates profiling for real frames
	FrameTiming::set_frame_rendered(true);

	// render screens/overlays (skipped during Switching phase)
	if (GameModeManager::get_transition_state() != TransitionState::Switching)
	{
		GameModeManager::render();
	}

	// draw fade overlay if transitioning between game modes
	if (GameModeManager::is_transitioning())
	{
		float alpha = GameModeManager::get_fade_alpha();
		m_Renderer->draw_rect_filled(0, 0, m_Renderer->get_width(), m_Renderer->get_height(), hb::shared::render::Color::Black(static_cast<uint8_t>(alpha * 255.0f)));
	}

	// draw teleport fade overlay (fading to/from black during teleport transitions)
	float tp_alpha = teleport_manager::get().get_fade_alpha();
	if (tp_alpha > 0.0f) {
		m_Renderer->draw_rect_filled(0, 0, m_Renderer->get_width(), m_Renderer->get_height(),
			hb::shared::render::Color::Black(static_cast<uint8_t>(tp_alpha * 255.0f)));
	}

	// Performance monitor overlay — top bar with FPS, latency, frame timing
	performance_monitor::get().render(m_Renderer, m_latency_ms);

	// Cursor always on top - drawn LAST after everything including fade overlay
	draw_cursor();

	// Flip to show the drawn content
	FrameTiming::begin_profile(ProfileStage::Flip);
	m_Renderer->end_frame_check_lost_surface();
	FrameTiming::end_profile(ProfileStage::Flip);

	// reset scroll delta now that dialogs have consumed it this frame
	// (scroll accumulates across skip frames until a rendered frame processes it)
	hb::shared::input::reset_mouse_wheel_delta();
}


// MODERNIZED: v4 Networking Architecture (Drain -> Queue -> Process)
void CGame::on_game_socket_event()
{
	if (m_g_sock == 0) return;

	// 1. Check for socket state changes (Connect, Close, Error)
	int ret = m_g_sock->Poll();

	switch (ret) {
	case sock::Event::SocketClosed:
		change_game_mode(GameMode::ConnectionLost);
		m_g_sock.reset();
		return;
	case sock::Event::SocketError:
		printf("[ERROR] Game socket error\n");
		change_game_mode(GameMode::ConnectionLost);
		m_g_sock.reset();
		return;

	case sock::Event::ConnectionEstablish:
		connection_establish_handler(static_cast<int>(ServerType::Game));
		break;
	}

	// 2. Drain all available data from TCP buffer to the Queue
	// Only drain if socket is connected (m_is_available is set on FD_CONNECT)
	if (!m_g_sock->m_is_available) {
		return; // Still connecting, don't try to read yet
	}

	// If Poll() completed a packet, queue it before drain_to_queue() overwrites the buffer
	if (ret == sock::Event::ReadComplete) {
		size_t size = 0;
		char* data = m_g_sock->get_rcv_data_pointer(&size);
		if (data != nullptr && size > 0) {
			m_g_sock->queue_completed_packet(data, size);
		}
	}

	int drained = m_g_sock->drain_to_queue();

	if (drained < 0) {
		printf("[ERROR] Game socket drain_to_queue failed: %d\n", drained);
		change_game_mode(GameMode::ConnectionLost);
		m_g_sock.reset();
		return;
	}

	// 3. Process the queue with a Time Budget
	//    We process as many packets as possible within the budget to keep the game responsive.
	constexpr int MAX_PACKETS_PER_FRAME = 120; // Safety limit
	constexpr uint32_t MAX_TIME_MS = 3;        // 3ms budget for network processing

	uint32_t start_time = GameClock::get_time_ms();
	int processed = 0;

	hb::shared::net::NetworkPacket packet;
	while (processed < MAX_PACKETS_PER_FRAME) {

		// Check budget
		if (GameClock::get_time_ms() - start_time > MAX_TIME_MS) {
			break;
		}

		// Peek next packet
		if (!m_g_sock->peek_packet(packet)) {
			break; // Queue empty
		}

		// update functionality timestamps (legacy requirement)
		m_last_net_recv_time = GameClock::get_time_ms();
		m_time = GameClock::get_time_ms();

		// Process (using the pointer directly from the packet vector)
		if (!packet.empty()) {
			game_recv_msg_handler(static_cast<uint32_t>(packet.size()),
				const_cast<char*>(packet.ptr()));
		}

		// CRITICAL FIX: The handler might have closed/deleted the socket!
		if (m_g_sock == nullptr) return;

		// pop logic (remove from queue)
		m_g_sock->pop_packet();
		processed++;
	}
}


bool CGame::check_send_result(int result)
{
	switch (result) {
	case sock::Event::SocketClosed:
	case sock::Event::SocketError:
	case sock::Event::QueueFull:
		change_game_mode(GameMode::ConnectionLost);
		m_g_sock.reset();
		return false;

	case sock::Event::CriticalError:
		m_g_sock.reset();
		hb::shared::render::Window::close();
		return false;
	}
	return true;
}

bool CGame::send_game_packet_impl(const hb::net::packet_base& pkt, size_t size, bool encrypt)
{
	if (!m_g_sock) return false;
	uint8_t key = encrypt ? (static_cast<uint8_t>(rand() % 255) + 1) : 0;
	int result = m_g_sock->send_msg(const_cast<char*>(reinterpret_cast<const char*>(&pkt)), static_cast<uint32_t>(size), key);
	return check_send_result(result);
}

bool CGame::send_game_packet_raw(const char* data, uint32_t size, bool encrypt)
{
	if (!m_g_sock) return false;
	uint8_t key = encrypt ? (static_cast<uint8_t>(rand() % 255) + 1) : 0;
	int result = m_g_sock->send_msg(const_cast<char*>(data), size, key);
	return check_send_result(result);
}

bool CGame::send_chat_message(const char* text)
{
	if (teleport_manager::get().is_requested()) return false;
	if (!text) return false;
	if (check_local_chat_command(text)) return false;

	hb::net::PacketCommandChatMsgHeader req{};
	req.header.msg_id = MsgId::CommandChatMsg;
	req.header.msg_type = 0;
	req.x = m_player->m_player_x;
	req.y = m_player->m_player_y;
	std::snprintf(req.name, sizeof(req.name), "%s", m_player->m_player_name.c_str());
	req.chat_type = 0;

	std::size_t text_len = std::strlen(text);
	char message[300]{};
	std::memcpy(message, &req, sizeof(req));
	std::memcpy(message + sizeof(req), text, text_len + 1);
	return send_game_packet_raw(message, static_cast<uint32_t>(sizeof(req) + text_len + 1));
}

void CGame::set_pending_login_packet_impl(const hb::net::packet_base& pkt, size_t size)
{
	const char* p = reinterpret_cast<const char*>(&pkt);
	m_pending_login_packet.assign(p, p + size);
}


bool CGame::cache_process_item_config(char* data, uint32_t msg_size)
{
	LocalCacheManager::get().accumulate_packet(ConfigCacheType::Items, data, msg_size);

	// Parse binary item config packet
	constexpr size_t headerSize = sizeof(hb::net::PacketItemConfigHeader);
	constexpr size_t entrySize = sizeof(hb::net::PacketItemConfigEntry);

	if (msg_size < headerSize) {
		return false;
	}

	const auto* pktHeader = reinterpret_cast<const hb::net::PacketItemConfigHeader*>(data);
	uint16_t itemCount = pktHeader->itemCount;
	uint16_t totalItems = pktHeader->totalItems;

	if (msg_size < headerSize + (itemCount * entrySize)) {
		return false;
	}

	const auto* entries = reinterpret_cast<const hb::net::PacketItemConfigEntry*>(data + headerSize);

	for (uint16_t i = 0; i < itemCount; i++) {
		const auto& entry = entries[i];
		int itemId = entry.itemId;

		if (itemId <= 0 || itemId >= 5000) {
			continue;
		}

		// Delete existing item if present (shouldn't happen, but be safe)
		if (m_item_config_list[itemId] != 0) {
			m_item_config_list[itemId].reset();
		}

		m_item_config_list[itemId] = std::make_unique<CItem>();
		CItem* item = m_item_config_list[itemId].get();

		item->m_id_num = entry.itemId;
		std::snprintf(item->m_name, sizeof(item->m_name), "%s", entry.name);
		item->m_item_type = entry.itemType;
		item->m_item_sub_type = entry.itemSubType;
		item->m_equip_pos = entry.equipPos;
		item->m_weapon_class = entry.weaponClass;
		item->m_item_effect_type = entry.effectType;
		item->m_item_effect_value1 = entry.effectValue1;
		item->m_item_effect_value2 = entry.effectValue2;
		item->m_item_effect_value3 = entry.effectValue3;
		item->m_item_effect_value4 = entry.effectValue4;
		item->m_item_effect_value5 = entry.effectValue5;
		item->m_item_effect_value6 = entry.effectValue6;
		item->m_durability = entry.durability;
		item->m_special_effect = entry.specialEffect;
		item->m_sell_price = entry.sellPrice;
		item->m_weight = entry.weight;
		item->m_swing_speed = entry.swingSpeed;
		item->m_level_requirement = entry.levelRequirement;
		item->m_gender_requirement = entry.genderRequirement;
		item->m_special_effect_value1 = entry.specialEffectValue1;
		item->m_special_effect_value2 = entry.specialEffectValue2;
		item->m_related_skill = entry.relatedSkill;
		item->m_hide_armor = entry.hideArmor;
		item->m_is_skirt = entry.isSkirt;
		item->m_stackable = entry.stackable;
		item->m_is_dyeable = entry.isDyeable;
		item->m_armor_class = entry.armorClass;
		item->m_set_id = entry.setId;
		item->m_instance.item_color = entry.itemColor;
		item->m_display_id = entry.displayId;
	}

	// Log total count on last packet
	int totalLoaded = 0;
	for (int j = 0; j < 5000; j++) {
		if (m_item_config_list[j] != 0) totalLoaded++;
	}
	if (totalLoaded >= totalItems && !LocalCacheManager::get().is_replaying()) {
		if (LocalCacheManager::get().finalize_and_save(ConfigCacheType::Items)) {
		}
		else {
		}
	}

	return true;
}

bool CGame::cache_process_magic_config(char* data, uint32_t msg_size)
{
	LocalCacheManager::get().accumulate_packet(ConfigCacheType::Magic, data, msg_size);

	constexpr size_t headerSize = sizeof(hb::net::PacketMagicConfigHeader);
	constexpr size_t entrySize = sizeof(hb::net::PacketMagicConfigEntry);

	if (msg_size < headerSize) {
		return false;
	}

	const auto* pktHeader = reinterpret_cast<const hb::net::PacketMagicConfigHeader*>(data);
	uint16_t magicCount = pktHeader->magicCount;
	uint16_t totalMagics = pktHeader->totalMagics;

	if (msg_size < headerSize + (magicCount * entrySize)) {
		return false;
	}

	const auto* entries = reinterpret_cast<const hb::net::PacketMagicConfigEntry*>(data + headerSize);

	for (uint16_t i = 0; i < magicCount; i++) {
		const auto& entry = entries[i];
		int magicId = entry.magicId;

		if (magicId < 0 || magicId >= hb::shared::limits::MaxMagicType) {
			continue;
		}

		if (m_magic_cfg_list[magicId] != 0) {
			m_magic_cfg_list[magicId].reset();
		}

		m_magic_cfg_list[magicId] = std::make_unique<CMagic>();
		CMagic* magic = m_magic_cfg_list[magicId].get();
		magic->m_name = entry.name;
		magic->m_value_1 = entry.manaCost;
		magic->m_value_2 = entry.intLimit;
		magic->m_value_3 = (entry.goldCost >= 0) ? entry.goldCost : -entry.goldCost;
		magic->m_is_visible = (entry.visible != 0);
		magic->m_type = entry.magicType;
		magic->m_aoe_radius_x = entry.aoeRadiusX;
		magic->m_aoe_radius_y = entry.aoeRadiusY;
		magic->m_dynamic_pattern = entry.dynamicPattern;
		magic->m_dynamic_radius = entry.dynamicRadius;
	}

	// Log total count on last packet
	int totalLoaded = 0;
	for (int j = 0; j < hb::shared::limits::MaxMagicType; j++) {
		if (m_magic_cfg_list[j] != 0) totalLoaded++;
	}
	if (totalLoaded >= totalMagics && !LocalCacheManager::get().is_replaying()) {
		if (LocalCacheManager::get().finalize_and_save(ConfigCacheType::Magic)) {
		}
		else {
		}
	}

	return true;
}

bool CGame::cache_process_skill_config(char* data, uint32_t msg_size)
{
	LocalCacheManager::get().accumulate_packet(ConfigCacheType::Skills, data, msg_size);

	constexpr size_t headerSize = sizeof(hb::net::PacketSkillConfigHeader);
	constexpr size_t entrySize = sizeof(hb::net::PacketSkillConfigEntry);

	if (msg_size < headerSize) {
		return false;
	}

	const auto* pktHeader = reinterpret_cast<const hb::net::PacketSkillConfigHeader*>(data);
	uint16_t skillCount = pktHeader->skillCount;
	uint16_t totalSkills = pktHeader->totalSkills;

	if (msg_size < headerSize + (skillCount * entrySize)) {
		return false;
	}

	const auto* entries = reinterpret_cast<const hb::net::PacketSkillConfigEntry*>(data + headerSize);

	for (uint16_t i = 0; i < skillCount; i++) {
		const auto& entry = entries[i];
		int skillId = entry.skillId;

		if (skillId < 0 || skillId >= hb::shared::limits::MaxSkillType) {
			continue;
		}

		if (m_skill_cfg_list[skillId] != 0) {
			m_skill_cfg_list[skillId].reset();
		}

		m_skill_cfg_list[skillId] = std::make_unique<CSkill>();
		CSkill* skill = m_skill_cfg_list[skillId].get();
		skill->m_name = entry.name;
		skill->m_is_useable = (entry.useable != 0);
		skill->m_use_method = entry.useMethod;
		// Apply mastery level if already received from init_item_list
		skill->m_level = static_cast<int>(m_player->m_skill_mastery[skillId]);
	}

	// Log total count on last packet
	int totalLoaded = 0;
	for (int j = 0; j < hb::shared::limits::MaxSkillType; j++) {
		if (m_skill_cfg_list[j] != 0) totalLoaded++;
	}
	if (totalLoaded >= totalSkills && !LocalCacheManager::get().is_replaying()) {
		if (LocalCacheManager::get().finalize_and_save(ConfigCacheType::Skills)) {
		}
		else {
		}
	}

	return true;
}

bool CGame::cache_process_npc_config(char* data, uint32_t msg_size)
{
	LocalCacheManager::get().accumulate_packet(ConfigCacheType::Npcs, data, msg_size);

	constexpr size_t headerSize = sizeof(hb::net::PacketNpcConfigHeader);
	constexpr size_t entrySize = sizeof(hb::net::PacketNpcConfigEntry);

	if (msg_size < headerSize) {
		return false;
	}

	const auto* pktHeader = reinterpret_cast<const hb::net::PacketNpcConfigHeader*>(data);
	uint16_t npcCount = pktHeader->npcCount;
	uint16_t totalNpcs = pktHeader->totalNpcs;

	if (msg_size < headerSize + (npcCount * entrySize)) {
		return false;
	}

	const auto* entries = reinterpret_cast<const hb::net::PacketNpcConfigEntry*>(data + headerSize);

	for (uint16_t i = 0; i < npcCount; i++) {
		const auto& entry = entries[i];
		int npcId = entry.npcId;
		int npcType = entry.npcType;

		// Store by npc_id (primary key)
		if (npcId >= 0 && npcId < hb::shared::limits::MaxNpcConfigs) {
			m_npc_config_list[npcId].npcType = static_cast<short>(npcType);
			m_npc_config_list[npcId].name = entry.name;
			m_npc_config_list[npcId].valid = true;
		}

		// (type-based reverse map removed — use config_id for all name lookups)
	}

	// Track raw entries received across packets
	if (pktHeader->packetIndex == 0) m_npc_configs_received = 0;
	m_npc_configs_received += npcCount;

	if (m_npc_configs_received >= totalNpcs && !LocalCacheManager::get().is_replaying()) {
		if (LocalCacheManager::get().finalize_and_save(ConfigCacheType::Npcs)) {
		}
		else {
		}
	}

	return true;
}

const char* CGame::get_npc_config_name_by_id(short npcConfigId) const
{
	if (npcConfigId >= 0 && npcConfigId < hb::shared::limits::MaxNpcConfigs && m_npc_config_list[npcConfigId].valid) {
		return m_npc_config_list[npcConfigId].name.c_str();
	}
	return "Unknown";
}

short CGame::resolve_npc_type(short npcConfigId) const
{
	if (npcConfigId >= 0 && npcConfigId < hb::shared::limits::MaxNpcConfigs && m_npc_config_list[npcConfigId].valid)
		return m_npc_config_list[npcConfigId].npcType;
	return 0;
}

bool CGame::cache_process_map_config(char* data, uint32_t msg_size)
{
	LocalCacheManager::get().accumulate_packet(ConfigCacheType::Maps, data, msg_size);

	constexpr size_t headerSize = sizeof(hb::net::PacketMapConfigHeader);
	constexpr size_t entrySize = sizeof(hb::net::PacketMapConfigEntry);

	if (msg_size < headerSize) {
		return false;
	}

	const auto* pktHeader = reinterpret_cast<const hb::net::PacketMapConfigHeader*>(data);
	uint16_t mapCount = pktHeader->mapCount;
	uint16_t totalMaps = pktHeader->totalMaps;

	if (msg_size < headerSize + (mapCount * entrySize)) {
		return false;
	}

	const auto* entries = reinterpret_cast<const hb::net::PacketMapConfigEntry*>(data + headerSize);

	for (uint16_t i = 0; i < mapCount; i++) {
		const auto& entry = entries[i];
		std::string map_name(entry.map_name, strnlen(entry.map_name, sizeof(entry.map_name)));
		std::string display_name(entry.display_name, strnlen(entry.display_name, sizeof(entry.display_name)));
		if (!map_name.empty() && !display_name.empty()) {
			m_map_display_names[map_name] = display_name;
		}
	}

	if (pktHeader->packetIndex == 0) m_map_configs_received = 0;
	m_map_configs_received += mapCount;

	if (m_map_configs_received >= totalMaps && !LocalCacheManager::get().is_replaying()) {
		LocalCacheManager::get().finalize_and_save(ConfigCacheType::Maps);
	}

	return true;
}

bool CGame::cache_process_balance_config(char* data, uint32_t msg_size)
{
	if (msg_size <= sizeof(hb::net::PacketHeader)) return false;

	const char* payload = data + sizeof(hb::net::PacketHeader);
	uint32_t payload_size = msg_size - sizeof(hb::net::PacketHeader);

	// Accumulate for cache (single packet, finalize immediately)
	LocalCacheManager::get().accumulate_packet(ConfigCacheType::BalanceConfig, data, msg_size);
	if (!LocalCacheManager::get().is_replaying())
		LocalCacheManager::get().finalize_and_save(ConfigCacheType::BalanceConfig);

	// Deserialize into formula engine
	m_formula_engine.clear();
	if (!m_formula_engine.deserialize(reinterpret_cast<const uint8_t*>(payload), payload_size))
	{
		hb::logger::warn<hb::log_channel::network>("Failed to deserialize balance config");
		return false;
	}

	hb::logger::log<hb::log_channel::network>("Balance config loaded ({} bytes, {} formulas)",
		payload_size, m_formula_engine.formula_count());

	return true;
}

bool CGame::cache_process_color_palette(char* data, uint32_t msg_size)
{
	LocalCacheManager::get().accumulate_packet(ConfigCacheType::ColorPalette, data, msg_size);

	constexpr size_t headerSize = sizeof(hb::net::PacketColorPaletteConfigHeader);
	constexpr size_t entrySize = sizeof(hb::net::PacketColorPaletteConfigEntry);

	if (msg_size < headerSize) return false;

	const auto* pktHeader = reinterpret_cast<const hb::net::PacketColorPaletteConfigHeader*>(data);
	uint16_t colorCount = pktHeader->colorCount;
	uint16_t totalColors = pktHeader->totalColors;

	if (msg_size < headerSize + colorCount * entrySize) return false;

	const auto* entries = reinterpret_cast<const hb::net::PacketColorPaletteConfigEntry*>(data + headerSize);

	for (uint16_t i = 0; i < colorCount; i++)
	{
		uint8_t id = entries[i].colorId;
		m_color_palette[id] = hb::shared::render::Color(entries[i].r, entries[i].g, entries[i].b);
	}

	// Check if all packets received
	// Use packet index to determine: if this is the last packet, finalize
	uint16_t lastEntryId = (colorCount > 0) ? entries[colorCount - 1].colorId : 0;
	size_t entriesProcessed = pktHeader->packetIndex * ((7000 - headerSize) / entrySize) + colorCount;

	if (entriesProcessed >= totalColors && !LocalCacheManager::get().is_replaying())
	{
		LocalCacheManager::get().finalize_and_save(ConfigCacheType::ColorPalette);
	}

	m_color_palette_loaded = true;
	hb::logger::log<hb::log_channel::network>("Color palette loaded ({} entries in packet, {} total)", colorCount, totalColors);

	return true;
}

bool CGame::cache_process_attribute_types(char* data, uint32_t msg_size)
{
	LocalCacheManager::get().accumulate_packet(ConfigCacheType::AttributeTypes, data, msg_size);

	constexpr size_t headerSize = sizeof(hb::net::PacketAttributeTypeConfigHeader);

	if (msg_size < headerSize) return false;

	const auto* pktHeader = reinterpret_cast<const hb::net::PacketAttributeTypeConfigHeader*>(data);
	uint16_t entryCount = pktHeader->entryCount;
	uint8_t entryType = pktHeader->entryType;

	if (entryType == 0)
	{
		// Prefix entries
		constexpr size_t entrySize = sizeof(hb::net::PacketAttributePrefixTypeEntry);
		if (msg_size < headerSize + entryCount * entrySize) return false;

		const auto* entries = reinterpret_cast<const hb::net::PacketAttributePrefixTypeEntry*>(data + headerSize);
		for (uint16_t i = 0; i < entryCount; i++)
		{
			if (entries[i].prefix_id < 16)
				m_prefix_multiplier[entries[i].prefix_id] = entries[i].multiplier;
		}
	}
	else if (entryType == 1)
	{
		// Secondary entries
		constexpr size_t entrySize = sizeof(hb::net::PacketAttributeSecondaryTypeEntry);
		if (msg_size < headerSize + entryCount * entrySize) return false;

		const auto* entries = reinterpret_cast<const hb::net::PacketAttributeSecondaryTypeEntry*>(data + headerSize);
		for (uint16_t i = 0; i < entryCount; i++)
		{
			if (entries[i].secondary_id < 16)
				m_secondary_multiplier[entries[i].secondary_id] = entries[i].multiplier;
		}
	}

	// Finalize cache after receiving data (both prefix and secondary come as separate packets)
	if (!LocalCacheManager::get().is_replaying())
	{
		// For attribute types we get at most 2 packets (prefix + secondary).
		// Finalize after the secondary packet (entryType==1) or if only one type was sent.
		if (entryType == 1)
			LocalCacheManager::get().finalize_and_save(ConfigCacheType::AttributeTypes);
	}

	m_attribute_types_loaded = true;
	hb::logger::log<hb::log_channel::network>("Attribute types loaded (type={}, {} entries)", entryType, entryCount);

	return true;
}

void CGame::game_recv_msg_handler(uint32_t msg_size, char* data)
{
	const auto* header = hb::net::PacketCast<hb::net::PacketHeader>(data, sizeof(hb::net::PacketHeader));
	if (!header) return;
	m_last_net_msg_id = header->msg_id;
	m_last_net_msg_time = GameClock::get_time_ms();
	m_last_net_msg_size = msg_size;

	IGameScreen* screen = GameModeManager::get_active_screen();
	if (screen && screen->on_game_msg(header->msg_id, header->msg_type, data, msg_size))
		return;

	switch (header->msg_id) {
	case MSGID_RESPONSE_CONFIGCACHESTATUS:
	{
		const auto* cachePkt = hb::net::PacketCast<hb::net::PacketResponseConfigCacheStatus>(
			data, sizeof(hb::net::PacketResponseConfigCacheStatus));
		if (!cachePkt) break;

		struct ReplayCtx { CGame* game; };
		ReplayCtx ctx{ this };
		bool need_items = false, need_magic = false, need_skills = false, need_npcs = false;

		// clear config arrays before replay so verification is accurate
		// (stale entries from previous login would cause false positives)
		for (auto& item : m_item_config_list) item.reset();
		for (auto& magic : m_magic_cfg_list) magic.reset();
		for (auto& skill : m_skill_cfg_list) skill.reset();

		if (cachePkt->itemCacheValid) {
			bool replay_ok = LocalCacheManager::get().replay_from_cache(ConfigCacheType::Items,
				[](char* p, uint32_t s, void* c) -> bool {
					return static_cast<ReplayCtx*>(c)->game->cache_process_item_config(p, s);
				}, &ctx);
			// Verify items actually loaded
			bool has_items = false;
			if (replay_ok) {
				for (int i = 1; i < 5000; i++) { if (m_item_config_list[i]) { has_items = true; break; } }
			}
			if (replay_ok && has_items) {
				m_config_retry[0] = ConfigRetryLevel::None;
			}
			else {
				LocalCacheManager::get().reset_accumulator(ConfigCacheType::Items);
				m_config_retry[0] = ConfigRetryLevel::ServerRequested;
				need_items = true;
			}
		}
		else {
			LocalCacheManager::get().reset_accumulator(ConfigCacheType::Items);
			m_config_retry[0] = ConfigRetryLevel::ServerRequested;
			need_items = true;
		}

		if (cachePkt->magicCacheValid) {
			bool replay_ok = LocalCacheManager::get().replay_from_cache(ConfigCacheType::Magic,
				[](char* p, uint32_t s, void* c) -> bool {
					return static_cast<ReplayCtx*>(c)->game->cache_process_magic_config(p, s);
				}, &ctx);
			bool has_magic = false;
			if (replay_ok) {
				for (int i = 0; i < hb::shared::limits::MaxMagicType; i++) { if (m_magic_cfg_list[i]) { has_magic = true; break; } }
			}
			if (replay_ok && has_magic) {
				m_config_retry[1] = ConfigRetryLevel::None;
			}
			else {
				LocalCacheManager::get().reset_accumulator(ConfigCacheType::Magic);
				m_config_retry[1] = ConfigRetryLevel::ServerRequested;
				need_magic = true;
			}
		}
		else {
			LocalCacheManager::get().reset_accumulator(ConfigCacheType::Magic);
			m_config_retry[1] = ConfigRetryLevel::ServerRequested;
			need_magic = true;
		}

		if (cachePkt->skillCacheValid) {
			bool replay_ok = LocalCacheManager::get().replay_from_cache(ConfigCacheType::Skills,
				[](char* p, uint32_t s, void* c) -> bool {
					return static_cast<ReplayCtx*>(c)->game->cache_process_skill_config(p, s);
				}, &ctx);
			bool has_skills = false;
			if (replay_ok) {
				for (int i = 0; i < hb::shared::limits::MaxSkillType; i++) { if (m_skill_cfg_list[i]) { has_skills = true; break; } }
			}
			if (replay_ok && has_skills) {
				m_config_retry[2] = ConfigRetryLevel::None;
			}
			else {
				LocalCacheManager::get().reset_accumulator(ConfigCacheType::Skills);
				m_config_retry[2] = ConfigRetryLevel::ServerRequested;
				need_skills = true;
			}
		}
		else {
			LocalCacheManager::get().reset_accumulator(ConfigCacheType::Skills);
			m_config_retry[2] = ConfigRetryLevel::ServerRequested;
			need_skills = true;
		}

		if (cachePkt->npcCacheValid) {
			bool replay_ok = LocalCacheManager::get().replay_from_cache(ConfigCacheType::Npcs,
				[](char* p, uint32_t s, void* c) -> bool {
					return static_cast<ReplayCtx*>(c)->game->cache_process_npc_config(p, s);
				}, &ctx);
			bool has_npcs = false;
			if (replay_ok) {
				for (int i = 0; i < hb::shared::limits::MaxNpcConfigs; i++) { if (m_npc_config_list[i].valid) { has_npcs = true; break; } }
			}
			if (replay_ok && has_npcs) {
				m_config_retry[3] = ConfigRetryLevel::None;
			}
			else {
				LocalCacheManager::get().reset_accumulator(ConfigCacheType::Npcs);
				m_config_retry[3] = ConfigRetryLevel::ServerRequested;
				need_npcs = true;
			}
		}
		else {
			LocalCacheManager::get().reset_accumulator(ConfigCacheType::Npcs);
			m_config_retry[3] = ConfigRetryLevel::ServerRequested;
			need_npcs = true;
		}

		bool need_maps = false;
		m_map_display_names.clear();
		if (cachePkt->mapCacheValid) {
			bool replay_ok = LocalCacheManager::get().replay_from_cache(ConfigCacheType::Maps,
				[](char* p, uint32_t s, void* c) -> bool {
					return static_cast<ReplayCtx*>(c)->game->cache_process_map_config(p, s);
				}, &ctx);
			if (replay_ok && !m_map_display_names.empty()) {
				m_config_retry[4] = ConfigRetryLevel::None;
			}
			else {
				LocalCacheManager::get().reset_accumulator(ConfigCacheType::Maps);
				m_config_retry[4] = ConfigRetryLevel::ServerRequested;
				need_maps = true;
			}
		}
		else {
			LocalCacheManager::get().reset_accumulator(ConfigCacheType::Maps);
			m_config_retry[4] = ConfigRetryLevel::ServerRequested;
			need_maps = true;
		}

		bool need_balance = false;
		if (cachePkt->balanceCacheValid) {
			bool replay_ok = LocalCacheManager::get().replay_from_cache(ConfigCacheType::BalanceConfig,
				[](char* p, uint32_t s, void* c) -> bool {
					return static_cast<ReplayCtx*>(c)->game->cache_process_balance_config(p, s);
				}, &ctx);
			if (!replay_ok || m_formula_engine.formula_count() == 0) {
				LocalCacheManager::get().reset_accumulator(ConfigCacheType::BalanceConfig);
				need_balance = true;
			}
		}
		else {
			LocalCacheManager::get().reset_accumulator(ConfigCacheType::BalanceConfig);
			need_balance = true;
		}

		bool need_color_palette = false;
		if (cachePkt->colorPaletteCacheValid) {
			bool replay_ok = LocalCacheManager::get().replay_from_cache(ConfigCacheType::ColorPalette,
				[](char* p, uint32_t s, void* c) -> bool {
					return static_cast<ReplayCtx*>(c)->game->cache_process_color_palette(p, s);
				}, &ctx);
			if (!replay_ok || !m_color_palette_loaded) {
				LocalCacheManager::get().reset_accumulator(ConfigCacheType::ColorPalette);
				need_color_palette = true;
			}
		}
		else {
			LocalCacheManager::get().reset_accumulator(ConfigCacheType::ColorPalette);
			need_color_palette = true;
		}

		bool need_attribute_types = false;
		if (cachePkt->attributeTypeCacheValid) {
			bool replay_ok = LocalCacheManager::get().replay_from_cache(ConfigCacheType::AttributeTypes,
				[](char* p, uint32_t s, void* c) -> bool {
					return static_cast<ReplayCtx*>(c)->game->cache_process_attribute_types(p, s);
				}, &ctx);
			if (!replay_ok || !m_attribute_types_loaded) {
				LocalCacheManager::get().reset_accumulator(ConfigCacheType::AttributeTypes);
				need_attribute_types = true;
			}
		}
		else {
			LocalCacheManager::get().reset_accumulator(ConfigCacheType::AttributeTypes);
			need_attribute_types = true;
		}

		if (need_items || need_magic || need_skills || need_npcs || need_maps || need_balance || need_color_palette || need_attribute_types) {
			request_configs_from_server(need_items, need_magic, need_skills, need_npcs, need_maps, need_balance, need_color_palette, need_attribute_types);
			m_config_request_time = GameClock::get_time_ms();
		}
		else {
			m_configs_ready = true;
			if (m_init_data_ready) {
				GameModeManager::clear_overlay();
			}
		}
	}
	break;

	case MsgId::NotifyConfigReload:
	{
		const auto* reloadPkt = hb::net::PacketCast<hb::net::PacketNotifyConfigReload>(
			data, sizeof(hb::net::PacketNotifyConfigReload));
		if (!reloadPkt) break;

		if (reloadPkt->reloadItems)
			LocalCacheManager::get().reset_accumulator(ConfigCacheType::Items);
		if (reloadPkt->reloadMagic)
			LocalCacheManager::get().reset_accumulator(ConfigCacheType::Magic);
		if (reloadPkt->reloadSkills)
			LocalCacheManager::get().reset_accumulator(ConfigCacheType::Skills);
		if (reloadPkt->reloadNpcs) {
			LocalCacheManager::get().reset_accumulator(ConfigCacheType::Npcs);
			m_npc_config_list.fill({});
			m_npc_configs_received = 0;
		}
		if (reloadPkt->reloadMaps) {
			LocalCacheManager::get().reset_accumulator(ConfigCacheType::Maps);
			m_map_display_names.clear();
		}
		if (reloadPkt->reloadBalance) {
			LocalCacheManager::get().reset_accumulator(ConfigCacheType::BalanceConfig);
			m_formula_engine.clear();
		}
		if (reloadPkt->reloadColorPalette) {
			LocalCacheManager::get().reset_accumulator(ConfigCacheType::ColorPalette);
			m_color_palette_loaded = false;
		}
		if (reloadPkt->reloadAttributeTypes) {
			LocalCacheManager::get().reset_accumulator(ConfigCacheType::AttributeTypes);
			m_attribute_types_loaded = false;
		}

		set_top_msg((char*)"Administration kicked off a config reload, some lag may occur.", 5);
	}
	break;

	case MsgId::ItemConfigContents:
		cache_process_item_config(data, msg_size);
		m_config_retry[0] = ConfigRetryLevel::None;
		check_configs_ready_and_enter_game();
		break;
	case MsgId::MagicConfigContents:
		cache_process_magic_config(data, msg_size);
		m_config_retry[1] = ConfigRetryLevel::None;
		check_configs_ready_and_enter_game();
		break;
	case MsgId::SkillConfigContents:
		cache_process_skill_config(data, msg_size);
		m_config_retry[2] = ConfigRetryLevel::None;
		check_configs_ready_and_enter_game();
		break;
	case MsgId::NpcConfigContents:
		cache_process_npc_config(data, msg_size);
		m_config_retry[3] = ConfigRetryLevel::None;
		check_configs_ready_and_enter_game();
		break;
	case MsgId::MapConfigContents:
		cache_process_map_config(data, msg_size);
		m_config_retry[4] = ConfigRetryLevel::None;
		check_configs_ready_and_enter_game();
		break;
	case MsgId::BalanceConfigContents:
		cache_process_balance_config(data, msg_size);
		break;
	case MsgId::ColorPaletteConfigContents:
		cache_process_color_palette(data, msg_size);
		check_configs_ready_and_enter_game();
		break;
	case MsgId::AttributeTypeConfigContents:
		cache_process_attribute_types(data, msg_size);
		break;
	case MsgId::ResponseInitPlayer:
		init_player_response_handler(data);
		break;

	case MsgId::ResponseInitData:
		init_data_response_handler(data);
		break;

	case MsgId::CommandCheckConnection:
	{
		const auto* pkt = hb::net::PacketCast<hb::net::PacketCommandCheckConnection>(
			data, sizeof(hb::net::PacketCommandCheckConnection));
		if (!pkt) return;
		uint32_t recv_time = m_last_net_recv_time;
		if (recv_time == 0) recv_time = GameClock::get_time_ms();
		if (recv_time >= pkt->time_ms)
		{
			m_latency_ms = static_cast<int>(recv_time - pkt->time_ms);
		}
	}
	break;
	}
}

void CGame::connection_establish_handler(char where)
{
	change_game_mode(GameMode::WaitingResponse);

	switch (where) {
	case static_cast<int>(ServerType::Game):
	{
		hb::net::PacketRequestInitPlayer req{};
		req.header.msg_id = MsgId::RequestInitPlayer;
		req.header.msg_type = 0;
		std::snprintf(req.player, sizeof(req.player), "%s", m_selected_char_name.c_str());
		std::snprintf(req.account, sizeof(req.account), "%s", m_account_name.c_str());
		std::snprintf(req.password, sizeof(req.password), "%s", m_account_password.c_str());
		req.is_observer = static_cast<uint8_t>(on_game()->m_is_observer_mode);
		std::snprintf(req.server, sizeof(req.server), "%s", m_game_server_name.c_str());
		req.padding = 0;
		send_game_packet(req);
		break;
	}

	case static_cast<int>(ServerType::Log):
		if (!m_pending_login_packet.empty()) {
			char key = static_cast<char>(rand() % 255) + 1;
			m_l_sock->send_msg(m_pending_login_packet.data(), static_cast<uint32_t>(m_pending_login_packet.size()), key);
			m_pending_login_packet.clear();
		}
		break;
	}
}

void CGame::init_player_response_handler(char* data)
{
	const auto* header = hb::net::PacketCast<hb::net::PacketHeader>(
		data, sizeof(hb::net::PacketHeader));
	if (!header) return;
	switch (header->msg_type) {
	case MsgType::Confirm:
	{
		hb::net::PacketRequestInitDataEx req{};
		req.header.msg_id = MsgId::RequestInitData;
		std::snprintf(req.player, sizeof(req.player), "%s", m_selected_char_name.c_str());
		std::snprintf(req.account, sizeof(req.account), "%s", m_account_name.c_str());
		std::snprintf(req.password, sizeof(req.password), "%s", m_account_password.c_str());
		req.is_observer = static_cast<uint8_t>(on_game()->m_is_observer_mode);
		std::snprintf(req.server, sizeof(req.server), "%s", m_game_server_name.c_str());
		std::snprintf(req.itemConfigHash, sizeof(req.itemConfigHash), "%s",
			LocalCacheManager::get().get_hash(ConfigCacheType::Items).c_str());
		std::snprintf(req.magicConfigHash, sizeof(req.magicConfigHash), "%s",
			LocalCacheManager::get().get_hash(ConfigCacheType::Magic).c_str());
		std::snprintf(req.skillConfigHash, sizeof(req.skillConfigHash), "%s",
			LocalCacheManager::get().get_hash(ConfigCacheType::Skills).c_str());
		std::snprintf(req.npcConfigHash, sizeof(req.npcConfigHash), "%s",
			LocalCacheManager::get().get_hash(ConfigCacheType::Npcs).c_str());
		std::snprintf(req.mapConfigHash, sizeof(req.mapConfigHash), "%s",
			LocalCacheManager::get().get_hash(ConfigCacheType::Maps).c_str());
		std::snprintf(req.balanceConfigHash, sizeof(req.balanceConfigHash), "%s",
			LocalCacheManager::get().get_hash(ConfigCacheType::BalanceConfig).c_str());
		std::snprintf(req.colorPaletteConfigHash, sizeof(req.colorPaletteConfigHash), "%s",
			LocalCacheManager::get().get_hash(ConfigCacheType::ColorPalette).c_str());
		std::snprintf(req.attributeTypeConfigHash, sizeof(req.attributeTypeConfigHash), "%s",
			LocalCacheManager::get().get_hash(ConfigCacheType::AttributeTypes).c_str());
		send_game_packet(req);
		change_game_mode(GameMode::WaitingInitData);
	}
		break;

	case MsgType::Reject:
		std::snprintf(m_msg, sizeof(m_msg), "%s", "3J");
		change_game_mode(GameMode::LogResMsg);
		break;
	}
}

void CGame::on_timer()
{
	if (GameModeManager::get_mode_value() < 0) return;
	uint32_t time = GameClock::get_time_ms();

	if (GameModeManager::get_mode() != GameMode::Loading) {
		if ((time - m_check_spr_time) > 30000)
		{
			m_check_spr_time = time;
			release_unused_sprites();
		}
		if ((time - m_check_connection_time) > 1000)
		{
			m_check_connection_time = time;
			if ((m_g_sock != 0) && (m_g_sock->m_is_available == true))
			{
				hb::net::PacketCommandCheckConnection pkt{};
				pkt.header.msg_id = MsgId::CommandCheckConnection;
				pkt.time_ms = GameClock::get_time_ms();
				pkt.client_major = static_cast<uint8_t>(hb::version::client::major);
				pkt.client_minor = static_cast<uint8_t>(hb::version::client::minor);
				pkt.client_patch = static_cast<uint8_t>(hb::version::client::patch);
				pkt.client_build = static_cast<uint16_t>(hb::version::client::build_number);
				send_game_packet(pkt);
			}
		}
	}

	if (GameModeManager::get_mode() == GameMode::MainGame)
	{
		if ((time - m_check_conn_time) > 5000)
		{
			m_check_conn_time = time;
			if ((on_game()->m_is_crusade_mode) && (m_player->m_crusade_duty == 0)) get_dialog_box_manager().enable_dialog_box(DialogBoxId::CrusadeJob, 1, 0, 0);
		}

		if ((time - m_check_chat_time) > 2000)
		{
			m_check_chat_time = time;
			get_floating_text().release_expired(time);
			if (m_player->m_Controller.get_command_count() >= 6)
			{
				m_net_lag_count++;
				if (m_net_lag_count >= 7)
				{
					change_game_mode(GameMode::ConnectionLost);
					m_g_sock.reset();
					return;
				}
			}
			else m_net_lag_count = 0;
		}
	}
}

void CGame::common_event_handler(char* data)
{
	uint16_t event_type;
	short sX, sY, v1, v2, v3;
	uint32_t dw_v4;

	const auto* base = hb::net::PacketCast<hb::net::PacketEventCommonBase>(data, sizeof(hb::net::PacketEventCommonBase));
	if (!base) return;
	event_type = base->header.msg_type;
	sX = base->x;
	sY = base->y;
	v1 = base->v1;
	v2 = base->v2;
	v3 = base->v3;

	switch (event_type) {
	case CommonType::ItemDrop:
	case CommonType::SetItem:
	{
		const auto* pkt = hb::net::PacketCast<hb::net::PacketEventGroundItem>(
			data, sizeof(hb::net::PacketEventGroundItem));
		if (!pkt) return;
		auto idata = hb::shared::item::item_instance_data::from_ground_item_packet(*pkt);
		short item_id = pkt->item_id;
		bool is_drop = (event_type == CommonType::ItemDrop);
		if (is_drop && item_id == hb::shared::item::ItemId::Gold)
			m_effect_manager->add_effect(EffectType::GOLD_DROP, sX, sY, 0, 0, 0);
		m_map_data->set_item(sX, sY, item_id, idata, is_drop);
	}
	break;

	case CommonType::Magic:
	{
		const auto* pkt = hb::net::PacketCast<hb::net::PacketEventCommonMagic>(data, sizeof(hb::net::PacketEventCommonMagic));
		if (!pkt) return;
		dw_v4 = pkt->v4;
	}
	m_effect_manager->add_effect(static_cast<EffectType>(v3), sX, sY, v1, v2, 0, dw_v4);
	break;

	}
}


Screen_OnGame* CGame::on_game()
{
	return get_active_screen_as<Screen_OnGame>();
}

// init_player_characteristics MOVED to NetworkMessages_Player.cpp

// enable_dialog_box / disable_dialog_box wrappers REMOVED — callers use get_dialog_box_manager() directly

void CGame::add_event_list(const char* txt, char color, bool dup_allow)
{
	event_list_manager::get().add_event(txt, color, dup_allow);
}

void CGame::request_full_object_data(uint16_t object_id)
{
	int     ret;
	hb::net::PacketHeader header{};
	header.msg_id = MsgId::request_full_object_data;
	header.msg_type = object_id;

	ret = m_g_sock->send_msg(reinterpret_cast<char*>(&header), sizeof(header));

	switch (ret) {
	case sock::Event::SocketClosed:
	case sock::Event::SocketError:
	case sock::Event::QueueFull:
		change_game_mode(GameMode::ConnectionLost);
		m_g_sock.reset();
		break;

	case sock::Event::CriticalError:
		m_g_sock.reset();
		hb::shared::render::Window::close();
		break;
	}
}


void CGame::read_map_data(short pivot_x, short pivot_y, const char* packet_data)
{
	char header_byte = 0;
	direction move_dir = direction{};
	std::string name;
	short total_entries = 0, map_x = 0, map_y = 0, owner_type = 0, dynamic_type = 0;
	short npc_config_id = -1;
	hb::shared::entity::PlayerStatus status;
	hb::shared::entity::PlayerAppearance appearance;
	uint16_t object_id = 0;
	uint16_t dynamic_object_id = 0;

	const char* cursor = packet_data;
	m_vdl_x = pivot_x; // Valid Data Loc-X
	m_vdl_y = pivot_y;

	const auto* mapHeader = hb::net::PacketCast<hb::net::PacketMapDataHeader>(cursor, sizeof(hb::net::PacketMapDataHeader));
	if (!mapHeader) return;
	total_entries = mapHeader->total;
	cursor += sizeof(hb::net::PacketMapDataHeader);
	for (int i = 1; i <= total_entries; i++)
	{
		const auto* entry = hb::net::PacketCast<hb::net::PacketMapDataEntryHeader>(cursor, sizeof(hb::net::PacketMapDataEntryHeader));
		if (!entry) return;
		map_x = entry->x;
		map_y = entry->y;
		header_byte = entry->flags;
		cursor += sizeof(hb::net::PacketMapDataEntryHeader);
		if (header_byte & 0x01) // object ID
		{
			const auto* objBase = hb::net::PacketCast<hb::net::PacketMapDataObjectBase>(cursor, sizeof(hb::net::PacketMapDataObjectBase));
			if (!objBase) return;
			if (hb::shared::object_id::is_player_id(objBase->object_id))
			{
				const auto* obj = hb::net::PacketCast<hb::net::PacketMapDataObjectPlayer>(cursor, sizeof(hb::net::PacketMapDataObjectPlayer));
				if (!obj) return;
				object_id = obj->base.object_id;
				owner_type = obj->type;
				move_dir = static_cast<direction>(obj->dir);
				appearance = obj->appearance;
				status = obj->status;
				name.clear();
				name.assign(obj->name, strnlen(obj->name, sizeof(obj->name)));
				cursor += sizeof(hb::net::PacketMapDataObjectPlayer);
			}
			else // NPC
			{
				const auto* obj = hb::net::PacketCast<hb::net::PacketMapDataObjectNpc>(cursor, sizeof(hb::net::PacketMapDataObjectNpc));
				if (!obj) return;
				object_id = obj->base.object_id;
				npc_config_id = obj->config_id;
				owner_type = resolve_npc_type(npc_config_id);
				move_dir = static_cast<direction>(obj->dir);
				appearance.SetFromNpcAppearance(obj->appearance);
				status.SetFromEntityStatus(obj->status);
				name.clear();
				name.assign(obj->name, strnlen(obj->name, sizeof(obj->name)));
				cursor += sizeof(hb::net::PacketMapDataObjectNpc);
			}
			{
				m_map_data->set_owner(object_id, pivot_x + map_x, pivot_y + map_y, owner_type, move_dir, appearance, status, name, Type::stop, 0, 0, 0, 0, 0, npc_config_id);
			}
		}
		if (header_byte & 0x02) // object ID
		{
			const auto* objBase = hb::net::PacketCast<hb::net::PacketMapDataObjectBase>(cursor, sizeof(hb::net::PacketMapDataObjectBase));
			if (!objBase) return;
			if (hb::shared::object_id::is_player_id(objBase->object_id))
			{
				const auto* obj = hb::net::PacketCast<hb::net::PacketMapDataObjectPlayer>(cursor, sizeof(hb::net::PacketMapDataObjectPlayer));
				if (!obj) return;
				object_id = obj->base.object_id;
				owner_type = obj->type;
				move_dir = static_cast<direction>(obj->dir);
				appearance = obj->appearance;
				status = obj->status;
				name.clear();
				name.assign(obj->name, strnlen(obj->name, sizeof(obj->name)));
				cursor += sizeof(hb::net::PacketMapDataObjectPlayer);
			}
			else // NPC
			{
				const auto* obj = hb::net::PacketCast<hb::net::PacketMapDataObjectNpc>(cursor, sizeof(hb::net::PacketMapDataObjectNpc));
				if (!obj) return;
				object_id = obj->base.object_id;
				npc_config_id = obj->config_id;
				owner_type = resolve_npc_type(npc_config_id);
				move_dir = static_cast<direction>(obj->dir);
				appearance.SetFromNpcAppearance(obj->appearance);
				status.SetFromEntityStatus(obj->status);
				name.clear();
				name.assign(obj->name, strnlen(obj->name, sizeof(obj->name)));
				cursor += sizeof(hb::net::PacketMapDataObjectNpc);
			}
			{ m_map_data->set_dead_owner(object_id, pivot_x + map_x, pivot_y + map_y, owner_type, move_dir, appearance, status, name, npc_config_id); }
		}
		if (header_byte & 0x04)
		{
			const auto* item = hb::net::PacketCast<hb::net::PacketMapDataItem>(cursor, sizeof(hb::net::PacketMapDataItem));
			if (!item) return;
			hb::shared::item::item_instance_data idata;
			short map_item_id = item->item_id;
			idata.count = static_cast<uint64_t>(std::max<std::int16_t>(item->count, 0));
			idata.item_color = static_cast<int8_t>(item->color);
			idata.custom_made = item->custom_made;
			idata.prefix_type = item->prefix_type;
			idata.prefix_value = item->prefix_value;
			idata.secondary_type = item->secondary_type;
			idata.secondary_value = item->secondary_value;
			idata.enchant_bonus = item->enchant_bonus;
			cursor += sizeof(hb::net::PacketMapDataItem);
			m_map_data->set_item(pivot_x + map_x, pivot_y + map_y, map_item_id, idata, false);
		}
		if (header_byte & 0x08) // Dynamic object
		{
			const auto* dyn = hb::net::PacketCast<hb::net::PacketMapDataDynamicObject>(cursor, sizeof(hb::net::PacketMapDataDynamicObject));
			if (!dyn) return;
			dynamic_object_id = dyn->object_id;
			dynamic_type = dyn->type;
			cursor += sizeof(hb::net::PacketMapDataDynamicObject);
			m_map_data->set_dynamic_object(pivot_x + map_x, pivot_y + map_y, dynamic_object_id, dynamic_type, false);
		}
	}
}

void CGame::log_event_handler(char* data)
{
	uint16_t event_type, object_id;
	short sX, sY, type;
	short npcConfigId = -1;
	hb::shared::entity::PlayerStatus status;
	direction dir;
	std::string name;
	hb::shared::entity::PlayerAppearance playerAppearance;

	const auto* base = hb::net::PacketCast<hb::net::PacketEventLogBase>(data, sizeof(hb::net::PacketEventLogBase));
	if (!base) return;
	event_type = base->header.msg_type;
	object_id = base->object_id;
	sX = base->x;
	sY = base->y;
	name.clear();
	if (hb::shared::object_id::is_player_id(object_id))
	{
		const auto* pkt = hb::net::PacketCast<hb::net::PacketEventLogPlayer>(data, sizeof(hb::net::PacketEventLogPlayer));
		if (!pkt) return;
		type = pkt->type;
		dir = static_cast<direction>(pkt->dir);
		name.assign(pkt->name, strnlen(pkt->name, sizeof(pkt->name)));
		playerAppearance = pkt->appearance;
		status = pkt->status;
	}
	else 	// NPC
	{
		const auto* pkt = hb::net::PacketCast<hb::net::PacketEventLogNpc>(data, sizeof(hb::net::PacketEventLogNpc));
		if (!pkt) return;
		npcConfigId = pkt->config_id;
		type = resolve_npc_type(npcConfigId);
		dir = static_cast<direction>(pkt->dir);
		name.assign(pkt->name, strnlen(pkt->name, sizeof(pkt->name)));
		playerAppearance.SetFromNpcAppearance(pkt->appearance);
		status.SetFromEntityStatus(pkt->status);
	}

	switch (event_type) {
	case MsgType::Confirm:
	{ m_map_data->set_owner(object_id, sX, sY, type, dir, playerAppearance, status, name, Type::stop, 0, 0, 0, 0, 0, npcConfigId); }
	switch (type) {
	case hb::shared::owner::LightWarBeetle: // LWB
	case hb::shared::owner::GodsHandKnight: // GHK
	case hb::shared::owner::GodsHandKnightCK: // GHKABS
	case hb::shared::owner::TempleKnight: // TK
	case hb::shared::owner::BattleGolem: // BG
		m_effect_manager->add_effect(EffectType::WHITE_HALO, (sX) * 32, (sY) * 32, 0, 0, 0);
		break;
	}
	break;

	case MsgType::Reject:
	{ m_map_data->set_owner(object_id, -1, -1, type, dir, playerAppearance, status, name, Type::stop, 0, 0, 0, 0, 0, npcConfigId); }
	break;
	}

	get_floating_text().remove_by_object_id(object_id);
}

// MODERNIZED: No longer a window message handler - polls socket directly
// MODERNIZED: v4 Networking Architecture (Drain -> Queue -> Process) for Login Socket
void CGame::on_log_socket_event()
{
	if (m_l_sock == 0) return;

	// 1. Check for socket state changes (Connect, Close, Error)
	int ret = m_l_sock->Poll();

	switch (ret) {
	case sock::Event::SocketClosed:
		change_game_mode(GameMode::ConnectionLost);
		m_l_sock.reset();
		return;
	case sock::Event::SocketError:
		printf("[ERROR] Login socket error\n");
		change_game_mode(GameMode::ConnectionLost);
		m_l_sock.reset();
		return;

	case sock::Event::ConnectionEstablish:
		connection_establish_handler(static_cast<int>(ServerType::Log));
		break;
	}

	// 2. Drain all available data from TCP buffer to the Queue
	// Only drain if socket is connected (m_is_available is set on FD_CONNECT)
	if (!m_l_sock->m_is_available) {
		return; // Still connecting, don't try to read yet
	}

	// If Poll() completed a packet, queue it before drain_to_queue() overwrites the buffer
	if (ret == sock::Event::ReadComplete) {
		size_t size = 0;
		char* data = m_l_sock->get_rcv_data_pointer(&size);
		if (data != nullptr && size > 0) {
			m_l_sock->queue_completed_packet(data, size);
		}
	}

	int drained = m_l_sock->drain_to_queue();

	if (drained < 0) {
		printf("[ERROR] Login socket drain_to_queue failed: %d\n", drained);
		change_game_mode(GameMode::ConnectionLost);
		m_l_sock.reset();
		return;
	}

	// 3. Process the queue with a Time Budget
	constexpr int MAX_PACKETS_PER_FRAME = 120; // Safety limit
	constexpr uint32_t MAX_TIME_MS = 3;        // 3ms budget for network processing

	uint32_t start_time = GameClock::get_time_ms();
	int processed = 0;

	hb::shared::net::NetworkPacket packet;
	while (processed < MAX_PACKETS_PER_FRAME) {

		// Check budget
		if (GameClock::get_time_ms() - start_time > MAX_TIME_MS) {
			break;
		}

		// Peek next packet
		if (!m_l_sock->peek_packet(packet)) {
			break; // Queue empty
		}

		// update timestamps
		m_last_net_recv_time = GameClock::get_time_ms();

		// Process (using the pointer directly from the packet vector)
		if (!packet.empty()) {
			log_recv_msg_handler(const_cast<char*>(packet.ptr()), static_cast<uint32_t>(packet.size()));
		}

		// CRITICAL FIX: The handler might have closed/deleted the socket!
		if (m_l_sock == nullptr) return;

		// pop logic (remove from queue)
		m_l_sock->pop_packet();
		processed++;
	}
}

void CGame::log_response_handler(char* packet_data)
{
	const auto* header = hb::net::PacketCast<hb::net::PacketHeader>(packet_data, sizeof(hb::net::PacketHeader));
	if (!header) return;
	uint16_t response = header->msg_type;

	// Route to the active screen — all log responses are handled by screens
	IGameScreen* screen = GameModeManager::get_active_screen();
	if (screen && screen->on_net_response(response, packet_data)) {
		m_l_sock.reset();
		return;
	}

	hb::logger::warn("Unhandled log response {} — no active screen claimed it", response);
	m_l_sock.reset();
}

void CGame::log_recv_msg_handler(char* data, uint32_t msg_size)
{
	// Intercept balance config — sent before login response, must not close socket
	const auto* header = hb::net::PacketCast<hb::net::PacketHeader>(data, sizeof(hb::net::PacketHeader));
	if (header && header->msg_id == MsgId::BalanceConfigContents)
	{
		cache_process_balance_config(data, msg_size);
		return;
	}

	// Intercept color palette — sent before login response
	if (header && header->msg_id == MsgId::ColorPaletteConfigContents)
	{
		cache_process_color_palette(data, msg_size);
		return;
	}

	// Intercept attribute types — sent before login response
	if (header && header->msg_id == MsgId::AttributeTypeConfigContents)
	{
		cache_process_attribute_types(data, msg_size);
		return;
	}

	// Intercept server config — sent before login response
	if (header && header->msg_id == MsgId::ServerConfigUpdate)
	{
		NetworkMessageHandlers::HandleServerConfigUpdate(this, data);
		return;
	}

	log_response_handler(data);
}

void CGame::change_game_mode(GameMode mode)
{
	// Determine if this mode change should be instant (no fade-out)
	// Instant transitions are used for:
	// - Error states that need immediate feedback
	// - Transitions FROM loading/waiting states that don't need fade-outs
	bool instant = false;

	// Check if TARGET mode should be instant (error states)
	switch (mode)
	{
	case GameMode::ConnectionLost:  // Error - show immediately
	case GameMode::VersionNotMatch: // Error - show immediately
	case GameMode::Msg:             // Error/info - show immediately
	case GameMode::Quit:            // Already fading out visually
		instant = true;
		break;
	default:
		break;
	}

	// Check if CURRENT mode shouldn't have fade-out (loading/waiting states)
	// Use manager's current mode as source of truth
	if (!instant)
	{
		switch (GameModeManager::get_mode())
		{
		case GameMode::Loading:         // Loading screen - no fade needed
		case GameMode::Connecting:      // Waiting screen - no fade needed
		case GameMode::WaitingInitData: // Waiting screen - no fade needed
		case GameMode::WaitingResponse: // Waiting screen - no fade needed
			instant = true;
			break;
		default:
			break;
		}
	}

	// Route to new Screen system or legacy system
	switch (mode) {
		// Full screens
	case GameMode::MainMenu:
		GameModeManager::set_screen<Screen_MainMenu>();
		break;
	case GameMode::Login:
		GameModeManager::set_screen<Screen_Login>();
		break;
	case GameMode::SelectCharacter:
		GameModeManager::set_screen<Screen_SelectCharacter>();
		break;
	case GameMode::CreateNewCharacter:
		GameModeManager::set_screen<Screen_CreateNewCharacter>();
		break;
	case GameMode::CreateNewAccount:
		GameModeManager::set_screen<Screen_CreateAccount>();
		break;
	case GameMode::Quit:
		GameModeManager::set_screen<Screen_Quit>();
		break;
	case GameMode::Loading:
		GameModeManager::set_screen<Screen_Loading>();
		break;

		// Overlays - displayed on top of current base screen
	case GameMode::Connecting:
		GameModeManager::set_overlay<Overlay_Connecting>();
		break;
	case GameMode::WaitingResponse:
		GameModeManager::set_overlay<Overlay_WaitingResponse>();
		break;
	case GameMode::QueryForceLogin:
		GameModeManager::set_overlay<Overlay_QueryForceLogin>();
		break;
	case GameMode::QueryDeleteCharacter:
		GameModeManager::set_overlay<Overlay_QueryDeleteCharacter>();
		break;
	case GameMode::LogResMsg:
		GameModeManager::set_overlay<Overlay_LogResMsg>();
		break;
	case GameMode::ChangePassword:
		GameModeManager::set_overlay<Overlay_ChangePassword>();
		break;
	case GameMode::VersionNotMatch:
		GameModeManager::set_overlay<Overlay_VersionNotMatch>();
		break;
	case GameMode::ConnectionLost:
		GameModeManager::set_overlay<Overlay_ConnectionLost>();
		break;
	case GameMode::Msg:
		GameModeManager::set_overlay<Overlay_Msg>();
		break;
	case GameMode::WaitingInitData:
		GameModeManager::set_overlay<Overlay_WaitInitData>();
		break;

	case GameMode::MainGame:
		GameModeManager::set_screen<Screen_OnGame>();
		break;

	case GameMode::Null:
		// Null mode signals application exit
		GameModeManager::set_current_mode(GameMode::Null);
		request_quit();
		break;

	default:
		// Unhandled modes - log warning (Introduction, Agreement, InputKeyCode are unused)
		break;
	}
}

void CGame::release_unused_sprites()
{
	for (auto& [idx, spr] : m_sprite)
	{
		if (spr->IsLoaded() && !spr->IsInUse())
		{
			if (GameClock::get_time_ms() - spr->GetLastAccessTime() > 30000)
				spr->Unload();
		}
	}
	for (auto& [idx, spr] : m_tile_spr)
	{
		if (spr->IsLoaded() && !spr->IsInUse())
		{
			if (GameClock::get_time_ms() - spr->GetLastAccessTime() > 30000)
				spr->Unload();
		}
	}
	for (auto& [idx, spr] : m_effect_sprites)
	{
		if (spr->IsLoaded() && !spr->IsInUse())
		{
			if (GameClock::get_time_ms() - spr->GetLastAccessTime() > 30000)
				spr->Unload();
		}
	}

	// Stale sound buffer release is now handled by audio_manager::update()
	audio_manager::get().update();
}

void CGame::chat_msg_handler(char* packet_data)
{
	int object_id = 0, location = 0;
	short map_x = 0, map_y = 0;
	std::string text2;

	char msg_type = 0, temp[100]{}, message[204]{}, text1[100]{};
	std::string name;
	uint32_t current_time = m_cur_time;
	bool is_done = false;

	std::string head_msg;

	std::memset(text1, 0, sizeof(text1));
	std::memset(message, 0, sizeof(message));

	const auto* pkt = hb::net::PacketCast<hb::net::PacketCommandChatMsgHeader>(
		packet_data, sizeof(hb::net::PacketCommandChatMsgHeader));
	if (!pkt) return;
	object_id = static_cast<int>(pkt->header.msg_type);
	map_x = pkt->x;
	map_y = pkt->y;
	name.clear();
	name.assign(pkt->name, strnlen(pkt->name, sizeof(pkt->name)));
	msg_type = static_cast<char>(pkt->chat_type);

	if (check_ex_id(name.c_str()) == true) return;

	std::snprintf(temp, sizeof(temp), "%s", packet_data + sizeof(hb::net::PacketCommandChatMsgHeader));

	if ((msg_type == 0) || (msg_type == 2) || (msg_type == 3))
	{
	}
	if (!ChatManager::get().is_whisper_enabled())
	{
		if (msg_type == 20) return;
	}
	if (!ChatManager::get().is_shout_enabled())
	{
		if (msg_type == 2 || msg_type == 3) return;
	}

	std::snprintf(message, sizeof(message), "%s: %s", name.c_str(), temp);
	m_Renderer->begin_text_batch();
	is_done = false;
	short check_byte = 0;
	while (is_done == false)
	{
		int msgLen = static_cast<int>(strlen(message));
		location = m_Renderer->get_text_length(message, 305);

		// get_text_length returns how many chars fit; if all fit, no wrapping needed
		if (location >= msgLen)
		{
			ChatManager::get().add_message(message, msg_type);
			is_done = true;
		}
		else if (location > 0)
		{
			// Count double-byte characters for proper splitting
			for (int i = 0; i < location; i++) if (message[i] < 0) check_byte++;

			if ((check_byte % 2) == 0)
			{
				std::memset(temp, 0, sizeof(temp));
				memcpy(temp, message, location);
				ChatManager::get().add_message(temp, msg_type);
				std::snprintf(temp, sizeof(temp), "%s", message + location);
				std::snprintf(message, sizeof(message), " %s", temp);
			}
			else
			{
				std::memset(temp, 0, sizeof(temp));
				memcpy(temp, message, location + 1);
				ChatManager::get().add_message(temp, msg_type);
				std::snprintf(temp, sizeof(temp), "%s", message + location + 1);
				std::snprintf(message, sizeof(message), " %s", temp);
			}
		}
		else
		{
			// Edge case: even a single char doesn't fit, add anyway to avoid infinite loop
			ChatManager::get().add_message(message, msg_type);
			is_done = true;
		}
	}

	m_Renderer->end_text_batch();

	get_floating_text().remove_by_object_id(object_id);

	const char* cp = packet_data + sizeof(hb::net::PacketCommandChatMsgHeader);
	int chat_slot = get_floating_text().add_chat_text(cp, current_time, object_id, m_map_data.get(), map_x, map_y);
	if (chat_slot != 0 || msg_type == 20 || msg_type == 10) {
		if ((msg_type != 0) && (get_dialog_box_manager().is_enabled(DialogBoxId::ChatHistory) != true)) {
			head_msg = std::format("{}:{}", name, cp);
			if (msg_type == 10) {
				event_list_manager::get().add_event_top(head_msg.c_str(), msg_type);
			}
			else {
				add_event_list(head_msg.c_str(), msg_type);
			}
		}
		return;
	}
}

// Equipment sprite indices for character rendering (menu only)
struct MenuCharEquipment {
	int body, undies, hair, bodyArmor, armArmor, pants, boots, weapon, shield, mantle, helm;
	int weaponColor, shieldColor, armorColor, mantleColor, armColor, pantsColor, bootsColor, helmColor;
	bool skirtDraw;
};

// Calculate equipment indices for human characters (male/female) in menu
static void CalcHumanEquipment(const CEntityRenderState& state, bool female, MenuCharEquipment& eq)
{
	const auto& appr = state.m_appearance;

	// Walking uses pose 3, standing uses pose 2
	bool walking = appr.is_walking;
	int pose = walking ? 3 : 2;

	// Body index (still from m_sprite)
	eq.body = 500 + (state.m_owner_type - 1) * 8 * 15 + (pose * 8);

	// Cosmetics — still from m_sprite with old base IDs
	int UNDIES = female ? UndiesW : UndiesM;
	int HAIR   = female ? HairW  : HairM;
	eq.undies = UNDIES + appr.underwear_type * 15 + pose;
	eq.hair   = HAIR + appr.hair_style * 15 + pose;

	// Equipment — from m_equip_sprites via equip_sprite::index()
	eq.bodyArmor = (!appr.hide_armor && appr.armor_item_id > 0)  ? equip_sprite::index(female, appr.armor_display_id, pose)   : -1;
	eq.armArmor  = (appr.arm_item_id > 0)    ? equip_sprite::index(female, appr.arm_display_id, pose)    : -1;
	eq.pants     = (appr.pants_item_id > 0)   ? equip_sprite::index(female, appr.pants_display_id, pose)  : -1;
	eq.boots     = (appr.boots_item_id > 0)   ? equip_sprite::index(female, appr.boots_display_id, pose)  : -1;
	eq.mantle    = (appr.mantle_item_id > 0)  ? equip_sprite::index(female, appr.mantle_display_id, pose) : -1;
	eq.helm      = (appr.helm_item_id > 0)    ? equip_sprite::index(female, appr.helm_display_id, pose)   : -1;
	eq.weapon    = (appr.weapon_item_id > 0)  ? equip_sprite::index(female, appr.weapon_display_id, pose) : -1;
	eq.shield    = (appr.shield_item_id > 0)  ? equip_sprite::index(female, appr.shield_display_id, pose) : -1;

	// Female skirt check — from is_skirt flag computed at broadcast time
	eq.skirtDraw = female && appr.is_skirt;
}

hb::shared::sprite::BoundRect CGame::draw_object_on_move_for_menu(int indexX, int indexY, int sX, int sY, bool trans, uint32_t time, bool draw_shadow)
{
	if (m_entity_state.m_dir < 1 || m_entity_state.m_dir > 8) return {};

	// Extract equipment colors from packed appearance color
	MenuCharEquipment eq = {};
	eq.weaponColor = m_entity_state.m_appearance.weapon_color;
	eq.shieldColor = m_entity_state.m_appearance.shield_color;
	eq.armorColor  = m_entity_state.m_appearance.armor_color;
	eq.mantleColor = m_entity_state.m_appearance.mantle_color;
	eq.armColor    = m_entity_state.m_appearance.arm_color;
	eq.pantsColor  = m_entity_state.m_appearance.pants_color;
	eq.bootsColor  = m_entity_state.m_appearance.boots_color;
	eq.helmColor   = m_entity_state.m_appearance.helm_color;

	// Calculate equipment indices based on character type
	bool mob = false;
	switch (m_entity_state.m_owner_type) {
	case 1: case 2: case 3:  // Male
		CalcHumanEquipment(m_entity_state, false, eq);
		break;
	case 4: case 5: case 6:  // Female
		CalcHumanEquipment(m_entity_state, true, eq);
		break;
	default:  // Mob/NPC
		if (m_entity_state.m_owner_type < 10) return {};
		eq.body = Mob + (m_entity_state.m_owner_type - 10) * 8 * 7 + (1 * 8);
		eq.undies = eq.hair = eq.bodyArmor = eq.armArmor = -1;
		eq.boots = eq.pants = eq.weapon = eq.shield = eq.helm = eq.mantle = -1;
		mob = true;
		break;
	}
	// Helper lambdas for drawing with optional color tint
	int dirFrame = (m_entity_state.m_dir - 1) * 8 + m_entity_state.m_frame;
	int hairColor = m_entity_state.m_appearance.hair_color;

	// Equipment draws from m_equip_sprites, cosmetics from m_sprite
	auto drawCosmeticLayer = [&](int idx, int color) {
		if (idx == -1) return;
		if (color == 0)
			m_sprite[idx]->draw(sX, sY, dirFrame);
		else
			m_sprite[idx]->draw(sX, sY, dirFrame, hb::shared::sprite::DrawParams::tint(m_color_palette[color].r, m_color_palette[color].g, m_color_palette[color].b));
	};

	auto drawEquipLayer = [&](int idx, int color) {
		if (idx == -1) return;
		if (color == 0)
			m_equip_sprites[idx]->draw(sX, sY, dirFrame);
		else
			m_equip_sprites[idx]->draw(sX, sY, dirFrame, hb::shared::sprite::DrawParams::tint(m_color_palette[color].r, m_color_palette[color].g, m_color_palette[color].b));
	};

	auto drawWeapon = [&]() {
		if (eq.weapon == -1) return;
		if (eq.weaponColor == 0)
			m_equip_sprites[eq.weapon]->draw(sX, sY, dirFrame);
		else
			m_equip_sprites[eq.weapon]->draw(sX, sY, dirFrame, hb::shared::sprite::DrawParams::tint(m_color_palette[eq.weaponColor].r, m_color_palette[eq.weaponColor].g, m_color_palette[eq.weaponColor].b));
	};

	auto drawMantle = [&](int order) {
		if (eq.mantle != -1 && mantle_draw_order[m_entity_state.m_dir] == order)
			drawEquipLayer(eq.mantle, eq.mantleColor);
	};

	// Check if mob type should skip shadow
	auto shouldSkipShadow = [&]() {
		switch (m_entity_state.m_owner_type) {
		case hb::shared::owner::Slime: case hb::shared::owner::EnergySphere: case hb::shared::owner::TigerWorm: case hb::shared::owner::Catapult: case hb::shared::owner::CannibalPlant: case hb::shared::owner::IceGolem: case hb::shared::owner::Abaddon: case hb::shared::owner::Gate:
			return true;
		default:
			return false;
		}
	};

	// draw body shadow
	if (draw_shadow && !shouldSkipShadow() && config_manager::get().get_detail_level() != 0 && !mob)
		m_sprite[eq.body + (m_entity_state.m_dir - 1)]->draw(sX, sY, m_entity_state.m_frame, hb::shared::sprite::DrawParams::shadow());

	// draw weapon first if drawing order is 1
	if (weapon_draw_order[m_entity_state.m_dir] == 1)
		drawWeapon();

	// draw body
	if (mob)
		m_sprite[eq.body + (m_entity_state.m_dir - 1)]->draw(sX, sY, m_entity_state.m_frame, hb::shared::sprite::DrawParams::alpha_blend(0.5f));
	else
		m_sprite[eq.body + (m_entity_state.m_dir - 1)]->draw(sX, sY, m_entity_state.m_frame);

	// draw equipment layers (back-to-front order)
	drawMantle(0);  // Mantle behind body
	drawCosmeticLayer(eq.undies, 0);  // Undies from m_sprite

	// Hair (cosmetic, from m_sprite, only if no helm)
	if (eq.hair != -1 && eq.helm == -1)
	{
		const auto& hc = m_color_palette[hairColor];
		m_sprite[eq.hair]->draw(sX, sY, dirFrame, hb::shared::sprite::DrawParams::tint(hc.r, hc.g, hc.b));
	}

	// Boots before pants if wearing skirt
	if (eq.skirtDraw)
		drawEquipLayer(eq.boots, eq.bootsColor);

	drawEquipLayer(eq.pants, eq.pantsColor);
	drawEquipLayer(eq.armArmor, eq.armColor);

	// Boots after pants if not wearing skirt
	if (!eq.skirtDraw)
		drawEquipLayer(eq.boots, eq.bootsColor);

	drawEquipLayer(eq.bodyArmor, eq.armorColor);
	drawEquipLayer(eq.helm, eq.helmColor);
	drawMantle(2);  // Mantle over armor
	drawEquipLayer(eq.shield, eq.shieldColor);
	drawMantle(1);  // Mantle in front

	// draw weapon last if drawing order is not 1
	if (weapon_draw_order[m_entity_state.m_dir] != 1)
		drawWeapon();

	// Chat message
	if (m_entity_state.m_chat_index != 0)
	{
		if (get_floating_text().is_occupied(m_entity_state.m_chat_index))
			get_floating_text().draw_single(m_entity_state.m_chat_index, sX, sY, m_cur_time, m_Renderer);
		else
			m_map_data->clear_chat_msg(indexX, indexY);
	}

	m_entity_state.m_move_offset_x = 0;
	m_entity_state.m_move_offset_y = 0;
	return m_sprite[eq.body + (m_entity_state.m_dir - 1)]->GetBoundRect();
}

// draw_background moved to Screen_OnGame.DrawObjects.cpp

void CGame::init_item_list(char* packet_data)
{
	int angel_value = 0;
	uint16_t total_items = 0;

	for (int i = 0; i < hb::shared::limits::MaxItems; i++)
		m_item_order[i] = -1;

	for (int i = 0; i < DEF_MAXITEMEQUIPPOS; i++)
		m_item_equipment_status[i] = -1;

	inventory_manager::get().unlock_all();

	const auto* header = hb::net::PacketCast<hb::net::PacketResponseItemListHeader>(
		packet_data, sizeof(hb::net::PacketResponseItemListHeader));
	if (!header) return;
	total_items = header->item_count;

	for (int i = 0; i < hb::shared::limits::MaxItems; i++)
		if (m_player->m_item_list[i] != 0)
		{
			m_player->m_item_list[i].reset();
		}

	for (int i = 0; i < hb::shared::limits::MaxBankItems; i++)
		if (m_player->m_bank_list[i] != 0)
		{
			m_player->m_bank_list[i].reset();
		}

	const auto* itemEntries = reinterpret_cast<const hb::net::PacketResponseItemListEntry*>(header + 1);
	for (int i = 0; i < total_items; i++)
	{
		const auto& entry = itemEntries[i];
		m_player->m_item_list[i] = std::make_unique<CItem>();
		m_player->m_item_list[i]->m_id_num = entry.item_id;
		m_player->m_item_list[i]->m_instance.count = entry.count;
		m_player->m_item_list[i]->m_x = 40;
		m_player->m_item_list[i]->m_y = 30;
		if (entry.is_equipped == 0) m_is_item_equipped[i] = false;
		else m_is_item_equipped[i] = true;
		CItem* cfg = get_item_config(entry.item_id);
		if (m_is_item_equipped[i] == true && cfg)
		{
			m_item_equipment_status[cfg->m_equip_pos] = i;
		}
		m_player->m_item_list[i]->m_instance.cur_durability = entry.cur_durability;
		m_player->m_item_list[i]->m_instance.item_color = entry.item_color;
		m_player->m_item_list[i]->m_instance.special_effect_value2 = static_cast<short>(entry.spec_value2); // v1.41
		m_player->m_item_list[i]->load_attributes_from(entry);
		m_item_order[i] = i;
		// Snoopy: Add Angelic Stats
		if (cfg && (cfg->get_item_type() == hb::shared::item::item_type::equipment)
			&& (m_is_item_equipped[i] == true)
			&& (cfg->get_equip_pos() >= EquipPos::LeftFinger))
		{
			angel_value = m_player->m_item_list[i]->m_instance.enchant_bonus;
			if (m_player->m_item_list[i]->m_id_num == hb::shared::item::ItemId::AngelicPendantSTR)
				m_player->m_angelic_str = 1 + angel_value;
			else if (m_player->m_item_list[i]->m_id_num == hb::shared::item::ItemId::AngelicPendantDEX)
				m_player->m_angelic_dex = 1 + angel_value;
			else if (m_player->m_item_list[i]->m_id_num == hb::shared::item::ItemId::AngelicPendantINT)
				m_player->m_angelic_int = 1 + angel_value;
			else if (m_player->m_item_list[i]->m_id_num == hb::shared::item::ItemId::AngelicPendantMAG)
				m_player->m_angelic_mag = 1 + angel_value;
		}
	}

	const auto* bank_header = reinterpret_cast<const hb::net::PacketResponseBankItemListHeader*>(itemEntries + total_items);
	total_items = bank_header->bank_item_count;

	for (int i = 0; i < hb::shared::limits::MaxBankItems; i++)
		if (m_player->m_bank_list[i] != 0)
		{
			m_player->m_bank_list[i].reset();
		}

	const auto* bankEntries = reinterpret_cast<const hb::net::PacketResponseBankItemEntry*>(bank_header + 1);
	for (int i = 0; i < total_items; i++)
	{
		const auto& entry = bankEntries[i];
		m_player->m_bank_list[i] = std::make_unique<CItem>();
		m_player->m_bank_list[i]->m_id_num = entry.item_id;
		m_player->m_bank_list[i]->m_instance.count = entry.count;
		m_player->m_bank_list[i]->m_x = 40;
		m_player->m_bank_list[i]->m_y = 30;
		m_player->m_bank_list[i]->m_instance.cur_durability = entry.cur_durability;
		m_player->m_bank_list[i]->m_instance.item_color = entry.item_color;
		m_player->m_bank_list[i]->m_instance.special_effect_value2 = static_cast<short>(entry.spec_value2); // v1.41
		m_player->m_bank_list[i]->load_attributes_from(entry);
	}

	const auto* mastery = reinterpret_cast<const hb::net::PacketResponseMasteryData*>(bankEntries + total_items);

	for (int i = 0; i < hb::shared::limits::MaxMagicType; i++)
		m_player->m_magic_mastery[i] = mastery->magic_mastery[i];

	for (int i = 0; i < hb::shared::limits::MaxSkillType; i++)
	{
		m_player->m_skill_mastery[i] = static_cast<unsigned char>(mastery->skill_mastery[i]);
		if (m_skill_cfg_list[i] != 0)
			m_skill_cfg_list[i]->m_level = static_cast<int>(mastery->skill_mastery[i]);
	}

	// Diagnostic: count what was loaded
	int nItems = 0, nBank = 0, nMagic = 0, nSkills = 0;
	for (int i = 0; i < hb::shared::limits::MaxItems; i++) if (m_player->m_item_list[i]) nItems++;
	for (int i = 0; i < hb::shared::limits::MaxBankItems; i++) if (m_player->m_bank_list[i]) nBank++;
	for (int i = 0; i < hb::shared::limits::MaxMagicType; i++) if (m_player->m_magic_mastery[i] != 0) nMagic++;
	for (int i = 0; i < hb::shared::limits::MaxSkillType; i++) if (m_player->m_skill_mastery[i] != 0) nSkills++;
}

// draw_dialog_boxs REMOVED — rendering moved to DialogBoxManager::draw_all()
// Super attack overlay moved to DialogBox_HudPanel::draw_super_attack_overlay()

// draw_character_body REMOVED — moved to Screen_OnGame::draw_character_body (static)

// get_top_dialog_box_index REMOVED — use get_dialog_box_manager().get_top_id()


void CGame::load_text_dlg_contents(int type)
{
	for (int i = 0; i < game_limits::max_text_dlg_lines; i++)
	{
		if (on_game()->m_msg_text_list[i] != 0)
			on_game()->m_msg_text_list[i].reset();
	}

	std::string fileName = std::format("contents/contents{}.txt", type);

	std::ifstream file(fileName);
	if (!file) return;

	std::string line;
	int index = 0;
	while (std::getline(file, line) && index < game_limits::max_text_dlg_lines)
	{
		if (!line.empty())
		{
			on_game()->m_msg_text_list[index] = std::make_unique<CMsg>(0, line.c_str(), 0);
			index++;
		}
	}
}

int CGame::load_text_dlg_contents2(int type)
{
	for (int i = 0; i < game_limits::max_text_dlg_lines; i++)
	{
		if (on_game()->m_msg_text_list2[i] != 0)
			on_game()->m_msg_text_list2[i].reset();
	}

	std::string fileName = std::format("contents/contents{}.txt", type);

	std::ifstream file(fileName);
	if (!file) return -1;

	std::string line;
	int index = 0;
	while (std::getline(file, line) && index < game_limits::max_text_dlg_lines)
	{
		if (!line.empty())
		{
			on_game()->m_msg_text_list2[index] = std::make_unique<CMsg>(0, line.c_str(), 0);
			index++;
		}
	}
	return index;
}

void CGame::load_game_msg_text_contents()
{
	for (int i = 0; i < game_limits::max_game_msgs; i++)
	{
		if (m_game_msg_list[i] != 0)
			m_game_msg_list[i].reset();
	}

	std::ifstream file("contents/gamemsglist.txt");
	if (!file) return;

	std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

	int index = 0;
	size_t start = 0;
	size_t end = 0;
	while ((end = content.find_first_of(";\n", start)) != std::string::npos && index < game_limits::max_game_msgs)
	{
		if (end > start)
		{
			std::string token = content.substr(start, end - start);
			m_game_msg_list[index] = std::make_unique<CMsg>(0, token.c_str(), 0);
			index++;
		}
		start = end + 1;
	}
	if (start < content.size() && index < game_limits::max_game_msgs)
	{
		std::string token = content.substr(start);
		if (!token.empty())
		{
			m_game_msg_list[index] = std::make_unique<CMsg>(0, token.c_str(), 0);
		}
	}
}

void CGame::request_map_status(const char* map_name, int mode)
{
	{
		auto pkt = hb::net::make_common_command_str(CommonType::RequestMapStatus, m_player->m_player_x, m_player->m_player_y);
		pkt.v1 = mode;
		std::snprintf(pkt.text, sizeof(pkt.text), "%s", map_name);
		send_game_packet(pkt);
	}
}

void CGame::add_map_status_info(const char* data, bool is_last_data)
{
	char total;
	short index;
	int i;


	const auto* header = hb::net::PacketCast<hb::net::PacketNotifyMapStatusHeader>(
		data, sizeof(hb::net::PacketNotifyMapStatusHeader));
	if (!header) return;
	m_status_map_name.assign(header->map_name, strnlen(header->map_name, sizeof(header->map_name)));
	index = header->index;
	total = header->total;

	const auto* entries = reinterpret_cast<const hb::net::PacketNotifyMapStatusEntry*>(header + 1);

	for (i = 1; i <= total; i++) {
		on_game()->m_crusade_structure_info[index].type = entries->type;
		on_game()->m_crusade_structure_info[index].x = entries->x;
		on_game()->m_crusade_structure_info[index].y = entries->y;
		on_game()->m_crusade_structure_info[index].side = entries->side;
		entries++;

		index++;
	}

	if (is_last_data == true) {
		while (index < hb::shared::limits::MaxCrusadeStructures) {
			on_game()->m_crusade_structure_info[index].type = 0;
			on_game()->m_crusade_structure_info[index].x = 0;
			on_game()->m_crusade_structure_info[index].y = 0;
			on_game()->m_crusade_structure_info[index].side = 0;
			index++;
		}
	}
}

void CGame::draw_new_dialog_box(char type, int sX, int sY, int frame, bool is_no_color_key, bool is_trans)
{
	if (m_sprite[type] == 0) return;
	if (is_no_color_key == false)
	{
		if (is_trans == true)
			m_sprite[type]->draw(sX, sY, frame, hb::shared::sprite::DrawParams::alpha_blend(0.25f));
		else m_sprite[type]->draw(sX, sY, frame);
	}
	else m_sprite[type]->draw(sX, sY, frame, hb::shared::sprite::DrawParams::no_color_key());
}

void CGame::set_camera_shaking_effect(short dist, int mul)
{
	if (config_manager::get().is_reduced_motion_enabled()) return;

	int degree = 5 - dist;
	if (degree <= 0) degree = 0;
	degree *= 2;

	if (mul != 0) degree *= mul;

	if (degree <= 2) return;

	m_Camera.set_shake(degree);
}

void CGame::meteor_strike_coming(int code)
{
	switch (code) {
	case 1: //
		set_top_msg(m_game_msg_list[0]->m_pMsg, 5);
		break;
	case 2: //
		set_top_msg(m_game_msg_list[10]->m_pMsg, 10);
		break;
	case 3: //
		set_top_msg(m_game_msg_list[91]->m_pMsg, 5);
		break;
	case 4: //
		set_top_msg(m_game_msg_list[11]->m_pMsg, 10);
		break;
	}
}

// draw_object_foe REMOVED — moved to Screen_OnGame

void CGame::set_top_msg(const char* string, unsigned char last_sec)
{
	on_game()->m_top_msg = string;

	on_game()->m_top_msg_last_sec = last_sec;
	on_game()->m_top_msg_time = GameClock::get_time_ms();
}

// draw_top_msg moved to Screen_OnGame.DrawObjects.cpp

void CGame::cannot_construct(int code)
{
	std::string G_cTxt;
	switch (code) {
	case 1: //
		set_top_msg(m_game_msg_list[18]->m_pMsg, 5);
		break;

	case 2: //
		G_cTxt = std::format("{} XY({}, {})", m_game_msg_list[19]->m_pMsg, m_player->m_construct_loc_x, m_player->m_construct_loc_y);
		set_top_msg(G_cTxt.c_str(), 5);
		break;

	case 3: //
		set_top_msg(m_game_msg_list[20]->m_pMsg, 5);
		break;
	case 4: //
		set_top_msg(m_game_msg_list[20]->m_pMsg, 5);
		break;

	}
}

DialogBoxManager& CGame::get_dialog_box_manager()
{
	auto* screen = get_active_screen_as<Screen_OnGame>();
	return screen->get_dialog_box_manager();
}

floating_text_manager& CGame::get_floating_text()
{
	auto* screen = get_active_screen_as<Screen_OnGame>();
	return screen->get_floating_text();
}

void CGame::connect_to_game_server()
{
	m_g_sock = std::make_unique<hb::shared::net::ASIOSocket>(m_io_pool->get_context(), game_limits::socket_block_limit);
	m_g_sock->connect(m_log_server_addr.c_str(), m_game_server_port);
	m_g_sock->init_buffer_size(hb::shared::limits::MsgBufferSize);
}

std::string CGame::format_comma_number(uint64_t value)
{
	auto numStr = std::format("{}", value);
#ifdef DEF_COMMA_GOLD
	std::string result;
	int len = static_cast<int>(numStr.length());
	for (int i = 0; i < len; i++)
	{
		if (i > 0 && (len - i) % 3 == 0)
			result += ',';
		result += numStr[i];
	}
	return result;
#else
	return numStr;
#endif
}


void CGame::crusade_contribution_result(int war_contribution)
{
	char temp[120]{};
	get_dialog_box_manager().disable_dialog_box(DialogBoxId::Text);
	for (int i = 0; i < game_limits::max_text_dlg_lines; i++)
	{
		if (on_game()->m_msg_text_list[i] != 0)
			on_game()->m_msg_text_list[i].reset();
	}
	if (war_contribution > 0)
	{
		audio_manager::get().play_game_sound(sound_type::effect, 23, 0, 0);
		audio_manager::get().play_game_sound(sound_type::character, 21, 0, 0);
		audio_manager::get().play_game_sound(sound_type::character, 22, 0, 0);
		on_game()->m_msg_text_list[0] = std::make_unique<CMsg>(0, m_game_msg_list[22]->m_pMsg, 0); // Congratulations! Your nation
		on_game()->m_msg_text_list[1] = std::make_unique<CMsg>(0, m_game_msg_list[23]->m_pMsg, 0); // was victory in the battle!
		on_game()->m_msg_text_list[2] = std::make_unique<CMsg>(0, " ", 0);
		on_game()->m_msg_text_list[3] = std::make_unique<CMsg>(0, m_game_msg_list[24]->m_pMsg, 0); // As a victorious citizen
		on_game()->m_msg_text_list[4] = std::make_unique<CMsg>(0, m_game_msg_list[25]->m_pMsg, 0); // You will receive
		on_game()->m_msg_text_list[5] = std::make_unique<CMsg>(0, m_game_msg_list[26]->m_pMsg, 0); // a prize
		on_game()->m_msg_text_list[6] = std::make_unique<CMsg>(0, " ", 0);
		on_game()->m_msg_text_list[7] = std::make_unique<CMsg>(0, m_game_msg_list[27]->m_pMsg, 0); // Experience point of the battle contribution:
		std::snprintf(temp, sizeof(temp), "+%d exp Points!", war_contribution);
		on_game()->m_msg_text_list[8] = std::make_unique<CMsg>(0, temp, 0);
		for (int i = 9; i < 18; i++)
			on_game()->m_msg_text_list[i] = std::make_unique<CMsg>(0, " ", 0);

	}
	else if (war_contribution < 0)
	{
		audio_manager::get().play_game_sound(sound_type::effect, 24, 0, 0);
		audio_manager::get().play_game_sound(sound_type::character, 12, 0, 0);
		audio_manager::get().play_game_sound(sound_type::character, 13, 0, 0);
		on_game()->m_msg_text_list[0] = std::make_unique<CMsg>(0, m_game_msg_list[28]->m_pMsg, 0); // Unfortunately! Your country
		on_game()->m_msg_text_list[1] = std::make_unique<CMsg>(0, m_game_msg_list[29]->m_pMsg, 0); // have lost the all out war.
		on_game()->m_msg_text_list[2] = std::make_unique<CMsg>(0, " ", 0);
		on_game()->m_msg_text_list[3] = std::make_unique<CMsg>(0, m_game_msg_list[30]->m_pMsg, 0); // As a losser citizen;
		on_game()->m_msg_text_list[4] = std::make_unique<CMsg>(0, m_game_msg_list[31]->m_pMsg, 0); // the prize that accomplishes
		on_game()->m_msg_text_list[5] = std::make_unique<CMsg>(0, m_game_msg_list[32]->m_pMsg, 0); // will not be given.
		on_game()->m_msg_text_list[6] = std::make_unique<CMsg>(0, " ", 0);
		on_game()->m_msg_text_list[7] = std::make_unique<CMsg>(0, m_game_msg_list[33]->m_pMsg, 0); // I hope you to win
		on_game()->m_msg_text_list[8] = std::make_unique<CMsg>(0, m_game_msg_list[34]->m_pMsg, 0); // in the next battle
		for (int i = 9; i < 18; i++)
			on_game()->m_msg_text_list[i] = std::make_unique<CMsg>(0, " ", 0);
	}
	else if (war_contribution == 0)
	{
		audio_manager::get().play_game_sound(sound_type::effect, 25, 0, 0);
		on_game()->m_msg_text_list[0] = std::make_unique<CMsg>(0, m_game_msg_list[50]->m_pMsg, 0); // The battle that you have participated
		on_game()->m_msg_text_list[1] = std::make_unique<CMsg>(0, m_game_msg_list[51]->m_pMsg, 0); // is already finished;
		on_game()->m_msg_text_list[2] = std::make_unique<CMsg>(0, m_game_msg_list[52]->m_pMsg, 0); //
		on_game()->m_msg_text_list[3] = std::make_unique<CMsg>(0, " ", 0);
		on_game()->m_msg_text_list[4] = std::make_unique<CMsg>(0, m_game_msg_list[53]->m_pMsg, 0); // You must connect after finishing
		on_game()->m_msg_text_list[5] = std::make_unique<CMsg>(0, m_game_msg_list[54]->m_pMsg, 0); // the previous and before starting
		on_game()->m_msg_text_list[6] = std::make_unique<CMsg>(0, m_game_msg_list[55]->m_pMsg, 0); // the next battle so you can receive
		on_game()->m_msg_text_list[7] = std::make_unique<CMsg>(0, m_game_msg_list[56]->m_pMsg, 0); // the prize
		for (int i = 8; i < 18; i++)
			on_game()->m_msg_text_list[i] = std::make_unique<CMsg>(0, " ", 0);
	}
	get_dialog_box_manager().enable_dialog_box(DialogBoxId::Text, 0, 0, 0);
}

void CGame::crusade_war_result(int winner_side)
{
	int player_side = 0;
	get_dialog_box_manager().disable_dialog_box(DialogBoxId::Text);
	for (int i = 0; i < game_limits::max_text_dlg_lines; i++)
	{
		if (on_game()->m_msg_text_list[i] != 0)
			on_game()->m_msg_text_list[i].reset();
	}
	if (m_player->m_citizen == false) player_side = 0;
	else if (m_player->m_aresden == true) player_side = 1;
	else if (m_player->m_aresden == false) player_side = 2;
	if (player_side == 0)
	{
		switch (winner_side) {
		case 0:
			audio_manager::get().play_game_sound(sound_type::effect, 25, 0, 0);
			on_game()->m_msg_text_list[0] = std::make_unique<CMsg>(0, m_game_msg_list[35]->m_pMsg, 0); // All out war finished!
			on_game()->m_msg_text_list[1] = std::make_unique<CMsg>(0, m_game_msg_list[36]->m_pMsg, 0); // There was a draw in the
			on_game()->m_msg_text_list[2] = std::make_unique<CMsg>(0, m_game_msg_list[37]->m_pMsg, 0); // battle
			on_game()->m_msg_text_list[3] = std::make_unique<CMsg>(0, " ", 0);
			break;
		case 1:
			audio_manager::get().play_game_sound(sound_type::effect, 25, 0, 0);
			on_game()->m_msg_text_list[0] = std::make_unique<CMsg>(0, m_game_msg_list[35]->m_pMsg, 0); // All out war finished!
			on_game()->m_msg_text_list[1] = std::make_unique<CMsg>(0, m_game_msg_list[38]->m_pMsg, 0); // Aresden was victorious
			on_game()->m_msg_text_list[2] = std::make_unique<CMsg>(0, m_game_msg_list[39]->m_pMsg, 0); // and put an end to the war
			on_game()->m_msg_text_list[3] = std::make_unique<CMsg>(0, " ", 0);
			break;
		case 2:
			audio_manager::get().play_game_sound(sound_type::effect, 25, 0, 0);
			on_game()->m_msg_text_list[0] = std::make_unique<CMsg>(0, m_game_msg_list[35]->m_pMsg, 0); // All out war finished!
			on_game()->m_msg_text_list[1] = std::make_unique<CMsg>(0, m_game_msg_list[40]->m_pMsg, 0); // Elvine was victorious
			on_game()->m_msg_text_list[2] = std::make_unique<CMsg>(0, m_game_msg_list[41]->m_pMsg, 0); // and put an end to the war
			on_game()->m_msg_text_list[3] = std::make_unique<CMsg>(0, " ", 0);
			break;
		}
		for (int i = 4; i < 18; i++)
			on_game()->m_msg_text_list[i] = std::make_unique<CMsg>(0, " ", 0);
	}
	else
	{
		if (winner_side == 0)
		{
			audio_manager::get().play_game_sound(sound_type::effect, 25, 0, 0);
			on_game()->m_msg_text_list[0] = std::make_unique<CMsg>(0, m_game_msg_list[35]->m_pMsg, 0); // All out war finished!
			on_game()->m_msg_text_list[1] = std::make_unique<CMsg>(0, m_game_msg_list[36]->m_pMsg, 0); // There was a draw in the
			on_game()->m_msg_text_list[2] = std::make_unique<CMsg>(0, m_game_msg_list[37]->m_pMsg, 0); // battle
			on_game()->m_msg_text_list[3] = std::make_unique<CMsg>(0, " ", 0);
			for (int i = 4; i < 18; i++)
				on_game()->m_msg_text_list[i] = std::make_unique<CMsg>(0, " ", 0);
		}
		else
		{
			if (winner_side == player_side)
			{
				audio_manager::get().play_game_sound(sound_type::effect, 23, 0, 0);
				audio_manager::get().play_game_sound(sound_type::character, 21, 0, 0);
				audio_manager::get().play_game_sound(sound_type::character, 22, 0, 0);
				switch (winner_side) {
				case 1:
					on_game()->m_msg_text_list[0] = std::make_unique<CMsg>(0, m_game_msg_list[35]->m_pMsg, 0); // All out war finished!;
					on_game()->m_msg_text_list[1] = std::make_unique<CMsg>(0, m_game_msg_list[38]->m_pMsg, 0); // Aresden was victorious;
					on_game()->m_msg_text_list[2] = std::make_unique<CMsg>(0, m_game_msg_list[39]->m_pMsg, 0); // and put an end to the war
					on_game()->m_msg_text_list[3] = std::make_unique<CMsg>(0, " ", 0);
					on_game()->m_msg_text_list[4] = std::make_unique<CMsg>(0, m_game_msg_list[42]->m_pMsg, 0); // Congratulations!
					on_game()->m_msg_text_list[5] = std::make_unique<CMsg>(0, m_game_msg_list[43]->m_pMsg, 0); // As a victorious citizen
					on_game()->m_msg_text_list[6] = std::make_unique<CMsg>(0, m_game_msg_list[44]->m_pMsg, 0); // You will receive
					on_game()->m_msg_text_list[7] = std::make_unique<CMsg>(0, m_game_msg_list[45]->m_pMsg, 0); // a prize
					break;
				case 2:
					on_game()->m_msg_text_list[0] = std::make_unique<CMsg>(0, m_game_msg_list[35]->m_pMsg, 0); // All out war finished!
					on_game()->m_msg_text_list[1] = std::make_unique<CMsg>(0, m_game_msg_list[40]->m_pMsg, 0); // Elvine was victorious
					on_game()->m_msg_text_list[2] = std::make_unique<CMsg>(0, m_game_msg_list[41]->m_pMsg, 0); // and put an end to the war
					on_game()->m_msg_text_list[3] = std::make_unique<CMsg>(0, " ", 0);
					on_game()->m_msg_text_list[4] = std::make_unique<CMsg>(0, m_game_msg_list[42]->m_pMsg, 0); // Congratulations!
					on_game()->m_msg_text_list[5] = std::make_unique<CMsg>(0, m_game_msg_list[43]->m_pMsg, 0); // As a victorious citizen
					on_game()->m_msg_text_list[6] = std::make_unique<CMsg>(0, m_game_msg_list[44]->m_pMsg, 0); // You will receive
					on_game()->m_msg_text_list[7] = std::make_unique<CMsg>(0, m_game_msg_list[45]->m_pMsg, 0); // a prize
					break;
				}
				for (int i = 8; i < 18; i++)
					on_game()->m_msg_text_list[i] = std::make_unique<CMsg>(0, " ", 0);
			}
			else if (winner_side != player_side)
			{
				audio_manager::get().play_game_sound(sound_type::effect, 24, 0, 0);
				audio_manager::get().play_game_sound(sound_type::character, 12, 0, 0);
				audio_manager::get().play_game_sound(sound_type::character, 13, 0, 0);
				switch (winner_side) {
				case 1:
					on_game()->m_msg_text_list[0] = std::make_unique<CMsg>(0, m_game_msg_list[35]->m_pMsg, 0); // All out war finished!
					on_game()->m_msg_text_list[1] = std::make_unique<CMsg>(0, m_game_msg_list[38]->m_pMsg, 0); // Aresden was victorious;
					on_game()->m_msg_text_list[2] = std::make_unique<CMsg>(0, m_game_msg_list[39]->m_pMsg, 0); // and put an end to the war
					on_game()->m_msg_text_list[3] = std::make_unique<CMsg>(0, " ", 0);
					on_game()->m_msg_text_list[4] = std::make_unique<CMsg>(0, m_game_msg_list[46]->m_pMsg, 0); // Unfortunately,
					on_game()->m_msg_text_list[5] = std::make_unique<CMsg>(0, m_game_msg_list[47]->m_pMsg, 0); // As a losser citizen
					on_game()->m_msg_text_list[6] = std::make_unique<CMsg>(0, m_game_msg_list[48]->m_pMsg, 0); // the prize that accomplishes
					on_game()->m_msg_text_list[7] = std::make_unique<CMsg>(0, m_game_msg_list[49]->m_pMsg, 0); // will not be given.
					break;
				case 2:
					on_game()->m_msg_text_list[0] = std::make_unique<CMsg>(0, m_game_msg_list[35]->m_pMsg, 0); // All out war finished!
					on_game()->m_msg_text_list[1] = std::make_unique<CMsg>(0, m_game_msg_list[40]->m_pMsg, 0); // Elvine was victorious
					on_game()->m_msg_text_list[2] = std::make_unique<CMsg>(0, m_game_msg_list[41]->m_pMsg, 0); // and put an end to the war
					on_game()->m_msg_text_list[3] = std::make_unique<CMsg>(0, " ", 0);
					on_game()->m_msg_text_list[4] = std::make_unique<CMsg>(0, m_game_msg_list[46]->m_pMsg, 0); // Unfortunately,
					on_game()->m_msg_text_list[5] = std::make_unique<CMsg>(0, m_game_msg_list[47]->m_pMsg, 0); // As a losser citizen
					on_game()->m_msg_text_list[6] = std::make_unique<CMsg>(0, m_game_msg_list[48]->m_pMsg, 0); // the prize that accomplishes
					on_game()->m_msg_text_list[7] = std::make_unique<CMsg>(0, m_game_msg_list[49]->m_pMsg, 0); // will not be given.
					break;
				}
				for (int i = 8; i < 18; i++)
					on_game()->m_msg_text_list[i] = std::make_unique<CMsg>(0, " ", 0);
			}
		}
	}
	get_dialog_box_manager().enable_dialog_box(DialogBoxId::Text, 0, 0, 0);
	get_dialog_box_manager().disable_crusade_dialogs();
}


// _Draw_UpdateScreen_OnCreateNewAccount removed - migrated to Screen_CreateAccount

void CGame::civil_right_admission_handler(char* data)
{
	uint16_t result;
	const auto* header = hb::net::PacketCast<hb::net::PacketHeader>(
		data, sizeof(hb::net::PacketHeader));
	if (!header) return;
	result = header->msg_type;

	switch (result) {
	case 0:
		get_dialog_box_manager().get_dialog_as<DialogBox_CityHallMenu>(DialogBoxId::CityHallMenu)->m_mode = DialogBox_CityHallMenu::mode::citizenship_failed;
		break;

	case 1:
		get_dialog_box_manager().get_dialog_as<DialogBox_CityHallMenu>(DialogBoxId::CityHallMenu)->m_mode = DialogBox_CityHallMenu::mode::citizenship_success;
		const auto* pkt = hb::net::PacketCast<hb::net::PacketResponseCivilRight>(
			data, sizeof(hb::net::PacketResponseCivilRight));
		if (!pkt) return;
		m_location.assign(pkt->location, strnlen(pkt->location, 10));
		if (m_location.starts_with("aresden"))
		{
			m_player->m_aresden = true;
			m_player->m_citizen = true;
			m_player->m_hunter = false;
		}
		else if (m_location.starts_with("arehunter"))
		{
			m_player->m_aresden = true;
			m_player->m_citizen = true;
			m_player->m_hunter = true;
		}
		else if (m_location.starts_with("elvine"))
		{
			m_player->m_aresden = false;
			m_player->m_citizen = true;
			m_player->m_hunter = false;
		}
		else if (m_location.starts_with("elvhunter"))
		{
			m_player->m_aresden = false;
			m_player->m_citizen = true;
			m_player->m_hunter = true;
		}
		else
		{
			m_player->m_aresden = true;
			m_player->m_citizen = false;
			m_player->m_hunter = true;
		}
		break;
	}
}


bool CGame::has_item_with_sub_type(hb::shared::item::item_sub_type::item_sub_type sub_type)
{
	for (int i = 0; i < hb::shared::limits::MaxItems; i++)
		if (m_player->m_item_list[i] != 0) {
			CItem* cfg = get_item_config(m_player->m_item_list[i]->m_id_num);
			if (cfg && cfg->get_item_sub_type() == sub_type) return true;
		}

	return false;
}

void CGame::dynamic_object_handler(char* data)
{
	short sX, sY, v1, v2, v3;
	const auto* pkt = hb::net::PacketCast<hb::net::PacketResponseDynamicObject>(
		data, sizeof(hb::net::PacketResponseDynamicObject));
	if (!pkt) return;
	sX = pkt->x;
	sY = pkt->y;
	v1 = pkt->v1;
	v2 = pkt->v2;
	v3 = pkt->v3;

	switch (pkt->header.msg_type) {
	case MsgType::Confirm:// Dynamic Object
		m_map_data->set_dynamic_object(sX, sY, v2, v1, true);
		break;

	case MsgType::Reject:// Dynamic object
		m_map_data->set_dynamic_object(sX, sY, v2, 0, true);
		break;
	}
}

bool CGame::is_item_on_hand() // Snoopy: Fixed to remove ShieldCast
{
	int i;
	for (i = 0; i < hb::shared::limits::MaxItems; i++)
		if ((m_player->m_item_list[i] != 0) && (m_is_item_equipped[i] == true))
		{
			CItem* cfg = get_item_config(m_player->m_item_list[i]->m_id_num);
			if (cfg && ((cfg->get_equip_pos() == EquipPos::LeftHand)
				|| (cfg->get_equip_pos() == EquipPos::TwoHand)))
				return true;
		}
	for (i = 0; i < hb::shared::limits::MaxItems; i++)
		if ((m_player->m_item_list[i] != 0) && (m_is_item_equipped[i] == true))
		{
			CItem* cfg = get_item_config(m_player->m_item_list[i]->m_id_num);
			if (cfg && cfg->get_equip_pos() == EquipPos::RightHand)
			{
				// Wands don't count as "item on hand" for combat
				if (cfg->get_weapon_class() == hb::shared::item::weapon_class::wand) return false;
				else return true;
			}
		}
	return false;
}

uint32_t CGame::get_level_exp(int level)
{
	return hb::shared::calc::level_exp(m_formula_engine, hb::shared::calc::level{(double)level});
}

bool CGame::check_ex_id(const char* name)
{
	if (m_ex_id == 0) return false;
	if (m_player->m_player_name == name) return false;
	std::string txt;
	txt = m_ex_id->m_pMsg;
	if (memcmp(txt.c_str(), name, 10) == 0) return true;
	else return false;
}

bool CGame::item_drop_history(short item_id)
{
	bool flag = false;
	if (m_item_drop_cnt == 0)
	{
		m_item_drop_id[m_item_drop_cnt] = item_id;
		m_item_drop_cnt++;
		return true;
	}
	if ((1 <= m_item_drop_cnt) && (20 >= m_item_drop_cnt))
	{
		for (int i = 0; i < m_item_drop_cnt; i++)
		{
			if (m_item_drop_id[i] == item_id)
			{
				flag = true;
				break;
			}
		}
		if (flag)
		{
			if (m_item_drop)
				return false;
			else
				return true;
		}

		if (20 < m_item_drop_cnt)
		{
			for (int i = 0; i < m_item_drop_cnt; i++)
				m_item_drop_id[i - 1] = item_id;
			m_item_drop_id[20] = item_id;
			m_item_drop_cnt = 21;
		}
		else
		{
			m_item_drop_id[m_item_drop_cnt] = item_id;
			m_item_drop_cnt++;
		}
	}
	return true;
}

CItem* CGame::get_item_config(int item_id) const
{
	if (item_id <= 0 || item_id >= 5000) return nullptr;
	return m_item_config_list[item_id].get();
}

item_draw_ref CGame::get_item_draw(int16_t display_id, int atlas_type, bool is_female)
{
	if (display_id >= 0)
	{
		const auto* entry = item_sprite_manager::get().find(display_id);
		if (entry != nullptr)
		{
			auto* atlas_spr = m_item_sprites.get(static_cast<size_t>(atlas_type));
			if (atlas_spr != nullptr)
			{
				int16_t frame = 0;
				if (entry->is_equippable)
				{
					const auto& gd = is_female ? entry->female : entry->male;
					switch (atlas_type)
					{
					case item_atlas::equip:  frame = gd.equip_frame;  break;
					case item_atlas::ground: frame = gd.ground_frame; break;
					case item_atlas::pack:   frame = gd.pack_frame;   break;
					}
				}
				else
				{
					// Non-equippable items (accessories, potions, etc.) have no equip
					// sprite — redirect equip requests to the pack atlas instead.
					if (atlas_type == item_atlas::equip)
						atlas_spr = m_item_sprites.get(static_cast<size_t>(item_atlas::pack));

					switch (atlas_type)
					{
					case item_atlas::ground: frame = entry->ground_frame;    break;
					default:                 frame = entry->inventory_frame; break;
					}
				}
				item_draw_ref ref;
				ref.sprite = atlas_spr;
				ref.frame = frame;
				return ref;
			}
		}
	}

	// No atlas entry — return null sprite
	hb::logger::warn("get_item_draw: unmapped display_id={} atlas_type={}", display_id, atlas_type);
	item_draw_ref ref;
	ref.sprite = hb::shared::sprite::GetNullSprite();
	ref.frame = 0;
	return ref;
}

bool CGame::ensure_config_loaded(int type)
{
	// Fast path: check if a representative config entry exists
	bool loaded = false;
	switch (type) {
	case 0: // Items
		for (int i = 1; i < 5000; i++) { if (m_item_config_list[i]) { loaded = true; break; } }
		break;
	case 1: // Magic
		for (int i = 0; i < hb::shared::limits::MaxMagicType; i++) { if (m_magic_cfg_list[i]) { loaded = true; break; } }
		break;
	case 2: // Skills
		for (int i = 0; i < hb::shared::limits::MaxSkillType; i++) { if (m_skill_cfg_list[i]) { loaded = true; break; } }
		break;
	case 3: // Npcs
		for (int i = 0; i < hb::shared::limits::MaxNpcConfigs; i++) { if (m_npc_config_list[i].valid) { loaded = true; break; } }
		break;
	case 4: // Maps
		loaded = !m_map_display_names.empty();
		break;
	}

	if (loaded) {
		m_config_retry[type] = ConfigRetryLevel::None;
		return true;
	}

	const char* configNames[] = { "ITEMCFG", "MAGICCFG", "SKILLCFG", "NPCCFG", "MAPCFG" };

	switch (m_config_retry[type]) {
	case ConfigRetryLevel::None:
		if (try_replay_cache_for_config(type)) {
			return true;
		}
		m_config_retry[type] = ConfigRetryLevel::CacheTried;
		return false;

	case ConfigRetryLevel::CacheTried:
		request_configs_from_server(type == 0, type == 1, type == 2, type == 3, type == 4);
		m_config_retry[type] = ConfigRetryLevel::ServerRequested;
		m_config_request_time = GameClock::get_time_ms();
		return false;

	case ConfigRetryLevel::ServerRequested:
		if (GameClock::get_time_ms() - m_config_request_time > CONFIG_REQUEST_TIMEOUT_MS) {
			m_config_retry[type] = ConfigRetryLevel::Failed;
			change_game_mode(GameMode::ConnectionLost);
		}
		return false;

	case ConfigRetryLevel::Failed:
		return false;
	}
	return false;
}

bool CGame::try_replay_cache_for_config(int type)
{
	struct ReplayCtx { CGame* game; };
	ReplayCtx ctx{ this };

	ConfigCacheType cacheType = static_cast<ConfigCacheType>(type);

	switch (type) {
	case 0:
		return LocalCacheManager::get().replay_from_cache(cacheType,
			[](char* p, uint32_t s, void* c) -> bool {
				return static_cast<ReplayCtx*>(c)->game->cache_process_item_config(p, s);
			}, &ctx);
	case 1:
		return LocalCacheManager::get().replay_from_cache(cacheType,
			[](char* p, uint32_t s, void* c) -> bool {
				return static_cast<ReplayCtx*>(c)->game->cache_process_magic_config(p, s);
			}, &ctx);
	case 2:
		return LocalCacheManager::get().replay_from_cache(cacheType,
			[](char* p, uint32_t s, void* c) -> bool {
				return static_cast<ReplayCtx*>(c)->game->cache_process_skill_config(p, s);
			}, &ctx);
	case 3:
		return LocalCacheManager::get().replay_from_cache(cacheType,
			[](char* p, uint32_t s, void* c) -> bool {
				return static_cast<ReplayCtx*>(c)->game->cache_process_npc_config(p, s);
			}, &ctx);
	case 4:
		return LocalCacheManager::get().replay_from_cache(cacheType,
			[](char* p, uint32_t s, void* c) -> bool {
				return static_cast<ReplayCtx*>(c)->game->cache_process_map_config(p, s);
			}, &ctx);
	}
	return false;
}

void CGame::request_configs_from_server(bool items, bool magic, bool skills, bool npcs, bool maps, bool balance, bool color_palette, bool attribute_types)
{
	if (!m_g_sock) return;
	hb::net::PacketRequestConfigData pkt{};
	pkt.header.msg_id = MsgId::RequestConfigData;
	pkt.header.msg_type = 0;
	pkt.requestItems = items ? 1 : 0;
	pkt.requestMagic = magic ? 1 : 0;
	pkt.requestSkills = skills ? 1 : 0;
	pkt.requestNpcs = npcs ? 1 : 0;
	pkt.requestMaps = maps ? 1 : 0;
	pkt.requestBalance = balance ? 1 : 0;
	pkt.requestColorPalette = color_palette ? 1 : 0;
	pkt.requestAttributeTypes = attribute_types ? 1 : 0;
	m_g_sock->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
}

void CGame::check_configs_ready_and_enter_game()
{
	if (m_configs_ready) return;

	// Check if all five config types have at least one entry loaded
	bool has_items = false, has_magic = false, has_skills = false, has_npcs = false;
	for (int i = 1; i < 5000; i++) { if (m_item_config_list[i]) { has_items = true; break; } }
	for (int i = 0; i < hb::shared::limits::MaxMagicType; i++) { if (m_magic_cfg_list[i]) { has_magic = true; break; } }
	for (int i = 0; i < hb::shared::limits::MaxSkillType; i++) { if (m_skill_cfg_list[i]) { has_skills = true; break; } }
	for (int i = 0; i < hb::shared::limits::MaxNpcConfigs; i++) { if (m_npc_config_list[i].valid) { has_npcs = true; break; } }
	bool has_maps = !m_map_display_names.empty();

	if (has_items && has_magic && has_skills && has_npcs && has_maps) {
		m_configs_ready = true;
		if (m_init_data_ready) {
			GameModeManager::clear_overlay();
		}
	}
}

short CGame::find_item_id_by_name(const char* item_name)
{
	if (item_name == nullptr) return 0;
	for (int i = 1; i < 5000; i++) {
		if (m_item_config_list[i] != nullptr &&
			memcmp(m_item_config_list[i]->m_name, item_name, hb::shared::limits::ItemNameLen - 1) == 0) {
			return static_cast<short>(i);
		}
	}
	return 0; // Not found
}

void CGame::noticement_handler(char* data)
{
	const auto* header = hb::net::PacketCast<hb::net::PacketHeader>(
		data, sizeof(hb::net::PacketHeader));
	if (!header) return;
	switch (header->msg_type) {
	case MsgType::Confirm:
	case MsgType::Reject:
		const auto* pkt = hb::net::PacketCast<hb::net::PacketResponseNoticementText>(
			data, sizeof(hb::net::PacketResponseNoticementText));
		if (!pkt) return;
		{
			std::ofstream file("contents/contents1000.txt");
			if (!file) return;
			file << pkt->text;
		}
		get_dialog_box_manager().get_dialog_box(DialogBoxId::Text)->m_x = 20;
		get_dialog_box_manager().get_dialog_box(DialogBoxId::Text)->m_y = 65;
		get_dialog_box_manager().enable_dialog_box(DialogBoxId::Text, 1000, 0, 0);
		break;
	}
	add_event_list("Press F1 for news and help.", 10);
	get_dialog_box_manager().enable_dialog_box(DialogBoxId::Help, 0, 0, 0);
}

void CGame::set_ilusion_effect(int owner_h)
{
	direction dir;

	on_game()->m_ilusion_owner_h = owner_h;

	std::string nameBuf_IE;
	m_map_data->get_owner_status_by_object_id(owner_h, &on_game()->m_ilusion_owner_type, &dir, &m_player->m_illusionAppearance, &m_player->m_illusionStatus, nameBuf_IE);
	(void)nameBuf_IE; // Unused
}

void CGame::response_panning_handler(char* data)
{
	direction dir;
	short sX, sY;
	const auto* pkt = hb::net::PacketCast<hb::net::PacketResponsePanningHeader>(
		data, sizeof(hb::net::PacketResponsePanningHeader));
	if (!pkt) return;
	sX = pkt->x;
	sY = pkt->y;
	dir = static_cast<direction>(pkt->dir);

	switch (dir) {
	case direction::north: m_Camera.move_destination(0, -32); m_player->m_player_y--; break;
	case direction::northeast: m_Camera.move_destination(32, -32); m_player->m_player_y--; m_player->m_player_x++; break;
	case direction::east: m_Camera.move_destination(32, 0); m_player->m_player_x++; break;
	case direction::southeast: m_Camera.move_destination(32, 32); m_player->m_player_y++; m_player->m_player_x++; break;
	case direction::south: m_Camera.move_destination(0, 32); m_player->m_player_y++; break;
	case direction::southwest: m_Camera.move_destination(-32, 32); m_player->m_player_y++; m_player->m_player_x--; break;
	case direction::west: m_Camera.move_destination(-32, 0); m_player->m_player_x--; break;
	case direction::northwest: m_Camera.move_destination(-32, -32); m_player->m_player_y--; m_player->m_player_x--; break;
	default: break;
	}

	m_map_data->shift_map_data(dir);
	const char* mapData = reinterpret_cast<const char*>(data) + sizeof(hb::net::PacketResponsePanningHeader);
	read_map_data(sX, sY, mapData);

	on_game()->m_is_observer_commanded = false;
}

/*********************************************************************************************************************
** void CGame::create_screen_shot()										(snoopy)									**
**  description			:: Fixed Screen Shots																		**
**********************************************************************************************************************/
void CGame::create_screen_shot()
{
	char longMapName[128] = {};
	get_official_map_name(m_map_name.c_str(), longMapName);

	auto now = std::chrono::system_clock::now();
	auto time = std::chrono::system_clock::to_time_t(now);
	std::tm tm{};
#ifdef _WIN32
	localtime_s(&tm, &time);
#else
	localtime_r(&time, &tm);
#endif

	std::string timeStr = std::format("{:02d}:{:02d} - {:02d}:{:02d}:{:02d}",
		tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
	hb::shared::text::draw_text_aligned(GameFont::Default, 500, 30, 150, 15, timeStr.c_str(),
		hb::shared::text::TextStyle::from_color(GameColors::UIWhite),
		hb::shared::text::Align::TopCenter);

	std::filesystem::create_directory("save");

	for (int i = 0; i < 1000; i++)
	{
		std::string fileName = std::format("save/helshot{:04d}{:02d}{:02d}_{:02d}{:02d}{:02d}_{}{:03d}.png",
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec,
			longMapName, i);

		if (!std::filesystem::exists(fileName))
		{
			m_Renderer->screenshot(fileName.c_str());
			add_event_list(std::format(NOTIFYMSG_CREATE_SCREENSHOT1, fileName).c_str(), 10);
			return;
		}
	}
	add_event_list(NOTIFYMSG_CREATE_SCREENSHOT2, 10);
}


void CGame::handle_key_up(KeyCode _key)
{
	auto* screen = GameModeManager::get().get_active_screen();
	if (screen) screen->on_key_up(_key);
}

void CGame::handle_key_down(KeyCode _key)
{
	// Overlay gate: only screenshot allowed
	if (GameModeManager::get_active_overlay() != nullptr)
	{
		if (_key == KeyCode::F11) create_screen_shot();
		return;
	}
	// Route to active screen
	auto* screen = GameModeManager::get().get_active_screen();
	if (screen) screen->on_key_down(_key);
}

// reserve_fightzone_response_handler removed with guild/fightzone system


// UpdateScreen_OnLogResMsg removed - replaced by Overlay_LogResMsg

void CGame::retrieve_item_handler(char* data)
{
	std::string txt;

	char bank_item_index, item_index;
	int j;
	const auto* header = hb::net::PacketCast<hb::net::PacketHeader>(
		data, sizeof(hb::net::PacketHeader));
	if (!header) return;
	if (header->msg_type != MsgType::Reject)
	{
		const auto* pkt = hb::net::PacketCast<hb::net::PacketResponseRetrieveItem>(
			data, sizeof(hb::net::PacketResponseRetrieveItem));
		if (!pkt) return;
		bank_item_index = static_cast<char>(pkt->bank_index);
		item_index = static_cast<char>(pkt->item_index);

		if (m_player->m_bank_list[bank_item_index] != 0) {
			// v1.42
			auto itemInfo = item_name_formatter::get().format(m_player->m_bank_list[bank_item_index].get());

			txt = std::format(RETIEVE_ITEM_HANDLER4, itemInfo.name.c_str());//""You took out %s."
			add_event_list(txt.c_str(), 10);

			CItem* cfg_bank = get_item_config(m_player->m_bank_list[bank_item_index]->m_id_num);
			bool stackable = cfg_bank && (cfg_bank->is_stackable());

			if (stackable && m_player->m_item_list[item_index] != 0)
			{
				// Stackable item with occupied inventory slot: just remove from bank
				m_player->m_bank_list[bank_item_index].reset();
			}
			else
			{
				// Non-stackable or empty inventory slot: place item into inventory
				if (m_player->m_item_list[item_index] != 0) return;
				short nX = 40;
				short nY = 30;
				for (j = 0; j < hb::shared::limits::MaxItems; j++)
				{
					if ((m_player->m_item_list[j] != 0) && (m_player->m_item_list[j]->m_id_num == m_player->m_bank_list[bank_item_index]->m_id_num))
					{
						nX = m_player->m_item_list[j]->m_x + 1;
						nY = m_player->m_item_list[j]->m_y + 1;
						break;
					}
				}
				m_player->m_item_list[item_index] = std::move(m_player->m_bank_list[bank_item_index]);
				m_player->m_item_list[item_index]->m_x = nX;
				m_player->m_item_list[item_index]->m_y = nY;
				{
			hb::net::PacketRequestSetItemPos req{};
			req.header.msg_id = MsgId::RequestSetItemPos;
			req.header.msg_type = 0;
			req.dir = static_cast<uint8_t>(item_index);
			req.x = static_cast<int16_t>(nX);
			req.y = static_cast<int16_t>(nY);
			send_game_packet(req, false);
		}

				for (j = 0; j < hb::shared::limits::MaxItems; j++)
					if (m_item_order[j] == -1)
					{
						m_item_order[j] = item_index;
						break;
					}
				m_is_item_equipped[item_index] = false;
				inventory_manager::get().unlock_item(item_index);
			}
			// Compact bank list
			for (j = 0; j <= hb::shared::limits::MaxBankItems - 2; j++)
			{
				if ((m_player->m_bank_list[j + 1] != 0) && (m_player->m_bank_list[j] == 0))
				{
					m_player->m_bank_list[j] = std::move(m_player->m_bank_list[j + 1]);
				}
			}
		}
	}
	get_dialog_box_manager().get_dialog_as<DialogBox_Bank>(DialogBoxId::Bank)->m_mode = DialogBox_Bank::mode::list;
}

// draw_npc_name REMOVED — moved to Screen_OnGame
// draw_object_name REMOVED — moved to Screen_OnGame

void CGame::draw_version()
{
	std::string G_cTxt;
	G_cTxt = std::format("Ver: {}", hb::version::client::full_version);
	hb::shared::text::draw_text(GameFont::Default, 12, (LOGICAL_HEIGHT() - 12 - 14), G_cTxt.c_str(), hb::shared::text::TextStyle::with_shadow(GameColors::UIDisabled));
}

char CGame::get_official_map_name(const char* map_name, char* name)
{	// MapIndex
	// Run the hardcoded chain to get the map_index, then override name from cache if available.
	char map_index = get_hardcoded_map_index(map_name, name);

	auto it = m_map_display_names.find(map_name);
	if (it != m_map_display_names.end() && !it->second.empty()) {
		std::snprintf(name, 21, "%s", it->second.c_str());
	}
	return map_index;
}

char CGame::get_hardcoded_map_index(const char* map_name, char* name)
{
	if (strcmp(map_name, "middleland") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME28);	// Middleland
		return 4;
	}
	else if (strcmp(map_name, "huntzone3") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME31);	// Death Valley
		return 0;
	}
	else if (strcmp(map_name, "huntzone1") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME29);	// Rocky Highland
		return 1;
	}
	else if (strcmp(map_name, "elvuni") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME57);	// Eldiniel Garden
		return 2;
	}
	else if (strcmp(map_name, "elvine") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME24);	// Elvine City
		return 3;
	}
	else if (strcmp(map_name, "elvfarm") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME2);	// Elvine Farm
		return 5;
	}
	else if (strcmp(map_name, "arefarm") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME1);	// Aresden Farm
		return 6;
	}
	else if (strcmp(map_name, "default") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME3);	// Beginner Zone
		return 7;
	}
	else if (strcmp(map_name, "huntzone4") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME32);	// Silent Wood
		return 8;
	}
	else if (strcmp(map_name, "huntzone2") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME30);	// Eternal Field
		return 9;
	}
	else if (strcmp(map_name, "areuni") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME56);	// Aresien Garden
		return 10;
	}
	else if (strcmp(map_name, "aresden") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME22);	// Aresden City
		return 11;
	}
	else if (strcmp(map_name, "dglv2") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME25);	// Dungeon L2
		return 12;
	}
	else if (strcmp(map_name, "dglv3") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME26);	// Dungeon L3
		return 13;
	}
	else if (strcmp(map_name, "dglv4") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME53);	// Dungeon L4
		return 14;
	}
	else if (strcmp(map_name, "elvined1") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME23);	// Elvine Dungeon
		return 15;
	}
	else if (strcmp(map_name, "aresdend1") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME21);	// Aresden Dungeon
		return 16;
	}
	else if (strcmp(map_name, "bisle") == 0) {
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME27);	// Bleeding Island
		return 17;
	}
	else if (strcmp(map_name, "toh1") == 0) {
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME60);	// Tower of Hell 1
		return 18;
	}
	else if (strcmp(map_name, "toh2") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME61);	// Tower of Hell 2
		return 19;
	}
	else if (strcmp(map_name, "toh3") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME62);	// Tower of Hell 3
		return 20;
	}
	else if (strcmp(map_name, "middled1x") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME58);	// Middleland Mine
		return 21;
	}
	else if (strcmp(map_name, "middled1n") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME59);	// Middleland Dungeon
		return 22;
	}
	else if (strcmp(map_name, "2ndmiddle") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME65);	// Promiseland
		return 23;
	}
	else if (strcmp(map_name, "icebound") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME66);	// Ice Map
		return 24;
		// Snoopy:
	}
	else if (strcmp(map_name, "druncncity") == 0) // Snoopy: Apocalypse maps
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME70);
		return 25;
	}
	else if (strcmp(map_name, "inferniaA") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME71);
		return 26;
	}
	else if (strcmp(map_name, "inferniaB") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME72);
		return 27;
	}
	else if (strcmp(map_name, "maze") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME73);
		return 28;
	}
	else if (strcmp(map_name, "procella") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME74);
		return 29;
	}
	else if (strcmp(map_name, "abaddon") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME75);
		return 30;
	}
	else if (strcmp(map_name, "BtField") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME76);
		return 35;
	}
	else if (strcmp(map_name, "GodH") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME77);
		return 36;
	}
	else if (strcmp(map_name, "HRampart") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME78);
		return 37;
	}
	else if (strcmp(map_name, "cityhall_1") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME35);	// Aresden Cityhall
		return -1;
	}
	else if (strcmp(map_name, "cityhall_2") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME36);	// Elvine Cityhall
		return -1;
	}
	else if (strcmp(map_name, "gldhall_1") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME37);	// Aresden Guildhall
		return -1;
	}
	else if (strcmp(map_name, "gldhall_2") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME38);	// Elvine Guildhall
		return -1;
	}
	else if (strcmp(map_name, "bsmith_1") == 0 || strcmp(map_name, "bsmith_1f") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME33);	// Aresden Blacksmith
		return -1;
	}
	else if (strcmp(map_name, "bsmith_2") == 0 || strcmp(map_name, "bsmith_2f") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME34);	// Elvine Blacksmith
		return -1;
	}
	else if (strcmp(map_name, "gshop_1") == 0 || strcmp(map_name, "gshop_1f") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME39);	// Aresden Shop
		return -1;
	}
	else if (strcmp(map_name, "gshop_2") == 0 || strcmp(map_name, "gshop_2f") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME40);	// Elvine Shop
		return -1;
	}
	else if (strcmp(map_name, "wrhus_1") == 0 || strcmp(map_name, "wrhus_1f") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME43);	// Aresden Warehouse
		return -1;
	}
	else if (strcmp(map_name, "wrhus_2") == 0 || strcmp(map_name, "wrhus_2f") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME44);	// Elvine Warehouse
		return -1;
	}
	else if (strcmp(map_name, "arewrhus") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME45);	// Aresden Warehouse
		return -1;
	}
	else if (strcmp(map_name, "elvwrhus") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME46);	// Elvine Warehouse
		return -1;
	}
	else if (strcmp(map_name, "wzdtwr_1") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME41);	// Magic Tower
		return -1;
	}
	else if (strcmp(map_name, "wzdtwr_2") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME42);	// Magic Tower
		return -1;
	}
	else if (strcmp(map_name, "cath_1") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME47);	// Aresien Church
		return -1;
	}
	else if (strcmp(map_name, "cath_2") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME48);	// Eldiniel Church
		return -1;
	}
	else if (strcmp(map_name, "resurr1") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME54);	// Revival Zone
		return -1;
	}
	else if (strcmp(map_name, "resurr2") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME55);	// Revival Zone
		return -1;
	}
	else if (strcmp(map_name, "arebrk11") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME4);	// Aresden Barrack 1
		return -1;
	}
	else if (strcmp(map_name, "arebrk12") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME5);	// Aresden Barrack 1
		return -1;
	}
	else if (strcmp(map_name, "arebrk21") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME6);	// Aresden Barrack 2
		return -1;
	}
	else if (strcmp(map_name, "arebrk22") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME7);	// Aresden Barrack 2
		return -1;
	}
	else if (strcmp(map_name, "elvbrk11") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME8);	// Elvine Barrack 1
		return -1;
	}
	else if (strcmp(map_name, "elvbrk12") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME9);	// Elvine Barrack 1
		return -1;
	}
	else if (strcmp(map_name, "elvbrk21") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME10);	// Elvine Barrack 2
		return -1;
	}
	else if (strcmp(map_name, "elvbrk22") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME11);	// Elvine Barrack 2
		return -1;
	}
	else if (strcmp(map_name, "fightzone1") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME12);	// Arena 1
		return -1;
	}
	else if (strcmp(map_name, "fightzone2") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME13);	// Arena 2
		return -1;
	}
	else if (strcmp(map_name, "fightzone3") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME14);	// Arena 3
		return -1;
	}
	else if (strcmp(map_name, "fightzone4") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME15);	// Arena 4
		return -1;
	}
	else if (strcmp(map_name, "fightzone5") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME16);	// Arena 5
		return -1;
	}
	else if (strcmp(map_name, "fightzone6") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME17);	// Arena 6
		return -1;
	}
	else if (strcmp(map_name, "fightzone7") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME18);	// Arena 7
		return -1;
	}
	else if (strcmp(map_name, "fightzone8") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME19);	// Arena 8
		return -1;
	}
	else if (strcmp(map_name, "fightzone9") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME20);	// Arena 9
		return -1;
	}
	else if (strcmp(map_name, "arejail") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME63);	// Aresden Jail
		return -1;
	}
	else if (strcmp(map_name, "elvjail") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME64);	// Elvine Jail
		return -1;
	}
	else if (strcmp(map_name, "CmdHall_1") == 0) // Snoopy: Commander Halls
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME79);
		return -1;
	}
	else if (strcmp(map_name, "CmdHall_2") == 0)
	{
		std::snprintf(name, 21, "%s", GET_OFFICIAL_MAP_NAME79);
		return -1;
	}
	else
	{
		std::snprintf(name, 21, "%s", map_name);
		return -1;
	}
}

bool CGame::check_local_chat_command(const char* pMsg)
{
	return ChatCommandManager::get().process_command(pMsg);
}

void CGame::clear_skill_using_status()
{
	if (on_game()->m_skill_using_status == true)
	{
		add_event_list(CLEAR_SKILL_USING_STATUS1, 10);//"
		get_dialog_box_manager().disable_crafting_dialogs();
		if ((m_player->m_player_type >= 1) && (m_player->m_player_type <= 6)/* && (!m_player->m_playerAppearance.is_walking)*/) {
			m_player->m_Controller.set_command(Type::stop);
			m_player->m_Controller.set_destination(m_player->m_player_x, m_player->m_player_y);
		}
	}
	on_game()->m_skill_using_status = false;
}

void CGame::npc_talk_handler(char* packet_data)
{
	std::string text;

	char reward_name[hb::shared::limits::ItemNameLen]{};
	char target_name[21]{};
	char temp[21]{};

	const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyNpcTalk>(
		packet_data, sizeof(hb::net::PacketNotifyNpcTalk));
	if (!pkt) return;

	short npc_type = pkt->type;
	short response = pkt->response;
	int amount = pkt->amount;
	int contribution = pkt->contribution;
	int target_type = pkt->target_config_id;
	int target_count = pkt->target_count;
	int target_x = pkt->x;
	int target_y = pkt->y;
	int target_range = pkt->range;
	int question_type = 0;
	int index = 0;

	std::memset(reward_name, 0, sizeof(reward_name));
	memcpy(reward_name, pkt->reward_name, hb::shared::limits::ItemNameLen - 1);
	memcpy(target_name, pkt->target_name, 20);

	get_dialog_box_manager().enable_dialog_box(DialogBoxId::NpcTalk, response, npc_type, 0);

	if ((npc_type >= 1) && (npc_type <= 100))
	{
		index = get_dialog_box_manager().get_dialog_as<DialogBox_NpcTalk>(DialogBoxId::NpcTalk)->m_text_line_count;
		on_game()->m_msg_text_list2[index] = std::make_unique<CMsg>(0, "  ", 0);
		index++;
		question_type = 0;
		switch (npc_type) {
		case 1: //Monster Hunt
			std::snprintf(temp, hb::shared::limits::NpcNameLen, "%s", get_npc_config_name_by_id(target_type));
			text = std::format(NPC_TALK_HANDLER16, target_count, temp);
			on_game()->m_msg_text_list2[index] = std::make_unique<CMsg>(0, text.c_str(), 0);
			index++;

			if (memcmp(target_name, "NONE", 4) == 0) {
				text = NPC_TALK_HANDLER17;//"
				on_game()->m_msg_text_list2[index] = std::make_unique<CMsg>(0, text.c_str(), 0);
				index++;
			}
			else {
				std::memset(temp, 0, sizeof(temp));
				get_official_map_name(target_name, temp);
				text = std::format(NPC_TALK_HANDLER18, temp);//"Map : %s"
				on_game()->m_msg_text_list2[index] = std::make_unique<CMsg>(0, text.c_str(), 0);
				index++;

				if (target_x != 0) {
					text = std::format(NPC_TALK_HANDLER19, target_x, target_y, target_range);//"Position: %d,%d within %d blocks"
					on_game()->m_msg_text_list2[index] = std::make_unique<CMsg>(0, text.c_str(), 0);
					index++;
				}

				text = std::format(NPC_TALK_HANDLER20, contribution);//"
				on_game()->m_msg_text_list2[index] = std::make_unique<CMsg>(0, text.c_str(), 0);
				index++;
			}
			question_type = 1;
			break;

		case 7: //
			on_game()->m_msg_text_list2[index] = std::make_unique<CMsg>(0, NPC_TALK_HANDLER21, 0);
			index++;

			if (memcmp(target_name, "NONE", 4) == 0) {
				text = NPC_TALK_HANDLER22;
				on_game()->m_msg_text_list2[index] = std::make_unique<CMsg>(0, text.c_str(), 0);
				index++;
			}
			else {
				std::memset(temp, 0, sizeof(temp));
				get_official_map_name(target_name, temp);
				text = std::format(NPC_TALK_HANDLER23, temp);
				on_game()->m_msg_text_list2[index] = std::make_unique<CMsg>(0, text.c_str(), 0);
				index++;

				if (target_x != 0) {
					text = std::format(NPC_TALK_HANDLER24, target_x, target_y, target_range);
					on_game()->m_msg_text_list2[index] = std::make_unique<CMsg>(0, text.c_str(), 0);
					index++;
				}

				text = std::format(NPC_TALK_HANDLER25, contribution);
				on_game()->m_msg_text_list2[index] = std::make_unique<CMsg>(0, text.c_str(), 0);
				index++;
			}
			question_type = 1;
			break;

		case hb::shared::owner::Slime: // Crusade
			on_game()->m_msg_text_list2[index] = std::make_unique<CMsg>(0, NPC_TALK_HANDLER26, 0);
			index++;

			text = NPC_TALK_HANDLER27;//"
			on_game()->m_msg_text_list2[index] = std::make_unique<CMsg>(0, text.c_str(), 0);
			index++;

			text = NPC_TALK_HANDLER28;//"
			on_game()->m_msg_text_list2[index] = std::make_unique<CMsg>(0, text.c_str(), 0);
			index++;

			text = NPC_TALK_HANDLER29;//"
			on_game()->m_msg_text_list2[index] = std::make_unique<CMsg>(0, text.c_str(), 0);
			index++;

			text = NPC_TALK_HANDLER30;//"
			on_game()->m_msg_text_list2[index] = std::make_unique<CMsg>(0, text.c_str(), 0);
			index++;

			text = " ";
			on_game()->m_msg_text_list2[index] = std::make_unique<CMsg>(0, text.c_str(), 0);
			index++;

			if (memcmp(target_name, "NONE", 4) == 0) {
				text = NPC_TALK_HANDLER31;//"
				on_game()->m_msg_text_list2[index] = std::make_unique<CMsg>(0, text.c_str(), 0);
				index++;
			}
			else {
				std::memset(temp, 0, sizeof(temp));
				get_official_map_name(target_name, temp);
				text = std::format(NPC_TALK_HANDLER32, temp);//"
				on_game()->m_msg_text_list2[index] = std::make_unique<CMsg>(0, text.c_str(), 0);
				index++;
			}
			question_type = 2;
			break;
		}

		switch (question_type) {
		case 1:
			on_game()->m_msg_text_list2[index] = std::make_unique<CMsg>(0, "  ", 0);
			index++;
			on_game()->m_msg_text_list2[index] = std::make_unique<CMsg>(0, NPC_TALK_HANDLER33, 0);//"
			index++;
			on_game()->m_msg_text_list2[index] = std::make_unique<CMsg>(0, NPC_TALK_HANDLER34, 0);//"
			index++;
			on_game()->m_msg_text_list2[index] = std::make_unique<CMsg>(0, "  ", 0);
			index++;
			break;

		case 2:
			on_game()->m_msg_text_list2[index] = std::make_unique<CMsg>(0, "  ", 0);
			index++;
			on_game()->m_msg_text_list2[index] = std::make_unique<CMsg>(0, NPC_TALK_HANDLER35, 0);//"
			index++;
			on_game()->m_msg_text_list2[index] = std::make_unique<CMsg>(0, "  ", 0);
			index++;
			break;

		default: break;
		}
	}
}

void CGame::point_command_handler(int indexX, int indexY, char item_id)
{
	char temp[hb::shared::limits::ItemNameLen]{};
	if ((on_game()->m_point_command_type >= 100) && (on_game()->m_point_command_type < 200))
	{
		// get target object ID for auto-aim (lag compensation)
		// If player clicked on an entity, send its ID so server can track its current position
		int targetObjectID = 0;
		const FocusedObject& focused = CursorTarget::GetFocusedObject();
		if (focused.m_valid && (focused.m_type == FocusedObjectType::Player || focused.m_type == FocusedObjectType::NPC))
		{
			targetObjectID = focused.m_object_id;
		}
		{
			hb::net::PacketCommandCommonWithTime req{};
			req.base.header.msg_id = MsgId::CommandCommon;
			req.base.header.msg_type = CommonType::Magic;
			req.base.x = m_player->m_player_x;
			req.base.y = m_player->m_player_y;
			req.v1 = indexX;
			req.v2 = indexY;
			req.v3 = on_game()->m_point_command_type;
			req.time_ms = static_cast<uint32_t>(targetObjectID);
			send_game_packet(req, false);
		}
	}
	else if ((on_game()->m_point_command_type >= 0) && (on_game()->m_point_command_type < 50))
	{
		{
			auto pkt = hb::net::make_common_command_str(CommonType::ReqUseItem, m_player->m_player_x, m_player->m_player_y);
			pkt.v1 = on_game()->m_point_command_type;
			pkt.v2 = indexX;
			pkt.v3 = indexY;
			std::snprintf(pkt.text, sizeof(pkt.text), "%s", temp);
			pkt.v4 = item_id;
			send_game_packet(pkt);
		}

		CItem* cfg_pt = get_item_config(m_player->m_item_list[on_game()->m_point_command_type]->m_id_num);
		if (cfg_pt && cfg_pt->get_item_type() == hb::shared::item::item_type::tool)
			on_game()->m_skill_using_status = true;
	}
	else if (on_game()->m_point_command_type == 200) // Normal Hand
	{
		if ((m_mc_name.size() == 0) || (m_mc_name == m_player->m_player_name) || (m_mc_name[0] == '_'))
		{
			get_dialog_box_manager().get_dialog_as<DialogBox_Party>(DialogBoxId::Party)->m_mode = DialogBox_Party::mode::main_menu;
			audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
			add_event_list(POINT_COMMAND_HANDLER1, 10);
		}
		else
		{
			get_dialog_box_manager().get_dialog_as<DialogBox_Party>(DialogBoxId::Party)->m_mode = DialogBox_Party::mode::join_requested;
			audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
			std::snprintf(get_dialog_box_manager().get_dialog_as<DialogBox_Party>(DialogBoxId::Party)->m_leader_name, sizeof(DialogBox_Party::m_leader_name), "%s", m_mc_name.c_str());
			{
				auto pkt = hb::net::make_common_command_str(CommonType::RequestJoinParty, m_player->m_player_x, m_player->m_player_y);
				pkt.v1 = 1;
				std::snprintf(pkt.text, sizeof(pkt.text), "%s", m_mc_name.c_str());
				send_game_packet(pkt);
			}
			return;
		}
	}
}


void CGame::start_bgm()
{
	// Determine track name based on current location
	const char* trackName = "maintm";

	if ((on_game()->m_is_xmas == true) && (weather_manager::get().is_snowing()))
	{
		trackName = "carol";
	}
	else if (m_cur_location.starts_with("aresden"))
	{
		trackName = "aresden";
	}
	else if (m_cur_location.starts_with("elvine"))
	{
		trackName = "elvine";
	}
	else if (m_cur_location.starts_with("dglv"))
	{
		trackName = "dungeon";
	}
	else if (m_cur_location.starts_with("middled1"))
	{
		trackName = "dungeon";
	}
	else if (m_cur_location.starts_with("middleland"))
	{
		trackName = "middleland";
	}
	else if (m_cur_location.starts_with("druncncity"))
	{
		trackName = "druncncity";
	}
	else if (m_cur_location.starts_with("inferniaA"))
	{
		trackName = "middleland";
	}
	else if (m_cur_location.starts_with("inferniaB"))
	{
		trackName = "middleland";
	}
	else if (m_cur_location.starts_with("maze"))
	{
		trackName = "dungeon";
	}
	else if (m_cur_location.starts_with("abaddon"))
	{
		trackName = "abaddon";
	}

	// Forward to audio_manager
	audio_manager::get().play_music(trackName);
}

void CGame::motion_response_handler(char* packet_data)
{
	uint16_t response = 0;
	short map_x = 0, map_y = 0;
	direction move_dir = direction{};
	int previous_hp = 0;
	//						          0 3        4 5						 6 7		8 9		   10	    11
	// Confirm Code(4) | MsgSize(4) | MsgID(4) | Confirm::MoveConfirm(2) | Loc-X(2) | Loc-Y(2) | Dir(1) | MapData ...
	// Confirm Code(4) | MsgSize(4) | MsgID(4) | Confirm::MoveReject(2)  | Loc-X(2) | Loc-Y(2)
	const auto* header = hb::net::PacketCast<hb::net::PacketHeader>(
		packet_data, sizeof(hb::net::PacketHeader));
	if (!header) return;
	response = header->msg_type;

	switch (response) {
	case Confirm::MotionConfirm:
		m_player->m_Controller.decrement_command_count();
		break;

	case Confirm::MotionAttackConfirm:
		m_player->m_Controller.decrement_command_count();
		break;

	case Confirm::MotionReject:
		if (m_player->m_hp <= 0) return;
		{
			const auto* pkt = hb::net::PacketCast<hb::net::PacketResponseMotionReject>(
				packet_data, sizeof(hb::net::PacketResponseMotionReject));
			if (!pkt) return;
			m_player->m_player_x = pkt->x;
			m_player->m_player_y = pkt->y;
		}

		m_player->m_Controller.set_command(Type::stop);
		m_player->m_Controller.set_destination(m_player->m_player_x, m_player->m_player_y);

		m_map_data->set_owner(m_player->m_player_object_id, m_player->m_player_x, m_player->m_player_y, m_player->m_player_type, m_player->m_player_dir,
			m_player->m_playerAppearance,
			m_player->m_playerStatus, m_player->m_player_name,
			Type::stop, 0, 0, 0);
		m_player->m_Controller.reset_command_count();
		on_game()->m_is_get_pointing_mode = false;
		m_Camera.snap_to((m_player->m_player_x - VIEW_CENTER_TILE_X()) * 32 - 16, (m_player->m_player_y - VIEW_CENTER_TILE_Y()) * 32 - 16);
		break;

	case Confirm::MoveConfirm:
	{
		const auto* pkt = hb::net::PacketCast<hb::net::PacketResponseMotionMoveConfirm>(
			packet_data, sizeof(hb::net::PacketResponseMotionMoveConfirm));
		if (!pkt) return;
		map_x = pkt->x;
		map_y = pkt->y;
		move_dir = static_cast<direction>(pkt->dir);
		m_player->m_sp = m_player->m_sp - pkt->stamina_cost;
		if (m_player->m_sp < 0) m_player->m_sp = 0;
		previous_hp = m_player->m_hp;
		m_player->m_hp = pkt->hp;

		if (m_player->m_hp != previous_hp)
		{
			std::string G_cTxt;
			if (m_player->m_hp < previous_hp)
			{
				G_cTxt = std::format(NOTIFYMSG_HP_DOWN, previous_hp - m_player->m_hp);
				add_event_list(G_cTxt.c_str(), 10);
				m_damaged_time = GameClock::get_time_ms();
				if ((on_game()->m_logout_count > 0) && (m_force_disconn == false))
				{
					on_game()->m_logout_count = -1;
					add_event_list(MOTION_RESPONSE_HANDLER2, 10);
				}
			}
			else
			{
				G_cTxt = std::format(NOTIFYMSG_HP_UP, m_player->m_hp - previous_hp);
				add_event_list(G_cTxt.c_str(), 10);
			}
		}
		m_map_data->shift_map_data(move_dir);
		const char* mapData = reinterpret_cast<const char*>(packet_data) + sizeof(hb::net::PacketResponseMotionMoveConfirm);
		read_map_data(map_x, map_y, mapData);
		m_player->m_Controller.decrement_command_count();
	}
	break;

	case Confirm::MoveReject:
		if (m_player->m_hp <= 0) return;
		{
			const auto* pkt = hb::net::PacketCast<hb::net::PacketResponseMotionMoveReject>(
				packet_data, sizeof(hb::net::PacketResponseMotionMoveReject));
			if (!pkt) return;
			if (m_player->m_player_object_id != pkt->object_id) return;
			m_player->m_player_x = pkt->x;
			m_player->m_player_y = pkt->y;
			m_player->m_player_type = pkt->type;
			m_player->m_player_dir = static_cast<direction>(pkt->dir);
			m_player->m_playerAppearance = pkt->appearance;
			m_player->m_playerStatus = pkt->status;
			m_player->m_is_gm_mode = m_player->m_playerStatus.gm_mode;
		}
		m_player->m_Controller.set_command(Type::stop);
		m_player->m_Controller.set_destination(m_player->m_player_x, m_player->m_player_y);
		m_map_data->set_owner(m_player->m_player_object_id, m_player->m_player_x, m_player->m_player_y, m_player->m_player_type, m_player->m_player_dir,
			m_player->m_playerAppearance, // v1.4
			m_player->m_playerStatus, m_player->m_player_name,
			Type::stop, 0, 0, 0,
			0, 7);
		m_player->m_Controller.reset_command_count();
		on_game()->m_is_get_pointing_mode = false;
		m_Camera.snap_to((m_player->m_player_x - VIEW_CENTER_TILE_X()) * 32 - 16, (m_player->m_player_y - VIEW_CENTER_TILE_Y()) * 32 - 16);
		m_player->m_Controller.set_prev_move_blocked(true);
		switch (m_player->m_player_type) {
		case 1:
		case 2:
		case 3:
			audio_manager::get().play_game_sound(sound_type::character, 12, 0);
			break;
		case 4:
		case 5:
		case 6:
			audio_manager::get().play_game_sound(sound_type::character, 13, 0);
			break;
		}
		break;
	}
}

void CGame::command_processor(short mouse_x, short mouse_y, short tile_x, short tile_y, char left_button, char right_button)
{
	uint32_t current_time = GameClock::get_time_ms();
	uint16_t action_type = 0;
	int result = 0;
	short dialog_x = 0, dialog_y = 0;
	direction move_dir = direction{};

	// Fixed by Snoopy
	if ((on_game()->m_is_observer_commanded == false) && (on_game()->m_is_observer_mode == true))
	{
		if ((mouse_x == 0) && (mouse_y == 0) && (m_Camera.get_destination_x() > 32 * VIEW_TILE_WIDTH()) && (m_Camera.get_destination_y() > 32 * VIEW_TILE_HEIGHT()))
			send_game_packet(hb::net::make_panning_request(8));
		else if ((mouse_x == LOGICAL_MAX_X()) && (mouse_y == 0) && (m_Camera.get_destination_x() < 32 * m_map_data->m_map_size_x - 32 * VIEW_TILE_WIDTH()) && (m_Camera.get_destination_y() > 32 * VIEW_TILE_HEIGHT()))
			send_game_packet(hb::net::make_panning_request(2));
		else if ((mouse_x == LOGICAL_MAX_X()) && (mouse_y == LOGICAL_MAX_Y()) && (m_Camera.get_destination_x() < 32 * m_map_data->m_map_size_x - 32 * VIEW_TILE_WIDTH()) && (m_Camera.get_destination_y() < 32 * m_map_data->m_map_size_y - 32 * VIEW_TILE_HEIGHT()))
			send_game_packet(hb::net::make_panning_request(4));
		else if ((mouse_x == 0) && (mouse_y == LOGICAL_MAX_Y()))
			send_game_packet(hb::net::make_panning_request(6));
		else if ((mouse_x == 0) && (m_Camera.get_destination_x() > 32 * VIEW_TILE_WIDTH()))
			send_game_packet(hb::net::make_panning_request(7));
		else if ((mouse_x == LOGICAL_MAX_X()) && (m_Camera.get_destination_x() < 32 * m_map_data->m_map_size_x - 32 * VIEW_TILE_WIDTH()))
			send_game_packet(hb::net::make_panning_request(3));
		else if ((mouse_y == 0) && (m_Camera.get_destination_y() > 32 * VIEW_TILE_HEIGHT()))
			send_game_packet(hb::net::make_panning_request(1));
		else if ((mouse_y == LOGICAL_MAX_Y()) && (m_Camera.get_destination_y() < 32 * m_map_data->m_map_size_y - 32 * VIEW_TILE_HEIGHT()))
			send_game_packet(hb::net::make_panning_request(5));
		else return;

		on_game()->m_is_observer_commanded = true;
		m_arrow_pressed = 0;
		return;
	}

	if (on_game()->m_is_observer_mode == true) return;

	if (hb::shared::input::is_alt_down()) // [ALT]
		m_player->m_super_attack_mode = true;
	else m_player->m_super_attack_mode = false;

	switch (CursorTarget::GetCursorStatus()) {
	case CursorStatus::Null:
		if (left_button != 0)
		{
			result = get_dialog_box_manager().handle_mouse_down();
			if (result == 1)
			{
				CursorTarget::set_cursor_status(CursorStatus::Selected);
				return;
			}
			else if (result == -1)
			{
				// Scroll/slider claimed - set status to prevent re-processing
				CursorTarget::set_cursor_status(CursorStatus::Selected);
				return;
			}
			else if (result == 0)
			{
				CursorTarget::set_cursor_status(CursorStatus::Pressed);
			}
		}
		else if (right_button != 0)
		{
			if (get_dialog_box_manager().handle_right_click(current_time)) return;
		}
		break;
	case CursorStatus::Pressed:
		if (left_button == 0) // Normal Click
		{
			CursorTarget::set_cursor_status(CursorStatus::Null);
		}
		break;
	case CursorStatus::Selected:
		if (left_button == 0)
		{
			CursorTarget::set_cursor_status(CursorStatus::Null);
			bool double_click_consumed = false;
			//ZeroEoyPnk - Bye delay...
			if (((get_dialog_box_manager().is_enabled(DialogBoxId::LevelUpSetting) != true) || (CursorTarget::get_selected_id() != 12))
				&& ((get_dialog_box_manager().is_enabled(DialogBoxId::ChangeStatsMajestic) != true) || (CursorTarget::get_selected_id() != 42)))
			{
				if (((current_time - CursorTarget::get_selection_click_time()) < input_config::double_click_time_ms) 	// Double Click
					&& (abs(mouse_x - CursorTarget::get_selection_click_x()) <= input_config::double_click_tolerance)
					&& (abs(mouse_y - CursorTarget::get_selection_click_y()) <= input_config::double_click_tolerance))
				{
					CursorTarget::reset_selection_click_time(); // reset to prevent triple-click
					double_click_consumed = get_dialog_box_manager().handle_double_click();
					if (!double_click_consumed)
						get_dialog_box_manager().handle_click();
				}
				else // Click
				{
					get_dialog_box_manager().handle_click();
				}
			}
			else
			{
				get_dialog_box_manager().handle_click();
			}
			CursorTarget::record_selection_click(mouse_x, mouse_y, current_time);
			if (!double_click_consumed && CursorTarget::GetSelectedType() == SelectedObjectType::Item)
			{
				if (!get_dialog_box_manager().handle_dragging_item_release())
				{
					if (auto* on_game = GameModeManager::get_active_screen_as<Screen_OnGame>())
						on_game->item_drop_external_screen(static_cast<char>(CursorTarget::get_selected_id()), mouse_x, mouse_y);
				}
			}
			// Always clear selection after click-release to prevent stale state
			CursorTarget::clear_selection();
			return;
		}
		else 			// v2.05 01-11-30
		{
			if ((m_map_data->is_teleport_loc(m_player->m_player_x, m_player->m_player_y) == true) && (m_player->m_Controller.get_command_count() == 0)) break;

			if ((abs(CursorTarget::get_prev_x() - mouse_x) > input_config::double_click_tolerance) ||
				(abs(CursorTarget::get_prev_y() - mouse_y) > input_config::double_click_tolerance))
			{
				CursorTarget::set_cursor_status(CursorStatus::Dragging);
				CursorTarget::set_prev_position(mouse_x, mouse_y);
				if ((CursorTarget::GetSelectedType() == SelectedObjectType::DialogBox) &&
					(CursorTarget::get_selected_id() == 30))
				{
				}

				// Query Drop Item Amount
				if ((CursorTarget::GetSelectedType() == SelectedObjectType::DialogBox) &&
					(CursorTarget::get_selected_id() == 17) && (get_dialog_box_manager().get_dialog_as<DialogBox_ItemDropAmount>(DialogBoxId::ItemDropExternal)->m_mode == DialogBox_ItemDropAmount::mode::input))
				{
					text_input_manager::get().end_input();
					get_dialog_box_manager().get_dialog_as<DialogBox_ItemDropAmount>(DialogBoxId::ItemDropExternal)->m_mode = DialogBox_ItemDropAmount::mode::selected;
				}
				return;
			}
			if ((m_player->m_Controller.get_command() == Type::Move) || (m_player->m_Controller.get_command() == Type::Run)) { process_motion_commands(action_type); return; }
			return;
		}
		break;
	case CursorStatus::Dragging:
		if (left_button != 0)
		{
			if ((m_map_data->is_teleport_loc(m_player->m_player_x, m_player->m_player_y) == true) && (m_player->m_Controller.get_command_count() == 0)) break;
			if (CursorTarget::GetSelectedType() == SelectedObjectType::DialogBox)
			{
				// HudPanel is fixed and cannot be moved
				if (CursorTarget::get_selected_id() != DialogBoxId::HudPanel)
				{
					auto* drag_dlg = get_dialog_box_manager().get_dialog_box(CursorTarget::get_selected_id());
					if (drag_dlg) {
						drag_dlg->m_x = mouse_x - CursorTarget::get_drag_dist_x();
						drag_dlg->m_y = mouse_y - CursorTarget::get_drag_dist_y();
					}
				}
			}
			CursorTarget::set_prev_position(mouse_x, mouse_y);

			if ((m_player->m_Controller.get_command() == Type::Move) || (m_player->m_Controller.get_command() == Type::Run)) { process_motion_commands(action_type); return; }
			return;
		}
		if (left_button == 0) {
			CursorTarget::set_cursor_status(CursorStatus::Null);
			switch (CursorTarget::GetSelectedType()) {
			case SelectedObjectType::DialogBox:
				if ((CursorTarget::GetSelectedType() == SelectedObjectType::DialogBox) &&
					(CursorTarget::get_selected_id() == 17) && (get_dialog_box_manager().get_dialog_as<DialogBox_ItemDropAmount>(DialogBoxId::ItemDropExternal)->m_mode == DialogBox_ItemDropAmount::mode::selected))
				{	// Query Drop Item Amount
					dialog_x = get_dialog_box_manager().get_dialog_box(DialogBoxId::ItemDropExternal)->m_x;
					dialog_y = get_dialog_box_manager().get_dialog_box(DialogBoxId::ItemDropExternal)->m_y;
					text_input_manager::get().start_input(dialog_x + 40, dialog_y + 57, AmountStringMaxLen, m_amount_string);
					get_dialog_box_manager().get_dialog_as<DialogBox_ItemDropAmount>(DialogBoxId::ItemDropExternal)->m_mode = DialogBox_ItemDropAmount::mode::input;
				}

				if (CursorTarget::get_selected_id() == 9)
				{
					{
						if (mouse_x < LOGICAL_WIDTH() / 2)
						{
							get_dialog_box_manager().get_dialog_box(DialogBoxId::GuideMap)->m_x = 0;
						}
						else
						{
							get_dialog_box_manager().get_dialog_box(DialogBoxId::GuideMap)->m_x = LOGICAL_MAX_X() - get_dialog_box_manager().get_dialog_box(DialogBoxId::GuideMap)->m_size_x;
						}

						if (mouse_y < LOGICAL_HEIGHT() / 2)
						{
							get_dialog_box_manager().get_dialog_box(DialogBoxId::GuideMap)->m_y = 0;
						}
						else
						{
							get_dialog_box_manager().get_dialog_box(DialogBoxId::GuideMap)->m_y = (LOGICAL_HEIGHT() - ICON_PANEL_HEIGHT()) - get_dialog_box_manager().get_dialog_box(DialogBoxId::GuideMap)->m_size_y;
						}
					}
				}

				CursorTarget::clear_selection();
				break;

			case SelectedObjectType::Item:
				if (!get_dialog_box_manager().handle_dragging_item_release())
				{
					if (auto* on_game = GameModeManager::get_active_screen_as<Screen_OnGame>())
						on_game->item_drop_external_screen(static_cast<char>(CursorTarget::get_selected_id()), mouse_x, mouse_y);
				}
				CursorTarget::clear_selection();
				break;

			default:
				CursorTarget::clear_selection();
				break;
			}
			return;
		}
		break;
	}

	// Allow clicks to be responsive even if command not yet available
	if (m_player->m_Controller.is_command_available() == false)
	{
		char cmd = m_player->m_Controller.get_command();
		if (config_manager::get().is_quick_actions_enabled() && (cmd == Type::Move || cmd == Type::Run))
		{
			if (left_button != 0)
			{
				// Click on self while moving = pickup (interrupt movement)
				if (m_mc_name == m_player->m_player_name)
				{
					if ((m_player->m_player_type >= 1) && (m_player->m_player_type <= 6))
					{
						m_player->m_Controller.set_command(Type::GetItem);
						m_player->m_Controller.set_destination(m_player->m_player_x, m_player->m_player_y);
						return;
					}
				}
				// Left click while moving: update destination immediately
				m_player->m_Controller.set_destination(tile_x, tile_y);
			}
			else if (right_button != 0)
			{
				// Right click on self while moving = pickup (interrupt movement)
				if (m_mc_name == m_player->m_player_name)
				{
					if ((m_player->m_player_type >= 1) && (m_player->m_player_type <= 6))
					{
						m_player->m_Controller.set_command(Type::GetItem);
						m_player->m_Controller.set_destination(m_player->m_player_x, m_player->m_player_y);
						return;
					}
				}
				// Right click while moving: stop after current step and face click direction
				m_player->m_Controller.set_destination(m_player->m_player_x, m_player->m_player_y);
				// save pending direction to apply when movement stops
				direction pending_dir = CMisc::get_next_move_dir(m_player->m_player_x, m_player->m_player_y, tile_x, tile_y);
				if (pending_dir != 0) m_player->m_Controller.set_pending_stop_dir(pending_dir);
			}
		}
		else if (config_manager::get().is_quick_actions_enabled() && right_button != 0 && cmd == Type::stop && !on_game()->m_is_get_pointing_mode)
		{
			// Right click while stopped (and not casting): process turn immediately
			// But don't interrupt attack/magic animations — the controller command is STOP
			// after dispatch, but the tile animation may still be playing
			int xc = m_player->m_player_x - m_map_data->m_pivot_x;
			int yc = m_player->m_player_y - m_map_data->m_pivot_y;
			if (xc >= 0 && xc < MapDataSizeX && yc >= 0 && yc < MapDataSizeY) {
				int8_t animAction = m_map_data->m_data[xc][yc].m_animation.m_action;
				if (animAction == Type::Attack || animAction == Type::AttackMove || animAction == Type::Magic
					|| animAction == Type::GetItem || animAction == Type::Damage || animAction == Type::DamageMove)
					return;
			}
			move_dir = CMisc::get_next_move_dir(m_player->m_player_x, m_player->m_player_y, tile_x, tile_y);
			if (move_dir != 0 && m_player->m_player_dir != move_dir)
			{
				m_player->m_player_dir = move_dir;
				send_game_packet(hb::net::make_motion(Type::stop, m_player->m_player_x, m_player->m_player_y, move_dir));
				m_player->m_Controller.increment_command_count();
				m_map_data->set_owner(m_player->m_player_object_id, m_player->m_player_x, m_player->m_player_y,
					m_player->m_player_type, move_dir, m_player->m_playerAppearance,
					m_player->m_playerStatus, m_player->m_player_name, Type::stop, 0, 0, 0, 0, 10);
				m_player->m_Controller.set_command_time(current_time);
			}
		}
		return;
	}
	if ((current_time - m_player->m_Controller.get_command_time()) < 300)
	{
		m_g_sock.reset();
		audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
		audio_manager::get().stop_sound(sound_type::effect, 38);
		audio_manager::get().stop_music();
		change_game_mode(GameMode::MainMenu);
		return;
	}
	if (m_player->m_hp <= 0) return;

	// Block all movement and input while a teleport is in progress.
	// Without this, the player can walk off the teleport tile during the
	// auth wait or fade, causing the server-side search_teleport_dest to fail.
	if (teleport_manager::get().is_active())
		return;

	if (m_player->m_damage_move != 0)
	{
		m_player->m_Controller.set_command(Type::DamageMove);
		process_motion_commands(action_type); return;
	}

	if ((m_map_data->is_teleport_loc(m_player->m_player_x, m_player->m_player_y) == true) && (m_player->m_Controller.get_command_count() == 0)
		&& !teleport_manager::get().is_rejected_tile(m_player->m_player_x, m_player->m_player_y))
		request_teleport_and_wait_data();

	// indexX, indexY

	if (left_button != 0)
	{
		if (process_left_click(mouse_x, mouse_y, tile_x, tile_y, current_time, action_type)) return;
	}
	else if (right_button != 0)
	{
		if (process_right_click(mouse_x, mouse_y, tile_x, tile_y, current_time, action_type)) return;
	}

	process_motion_commands(action_type);
}

bool CGame::process_left_click(short mouse_x, short mouse_y, short tile_x, short tile_y, uint32_t current_time, uint16_t& action_type)
{
	std::string name;
	short object_type = 0, dialog_x = 0, dialog_y = 0;
	hb::shared::entity::PlayerStatus object_status;
	char abs_x = 0, abs_y = 0;

	if (on_game()->m_is_get_pointing_mode == true)
	{
		if ((m_mcx != 0) || (m_mcy != 0))
			point_command_handler(m_mcx, m_mcy);
		else point_command_handler(tile_x, tile_y);

		m_player->m_Controller.set_command_available(false);
		m_player->m_Controller.set_command_time(GameClock::get_time_ms());
		on_game()->m_is_get_pointing_mode = false;
		on_game()->m_magic_cast_time = current_time;  // Track when magic was cast
		return true;
	}

	// Delay after magic cast before allowing held-click actions
	if (on_game()->m_magic_cast_time > 0 && (current_time - on_game()->m_magic_cast_time) < 750) return true;

	m_map_data->get_owner(m_mcx, m_mcy - 1, name, &object_type, &object_status, &m_comm_object_id); // v1.4
	if (m_mc_name == m_player->m_player_name && (object_type <= 6 || (m_map_data->m_data[m_player->m_player_x - m_map_data->m_pivot_x][m_player->m_player_y - m_map_data->m_pivot_y].m_item_id != 0 && m_item_config_list[m_map_data->m_data[m_player->m_player_x - m_map_data->m_pivot_x][m_player->m_player_y - m_map_data->m_pivot_y].m_item_id] != nullptr)))
	{
		if ((m_player->m_player_type >= 1) && (m_player->m_player_type <= 6)/* && (!m_player->m_playerAppearance.is_walking)*/)
		{
			m_player->m_Controller.set_command(Type::GetItem);
			m_player->m_Controller.set_destination(m_player->m_player_x, m_player->m_player_y);
		}
	}
	else
	{
		if (m_mc_name == m_player->m_player_name) m_mcy -= 1;
		if ((m_mcx != 0) && (m_mcy != 0)) // m_mcx, m_mcy
		{
			if (hb::shared::input::is_ctrl_down() == true)
			{
				m_map_data->get_owner(m_mcx, m_mcy, name, &object_type, &object_status, &m_comm_object_id);
				if (object_status.invisibility) return true;
				if ((object_type == 15) || (object_type == 20) || (object_type == 24)) return true;
				abs_x = abs(m_player->m_player_x - m_mcx);
				abs_y = abs(m_player->m_player_y - m_mcy);
				if ((abs_x <= 1) && (abs_y <= 1))
				{
					action_type = combat_system::get().get_attack_type();
					m_player->m_Controller.set_command(Type::Attack);
					m_player->m_Controller.set_destination(m_mcx, m_mcy);
				}
				else if ((abs_x <= 2) && (abs_y <= 2) // strike on Big mobs & gate from a range
					&& ((object_type == 66) || (object_type == 73) || (object_type == 81) || (object_type == 91)))
				{
					action_type = combat_system::get().get_attack_type();
					m_player->m_Controller.set_command(Type::Attack);
					m_player->m_Controller.set_destination(m_mcx, m_mcy);
				}
				else // Pas au corp � corp
				{
					switch (combat_system::get().get_weapon_skill_type()) {
					case 6: // Bow
						m_player->m_Controller.set_command(Type::Attack);
						m_player->m_Controller.set_destination(m_mcx, m_mcy);
						action_type = combat_system::get().get_attack_type();
						break;

					case 5: // OpenHand
					case 7: // SS
						if (((abs_x == 2) && (abs_y == 2)) || ((abs_x == 0) && (abs_y == 2)) || ((abs_x == 2) && (abs_y == 0)))
						{
							if ((hb::shared::input::is_shift_down() || config_manager::get().is_running_mode_enabled()) && (m_player->m_sp > 0))
							{
								if (m_player->m_skill_mastery[combat_system::get().get_weapon_skill_type()] == 100)
								{
									m_player->m_Controller.set_command(Type::AttackMove);
									action_type = combat_system::get().get_attack_type();
								}
								else
								{
									m_player->m_Controller.set_command(Type::Run);
									m_player->m_Controller.calculate_player_turn(m_player->m_player_x, m_player->m_player_y, m_map_data.get());
								}
								m_player->m_Controller.set_destination(m_mcx, m_mcy);
							}
							else
							{
								m_player->m_Controller.set_command(Type::Move);
								m_player->m_Controller.set_destination(m_mcx, m_mcy);
								m_player->m_Controller.calculate_player_turn(m_player->m_player_x, m_player->m_player_y, m_map_data.get());
							}
						}
						else
						{
							if ((hb::shared::input::is_shift_down() || config_manager::get().is_running_mode_enabled()) && (m_player->m_sp > 0)
								&& (m_player->m_player_type >= 1) && (m_player->m_player_type <= 6))
								m_player->m_Controller.set_command(Type::Run);	// Staminar
							else m_player->m_Controller.set_command(Type::Move);
							m_player->m_Controller.set_destination(m_mcx, m_mcy);
							m_player->m_Controller.calculate_player_turn(m_player->m_player_x, m_player->m_player_y, m_map_data.get());
						}
						break;

					case 8: // LS
						if ((abs_x <= 3) && (abs_y <= 3) && combat_system::get().can_super_attack()
							&& (combat_system::get().get_attack_type() != 30)) // Crit without StormBlade
						{
							action_type = combat_system::get().get_attack_type();
							m_player->m_Controller.set_command(Type::Attack);
							m_player->m_Controller.set_destination(m_mcx, m_mcy);
						}
						else if ((abs_x <= 5) && (abs_y <= 5) && combat_system::get().can_super_attack()
							&& (combat_system::get().get_attack_type() == 30))  // Crit with StormBlade (by Snoopy)
						{
							action_type = combat_system::get().get_attack_type();
							m_player->m_Controller.set_command(Type::Attack);
							m_player->m_Controller.set_destination(m_mcx, m_mcy);
						}
						else if ((abs_x <= 3) && (abs_y <= 3)
							&& (combat_system::get().get_attack_type() == 5))  // Normal hit with StormBlade (by Snoopy)
						{
							action_type = combat_system::get().get_attack_type();
							m_player->m_Controller.set_command(Type::Attack);
							m_player->m_Controller.set_destination(m_mcx, m_mcy);
						}
						else // Swing
						{
							if (((abs_x == 2) && (abs_y == 2)) || ((abs_x == 0) && (abs_y == 2)) || ((abs_x == 2) && (abs_y == 0))
								&& (combat_system::get().get_attack_type() != 5)) // no Dash possible with StormBlade
							{
								if ((hb::shared::input::is_shift_down() || config_manager::get().is_running_mode_enabled()) && (m_player->m_sp > 0))
								{
									if (m_player->m_skill_mastery[combat_system::get().get_weapon_skill_type()] == 100)
									{
										m_player->m_Controller.set_command(Type::AttackMove);
										action_type = combat_system::get().get_attack_type();
									}
									else
									{
										m_player->m_Controller.set_command(Type::Run);
										m_player->m_Controller.calculate_player_turn(m_player->m_player_x, m_player->m_player_y, m_map_data.get());
									}
									m_player->m_Controller.set_destination(m_mcx, m_mcy);
								}
								else
								{
									m_player->m_Controller.set_command(Type::Move);
									m_player->m_Controller.set_destination(m_mcx, m_mcy);
									m_player->m_Controller.calculate_player_turn(m_player->m_player_x, m_player->m_player_y, m_map_data.get());
								}
							}
							else
							{
								if ((hb::shared::input::is_shift_down() || config_manager::get().is_running_mode_enabled()) && (m_player->m_sp > 0)
									&& (m_player->m_player_type >= 1) && (m_player->m_player_type <= 6))
									m_player->m_Controller.set_command(Type::Run);
								else m_player->m_Controller.set_command(Type::Move);
								m_player->m_Controller.set_destination(m_mcx, m_mcy);
								m_player->m_Controller.calculate_player_turn(m_player->m_player_x, m_player->m_player_y, m_map_data.get());
							}
						}
						break;

					case 9: // Fencing
						if ((abs_x <= 4) && (abs_y <= 4) && combat_system::get().can_super_attack())
						{
							m_player->m_Controller.set_command(Type::Attack);
							m_player->m_Controller.set_destination(m_mcx, m_mcy);
							action_type = combat_system::get().get_attack_type();
						}
						else {
							if (((abs_x == 2) && (abs_y == 2)) || ((abs_x == 0) && (abs_y == 2)) || ((abs_x == 2) && (abs_y == 0))) {
								if ((hb::shared::input::is_shift_down() || config_manager::get().is_running_mode_enabled()) && (m_player->m_sp > 0)) {
									if (m_player->m_skill_mastery[combat_system::get().get_weapon_skill_type()] == 100) {
										m_player->m_Controller.set_command(Type::AttackMove);
										action_type = combat_system::get().get_attack_type();
									}
									else {
										m_player->m_Controller.set_command(Type::Run);
										m_player->m_Controller.calculate_player_turn(m_player->m_player_x, m_player->m_player_y, m_map_data.get());
									}
									m_player->m_Controller.set_destination(m_mcx, m_mcy);
								}
								else {
									m_player->m_Controller.set_command(Type::Move);
									m_player->m_Controller.set_destination(m_mcx, m_mcy);
									m_player->m_Controller.calculate_player_turn(m_player->m_player_x, m_player->m_player_y, m_map_data.get());
								}
							}
							else {
								if ((hb::shared::input::is_shift_down() || config_manager::get().is_running_mode_enabled()) && (m_player->m_sp > 0) &&
									(m_player->m_player_type >= 1) && (m_player->m_player_type <= 6))
									m_player->m_Controller.set_command(Type::Run);
								else m_player->m_Controller.set_command(Type::Move);
								m_player->m_Controller.set_destination(m_mcx, m_mcy);
								m_player->m_Controller.calculate_player_turn(m_player->m_player_x, m_player->m_player_y, m_map_data.get());
							}
						}
						break;

					case hb::shared::owner::Slime: // Axe
						if ((abs_x <= 2) && (abs_y <= 2) && combat_system::get().can_super_attack())
						{
							m_player->m_Controller.set_command(Type::Attack);
							m_player->m_Controller.set_destination(m_mcx, m_mcy);
							action_type = combat_system::get().get_attack_type();
						}
						else
						{
							if (((abs_x == 2) && (abs_y == 2)) || ((abs_x == 0) && (abs_y == 2)) || ((abs_x == 2) && (abs_y == 0)))
							{
								if ((hb::shared::input::is_shift_down() || config_manager::get().is_running_mode_enabled()) && (m_player->m_sp > 0))
								{
									if (m_player->m_skill_mastery[combat_system::get().get_weapon_skill_type()] == 100)
									{
										m_player->m_Controller.set_command(Type::AttackMove);
										action_type = combat_system::get().get_attack_type();
									}
									else
									{
										m_player->m_Controller.set_command(Type::Run);
										m_player->m_Controller.calculate_player_turn(m_player->m_player_x, m_player->m_player_y, m_map_data.get());
									}
									m_player->m_Controller.set_destination(m_mcx, m_mcy);
								}
								else
								{
									m_player->m_Controller.set_command(Type::Move);
									m_player->m_Controller.set_destination(m_mcx, m_mcy);
									m_player->m_Controller.calculate_player_turn(m_player->m_player_x, m_player->m_player_y, m_map_data.get());
								}
							}
							else
							{
								if ((hb::shared::input::is_shift_down() || config_manager::get().is_running_mode_enabled()) && (m_player->m_sp > 0) &&
									(m_player->m_player_type >= 1) && (m_player->m_player_type <= 6))
									m_player->m_Controller.set_command(Type::Run);
								else m_player->m_Controller.set_command(Type::Move);
								m_player->m_Controller.set_destination(m_mcx, m_mcy);
								m_player->m_Controller.calculate_player_turn(m_player->m_player_x, m_player->m_player_y, m_map_data.get());
							}
						}
						break;
					case hb::shared::owner::OrcMage: // Hammer
						if ((abs_x <= 2) && (abs_y <= 2) && combat_system::get().can_super_attack()) {
							m_player->m_Controller.set_command(Type::Attack);
							m_player->m_Controller.set_destination(m_mcx, m_mcy);
							action_type = combat_system::get().get_attack_type();
						}
						else {
							if (((abs_x == 2) && (abs_y == 2)) || ((abs_x == 0) && (abs_y == 2)) || ((abs_x == 2) && (abs_y == 0))) {
								if ((hb::shared::input::is_shift_down() || config_manager::get().is_running_mode_enabled()) && (m_player->m_sp > 0)) {
									if (m_player->m_skill_mastery[combat_system::get().get_weapon_skill_type()] == 100) {
										m_player->m_Controller.set_command(Type::AttackMove);
										action_type = combat_system::get().get_attack_type();
									}
									else {
										m_player->m_Controller.set_command(Type::Run);
										m_player->m_Controller.calculate_player_turn(m_player->m_player_x, m_player->m_player_y, m_map_data.get());
									}
									m_player->m_Controller.set_destination(m_mcx, m_mcy);
								}
								else {
									m_player->m_Controller.set_command(Type::Move);
									m_player->m_Controller.set_destination(m_mcx, m_mcy);
									m_player->m_Controller.calculate_player_turn(m_player->m_player_x, m_player->m_player_y, m_map_data.get());
								}
							}
							else {
								if ((hb::shared::input::is_shift_down() || config_manager::get().is_running_mode_enabled()) && (m_player->m_sp > 0) &&
									(m_player->m_player_type >= 1) && (m_player->m_player_type <= 6))
									m_player->m_Controller.set_command(Type::Run);
								else m_player->m_Controller.set_command(Type::Move);
								m_player->m_Controller.set_destination(m_mcx, m_mcy);
								m_player->m_Controller.calculate_player_turn(m_player->m_player_x, m_player->m_player_y, m_map_data.get());
							}
						}
						break;
					case hb::shared::owner::Guard: // Wand
						if ((abs_x <= 2) && (abs_y <= 2) && combat_system::get().can_super_attack()) {
							m_player->m_Controller.set_command(Type::Attack);
							m_player->m_Controller.set_destination(m_mcx, m_mcy);
							action_type = combat_system::get().get_attack_type();
						}
						else {
							if (((abs_x == 2) && (abs_y == 2)) || ((abs_x == 0) && (abs_y == 2)) || ((abs_x == 2) && (abs_y == 0))) {
								if ((hb::shared::input::is_shift_down() || config_manager::get().is_running_mode_enabled()) && (m_player->m_sp > 0)) {
									if (m_player->m_skill_mastery[combat_system::get().get_weapon_skill_type()] == 100) {
										m_player->m_Controller.set_command(Type::AttackMove);
										action_type = combat_system::get().get_attack_type();
									}
									else {
										m_player->m_Controller.set_command(Type::Run);
										m_player->m_Controller.calculate_player_turn(m_player->m_player_x, m_player->m_player_y, m_map_data.get());
									}
									m_player->m_Controller.set_destination(m_mcx, m_mcy);
								}
								else {
									m_player->m_Controller.set_command(Type::Move);
									m_player->m_Controller.set_destination(m_mcx, m_mcy);
									m_player->m_Controller.calculate_player_turn(m_player->m_player_x, m_player->m_player_y, m_map_data.get());
								}
							}
							else {
								if ((hb::shared::input::is_shift_down() || config_manager::get().is_running_mode_enabled()) && (m_player->m_sp > 0) &&
									(m_player->m_player_type >= 1) && (m_player->m_player_type <= 6))
									m_player->m_Controller.set_command(Type::Run);
								else m_player->m_Controller.set_command(Type::Move);
								m_player->m_Controller.set_destination(m_mcx, m_mcy);
								m_player->m_Controller.calculate_player_turn(m_player->m_player_x, m_player->m_player_y, m_map_data.get());
							}
						}
						break;
					}
				}
			}
			else // CTRL not pressed
			{
				short npc_config_id = -1;
				m_map_data->get_owner(m_mcx, m_mcy, name, &object_type, &object_status, &m_comm_object_id, &npc_config_id);
				if (object_type >= 10 || ((object_type >= 1) && (object_type <= 6)))
				{
					switch (object_type) { 	// CLEROTH - NPC TALK
					case hb::shared::owner::ShopKeeper: // ShopKeeper-W�
					{
						auto* npcDlg = get_dialog_box_manager().get_dialog_as<DialogBox_NpcActionQuery>(DialogBoxId::NpcActionQuery);
						npcDlg->enable_with_target(5, 11, npc_config_id, 15, 0, 0, 0, get_npc_config_name_by_id(npc_config_id));
						dialog_x = mouse_x - 117;
						dialog_y = mouse_y - 50;
						if (dialog_x < 0) dialog_x = 0;
						if ((dialog_x + 235) > LOGICAL_MAX_X()) dialog_x = LOGICAL_MAX_X() - 235;
						if (dialog_y < 0) dialog_y = 0;
						if ((dialog_y + 100) > LOGICAL_MAX_Y()) dialog_y = LOGICAL_MAX_Y() - 100;
						npcDlg->m_x = dialog_x;
						npcDlg->m_y = dialog_y;
					}	break;

					case hb::shared::owner::Gandalf: // Gandlf
					{
						auto* npcDlg = get_dialog_box_manager().get_dialog_as<DialogBox_NpcActionQuery>(DialogBoxId::NpcActionQuery);
						npcDlg->enable_with_target(0, 16, 0, 19, 0, 0, 0, get_npc_config_name_by_id(npc_config_id));
						dialog_x = mouse_x - 117;
						dialog_y = mouse_y - 50;
						if (dialog_x < 0) dialog_x = 0;
						if ((dialog_x + 235) > LOGICAL_MAX_X()) dialog_x = LOGICAL_MAX_X() - 235;
						if (dialog_y < 0) dialog_y = 0;
						if ((dialog_y + 100) > LOGICAL_MAX_Y()) dialog_y = LOGICAL_MAX_Y() - 100;
						npcDlg->m_x = dialog_x;
						npcDlg->m_y = dialog_y;
					}	break;

					case hb::shared::owner::Howard: // Howard
					{
						auto* npcDlg = get_dialog_box_manager().get_dialog_as<DialogBox_NpcActionQuery>(DialogBoxId::NpcActionQuery);
						npcDlg->enable_with_target(0, 14, 0, 20, 0, 0, 0, get_npc_config_name_by_id(npc_config_id));
						dialog_x = mouse_x - 117;
						dialog_y = mouse_y - 50;
						if (dialog_x < 0) dialog_x = 0;
						if ((dialog_x + 235) > LOGICAL_MAX_X()) dialog_x = LOGICAL_MAX_X() - 235;
						if (dialog_y < 0) dialog_y = 0;
						if ((dialog_y + 100) > LOGICAL_MAX_Y()) dialog_y = LOGICAL_MAX_Y() - 100;
						npcDlg->m_x = dialog_x;
						npcDlg->m_y = dialog_y;
						get_dialog_box_manager().m_give_item.action_type = 20;
						get_dialog_box_manager().m_give_item.object_id = m_comm_object_id;
						get_dialog_box_manager().m_give_item.target_x = m_mcx;
						get_dialog_box_manager().m_give_item.target_y = m_mcy;
					}	break;

					case hb::shared::owner::Tom: // Tom
					{
						auto* npcDlg = get_dialog_box_manager().get_dialog_as<DialogBox_NpcActionQuery>(DialogBoxId::NpcActionQuery);
						npcDlg->enable_with_target(5, 11, npc_config_id, 24, 0, 0, 0, get_npc_config_name_by_id(npc_config_id));
						dialog_x = mouse_x - 117;
						dialog_y = mouse_y - 50;
						if (dialog_x < 0) dialog_x = 0;
						if ((dialog_x + 235) > LOGICAL_MAX_X()) dialog_x = LOGICAL_MAX_X() - 235;
						if (dialog_y < 0) dialog_y = 0;
						if ((dialog_y + 100) > LOGICAL_MAX_Y()) dialog_y = LOGICAL_MAX_Y() - 100;
						npcDlg->m_x = dialog_x;
						npcDlg->m_y = dialog_y;
						get_dialog_box_manager().m_give_item.action_type = 24;
						get_dialog_box_manager().m_give_item.object_id = m_comm_object_id;
						get_dialog_box_manager().m_give_item.target_x = m_mcx;
						get_dialog_box_manager().m_give_item.target_y = m_mcy;
					}	break;

					case hb::shared::owner::William: // William
					{
						auto* npcDlg = get_dialog_box_manager().get_dialog_as<DialogBox_NpcActionQuery>(DialogBoxId::NpcActionQuery);
						npcDlg->enable_with_target(0, 13, 0, 25, 0, 0, 0, get_npc_config_name_by_id(npc_config_id));
						dialog_x = mouse_x - 117;
						dialog_y = mouse_y - 50;
						if (dialog_x < 0) dialog_x = 0;
						if ((dialog_x + 235) > LOGICAL_MAX_X()) dialog_x = LOGICAL_MAX_X() - 235;
						if (dialog_y < 0) dialog_y = 0;
						if ((dialog_y + 100) > LOGICAL_MAX_Y()) dialog_y = LOGICAL_MAX_Y() - 100;
						npcDlg->m_x = dialog_x;
						npcDlg->m_y = dialog_y;
					}	break;

					case hb::shared::owner::Kennedy: // Kennedy
					{
						auto* npcDlg = get_dialog_box_manager().get_dialog_as<DialogBox_NpcActionQuery>(DialogBoxId::NpcActionQuery);
						npcDlg->enable_with_target(0, 7, 0, 26, 0, 0, 0, get_npc_config_name_by_id(npc_config_id));
						dialog_x = mouse_x - 117;
						dialog_y = mouse_y - 50;
						if (dialog_x < 0) dialog_x = 0;
						if ((dialog_x + 235) > LOGICAL_MAX_X()) dialog_x = LOGICAL_MAX_X() - 235;
						if (dialog_y < 0) dialog_y = 0;
						if ((dialog_y + 100) > LOGICAL_MAX_Y()) dialog_y = LOGICAL_MAX_Y() - 100;
						npcDlg->m_x = dialog_x;
						npcDlg->m_y = dialog_y;
					}	break;

					case hb::shared::owner::Guard: // Guard
						if (!IsHostile(object_status.relationship) && (!m_player->m_is_combat_mode))
						{
							auto* npcDlg = get_dialog_box_manager().get_dialog_as<DialogBox_NpcActionQuery>(DialogBoxId::NpcActionQuery);
							npcDlg->enable_with_target(4, 0, 0, 21, 0, 0, 0, get_npc_config_name_by_id(npc_config_id));
							dialog_x = mouse_x - 117;
							dialog_y = mouse_y - 50;
							if (dialog_x < 0) dialog_x = 0;
							if ((dialog_x + 235) > LOGICAL_MAX_X()) dialog_x = LOGICAL_MAX_X() - 235;
							if (dialog_y < 0) dialog_y = 0;
							if ((dialog_y + 100) > LOGICAL_MAX_Y()) dialog_y = LOGICAL_MAX_Y() - 100;
							npcDlg->m_x = dialog_x;
							npcDlg->m_y = dialog_y;
						}
						break;
					case hb::shared::owner::McGaffin: // McGaffin
					case hb::shared::owner::Perry: // Perry
					case hb::shared::owner::Devlin: // Devlin
						if (!m_player->m_is_combat_mode)
						{
							auto* npcDlg = get_dialog_box_manager().get_dialog_as<DialogBox_NpcActionQuery>(DialogBoxId::NpcActionQuery);
							npcDlg->enable_with_target(4, 0, 0, object_type, 0, 0, 0, get_npc_config_name_by_id(npc_config_id));
							dialog_x = mouse_x - 117;
							dialog_y = mouse_y - 50;
							if (dialog_x < 0) dialog_x = 0;
							if ((dialog_x + 235) > LOGICAL_MAX_X()) dialog_x = LOGICAL_MAX_X() - 235;
							if (dialog_y < 0) dialog_y = 0;
							if ((dialog_y + 100) > LOGICAL_MAX_Y()) dialog_y = LOGICAL_MAX_Y() - 100;
							npcDlg->m_x = dialog_x;
							npcDlg->m_y = dialog_y;
						}
						break;

					case hb::shared::owner::Unicorn: // Unicorn
						if (!m_player->m_is_combat_mode)
						{
							auto* npcDlg = get_dialog_box_manager().get_dialog_as<DialogBox_NpcActionQuery>(DialogBoxId::NpcActionQuery);
							npcDlg->enable_with_target(4, 0, 0, 32, 0, 0, 0, get_npc_config_name_by_id(npc_config_id));
							dialog_x = mouse_x - 117;
							dialog_y = mouse_y - 50;
							if (dialog_x < 0) dialog_x = 0;
							if ((dialog_x + 235) > LOGICAL_MAX_X()) dialog_x = LOGICAL_MAX_X() - 235;
							if (dialog_y < 0) dialog_y = 0;
							if ((dialog_y + 100) > LOGICAL_MAX_Y()) dialog_y = LOGICAL_MAX_Y() - 100;
							npcDlg->m_x = dialog_x;
							npcDlg->m_y = dialog_y;
						}
						break;

					case hb::shared::owner::Gail: // Snoopy: Gail
					{
						auto* npcDlg = get_dialog_box_manager().get_dialog_as<DialogBox_NpcActionQuery>(DialogBoxId::NpcActionQuery);
						npcDlg->enable_with_target(6, 0, 0, 90, 0, 0, 0, get_npc_config_name_by_id(npc_config_id));
						dialog_x = mouse_x - 117;
						dialog_y = mouse_y - 50;
						if (dialog_x < 0) dialog_x = 0;
						if ((dialog_x + 235) > LOGICAL_MAX_X()) dialog_x = LOGICAL_MAX_X() - 235;
						if (dialog_y < 0) dialog_y = 0;
						if ((dialog_y + 100) > LOGICAL_MAX_Y()) dialog_y = LOGICAL_MAX_Y() - 100;
						npcDlg->m_x = dialog_x;
						npcDlg->m_y = dialog_y;
					}	break;

					default: // Other mobs
						if (!IsHostile(object_status.relationship)) break;
						if ((object_type >= 1) && (object_type <= 6) && (m_player->m_force_attack == false)) break;
						abs_x = abs(m_player->m_player_x - m_mcx);
						abs_y = abs(m_player->m_player_y - m_mcy);
						if ((abs_x <= 1) && (abs_y <= 1))
						{
							action_type = combat_system::get().get_attack_type();
							m_player->m_Controller.set_command(Type::Attack);
							m_player->m_Controller.set_destination(m_mcx, m_mcy);
						}
						else if ((abs_x <= 2) && (abs_y <= 2) // strike on Big mobs & gate from a range
							&& ((object_type == 66) || (object_type == 73) || (object_type == 81) || (object_type == 91)))
						{
							action_type = combat_system::get().get_attack_type();
							m_player->m_Controller.set_command(Type::Attack);
							m_player->m_Controller.set_destination(m_mcx, m_mcy);
						}
						else // Normal hit from a range.
						{
							switch (combat_system::get().get_weapon_skill_type()) {
							case 6: // Bow
								m_player->m_Controller.set_command(Type::Attack);
								m_player->m_Controller.set_destination(m_mcx, m_mcy);
								action_type = combat_system::get().get_attack_type();
								break;

							case 5: // Boxe
							case 7: // SS
								if ((hb::shared::input::is_shift_down() || config_manager::get().is_running_mode_enabled()) && (m_player->m_sp > 0)
									&& (m_player->m_player_type >= 1) && (m_player->m_player_type <= 6))
									m_player->m_Controller.set_command(Type::Run);
								else m_player->m_Controller.set_command(Type::Move);
								m_player->m_Controller.set_destination(m_mcx, m_mcy);
								m_player->m_Controller.calculate_player_turn(m_player->m_player_x, m_player->m_player_y, m_map_data.get());
								break;

							case 8: // LS
								if ((abs_x <= 3) && (abs_y <= 3) && combat_system::get().can_super_attack()
									&& (combat_system::get().get_attack_type() != 30)) // Crit without StormBlade by Snoopy
								{
									if ((abs_x <= 1) && (abs_y <= 1) && (hb::shared::input::is_shift_down() || config_manager::get().is_running_mode_enabled()) && (m_player->m_sp > 0))
										m_player->m_Controller.set_command(Type::AttackMove);
									else m_player->m_Controller.set_command(Type::Attack);
									m_player->m_Controller.set_destination(m_mcx, m_mcy);
									action_type = combat_system::get().get_attack_type();
								}
								else if ((abs_x <= 5) && (abs_y <= 5) && combat_system::get().can_super_attack()
									&& (combat_system::get().get_attack_type() == 30)) // Crit with StormBlade by Snoopy
								{
									if ((abs_x <= 1) && (abs_y <= 1) && (hb::shared::input::is_shift_down() || config_manager::get().is_running_mode_enabled()) && (m_player->m_sp > 0))
										m_player->m_Controller.set_command(Type::AttackMove);
									else m_player->m_Controller.set_command(Type::Attack);
									m_player->m_Controller.set_destination(m_mcx, m_mcy);
									action_type = combat_system::get().get_attack_type();
								}
								else if ((abs_x <= 3) && (abs_y <= 3)
									&& (combat_system::get().get_attack_type() == 5)) // Normal hit with StormBlade by Snoopy
								{
									m_player->m_Controller.set_command(Type::Attack);
									m_player->m_Controller.set_destination(m_mcx, m_mcy);
									action_type = combat_system::get().get_attack_type();
								}
								else
								{
									if ((hb::shared::input::is_shift_down() || config_manager::get().is_running_mode_enabled()) && (m_player->m_sp > 0) &&
										(m_player->m_player_type >= 1) && (m_player->m_player_type <= 6))
										m_player->m_Controller.set_command(Type::Run);
									else m_player->m_Controller.set_command(Type::Move);
									m_player->m_Controller.set_destination(m_mcx, m_mcy);
									m_player->m_Controller.calculate_player_turn(m_player->m_player_x, m_player->m_player_y, m_map_data.get());
								}
								break;

							case 9: // Fencing
								if ((abs_x <= 4) && (abs_y <= 4) && combat_system::get().can_super_attack())
								{
									if ((abs_x <= 1) && (abs_y <= 1) && (hb::shared::input::is_shift_down() || config_manager::get().is_running_mode_enabled()) && (m_player->m_sp > 0))
										m_player->m_Controller.set_command(Type::AttackMove);
									else m_player->m_Controller.set_command(Type::Attack);
									m_player->m_Controller.set_destination(m_mcx, m_mcy);
									action_type = combat_system::get().get_attack_type();
								}
								else
								{
									if ((hb::shared::input::is_shift_down() || config_manager::get().is_running_mode_enabled()) && (m_player->m_sp > 0) &&
										(m_player->m_player_type >= 1) && (m_player->m_player_type <= 6))
										m_player->m_Controller.set_command(Type::Run);
									else m_player->m_Controller.set_command(Type::Move);
									m_player->m_Controller.set_destination(m_mcx, m_mcy);
									m_player->m_Controller.calculate_player_turn(m_player->m_player_x, m_player->m_player_y, m_map_data.get());
								}
								break;

							case hb::shared::owner::Slime: //
								if ((abs_x <= 2) && (abs_y <= 2) && combat_system::get().can_super_attack()) {
									if ((abs_x <= 1) && (abs_y <= 1) && (hb::shared::input::is_shift_down() || config_manager::get().is_running_mode_enabled()) && (m_player->m_sp > 0))
										m_player->m_Controller.set_command(Type::AttackMove);
									else m_player->m_Controller.set_command(Type::Attack);
									m_player->m_Controller.set_destination(m_mcx, m_mcy);
									action_type = combat_system::get().get_attack_type();
								}
								else {
									if ((hb::shared::input::is_shift_down() || config_manager::get().is_running_mode_enabled()) && (m_player->m_sp > 0) &&
										(m_player->m_player_type >= 1) && (m_player->m_player_type <= 6))
										m_player->m_Controller.set_command(Type::Run);
									else m_player->m_Controller.set_command(Type::Move);
									m_player->m_Controller.set_destination(m_mcx, m_mcy);
									m_player->m_Controller.calculate_player_turn(m_player->m_player_x, m_player->m_player_y, m_map_data.get());
								}
								break;
							case hb::shared::owner::OrcMage: //
								if ((abs_x <= 2) && (abs_y <= 2) && combat_system::get().can_super_attack()) {
									if ((abs_x <= 1) && (abs_y <= 1) && (hb::shared::input::is_shift_down() || config_manager::get().is_running_mode_enabled()) && (m_player->m_sp > 0))
										m_player->m_Controller.set_command(Type::AttackMove);
									else m_player->m_Controller.set_command(Type::Attack);
									m_player->m_Controller.set_destination(m_mcx, m_mcy);
									action_type = combat_system::get().get_attack_type();
								}
								else {
									if ((hb::shared::input::is_shift_down() || config_manager::get().is_running_mode_enabled()) && (m_player->m_sp > 0) &&
										(m_player->m_player_type >= 1) && (m_player->m_player_type <= 6))
										m_player->m_Controller.set_command(Type::Run);
									else m_player->m_Controller.set_command(Type::Move);
									m_player->m_Controller.set_destination(m_mcx, m_mcy);
									m_player->m_Controller.calculate_player_turn(m_player->m_player_x, m_player->m_player_y, m_map_data.get());
								}
								break;
							case hb::shared::owner::Guard: //
								if ((abs_x <= 2) && (abs_y <= 2) && combat_system::get().can_super_attack()) {
									if ((abs_x <= 1) && (abs_y <= 1) && (hb::shared::input::is_shift_down() || config_manager::get().is_running_mode_enabled()) && (m_player->m_sp > 0))
										m_player->m_Controller.set_command(Type::AttackMove);
									else m_player->m_Controller.set_command(Type::Attack);
									m_player->m_Controller.set_destination(m_mcx, m_mcy);
									action_type = combat_system::get().get_attack_type();
								}
								else {
									if ((hb::shared::input::is_shift_down() || config_manager::get().is_running_mode_enabled()) && (m_player->m_sp > 0) &&
										(m_player->m_player_type >= 1) && (m_player->m_player_type <= 6))
										m_player->m_Controller.set_command(Type::Run);
									else m_player->m_Controller.set_command(Type::Move);
									m_player->m_Controller.set_destination(m_mcx, m_mcy);
									m_player->m_Controller.calculate_player_turn(m_player->m_player_x, m_player->m_player_y, m_map_data.get());
								}
								break;
							}
						}
						break;
					}
				}
				else {
					if ((hb::shared::input::is_shift_down() || config_manager::get().is_running_mode_enabled()) && (m_player->m_sp > 0) &&
						(m_player->m_player_type >= 1) && (m_player->m_player_type <= 6))
						m_player->m_Controller.set_command(Type::Run);
					else m_player->m_Controller.set_command(Type::Move);
					m_player->m_Controller.set_destination(m_mcx, m_mcy);
					m_player->m_Controller.calculate_player_turn(m_player->m_player_x, m_player->m_player_y, m_map_data.get());
				}
			}
		}
		else
		{
			if ((hb::shared::input::is_shift_down() || config_manager::get().is_running_mode_enabled()) && (m_player->m_sp > 0) &&
				(m_player->m_player_type >= 1) && (m_player->m_player_type <= 6))
				m_player->m_Controller.set_command(Type::Run);
			else m_player->m_Controller.set_command(Type::Move);
			m_player->m_Controller.set_destination(tile_x, tile_y);
			m_player->m_Controller.calculate_player_turn(m_player->m_player_x, m_player->m_player_y, m_map_data.get());
		}
	}
	return false;
}

bool CGame::process_right_click(short mouse_x, short mouse_y, short tile_x, short tile_y, uint32_t current_time, uint16_t& action_type)
{
	std::string name;
	short object_type = 0;
	hb::shared::entity::PlayerStatus object_status;
	direction move_dir = direction{};
	char abs_x = 0, abs_y = 0;

	// Right click on self = pickup (Quick Actions feature)
	if (config_manager::get().is_quick_actions_enabled() &&
		m_mc_name == m_player->m_player_name &&
		(m_player->m_player_type >= 1) && (m_player->m_player_type <= 6))
	{
		m_player->m_Controller.set_command(Type::GetItem);
		m_player->m_Controller.set_destination(m_player->m_player_x, m_player->m_player_y);
		return false;
	}
	else
	{
		// Original right click behavior (stop, turn, attack, etc.)
		m_player->m_Controller.set_command(Type::stop);
		if (on_game()->m_is_get_pointing_mode == true)
		{
			on_game()->m_is_get_pointing_mode = false;
			add_event_list(COMMAND_PROCESSOR1, 10);
		}
		if (m_player->m_Controller.is_command_available() == false) return true;
		if (m_player->m_Controller.get_command_count() >= 6) return true;

		if ((m_mcx != 0) && (m_mcy != 0))
		{
			abs_x = abs(m_player->m_player_x - m_mcx);
			abs_y = abs(m_player->m_player_y - m_mcy);
			if (abs_x == 0 && abs_y == 0) return true;

			if (hb::shared::input::is_ctrl_down() == true)
			{
				m_map_data->get_owner(m_mcx, m_mcy, name, &object_type, &object_status, &m_comm_object_id);
				if (object_status.invisibility) return true;
				if ((object_type == 15) || (object_type == 20) || (object_type == 24)) return true;

				if ((abs_x <= 1) && (abs_y <= 1))
				{
					action_type = combat_system::get().get_attack_type();
					m_player->m_Controller.set_command(Type::Attack);
					m_player->m_Controller.set_destination(m_mcx, m_mcy);
				}
				else if ((abs_x <= 2) && (abs_y <= 2) // strike on Big mobs & gate from a range
					&& ((object_type == 66) || (object_type == 73) || (object_type == 81) || (object_type == 91)))
				{
					action_type = combat_system::get().get_attack_type();
					m_player->m_Controller.set_command(Type::Attack);
					m_player->m_Controller.set_destination(m_mcx, m_mcy);
				}
				else
				{
					switch (combat_system::get().get_weapon_skill_type()) {
					case 6: // Bow
						m_player->m_Controller.set_command(Type::Attack);
						m_player->m_Controller.set_destination(m_mcx, m_mcy);
						action_type = combat_system::get().get_attack_type();
						break;

					case 5: // Boxe
					case 7: // SS
						break;

					case 8: // LS
						if ((abs_x <= 3) && (abs_y <= 3) && combat_system::get().can_super_attack()
							&& (combat_system::get().get_attack_type() != 30)) // without StormBlade by Snoopy
						{
							action_type = combat_system::get().get_attack_type();
							m_player->m_Controller.set_command(Type::Attack);
							m_player->m_Controller.set_destination(m_mcx, m_mcy);
						}
						else if ((abs_x <= 5) && (abs_y <= 5) && combat_system::get().can_super_attack()
							&& (combat_system::get().get_attack_type() == 30)) // with stormBlade crit by Snoopy
						{
							action_type = combat_system::get().get_attack_type();
							m_player->m_Controller.set_command(Type::Attack);
							m_player->m_Controller.set_destination(m_mcx, m_mcy);
						}
						else if ((abs_x <= 3) && (abs_y <= 3)
							&& (combat_system::get().get_attack_type() == 5)) // with stormBlade no crit by Snoopy
						{
							action_type = combat_system::get().get_attack_type();
							m_player->m_Controller.set_command(Type::Attack);
							m_player->m_Controller.set_destination(m_mcx, m_mcy);
						}
						break;

					case 9: // Fencing
						if ((abs_x <= 4) && (abs_y <= 4) && combat_system::get().can_super_attack()) {
							m_player->m_Controller.set_command(Type::Attack);
							m_player->m_Controller.set_destination(m_mcx, m_mcy);
							action_type = combat_system::get().get_attack_type();
						}
						break;

					case hb::shared::owner::Slime: //
						if ((abs_x <= 2) && (abs_y <= 2) && combat_system::get().can_super_attack()) {
							m_player->m_Controller.set_command(Type::Attack);
							m_player->m_Controller.set_destination(m_mcx, m_mcy);
							action_type = combat_system::get().get_attack_type();
						}
						break;

					case hb::shared::owner::OrcMage: //
						if ((abs_x <= 2) && (abs_y <= 2) && combat_system::get().can_super_attack()) {
							m_player->m_Controller.set_command(Type::Attack);
							m_player->m_Controller.set_destination(m_mcx, m_mcy);
							action_type = combat_system::get().get_attack_type();
						}
						break;
					case hb::shared::owner::Guard: //
						if ((abs_x <= 2) && (abs_y <= 2) && combat_system::get().can_super_attack()) {
							m_player->m_Controller.set_command(Type::Attack);
							m_player->m_Controller.set_destination(m_mcx, m_mcy);
							action_type = combat_system::get().get_attack_type();
						}
						break;
					}
				}
			}
			else // CTRL not pressed
			{
				abs_x = abs(m_player->m_player_x - m_mcx);
				abs_y = abs(m_player->m_player_y - m_mcy);
				m_map_data->get_owner(m_mcx, m_mcy, name, &object_type, &object_status, &m_comm_object_id);
				if (object_type >= 10 || ((object_type >= 1) && (object_type <= 6))) {
					switch (object_type) {
					case hb::shared::owner::ShopKeeper:
					case hb::shared::owner::Gandalf:
					case hb::shared::owner::Howard:
					case hb::shared::owner::Tom:
					case hb::shared::owner::William:
					case hb::shared::owner::Kennedy: // npcs
						break;

					default: // All "normal mobs"
						if (!IsHostile(object_status.relationship)) break;
						if ((object_type >= 1) && (object_type <= 6) && (m_player->m_force_attack == false)) break;
						if ((abs_x <= 1) && (abs_y <= 1))
						{
							action_type = combat_system::get().get_attack_type();
							m_player->m_Controller.set_command(Type::Attack);
							m_player->m_Controller.set_destination(m_mcx, m_mcy);
						}
						else if ((abs_x <= 2) && (abs_y <= 2) // strike on Big mobs & gate from a range
							&& ((object_type == 66) || (object_type == 73) || (object_type == 81) || (object_type == 91)))
						{
							action_type = combat_system::get().get_attack_type();
							m_player->m_Controller.set_command(Type::Attack);
							m_player->m_Controller.set_destination(m_mcx, m_mcy);
						}
						else //
						{
							switch (combat_system::get().get_weapon_skill_type()) {
							case 6: // Bow
								m_player->m_Controller.set_command(Type::Attack);
								m_player->m_Controller.set_destination(m_mcx, m_mcy);
								action_type = combat_system::get().get_attack_type();
								break;

							case 5: // Boxe
							case 7: // SS
								break;

							case 8: // LS
								if ((abs_x <= 3) && (abs_y <= 3) && combat_system::get().can_super_attack()
									&& (combat_system::get().get_attack_type() != 30)) // crit without StormBlade by Snoopy
								{
									action_type = combat_system::get().get_attack_type();
									m_player->m_Controller.set_command(Type::Attack);
									m_player->m_Controller.set_destination(m_mcx, m_mcy);
								}
								else if ((abs_x <= 5) && (abs_y <= 5) && combat_system::get().can_super_attack()
									&& (combat_system::get().get_attack_type() == 30)) // with stormBlade crit by Snoopy
								{
									action_type = combat_system::get().get_attack_type();
									m_player->m_Controller.set_command(Type::Attack);
									m_player->m_Controller.set_destination(m_mcx, m_mcy);
								}
								else if ((abs_x <= 3) && (abs_y <= 3)
									&& (combat_system::get().get_attack_type() == 5)) // with stormBlade no crit by Snoopy
								{
									action_type = combat_system::get().get_attack_type();
									m_player->m_Controller.set_command(Type::Attack);
									m_player->m_Controller.set_destination(m_mcx, m_mcy);
								}
								break;

							case 9: // fencing
								if ((abs_x <= 4) && (abs_y <= 4) && combat_system::get().can_super_attack()) {
									m_player->m_Controller.set_command(Type::Attack);
									m_player->m_Controller.set_destination(m_mcx, m_mcy);
									action_type = combat_system::get().get_attack_type();
								}
								break;

							case hb::shared::owner::Slime: //
								if ((abs_x <= 2) && (abs_y <= 2) && combat_system::get().can_super_attack()) {
									m_player->m_Controller.set_command(Type::Attack);
									m_player->m_Controller.set_destination(m_mcx, m_mcy);
									action_type = combat_system::get().get_attack_type();
								}
								break;
							case hb::shared::owner::OrcMage: // hammer
								if ((abs_x <= 2) && (abs_y <= 2) && combat_system::get().can_super_attack()) {
									m_player->m_Controller.set_command(Type::Attack);
									m_player->m_Controller.set_destination(m_mcx, m_mcy);
									action_type = combat_system::get().get_attack_type();
								}
								break;
							case hb::shared::owner::Guard: // wand
								if ((abs_x <= 2) && (abs_y <= 2) && combat_system::get().can_super_attack()) {
									m_player->m_Controller.set_command(Type::Attack);
									m_player->m_Controller.set_destination(m_mcx, m_mcy);
									action_type = combat_system::get().get_attack_type();
								}
								break;
							}
						}
						break;
					}
				}
			}
		}
		else
		{
			move_dir = CMisc::get_next_move_dir(m_player->m_player_x, m_player->m_player_y, tile_x, tile_y);
			if (m_player->m_hp <= 0) return true;
			if (move_dir == 0) return true;
			if (m_player->m_player_dir == move_dir) return true;
			clear_skill_using_status();
			m_player->m_player_dir = move_dir;
			send_game_packet(hb::net::make_motion(Type::stop, m_player->m_player_x, m_player->m_player_y, m_player->m_player_dir));
			m_player->m_Controller.increment_command_count();

			m_map_data->set_owner(m_player->m_player_object_id, m_player->m_player_x, m_player->m_player_y, m_player->m_player_type, m_player->m_player_dir,
				m_player->m_playerAppearance,
				m_player->m_playerStatus, m_player->m_player_name,
				m_player->m_Controller.get_command(), 0, 0, 0, 0,
				10);
			m_player->m_Controller.set_command_available(false);
			m_player->m_Controller.set_command_time(GameClock::get_time_ms());
			return true;
		}
	} // close else block for "not clicking on self"
	return false;
}

void CGame::process_motion_commands(uint16_t action_type)
{
	// Block all movement during an active teleport to prevent walking off
	// the teleport tile or sending stale movement to the server.
	if (teleport_manager::get().is_active()) return;

	direction move_dir = direction{};
	std::string dest_name;
	short dest_owner_type = 0;
	hb::shared::entity::PlayerStatus dest_owner_status;
	bool has_owner = false;
	std::string text;


	if (m_player->m_Controller.get_command() != Type::stop)
	{
		if (m_player->m_hp <= 0) return;
		if (m_player->m_Controller.get_command_count() == 5) add_event_list(COMMAND_PROCESSOR2, 10, false);
		if (m_player->m_Controller.is_command_available() == false) return;
		if (m_player->m_Controller.get_command_count() >= 6) return;

		if ((m_player->m_player_type >= 0) && (m_player->m_player_type > 6))
		{
			switch (m_player->m_Controller.get_command()) {
			case Type::Run:
			case Type::Magic:
			case Type::GetItem:
				m_player->m_Controller.set_command(Type::stop);
				break;
			}
		}

		clear_skill_using_status();

		if ((m_player->m_damage_move != 0) || (m_player->m_damage_move_amount != 0))
		{
			if (m_player->m_damage_move != 0)
			{
				m_player->m_Controller.set_command(Type::DamageMove);
				m_player->m_Controller.set_destination(m_player->m_player_x, m_player->m_player_y);

				// mim crit fixed by kaozures tocado para ande bien by cloud :P
				if (m_illusion_mvt == true) {
					switch (m_player->m_damage_move) {
					case 1: m_player->m_Controller.move_destination(0, 1); break;
					case 2: m_player->m_Controller.move_destination(-1, 1); break;
					case 3: m_player->m_Controller.move_destination(-1, 0); break;
					case 4: m_player->m_Controller.move_destination(-1, -1); break;
					case 5: m_player->m_Controller.move_destination(0, -1); break;
					case 6: m_player->m_Controller.move_destination(1, -1); break;
					case 7: m_player->m_Controller.move_destination(1, 0); break;
					case 8: m_player->m_Controller.move_destination(1, 1); break;
					}
				}
				else {
					switch (m_player->m_damage_move) {
					case 1: m_player->m_Controller.move_destination(0, -1); break;
					case 2: m_player->m_Controller.move_destination(1, -1); break;
					case 3: m_player->m_Controller.move_destination(1, 0); break;
					case 4: m_player->m_Controller.move_destination(1, 1); break;
					case 5: m_player->m_Controller.move_destination(0, 1); break;
					case 6: m_player->m_Controller.move_destination(-1, 1); break;
					case 7: m_player->m_Controller.move_destination(-1, 0); break;
					case 8: m_player->m_Controller.move_destination(-1, -1); break;
					}
				}
			}

			get_floating_text().add_damage_from_value(m_player->m_damage_move_amount, false, m_cur_time,
				m_player->m_player_object_id, m_map_data.get());
			m_player->m_damage_move = 0;
			m_player->m_damage_move_amount = 0;
		}

		switch (m_player->m_Controller.get_command()) {
		case Type::Run:
		case Type::Move:
		case Type::DamageMove: // v1.43

			if (m_player->m_paralyze) return;
			has_owner = m_map_data->get_owner(m_player->m_Controller.get_destination_x(), m_player->m_Controller.get_destination_y(), dest_name, &dest_owner_type, &dest_owner_status, &m_comm_object_id); // v1.4

			if ((m_player->m_player_x == m_player->m_Controller.get_destination_x()) && (m_player->m_player_y == m_player->m_Controller.get_destination_y()))
			{
				m_player->m_Controller.set_command(Type::stop);
				// Apply pending stop direction if set (from right-click while moving)
				direction pending_dir = m_player->m_Controller.get_pending_stop_dir();
				if (pending_dir != 0)
				{
					m_player->m_player_dir = pending_dir;
					send_game_packet(hb::net::make_motion(Type::stop, m_player->m_player_x, m_player->m_player_y, pending_dir));
					m_player->m_Controller.increment_command_count();
					m_map_data->set_owner(m_player->m_player_object_id, m_player->m_player_x, m_player->m_player_y,
						m_player->m_player_type, pending_dir, m_player->m_playerAppearance,
						m_player->m_playerStatus, m_player->m_player_name, Type::stop, 0, 0, 0, 0, 10);
					m_player->m_Controller.clear_pending_stop_dir();
				}
			}
			else if ((abs(m_player->m_player_x - m_player->m_Controller.get_destination_x()) <= 1) && (abs(m_player->m_player_y - m_player->m_Controller.get_destination_y()) <= 1) &&
				(has_owner == true) && (dest_owner_type != 0))
				m_player->m_Controller.set_command(Type::stop);
			else if ((abs(m_player->m_player_x - m_player->m_Controller.get_destination_x()) <= 2) && (abs(m_player->m_player_y - m_player->m_Controller.get_destination_y()) <= 2) &&
				(m_map_data->m_tile[m_player->m_Controller.get_destination_x()][m_player->m_Controller.get_destination_y()].m_bIsMoveAllowed == false))
				m_player->m_Controller.set_command(Type::stop);
			else
			{
				if (m_player->m_Controller.get_command() == Type::Move)
				{
					if (config_manager::get().is_running_mode_enabled() || hb::shared::input::is_shift_down()) m_player->m_Controller.set_command(Type::Run);
				}
				if (m_player->m_Controller.get_command() == Type::Run)
				{
					if ((config_manager::get().is_running_mode_enabled() == false) && (hb::shared::input::is_shift_down() == false)) m_player->m_Controller.set_command(Type::Move);
					if (m_player->m_sp < 1) m_player->m_Controller.set_command(Type::Move);
				}

				move_dir = m_player->m_Controller.get_next_move_dir(m_player->m_player_x, m_player->m_player_y, m_player->m_Controller.get_destination_x(), m_player->m_Controller.get_destination_y(), m_map_data.get(), true);
				// Snoopy: Illusion Movement
				if ((m_illusion_mvt == true) && (m_player->m_Controller.get_command() != Type::DamageMove))
				{
					move_dir = m_player->m_Controller.get_next_move_dir(m_player->m_player_x, m_player->m_player_y, m_player->m_Controller.get_destination_x(), m_player->m_Controller.get_destination_y(), m_map_data.get(), true, true);
				}
				if (move_dir != 0)
				{
					// Cancel logout countdown on movement
					if ((on_game()->m_logout_count > 0) && (m_force_disconn == false))
					{
						on_game()->m_logout_count = -1;
						add_event_list(DLGBOX_CLICK_SYSMENU2, 10);
					}

					m_player->m_player_dir = move_dir;
					send_game_packet(hb::net::make_motion(m_player->m_Controller.get_command(), m_player->m_player_x, m_player->m_player_y, move_dir));
					m_player->m_Controller.increment_command_count();
					switch (move_dir) {
					case direction::north:	m_player->m_player_y--; break;
					case direction::northeast:	m_player->m_player_y--; m_player->m_player_x++;	break;
					case direction::east:	m_player->m_player_x++; break;
					case direction::southeast:	m_player->m_player_x++; m_player->m_player_y++;	break;
					case direction::south:	m_player->m_player_y++; break;
					case direction::southwest:	m_player->m_player_x--; m_player->m_player_y++;	break;
					case direction::west:	m_player->m_player_x--; break;
					case direction::northwest:	m_player->m_player_x--; m_player->m_player_y--;	break;
					default: break;
					}
					m_map_data->set_owner(m_player->m_player_object_id, m_player->m_player_x, m_player->m_player_y, m_player->m_player_type, m_player->m_player_dir,
						m_player->m_playerAppearance, // v1.4
						m_player->m_playerStatus, m_player->m_player_name,
						m_player->m_Controller.get_command(), 0, 0, 0);
					m_player->m_Controller.set_command_available(false);
					m_player->m_Controller.set_command_time(GameClock::get_time_ms());
					m_player->m_Controller.set_prev_move(m_player->m_player_x, m_player->m_player_y);
				}
			}

			if (m_player->m_Controller.get_command() == Type::DamageMove)
			{
				on_game()->m_is_get_pointing_mode = false;
				on_game()->m_point_command_type = -1;
				clear_skill_using_status();
				m_player->m_Controller.set_command(Type::stop);
			}
			break;

		case Type::Attack:
			move_dir = CMisc::get_next_move_dir(m_player->m_player_x, m_player->m_player_y, m_player->m_Controller.get_destination_x(), m_player->m_Controller.get_destination_y());
			// Snoopy: Illusion movement
			if (m_illusion_mvt == true)
			{
				move_dir = static_cast<direction>(move_dir + 4);
				if (move_dir > 8) move_dir = static_cast<direction>(move_dir - 8);
			}
			if (move_dir != 0)
			{
				// Cancel logout countdown on attack
				if ((on_game()->m_logout_count > 0) && (m_force_disconn == false))
				{
					on_game()->m_logout_count = -1;
					add_event_list(DLGBOX_CLICK_SYSMENU2, 10);
				}

				if ((action_type == 2) || (action_type == 25))
				{
					if (has_item_with_sub_type(hb::shared::item::item_sub_type::ammo) == false)
						action_type = 0;
				}
				m_player->m_player_dir = move_dir;
				m_last_attack_target_id = m_comm_object_id;
				send_game_packet(hb::net::make_motion_attack(Type::Attack, m_player->m_player_x, m_player->m_player_y, move_dir, static_cast<int16_t>(m_player->m_Controller.get_destination_x()), static_cast<int16_t>(m_player->m_Controller.get_destination_y()), static_cast<int16_t>(action_type), static_cast<uint16_t>(m_comm_object_id)));
				m_player->m_Controller.increment_command_count();
				m_map_data->set_owner(m_player->m_player_object_id, m_player->m_player_x, m_player->m_player_y, m_player->m_player_type, m_player->m_player_dir,
					m_player->m_playerAppearance,
					m_player->m_playerStatus, m_player->m_player_name,
					Type::Attack,
					m_player->m_Controller.get_destination_x() - m_player->m_player_x, m_player->m_Controller.get_destination_y() - m_player->m_player_y, action_type);
				m_player->m_Controller.set_command_available(false);
				m_player->m_Controller.set_command_time(GameClock::get_time_ms());
				// Compute expected swing duration (must match server's check_client_attack_frequency formula)
				{
					int base_swing = hb::shared::calc::swing_time(m_player->m_playerStatus.attack_delay);
					int effective_swing = base_swing;
					if (m_player->m_playerStatus.frozen) effective_swing += hb::shared::balance::swing_frames * (hb::shared::balance::base_frame_time >> 2);
					if (m_player->m_playerStatus.haste)  effective_swing -= hb::shared::balance::swing_frames * static_cast<int>(hb::shared::balance::run_frame_time / 2.3);
					m_player->m_Controller.set_attack_end_time(GameClock::get_time_ms() + effective_swing);
				}
			}
			m_player->m_Controller.set_command(Type::stop);
			break;

		case Type::AttackMove:
			if (m_player->m_paralyze) return;
			has_owner = m_map_data->get_owner(m_player->m_Controller.get_destination_x(), m_player->m_Controller.get_destination_y(), dest_name, &dest_owner_type, &dest_owner_status, &m_comm_object_id);
			if ((m_player->m_player_x == m_player->m_Controller.get_destination_x()) && (m_player->m_player_y == m_player->m_Controller.get_destination_y()))
				m_player->m_Controller.set_command(Type::stop);
			else if ((abs(m_player->m_player_x - m_player->m_Controller.get_destination_x()) <= 1) && (abs(m_player->m_player_y - m_player->m_Controller.get_destination_y()) <= 1) &&
				(has_owner == true) && (dest_owner_type != 0))
				m_player->m_Controller.set_command(Type::stop);
			else
			{
				move_dir = m_player->m_Controller.get_next_move_dir(m_player->m_player_x, m_player->m_player_y, m_player->m_Controller.get_destination_x(), m_player->m_Controller.get_destination_y(), m_map_data.get(), true);
				// Snoopy: Illusion mvt
				if (m_illusion_mvt == true)
				{
					move_dir = m_player->m_Controller.get_next_move_dir(m_player->m_player_x, m_player->m_player_y, m_player->m_Controller.get_destination_x(), m_player->m_Controller.get_destination_y(), m_map_data.get(), true, true);
				}
				if (move_dir != 0)
				{
					// Cancel logout countdown on attack-move
					if ((on_game()->m_logout_count > 0) && (m_force_disconn == false))
					{
						on_game()->m_logout_count = -1;
						add_event_list(DLGBOX_CLICK_SYSMENU2, 10);
					}

					m_player->m_player_dir = move_dir;
					m_last_attack_target_id = m_comm_object_id;
					send_game_packet(hb::net::make_motion_attack(Type::AttackMove, m_player->m_player_x, m_player->m_player_y, move_dir, static_cast<int16_t>(m_player->m_Controller.get_destination_x()), static_cast<int16_t>(m_player->m_Controller.get_destination_y()), static_cast<int16_t>(action_type), static_cast<uint16_t>(m_comm_object_id)));
					m_player->m_Controller.increment_command_count();
					switch (move_dir) {
					case direction::north:	m_player->m_player_y--; break;
					case direction::northeast:	m_player->m_player_y--; m_player->m_player_x++;	break;
					case direction::east:	m_player->m_player_x++; break;
					case direction::southeast:	m_player->m_player_x++; m_player->m_player_y++;	break;
					case direction::south:	m_player->m_player_y++; break;
					case direction::southwest:	m_player->m_player_x--; m_player->m_player_y++;	break;
					case direction::west:	m_player->m_player_x--; break;
					case direction::northwest:	m_player->m_player_x--; m_player->m_player_y--;	break;
					default: break;
					}

					m_map_data->set_owner(m_player->m_player_object_id, m_player->m_player_x, m_player->m_player_y, m_player->m_player_type, m_player->m_player_dir,
						m_player->m_playerAppearance,
						m_player->m_playerStatus, m_player->m_player_name,
						m_player->m_Controller.get_command(), m_player->m_Controller.get_destination_x() - m_player->m_player_x, m_player->m_Controller.get_destination_y() - m_player->m_player_y, action_type);
					m_player->m_Controller.set_command_available(false);
					m_player->m_Controller.set_command_time(GameClock::get_time_ms());
					// Compute expected swing duration (must match server's check_client_attack_frequency formula)
					{
						int base_swing = hb::shared::calc::swing_time(m_player->m_playerStatus.attack_delay);
						int effective_swing = base_swing;
						if (m_player->m_playerStatus.frozen) effective_swing += hb::shared::balance::swing_frames * (hb::shared::balance::base_frame_time >> 2);
						if (m_player->m_playerStatus.haste)  effective_swing -= hb::shared::balance::swing_frames * static_cast<int>(hb::shared::balance::run_frame_time / 2.3);
						m_player->m_Controller.set_attack_end_time(GameClock::get_time_ms() + effective_swing);
					}
					m_player->m_Controller.set_prev_move(m_player->m_player_x, m_player->m_player_y);
				}
			}
			m_player->m_Controller.set_command(Type::stop);
			break;

		case Type::GetItem:
			// Cancel logout countdown on get item
			if ((on_game()->m_logout_count > 0) && (m_force_disconn == false))
			{
				on_game()->m_logout_count = -1;
				add_event_list(DLGBOX_CLICK_SYSMENU2, 10);
			}

			send_game_packet(hb::net::make_motion(Type::GetItem, m_player->m_player_x, m_player->m_player_y, m_player->m_player_dir));
			m_player->m_Controller.increment_command_count();
			m_map_data->set_owner(m_player->m_player_object_id, m_player->m_player_x, m_player->m_player_y, m_player->m_player_type, m_player->m_player_dir,
				m_player->m_playerAppearance,
				m_player->m_playerStatus, m_player->m_player_name,
				Type::GetItem, 0, 0, 0);
			m_player->m_Controller.set_command_available(false);
			m_player->m_Controller.set_command(Type::stop);
			break;

		case Type::Magic:
			// Cancel logout countdown on magic cast
			if ((on_game()->m_logout_count > 0) && (m_force_disconn == false))
			{
				on_game()->m_logout_count = -1;
				add_event_list(DLGBOX_CLICK_SYSMENU2, 10);
			}

			send_game_packet(hb::net::make_motion(Type::Magic, m_player->m_player_x, m_player->m_player_y, m_player->m_player_dir, static_cast<int16_t>(m_casting_magic_type)));
			m_player->m_Controller.increment_command_count();
			m_map_data->set_owner(m_player->m_player_object_id, m_player->m_player_x, m_player->m_player_y, m_player->m_player_type, m_player->m_player_dir,
				m_player->m_playerAppearance,
				m_player->m_playerStatus, m_player->m_player_name,
				Type::Magic, m_casting_magic_type, 0, 0);
			m_player->m_Controller.set_command_available(false);
			m_player->m_Controller.set_command_time(GameClock::get_time_ms());
			// Only enter targeting mode if the cast wasn't interrupted by damage
			if (on_game()->m_point_command_type >= 100 && on_game()->m_point_command_type < 200)
				on_game()->m_is_get_pointing_mode = true;
			m_player->m_Controller.set_command(Type::stop);
			get_floating_text().remove_by_object_id(m_player->m_player_object_id);
			{
				text = std::format("{}!", m_magic_cfg_list[m_casting_magic_type]->m_name);
				get_floating_text().add_notify_text(notify_text_type::magic_cast_name, text, GameClock::get_time_ms(),
					m_player->m_player_object_id, m_map_data.get());
			}
			return;

		default:
			break;
		}
	}
}

// _Draw_OnLogin removed - migrated to Screen_Login

void CGame::request_teleport_and_wait_data()
{
	if (teleport_manager::get().is_requested()) return;
	if (teleport_manager::get().is_active()) return;

	// Stop all movement and clear cursor state so the player doesn't
	// walk off the teleport tile while waiting for server approval.
	m_player->m_Controller.set_command(Type::stop);
	m_player->m_Controller.reset_command_count();
	CursorTarget::set_cursor_status(CursorStatus::Null);

	teleport_manager::get().request_auth(m_player->m_player_x, m_player->m_player_y);
}

void CGame::init_data_response_handler(char* packet_data)
{
	short pivot_x = 0, pivot_y = 0;
	std::string text, prev_location;

	char map_filename[32]{};
	bool is_observer = false;
	uint32_t file_size = 0;

	m_player->m_paralyze = false;
	m_map_data->init();

	m_monster_id = 0;
	m_monster_event_time = 0;

	get_dialog_box_manager().disable_npc_dialogs();

	m_player->m_Controller.set_command(Type::stop);
	m_player->m_Controller.reset_command_count();
	on_game()->m_is_get_pointing_mode = false;
	on_game()->m_point_command_type = -1;
	on_game()->m_ilusion_owner_h = 0;
	on_game()->m_ilusion_owner_type = 0;
	teleport_manager::get().set_requested(false);
	m_player->m_is_confusion = false;
	on_game()->m_skill_using_status = false;

	on_game()->m_item_using_status = false;

	m_restart_count = -1;
	m_restart_count_time = 0;

	if (m_effect_manager) m_effect_manager->clear_all_effects();

	weather_manager::get().reset_particles();

	get_floating_text().clear_all();

	const auto* pkt = hb::net::PacketCast<hb::net::PacketResponseInitDataHeader>(
		packet_data, sizeof(hb::net::PacketResponseInitDataHeader));
	if (!pkt) return;
	m_player->m_player_object_id = pkt->player_object_id;
	pivot_x = pkt->pivot_x;
	pivot_y = pkt->pivot_y;
	m_player->m_player_type = pkt->player_type;
	m_player->m_playerAppearance = pkt->appearance;
	m_player->m_playerStatus = pkt->status;

	//Snoopy MIM fix
	if (m_player->m_playerStatus.illusion_movement)
	{
		m_illusion_mvt = true;
	}
	else
	{
		m_illusion_mvt = false;
	}

	// GM mode detection
	m_player->m_is_gm_mode = m_player->m_playerStatus.gm_mode;
	m_map_name.assign(pkt->map_name, strnlen(pkt->map_name, sizeof(pkt->map_name)));
	char mapMsgBuf[32]{};
	m_map_index = get_official_map_name(m_map_name.c_str(), mapMsgBuf);
	m_map_message = mapMsgBuf;
	if (m_map_index < 0)
	{
		get_dialog_box_manager().get_dialog_box(DialogBoxId::GuideMap)->m_size_x = -1;
		get_dialog_box_manager().get_dialog_box(DialogBoxId::GuideMap)->m_size_y = -1;
	}
	else
	{
		get_dialog_box_manager().get_dialog_box(DialogBoxId::GuideMap)->m_size_x = 128;
		get_dialog_box_manager().get_dialog_box(DialogBoxId::GuideMap)->m_size_y = 128;
		get_dialog_box_manager().enable_dialog_box(DialogBoxId::GuideMap, 0, 0, 0);
	}

	prev_location = m_cur_location;
	m_cur_location.assign(pkt->cur_location, strnlen(pkt->cur_location, sizeof(pkt->cur_location)));

	weather_manager::get().set_ambient_light(static_cast<char>(pkt->sprite_alpha));

	weather_manager::get().set_weather_status(static_cast<char>(pkt->weather_status));
	switch (weather_manager::get().get_ambient_light()) { // Xmas bulbs
		// Will be sent by server if DayTime is 3 (and a snowy weather)
	case 1:	on_game()->m_is_xmas = false; break;
	case 2: on_game()->m_is_xmas = false; break;
	case 3: // Snoopy Special night with chrismas bulbs
		if (weather_manager::get().get_weather_status() > 3) on_game()->m_is_xmas = true;
		else on_game()->m_is_xmas = false;
		weather_manager::get().set_xmas(on_game()->m_is_xmas);
		weather_manager::get().set_ambient_light(2);
		break;
	}
	m_player->m_contribution = pkt->contribution;
	is_observer = pkt->observer_mode != 0;
	m_player->m_hp = pkt->hp;
	m_discount = static_cast<char>(pkt->discount);

	const char* cursor = reinterpret_cast<const char*>(packet_data) + sizeof(hb::net::PacketResponseInitDataHeader);

	{
		char ws = weather_manager::get().get_weather_status();
		if (ws != 0)
		{
			weather_manager::get().set_weather(true, ws);
			if (ws >= 4 && ws <= 6 && audio_manager::get().is_music_enabled())
				start_bgm();
		}
		else weather_manager::get().set_weather(false, 0);
	}

	// Lowercase map name for case-sensitive filesystem compatibility
	std::string lower_map_name = m_map_name;
	std::transform(lower_map_name.begin(), lower_map_name.end(), lower_map_name.begin(),
		[](unsigned char c) { return static_cast<char>(std::tolower(c)); });

	std::memset(map_filename, 0, sizeof(map_filename));
	std::snprintf(map_filename + strlen(map_filename), sizeof(map_filename) - strlen(map_filename), "%s", "mapdata/");
	// CLEROTH - MW MAPS
	if (lower_map_name.starts_with("defaultmw"))
	{
		std::snprintf(map_filename + strlen(map_filename), sizeof(map_filename) - strlen(map_filename), "%s", "mw/defaultmw");
	}
	else
	{
		std::snprintf(map_filename + strlen(map_filename), sizeof(map_filename) - strlen(map_filename), "%s", lower_map_name.c_str());
	}

	std::snprintf(map_filename + strlen(map_filename), sizeof(map_filename) - strlen(map_filename), "%s", ".amd");
	m_map_data->open_map_data_file(map_filename);

	m_map_data->m_pivot_x = pivot_x;
	m_map_data->m_pivot_y = pivot_y;

	m_player->m_player_x = pivot_x + hb::shared::view::PlayerPivotOffsetX;
	m_player->m_player_y = pivot_y + hb::shared::view::PlayerPivotOffsetY;

	m_player->m_player_dir = direction::south;

	if (is_observer == false)
	{
		m_map_data->set_owner(m_player->m_player_object_id, m_player->m_player_x, m_player->m_player_y, m_player->m_player_type, m_player->m_player_dir,
			m_player->m_playerAppearance, // v1.4
			m_player->m_playerStatus, m_player->m_player_name,
			Type::stop, 0, 0, 0);
	}

	m_Camera.snap_to((m_player->m_player_x - VIEW_CENTER_TILE_X()) * 32 - 16, (m_player->m_player_y - VIEW_CENTER_TILE_Y()) * 32 - 16);
	read_map_data(pivot_x + hb::shared::view::MapDataBufferX, pivot_y + hb::shared::view::MapDataBufferY, cursor);
	// ------------------------------------------------------------------------+
	text = std::format(INITDATA_RESPONSE_HANDLER1, m_map_message);
	add_event_list(text.c_str(), 10);

	get_dialog_box_manager().get_dialog_box(DialogBoxId::WarningBattleArea)->m_x = 150;
	get_dialog_box_manager().get_dialog_box(DialogBoxId::WarningBattleArea)->m_y = 130;

	if ((m_cur_location.starts_with("middleland"))
		|| (m_cur_location.starts_with("dglv2"))
		|| (m_cur_location.starts_with("middled1n")))
		get_dialog_box_manager().enable_dialog_box(DialogBoxId::WarningBattleArea, 0, 0, 0);

	m_is_server_changing = false;

	// Notify TeleportManager that map data has loaded (screen is still black)
	if (teleport_manager::get().get_state() == teleport_state::awaiting_data)
		teleport_manager::get().on_map_loaded();

	// Screen_OnGame is already active — clear the waiting overlay when all data is ready
	m_init_data_ready = true;
	if (m_configs_ready) {
		GameModeManager::clear_overlay();
	}

	//v1.41
	if (m_player->m_playerAppearance.is_walking)
		m_player->m_is_combat_mode = true;
	else m_player->m_is_combat_mode = false;

	//v1.42
	if (m_is_first_conn == true)
	{
		m_is_first_conn = false;
		std::error_code ec;
		auto sz = std::filesystem::file_size("contents/contents1000.txt", ec);
		file_size = ec ? 0 : static_cast<uint32_t>(sz);
		{
			hb::net::PacketRequestNoticement pkt{};
			pkt.header.msg_id = MsgId::RequestNoticement;
			pkt.value = static_cast<int>(file_size);
			send_game_packet(pkt);
		}
	}
}

void CGame::motion_event_handler(char* packet_data)
{
	uint16_t event_type = 0, object_id = 0;
	short map_x = -1, map_y = -1, owner_type = 0;
	int value1 = 0;
	short value2 = 0, value3 = 0;
	short npc_config_id = -1;
	hb::shared::entity::PlayerStatus status;
	direction move_dir = direction{};
	std::string name;
	int location = 0;
	hb::shared::entity::PlayerAppearance appearance;
	bool prev_combat_mode = false;
	std::string text;

	const auto* header = hb::net::PacketCast<hb::net::PacketHeader>(packet_data, sizeof(hb::net::PacketHeader));
	if (!header) return;
	event_type = header->msg_type;

	const auto* baseId = hb::net::PacketCast<hb::net::PacketEventMotionBaseId>(packet_data, sizeof(hb::net::PacketEventMotionBaseId));
	if (!baseId) return;
	object_id = baseId->object_id;

	if (hb::shared::object_id::IsNpcID(object_id)) {
		m_last_npc_event_time = GameClock::get_time_ms();
	}

	if (!hb::shared::object_id::IsNearbyOffset(object_id))
	{
		if (hb::shared::object_id::is_player_id(object_id)) 	// Player
		{
			const auto* pkt = hb::net::PacketCast<hb::net::PacketEventMotionPlayer>(packet_data, sizeof(hb::net::PacketEventMotionPlayer));
			if (!pkt) return;
			map_x = pkt->x;
			map_y = pkt->y;
			owner_type = pkt->type;
			move_dir = static_cast<direction>(pkt->dir);
			name.assign(pkt->name, strnlen(pkt->name, sizeof(pkt->name)));
			appearance = pkt->appearance;
			status = pkt->status;
			location = pkt->loc;
		}
		else 	// Npc or mob
		{
			const auto* pkt = hb::net::PacketCast<hb::net::PacketEventMotionNpc>(packet_data, sizeof(hb::net::PacketEventMotionNpc));
			if (!pkt) return;
			map_x = pkt->x;
			map_y = pkt->y;
			npc_config_id = pkt->config_id;
			owner_type = resolve_npc_type(npc_config_id);
			move_dir = static_cast<direction>(pkt->dir);
			name.assign(pkt->name, strnlen(pkt->name, sizeof(pkt->name)));
			appearance.SetFromNpcAppearance(pkt->appearance);
			status.SetFromEntityStatus(pkt->status);
			location = pkt->loc;
		}
	}
	else
	{
		switch (event_type) {
		case Type::Move:
		case Type::Run:
		{
			const auto* pkt = hb::net::PacketCast<hb::net::PacketEventMotionDirOnly>(packet_data, sizeof(hb::net::PacketEventMotionDirOnly));
			if (!pkt) return;
			move_dir = static_cast<direction>(pkt->dir);
			map_x = -1;
			map_y = -1;
		}
		break;

		case Type::Magic:
		case Type::Damage:
		case Type::DamageMove:
		{
			const auto* pkt = hb::net::PacketCast<hb::net::PacketEventMotionShort>(packet_data, sizeof(hb::net::PacketEventMotionShort));
			if (!pkt) return;
			move_dir = static_cast<direction>(pkt->dir);
			value1 = pkt->v1; // Damage or 0
			value2 = pkt->v2;
			map_x = -1;
			map_y = -1;
		}
		break;

		case Type::Dying:
		{
			const auto* pkt = hb::net::PacketCast<hb::net::PacketEventMotionMove>(packet_data, sizeof(hb::net::PacketEventMotionMove));
			if (!pkt) return;
			move_dir = static_cast<direction>(pkt->dir);
			value1 = pkt->v1;
			value2 = pkt->v2;
			map_x = pkt->x;
			map_y = pkt->y;
		}
		break;

		case Type::Attack:
		case Type::AttackMove:
		{
			const auto* pkt = hb::net::PacketCast<hb::net::PacketEventMotionAttack>(packet_data, sizeof(hb::net::PacketEventMotionAttack));
			if (!pkt) return;
			move_dir = static_cast<direction>(pkt->dir);
			value1 = pkt->v1;
			value2 = pkt->v2;
			value3 = pkt->v3;
		}
		break;

		default:
		{
			const auto* pkt = hb::net::PacketCast<hb::net::PacketEventMotionDirOnly>(packet_data, sizeof(hb::net::PacketEventMotionDirOnly));
			if (!pkt) return;
			move_dir = static_cast<direction>(pkt->dir);
		}
		break;
		}
	}

	if ((event_type == Type::NullAction) && (name == m_player->m_player_name))
	{
		m_player->m_player_type = owner_type;
		prev_combat_mode = m_player->m_playerAppearance.is_walking;
		m_player->m_playerAppearance = appearance;
		m_player->m_playerStatus = status;
		m_player->m_is_gm_mode = m_player->m_playerStatus.gm_mode;
		if (!prev_combat_mode)
		{
			if (appearance.is_walking)
			{
				add_event_list(MOTION_EVENT_HANDLER1, 10);
				m_player->m_is_combat_mode = true;
			}
		}
		else
		{
			if (!appearance.is_walking)
			{
				add_event_list(MOTION_EVENT_HANDLER2, 10);
				m_player->m_is_combat_mode = false;
			}
		}
		if (m_player->m_Controller.get_command() != Type::Run && m_player->m_Controller.get_command() != Type::Move) { m_map_data->set_owner(object_id, map_x, map_y, owner_type, move_dir, appearance, status, name, (char)event_type, value1, value2, value3, location, 0, npc_config_id); }
	}
	else { m_map_data->set_owner(object_id, map_x, map_y, owner_type, move_dir, appearance, status, name, (char)event_type, value1, value2, value3, location, 0, npc_config_id); }

	switch (event_type) {
	case Type::Magic: // Casting
		get_floating_text().remove_by_object_id(hb::shared::object_id::ToRealID(object_id));
		{
			text = std::format("{}!", m_magic_cfg_list[value1]->m_name);
			get_floating_text().add_notify_text(notify_text_type::magic_cast_name, text, m_cur_time,
				hb::shared::object_id::ToRealID(object_id), m_map_data.get());
		}
		break;

	case Type::Dying:
		get_floating_text().remove_by_object_id(hb::shared::object_id::ToRealID(object_id));
		get_floating_text().add_damage_from_value(value1, true, m_cur_time,
			hb::shared::object_id::ToRealID(object_id), m_map_data.get());
		break;

	case Type::Damage:
	case Type::DamageMove:
		if (name == m_player->m_player_name)
		{
			// Cancel spell casting if in the animation phase (Type::Magic)
			if (m_player->m_Controller.get_command() == Type::Magic)
				m_player->m_Controller.set_command(Type::stop);
			on_game()->m_is_get_pointing_mode = false;
			on_game()->m_point_command_type = -1;
			clear_skill_using_status();
			// Lock the controller until the damage animation finishes.
			// Without this, quick actions allows immediate movement after being hit.
			m_player->m_Controller.set_command(Type::stop);
			m_player->m_Controller.set_command_available(false);
			m_player->m_Controller.set_command_time(GameClock::get_time_ms());
		}
		get_floating_text().remove_by_object_id(hb::shared::object_id::ToRealID(object_id));
		get_floating_text().add_damage_from_value(value1, false, m_cur_time,
			hb::shared::object_id::ToRealID(object_id), m_map_data.get());
		break;

	case Type::Attack:
	case Type::AttackMove:
		if (object_id == m_player->m_player_object_id + hb::shared::object_id::NearbyOffset)
		{
			if (m_magic_cfg_list[value3] != 0)
			{
				text = m_magic_cfg_list[value3]->m_name;
				add_event_list(text.c_str(), 10);
			}
		}
		break;
	}
}

void CGame::grand_magic_result(const char* map_name, int ares_crusade_points, int elv_crusade_points, int ares_industry_points, int elv_industry_points, int ares_crusade_casualties, int ares_industry_casualties, int elv_crusade_casualties, int elv_industry_casualties)
{
	int text_index = 0;
	char temp[120]{};

	for (int i = 0; i < game_limits::max_text_dlg_lines; i++)
	{
		if (on_game()->m_msg_text_list[i] != 0)
			on_game()->m_msg_text_list[i].reset();
	}

	for (int i = 0; i < 92; i++)
		if (m_game_msg_list[i] == 0) return;

	if (strcmp(map_name, "aresden") == 0)
	{
		on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[2]->m_pMsg, 0);
		on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[3]->m_pMsg, 0);
		on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, " ", 0);

		std::snprintf(temp, sizeof(temp), "%s %d", m_game_msg_list[4]->m_pMsg, ares_crusade_points);
		on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, temp, 0);

		std::snprintf(temp, sizeof(temp), "%s %d", m_game_msg_list[5]->m_pMsg, elv_crusade_points);
		on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, temp, 0);

		std::snprintf(temp, sizeof(temp), "%s %d", m_game_msg_list[6]->m_pMsg, ares_industry_points);
		on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, temp, 0);

		std::snprintf(temp, sizeof(temp), "%s %d", m_game_msg_list[58]->m_pMsg, elv_industry_points);
		on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, temp, 0);
		on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, " ", 0);

		std::snprintf(temp, sizeof(temp), "%s %d %d %d %d", NOTIFY_MSG_STRUCTURE_HP, ares_crusade_casualties, ares_industry_casualties, elv_crusade_casualties, elv_industry_casualties);
		on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, temp, 0);
		on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, " ", 0);

		if (elv_crusade_points == 0) {
			if ((m_player->m_citizen == true) && (m_player->m_aresden == false))
			{
				audio_manager::get().play_game_sound(sound_type::effect, 25, 0, 0);
				on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[59]->m_pMsg, 0);
				on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[60]->m_pMsg, 0);
				on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[61]->m_pMsg, 0);
				on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[62]->m_pMsg, 0);
				for (int i = text_index; i < 18; i++) on_game()->m_msg_text_list[i] = std::make_unique<CMsg>(0, " ", 0);
			}
			else if ((m_player->m_citizen == true) && (m_player->m_aresden == true))
			{
				audio_manager::get().play_game_sound(sound_type::effect, 25, 0, 0);
				on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[69]->m_pMsg, 0);
				on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[70]->m_pMsg, 0);
				on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[71]->m_pMsg, 0);
				on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[72]->m_pMsg, 0);
				on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[73]->m_pMsg, 0);
				on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[74]->m_pMsg, 0);
				for (int i = text_index; i < 18; i++) on_game()->m_msg_text_list[i] = std::make_unique<CMsg>(0, " ", 0);
			}
			else audio_manager::get().play_game_sound(sound_type::effect, 25, 0, 0);
		}
		else
		{
			if (ares_crusade_points != 0)
			{
				if ((m_player->m_citizen == true) && (m_player->m_aresden == false))
				{
					audio_manager::get().play_game_sound(sound_type::effect, 23, 0, 0);
					audio_manager::get().play_game_sound(sound_type::character, 21, 0, 0);
					audio_manager::get().play_game_sound(sound_type::character, 22, 0, 0);
					on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[63]->m_pMsg, 0);
					on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[64]->m_pMsg, 0);
					on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[65]->m_pMsg, 0);
					for (int i = text_index; i < 18; i++) on_game()->m_msg_text_list[i] = std::make_unique<CMsg>(0, " ", 0);
				}
				else if ((m_player->m_citizen == true) && (m_player->m_aresden == true))
				{
					audio_manager::get().play_game_sound(sound_type::effect, 24, 0, 0);
					audio_manager::get().play_game_sound(sound_type::character, 12, 0, 0);
					audio_manager::get().play_game_sound(sound_type::character, 13, 0, 0);
					on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[75]->m_pMsg, 0);
					on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[76]->m_pMsg, 0);
					on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[77]->m_pMsg, 0);
					on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[78]->m_pMsg, 0);
					on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[79]->m_pMsg, 0);
					on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[80]->m_pMsg, 0);
					on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[81]->m_pMsg, 0);
					on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[82]->m_pMsg, 0);
					for (int i = text_index; i < 18; i++) on_game()->m_msg_text_list[i] = std::make_unique<CMsg>(0, " ", 0);
				}
				else audio_manager::get().play_game_sound(sound_type::effect, 25, 0, 0);
			}
			else
			{
				if ((m_player->m_citizen == true) && (m_player->m_aresden == false))
				{
					audio_manager::get().play_game_sound(sound_type::effect, 23, 0, 0);
					on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[66]->m_pMsg, 0);
					on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[67]->m_pMsg, 0);
					on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[68]->m_pMsg, 0);
					for (int i = text_index; i < 18; i++) on_game()->m_msg_text_list[i] = std::make_unique<CMsg>(0, " ", 0);
				}
				else if ((m_player->m_citizen == true) && (m_player->m_aresden == true))
				{
					audio_manager::get().play_game_sound(sound_type::effect, 24, 0, 0);
					on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[83]->m_pMsg, 0);
					on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[84]->m_pMsg, 0);
					on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[85]->m_pMsg, 0);
					on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[86]->m_pMsg, 0);
					on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[87]->m_pMsg, 0);
					on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[88]->m_pMsg, 0);
					on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[89]->m_pMsg, 0);
					on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[90]->m_pMsg, 0);
					for (int i = text_index; i < 18; i++) on_game()->m_msg_text_list[i] = std::make_unique<CMsg>(0, " ", 0);
				}
				else audio_manager::get().play_game_sound(sound_type::effect, 25, 0, 0);
			}
		}
	}
	else if (strcmp(map_name, "elvine") == 0)
	{
		on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[7]->m_pMsg, 0);
		on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[8]->m_pMsg, 0);
		on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, " ", 0);

		std::snprintf(temp, sizeof(temp), "%s %d", m_game_msg_list[4]->m_pMsg, ares_crusade_points);
		on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, temp, 0);

		std::snprintf(temp, sizeof(temp), "%s %d", m_game_msg_list[5]->m_pMsg, elv_crusade_points);
		on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, temp, 0);

		std::snprintf(temp, sizeof(temp), "%s %d", m_game_msg_list[6]->m_pMsg, ares_industry_points);
		on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, temp, 0);

		std::snprintf(temp, sizeof(temp), "%s %d", m_game_msg_list[58]->m_pMsg, elv_industry_points);
		on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, temp, 0);
		on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, " ", 0);

		std::snprintf(temp, sizeof(temp), "%s %d %d %d %d", NOTIFY_MSG_STRUCTURE_HP, ares_crusade_casualties, ares_industry_casualties, elv_crusade_casualties, elv_industry_casualties);
		on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, temp, 0);
		on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, " ", 0);

		if (elv_crusade_points == 0) {
			if ((m_player->m_citizen == true) && (m_player->m_aresden == true))
			{
				audio_manager::get().play_game_sound(sound_type::effect, 25, 0, 0);
				on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[59]->m_pMsg, 0);
				on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[60]->m_pMsg, 0);
				on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[61]->m_pMsg, 0);
				on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[62]->m_pMsg, 0);
				for (int i = text_index; i < 18; i++) on_game()->m_msg_text_list[i] = std::make_unique<CMsg>(0, " ", 0);
			}
			else if ((m_player->m_citizen == true) && (m_player->m_aresden == false))
			{
				audio_manager::get().play_game_sound(sound_type::effect, 25, 0, 0);
				on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[69]->m_pMsg, 0);
				on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[70]->m_pMsg, 0);
				on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[71]->m_pMsg, 0);
				on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[72]->m_pMsg, 0);
				on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[73]->m_pMsg, 0);
				on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[74]->m_pMsg, 0);
				for (int i = text_index; i < 18; i++) on_game()->m_msg_text_list[i] = std::make_unique<CMsg>(0, " ", 0);
			}
			else audio_manager::get().play_game_sound(sound_type::effect, 25, 0, 0);
		}
		else
		{
			if (ares_crusade_points != 0) {
				if ((m_player->m_citizen == true) && (m_player->m_aresden == true))
				{
					audio_manager::get().play_game_sound(sound_type::effect, 23, 0, 0);
					audio_manager::get().play_game_sound(sound_type::character, 21, 0, 0);
					audio_manager::get().play_game_sound(sound_type::character, 22, 0, 0);
					on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[63]->m_pMsg, 0);
					on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[64]->m_pMsg, 0);
					on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[65]->m_pMsg, 0);
					for (int i = text_index; i < 18; i++) on_game()->m_msg_text_list[i] = std::make_unique<CMsg>(0, " ", 0);
				}
				else if ((m_player->m_citizen == true) && (m_player->m_aresden == false))
				{
					audio_manager::get().play_game_sound(sound_type::effect, 24, 0, 0);
					audio_manager::get().play_game_sound(sound_type::character, 12, 0, 0);
					audio_manager::get().play_game_sound(sound_type::character, 13, 0, 0);
					on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[75]->m_pMsg, 0);
					on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[76]->m_pMsg, 0);
					on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[77]->m_pMsg, 0);
					on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[78]->m_pMsg, 0);
					on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[79]->m_pMsg, 0);
					on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[80]->m_pMsg, 0);
					on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[81]->m_pMsg, 0);
					on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[82]->m_pMsg, 0);
					for (int i = text_index; i < 18; i++) on_game()->m_msg_text_list[i] = std::make_unique<CMsg>(0, " ", 0);
				}
				else audio_manager::get().play_game_sound(sound_type::effect, 25, 0, 0);
			}
			else
			{
				if ((m_player->m_citizen == true) && (m_player->m_aresden == true))
				{
					audio_manager::get().play_game_sound(sound_type::effect, 23, 0, 0);
					on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[66]->m_pMsg, 0);
					on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[67]->m_pMsg, 0);
					on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[68]->m_pMsg, 0);
					for (int i = text_index; i < 18; i++) on_game()->m_msg_text_list[i] = std::make_unique<CMsg>(0, " ", 0);
				}
				else if ((m_player->m_citizen == true) && (m_player->m_aresden == false))
				{
					audio_manager::get().play_game_sound(sound_type::effect, 24, 0, 0);
					on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[83]->m_pMsg, 0);
					on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[84]->m_pMsg, 0);
					on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[85]->m_pMsg, 0);
					on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[86]->m_pMsg, 0);
					on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[87]->m_pMsg, 0);
					on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[88]->m_pMsg, 0);
					on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[89]->m_pMsg, 0);
					on_game()->m_msg_text_list[text_index++] = std::make_unique<CMsg>(0, m_game_msg_list[90]->m_pMsg, 0);
					for (int i = text_index; i < 18; i++) on_game()->m_msg_text_list[i] = std::make_unique<CMsg>(0, " ", 0);
				}
				else audio_manager::get().play_game_sound(sound_type::effect, 25, 0, 0);
			}
		}
	}

	get_dialog_box_manager().enable_dialog_box(DialogBoxId::Text, 0, 0, 0);
}

// num : 1 - F2, 2 - F3
void CGame::use_shortcut(int num)
{
	std::string G_cTxt;
	int index;
	if (num < 4) index = num;
	else index = num + 7;
	if (GameModeManager::get_mode() != GameMode::MainGame) return;
	if (hb::shared::input::is_ctrl_down() == true)
	{
		if (m_recent_short_cut == -1)
		{
			add_event_list(MSG_SHORTCUT1, 10);
			G_cTxt = std::format(MSG_SHORTCUT2, index);// [F%d]
			add_event_list(G_cTxt.c_str(), 10);
			G_cTxt = std::format(MSG_SHORTCUT3, index);// [Control]-[F%d]
			add_event_list(G_cTxt.c_str(), 10);
		}
		else
		{
			m_short_cut[num] = m_recent_short_cut;
			if (m_short_cut[num] < 100)
			{
				if (m_player->m_item_list[m_short_cut[num]] == 0)
				{
					m_short_cut[num] = -1;
					m_recent_short_cut = -1;
					return;
				}

				auto itemInfo2 = item_name_formatter::get().format(m_player->m_item_list[m_short_cut[num]].get());
				auto effect2 = itemInfo2.effect_text();
				auto extra2 = itemInfo2.extra_text();
				G_cTxt = std::format(MSG_SHORTCUT4, itemInfo2.name.c_str(), effect2.c_str(), extra2.c_str(), index);// (%s %s %s) [F%d]
				add_event_list(G_cTxt.c_str(), 10);
			}
			else if (m_short_cut[num] >= 100)
			{
				if (m_magic_cfg_list[m_short_cut[num] - 100] == 0)
				{
					m_short_cut[num] = -1;
					m_recent_short_cut = -1;
					return;
				}
				G_cTxt = std::format(MSG_SHORTCUT5, m_magic_cfg_list[m_short_cut[num] - 100]->m_name, index);// %s) [F%d])
				add_event_list(G_cTxt.c_str(), 10);
			}
		}
	}
	else
	{
		if (m_short_cut[num] == -1)
		{
			add_event_list(MSG_SHORTCUT1, 10);
			G_cTxt = std::format(MSG_SHORTCUT2, index);// [F%d]
			add_event_list(G_cTxt.c_str(), 10);
			G_cTxt = std::format(MSG_SHORTCUT3, index);// [Control]-[F%d]
			add_event_list(G_cTxt.c_str(), 10);
		}
		else if (m_short_cut[num] < 100)
		{
			inventory_manager::get().equip_item(static_cast<char>(m_short_cut[num]));
		}
		else if (m_short_cut[num] >= 100) magic_casting_system::get().begin_cast(m_short_cut[num] - 100);
	}
}

/*********************************************************************************************************************
**  void check_active_aura(short sX, short sY, uint32_t time, short owner_type)( initially Cleroth fixed by Snoopy )	**
**  description			: Generates special auras around players													**
**						: v351 implements this in each drawn function,beter to regroup in single function.			**
**********************************************************************************************************************/
void CGame::check_active_aura(short sX, short sY, uint32_t time, short owner_type)
{	// Used at the beginning of character drawing
	// DefenseShield
	if (m_entity_state.m_status.defense_shield)
		m_effect_sprites[80]->draw(sX + 75, sY + 107, m_entity_state.m_effect_frame % 17, hb::shared::sprite::DrawParams::alpha_blend(0.5f));

	// Protection From Magic
	if (m_entity_state.m_status.magic_protection)
		m_effect_sprites[79]->draw(sX + 101, sY + 135, m_entity_state.m_effect_frame % 15, hb::shared::sprite::DrawParams::alpha_blend(0.7f));

	// Protection From Arrow
	if (m_entity_state.m_status.protection_from_arrow)
		m_effect_sprites[72]->draw(sX, sY + 35, m_entity_state.m_effect_frame % 30, hb::shared::sprite::DrawParams::alpha_blend(0.7f));

	// Illusion
	if (m_entity_state.m_status.illusion)
		m_effect_sprites[73]->draw(sX + 125, sY + 130 - entity_visual::attacker_height[owner_type], m_entity_state.m_effect_frame % 24, hb::shared::sprite::DrawParams::alpha_blend(0.7f));

	// Illusion movement
	if ((m_entity_state.m_status.illusion_movement) != 0)
		m_effect_sprites[151]->draw(sX + 90, sY + 90 - entity_visual::attacker_height[owner_type], m_entity_state.m_effect_frame % 24, hb::shared::sprite::DrawParams::alpha_blend(0.7f));

	// Slate red (HP)
	if (m_entity_state.m_status.slate_invincible)
		m_effect_sprites[149]->draw(sX + 90, sY + 120, m_entity_state.m_effect_frame % 15, hb::shared::sprite::DrawParams::alpha_blend(0.7f));

	// Slate Blue (Mana)
	if (m_entity_state.m_status.slate_mana)
		m_effect_sprites[150]->draw(sX + 1, sY + 26, m_entity_state.m_effect_frame % 15, hb::shared::sprite::DrawParams::alpha_blend(0.7f));

	// Slate Green (XP)
	if (m_entity_state.m_status.slate_exp)
		m_effect_sprites[148]->draw(sX, sY + 32, m_entity_state.m_effect_frame % 23, hb::shared::sprite::DrawParams::alpha_blend(0.7f));

	// Hero Flag (Heldenian)
	if (m_entity_state.m_status.hero)
		m_effect_sprites[87]->draw(sX + 53, sY + 54, m_entity_state.m_effect_frame % 29, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
}

/*********************************************************************************************************************
**  void check_active_aura2(short sX, short sY, uint32_t time,  m_entity_state.m_owner_type) ( initially Cleroth fixed by Snoopy )	**
**  description			: Generates poison aura around players. This one should be use later...						**
**						: v351 implements this in each drawn function,beter to regroup in single function.			**
**********************************************************************************************************************/
void CGame::check_active_aura2(short sX, short sY, uint32_t time, short owner_type)
{	// Poison
	if (m_entity_state.m_status.poisoned)
		m_effect_sprites[81]->draw(sX + 115, sY + 120 - entity_visual::attacker_height[owner_type], m_entity_state.m_effect_frame % 21, hb::shared::sprite::DrawParams::alpha_blend(0.7f));
}

// draw_angel REMOVED — moved to Screen_OnGame
/*********************************************************************************************************************
**  int CGame::has_hero_set( short m_sAppr3, short m_sAppr3, char OwnerType)		( Snoopy )							**
**  description			:: check weather the object (is character) is using a hero set (1:war, 2:mage)				**
**********************************************************************************************************************/
int CGame::has_hero_set(const hb::shared::entity::PlayerAppearance& appr, short OwnerType)
{
	// Look up set_id for each armor slot via item config
	auto get_set = [this](int16_t item_id) -> int16_t {
		if (item_id <= 0) return 0;
		CItem* cfg = get_item_config(item_id);
		return cfg ? cfg->m_set_id : 0;
	};

	int16_t armor_set = get_set(appr.armor_item_id);
	if (armor_set == 0) return 0;
	int16_t leg_set = get_set(appr.pants_item_id);
	int16_t hat_set = get_set(appr.helm_item_id);
	int16_t berk_set = get_set(appr.arm_item_id);

	// All four pieces must have the same non-zero set_id
	if (armor_set == leg_set && armor_set == hat_set && armor_set == berk_set)
		return armor_set; // set_id encodes set type: 1=warrior, 2=mage
	return 0;
}
/*********************************************************************************************************************
**  void show_heldenian_victory( short side)				( Snoopy )													**
**  description			: Shows the Heldenian's End window															**
**********************************************************************************************************************/
void CGame::show_heldenian_victory(short side)
{
	int player_side = 0;
	get_dialog_box_manager().disable_dialog_box(DialogBoxId::Text);
	for (int i = 0; i < game_limits::max_text_dlg_lines; i++)
	{
		if (on_game()->m_msg_text_list[i] != 0)
			on_game()->m_msg_text_list[i].reset();
	}
	if (m_player->m_citizen == false) player_side = 0;
	else if (m_player->m_aresden == true) player_side = 1;
	else if (m_player->m_aresden == false) player_side = 2;
	switch (side) {
	case 0:
		audio_manager::get().play_game_sound(sound_type::effect, 25, 0, 0);
		on_game()->m_msg_text_list[0] = std::make_unique<CMsg>(0, "Heldenian holy war has been closed!", 0);
		on_game()->m_msg_text_list[1] = std::make_unique<CMsg>(0, " ", 0);
		on_game()->m_msg_text_list[2] = std::make_unique<CMsg>(0, "Heldenian Holy war ended", 0);
		on_game()->m_msg_text_list[3] = std::make_unique<CMsg>(0, "in a tie.", 0);
		break;
	case 1:
		audio_manager::get().play_game_sound(sound_type::effect, 25, 0, 0);
		on_game()->m_msg_text_list[0] = std::make_unique<CMsg>(0, "Heldenian holy war has been closed!", 0);
		on_game()->m_msg_text_list[1] = std::make_unique<CMsg>(0, " ", 0);
		on_game()->m_msg_text_list[2] = std::make_unique<CMsg>(0, "Heldenian Holy war ended", 0);
		on_game()->m_msg_text_list[3] = std::make_unique<CMsg>(0, "in favor of Aresden.", 0);
		break;
	case 2:
		audio_manager::get().play_game_sound(sound_type::effect, 25, 0, 0);
		on_game()->m_msg_text_list[0] = std::make_unique<CMsg>(0, "Heldenian holy war has been closed!", 0);
		on_game()->m_msg_text_list[1] = std::make_unique<CMsg>(0, " ", 0);
		on_game()->m_msg_text_list[2] = std::make_unique<CMsg>(0, "Heldenian Holy war ended", 0);
		on_game()->m_msg_text_list[3] = std::make_unique<CMsg>(0, "in favor of Elvine.", 0);
		break;
	}
	on_game()->m_msg_text_list[4] = std::make_unique<CMsg>(0, " ", 0);

	if (((player_side != 1) && (player_side != 2))   // Player not a normal citizen
		|| (side == 0))								// or no winner
	{
		audio_manager::get().play_game_sound(sound_type::effect, 25, 0, 0);
		on_game()->m_msg_text_list[5] = std::make_unique<CMsg>(0, " ", 0);
		on_game()->m_msg_text_list[6] = std::make_unique<CMsg>(0, " ", 0);
		on_game()->m_msg_text_list[7] = std::make_unique<CMsg>(0, " ", 0);
		on_game()->m_msg_text_list[8] = std::make_unique<CMsg>(0, " ", 0);
	}
	else
	{
		if (side == player_side)
		{
			audio_manager::get().play_game_sound(sound_type::effect, 23, 0, 0);
			audio_manager::get().play_game_sound(sound_type::character, 21, 0, 0);
			audio_manager::get().play_game_sound(sound_type::character, 22, 0, 0);
			on_game()->m_msg_text_list[5] = std::make_unique<CMsg>(0, "Congratulation.", 0);
			on_game()->m_msg_text_list[6] = std::make_unique<CMsg>(0, "As cityzen of victory,", 0);
			on_game()->m_msg_text_list[7] = std::make_unique<CMsg>(0, "You will recieve a reward.", 0);
			on_game()->m_msg_text_list[8] = std::make_unique<CMsg>(0, "      ", 0);
		}
		else
		{
			audio_manager::get().play_game_sound(sound_type::effect, 24, 0, 0);
			audio_manager::get().play_game_sound(sound_type::character, 12, 0, 0);
			audio_manager::get().play_game_sound(sound_type::character, 13, 0, 0);
			on_game()->m_msg_text_list[5] = std::make_unique<CMsg>(0, "To our regret", 0);
			on_game()->m_msg_text_list[6] = std::make_unique<CMsg>(0, "As cityzen of defeat,", 0);
			on_game()->m_msg_text_list[7] = std::make_unique<CMsg>(0, "You cannot recieve any reward.", 0);
			on_game()->m_msg_text_list[8] = std::make_unique<CMsg>(0, "     ", 0);
		}
	}
	for (int i = 9; i < 18; i++)
		on_game()->m_msg_text_list[i] = std::make_unique<CMsg>(0, " ", 0);
	get_dialog_box_manager().enable_dialog_box(DialogBoxId::Text, 0, 0, 0);
	get_dialog_box_manager().disable_crusade_dialogs();
}

/*********************************************************************************************************************
**  void 	ResponseHeldenianTeleportList(char *data)									(  Snoopy )					**
**  description			: Gail's TP																					**
**********************************************************************************************************************/
// dk_glare REMOVED — moved to Screen_OnGame
// abaddon_corpse REMOVED — moved to Screen_OnGame
