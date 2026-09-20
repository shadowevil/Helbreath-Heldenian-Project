#include "GameCmdSpawn.h"
#include "Game.h"
#include "Npc.h"
#include <cstring>
#include <cstdio>

using namespace hb::shared::net;
using namespace hb::server::config;
bool GameCmdSpawn::execute(CGame* game, int client_h, const char* args)
{
	if (game->m_client_list[client_h] == nullptr)
		return true;

	int npc_id = 0, amount = 1;
	if (args == nullptr || args[0] == '\0' || sscanf(args, "%d %d", &npc_id, &amount) < 1)
	{
		game->send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0, "Usage: /spawn <npc_id> [amount]");
		return true;
	}

	if (npc_id < 0 || npc_id >= MaxNpcTypes || game->m_npc_config_list[npc_id] == nullptr)
	{
		game->send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0, "Invalid NPC ID.");
		return true;
	}

	if (amount < 1) amount = 1;
	if (amount > 50) amount = 50;

	char* map_name = game->m_map_list[game->m_client_list[client_h]->m_map_index]->m_name;
	int spawned = 0;

	for (int i = 0; i < amount; i++)
	{
		char unique_name[21];
		std::snprintf(unique_name, sizeof(unique_name), "GM-spawn%d", i);

		int tX = game->m_client_list[client_h]->m_x;
		int tY = game->m_client_list[client_h]->m_y;

		// is_summoned=false so NPCs give EXP/drops, bypass_mob_limit=true so they don't count toward map limit
		if (game->create_new_npc(npc_id, unique_name, map_name, 0, 0, hb::server::npc::MoveType::Random,
			&tX, &tY, nullptr, nullptr, 0, -1, false, false, false, false, true))
		{
			spawned++;
		}
	}

	char buf[128];
	std::snprintf(buf, sizeof(buf), "Spawned %d x %s (ID: %d).", spawned, game->m_npc_config_list[npc_id]->m_npc_name, npc_id);
	game->send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0, buf);

	return true;
}
