#pragma once

#include <string_view>

namespace hb {

enum class log_channel : int
{
	main,               // client.log
	network,            // network.log

	count
};

constexpr std::string_view channel_name(log_channel ch)
{
	switch (ch)
	{
	case log_channel::main:    return "main";
	case log_channel::network: return "network";
	default:                   return "unknown";
	}
}

} // namespace hb
