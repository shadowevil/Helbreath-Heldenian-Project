#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace hb::shared::util
{
	inline constexpr std::size_t sha256_digest_bytes = 32;
	using sha256_digest = std::array<std::uint8_t, sha256_digest_bytes>;

	// Raw (binary) SHA-256 of a byte buffer. sha256.h's sha256() returns the same digest as hex.
	[[nodiscard]] sha256_digest sha256_bytes(std::span<const std::uint8_t> data);

	// HMAC-SHA256 (RFC 2104). A key longer than the 64-byte block is hashed first, as the RFC requires.
	[[nodiscard]] sha256_digest hmac_sha256(std::span<const std::uint8_t> key, std::span<const std::uint8_t> data);

} // namespace hb::shared::util
