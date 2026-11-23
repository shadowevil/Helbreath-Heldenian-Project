#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <string>

class CGame;

struct TeleportEntry
{
	int index = 0;
	std::string mapname;
	int x = 0;
	int y = 0;
	int cost = 0;
};

enum class teleport_state : uint8_t {
	idle,            // No teleport in progress
	awaiting_auth,   // Pre-auth sent, waiting for server response
	fading_out,      // Approved, fading screen to black
	awaiting_data,   // Black screen, RequestTeleport sent, waiting for ResponseInitData
	transitioning    // Map loaded, GameModeManager handling fade-in
};

class teleport_manager
{
public:
	static teleport_manager& get();
	void set_game(CGame* game);
	void reset();

	// Response handlers (moved from CGame)
	void handle_teleport_list(char* data);
	void handle_charged_teleport(char* data);
	void handle_heldenian_teleport_list(char* data);

	// Teleport list access
	int get_map_count() const { return m_map_count; }
	void set_map_count(int count) { m_map_count = count; }
	auto& get_list() { return m_list; }
	const auto& get_list() const { return m_list; }

	// Crusade teleport location
	int get_loc_x() const { return m_loc_x; }
	int get_loc_y() const { return m_loc_y; }
	void set_location(int x, int y) { m_loc_x = x; m_loc_y = y; }
	const char* get_map_name() { return m_map_name.c_str(); }
	const char* get_map_name() const { return m_map_name.c_str(); }
	void set_map_name(const char* src, size_t len) { m_map_name.assign(src, len); }

	// Request state
	bool is_requested() const { return m_is_requested; }
	void set_requested(bool val) { m_is_requested = val; }

	// Pre-auth + fade state machine
	void request_auth(short player_x, short player_y);
	void on_auth_approved();
	void on_auth_rejected();
	void on_map_loaded();
	void update();
	float get_fade_alpha() const { return m_fade_alpha; }
	bool is_active() const { return m_state != teleport_state::idle; }
	teleport_state get_state() const { return m_state; }
	bool is_rejected_tile(short x, short y) const { return m_rejected_x == x && m_rejected_y == y; }

private:
	teleport_manager();
	~teleport_manager();

	CGame* m_game = nullptr;
	int m_map_count = 0;
	std::array<TeleportEntry, 20> m_list{};
	bool m_is_requested = false;
	int m_loc_x = -1;
	int m_loc_y = -1;
	std::string m_map_name;

	// Pre-auth fade state
	teleport_state m_state = teleport_state::idle;
	float m_fade_alpha = 0.0f;
	uint32_t m_fade_start_time = 0;
	short m_rejected_x = -1;
	short m_rejected_y = -1;
	static constexpr float FADE_DURATION_MS = 150.0f;
	static constexpr uint32_t AUTH_TIMEOUT_MS = 2000;
};
