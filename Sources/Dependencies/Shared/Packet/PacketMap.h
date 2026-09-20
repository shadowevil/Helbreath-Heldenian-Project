#pragma once

#include "PacketCommon.h"
#include "NetConstants.h"
#include "Appearance.h"
#include "PlayerStatusData.h"

#include <cstdint>

namespace hb {
namespace net {
	HB_PACK_BEGIN
	struct HB_PACKED PacketMapDataHeader {
		std::int16_t total;
	};

	struct HB_PACKED PacketMapDataEntryHeader {
		std::int16_t x;
		std::int16_t y;
		std::uint8_t flags;
	};

	struct HB_PACKED PacketMapDataObjectBase {
		std::uint16_t object_id;
	};

	struct HB_PACKED PacketMapDataObjectPlayer {
		PacketMapDataObjectBase base;
		std::int16_t type;
		std::uint8_t dir;
		hb::shared::entity::PlayerAppearance appearance;
		hb::shared::entity::PlayerStatus status;
		char name[hb::shared::limits::CharNameLen];
	};

	struct HB_PACKED PacketMapDataObjectNpc {
		PacketMapDataObjectBase base;
		std::int16_t config_id;
		std::uint8_t dir;
		hb::shared::entity::EntityAppearance appearance;
		hb::shared::entity::EntityStatus status;
		char name[5];
	};

	struct HB_PACKED PacketMapDataItem {
		std::int16_t item_id;
		std::uint8_t color;
		std::uint8_t custom_made;
		std::uint8_t prefix_type;
		std::uint8_t prefix_value;
		std::uint8_t secondary_type;
		std::uint8_t secondary_value;
		std::uint8_t enchant_bonus;
		std::int16_t count;
	};

	struct HB_PACKED PacketMapDataDynamicObject {
		std::uint16_t object_id;
		std::int16_t type;
	};
	HB_PACK_END
}
}
