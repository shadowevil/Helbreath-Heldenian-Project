#include "log.h"
#include "log_backend.h"
#include "server_log_channels.h"
#include "server_console.h"
#include <iostream>
#include <type_traits>

static constexpr std::string_view channel_filenames[] =
{
	"server.log",           // main
	"events.log",           // events
	"hackevents.log",       // security
	"pvpevents.log",        // pvp
	"xsocket.log",          // network
	"logevents.log",        // log_events
	"chat.log",             // chat
	"commands.log",         // commands
	"monster_drops.log",    // drops
	"player_trade.log",     // trade
	"shop.log",             // shop
	"crafting.log",         // crafting
	"upgrades.log",         // upgrades
	"bank.log",             // bank
	"misc.log",             // items_misc
};
static_assert(std::size(channel_filenames) == static_cast<size_t>(hb::log_channel::count));

class server_log_backend : public hb::logger::log_backend
{
protected:
	void write_console(int channel, std::string_view colored_line) override
	{
		if (channel != (int)hb::log_channel::main) return;
		GetServerConsole().write_line_raw(colored_line);
	}
};

static server_log_backend s_backend;

static std::string_view server_channel_namer(int ch)
{
	return hb::channel_name(static_cast<hb::log_channel>(ch));
}

void hb::logger::initialize(std::string_view log_directory)
{
	log_backend::config cfg;
	cfg.directory = log_directory;
	cfg.filenames = channel_filenames;
	cfg.channel_count = static_cast<int>(hb::log_channel::count);
	cfg.channel_namer = server_channel_namer;
	s_backend.init(cfg);
}

void hb::logger::detail::write(int channel, int level, std::string_view message)
{
	s_backend.write(channel, level, message);
}

void hb::logger::shutdown()
{
	s_backend.close();
}
