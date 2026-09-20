#pragma once

#include "PacketHeaders.h"
#include "NetConstants.h"

#include <cstdint>

namespace hb {
namespace net {
	HB_PACK_BEGIN
	struct HB_PACKED PacketRequestHeaderOnly : packet_base {
		PacketHeader header;
	};

	struct HB_PACKED PacketRequestAngel : packet_base {
		PacketHeader header;
		char name[hb::shared::limits::NpcNameLen];
		int32_t angel_id;
	};

	struct HB_PACKED PacketRequestHeldenianScroll : packet_base {
		PacketHeader header;
		char name[hb::shared::limits::ItemNameLen];
		uint16_t item_id;
	};

	struct HB_PACKED PacketRequestName20 : packet_base {
		PacketHeader header;
		char name[hb::shared::limits::NpcNameLen];
	};

	struct HB_PACKED PacketRequestTeleportId : packet_base {
		PacketHeader header;
		int32_t teleport_id;
	};

	struct HB_PACKED PacketRequestPanning : packet_base {
		PacketHeader header;
		uint8_t dir;
	};

	struct HB_PACKED PacketRequestSetItemPos : packet_base {
		PacketHeader header;
		uint8_t dir;
		int16_t x;
		int16_t y;
	};

	struct HB_PACKED PacketCommandCheckConnection : packet_base {
		PacketHeader header;
		uint32_t time_ms;
		uint8_t client_major;
		uint8_t client_minor;
		uint8_t client_patch;
		uint16_t client_build;
	};

	struct HB_PACKED PacketRequestInitPlayer : packet_base {
		PacketHeader header;
		char player[hb::shared::limits::CharNameLen];
		char account[hb::shared::limits::AccountNameLen];
		char password[hb::shared::limits::AccountPassLen];
		uint8_t is_observer;
		char server[20];
		uint8_t padding;
	};

	struct HB_PACKED PacketRequestLevelUpSettings : packet_base {
		PacketHeader header;
		int16_t str;
		int16_t vit;
		int16_t dex;
		int16_t intel;
		int16_t mag;
		int16_t chr;
	};

	struct HB_PACKED PacketRequestSellItemListEntry {
		uint8_t index;
		int32_t amount;
	};

	struct HB_PACKED PacketRequestSellItemList : packet_base {
		PacketHeader header;
		PacketRequestSellItemListEntry entries[12];
		uint8_t padding[4];
	};

	struct HB_PACKED PacketCommandChatMsgHeader : packet_base {
		PacketHeader header;
		int16_t x;
		int16_t y;
		char name[hb::shared::limits::CharNameLen];
		uint8_t chat_type;
	};

	struct HB_PACKED PacketCommandCommonBase {
		PacketHeader header;
		int16_t x;
		int16_t y;
		uint8_t dir;
	};

	struct HB_PACKED PacketCommandCommonWithTime : packet_base {
		PacketCommandCommonBase base;
		int32_t v1;
		int32_t v2;
		int32_t v3;
		uint32_t time_ms;
	};

	struct HB_PACKED PacketCommandCommonWithString : packet_base {
		PacketCommandCommonBase base;
		int32_t v1;
		int32_t v2;
		int32_t v3;
		char text[hb::shared::limits::ItemNameLen];
		int32_t v4;
	};

	struct HB_PACKED PacketCommandCommonItems : packet_base {
		PacketCommandCommonBase base;
		uint8_t item_ids[6];
		uint8_t padding;
	};

	struct HB_PACKED PacketCommandCommonBuild : packet_base {
		PacketCommandCommonBase base;
		char name[hb::shared::limits::ItemNameLen];
		uint8_t item_ids[6];
	};

	struct HB_PACKED PacketRequestRetrieveItem : packet_base {
		PacketHeader header;
		uint8_t item_slot;
	};

	struct HB_PACKED PacketRequestNoticement : packet_base {
		PacketHeader header;
		int32_t value;
	};

	struct HB_PACKED PacketRequestStateChange : packet_base {
		PacketHeader header;
		int16_t str;
		int16_t vit;
		int16_t dex;
		int16_t intel;
		int16_t mag;
		int16_t chr;
	};

	struct HB_PACKED PacketCommandMotionBase {
		PacketHeader header;
		int16_t x;
		int16_t y;
		uint8_t dir;
		int16_t dx;
		int16_t dy;
		int16_t type;
	};

	struct HB_PACKED PacketCommandMotionSimple : packet_base {
		PacketCommandMotionBase base;
		uint32_t time_ms;
	};

	struct HB_PACKED PacketCommandMotionAttack : packet_base {
		PacketCommandMotionBase base;
		uint16_t target_id;
		uint32_t time_ms;
	};
	HB_PACK_END
}
}
