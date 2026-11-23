#include "log.h"
#include "log_backend.h"
#include "client_log_channels.h"
#include <iostream>
#include <type_traits>

static constexpr std::string_view channel_filenames[] =
{
	"client.log",           // main
	"network.log",          // network
};
static_assert(std::size(channel_filenames) == static_cast<size_t>(hb::log_channel::count));

class client_log_backend : public hb::logger::log_backend
{
protected:
	void write_console(int channel, std::string_view colored_line) override
	{
#ifdef _DEBUG
		(void)channel;
		std::cout << colored_line << '\n';
#else
		(void)channel; (void)colored_line;
#endif
	}
};

static client_log_backend s_backend;

static std::string_view client_channel_namer(int ch)
{
	return hb::channel_name(static_cast<hb::log_channel>(ch));
}

void hb::logger::initialize(std::string_view log_directory)
{
	log_backend::config cfg;
	cfg.directory = log_directory;
	cfg.filenames = channel_filenames;
	cfg.channel_count = static_cast<int>(hb::log_channel::count);
	cfg.channel_namer = client_channel_namer;
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
