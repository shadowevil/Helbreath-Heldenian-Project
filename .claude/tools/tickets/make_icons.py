"""Bake the ticket viewer's app icons: a coloured square with the project's initials in a white pixel font.

    python <CLI_DIR>/make_icons.py                    the text and colour from .claude/tickets/config.json
    python <CLI_DIR>/make_icons.py --text XY          this text instead
    python <CLI_DIR>/make_icons.py --color "#68217a"  this background colour instead (quote it: an unquoted
                                                      # starts a comment in PowerShell and in bash)

The text is config.json's icon_text, or by default its id_prefix without the hyphen (at most three
characters): one to three of A-Z and 0-9. The background is config.json's icon_color, #rrggbb in either
case, by default #007acc (Visual Studio blue). With both --text and --color no config is needed; with
--text alone the colour comes from config.json when there is one, else the default. Writes
<CLI_DIR>/icons/{icon-192,icon-512,apple-touch-icon}.png (stdlib only, no downloads) and reports what the
bake cost (time and bytes), as every derived bake must.
"""

import argparse
import os
import re
import struct
import sys
import time
import zlib

tool_dir = os.path.dirname(os.path.abspath(__file__))
icons_dir = os.path.join(tool_dir, "icons")
# name -> edge in pixels: the two manifest sizes plus the 180 px iOS home-screen icon.
icon_sizes = {"icon-192.png": 192, "icon-512.png": 512, "apple-touch-icon.png": 180}
config_relative_path = os.path.join(".claude", "tickets", "config.json")
# The background without a config (tickets.py fills in the same default for config.json's icon_color).
default_color = "#007acc"
colour_pattern = re.compile(r"^#[0-9a-fA-F]{6}$")
foreground = (0xff, 0xff, 0xff)
# 5 x 7 glyphs; "#" is ink.
glyphs = {
	"A": [".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"],
	"B": ["####.", "#...#", "#...#", "####.", "#...#", "#...#", "####."],
	"C": [".###.", "#...#", "#....", "#....", "#....", "#...#", ".###."],
	"D": ["####.", "#...#", "#...#", "#...#", "#...#", "#...#", "####."],
	"E": ["#####", "#....", "#....", "####.", "#....", "#....", "#####"],
	"F": ["#####", "#....", "#....", "####.", "#....", "#....", "#...."],
	"G": [".###.", "#...#", "#....", "#.###", "#...#", "#...#", ".####"],
	"H": ["#...#", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"],
	"I": [".###.", "..#..", "..#..", "..#..", "..#..", "..#..", ".###."],
	"J": ["..###", "...#.", "...#.", "...#.", "...#.", "#..#.", ".##.."],
	"K": ["#...#", "#..#.", "#.#..", "##...", "#.#..", "#..#.", "#...#"],
	"L": ["#....", "#....", "#....", "#....", "#....", "#....", "#####"],
	"M": ["#...#", "##.##", "#.#.#", "#.#.#", "#...#", "#...#", "#...#"],
	"N": ["#...#", "#...#", "##..#", "#.#.#", "#..##", "#...#", "#...#"],
	"O": [".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."],
	"P": ["####.", "#...#", "#...#", "####.", "#....", "#....", "#...."],
	"Q": [".###.", "#...#", "#...#", "#...#", "#.#.#", "#..#.", ".##.#"],
	"R": ["####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#"],
	"S": [".####", "#....", "#....", ".###.", "....#", "....#", "####."],
	"T": ["#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.."],
	"U": ["#...#", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."],
	"V": ["#...#", "#...#", "#...#", "#...#", "#...#", ".#.#.", "..#.."],
	"W": ["#...#", "#...#", "#...#", "#.#.#", "#.#.#", "#.#.#", ".#.#."],
	"X": ["#...#", "#...#", ".#.#.", "..#..", ".#.#.", "#...#", "#...#"],
	"Y": ["#...#", "#...#", ".#.#.", "..#..", "..#..", "..#..", "..#.."],
	"Z": ["#####", "....#", "...#.", "..#..", ".#...", "#....", "#####"],
	"0": [".###.", "#...#", "#..##", "#.#.#", "##..#", "#...#", ".###."],
	"1": ["..#..", ".##..", "..#..", "..#..", "..#..", "..#..", ".###."],
	"2": [".###.", "#...#", "....#", "...#.", "..#..", ".#...", "#####"],
	"3": ["#####", "...#.", "..#..", "...#.", "....#", "#...#", ".###."],
	"4": ["...#.", "..##.", ".#.#.", "#..#.", "#####", "...#.", "...#."],
	"5": ["#####", "#....", "####.", "....#", "....#", "#...#", ".###."],
	"6": ["..##.", ".#...", "#....", "####.", "#...#", "#...#", ".###."],
	"7": ["#####", "....#", "...#.", "..#..", ".#...", ".#...", ".#..."],
	"8": [".###.", "#...#", "#...#", ".###.", "#...#", "#...#", ".###."],
	"9": [".###.", "#...#", "#...#", ".####", "....#", "...#.", ".##.."],
}
glyph_width = 5
glyph_height = 7
glyph_gap = 1
max_text_length = 3
# The text spans this share of the edge, by its length, and stays within text_height_share of it in
# height, so it sits inside the maskable-icon safe zone (the central circle, 80% of the edge across).
text_width_share = {1: 0.56, 2: 0.56, 3: 0.66}
text_height_share = 0.40
png_signature = b"\x89PNG\r\n\x1a\n"
compression_level = 9


class icon_error(Exception):
	pass


def check_text(text):
	if not isinstance(text, str) or not 1 <= len(text) <= max_text_length:
		raise icon_error("icon text must be 1 to {} characters of A-Z and 0-9, not {}".format(
			max_text_length, ascii(text)))
	for letter in text:
		if letter not in glyphs:
			raise icon_error("icon text {}: unsupported character {} (use A-Z and 0-9)".format(ascii(text), ascii(letter)))
	return text


def check_color(text, source):
	"""(r, g, b) from a #rrggbb colour; source names where it came from in the error."""
	# fullmatch: a trailing newline would satisfy the pattern's "$".
	if not isinstance(text, str) or not colour_pattern.fullmatch(text):
		raise icon_error("{} must be a colour written #rrggbb (e.g. {}), not {}".format(source, default_color, ascii(text)))
	return tuple(int(text[index:index + 2], 16) for index in (1, 3, 5))


def ink_mask(size, text):
	columns = len(text) * glyph_width + (len(text) - 1) * glyph_gap
	cell = max(1, int(min(size * text_width_share[len(text)] / columns, size * text_height_share / glyph_height)))
	left = (size - columns * cell) // 2
	top = (size - glyph_height * cell) // 2
	mask = set()
	for index, letter in enumerate(text):
		origin = left + index * (glyph_width + glyph_gap) * cell
		for row, line in enumerate(glyphs[letter]):
			for column, mark in enumerate(line):
				if mark == "#":
					for y in range(top + row * cell, top + (row + 1) * cell):
						for x in range(origin + column * cell, origin + (column + 1) * cell):
							mask.add((x, y))
	return mask


def chunk(kind, data):
	body = kind + data
	return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xffffffff)


def png_bytes(size, text, background):
	mask = ink_mask(size, text)
	rows = bytearray()
	for y in range(size):
		rows.append(0)
		for x in range(size):
			rows.extend(foreground if (x, y) in mask else background)
	header = struct.pack(">IIBBBBB", size, size, 8, 2, 0, 0, 0)
	return (png_signature + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(bytes(rows), compression_level))
		+ chunk(b"IEND", b""))


def configured(key):
	"""icon_text or icon_color from .claude/tickets/config.json (tickets.py checks the file and fills in defaults)."""
	sys.path.insert(0, tool_dir)
	import tickets as ticket_store  # noqa: E402 - the sibling module; prints a clean ERROR without a config
	return ticket_store.config[key]


def config_found():
	"""Whether tickets.py would find a config.json: at or above the tool folder, as its find_repo_root looks."""
	folder = tool_dir
	while True:
		if os.path.isfile(os.path.join(folder, config_relative_path)):
			return True
		parent = os.path.dirname(folder)
		if parent == folder:
			return False
		folder = parent


def main():
	parser = argparse.ArgumentParser(description="Bake the ticket viewer's icons into <CLI_DIR>/icons/.")
	parser.add_argument("--text", help="1 to 3 characters of A-Z and 0-9 (default: config.json icon_text)")
	parser.add_argument("--color", help="the background as #rrggbb, quoted in the shell (default: config.json "
		"icon_color, else " + default_color + ")")
	args = parser.parse_args()
	try:
		text = check_text(args.text if args.text is not None else configured("icon_text"))
		if args.color is not None:
			color, source = args.color, "--color"
		elif args.text is None or config_found():
			color, source = configured("icon_color"), "icon_color"
		else:
			color, source = default_color, "the default colour"
		background = check_color(color, source)
	except icon_error as error:
		print("ERROR " + str(error))
		return 1
	os.makedirs(icons_dir, exist_ok=True)
	total_bytes = 0
	started = time.perf_counter()
	for name, size in icon_sizes.items():
		data = png_bytes(size, text, background)
		with open(os.path.join(icons_dir, name), "wb") as handle:
			handle.write(data)
		total_bytes += len(data)
		print("wrote icons/{} ({}x{}, {} bytes, text {}, colour {})".format(name, size, size, len(data), text, color))
	elapsed_ms = (time.perf_counter() - started) * 1000.0
	print("bake cost: {:.0f} ms, {} bytes for {} icons".format(elapsed_ms, total_bytes, len(icon_sizes)))
	return 0


if __name__ == "__main__":
	sys.exit(main())
