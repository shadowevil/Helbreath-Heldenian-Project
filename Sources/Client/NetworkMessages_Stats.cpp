#include "Game.h"
#include "SharedCalculations.h"
#include "FloatingTextManager.h"
#include "NetworkMessageManager.h"
#include "Packet/SharedPackets.h"
#include "lan_eng.h"
#include "Log.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <format>
#include <string>
#include "Screen_OnGame.h"
#include "AudioManager.h"


namespace NetworkMessageHandlers {
	void HandleHP(CGame* game, char* data)
	{
		int prev_hp;
		std::string txt;

		prev_hp = game->m_player->m_hp;
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyHP>(
			data, sizeof(hb::net::PacketNotifyHP));
		if (!pkt) return;
		game->m_player->m_hp = static_cast<int>(pkt->hp);

		if (!game->m_player->m_stats_initialized) return;

		if (game->m_player->m_hp > prev_hp)
		{
			txt = std::format(NOTIFYMSG_HP_UP, game->m_player->m_hp - prev_hp);
			game->add_event_list(txt.c_str(), 10);
			audio_manager::get().play_game_sound(sound_type::effect, 21, 0);
		}
		else if (game->m_player->m_hp < prev_hp)
		{
			if ((game->on_game()->m_logout_count > 0) && (game->m_force_disconn == false))
			{
				game->on_game()->m_logout_count = -1;
				game->add_event_list(NOTIFYMSG_HP2, 10);
			}
			game->m_damaged_time = GameClock::get_time_ms();
			if (game->m_player->m_hp < 20) game->add_event_list(NOTIFYMSG_HP3, 10);
			txt = std::format(NOTIFYMSG_HP_DOWN, prev_hp - game->m_player->m_hp);
			game->add_event_list(txt.c_str(), 10);
		}
	}

	void HandleMP(CGame* game, char* data)
	{
		int prev_mp;
		std::string txt;
		prev_mp = game->m_player->m_mp;
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyMP>(
			data, sizeof(hb::net::PacketNotifyMP));
		if (!pkt) return;
		game->m_player->m_mp = static_cast<int>(pkt->mp);

		if (!game->m_player->m_stats_initialized) return;
		if (game->m_player->m_mp == prev_mp) return;

		if (game->m_player->m_mp > prev_mp)
		{
			txt = std::format(NOTIFYMSG_MP_UP, game->m_player->m_mp - prev_mp);
			game->add_event_list(txt.c_str(), 10);
			audio_manager::get().play_game_sound(sound_type::effect, 21, 0);
		}
		else
		{
			txt = std::format(NOTIFYMSG_MP_DOWN, prev_mp - game->m_player->m_mp);
			game->add_event_list(txt.c_str(), 10);
		}
	}

	void HandleSP(CGame* game, char* data)
	{
		int prev_sp;
		std::string txt;
		prev_sp = game->m_player->m_sp;
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifySP>(
			data, sizeof(hb::net::PacketNotifySP));
		if (!pkt) return;
		game->m_player->m_sp = static_cast<int>(pkt->sp);

		if (!game->m_player->m_stats_initialized) return;

		if (game->m_player->m_sp > prev_sp)
		{
			txt = std::format(NOTIFYMSG_SP_UP, game->m_player->m_sp - prev_sp);
			game->add_event_list(txt.c_str(), 10);
			audio_manager::get().play_game_sound(sound_type::effect, 21, 0);
		}
	}

	void HandleExp(CGame* game, char* data)
	{
		uint32_t prev_exp;
		std::string txt;

		prev_exp = game->m_player->m_exp;
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyExp>(
			data, sizeof(hb::net::PacketNotifyExp));
		if (!pkt) return;
		game->m_player->m_exp = pkt->exp;

		if (game->m_player->m_exp > prev_exp)
		{
			txt = std::format(EXP_INCREASED, game->m_player->m_exp - prev_exp);
			game->add_event_list(txt.c_str(), 10);
		}
		else if (game->m_player->m_exp < prev_exp)
		{
			txt = std::format(EXP_DECREASED, prev_exp - game->m_player->m_exp);
			game->add_event_list(txt.c_str(), 10);
		}
	}

	void HandleLevelUp(CGame* game, char* data)
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

		// Reset exp so the subsequent Notify::Exp (with remainder) shows as an increase, not a decrease
		game->m_player->m_exp = 0;

		game->m_player->m_lu_str = game->m_player->m_lu_vit = game->m_player->m_lu_dex = game->m_player->m_lu_int = game->m_player->m_lu_mag = game->m_player->m_lu_char = 0;

		txt = std::format(NOTIFYMSG_LEVELUP1, game->m_player->m_level);
		game->add_event_list(txt.c_str(), 10);

		switch (game->m_player->m_player_type) {
		case 1:
		case 2:
		case 3:
			audio_manager::get().play_game_sound(sound_type::character, 21, 0);
			break;
		case 4:
		case 5:
		case 6:
			audio_manager::get().play_game_sound(sound_type::character, 22, 0);
			break;
		}

		game->get_floating_text().remove_by_object_id(game->m_player->m_player_object_id);
		game->get_floating_text().add_notify_text(notify_text_type::LevelUp, "Level up!", game->m_cur_time,
			game->m_player->m_player_object_id, game->m_map_data.get());
		return;
	}
	void HandleLevelUpPoints(CGame* game, char* data)
	{
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyLevelUpPoints>(
			data, sizeof(hb::net::PacketNotifyLevelUpPoints));
		if (!pkt) return;
		game->m_player->m_lu_point = pkt->lu_point;
	}

	void HandleForceStatRefresh(CGame* game, char* data)
	{
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
		game->m_player->m_lu_str = game->m_player->m_lu_vit = game->m_player->m_lu_dex = game->m_player->m_lu_int = game->m_player->m_lu_mag = game->m_player->m_lu_char = 0;
	}
} // namespace NetworkMessageHandlers
