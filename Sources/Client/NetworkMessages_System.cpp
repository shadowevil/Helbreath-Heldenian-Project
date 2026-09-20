#include "Game.h"
#include "FloatingTextManager.h"
#include "GameModeManager.h"
#include "AudioManager.h"
#include "WeatherManager.h"
#include "TeleportManager.h"

#include "NetworkMessageManager.h"
#include "Packet/SharedPackets.h"
#include "lan_eng.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <format>
#include <string>
#include "Screen_OnGame.h"

using namespace hb::shared::net;
namespace NetworkMessageHandlers {

void HandleWeatherChange(CGame* game, char* data)
{
	const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyWhetherChange>(
		data, sizeof(hb::net::PacketNotifyWhetherChange));
	if (!pkt) return;
	char weather_status = static_cast<char>(pkt->status);
	weather_manager::get().set_weather_status(weather_status);

	if (weather_status != 0)
	{
		weather_manager::get().set_weather(true, weather_status);
		if (weather_status >= 4 && weather_status <= 6 && audio_manager::get().is_music_enabled())
			game->start_bgm();
	}
	else weather_manager::get().set_weather(false, 0);
}

void HandleTimeChange(CGame* game, char* data)
{
	const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyTimeChange>(
		data, sizeof(hb::net::PacketNotifyTimeChange));
	if (!pkt) return;
	weather_manager::get().set_ambient_light(static_cast<char>(pkt->sprite_alpha));
	switch (weather_manager::get().get_ambient_light()) {
	case 1:	game->on_game()->m_is_xmas = false; audio_manager::get().play_game_sound(sound_type::effect, 32, 0); break;
	case 2: game->on_game()->m_is_xmas = false; audio_manager::get().play_game_sound(sound_type::effect, 31, 0); break;
	case 3: // Snoopy Special night with chrismas bulbs
		if (weather_manager::get().is_snowing()) game->on_game()->m_is_xmas = true;
		else game->on_game()->m_is_xmas = false;
		weather_manager::get().set_xmas(game->on_game()->m_is_xmas);
		audio_manager::get().play_game_sound(sound_type::effect, 31, 0);
		weather_manager::get().set_ambient_light(2); break;
	}
}

void HandleNoticeMsg(CGame* game, char* data)
{
	if (teleport_manager::get().get_state() == teleport_state::awaiting_auth)
		teleport_manager::get().on_auth_rejected();
	const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyNoticeMsg>(
		data, sizeof(hb::net::PacketNotifyNoticeMsg));
	if (!pkt) return;
	game->add_event_list(pkt->text, 10);
}

void HandleStatusText(CGame* game, char* data)
{
	const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyStatusText>(
		data, sizeof(hb::net::PacketNotifyStatusText));
	if (!pkt) return;

	// Display floating text above the local player's head (like "* Failed! *")
	game->get_floating_text().add_damage_text(damage_text_type::Medium, pkt->text, game->m_cur_time,
		game->m_player->m_player_object_id, game->m_map_data.get());
}

void HandleForceDisconn(CGame* game, char* data)
{
	const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyForceDisconn>(
		data, sizeof(hb::net::PacketNotifyForceDisconn));
	if (!pkt) return;
	const auto wpCount = pkt->seconds;
	game->m_force_disconn = true;
	if (game->on_game()->m_logout_count < 0 || game->on_game()->m_logout_count > 5)
	{
		game->on_game()->m_logout_count = 5;
		game->on_game()->m_logout_count_time = GameClock::get_time_ms();
	}
	game->add_event_list(NOTIFYMSG_FORCE_DISCONN1, 10);
}

void HandleSettingSuccess(CGame* game, char* data)
{
	int prev_level;
	std::string txt;
	prev_level = game->m_player->m_level;
	const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyLevelUp>(
		data, sizeof(hb::net::PacketNotifyLevelUp));
	if (!pkt) return;
	game->m_player->m_level = pkt->level;
	game->m_player->m_str = pkt->str;
	game->m_player->m_vit = pkt->vit;
	game->m_player->m_dex = pkt->dex;
	game->m_player->m_int = pkt->intel;
	game->m_player->m_mag = pkt->mag;
	game->m_player->m_charisma = pkt->chr;
	game->m_player->m_playerStatus.attack_delay = pkt->attack_delay;
	txt = "Your stat has been changed.";
	game->add_event_list(txt.c_str(), 10);
	game->m_player->m_lu_str = game->m_player->m_lu_vit = game->m_player->m_lu_dex = game->m_player->m_lu_int = game->m_player->m_lu_mag = game->m_player->m_lu_char = 0;
}

void HandleServerChange(CGame* game, char* data)
{
	if (game->m_is_server_changing) return;

	char world_server_addr[16]{};
	int world_server_port;

	const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyServerChange>(
		data, sizeof(hb::net::PacketNotifyServerChange));
	if (!pkt) return;

	game->m_is_server_changing = true;


	game->m_map_name.assign(pkt->map_name, strnlen(pkt->map_name, sizeof(pkt->map_name)));
	memcpy(world_server_addr, pkt->log_server_addr, 15);
	world_server_port = pkt->log_server_port;
	if (game->m_g_sock != 0)
	{
		game->m_g_sock.reset();
	}
	if (game->m_l_sock != 0)
	{
		game->m_l_sock.reset();
	}
	game->m_l_sock = std::make_unique<hb::shared::net::ASIOSocket>(game->m_io_pool->get_context(), game_limits::socket_block_limit);
	game->m_l_sock->connect(game->m_log_server_addr.c_str(), world_server_port);
	game->m_l_sock->init_buffer_size(hb::shared::limits::MsgBufferSize);

	game->m_player->m_is_poisoned = false;

	game->m_enter_game_type = EnterGameMsg::NewToWlsButMls;

	// Build enter game packet for deferred send after connection establishes
	hb::net::EnterGameRequestFull req{};
	req.header.msg_id = MsgId::request_enter_game;
	req.header.msg_type = static_cast<uint16_t>(game->m_enter_game_type);
	std::snprintf(req.character_name, sizeof(req.character_name), "%s", game->m_selected_char_name.c_str());
	std::snprintf(req.map_name, sizeof(req.map_name), "%s", game->m_map_name.c_str());
	std::snprintf(req.account_name, sizeof(req.account_name), "%s", game->m_account_name.c_str());
	std::snprintf(req.password, sizeof(req.password), "%s", game->m_account_password.c_str());
	req.level = game->m_selected_char_level;
	std::snprintf(req.world_name, sizeof(req.world_name), "%s", game->m_world_server_name.c_str());
	req.version_major = hb::version::compatibility::major;
	req.version_minor = hb::version::compatibility::minor;
	req.version_patch = hb::version::compatibility::patch;
	game->set_pending_login_packet(req);

	game->change_game_mode(GameMode::Connecting);
	std::snprintf(game->m_msg, sizeof(game->m_msg), "%s", "55");
}

void HandleTotalUsers(CGame* game, char* data)
{
	int total;
	std::string txt;
	const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyTotalUsers>(
		data, sizeof(hb::net::PacketNotifyTotalUsers));
	if (!pkt) return;
	total = pkt->total;
	txt = std::format(NOTIFYMSG_TOTAL_USER1, total);
	game->add_event_list(txt.c_str(), 10);
}

void HandleChangePlayMode(CGame* game, char* data)
{
	const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyChangePlayMode>(
		data, sizeof(hb::net::PacketNotifyChangePlayMode));
	if (!pkt) return;
	game->m_location.assign(pkt->location, strnlen(pkt->location, sizeof(pkt->location)));

	if (game->m_location.starts_with("aresden"))
	{
		game->m_player->m_aresden = true;
		game->m_player->m_citizen = true;
		game->m_player->m_hunter = false;
	}
	else if (game->m_location.starts_with("arehunter"))
	{
		game->m_player->m_aresden = true;
		game->m_player->m_citizen = true;
		game->m_player->m_hunter = true;
	}
	else if (game->m_location.starts_with("elvine"))
	{
		game->m_player->m_aresden = false;
		game->m_player->m_citizen = true;
		game->m_player->m_hunter = false;
	}
	else if (game->m_location.starts_with("elvhunter"))
	{
		game->m_player->m_aresden = false;
		game->m_player->m_citizen = true;
		game->m_player->m_hunter = true;
	}
	else
	{
		game->m_player->m_aresden = true;
		game->m_player->m_citizen = false;
		game->m_player->m_hunter = true;
	}
	game->add_event_list(DEF_MSG_GAMEMODE_CHANGED, 10);
}

void HandleForceRecallTime(CGame* game, char* data)
{
	short v1;
	std::string txt;
	const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyForceRecallTime>(
		data, sizeof(hb::net::PacketNotifyForceRecallTime));
	if (!pkt) return;
	v1 = static_cast<short>(pkt->seconds_left);
	if (static_cast<int>(v1 / 20) > 0)
		txt = std::format(NOTIFY_MSG_FORCERECALLTIME1, static_cast<int>(v1 / 20));
	else
		txt = NOTIFY_MSG_FORCERECALLTIME2;
	game->add_event_list(txt.c_str(), 10);
}

void HandleNoRecall(CGame* game, char* data)
{
	if (teleport_manager::get().get_state() == teleport_state::awaiting_auth)
		teleport_manager::get().on_auth_rejected();
	game->add_event_list("You can not recall in this map.", 10);
}

// HandleFightZoneReserve removed with guild/fightzone system

void HandleLoteryLost(CGame* game, char* data)
{
	game->add_event_list(DEF_MSG_NOTIFY_LOTERY_LOST, 10);
}

void HandleNotFlagSpot(CGame* game, char* data)
{
	game->add_event_list(NOTIFY_MSG_HANDLER45, 10);
}

void HandleNpcTalk(CGame* game, char* data)
{
	game->npc_talk_handler(data);
}

void HandleTravelerLimitedLevel(CGame* game, char* data)
{
	game->add_event_list(NOTIFY_MSG_HANDLER64, 10);
}

void HandleLimitedLevel(CGame* game, char* data)
{
	game->add_event_list(NOTIFYMSG_LIMITED_LEVEL1, 10);
}

void HandleToBeRecalled(CGame* game, char* data)
{
	game->add_event_list(NOTIFY_MSG_HANDLER62, 10);
}

} // namespace NetworkMessageHandlers