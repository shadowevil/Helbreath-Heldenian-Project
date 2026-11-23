#pragma once

#include <string_view>

namespace hb {

enum class log_channel : int
{
	main,               // server.log
	events,             // events.log
	security,           // hackevents.log
	pvp,                // pvpevents.log
	network,            // xsocket.log
	log_events,         // logevents.log
	chat,               // chat.log
	commands,           // commands.log
	drops,              // monster_drops.log
	trade,              // player_trade.log
	shop,               // shop.log
	crafting,           // crafting.log
	upgrades,           // upgrades.log
	bank,               // bank.log
	items_misc,         // misc.log

	count
};

constexpr std::string_view channel_name(log_channel ch)
{
	switch (ch)
	{
	case log_channel::main:         return "main";
	case log_channel::events:       return "events";
	case log_channel::security:     return "security";
	case log_channel::pvp:          return "pvp";
	case log_channel::network:      return "network";
	case log_channel::log_events:   return "log_events";
	case log_channel::chat:         return "chat";
	case log_channel::commands:     return "commands";
	case log_channel::drops:        return "drops";
	case log_channel::trade:        return "trade";
	case log_channel::shop:         return "shop";
	case log_channel::crafting:     return "crafting";
	case log_channel::upgrades:     return "upgrades";
	case log_channel::bank:         return "bank";
	case log_channel::items_misc:   return "items_misc";
	default:                        return "unknown";
	}
}

} // namespace hb
