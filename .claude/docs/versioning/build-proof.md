# Login gate and build proof

Verified against `sources/dependencies/shared/version.h`, `sources/server/login_server.cpp`
(`check_request_version`, `refuse_version`), `sources/dependencies/shared/packet/packet_login.h`,
`sources/client/game.cpp` (`retry_login_with_server_window`, `log_response_handler`) and
`sources/server/game.cpp` (CommandCheckConnection handler), 2026-11-23. `version.cpp` internals
(HMAC details) were read through `version.h` comments only.

## The gate

Both `LoginRequest` and `EnterGameRequest` carry `protocol_version` (u16), `reserved[2]`, `build_id` (u32),
`build_window` (u32) and `build_proof[16]` (the proof structs at `packet_login.h`). The server
(`admit_versioned_request` -> `check_request_version`) refuses in this order:

1. **Short request** (a legacy or truncated client): refused, not read past its end.
2. **`protocol_version` differs**: always refused, in every configuration.
3. **Build token** (`version::check_build_token`), enforced only when `build_gate_enforced` is true, i.e. a
   Release (`NDEBUG`) server: absent token or different Build ID -> window outside the tolerance -> proof
   compare (constant time). A Debug server admits everyone; a Debug client has seed 0 and so never passes a
   Release server.

A refusal is logged on the `security` channel (rate-capped by `refusal_log_limiter`; never the seed or proof),
and the reply is `LogResMsg::VersionMismatch`, or `LogResMsg::ClockSkew` carrying the server's window. The
dispatcher closes the login connection afterwards.

`CreateAccountRequest` has no version gate (plan sect. 4 default: login rejects old clients). Only login and
enter-game are gated: **unverified** that no other request type carries the token.

## The proof

`proof = first 16 bytes of HMAC-SHA256(key = release seed LE, domain tag "HB-BUILD-PROOF-2" || channel id ||
build_id LE || window LE || request msg id LE)`. The seed never crosses the wire. The signed request id is the
message type id, not a per-request nonce, so a captured proof replays on messages of the same type for about
3 windows (up to roughly 180 s). It binds a build, not a session. Constants (`version.h`): window 60 s,
tolerance +-1 window, `build_proof_bytes` 16. Bump the domain tag digit if the message layout changes.

## Clock-skew exchange

The window is `unix_seconds / 60` from the local clock. When the server answers `ClockSkew` with its window,
the client calls `learn_window_offset(server_window)`, which returns an offset only if it is within
`max_build_window_offset` (1440 windows = 24 hours, a wrongly set time zone). Larger offsets are refused so a
lying server cannot have the client sign far-future proofs. The client then re-signs the pending login packet
and reconnects, once per request (`m_clock_skew_retried`); a second skew reply, an undersized reply or an
out-of-bounds offset ends in `VersionNotMatch`. The server needs no offset bound: it admits only its own window
+-1.

## The release seed

A random 64-bit value per channel, written by `gen_release_seed.py` to
`<repo>/build/generated/<channel>/release_seed_generated.h` (gitignored). `--mode roll` (only
`build_release.ps1`) replaces it; `--mode ensure` (every other non-Debug build) writes one only when none
exists. A seed header records its channel and `version.cpp` `static_assert`s it matches the build's channel,
so a PROD seed can never end up in a DEV build. The client and server of one channel share the file; a WSL
build of the same checkout reads the same files. The seed sits in plain form in the binary: it keeps builds in
step, it is not anti-tamper. Never print or log it.

Consequence: a Release client and server only pair when built on the same seed, and after any partial rebuild
the Build ID differs too. Use `build_release.ps1` to build the pair.

## Game entry: notice only

After a client is in the game, `CGame` handles `CommandCheckConnection` (`sources/server/game.cpp`, the
`m_last_version_warning_time` block). The client sends its release as three bytes plus `build_id_high` /
`build_id_low` (u16). Every 5 minutes: a release lower than `client_release` (compared as a triple) sends a
"client is outdated, relaunch to update" notice; a different Build ID writes one log line. **Nothing here
refuses a client.** No persistent per-character record of the client's version was found in `game.cpp`:
**unverified** whether one exists elsewhere.

## Related

- Client display of a mismatch: `Overlay_VersionNotMatch` and `GameMode::VersionNotMatch`.
- Build constants and the seed: `implementation.md`.
