#pragma once

#include "PacketCommon.h"
#include "PacketHeaders.h"
#include <cstdint>

namespace hb {
namespace net {

//------------------------------------------------------------------------
// Color Palette Configuration Packet
// Sent from server to client to define the unified color palette
// used for item tints, weapon tints, and hair colors.
//------------------------------------------------------------------------

HB_PACK_BEGIN
struct HB_PACKED PacketColorPaletteConfigEntry
{
	uint8_t colorId;
	uint8_t r;
	uint8_t g;
	uint8_t b;
};
HB_PACK_END

//------------------------------------------------------------------------
// Color Palette Configuration Packet Header
// Sent before a batch of color palette entries
//------------------------------------------------------------------------

HB_PACK_BEGIN
struct HB_PACKED PacketColorPaletteConfigHeader
{
	PacketHeader header;            // msg_id = MsgId::ColorPaletteConfigContents
	uint16_t     colorCount;        // Number of entries in this packet
	uint16_t     totalColors;       // Total entries across all packets
	uint16_t     packetIndex;       // Index of this packet (0-based)
};
HB_PACK_END

} // namespace net
} // namespace hb
