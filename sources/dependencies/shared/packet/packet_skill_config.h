#pragma once

#include "packet_common.h"
#include "packet_headers.h"
#include "net_constants.h"
#include <cstdint>

namespace hb {
namespace net {

HB_PACK_BEGIN
struct HB_PACKED PacketSkillConfigEntry
{
	int16_t  skillId;       // Skill ID (0-59)
	char     name[hb::shared::limits::ItemNameLen];      // Skill display name (null-padded)
	int8_t   useable;     // Whether skill can be actively used (1=yes, 0=no)
	int8_t   useMethod;     // Use method (0=passive, 1=click, 2=target)
};
HB_PACK_END

HB_PACK_BEGIN
struct HB_PACKED PacketSkillConfigHeader
{
	PacketHeader header;    // msg_id = hb::shared::net::MsgId::SkillConfigContents
	uint16_t skillCount;    // Entries in this packet
	uint16_t totalSkills;   // Total across all packets
	uint16_t packetIndex;   // 0-based packet index
};
HB_PACK_END

} // namespace net
} // namespace hb
