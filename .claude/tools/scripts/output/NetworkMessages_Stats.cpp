#include "Game.h"
#include "FloatingTextManager.h"
#include "NetworkMessageManager.h"
#include "Packet/SharedPackets.h"
#include "lan_eng.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <format>
#include <string>
#include "Screen_OnGame.h"


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
	game->m_player->m_hunger_status = static_cast<int>(pkt->hunger);

	if (game->m_player->m_hp > prev_hp)
	{
		if ((game->m_player->m_hp - prev_hp) < 10) return;
		txt = std::format(NOTIFYMSG_HP_UP, game->m_player->m_hp - prev_hp);
		game->add_event_list(txt.c_str(), 10);
		game->play_game_sound('E', 21, 0);
	}
	else
	{
		if ((game->on_game()->m_logout_count > 0) && (game->m_force_disconn == false))
		{
			game->on_game()->m_logout_count = -1;
			game->add_event_list(NOTIFYMSG_HP2, 10);
		}
		game->m_damaged_time = GameClock::get_time_ms();
		if (game->m_player->m_hp < 20) game->add_event_list(NOTIFYMSG_HP3, 10);
		if ((prev_hp - game->m_player->m_hp) < 10) return;
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
		if (abs(game->m_player->m_mp - prev_mp) < 10) return;
		if (game->m_player->m_mp > prev_mp)
		{
			txt = std::format(NOTIFYMSG_MP_UP, game->m_player->m_mp - prev_mp);
			game->add_event_list(txt.c_str(), 10);
			game->play_game_sound('E', 21, 0);
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
		if (abs(game->m_player->m_sp - prev_sp) < 10) return;
		if (game->m_player->m_sp > prev_sp)
		{
			txt = std::format(NOTIFYMSG_SP_UP, game->m_player->m_sp - prev_sp);
			game->add_event_list(txt.c_str(), 10);
			game->play_game_sound('E', 21, 0);
		}
		else
		{
			txt = std::format(NOTIFYMSG_SP_DOWN, prev_sp - game->m_player->m_sp);
			game->add_event_list(txt.c_str(), 10);
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
		else
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

		game->m_player->m_lu_point = (game->m_player->m_level - 1) * 3 - ((game->m_player->m_str + game->m_player->m_vit + game->m_player->m_dex + game->m_player->m_int + game->m_player->m_mag + game->m_player->m_charisma) - 70);
		game->m_player->m_lu_str = game->m_player->m_lu_vit = game->m_player->m_lu_dex = game->m_player->m_lu_int = game->m_player->m_lu_mag = game->m_player->m_lu_char = 0;

		txt = std::format(NOTIFYMSG_LEVELUP1, game->m_player->m_level);
		game->add_event_list(txt.c_str(), 10);

		switch (game->m_player->m_player_type) {
		case 1:
		case 2:
		case 3:
			game->play_game_sound('C', 21, 0);
			break;
		case 4:
		case 5:
		case 6:
			game->play_game_sound('C', 22, 0);
			break;
		}

		game->get_floating_text().remove_by_object_id(game->m_player->m_player_object_id);
		game->get_floating_text().add_notify_text(notify_text_type::LevelUp, "Level up!", game->m_cur_time,
			game->m_player->m_player_object_id, game->m_map_data.get());
		return;
	}
} // namespace NetworkMessageHandlers
