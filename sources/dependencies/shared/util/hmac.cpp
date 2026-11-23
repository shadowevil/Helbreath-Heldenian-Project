#include "hmac.h"
#include "vendor/picosha2.h"

#include <algorithm>

namespace hb::shared::util
{
	namespace
	{
		constexpr std::size_t sha256_block_bytes = 64;
		constexpr std::uint8_t inner_pad = 0x36;
		constexpr std::uint8_t outer_pad = 0x5c;

		using block = std::array<std::uint8_t, sha256_block_bytes>;

		// SHA-256 over two buffers in sequence, without joining them first.
		sha256_digest sha256_concat(std::span<const std::uint8_t> first, std::span<const std::uint8_t> second)
		{
			picosha2::hash256_one_by_one hasher;
			hasher.process(first.begin(), first.end());
			hasher.process(second.begin(), second.end());
			hasher.finish();
			sha256_digest digest{};
			hasher.get_hash_bytes(digest.begin(), digest.end());
			return digest;
		}

		block padded_key(const block& key, std::uint8_t pad)
		{
			block result{};
			for (std::size_t i = 0; i < sha256_block_bytes; ++i)
			{
				result[i] = static_cast<std::uint8_t>(key[i] ^ pad);
			}
			return result;
		}
	} // namespace

	sha256_digest sha256_bytes(std::span<const std::uint8_t> data)
	{
		return sha256_concat(data, {});
	}

	sha256_digest hmac_sha256(std::span<const std::uint8_t> key, std::span<const std::uint8_t> data)
	{
		// K0: the key, hashed when it is longer than a block, then zero-padded to the block size
		block key_block{};
		if (key.size() > sha256_block_bytes)
		{
			const sha256_digest hashed = sha256_bytes(key);
			std::copy(hashed.begin(), hashed.end(), key_block.begin());
		}
		else
		{
			std::copy(key.begin(), key.end(), key_block.begin());
		}

		const sha256_digest inner = sha256_concat(padded_key(key_block, inner_pad), data);
		return sha256_concat(padded_key(key_block, outer_pad), inner);
	}

} // namespace hb::shared::util
