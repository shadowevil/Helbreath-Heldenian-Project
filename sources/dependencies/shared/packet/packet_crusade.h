#pragma once

#include "packet_common.h"
#include "net_constants.h"

#include <cstdint>

namespace hb {
namespace net {
	HB_PACK_BEGIN

	// Individual crusade structure entry (6 bytes)
	struct HB_PACKED CrusadeStructureEntry {
		std::uint8_t type;
		std::int16_t x;
		std::int16_t y;
		std::uint8_t side;
	};

	// Crusade map status header (precedes CrusadeStructureEntry array)
	struct HB_PACKED CrusadeMapStatusHeader {
		char map_name[hb::shared::limits::MapNameLen];
		std::int16_t send_point;
		std::uint8_t count;
	};

	// Meteor strike structure HP data header
	struct HB_PACKED MeteorStrikeHeader {
		std::uint16_t total_points;
	};

	// Heldenian teleport response (30 bytes, written after PacketHeader in txt buffer)
	// Note: short values written with cp+=4 in original code = int32 slots on LE
	struct HB_PACKED HeldenianTeleportResponse {
		std::int32_t count;       // always 4
		std::int32_t location;    // 1-4
		char map_name[hb::shared::limits::MapNameLen];
		std::int32_t x;
		std::int32_t y;
		std::int32_t why_return;
	};

	HB_PACK_END
}
}
