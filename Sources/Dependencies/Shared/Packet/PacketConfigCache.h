#pragma once

#include "PacketCommon.h"
#include "PacketHeaders.h"
#include "NetConstants.h"

#include <cstdint>

#define MSGID_RESPONSE_CONFIGCACHESTATUS	0x0FA314E2

namespace hb {
namespace net {
	HB_PACK_BEGIN
	struct HB_PACKED PacketRequestInitDataEx : packet_base {
		PacketHeader header;
		char player[hb::shared::limits::CharNameLen];
		char account[hb::shared::limits::AccountNameLen];
		char password[hb::shared::limits::AccountPassLen];
		uint8_t is_observer;
		char server[20];
		uint8_t padding;
		// --- Cache extension ---
		char itemConfigHash[65];
		char magicConfigHash[65];
		char skillConfigHash[65];
		char npcConfigHash[65];
		char mapConfigHash[65];
		char balanceConfigHash[65];
		char colorPaletteConfigHash[65];
		char attributeTypeConfigHash[65];
	};

	struct HB_PACKED PacketResponseConfigCacheStatus {
		PacketHeader header;
		uint8_t itemCacheValid;
		uint8_t magicCacheValid;
		uint8_t skillCacheValid;
		uint8_t npcCacheValid;
		uint8_t mapCacheValid;
		uint8_t balanceCacheValid;
		uint8_t colorPaletteCacheValid;
		uint8_t attributeTypeCacheValid;
	};

	struct HB_PACKED PacketNotifyConfigReload {
		PacketHeader header;
		uint8_t reloadItems;
		uint8_t reloadMagic;
		uint8_t reloadSkills;
		uint8_t reloadNpcs;
		uint8_t reloadMaps;
		uint8_t reloadBalance;
		uint8_t reloadColorPalette;
		uint8_t reloadAttributeTypes;
	};

	struct HB_PACKED PacketServerConfigUpdate {
		PacketHeader header;
		int16_t max_stats;
		int32_t max_level;
		int16_t max_bank_items;
		int16_t base_stat_value;
		int16_t max_creation_stat_value;
		int16_t creation_stat_points;
	};

	struct HB_PACKED PacketRequestConfigData {
		PacketHeader header;
		uint8_t requestItems;
		uint8_t requestMagic;
		uint8_t requestSkills;
		uint8_t requestNpcs;
		uint8_t requestMaps;
		uint8_t requestBalance;
		uint8_t requestColorPalette;
		uint8_t requestAttributeTypes;
	};

	struct HB_PACKED PacketMapConfigHeader {
		PacketHeader header;
		uint16_t mapCount;
		uint16_t totalMaps;
		uint16_t packetIndex;
	};

	struct HB_PACKED PacketMapConfigEntry {
		char map_name[hb::shared::limits::MapNameLen];
		char display_name[hb::shared::limits::MapDisplayNameLen];
	};
	HB_PACK_END
}
}
