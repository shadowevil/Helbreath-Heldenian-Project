#include "game.h"
#include "network_message_manager.h"
#include "packet/shared_packets.h"
#include "lan_eng.h"
#include <cstdio>
#include <cstring>
#include "dialog_box_ids.h"
#include <format>
#include <string>

namespace NetworkMessageHandlers {
	void HandleSpawnEvent(CGame* game, char* data)
	{
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifySpawnEvent>(
			data, sizeof(hb::net::PacketNotifySpawnEvent));
		if (!pkt) return;
		game->m_monster_id = pkt->monster_id;
		game->m_event_x = pkt->x;
		game->m_event_y = pkt->y;
		game->m_monster_event_time = game->m_cur_time;
	}

	void HandleMonsterCount(CGame* game, char* data)
	{
		std::string txt;
		int count;
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyMonsterCount>(
			data, sizeof(hb::net::PacketNotifyMonsterCount));
		if (!pkt) return;
		count = pkt->count;
		txt = std::format("Rest Monster :{}", count);
		game->add_event_list(txt.c_str(), 10);
	}

	void HandleResurrectPlayer(CGame* game, char* data)
	{
		game->get_dialog_box_manager().enable_dialog_box(DialogBoxId::Resurrect, 0, 0, 0);
	}
} // namespace NetworkMessageHandlers
