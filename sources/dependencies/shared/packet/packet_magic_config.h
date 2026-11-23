#pragma once

#include "packet_common.h"
#include "packet_headers.h"
#include <cstdint>

namespace hb {
namespace net {

HB_PACK_BEGIN
struct HB_PACKED PacketMagicConfigEntry
{
	int16_t  magicId;       // Magic ID (0-99)
	char     name[31];      // Spell display name (null-padded)
	int16_t  manaCost;      // Mana cost (m_value_1)
	int16_t  intLimit;      // Intelligence requirement (m_intelligence_limit)
	int32_t  goldCost;      // Gold cost, negative = not for sale (m_gold_cost)
	int8_t   visible;     // Show in magic shop (1=yes, 0=no)
	int16_t  magicType;     // DEF_MAGICTYPE_* (m_type)
	int16_t  aoeRadiusX;    // AoE X radius in tiles (m_value_2)
	int16_t  aoeRadiusY;    // AoE Y radius in tiles (m_value_3)
	int16_t  dynamicPattern; // Wall=1, Field=2 (m_value_11), 0 if N/A
	int16_t  dynamicRadius;  // Wall length / field radius (m_value_12)
};
HB_PACK_END

HB_PACK_BEGIN
struct HB_PACKED PacketMagicConfigHeader
{
	PacketHeader header;    // msg_id = hb::shared::net::MsgId::MagicConfigContents
	uint16_t magicCount;    // Entries in this packet
	uint16_t totalMagics;   // Total across all packets
	uint16_t packetIndex;   // 0-based packet index
};
HB_PACK_END

} // namespace net
} // namespace hb
