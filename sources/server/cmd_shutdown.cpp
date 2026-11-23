#include "cmd_shutdown.h"
#include "server_console.h"
#include "game.h"
#include "log.h"
#include "server_log_channels.h"
#include "net_messages.h"
#include "time_utils.h"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <algorithm>

using namespace hb::shared::net;
using namespace hb::server::config;

static void compute_milestones(int total_seconds, std::vector<int>& milestones)
{
	milestones.clear();
	if (total_seconds <= 0) return;

	milestones.push_back(total_seconds);

	// Halve repeatedly until we reach 60s or below
	int t = total_seconds / 2;
	while (t > 60)
	{
		milestones.push_back(t);
		t /= 2;
	}

	// Add fixed low-range milestones
	const int fixed[] = {60, 30, 15, 5};
	for (int m : fixed)
	{
		if (m < total_seconds)
			milestones.push_back(m);
	}

	// Sort descending, remove duplicates
	std::sort(milestones.begin(), milestones.end(), std::greater<int>());
	milestones.erase(std::unique(milestones.begin(), milestones.end()), milestones.end());
}

void CmdShutdown::execute(CGame* game, const char* args)
{
	int delay_seconds = 0;
	const char* message = nullptr;

	// Parse: shutdown [seconds] [message]
	if (args != nullptr && args[0] != '\0')
	{
		char* end = nullptr;
		long val = std::strtol(args, &end, 10);
		if (end != args && val >= 0)
		{
			delay_seconds = static_cast<int>(val);

			// Skip whitespace after number to find message
			while (*end == ' ' || *end == '\t')
				end++;

			if (*end != '\0')
				message = end;
		}
		else
		{
			hb::console::error("Usage: shutdown [seconds] [message]");
			return;
		}
	}

	hb::console::info("Initiating graceful shutdown...");
	hb::logger::log<hb::log_channel::commands>("shutdown: initiated (delay={}s)", delay_seconds);

	// Save all players immediately as a safety snapshot
	int count = game->save_all_players();
	hb::console::success("Saved {} player(s)", count);

	// Store custom message
	if (message != nullptr)
	{
		std::strncpy(game->m_shutdown_message, message, sizeof(game->m_shutdown_message) - 1);
		game->m_shutdown_message[sizeof(game->m_shutdown_message) - 1] = '\0';
	}
	else
	{
		game->m_shutdown_message[0] = '\0';
	}

	if (delay_seconds > 0)
	{
		// Compute countdown milestones
		compute_milestones(delay_seconds, game->m_shutdown_milestones);
		game->m_shutdown_next_milestone = 0;

		// Send first noticement immediately (the total time)
		for (int i = 1; i < MaxClients; i++)
		{
			if (game->m_client_list[i] != nullptr && game->m_client_list[i]->m_is_init_complete)
				game->send_notify_msg(0, i, Notify::ServerShutdown, 1, delay_seconds, 0,
					message != nullptr ? message : nullptr);
		}
		// Skip the first milestone since we just sent it
		if (!game->m_shutdown_milestones.empty()
			&& game->m_shutdown_milestones[0] == delay_seconds)
		{
			game->m_shutdown_next_milestone = 1;
		}

		// Schedule delayed shutdown
		game->m_shutdown_start_time = GameClock::GetTimeMS();
		game->m_shutdown_delay_ms = static_cast<uint32_t>(delay_seconds) * 1000;
		hb::console::info("Shutdown scheduled in {} seconds.", delay_seconds);
	}
	else
	{
		// Immediate shutdown — send noticement then begin disconnect
		for (int i = 1; i < MaxClients; i++)
		{
			if (game->m_client_list[i] != nullptr && game->m_client_list[i]->m_is_init_complete)
				game->send_notify_msg(0, i, Notify::ServerShutdown, 1, 0, 0,
					message != nullptr ? message : nullptr);
		}

		game->m_on_exit_process = true;
		game->m_exit_process_time = GameClock::GetTimeMS();
		hb::console::info("Disconnecting players and shutting down...");
	}
}
