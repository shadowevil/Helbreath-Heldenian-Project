"""gen_release_seed.py - writes the release seed that pairs a Release client with its server.

Ported from Helbreath-SDL. A Release server admits a client only when its Build ID matches AND its build
proof verifies, and the proof is keyed by this seed. The seed changes only on a full release build:
build_release.ps1 runs --mode roll once, then builds the client and the server. Every other non-Debug
build runs --mode ensure, which writes a seed only when none exists, so it reuses the current one. Debug
builds need no seed (shared/version.cpp returns 0 without NDEBUG), so --configuration Debug is a no-op.

    python gen_release_seed.py --mode ensure --environment DEV --configuration Release --output <path>
    python gen_release_seed.py --mode roll   --environment TEST --output <path>

Each channel has its OWN seed file, in its own folder so the include name stays fixed:
<repo>/build/generated/<channel>/release_seed_generated.h (git-ignored by /build/). The header records
its channel and version.cpp static_asserts that it matches the build's channel, so a PROD seed can never
end up in a DEV build. The client and the server of one channel share the file. Parallel project builds
may run ensure at the same moment: the first writer wins atomically (a no-replace link of a fully written
temp file) and every other caller keeps that seed.

The seed sits in plain form in the binary: it keeps builds in step, it is not anti-tamper. The seed
itself is never printed. Prints ASCII only.
"""

import argparse
import os
import secrets
import sys
from pathlib import Path

SEED_BITS = 64
CHANNEL_IDS = {"DEV": 0, "TEST": 1, "PROD": 2}
DEBUG_CONFIGURATION = "Debug"

HEADER_TEMPLATE = """// GENERATED FILE - do not edit and do not commit.
//
// Written by sources/build_tools/gen_release_seed.py and consumed only by shared/version.cpp in Release
// (NDEBUG) builds. It lives in an untracked build folder, one per channel.

#pragma once

#include <cstdint>

namespace hb::shared::version::generated
{{

// The channel this seed belongs to (0 = DEV, 1 = TEST, 2 = PROD); version.cpp checks it.
inline constexpr int release_seed_channel_id = {channel_id};

// Pairs a Release client with the server of the same full build; never 0.
inline constexpr std::uint64_t release_seed = 0x{seed:016x}ULL;

}} // namespace hb::shared::version::generated
"""


def configure_output() -> None:
	for stream in (sys.stdout, sys.stderr):
		try:
			stream.reconfigure(errors="replace")
		except (AttributeError, ValueError):
			pass


def draw_seed() -> int:
	# 0 means "Debug, no seed", so a drawn 0 is redrawn
	seed = 0
	while seed == 0:
		seed = secrets.randbits(SEED_BITS)
	return seed


def write_seed(output: Path, channel_id: int, replace: bool) -> bool:
	"""Writes a fresh seed. Returns False when replace is off and another writer got there first."""
	output.parent.mkdir(parents=True, exist_ok=True)
	temp = output.with_name(f"{output.name}.{os.getpid()}.{secrets.token_hex(4)}.tmp")
	temp.write_text(HEADER_TEMPLATE.format(channel_id=channel_id, seed=draw_seed()), encoding="utf-8", newline="\n")
	try:
		if replace:
			os.replace(temp, output)
			return True
		try:
			# a hard link never replaces an existing file, on Windows or POSIX
			os.link(temp, output)
			return True
		except FileExistsError:
			return False
	finally:
		if temp.exists():
			temp.unlink()


def main() -> int:
	configure_output()
	parser = argparse.ArgumentParser(description="Write the release seed header for one channel.")
	parser.add_argument("--mode", choices=("ensure", "roll"), default="ensure")
	parser.add_argument("--environment", choices=sorted(CHANNEL_IDS), default="DEV")
	parser.add_argument("--configuration", default="Release",
		help="the build configuration; Debug needs no seed and is a no-op")
	parser.add_argument("--output", required=True, type=Path)
	args = parser.parse_args()

	if args.configuration == DEBUG_CONFIGURATION:
		return 0
	try:
		if args.mode == "ensure" and args.output.is_file():
			print(f"release seed: kept ({args.environment}, ensure)")
			return 0
		written = write_seed(args.output, CHANNEL_IDS[args.environment], replace=args.mode == "roll")
	except OSError as error:
		print(f"gen_release_seed: {error}", file=sys.stderr)
		return 1

	print(f"release seed: {'written' if written else 'kept'} ({args.environment}, {args.mode})")
	return 0


if __name__ == "__main__":
	sys.exit(main())
