#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

// Build and release identity, ported from Helbreath-SDL's versioning standard.
//
// The hand-assigned release says how SIGNIFICANT a build is; the generated Build ID says which SOURCE STATE it
// came from. Canonical form: M.m.p[-alpha|-beta]:CHANNEL.High+Low, and PROD drops the channel label.
//
// The Build ID, channel and commit come from build_id_generated.h, which sources/build_tools/gen_build_id.py
// writes into each project's untracked build folder before every compile. Only version.cpp reads it.
namespace hb::shared::version
{
	// The independently versioned parts. Each has its own hand-assigned release; there is no umbrella
	// project version.
	enum class component
	{
		client, // the game client and its assets
		server, // the game server, including the login server inside it
		sql,    // the databases: schema, migrations and seed data
	};

	// alpha while major systems change, beta for testing and polish, stable (no suffix) from 1.0.0.
	enum class maturity
	{
		alpha,
		beta,
		stable,
	};

	// Major for overhauls, Minor for additions, Patch for fixes. Never incremented automatically.
	struct release
	{
		int major = 0;
		int minor = 0;
		int patch = 1;
		version::maturity maturity = version::maturity::alpha;
	};

	// THE RELEASE NUMBERS. These three lines are the only place they are assigned, and they move only by
	// hand. Bumping one does not touch the others. gen_version_info.py and both CMakeLists.txt parse these
	// lines, so keep the `inline constexpr release <name>{M, m, p, maturity::<x>};` shape.
	inline constexpr release client_release{0, 2, 52, maturity::alpha};
	inline constexpr release server_release{0, 1, 24, maturity::alpha};
	inline constexpr release sql_release{0, 0, 1, maturity::alpha};

	// The wire version that gates login. A wire value, not a release: bump it whenever a client needs a
	// code change to talk to a server.
	inline constexpr std::uint16_t protocol_version = 1;

	[[nodiscard]] constexpr const release& release_of(component which)
	{
		switch (which)
		{
		case component::client:
			return client_release;
		case component::sql:
			return sql_release;
		case component::server:
		default:
			return server_release;
		}
	}

	// The environment a binary was built for. DEV is daily work; TEST and PROD need a clean tree, the
	// Release configuration and an explicit fresh-source assertion (gen_build_id.py enforces all three).
	enum class channel
	{
		dev,
		test,
		prod,
	};

	[[nodiscard]] channel build_channel();

	// "DEV" / "TEST" / "PROD". PROD omits its label from the displayed version but still reports it here.
	[[nodiscard]] std::string_view channel_name();

	// Build ID halves, each 0-65535. One build has one source state, so every component shares them.
	[[nodiscard]] std::uint16_t build_id_high();
	[[nodiscard]] std::uint16_t build_id_low();

	// (high << 16) | low: the form the Build ID takes on the wire.
	[[nodiscard]] std::uint32_t build_id_value();

	// "41279+37580"
	[[nodiscard]] std::string_view build_id();

	// True when the Build ID came from HEAD alone; false for a dirty DEV tree, whose id is a reproducible
	// fingerprint of the working state that cannot be resolved back to a commit.
	[[nodiscard]] bool source_clean();

	// Full sha of the commit the build started from. Authoritative, unlike the 8-hex Build ID.
	[[nodiscard]] std::string_view commit_sha();

	// "0.2.52-alpha" for the named component.
	[[nodiscard]] std::string_view release_version(component which);

	// The canonical full form, e.g. "0.2.52-alpha:DEV.41279+37580" (PROD: "0.2.52-alpha:41279+37580").
	[[nodiscard]] std::string_view version_string(component which);

	// The pair players see: "Version 0.2.52-alpha" for a component, and the build line they all share,
	// "Build 41279+37580".
	[[nodiscard]] std::string_view player_version_line(component which);
	[[nodiscard]] std::string_view player_build_line();

	//============================================================================
	// build proof
	//============================================================================

	// The release seed never crosses the wire. A client proves it holds the seed with a truncated
	// HMAC-SHA256 over (domain tag, channel, Build ID, clock window, the id of the request it rides on).

	// Release builds (NDEBUG) embed a random seed rolled by build_release.ps1 and enforce the gate. Each
	// channel has its own seed file, so a PROD seed never reaches a DEV build. Debug builds have seed 0 and
	// admit everyone; a Debug client therefore never passes a Release server.
#ifdef NDEBUG
	inline constexpr bool build_gate_enforced = true;
#else
	inline constexpr bool build_gate_enforced = false;
#endif

	// The per-release secret a Release client and server share; 0 in Debug. It only keys the build proof.
	// Never send or log it. It sits in plain form in the binary: it keeps builds in step, it is not
	// anti-tamper.
	[[nodiscard]] std::uint64_t release_seed();

	// 128 bits: far past guessing within one window, and half the wire cost of the full digest.
	inline constexpr std::size_t build_proof_bytes = 16;
	using build_proof = std::array<std::uint8_t, build_proof_bytes>;

	// A proof names one 60 s clock window. With the tolerance below a server accepts it in that window and
	// one either side, so a captured proof replays for about 3 windows (up to roughly 180 s). The request id
	// it signs is the message TYPE id, not a per-request nonce: within those windows the proof can be replayed
	// on any message of the same type. It binds a build, not a session.
	inline constexpr std::int64_t build_proof_window_seconds = 60;
	// One window either side absorbs a request straddling a minute edge and a small clock difference.
	inline constexpr std::int64_t build_proof_window_tolerance = 1;
	// Separates this HMAC from any other use of the same key. Bump the digit if the message layout changes
	// (2: the channel joined the message).
	inline constexpr std::string_view build_proof_domain_tag = "HB-BUILD-PROOF-2";

	// Who built a client, the clock window its proof is for, and the proof. 24 bytes on the wire:
	// u32 build_id LE, u32 window LE, 16 proof bytes.
	struct client_build_token
	{
		std::uint32_t build_id = 0;
		std::uint32_t window = 0;
		build_proof proof{};

		[[nodiscard]] bool operator==(const client_build_token&) const = default;
	};
	inline constexpr std::size_t client_build_token_size = 24;

	// What a token is made with and checked against.
	// A channel mismatch is caught by the proof itself: both sides sign with their own channel.
	struct build_identity
	{
		std::uint32_t build_id = 0;
		std::uint64_t seed = 0;
		version::channel channel = version::channel::dev;
	};

	// This binary's own identity (build_id_value(), release_seed(), build_channel()).
	[[nodiscard]] build_identity local_build_identity();

	// unix_seconds / build_proof_window_seconds, from the local clock.
	[[nodiscard]] std::uint32_t current_build_window();

	// The first build_proof_bytes of HMAC-SHA256(key = seed LE,
	// domain tag || channel id (1 byte) || build_id LE || window LE || request_msg_id LE).
	[[nodiscard]] build_proof compute_build_proof(const build_identity& identity, std::uint32_t window,
		std::uint32_t request_msg_id);

	[[nodiscard]] client_build_token make_build_token(const build_identity& identity, std::uint32_t window,
		std::uint32_t request_msg_id);

	// The client's helper: this binary's token for a request, at the local window plus `window_offset`
	// (the server's window minus ours, learned from a clock_skew reply; 0 until then).
	[[nodiscard]] client_build_token make_build_token(std::uint32_t request_msg_id, std::int32_t window_offset = 0);

	// The largest clock offset a client will adopt from a clock_skew reply: 1440 windows, 24 hours, which
	// covers a clock set to the wrong time zone. A larger one is refused, so a lying server cannot have the
	// client sign proofs for a window far in the future and replay them later. The server needs no offset
	// bound of its own: it admits only its own window plus or minus build_proof_window_tolerance.
	inline constexpr std::int64_t max_build_window_offset = 1440;

	// The window_offset to retry with after a clock_skew reply carrying `server_window`
	// (server_window - current_build_window()), or nullopt when it is past max_build_window_offset.
	[[nodiscard]] std::optional<std::int32_t> learn_window_offset(std::uint32_t server_window);

	// Which check a token failed. absent / build_differs / proof_differs mean "version mismatch";
	// window_skew means "clock skew" and its reply carries the server's window so the client can retry.
	enum class build_check_result : std::uint8_t
	{
		admitted,
		absent,
		build_differs,
		window_skew,
		proof_differs,
	};

	struct build_check
	{
		build_check_result result = build_check_result::admitted;
		std::int64_t window_delta = 0; // their window - ours; 0 when no token was read

		[[nodiscard]] bool admitted() const { return result == build_check_result::admitted; }
		[[nodiscard]] bool clock_skew() const { return result == build_check_result::window_skew; }
	};

	// The gate, in its fixed order: absent or another Build ID, then the window, then a constant-time proof
	// compare for THEIR window and the request. Admits everything when not enforced.
	[[nodiscard]] build_check check_build_token(const std::optional<client_build_token>& theirs,
		std::uint32_t request_msg_id, const build_identity& ours, std::uint32_t our_window, bool enforced);

	// For a warning log line: which check failed, the window delta, and THEIR Build ID only when it
	// differs. Never a seed or a proof.
	[[nodiscard]] std::string describe_build_refusal(const std::optional<client_build_token>& theirs,
		const build_check& check);

} // namespace hb::shared::version
