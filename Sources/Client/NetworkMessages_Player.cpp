#include "Game.h"
#include "Screen_OnGame.h"
#include "ChatManager.h"
#include "NetworkMessageManager.h"
#include "Packet/SharedPackets.h"
#include "lan_eng.h"
#include "AudioManager.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string_view>
#include <cmath>
#include <format>
#include <string>

namespace NetworkMessageHandlers {
	void HandleCharisma(CGame* game, char* data)
	{
		int  prev_char;
		std::string txt;

		prev_char = game->m_player->m_charisma;
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyCharisma>(
			data, sizeof(hb::net::PacketNotifyCharisma));
		if (!pkt) return;
		game->m_player->m_charisma = static_cast<int>(pkt->charisma);

		if (game->m_player->m_charisma > prev_char)
		{
			txt = std::format(NOTIFYMSG_CHARISMA_UP, game->m_player->m_charisma - prev_char);
			game->add_event_list(txt.c_str(), 10);
			audio_manager::get().play_game_sound(sound_type::effect, 21, 0);
		}
		else
		{
			txt = std::format(NOTIFYMSG_CHARISMA_DOWN, prev_char - game->m_player->m_charisma);
			game->add_event_list(txt.c_str(), 10);
		}
	}

	void HandleHunger(CGame* game, char* data)
	{
		char h_lv;
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyHunger>(
			data, sizeof(hb::net::PacketNotifyHunger));
		if (!pkt) return;
		game->m_player->m_hunger_status = pkt->hunger;

		h_lv = game->m_player->m_hunger_status;
		if ((h_lv <= 40) && (h_lv > 30)) game->add_event_list(NOTIFYMSG_HUNGER1, 10);
		if ((h_lv <= 25) && (h_lv > 20)) game->add_event_list(NOTIFYMSG_HUNGER2, 10);
		if ((h_lv <= 20) && (h_lv > 15)) game->add_event_list(NOTIFYMSG_HUNGER3, 10);
		if ((h_lv <= 15) && (h_lv > 10)) game->add_event_list(NOTIFYMSG_HUNGER4, 10);
		if ((h_lv <= 10) && (h_lv >= 0)) game->add_event_list(NOTIFYMSG_HUNGER5, 10);
	}

	void HandlePlayerProfile(CGame* game, char* data)
	{
		std::string temp;
		int i;
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyPlayerProfile>(
			data, sizeof(hb::net::PacketNotifyPlayerProfile));
		if (!pkt) return;
		temp = pkt->text;
		for (i = 0; i < static_cast<int>(temp.size()); i++)
			if (temp[i] == '_') temp[i] = ' ';
		game->add_event_list(temp.c_str(), 10);
	}

	void HandlePlayerStatus(CGame* game, bool on_game, char* data)
	{
		char name[12]{}, map_name[12]{};
		std::string txt;
		uint16_t dx = 1, dy = 1;
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyPlayerStatus>(
			data, sizeof(hb::net::PacketNotifyPlayerStatus));
		if (!pkt) return;
		memcpy(name, pkt->name, sizeof(pkt->name));
		memcpy(map_name, pkt->map_name, sizeof(pkt->map_name));
		dx = pkt->x;
		dy = pkt->y;
		if (on_game == true) {
			if (map_name[0] == 0)
				txt = std::format(NOTIFYMSG_PLAYER_STATUS1, name);
			else txt = std::format(NOTIFYMSG_PLAYER_STATUS2, name, map_name, dx, dy);
		}
		else txt = std::format(NOTIFYMSG_PLAYER_STATUS3, name);
		game->add_event_list(txt.c_str(), 10);
	}

	void HandleWhisperMode(CGame* game, bool active, char* data)
	{
		char name[12]{};
		std::string txt;
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyWhisperMode>(
			data, sizeof(hb::net::PacketNotifyWhisperMode));
		if (!pkt) return;
		memcpy(name, pkt->name, sizeof(pkt->name));
		if (active == true)
		{
			txt = std::format(NOTIFYMSG_WHISPERMODE1, name);
			ChatManager::get().add_whisper_target(name);
		}
		else txt = NOTIFYMSG_WHISPERMODE2;

		game->add_event_list(txt.c_str(), 10);
	}

	void HandlePlayerShutUp(CGame* game, char* data)
	{
		char name[12]{};
		std::string txt;
		uint16_t time;
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyPlayerShutUp>(
			data, sizeof(hb::net::PacketNotifyPlayerShutUp));
		if (!pkt) return;
		time = pkt->time;
		memcpy(name, pkt->name, sizeof(pkt->name));
		if (game->m_player->m_player_name == std::string_view(name, strnlen(name, hb::shared::limits::CharNameLen)))
			txt = std::format(NOTIFYMSG_PLAYER_SHUTUP1, time);
		else txt = std::format(NOTIFYMSG_PLAYER_SHUTUP2, name, time);

		game->add_event_list(txt.c_str(), 10);
	}

	void HandleRatingPlayer(CGame* game, char* data)
	{
		char name[12]{};
		std::string txt;
		uint16_t value;
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyRatingPlayer>(
			data, sizeof(hb::net::PacketNotifyRatingPlayer));
		if (!pkt) return;
		value = pkt->result;
		memcpy(name, pkt->name, sizeof(pkt->name));
		if (game->m_player->m_player_name == std::string_view(name, strnlen(name, hb::shared::limits::CharNameLen)))
		{
			if (value == 1)
			{
				txt = NOTIFYMSG_RATING_PLAYER1;
				audio_manager::get().play_game_sound(sound_type::effect, 23, 0);
			}
		}
		else
		{
			if (value == 1)
				txt = std::format(NOTIFYMSG_RATING_PLAYER2, name);
			else txt = std::format(NOTIFYMSG_RATING_PLAYER3, name);
		}
		if (!txt.empty()) game->add_event_list(txt.c_str(), 10);
	}

	void HandleCannotRating(CGame* game, char* data)
	{
		std::string txt;

		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyCannotRating>(
			data, sizeof(hb::net::PacketNotifyCannotRating));
		if (!pkt) return;
		const auto time = pkt->time_left;

		if (time == 0) txt = NOTIFYMSG_CANNOT_RATING1;
		else txt = std::format(NOTIFYMSG_CANNOT_RATING2, time * 3);
		game->add_event_list(txt.c_str(), 10);
	}

	void HandlePlayerCharacterContents(CGame* game, char* data)
	{
		game->m_player->m_angelic_str = 0;
		game->m_player->m_angelic_dex = 0;
		game->m_player->m_angelic_int = 0;
		game->m_player->m_angelic_mag = 0;

		const auto* pkt = hb::net::PacketCast<hb::net::PacketResponsePlayerCharacterContents>(
			data, sizeof(hb::net::PacketResponsePlayerCharacterContents));
		if (!pkt) return;

		game->m_player->m_hp = pkt->hp;
		game->m_player->m_mp = pkt->mp;
		game->m_player->m_sp = pkt->sp;
		game->m_player->m_stats_initialized = true;
		game->m_player->m_ac = pkt->ac;
		game->m_player->m_thac0 = pkt->thac0;
		game->m_player->m_level = pkt->level;
		game->m_player->m_str = pkt->str;
		game->m_player->m_int = pkt->intel;
		game->m_player->m_vit = pkt->vit;
		game->m_player->m_dex = pkt->dex;
		game->m_player->m_mag = pkt->mag;
		game->m_player->m_charisma = pkt->chr;
		game->m_player->m_lu_point = pkt->lu_point;
		game->m_player->m_exp = pkt->exp;
		game->m_player->m_enemy_kill_count = pkt->enemy_kills;
		game->m_player->m_pk_count = pkt->pk_count;
		game->m_player->m_reward_gold = pkt->reward_gold;

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

		game->m_player->m_super_attack_left = pkt->super_attack_left;
		game->m_max_stats = pkt->max_stats;
		game->m_max_level = pkt->max_level;
		game->m_max_bank_items = pkt->max_bank_items;
	}

	void HandleServerConfigUpdate(CGame* game, char* data)
	{
		const auto* pkt = hb::net::PacketCast<hb::net::PacketServerConfigUpdate>(
			data, sizeof(hb::net::PacketServerConfigUpdate));
		if (!pkt) return;

		game->m_max_stats = pkt->max_stats;
		game->m_max_level = pkt->max_level;
		game->m_max_bank_items = pkt->max_bank_items;
		game->m_base_stat_value = pkt->base_stat_value;
		game->m_max_creation_stat_value = pkt->max_creation_stat_value;
		game->m_creation_stat_points = pkt->creation_stat_points;

		// During login (no player yet), only store config values
		if (!game->m_player) return;

		game->set_top_msg((char*)"Server configuration has been updated.", 5);

		// Clamp any pending level-up allocations that would exceed the new max
		auto& p = *game->m_player;
		struct { int base; int16_t& pending; } stat_pairs[] = {
			{ p.m_str, p.m_lu_str }, { p.m_vit, p.m_lu_vit },
			{ p.m_dex, p.m_lu_dex }, { p.m_int, p.m_lu_int },
			{ p.m_mag, p.m_lu_mag }, { p.m_charisma, p.m_lu_char }
		};
		for (auto& [base, pending] : stat_pairs)
		{
			int overflow = (base + pending) - game->m_max_stats;
			if (overflow > 0 && pending > 0)
			{
				int16_t reduce = static_cast<int16_t>(std::min(static_cast<int>(pending), overflow));
				pending -= reduce;
				p.m_lu_point += reduce;
			}
		}
	}
}
