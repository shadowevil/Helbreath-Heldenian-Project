#include "game.h"
#include "network_message_manager.h"
#include "packet/shared_packets.h"
#include "lan_eng.h"
#include <cstdio>
#include <cstring>

namespace NetworkMessageHandlers {
	void HandleAgricultureNoArea(CGame* game, char* data)
	{
		game->add_event_list(DEF_MSG_NOTIFY_AGRICULTURENOAREA, 10);
	}

	void HandleAgricultureSkillLimit(CGame* game, char* data)
	{
		game->add_event_list(DEF_MSG_NOTIFY_AGRICULTURESKILLLIMIT, 10);
	}

	void HandleNoMoreAgriculture(CGame* game, char* data)
	{
		game->add_event_list(DEF_MSG_NOTIFY_NOMOREAGRICULTURE, 10);
	}
} // namespace NetworkMessageHandlers
