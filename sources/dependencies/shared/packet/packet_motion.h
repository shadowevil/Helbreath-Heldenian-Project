#pragma once

#include "packet_headers.h"
#include "appearance.h"
#include "player_status_data.h"

#include <cstdint>

namespace hb {
namespace net {
	HB_PACK_BEGIN
	struct HB_PACKED PacketResponseMotionMoveConfirm
	{
		PacketHeader header;
		std::int16_t x;
		std::int16_t y;
		std::uint8_t dir;
		std::uint8_t stamina_cost;
		std::uint8_t occupy_status;
		std::int32_t hp;
	};

	struct HB_PACKED PacketResponseMotionMoveReject
	{
		PacketHeader header;
		std::uint16_t object_id;
		std::int16_t x;
		std::int16_t y;
		std::int16_t type;
		std::uint8_t dir;
		char name[10];
		hb::shared::entity::PlayerAppearance appearance;
		hb::shared::entity::PlayerStatus status;
		std::uint8_t padding;
	};

	struct HB_PACKED PacketResponseMotionReject
	{
		PacketHeader header;
		std::int16_t x;
		std::int16_t y;
	};
	HB_PACK_END
}
}
