"""gen_version_info.py - writes the Windows VERSIONINFO resource for one exe from shared/version.h.

Ported from Helbreath-SDL. The resource compiler cannot read C++ constexpr, so this parses the hand-assigned
release out of sources/dependencies/shared/version.h (the single source of truth) and writes a complete
VERSIONINFO block as an rc fragment into the project's untracked build folder. A tracked .rc includes it
with `#include "version_info.rc"`. The fragment holds no Build ID (a generated Build ID never goes into
tracked source, and the four numeric fields are the release).

    python gen_version_info.py --component client --description "Helbreath client"
        --original-filename Game_x64_win.exe --internal-name Game_x64_win --configuration Debug --output <path>

Run by each .vcxproj (GenerateVersionInfo, BeforeTargets=ResourceCompile). Rewrites the file only when its
text changes, so an unchanged release does not force a resource recompile and relink. Prints ASCII only.

Exit code 1 (message on stderr) when version.h cannot be parsed.
"""

import argparse
import re
import sys
from pathlib import Path

SOURCES_DIR = Path(__file__).resolve().parents[1]
VERSION_HEADER = SOURCES_DIR / "dependencies" / "shared" / "version.h"

PRODUCT_NAME = "Helbreath"
COMPANY_NAME = "ShadowEvil"
LEGAL_COPYRIGHT = "Copyright (C) 2026 ShadowEvil"

# The server release covers the game server and the login server inside it.
COMPONENT_VARIABLES = {"client": "client_release", "server": "server_release"}

# VERSIONINFO has four numeric fields; the fourth is unused.
REVISION_FIELD = 0
# US English, Unicode code page 1200: the "040904B0" string-table key and the Translation pair.
LANGUAGE_ID = 0x0409
CODE_PAGE = 1200
LINE_SEPARATOR = "\r\n"

RELEASE_PATTERN = (r"inline\s+constexpr\s+release\s+{name}\s*\{{\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*"
	r"maturity::(alpha|beta|stable)\s*\}}")

RC_TEMPLATE = """// GENERATED FILE - do not edit and do not commit.
//
// Written by sources/build_tools/gen_version_info.py before every resource compile, from the release in
// sources/dependencies/shared/version.h. It lives in an untracked build folder and is fully regenerable.

#include <winver.h>

VS_VERSION_INFO VERSIONINFO
FILEVERSION {numeric}
PRODUCTVERSION {numeric}
FILEFLAGSMASK VS_FFI_FILEFLAGSMASK
FILEFLAGS {flags}
FILEOS VOS_NT_WINDOWS32
FILETYPE VFT_APP
FILESUBTYPE VFT2_UNKNOWN
BEGIN
	BLOCK "StringFileInfo"
	BEGIN
		BLOCK "{language:04X}{code_page:04X}"
		BEGIN
			VALUE "CompanyName", "{company}"
			VALUE "FileDescription", "{description}"
			VALUE "FileVersion", "{text}"
			VALUE "InternalName", "{internal_name}"
			VALUE "LegalCopyright", "{copyright}"
			VALUE "OriginalFilename", "{original_filename}"
			VALUE "ProductName", "{product}"
			VALUE "ProductVersion", "{text}"
		END
	END
	BLOCK "VarFileInfo"
	BEGIN
		VALUE "Translation", 0x{language:04X}, {code_page}
	END
END
"""


class VersionInfoError(Exception):
	pass


def read_release(component: str) -> tuple:
	"""(major, minor, patch, maturity) for the component, parsed from version.h."""
	name = COMPONENT_VARIABLES[component]
	text = VERSION_HEADER.read_text(encoding="utf-8")
	match = re.search(RELEASE_PATTERN.format(name=name), text)
	if not match:
		raise VersionInfoError(f"could not find 'inline constexpr release {name}{{...}}' in {VERSION_HEADER}")
	major, minor, patch, maturity = match.groups()
	return int(major), int(minor), int(patch), maturity


def release_text(release: tuple) -> str:
	"""'0.2.52-alpha'; a stable release has no suffix."""
	major, minor, patch, maturity = release
	text = f"{major}.{minor}.{patch}"
	return text if maturity == "stable" else f"{text}-{maturity}"


def escape(value: str) -> str:
	return value.replace('"', '""')


def derive_rc(args, release: tuple) -> str:
	major, minor, patch, maturity = release
	flags = []
	if args.configuration == "Debug":
		flags.append("VS_FF_DEBUG")
	if maturity != "stable":
		flags.append("VS_FF_PRERELEASE")
	rc = RC_TEMPLATE.format(
		numeric=f"{major},{minor},{patch},{REVISION_FIELD}",
		flags=" | ".join(flags) if flags else "0",
		language=LANGUAGE_ID,
		code_page=CODE_PAGE,
		company=escape(COMPANY_NAME),
		description=escape(args.description),
		text=release_text(release),
		internal_name=escape(args.internal_name),
		copyright=escape(LEGAL_COPYRIGHT),
		original_filename=escape(args.original_filename),
		product=escape(PRODUCT_NAME))
	return rc.rstrip("\n").replace("\n", LINE_SEPARATOR) + LINE_SEPARATOR


def configure_output() -> None:
	for stream in (sys.stdout, sys.stderr):
		try:
			stream.reconfigure(errors="replace")
		except (AttributeError, ValueError):
			pass


def main() -> int:
	configure_output()
	parser = argparse.ArgumentParser(description="Write the VERSIONINFO rc fragment for one exe.")
	parser.add_argument("--component", choices=sorted(COMPONENT_VARIABLES), required=True)
	parser.add_argument("--description", required=True)
	parser.add_argument("--original-filename", required=True)
	parser.add_argument("--internal-name", required=True)
	parser.add_argument("--configuration", default="Debug")
	parser.add_argument("--output", required=True)
	args = parser.parse_args()

	try:
		release = read_release(args.component)
		rc = derive_rc(args, release)
		# the resource compiler reads this as ASCII; anything else in the inputs is an error here, not there
		data = rc.encode("ascii")
		output = Path(args.output)
		if not (output.is_file() and output.read_bytes() == data):
			output.parent.mkdir(parents=True, exist_ok=True)
			output.write_bytes(data)
	except (VersionInfoError, OSError, UnicodeEncodeError) as error:
		print(f"gen_version_info: {error}", file=sys.stderr)
		return 1

	print(f"version info: {args.original_filename} {release_text(release)} -> {output.name}")
	return 0


if __name__ == "__main__":
	sys.exit(main())
