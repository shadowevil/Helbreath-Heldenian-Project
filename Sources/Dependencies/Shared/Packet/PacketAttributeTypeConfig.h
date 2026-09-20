#pragma once

#include "PacketCommon.h"
#include "PacketHeaders.h"

#include <cstdint>

namespace hb {
namespace net {
	HB_PACK_BEGIN
	struct HB_PACKED PacketAttributePrefixTypeEntry {
		uint8_t prefix_id;
		char display_name[32];
		char effect_label[48];
		char effect_format[48];
		uint8_t min_value;
		uint8_t max_value;
		uint8_t weapon_color;
		uint8_t multiplier;
	};

	struct HB_PACKED PacketAttributeSecondaryTypeEntry {
		uint8_t secondary_id;
		char effect_label[48];
		char effect_format[48];
		uint8_t min_value;
		uint8_t max_value;
		uint8_t multiplier;
	};

	struct HB_PACKED PacketAttributeTypeConfigHeader {
		PacketHeader header;
		uint16_t entryCount;
		uint16_t totalEntries;
		uint16_t packetIndex;
		uint8_t  entryType;  // 0 = prefix, 1 = secondary
	};
	HB_PACK_END
}
}
