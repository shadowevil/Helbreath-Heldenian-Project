#include "game.h"
#include "network_message_manager.h"
#include "packet/shared_packets.h"
#include "lan_eng.h"
#include <cstdio>
#include <cstring>

namespace NetworkMessageHandlers {
	void HandleAngelFailed(CGame* game, char* data)
	{
		int v1;
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyAngelFailed>(
			data, sizeof(hb::net::PacketNotifyAngelFailed));
		if (!pkt) return;
		v1 = pkt->reason;
		switch (v1) {
		case 1:
			game->add_event_list(DEF_MSG_NOTIFY_ANGEL_FAILED, 10);
			break;
		case 2:
			game->add_event_list(DEF_MSG_NOTIFY_ANGEL_MAJESTIC, 10);
			break;
		case 3:
			game->add_event_list(DEF_MSG_NOTIFY_ANGEL_LOW_LVL, 10);
			break;
		}
	}

	void HandleAngelReceived(CGame* game, char* data)
	{
		game->add_event_list(DEF_MSG_NOTIFY_ANGEL_RECEIVED, 10);
	}

	void HandleAngelicStats(CGame* game, char* data)
	{
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyAngelicStats>(
			data, sizeof(hb::net::PacketNotifyAngelicStats));
		if (!pkt) return;
		game->m_player->m_angelic_str = pkt->str;
		game->m_player->m_angelic_int = pkt->intel;
		game->m_player->m_angelic_dex = pkt->dex;
		game->m_player->m_angelic_mag = pkt->mag;
	}
} // namespace NetworkMessageHandlers
