#pragma once

#include <cstdint>

// client_messages.h - Client-Only Network Messages
//
// Network message IDs used exclusively by the Helbreath Client.
//
// For shared protocol messages, see dependencies/shared/net/net_messages.h

namespace hb::client::net
{

// Client-Only Message IDs
namespace ClientMsgId
{
	enum : uint32_t
	{
		// Teleport list messages
		RequestTeleportList                     = 0x0EA03202,
		ResponseTeleportList                    = 0x0EA03203,
		RequestChargedTeleport                  = 0x0EA03204,
		ResponseChargedTeleport                 = 0x0EA03205,

		// Heldenian teleport messages
		RequestHeldenianTpList                  = 0x0EA03206,
		ResponseHeldenianTpList                 = 0x0EA03207,
		RequestHeldenianTp                      = 0x0EA03208,

		// Gateway messages
		GetMinimumLoadGateway                   = 0x3B1890EA,
	};
}

} // namespace hb::client::net
