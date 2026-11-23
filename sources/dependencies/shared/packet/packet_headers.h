#pragma once

#include "packet_common.h"

#include <cstdint>

namespace hb {
namespace net {

	// Empty base for all sendable packet structs.
	// Enables type-safe transport API without void* while preserving packed layout (EBO).
	struct packet_base {};

	HB_PACK_BEGIN
	struct HB_PACKED PacketHeader
	{
		std::uint32_t msg_id;
		std::uint16_t msg_type;
	};
	HB_PACK_END
}
}
