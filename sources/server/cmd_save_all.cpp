#include "cmd_save_all.h"
#include "server_console.h"
#include "game.h"
#include "log.h"
#include "server_log_channels.h"

void CmdSaveAll::execute(CGame* game, const char* args)
{
	int count = game->save_all_players();

	hb::console::success("Saved {} player(s)", count);
	hb::logger::log<hb::log_channel::commands>("saveall: saved {} player(s)", count);
}
