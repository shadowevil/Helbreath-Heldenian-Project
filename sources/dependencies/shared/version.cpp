#include "version.h"

#include "util/hmac.h"

// Generated before every compile by sources/build_tools/gen_build_id.py into the project's untracked build
// folder. A missing header fails the build on purpose: a binary must never ship an invented Build ID.
#include "build_id_generated.h"

// Release builds read their channel's seed, rolled by build_release.ps1 (or written once by the
// EnsureReleaseSeed step); the include path selects the channel's folder. Debug builds have none and admit
// everyone.
#ifdef NDEBUG
#include "release_seed_generated.h"
#endif

#include <algorithm>
#include <array>
#include <chrono>
#include <format>
#include <vector>

namespace hb::shared::version
{
	namespace
	{
		// The Build ID rides as (high << 16) | low.
		constexpr unsigned build_id_half_bits = 16;
		constexpr std::uint32_t build_id_half_mask = 0xffffu;
		constexpr unsigned bits_per_byte = 8;

		static_assert(generated::build_id_high <= build_id_half_mask && generated::build_id_low <= build_id_half_mask,
			"build_id_generated.h holds an out-of-range Build ID half");
		// TEST and PROD binaries always enforce the build gate (gen_build_id.py also refuses non-Release)
		static_assert(generated::channel_id == static_cast<int>(channel::dev) || build_gate_enforced,
			"a TEST or PROD build must be a Release (NDEBUG) build");
#ifdef NDEBUG
		static_assert(generated::release_seed != 0, "release_seed_generated.h holds no seed");
		static_assert(generated::release_seed_channel_id == generated::channel_id,
			"the release seed belongs to another channel");
#endif

		// Every displayed form is fixed for the life of the process, so each is built once.
		struct version_text
		{
			std::string release;
			std::string full;
			std::string player_version;
		};

		std::string_view maturity_suffix(maturity value)
		{
			switch (value)
			{
			case maturity::alpha:
				return "-alpha";
			case maturity::beta:
				return "-beta";
			case maturity::stable:
			default:
				return "";
			}
		}

		std::string build_id_text()
		{
			return std::format("{}+{}", generated::build_id_high, generated::build_id_low);
		}

		version_text build_text(component which)
		{
			const release& assigned = release_of(which);

			version_text value;
			value.release = std::format("{}.{}.{}{}", assigned.major, assigned.minor, assigned.patch,
				maturity_suffix(assigned.maturity));
			// production drops its channel label; dev and test keep theirs so a non-production build can never
			// be mistaken for one
			value.full = build_channel() == channel::prod
				? std::format("{}:{}", value.release, build_id_text())
				: std::format("{}:{}.{}", value.release, generated::channel_label, build_id_text());
			value.player_version = std::format("Version {}", value.release);
			return value;
		}

		// One entry per component, in enum order; only the release half differs between them.
		const version_text& text(component which)
		{
			static const std::array<version_text, 3> texts{
				build_text(component::client), build_text(component::server), build_text(component::sql)};
			return texts[static_cast<std::size_t>(which)];
		}

		// Appends `value` little-endian, `byte_count` bytes of it.
		void append_le(std::vector<std::uint8_t>& out, std::uint64_t value, std::size_t byte_count)
		{
			for (std::size_t index = 0; index < byte_count; ++index)
			{
				out.push_back(static_cast<std::uint8_t>(value >> (index * bits_per_byte)));
			}
		}

		// Equal-time compare: the loop never stops early, so a forger learns nothing from how long a refusal
		// took.
		bool proofs_equal(const build_proof& first, const build_proof& second)
		{
			std::uint8_t difference = 0;
			for (std::size_t index = 0; index < build_proof_bytes; ++index)
			{
				difference |= static_cast<std::uint8_t>(first[index] ^ second[index]);
			}
			return difference == 0;
		}
	} // namespace

	channel build_channel()
	{
		static_assert(generated::channel_id >= static_cast<int>(channel::dev)
			&& generated::channel_id <= static_cast<int>(channel::prod),
			"build_id_generated.h holds an unknown channel id");
		return static_cast<channel>(generated::channel_id);
	}

	std::string_view channel_name()
	{
		return generated::channel_label;
	}

	std::uint16_t build_id_high()
	{
		return static_cast<std::uint16_t>(generated::build_id_high);
	}

	std::uint16_t build_id_low()
	{
		return static_cast<std::uint16_t>(generated::build_id_low);
	}

	std::uint32_t build_id_value()
	{
		return (static_cast<std::uint32_t>(generated::build_id_high) << build_id_half_bits)
			| static_cast<std::uint32_t>(generated::build_id_low);
	}

	std::string_view build_id()
	{
		static const std::string built = build_id_text();
		return built;
	}

	bool source_clean()
	{
		return generated::source_clean;
	}

	std::string_view commit_sha()
	{
		return generated::commit_sha;
	}

	std::string_view release_version(component which)
	{
		return text(which).release;
	}

	std::string_view version_string(component which)
	{
		return text(which).full;
	}

	std::string_view player_version_line(component which)
	{
		return text(which).player_version;
	}

	std::string_view player_build_line()
	{
		static const std::string built = std::format("Build {}", build_id_text());
		return built;
	}

	//============================================================================
	// build proof
	//============================================================================

	std::uint64_t release_seed()
	{
#ifdef NDEBUG
		return generated::release_seed;
#else
		return 0;
#endif
	}

	build_identity local_build_identity()
	{
		return build_identity{.build_id = build_id_value(), .seed = release_seed(), .channel = build_channel()};
	}

	std::uint32_t current_build_window()
	{
		const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
			std::chrono::system_clock::now().time_since_epoch()).count();
		return static_cast<std::uint32_t>(seconds / build_proof_window_seconds);
	}

	build_proof compute_build_proof(const build_identity& identity, std::uint32_t window, std::uint32_t request_msg_id)
	{
		std::vector<std::uint8_t> key;
		append_le(key, identity.seed, sizeof(identity.seed));
		std::vector<std::uint8_t> message(build_proof_domain_tag.begin(), build_proof_domain_tag.end());
		message.push_back(static_cast<std::uint8_t>(identity.channel));
		append_le(message, identity.build_id, sizeof(identity.build_id));
		append_le(message, window, sizeof(window));
		append_le(message, request_msg_id, sizeof(request_msg_id));
		const hb::shared::util::sha256_digest digest = hb::shared::util::hmac_sha256(key, message);
		build_proof proof{};
		std::copy_n(digest.begin(), build_proof_bytes, proof.begin());
		return proof;
	}

	client_build_token make_build_token(const build_identity& identity, std::uint32_t window,
		std::uint32_t request_msg_id)
	{
		return client_build_token{.build_id = identity.build_id,
			.window = window,
			.proof = compute_build_proof(identity, window, request_msg_id)};
	}

	client_build_token make_build_token(std::uint32_t request_msg_id, std::int32_t window_offset)
	{
		const std::uint32_t window = current_build_window() + static_cast<std::uint32_t>(window_offset);
		return make_build_token(local_build_identity(), window, request_msg_id);
	}

	std::optional<std::int32_t> learn_window_offset(std::uint32_t server_window)
	{
		const std::int64_t offset =
			static_cast<std::int64_t>(server_window) - static_cast<std::int64_t>(current_build_window());
		if (offset > max_build_window_offset || offset < -max_build_window_offset)
		{
			return std::nullopt;
		}
		return static_cast<std::int32_t>(offset);
	}

	build_check check_build_token(const std::optional<client_build_token>& theirs, std::uint32_t request_msg_id,
		const build_identity& ours, std::uint32_t our_window, bool enforced)
	{
		build_check check;
		if (theirs.has_value())
		{
			check.window_delta = static_cast<std::int64_t>(theirs->window) - static_cast<std::int64_t>(our_window);
		}
		if (!enforced)
		{
			return check;
		}
		if (!theirs.has_value())
		{
			check.result = build_check_result::absent;
		}
		else if (theirs->build_id != ours.build_id)
		{
			check.result = build_check_result::build_differs;
		}
		else if (check.window_delta > build_proof_window_tolerance || check.window_delta < -build_proof_window_tolerance)
		{
			check.result = build_check_result::window_skew;
		}
		else if (!proofs_equal(theirs->proof, compute_build_proof(ours, theirs->window, request_msg_id)))
		{
			check.result = build_check_result::proof_differs;
		}
		return check;
	}

	std::string describe_build_refusal(const std::optional<client_build_token>& theirs, const build_check& check)
	{
		switch (check.result)
		{
		case build_check_result::absent:
			return "build check: no token sent (a legacy client)";
		case build_check_result::build_differs:
			// only a DIFFERENT id is named, since an equal one would state ours
			return std::format("build check: their Build ID {}+{} differs (window delta {})",
				theirs->build_id >> build_id_half_bits, theirs->build_id & build_id_half_mask, check.window_delta);
		case build_check_result::window_skew:
			return std::format("window check: window delta {} is past the tolerance of {}", check.window_delta,
				build_proof_window_tolerance);
		case build_check_result::proof_differs:
			return std::format("proof check: the proof does not verify (window delta {})", check.window_delta);
		case build_check_result::admitted:
		default:
			return "admitted";
		}
	}

} // namespace hb::shared::version
