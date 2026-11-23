#pragma once

#include "version.h"

#include <cstdint>
#include <cstring>

namespace hb::client
{
	// Fills the version gate of a login or enter-game request: the protocol version and the signed build
	// token. Call it after req.header.msg_id is set, because the proof signs that id.
	// window_offset is the server's clock window minus ours, learned from a clock-skew reply (0 until then).
	template<typename request>
	void fill_login_version(request& req, std::int32_t window_offset = 0)
	{
		namespace version = hb::shared::version;
		req.protocol_version = version::protocol_version;
		req.reserved[0] = 0;
		req.reserved[1] = 0;
		const auto token = version::make_build_token(static_cast<std::uint32_t>(req.header.msg_id), window_offset);
		req.build_id = token.build_id;
		req.build_window = token.window;
		std::memcpy(req.build_proof, token.proof.data(), sizeof(req.build_proof));
	}
}
