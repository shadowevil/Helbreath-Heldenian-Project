#pragma once

#include "packet_headers.h"
#include "appearance.h"
#include "net_constants.h"

#include <cstdint>

namespace hb {
namespace net {
	HB_PACK_BEGIN
	struct HB_PACKED PacketLogCharacterListHeader {
		PacketHeader header;
		std::int32_t total_chars;
	};

	struct HB_PACKED PacketLogCharacterEntry {
		char name[hb::shared::limits::CharNameLen];
		hb::shared::entity::PlayerAppearance appearance;
		std::uint16_t sex;
		std::uint16_t skin;
		std::uint16_t level;
		std::uint32_t exp;
		char map_name[hb::shared::limits::MapNameLen];
	};

	struct HB_PACKED PacketLogNewCharacterCreatedHeader {
		PacketHeader header;
		char character_name[hb::shared::limits::CharNameLen];
		std::int32_t total_chars;
	};

	struct HB_PACKED PacketLogResponseReject {
		PacketHeader header;
		std::int32_t block_year;
		std::int32_t block_month;
		std::int32_t block_day;
	};

	struct HB_PACKED PacketLogEnterGameConfirm {
		PacketHeader header;
		char game_server_addr[16];
		std::uint16_t game_server_port;
		char game_server_name[20];
	};

	struct HB_PACKED PacketLogResponseCode {
		PacketHeader header;
		std::uint8_t code;
	};

	// LogResMsg::ClockSkew (msg_id and msg_type both): the login server refused a build token for its clock
	// window and sends its own current window. The client takes the offset with
	// hb::shared::version::learn_window_offset(server_window) and re-signs once on a new connection.
	struct HB_PACKED PacketLogClockSkew {
		PacketHeader header;
		std::uint32_t server_window;
	};
	HB_PACK_END
}
}
