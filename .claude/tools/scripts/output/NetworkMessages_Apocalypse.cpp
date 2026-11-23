#include "Game.h"
#include "NetworkMessageManager.h"
#include "Packet/SharedPackets.h"
#include "lan_eng.h"
#include <cstdio>
#include <cstring>
#include <format>
#include <string>
#include "Screen_OnGame.h"

namespace NetworkMessageHandlers {

void HandleApocGateStart(CGame* game, char* data)
{
	game->set_top_msg("The portal to the Apocalypse is opened.", 10);
}

void HandleApocGateEnd(CGame* game, char* data)
{
	game->set_top_msg("The portal to the Apocalypse is closed.", 10);
}

void HandleApocGateOpen(CGame* game, char* data)
{
	const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyApocGateOpen>(
		data, sizeof(hb::net::PacketNotifyApocGateOpen));
	if (!pkt) return;
	game->on_game()->m_gate_posit_x = pkt->gate_x;
	game->on_game()->m_gate_posit_y = pkt->gate_y;
	game->m_gate_map_name.assign(pkt->map_name, strnlen(pkt->map_name, sizeof(pkt->map_name)));
}

void HandleApocGateClose(CGame* game, char* data)
{
	game->on_game()->m_gate_posit_x = game->on_game()->m_gate_posit_y = -1;
}

void HandleApocForceRecall(CGame* game, char* data)
{
	game->add_event_list("You are recalled by force, because the Apocalypse is started.", 10);
}

void HandleAbaddonKilled(CGame* game, char* data)
{
	std::string txt;
	char killer[21]{};
	const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyAbaddonKilled>(
		data, sizeof(hb::net::PacketNotifyAbaddonKilled));
	if (!pkt) return;
	memcpy(killer, pkt->killer_name, sizeof(pkt->killer_name));
	
	txt = std::format("Abaddon is destroyed by {}", killer);
	game->add_event_list(txt.c_str(), 10);
}

} // namespace NetworkMessageHandlers
