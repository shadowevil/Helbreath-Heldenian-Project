#pragma once

#include "packet_common.h"
#include "packet_headers.h"
#include "net_constants.h"
#include <cstdint>

namespace hb {
namespace net {

//------------------------------------------------------------------------
// NPC Configuration Packet
// Sent from server to client to define NPC display names.
// Keyed by npc_id (config index). npcType is an attribute for rendering.
//------------------------------------------------------------------------

HB_PACK_BEGIN
struct HB_PACKED PacketNpcConfigEntry
{
	int16_t  npcId;                 // Config ID (0-199, primary key)
	int16_t  npcType;               // Model type ID (10-110, for rendering)
	char     name[hb::shared::limits::NpcNameLen];     // Display name (20 chars + null)
};
HB_PACK_END

//------------------------------------------------------------------------
// NPC Configuration Packet Header
// Sent before a batch of NPC config entries
//------------------------------------------------------------------------

HB_PACK_BEGIN
struct HB_PACKED PacketNpcConfigHeader
{
	PacketHeader header;            // Standard packet header (msg_id = hb::shared::net::MsgId::NpcConfigContents)
	uint16_t     npcCount;          // Number of NPCs in this packet
	uint16_t     totalNpcs;         // Total NPCs across all packets
	uint16_t     packetIndex;       // Index of this packet (0-based, for multi-packet sends)
};
HB_PACK_END

} // namespace net
} // namespace hb
