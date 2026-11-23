#pragma once

#include "packet_common.h"
#include "packet_headers.h"
#include "net_constants.h"

#include <cstdint>

namespace hb {
namespace net {
	HB_PACK_BEGIN

	// Internal party operation payload: op_type + client_h + name + party_id
	// Used by ops 3 (join check), 4 (dismiss), 5 (get info), 6 (timeout dismiss)
	struct HB_PACKED PartyOpPayload {
		std::uint16_t op_type;
		std::uint16_t client_h;
		char name[hb::shared::limits::CharNameLen];
		std::uint16_t party_id;
	};

	// Party operation payload with message ID prefix (PartyAcceptHandler)
	struct HB_PACKED PartyOpPayloadWithId {
		std::uint32_t msg_id;
		std::uint16_t op_type;
		std::uint16_t client_h;
		char name[hb::shared::limits::CharNameLen];
		std::uint16_t party_id;
	};

	// Party create request: op_type + client_h + name (no party_id)
	// Also used for member check (op 3) from PartyManager
	struct HB_PACKED PartyOpCreateRequest {
		std::uint16_t op_type;
		std::uint16_t client_h;
		char name[hb::shared::limits::CharNameLen];
	};

	// Party result with status byte: op_type + result + client_h + name + party_id
	// Used for create result (case 1), join result (case 4), dismiss result (case 6)
	struct HB_PACKED PartyOpResultWithStatus {
		std::uint16_t op_type;
		std::uint8_t result;
		std::uint16_t client_h;
		char name[hb::shared::limits::CharNameLen];
		std::uint16_t party_id;
	};

	// Party delete result: op_type + party_id
	struct HB_PACKED PartyOpResultDelete {
		std::uint16_t op_type;
		std::uint16_t party_id;
	};

	// Party info response header: op_type + client_h + name + total
	// Variable-length member name list follows (11 bytes each: name[10] + padding)
	struct HB_PACKED PartyOpResultInfoHeader {
		std::uint16_t op_type;
		std::uint16_t client_h;
		char name[hb::shared::limits::CharNameLen];
		std::uint16_t total;
	};

	// Request party operation (network packet to client)
	struct HB_PACKED PacketPartyRequest {
		PacketHeader header;
		std::uint16_t op_type;
		std::uint16_t client_h;
		char name[hb::shared::limits::CharNameLen];
		std::uint16_t party_id;
	};

	// Party member info response (network packet to client)
	struct HB_PACKED PacketPartyMemberList {
		PacketHeader header;
		std::uint16_t op_type;
		std::uint16_t client_h;
		char name[hb::shared::limits::CharNameLen];
		std::uint16_t member_count;
	};

	HB_PACK_END
}
}
