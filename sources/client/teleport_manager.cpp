#include "teleport_manager.h"
#include "game.h"
#include "game_mode_manager.h"
#include "packet/packet_response.h"
#include "packet/packet_helpers.h"
#include "lan_eng.h"
#include "packet/shared_packets.h"

using namespace hb::shared::net;
using namespace hb::client::net;

teleport_manager::teleport_manager() = default;
teleport_manager::~teleport_manager() = default;

teleport_manager& teleport_manager::get()
{
	static teleport_manager instance;
	return instance;
}

void teleport_manager::set_game(CGame* game)
{
	m_game = game;
}

void teleport_manager::reset()
{
	m_is_requested = false;
	m_map_count = 0;
	m_list = {};
	m_loc_x = -1;
	m_loc_y = -1;
	m_state = teleport_state::idle;
	m_fade_alpha = 0.0f;
	m_fade_start_time = 0;
	m_rejected_x = -1;
	m_rejected_y = -1;
}

void teleport_manager::request_auth(short player_x, short player_y)
{
	if (m_is_requested || is_active()) return;
	if (is_rejected_tile(player_x, player_y)) return;

	m_rejected_x = player_x;
	m_rejected_y = player_y;
	{
		hb::net::PacketRequestHeaderOnly req{};
		req.header.msg_id = MsgId::RequestTeleportAuth;
		req.header.msg_type = 0;
		m_game->send_game_packet(req);
	}
	m_is_requested = true;
	m_state = teleport_state::awaiting_auth;
	m_fade_alpha = 0.0f;
	m_fade_start_time = GameClock::get_time_ms();
}

void teleport_manager::on_auth_approved()
{
	if (m_state != teleport_state::awaiting_auth) return;

	m_rejected_x = -1;
	m_rejected_y = -1;
	m_state = teleport_state::fading_out;
	m_fade_start_time = GameClock::get_time_ms();
	m_fade_alpha = 0.0f;
}

void teleport_manager::on_auth_rejected()
{
	m_state = teleport_state::idle;
	m_fade_alpha = 0.0f;
	m_is_requested = false;
}

void teleport_manager::on_map_loaded()
{
	if (m_state != teleport_state::awaiting_data) return;
	m_state = teleport_state::transitioning;

	// Screen is already black — skip GameModeManager's FadeOut, use fast FadeIn
	GameModeManager::set_transition_config({0.0f, FADE_DURATION_MS / 1000.0f});
}

void teleport_manager::update()
{
	if (m_state == teleport_state::idle) return;

	uint32_t now = GameClock::get_time_ms();

	switch (m_state) {
	case teleport_state::awaiting_auth:
	{
		uint32_t elapsed = now - m_fade_start_time;
		if (elapsed > AUTH_TIMEOUT_MS) {
			on_auth_rejected();
		}
		break;
	}

	case teleport_state::fading_out:
	{
		float elapsed = static_cast<float>(now - m_fade_start_time);
		m_fade_alpha = elapsed / FADE_DURATION_MS;
		if (m_fade_alpha >= 1.0f) {
			m_fade_alpha = 1.0f;
			// Screen is fully black — send the actual teleport request
			{
		hb::net::PacketRequestHeaderOnly req{};
		req.header.msg_id = MsgId::RequestTeleport;
		req.header.msg_type = MsgType::Confirm;
		m_game->send_game_packet(req, false);
	}
	teleport_manager::get().set_requested(true);
			m_game->change_game_mode(GameMode::WaitingInitData);
			m_state = teleport_state::awaiting_data;
		}
		break;
	}

	case teleport_state::awaiting_data:
		m_fade_alpha = 1.0f;
		break;

	case teleport_state::transitioning:
	{
		auto ts = GameModeManager::get_transition_state();
		if (ts == TransitionState::FadeIn || ts == TransitionState::None) {
			m_fade_alpha = 0.0f;
			m_state = teleport_state::idle;
			m_is_requested = false;
			// Restore default transition config for non-teleport screen changes
			GameModeManager::set_transition_config({GameModeManager::DEFAULT_FADE_DURATION, GameModeManager::DEFAULT_FADE_DURATION});
		}
		break;
	}

	default:
		break;
	}
}

void teleport_manager::handle_teleport_list(char* data)
{
	int i;
#ifdef _DEBUG
	m_game->add_event_list("Teleport ???", 10);
#endif
	const auto* header = hb::net::PacketCast<hb::net::PacketResponseTeleportListHeader>(
		data, sizeof(hb::net::PacketResponseTeleportListHeader));
	if (!header) return;
	const auto* entries = reinterpret_cast<const hb::net::PacketResponseTeleportListEntry*>(
		data + sizeof(hb::net::PacketResponseTeleportListHeader));
	m_map_count = header->count;
	if (m_map_count < 0) m_map_count = 0;
	if (m_map_count > static_cast<int>(m_list.size())) m_map_count = static_cast<int>(m_list.size());
	for (i = 0; i < m_map_count; i++)
	{
		m_list[i].index = entries[i].index;
		m_list[i].mapname.assign(entries[i].map_name, strnlen(entries[i].map_name, 10));
		m_list[i].x = entries[i].x;
		m_list[i].y = entries[i].y;
		m_list[i].cost = entries[i].cost;
	}
}

void teleport_manager::handle_charged_teleport(char* data)
{
	short reject_reason = 0;
	const auto* pkt = hb::net::PacketCast<hb::net::PacketResponseChargedTeleport>(
		data, sizeof(hb::net::PacketResponseChargedTeleport));
	if (!pkt) return;
	reject_reason = pkt->reason;

#ifdef _DEBUG
	m_game->add_event_list("charged teleport ?", 10);
#endif

	switch (reject_reason) {
	case 1:
		m_game->add_event_list(RESPONSE_CHARGED_TELEPORT1, 10);
		break;
	case 2:
		m_game->add_event_list(RESPONSE_CHARGED_TELEPORT2, 10);
		break;
	case 3:
		m_game->add_event_list(RESPONSE_CHARGED_TELEPORT3, 10);
		break;
	case 4:
		m_game->add_event_list(RESPONSE_CHARGED_TELEPORT4, 10);
		break;
	case 5:
		m_game->add_event_list(RESPONSE_CHARGED_TELEPORT5, 10);
		break;
	case 6:
		m_game->add_event_list(RESPONSE_CHARGED_TELEPORT6, 10);
		break;
	default:
		m_game->add_event_list(RESPONSE_CHARGED_TELEPORT7, 10);
	}
}

void teleport_manager::handle_heldenian_teleport_list(char* data)
{
	int i;
#ifdef _DEBUG
	m_game->add_event_list("Teleport ???", 10);
#endif
	const auto* header = hb::net::PacketCast<hb::net::PacketResponseTeleportListHeader>(
		data, sizeof(hb::net::PacketResponseTeleportListHeader));
	if (!header) return;
	const auto* entries = reinterpret_cast<const hb::net::PacketResponseTeleportListEntry*>(
		data + sizeof(hb::net::PacketResponseTeleportListHeader));
	m_map_count = header->count;
	if (m_map_count < 0) m_map_count = 0;
	if (m_map_count > static_cast<int>(m_list.size())) m_map_count = static_cast<int>(m_list.size());
	for (i = 0; i < m_map_count; i++)
	{
		m_list[i].index = entries[i].index;
		m_list[i].mapname.assign(entries[i].map_name, strnlen(entries[i].map_name, 10));
		m_list[i].x = entries[i].x;
		m_list[i].y = entries[i].y;
		m_list[i].cost = entries[i].cost;
	}
}
