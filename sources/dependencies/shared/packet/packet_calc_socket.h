#pragma once

#include "packet_common.h"

#include <cstdint>

namespace hb {
namespace net {
	HB_PACK_BEGIN
	struct HB_PACKED PacketCalcSocketPing
	{
		std::uint8_t key;
		std::uint16_t length;
		std::uint16_t reserved;
	};
	HB_PACK_END
}
}
