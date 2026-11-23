#!/usr/bin/env python3
"""
Phase 5a: Move draw methods from CGame to Screen_OnGame.

Reads Game.DrawObjects.cpp and Game.cpp, extracts the gameplay draw methods,
applies member-access transformations (CGame members -> m_game->member),
and writes Screen_OnGame.DrawObjects.cpp.

Usage:
    python move_draw_methods.py --dry-run    # Preview to Scripts/output/
    python move_draw_methods.py --verify     # Check for issues
    python move_draw_methods.py              # Apply changes
"""

import re
import sys
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CLIENT_DIR = os.path.join(ROOT, 'Sources', 'Client')
OUTPUT_DIR = os.path.join(ROOT, 'Scripts', 'output')

# CGame members that need m_game-> prefix (sorted longest first for safety)
GAME_MEMBERS = sorted([
	'm_entity_state', 'm_Camera', 'm_map_data', 'm_player', 'm_sprite', 'm_tile_spr',
	'm_effect_sprites', 'm_effect_manager', 'm_Renderer', 'm_cur_time', 'm_equip_sprites',
	'm_item_sprites', 'm_item_config_list', 'm_ilusion_owner_h', 'm_ilusion_owner_type',
	'm_player_rect', 'm_body_rect', 'm_is_xmas', 'm_env_effect_time', 'm_mcx', 'm_mcy',
	'm_mc_name', 'm_point_command_type', 'm_is_get_pointing_mode', 'm_is_observer_mode',
	'm_is_crusade_mode', 'm_crusade_structure_info', 'm_draw_flag', 'm_top_msg',
	'm_top_msg_time', 'm_top_msg_last_sec', 'm_map_index',
], key=len, reverse=True)

# Exception members that stay as direct access (Screen_OnGame-owned or special)
EXCEPTIONS = ['m_player_renderer', 'm_npc_renderer', 'm_floating_text', 'm_game']

# CGame methods called implicitly that need m_game-> prefix
GAME_METHODS = ['get_item_draw', 'draw_new_dialog_box']


def read_lines(filepath):
	with open(filepath, 'r', encoding='utf-8') as f:
		return f.readlines()


def extract_lines(lines, start, end):
	"""Extract lines (1-indexed, inclusive)."""
	return lines[start - 1:end]


def apply_member_transforms(code):
	"""Apply m_game-> prefix to CGame member accesses."""
	for member in GAME_MEMBERS:
		# Match member at word boundary, not preceded by . or > (avoids double-prefixing)
		pattern = r'(?<![.>])\b(' + re.escape(member) + r')\b'
		code = re.sub(pattern, r'm_game->\1', code)
	return code


def apply_method_transforms(code):
	"""Prefix implicit CGame method calls with m_game->."""
	for method in GAME_METHODS:
		# Match method name at word boundary, not preceded by . or > or :
		pattern = r'(?<![.>:])\b(' + re.escape(method) + r')\b(?=\s*\()'
		code = re.sub(pattern, r'm_game->\1', code)
	return code


def apply_class_rename(code):
	"""Change CGame:: to Screen_OnGame:: in method signatures."""
	return code.replace('CGame::', 'Screen_OnGame::')


def transform_code(code):
	"""Apply all transformations to extracted code."""
	code = apply_class_rename(code)
	code = apply_member_transforms(code)
	code = apply_method_transforms(code)
	return code


FILE_HEADER = '''\
// Screen_OnGame.DrawObjects.cpp: Screen_OnGame partial implementation — draw_objects coordinator + dispatchers
//
// Moved from Game.DrawObjects.cpp and Game.cpp as part of Phase 5a render pipeline migration.
//
//////////////////////////////////////////////////////////////////////

#include "Screen_OnGame.h"
#include "Game.h"
#include "WeatherManager.h"
#include "ItemNameFormatter.h"
#include "ItemTooltip.h"
#include "ItemSpriteMetadata.h"
#include "RenderHelpers.h"
#include "EntityRenderState.h"
#include "ConfigManager.h"
#include "GameFonts.h"
#include "TextLibExt.h"
#include "lan_eng.h"
#include "TeleportManager.h"



namespace dynamic_object = hb::shared::dynamic_object;

using namespace hb::shared::action;
using namespace hb::shared::direction;

using namespace hb::shared::item;
using namespace hb::client::config;
using namespace hb::client::sprite_id;

'''


def build_output():
	"""Build the Screen_OnGame.DrawObjects.cpp content."""
	draw_objects_path = os.path.join(CLIENT_DIR, 'Game.DrawObjects.cpp')
	game_cpp_path = os.path.join(CLIENT_DIR, 'Game.cpp')

	draw_objects_lines = read_lines(draw_objects_path)
	game_cpp_lines = read_lines(game_cpp_path)

	# Extract dispatchers (lines 204-292) and draw_objects (294-969)
	dispatchers = ''.join(extract_lines(draw_objects_lines, 204, 292))
	draw_objects = ''.join(extract_lines(draw_objects_lines, 294, 969))

	# Extract draw_background (lines 2301-2340) and draw_top_msg (2676-2687)
	draw_bg = ''.join(extract_lines(game_cpp_lines, 2301, 2340))
	draw_top = ''.join(extract_lines(game_cpp_lines, 2676, 2687))

	# Transform all extracted code
	dispatchers = transform_code(dispatchers)
	draw_objects = transform_code(draw_objects)
	draw_bg = transform_code(draw_bg)
	draw_top = transform_code(draw_top)

	# Compose the file
	output = FILE_HEADER
	output += dispatchers
	output += '\n'
	output += draw_objects
	output += '\n'
	output += draw_bg
	output += '\n'
	output += draw_top
	output += '\n'

	return output


def verify(output):
	"""Check for common issues in the transformed code."""
	issues = []

	# Check for double-prefixed members (m_game->m_game->)
	doubles = [m.start() for m in re.finditer(r'm_game->m_game->', output)]
	if doubles:
		issues.append(f"ERROR: Found {len(doubles)} double-prefixed m_game->m_game->")

	# Check that CGame:: is fully removed
	cgame_refs = [m.start() for m in re.finditer(r'CGame::', output)]
	if cgame_refs:
		issues.append(f"WARNING: Found {len(cgame_refs)} remaining CGame:: references")

	# Check exception members weren't prefixed
	for exc in EXCEPTIONS:
		pattern = r'm_game->' + re.escape(exc) + r'\b'
		matches = list(re.finditer(pattern, output))
		if matches:
			issues.append(f"ERROR: Exception member {exc} was incorrectly prefixed ({len(matches)} times)")

	# Check key transformations happened
	expected = [
		('m_game->m_entity_state', 'entity_state prefixing'),
		('m_game->m_map_data', 'map_data prefixing'),
		('m_game->m_Camera', 'Camera prefixing'),
		('m_game->m_player', 'player prefixing'),
		('m_game->m_Renderer', 'Renderer prefixing'),
		('m_game->m_top_msg', 'top_msg prefixing'),
		('Screen_OnGame::draw_objects', 'class rename for draw_objects'),
		('Screen_OnGame::draw_object_on_stop', 'class rename for dispatchers'),
		('Screen_OnGame::draw_background', 'class rename for draw_background'),
		('Screen_OnGame::draw_top_msg', 'class rename for draw_top_msg'),
		('m_game->get_item_draw', 'get_item_draw method prefixing'),
		('m_game->draw_new_dialog_box', 'draw_new_dialog_box method prefixing'),
		('m_player_renderer.draw_stop', 'player_renderer stays direct'),
		('m_npc_renderer.draw_stop', 'npc_renderer stays direct'),
	]
	for exp, desc in expected:
		if exp not in output:
			issues.append(f"WARNING: Expected '{exp}' not found ({desc})")

	# Verify dispatchers use direct m_player_renderer/m_npc_renderer (not m_game->)
	lines = output.split('\n')
	for i, line in enumerate(lines, 1):
		if 'm_game->m_player_renderer' in line:
			issues.append(f"ERROR: Line {i}: m_player_renderer incorrectly prefixed: {line.strip()}")
		if 'm_game->m_npc_renderer' in line:
			issues.append(f"ERROR: Line {i}: m_npc_renderer incorrectly prefixed: {line.strip()}")

	return issues


def main():
	dry_run = '--dry-run' in sys.argv
	verify_mode = '--verify' in sys.argv

	output = build_output()
	issues = verify(output)

	# Count transformations
	prefix_count = output.count('m_game->')
	line_count = output.count('\n')

	if verify_mode:
		print("=== Verification Results ===")
		if not issues:
			print("PASS: No issues found")
		for issue in issues:
			print(f"  {issue}")
		print(f"\nTotal m_game-> prefixes: {prefix_count}")
		print(f"Output lines: {line_count}")

		has_errors = any(i.startswith('ERROR') for i in issues)
		return 1 if has_errors else 0

	if dry_run:
		os.makedirs(OUTPUT_DIR, exist_ok=True)
		out_path = os.path.join(OUTPUT_DIR, 'Screen_OnGame.DrawObjects.cpp.preview')
		with open(out_path, 'w', encoding='utf-8') as f:
			f.write(output)
		print(f"Preview written to: {out_path}")
		print(f"Output lines: {line_count}")
		print(f"Total m_game-> prefixes: {prefix_count}")
		if issues:
			print("\nIssues:")
			for issue in issues:
				print(f"  {issue}")
		return 0

	# Apply
	if any(i.startswith('ERROR') for i in issues):
		print("ERROR: Cannot apply — verification errors found:")
		for issue in issues:
			print(f"  {issue}")
		return 1

	out_path = os.path.join(CLIENT_DIR, 'Screen_OnGame.DrawObjects.cpp')
	with open(out_path, 'w', encoding='utf-8') as f:
		f.write(output)
	print(f"Written: {out_path}")
	print(f"Output lines: {line_count}")
	print(f"Total m_game-> prefixes: {prefix_count}")
	if issues:
		print("\nWarnings (review manually):")
		for issue in issues:
			print(f"  {issue}")
	return 0


if __name__ == '__main__':
	sys.exit(main())
