#!/usr/bin/env python3
"""
Migrate play_game_sound calls from char-based CGame/IGameScreen wrappers
to direct audio_manager::get() singleton calls with sound_type enum.

Transforms:
  m_game->play_game_sound('E', 14, 5)       => audio_manager::get().play_game_sound(sound_type::effect, 14, 5)
  m_game->play_game_sound('E', 14, 5, 0)    => audio_manager::get().play_game_sound(sound_type::effect, 14, 5, 0)
  play_game_sound('E', 14, 5)               => audio_manager::get().play_game_sound(sound_type::effect, 14, 5)
  (IGameScreen subclass calls without m_game->)

Also renames enum values in AudioManager.h/cpp and existing callers:
  sound_type::Character => sound_type::character
  sound_type::Monster   => sound_type::monster
  sound_type::Effect    => sound_type::effect

Also ensures #include "AudioManager.h" is present in modified files.

Usage:
  python Scripts/migrate_play_game_sound.py --dry-run
  python Scripts/migrate_play_game_sound.py --verify
  python Scripts/migrate_play_game_sound.py
"""

import argparse
import os
import re
import subprocess
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CLIENT_DIR = os.path.join(REPO_ROOT, "Sources", "Client")
OUTPUT_DIR = os.path.join(REPO_ROOT, "Scripts", "output")

CHAR_MAP = {
	"'C'": "sound_type::character",
	"'M'": "sound_type::monster",
	"'E'": "sound_type::effect",
}

# Old PascalCase enum values -> new snake_case
ENUM_RENAMES = {
	"sound_type::Character": "sound_type::character",
	"sound_type::Monster": "sound_type::monster",
	"sound_type::Effect": "sound_type::effect",
}

# Pattern: optional m_game-> prefix, play_game_sound( char_literal, rest... )
# Captures: (prefix)(char_literal)(arguments_after_char)
CALL_PATTERN = re.compile(
	r'(m_game->|)play_game_sound\(\s*'
	r"('[CME]')"       # char literal
	r'\s*,'             # comma
	r'\s*(.+?)\)'       # remaining args + closing paren
)

# Files to skip (they define the wrappers, not call them)
SKIP_FILES_FOR_CALLS = {"Game.cpp", "IGameScreen.cpp"}

# Files where we remove the wrapper declarations/definitions
WRAPPER_FILES = {
	"Game.h": "declaration",
	"Game.cpp": "definition",
	"IGameScreen.h": "declaration",
	"IGameScreen.cpp": "definition",
}


def find_cpp_files(directory):
	"""Find all .cpp and .h files in directory."""
	results = []
	for root, dirs, files in os.walk(directory):
		for f in files:
			if f.endswith((".cpp", ".h")):
				results.append(os.path.join(root, f))
	return sorted(results)


def needs_audio_include(content, filename):
	"""Check if file needs #include AudioManager.h added."""
	if '#include "AudioManager.h"' in content:
		return False
	if filename == "AudioManager.h" or filename == "AudioManager.cpp":
		return False
	return True


def add_audio_include(content, filename):
	"""Add #include AudioManager.h after the last project #include block."""
	if not needs_audio_include(content, filename):
		return content

	lines = content.split('\n')
	# Find last #include "..." line (project includes)
	last_project_include = -1
	for i, line in enumerate(lines):
		if re.match(r'\s*#include\s+"[^"]*"', line):
			last_project_include = i

	if last_project_include >= 0:
		lines.insert(last_project_include + 1, '#include "AudioManager.h"')
	else:
		# No project includes found, add after first #include
		for i, line in enumerate(lines):
			if line.startswith('#include'):
				lines.insert(i + 1, '#include "AudioManager.h"')
				break

	return '\n'.join(lines)


def transform_calls(content, filename):
	"""Replace char-based play_game_sound calls with audio_manager singleton calls."""
	changes = []
	basename = os.path.basename(filename)

	if basename in SKIP_FILES_FOR_CALLS:
		return content, changes

	def replacer(match):
		prefix = match.group(1)      # "m_game->" or ""
		char_lit = match.group(2)    # "'C'" / "'M'" / "'E'"
		rest_args = match.group(3)   # "14, 5" or "14, 5, 0"

		sound_enum = CHAR_MAP.get(char_lit)
		if not sound_enum:
			return match.group(0)

		new_call = f"audio_manager::get().play_game_sound({sound_enum}, {rest_args})"
		changes.append((match.group(0), new_call))
		return new_call

	new_content = CALL_PATTERN.sub(replacer, content)
	return new_content, changes


def rename_enum_values(content, filename):
	"""Rename PascalCase enum values to snake_case."""
	changes = []
	for old, new in ENUM_RENAMES.items():
		count = content.count(old)
		if count > 0:
			content = content.replace(old, new)
			changes.append((old, new, count))
	return content, changes


def rename_enum_declarations(content, filename):
	"""Rename enum value declarations in AudioManager.h."""
	changes = []
	basename = os.path.basename(filename)
	if basename != "AudioManager.h":
		return content, changes

	decl_renames = {
		"Character,": "character,",
		"Monster,": "monster,",
		"Effect": "effect",
	}
	for old, new in decl_renames.items():
		if old in content:
			content = content.replace(old, new)
			changes.append((old, new))

	return content, changes


def remove_wrapper_declaration_game_h(content):
	"""Remove play_game_sound declaration from Game.h."""
	# Match the declaration line
	pattern = re.compile(r'\t*void play_game_sound\(char type.*?;\s*//.*\n?', re.MULTILINE)
	match = pattern.search(content)
	if match:
		content = content[:match.start()] + content[match.end():]
		return content, True

	# Try without comment
	pattern = re.compile(r'\t*void play_game_sound\(char type.*?;\n?', re.MULTILINE)
	match = pattern.search(content)
	if match:
		content = content[:match.start()] + content[match.end():]
		return content, True

	return content, False


def remove_wrapper_declaration_igamescreen_h(content):
	"""Remove play_game_sound declaration from IGameScreen.h."""
	# Remove the audio helpers section comment + declaration
	pattern = re.compile(
		r'\s*// Audio helpers\n'
		r'\s*void play_game_sound\(char type.*?;\n',
		re.MULTILINE
	)
	match = pattern.search(content)
	if match:
		content = content[:match.start()] + "\n" + content[match.end():]
		return content, True

	# Try just the declaration
	pattern = re.compile(r'\s*void play_game_sound\(char type.*?;\n', re.MULTILINE)
	match = pattern.search(content)
	if match:
		content = content[:match.start()] + content[match.end():]
		return content, True

	return content, False


def remove_wrapper_definition_game_cpp(content):
	"""Remove play_game_sound definition from Game.cpp."""
	pattern = re.compile(
		r'\nvoid CGame::play_game_sound\(char type.*?\n\}\n',
		re.DOTALL
	)
	match = pattern.search(content)
	if match:
		content = content[:match.start()] + "\n" + content[match.end():]
		return content, True
	return content, False


def remove_wrapper_definition_igamescreen_cpp(content):
	"""Remove play_game_sound definition from IGameScreen.cpp."""
	# Remove the section comment + definition
	pattern = re.compile(
		r'\n// =+ Audio Helpers =+\n\n'
		r'void IGameScreen::play_game_sound\(char type.*?\n\}\n',
		re.DOTALL
	)
	match = pattern.search(content)
	if match:
		content = content[:match.start()] + "\n" + content[match.end():]
		return content, True

	# Try just the definition
	pattern = re.compile(
		r'\nvoid IGameScreen::play_game_sound\(char type.*?\n\}\n',
		re.DOTALL
	)
	match = pattern.search(content)
	if match:
		content = content[:match.start()] + "\n" + content[match.end():]
		return content, True

	return content, False


def process_file(filepath, dry_run=False):
	"""Process a single file. Returns (changes_description, modified_content) or None."""
	with open(filepath, 'r', encoding='utf-8', errors='replace') as f:
		original = f.read()

	content = original
	basename = os.path.basename(filepath)
	all_changes = []

	# 1. Transform char-based calls
	content, call_changes = transform_calls(content, filepath)
	if call_changes:
		all_changes.extend([f"  CALL: {old} => {new}" for old, new in call_changes])

	# 2. Rename enum values (PascalCase -> snake_case)
	content, enum_changes = rename_enum_values(content, filepath)
	if enum_changes:
		all_changes.extend([f"  ENUM: {old} => {new} ({count}x)" for old, new, count in enum_changes])

	# 3. Rename enum declarations (AudioManager.h only)
	content, decl_changes = rename_enum_declarations(content, filepath)
	if decl_changes:
		all_changes.extend([f"  DECL: {old} => {new}" for old, new in decl_changes])

	# 4. Remove wrapper declarations/definitions
	if basename == "Game.h":
		content, removed = remove_wrapper_declaration_game_h(content)
		if removed:
			all_changes.append("  REMOVE: play_game_sound(char...) declaration from Game.h")
	elif basename == "Game.cpp":
		content, removed = remove_wrapper_definition_game_cpp(content)
		if removed:
			all_changes.append("  REMOVE: play_game_sound(char...) definition from Game.cpp")
	elif basename == "IGameScreen.h":
		content, removed = remove_wrapper_declaration_igamescreen_h(content)
		if removed:
			all_changes.append("  REMOVE: play_game_sound(char...) declaration from IGameScreen.h")
	elif basename == "IGameScreen.cpp":
		content, removed = remove_wrapper_definition_igamescreen_cpp(content)
		if removed:
			all_changes.append("  REMOVE: play_game_sound(char...) definition from IGameScreen.cpp")

	# 5. Add #include "AudioManager.h" if we made call changes and it's needed
	if call_changes and needs_audio_include(content, basename):
		content = add_audio_include(content, basename)
		all_changes.append('  INCLUDE: added #include "AudioManager.h"')

	if content == original:
		return None

	return all_changes, content


def run_dry_run(files):
	"""Preview all changes without modifying files."""
	os.makedirs(OUTPUT_DIR, exist_ok=True)
	log_path = os.path.join(OUTPUT_DIR, "migrate_play_game_sound_dry_run.log")

	total_changes = 0
	files_changed = 0
	log_lines = ["=== Dry Run: migrate_play_game_sound.py ===\n"]

	for filepath in files:
		result = process_file(filepath, dry_run=True)
		if result is None:
			continue

		changes, _ = result
		files_changed += 1
		total_changes += len(changes)

		rel_path = os.path.relpath(filepath, REPO_ROOT)
		log_lines.append(f"\n{rel_path}: {len(changes)} change(s)")
		for c in changes:
			log_lines.append(c)

	summary = f"\nWould change: {total_changes} modifications across {files_changed} files"
	log_lines.append(f"\n--- Summary ---")
	log_lines.append(summary)

	log_content = "\n".join(log_lines)
	with open(log_path, 'w') as f:
		f.write(log_content)

	print(log_content)
	print(f"\nFull log: {log_path}")
	return 0


def run_verify(files):
	"""Scan for potential issues."""
	os.makedirs(OUTPUT_DIR, exist_ok=True)
	log_path = os.path.join(OUTPUT_DIR, "migrate_play_game_sound_verify.log")
	issues = []

	# Check for any remaining char-based calls that the regex might miss
	for filepath in files:
		with open(filepath, 'r', encoding='utf-8', errors='replace') as f:
			content = f.read()

		basename = os.path.basename(filepath)
		if basename in SKIP_FILES_FOR_CALLS:
			continue

		# Look for play_game_sound with char that doesn't match our pattern
		odd_calls = re.findall(r"play_game_sound\s*\(\s*'([^CME])'", content)
		if odd_calls:
			rel_path = os.path.relpath(filepath, REPO_ROOT)
			issues.append(f"[UNKNOWN CHAR] {rel_path}: play_game_sound with char '{odd_calls[0]}'")

	# Check for sound_type references outside Client
	shared_dir = os.path.join(REPO_ROOT, "Sources", "Dependencies", "Shared")
	sfml_dir = os.path.join(REPO_ROOT, "Sources", "SFMLEngine")
	for scan_dir, label in [(shared_dir, "SHARED"), (sfml_dir, "SFMLENGINE")]:
		if not os.path.isdir(scan_dir):
			continue
		for root, dirs, fnames in os.walk(scan_dir):
			for fn in fnames:
				if not fn.endswith((".cpp", ".h")):
					continue
				fp = os.path.join(root, fn)
				with open(fp, 'r', encoding='utf-8', errors='replace') as f:
					c = f.read()
				if "sound_type" in c:
					issues.append(f"[{label}] {os.path.relpath(fp, REPO_ROOT)}: references sound_type")

	# Check for duplicate include issues
	# (AudioManager.h is Client-only, shouldn't cause cross-scope issues)

	log_lines = ["=== Verify: migrate_play_game_sound.py ===\n"]
	if issues:
		log_lines.append("--- Issues Found ---")
		for issue in issues:
			log_lines.append(issue)
	else:
		log_lines.append("--- No issues found ---")

	log_lines.append(f"\n{len(issues)} issue(s) found")

	log_content = "\n".join(log_lines)
	with open(log_path, 'w') as f:
		f.write(log_content)

	print(log_content)
	print(f"\nFull log: {log_path}")
	return 0 if not issues else 1


def run_apply(files, skip_verify=False):
	"""Apply all changes."""
	if not skip_verify:
		verify_log = os.path.join(OUTPUT_DIR, "migrate_play_game_sound_verify.log")
		if not os.path.exists(verify_log):
			print("ERROR: Run --verify first (or use --skip-verify to bypass)")
			return 1

	# Collect files that will be modified
	modified_files = []
	results = []
	for filepath in files:
		result = process_file(filepath)
		if result is not None:
			modified_files.append(filepath)
			results.append((filepath, result))

	if not modified_files:
		print("No files need changes.")
		return 0

	# Guard all files via bak.py
	bak_py = os.path.join(REPO_ROOT, "bak.py")
	guard_cmd = [sys.executable, bak_py, "guard"] + modified_files
	print(f"Guarding {len(modified_files)} files...")
	subprocess.run(guard_cmd, check=True)

	# Apply changes
	for filepath, (changes, new_content) in results:
		rel_path = os.path.relpath(filepath, REPO_ROOT)
		print(f"  Writing {rel_path} ({len(changes)} changes)")
		with open(filepath, 'w', encoding='utf-8', newline='\n') as f:
			f.write(new_content)

	print(f"\nApplied changes to {len(modified_files)} files.")
	print("Build to verify, then run: python bak.py commit")
	return 0


def main():
	parser = argparse.ArgumentParser(description="Migrate play_game_sound to audio_manager singleton")
	parser.add_argument("--dry-run", action="store_true", help="Preview changes without modifying files")
	parser.add_argument("--verify", action="store_true", help="Scan for collisions")
	parser.add_argument("--skip-verify", action="store_true", help="Apply without requiring prior --verify")
	args = parser.parse_args()

	files = find_cpp_files(CLIENT_DIR)
	print(f"Scanning {len(files)} files in {CLIENT_DIR}\n")

	if args.verify:
		return run_verify(files)
	elif args.dry_run:
		return run_dry_run(files)
	else:
		return run_apply(files, skip_verify=args.skip_verify)


if __name__ == "__main__":
	sys.exit(main())
