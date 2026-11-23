#include "Game.h"
#include "NetworkMessageManager.h"
#include "Packet/SharedPackets.h"
#include "lan_eng.h"
#include <cstdio>
#include <cstring>
#include "Screen_OnGame.h"

namespace NetworkMessageHandlers {
	void HandleSlateCreateSuccess(CGame* game, char* data)
	{
		game->add_event_list(DEF_MSG_NOTIFY_SLATE_CREATESUCCESS, 10);
	}

	void HandleSlateCreateFail(CGame* game, char* data)
	{
		game->add_event_list(DEF_MSG_NOTIFY_SLATE_CREATEFAIL, 10);
	}

	void HandleSlateInvincible(CGame* game, char* data)
	{
		game->add_event_list(DEF_MSG_NOTIFY_SLATE_INVINCIBLE, 10);
		game->on_game()->m_using_slate = true;
	}

	void HandleSlateMana(CGame* game, char* data)
	{
		game->add_event_list(DEF_MSG_NOTIFY_SLATE_MANA, 10);
		game->on_game()->m_using_slate = true;
	}

	void HandleSlateExp(CGame* game, char* data)
	{
		game->add_event_list(DEF_MSG_NOTIFY_SLATE_EXP, 10);
		game->on_game()->m_using_slate = true;
	}

	void HandleSlateStatus(CGame* game, char* data)
	{
		game->add_event_list(DEF_MSG_NOTIFY_SLATECLEAR, 10);
		game->on_game()->m_using_slate = false;
	}

	void HandleSlateBerserk(CGame* game, char* data)
	{
		game->add_event_list(DEF_MSG_NOTIFY_SLATE_BERSERK, 10); // "Berserk magic casted!"
		game->on_game()->m_using_slate = true;
	}
} // namespace NetworkMessageHandlers
