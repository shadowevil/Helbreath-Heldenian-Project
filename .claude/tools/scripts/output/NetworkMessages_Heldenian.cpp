#include "Game.h"
#include "NetworkMessageManager.h"
#include "Packet/SharedPackets.h"
#include "lan_eng.h"
#include <cstdio>
#include <cstring>
#include "Screen_OnGame.h"

namespace NetworkMessageHandlers {

void HandleHeldenianTeleport(CGame* game, char* data)
{
	game->set_top_msg("Teleport to Heldenian field is available from now. Magic casting is forbidden until real battle.", 10);
}

void HandleHeldenianEnd(CGame* game, char* data)
{
	game->set_top_msg("Heldenian holy war has been closed.", 10);
}

void HandleHeldenianStart(CGame* game, char* data)
{
	game->set_top_msg("Heldenian real battle has been started form now on.", 10);
}

void HandleHeldenianVictory(CGame* game, char* data)
{
	short side;
	const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyHeldenianVictory>(
		data, sizeof(hb::net::PacketNotifyHeldenianVictory));
	if (!pkt) return;
	side = pkt->side;
	game->show_heldenian_victory(side);
	game->on_game()->m_heldenian_aresden_left_tower = -1;
	game->on_game()->m_heldenian_elvine_left_tower = -1;
	game->on_game()->m_heldenian_aresden_flags = -1;
	game->on_game()->m_heldenian_elvine_flags = -1;
}

void HandleHeldenianCount(CGame* game, char* data)
{
	const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyHeldenianCount>(
		data, sizeof(hb::net::PacketNotifyHeldenianCount));
	if (!pkt) return;
	game->on_game()->m_heldenian_aresden_left_tower = pkt->aresden_tower_left;
	game->on_game()->m_heldenian_elvine_left_tower = pkt->elvine_tower_left;
	game->on_game()->m_heldenian_aresden_flags = pkt->aresden_flags;
	game->on_game()->m_heldenian_elvine_flags = pkt->elvine_flags;
}

void HandleHeldenianRecall(CGame* game, char* data)
{
	game->set_top_msg("Characters will be recalled by force as Heldenian begins.", 10);
}

} // namespace NetworkMessageHandlers
