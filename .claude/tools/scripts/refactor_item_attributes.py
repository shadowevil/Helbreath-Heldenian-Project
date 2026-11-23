"""
Mode 2 Script: Refactor CItem m_attribute references to individual fields.

This script handles the mechanical C++ code replacements needed after removing
the packed uint32_t m_attribute from CItem and replacing it with:
  m_custom_made, m_prefix_type, m_prefix_value,
  m_secondary_type, m_secondary_value, m_enchant_bonus

Patterns handled:
  1. item->m_attribute → NOT a simple replacement; needs context-specific handling
  2. Bitmask reads (enchant, prefix, secondary) → direct field access
  3. Bitmask writes (enchant) → direct field assignment
  4. pkt.attribute = item->m_attribute → 6-field copy
  5. Packet .attribute field access → 6 individual fields

Usage:
  python Scripts/refactor_item_attributes.py --dry-run
  python Scripts/refactor_item_attributes.py --verify
  python Scripts/refactor_item_attributes.py
"""

import re
import sys
import os

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.join(SCRIPT_DIR, "..")
OUTPUT_DIR = os.path.join(SCRIPT_DIR, "output")

log_lines = []

def log(msg):
	print(msg)
	log_lines.append(msg)


def read_file(path):
	with open(path, "r", encoding="utf-8", errors="replace") as f:
		return f.read()


def write_file(path, content):
	with open(path, "w", encoding="utf-8") as f:
		f.write(content)


# ============================================================
# Pattern definitions: each returns (old_text, new_text) pairs
# ============================================================

def fix_item_manager(content):
	"""Fix ItemManager.cpp — the heaviest file with ~120 references."""
	changes = []

	# ---- Pattern: enchant bonus READ ----
	# (item->m_attribute & 0xF0000000) >> 28
	# (item->m_attribute & 0x0F0000000) >> 28  (note: some have extra leading 0)
	# → item->m_enchant_bonus
	pattern = re.compile(
		r'\(([^)]+?)->m_attribute\s*&\s*0x0?F0000000\)\s*>>\s*28'
	)
	for m in pattern.finditer(content):
		old = m.group(0)
		obj = m.group(1)
		new = f'{obj}->m_enchant_bonus'
		changes.append((old, new))

	# ---- Pattern: prefix type CHECK ----
	# (item->m_attribute & 0x00F00000) != 0
	# → item->m_prefix_type != hb::shared::item::AttributePrefixType::None
	pattern = re.compile(
		r'\(([^)]+?)->m_attribute\s*&\s*0x00F00000\)\s*!=\s*0'
	)
	for m in pattern.finditer(content):
		old = m.group(0)
		obj = m.group(1)
		new = f'{obj}->m_prefix_type != hb::shared::item::AttributePrefixType::None'
		changes.append((old, new))

	# ---- Pattern: prefix type READ ----
	# (item->m_attribute & 0x00F00000) >> 20
	# → static_cast<int>(item->m_prefix_type)
	pattern = re.compile(
		r'\(([^)]+?)->m_attribute\s*&\s*0x00F00000\)\s*>>\s*20'
	)
	for m in pattern.finditer(content):
		old = m.group(0)
		obj = m.group(1)
		new = f'static_cast<int>({obj}->m_prefix_type)'
		changes.append((old, new))

	# ---- Pattern: prefix value READ ----
	# (item->m_attribute & 0x000F0000) >> 16
	# → item->m_prefix_value
	pattern = re.compile(
		r'\(([^)]+?)->m_attribute\s*&\s*0x000F0000\)\s*>>\s*16'
	)
	for m in pattern.finditer(content):
		old = m.group(0)
		obj = m.group(1)
		new = f'{obj}->m_prefix_value'
		changes.append((old, new))

	# ---- Pattern: secondary type CHECK ----
	# (item->m_attribute & 0x0000F000) != 0
	# → item->m_secondary_type != hb::shared::item::SecondaryEffectType::None
	pattern = re.compile(
		r'\(([^)]+?)->m_attribute\s*&\s*0x0000F000\)\s*!=\s*0'
	)
	for m in pattern.finditer(content):
		old = m.group(0)
		obj = m.group(1)
		new = f'{obj}->m_secondary_type != hb::shared::item::SecondaryEffectType::None'
		changes.append((old, new))

	# ---- Pattern: secondary type READ ----
	# (item->m_attribute & 0x0000F000) >> 12
	# → static_cast<int>(item->m_secondary_type)
	pattern = re.compile(
		r'\(([^)]+?)->m_attribute\s*&\s*0x0000F000\)\s*>>\s*12'
	)
	for m in pattern.finditer(content):
		old = m.group(0)
		obj = m.group(1)
		new = f'static_cast<int>({obj}->m_secondary_type)'
		changes.append((old, new))

	# ---- Pattern: secondary value READ ----
	# (item->m_attribute & 0x00000F00) >> 8
	# → item->m_secondary_value
	pattern = re.compile(
		r'\(([^)]+?)->m_attribute\s*&\s*0x00000F00\)\s*>>\s*8'
	)
	for m in pattern.finditer(content):
		old = m.group(0)
		obj = m.group(1)
		new = f'{obj}->m_secondary_value'
		changes.append((old, new))

	# ---- Pattern: custom-made CHECK ----
	# (item->m_attribute & 0x00000001) != 0
	# → item->m_custom_made
	pattern = re.compile(
		r'\(([^)]+?)->m_attribute\s*&\s*0x00000001\)\s*!=\s*0'
	)
	for m in pattern.finditer(content):
		old = m.group(0)
		obj = m.group(1)
		new = f'{obj}->m_custom_made'
		changes.append((old, new))

	# ---- Pattern: custom-made CHECK (== 0 form) ----
	# ((item->m_attribute & 0x00000001) == 0)
	# → (!item->m_custom_made)
	pattern = re.compile(
		r'\(\(([^)]+?)->m_attribute\s*&\s*0x00000001\)\s*==\s*0\)'
	)
	for m in pattern.finditer(content):
		old = m.group(0)
		obj = m.group(1)
		new = f'(!{obj}->m_custom_made)'
		changes.append((old, new))

	# ---- Pattern: has_special_attributes CHECK ----
	# (item->m_attribute & 0xF0F0F001) == 0
	# → !item->has_special_attributes()
	pattern = re.compile(
		r'\(([^)]+?)->m_attribute\s*&\s*0xF0F0F001\)\s*==\s*0'
	)
	for m in pattern.finditer(content):
		old = m.group(0)
		obj = m.group(1)
		new = f'!{obj}->has_special_attributes()'
		changes.append((old, new))

	# ---- Pattern: enchant bonus WRITE (clear & set) ----
	# temp = item->m_attribute;
	# <optional line>
	# item->m_attribute = temp | (value << 28);
	# This is complex multiline - handle via line-by-line pass later

	# ---- Pattern: m_attribute = 1 (custom made) ----
	# item->m_attribute = 1;
	# → item->m_custom_made = true;
	pattern = re.compile(r'(\w[\w\->\.]+)->m_attribute\s*=\s*1\s*;')
	for m in pattern.finditer(content):
		old = m.group(0)
		obj = m.group(1)
		new = f'{obj}->m_custom_made = true;'
		changes.append((old, new))

	# ---- Pattern: m_attribute != 0 ----
	# item->m_attribute != 0
	# → item->has_special_attributes()
	pattern = re.compile(r'(\w[\w\->\.]+)->m_attribute\s*!=\s*0')
	for m in pattern.finditer(content):
		old = m.group(0)
		obj = m.group(1)
		new = f'{obj}->has_special_attributes()'
		changes.append((old, new))

	# Apply all changes
	for old, new in changes:
		if old in content:
			content = content.replace(old, new, 1)
			log(f"  REPLACE: {old[:80]}...")
			log(f"     WITH: {new[:80]}...")

	return content


def fix_enchant_write_patterns(content):
	"""Fix the enchant bonus write pattern: temp = attr; attr = temp | (value << 28);
	Replace with: item->m_enchant_bonus = value;"""
	changes = 0
	lines = content.split('\n')
	new_lines = []
	i = 0
	while i < len(lines):
		line = lines[i]

		# Pattern: temp = item->m_attribute;
		m = re.match(r'(\s+)(\w+)\s*=\s*([^;]+)->m_attribute\s*;', line)
		if m:
			indent = m.group(1)
			temp_var = m.group(2)
			obj = m.group(3)

			# Look ahead for: item->m_attribute = temp | (value << 28);
			if i + 1 < len(lines):
				next_line = lines[i + 1]
				m2 = re.match(
					rf'\s+{re.escape(obj)}->m_attribute\s*=\s*{re.escape(temp_var)}\s*\|\s*\((\w+)\s*<<\s*28\)\s*;',
					next_line
				)
				if m2:
					val_var = m2.group(1)
					# Replace both lines with single line
					new_lines.append(f'{indent}{obj}->m_enchant_bonus = {val_var};')
					log(f"  ENCHANT WRITE: lines {i+1}-{i+2}: {obj}->m_enchant_bonus = {val_var};")
					changes += 1
					i += 2
					continue
			# Look ahead 2 lines (might have a blank line or comment between)
			if i + 2 < len(lines):
				next_next = lines[i + 2]
				m2 = re.match(
					rf'\s+{re.escape(obj)}->m_attribute\s*=\s*{re.escape(temp_var)}\s*\|\s*\((\w+)\s*<<\s*28\)\s*;',
					next_next
				)
				if m2:
					val_var = m2.group(1)
					new_lines.append(f'{indent}{obj}->m_enchant_bonus = {val_var};')
					# Keep the middle line if it's not just blank
					middle = lines[i + 1].strip()
					if middle and not middle.startswith('//'):
						new_lines.append(lines[i + 1])
					log(f"  ENCHANT WRITE: lines {i+1},{i+3}: {obj}->m_enchant_bonus = {val_var};")
					changes += 1
					i += 3
					continue

		new_lines.append(line)
		i += 1

	if changes > 0:
		content = '\n'.join(new_lines)
	return content, changes


def fix_pkt_attribute_assign(content, filename):
	"""Fix pkt.attribute = item->m_attribute → 6 field assignments."""
	# Pattern: pkt.attribute = item->m_attribute;
	pattern = re.compile(r'(\s+)(\w+)\.attribute\s*=\s*([^;]+)->m_attribute\s*;')
	matches = list(pattern.finditer(content))

	for m in reversed(matches):  # reverse to preserve offsets
		indent = m.group(1)
		pkt = m.group(2)
		obj = m.group(3)
		old = m.group(0)
		new = (
			f'{indent}{pkt}.custom_made = {obj}->m_custom_made ? 1 : 0;'
			f'{indent}{pkt}.prefix_type = static_cast<uint8_t>({obj}->m_prefix_type);'
			f'{indent}{pkt}.prefix_value = {obj}->m_prefix_value;'
			f'{indent}{pkt}.secondary_type = static_cast<uint8_t>({obj}->m_secondary_type);'
			f'{indent}{pkt}.secondary_value = {obj}->m_secondary_value;'
			f'{indent}{pkt}.enchant_bonus = {obj}->m_enchant_bonus;'
		)
		content = content[:m.start()] + new + content[m.end():]
		log(f"  PKT ATTRIBUTE ({filename}): {pkt}.attribute = {obj}->m_attribute")

	return content


def fix_pkt_attribute_from_var(content, filename):
	"""Fix pkt.attribute = variable (not item->)."""
	# Pattern: pkt.attribute = static_cast<uint32_t>(v2);
	# or: pkt.attribute = v8;
	# These are in the send_notify_msg handler — need special handling
	return content


def fix_send_event_calls(content):
	"""Fix send_event_to_near_client_type_b calls that pass item->m_attribute as last arg.
	Replace item->m_attribute with item->m_enchant_bonus (the only visually relevant field for ground display)."""

	# Pattern: item->m_attribute); // at end of send_event call
	# The attribute was used for ground item display — enchant_bonus is all that matters for visual
	content = re.sub(
		r'(\w[\w\->\.]+)->m_attribute\)',
		r'\1->m_enchant_bonus)',
		content
	)
	return content


def fix_copy_attribute(content):
	"""Fix copy->m_attribute = original->m_attribute → copy all 6 fields."""
	pattern = re.compile(r'(\s+)(\w+)->m_attribute\s*=\s*(\w+)->m_attribute\s*;')
	matches = list(pattern.finditer(content))

	for m in reversed(matches):
		indent = m.group(1)
		dst = m.group(2)
		src = m.group(3)
		new = (
			f'{indent}{dst}->m_custom_made = {src}->m_custom_made;'
			f'{indent}{dst}->m_prefix_type = {src}->m_prefix_type;'
			f'{indent}{dst}->m_prefix_value = {src}->m_prefix_value;'
			f'{indent}{dst}->m_secondary_type = {src}->m_secondary_type;'
			f'{indent}{dst}->m_secondary_value = {src}->m_secondary_value;'
			f'{indent}{dst}->m_enchant_bonus = {src}->m_enchant_bonus;'
		)
		content = content[:m.start()] + new + content[m.end():]
		log(f"  COPY: {dst}->m_attribute = {src}->m_attribute")

	return content


def fix_remaining_m_attribute_in_log(content):
	"""Fix m_attribute references in log/format strings — just use enchant_bonus as representative."""
	# Pattern in log: item->m_attribute (as a function argument in a log call)
	# These are typically in debug logging or as parameters to format strings
	# Replace with a meaningful value — enchant_bonus is the most commonly logged attribute
	return content


def fix_db_item_attribute_assign(content):
	"""Fix lines where item->m_attribute is assigned from DB row (item.attribute)."""
	# Pattern: item->m_attribute = item.attribute;
	# → assign all 6 fields from DB row
	pattern = re.compile(
		r'(\s+)(\w[\w\->\.]+)->m_attribute\s*=\s*(\w+)\.attribute\s*;'
	)
	matches = list(pattern.finditer(content))

	for m in reversed(matches):
		indent = m.group(1)
		obj = m.group(2)
		row = m.group(3)
		new = (
			f'{indent}{obj}->m_custom_made = {row}.custom_made != 0;'
			f'{indent}{obj}->m_prefix_type = static_cast<hb::shared::item::AttributePrefixType>({row}.prefix_type);'
			f'{indent}{obj}->m_prefix_value = static_cast<uint8_t>({row}.prefix_value);'
			f'{indent}{obj}->m_secondary_type = static_cast<hb::shared::item::SecondaryEffectType>({row}.secondary_type);'
			f'{indent}{obj}->m_secondary_value = static_cast<uint8_t>({row}.secondary_value);'
			f'{indent}{obj}->m_enchant_bonus = static_cast<uint8_t>({row}.enchant_bonus);'
		)
		content = content[:m.start()] + new + content[m.end():]
		log(f"  DB ASSIGN: {obj}->m_attribute = {row}.attribute")

	return content


def process_file(filepath, dry_run):
	"""Process a single file."""
	rel = os.path.relpath(filepath, ROOT_DIR)
	content = read_file(filepath)
	original = content

	# Check if file actually has CItem m_attribute references
	if 'm_attribute' not in content and '.attribute' not in content:
		return 0

	log(f"\n--- {rel} ---")

	basename = os.path.basename(filepath)

	# Skip non-CItem attribute references
	skip_files = {"Npc.cpp", "Npc.h", "Tile.cpp", "Tile.h", "Magic.h",
	              "BuildItem.h", "WarManager.cpp", "Map.h"}
	if basename in skip_files:
		log(f"  Skipped (non-CItem m_attribute)")
		return 0

	# Apply pattern fixes
	content = fix_item_manager(content)
	content, ec = fix_enchant_write_patterns(content)
	content = fix_pkt_attribute_assign(content, rel)
	content = fix_copy_attribute(content)
	content = fix_send_event_calls(content)
	content = fix_db_item_attribute_assign(content)

	if content == original:
		log(f"  No changes needed")
		return 0

	if dry_run:
		log(f"  [DRY RUN] Would modify file")
	else:
		write_file(filepath, content)
		log(f"  File modified")

	# Count changes
	change_count = sum(1 for a, b in zip(original.split('\n'), content.split('\n')) if a != b)
	# Also count lines that were added/removed
	orig_lines = len(original.split('\n'))
	new_lines = len(content.split('\n'))
	change_count += abs(new_lines - orig_lines)
	log(f"  ~{change_count} lines changed")
	return change_count


def main():
	dry_run = "--dry-run" in sys.argv
	do_verify = "--verify" in sys.argv

	os.makedirs(OUTPUT_DIR, exist_ok=True)

	if do_verify:
		log("=== refactor_item_attributes.py [VERIFY] ===")
		errors = 0
		# Check that no CItem m_attribute references remain
		server_dir = os.path.join(ROOT_DIR, "Sources", "Server")
		client_dir = os.path.join(ROOT_DIR, "Sources", "Client")
		for d in [server_dir, client_dir]:
			for root, dirs, files in os.walk(d):
				for f in files:
					if not f.endswith(('.cpp', '.h')):
						continue
					if '.bak' in f:
						continue
					path = os.path.join(root, f)
					content = read_file(path)
					# Skip non-CItem files
					if f in {"Npc.cpp", "Npc.h", "Tile.cpp", "Tile.h", "Magic.h",
					         "BuildItem.h", "WarManager.cpp", "Map.h"}:
						continue
					# Check for remaining m_attribute on CItem
					for i, line in enumerate(content.split('\n'), 1):
						if 'm_attribute' in line and 'CItem' not in line:
							# Could be legitimate (magic, npc, tile, build item)
							if any(x in line for x in ['magic', 'npc->', 'tile->', 'build']):
								continue
							if '->m_attribute' in line or '.m_attribute' in line:
								log(f"  REMAINING: {f}:{i}: {line.strip()}")
								errors += 1

		if errors == 0:
			log("\nVerification PASSED")
		else:
			log(f"\nVerification found {errors} remaining reference(s)")

		log_path = os.path.join(OUTPUT_DIR, "refactor_item_attributes_verify.log")
		with open(log_path, "w", encoding="utf-8") as f:
			f.write("\n".join(log_lines))
		log(f"\nLog written to: {log_path}")
		sys.exit(0 if errors == 0 else 1)

	mode = "DRY RUN" if dry_run else "APPLY"
	log(f"=== refactor_item_attributes.py [{mode}] ===")

	# Process server files
	server_files = [
		"Sources/Server/ItemManager.cpp",
		"Sources/Server/Game.cpp",
		"Sources/Server/CraftingManager.cpp",
		"Sources/Server/EntityManager.cpp",
		"Sources/Server/FishingManager.cpp",
		"Sources/Server/MagicManager.cpp",
		"Sources/Server/MiningManager.cpp",
		"Sources/Server/StatusEffectManager.cpp",
		"Sources/Server/Map.cpp",
		"Sources/Server/LoginServer.cpp",
	]

	# Process client files
	client_files = [
		"Sources/Client/Game.cpp",
		"Sources/Client/ItemNameFormatter.cpp",
		"Sources/Client/DialogBox_ItemUpgrade.cpp",
		"Sources/Client/DialogBox_Magic.cpp",
		"Sources/Client/InventoryManager.cpp",
		"Sources/Client/NetworkMessages_Items.cpp",
		"Sources/Client/NetworkMessages_Bank.cpp",
	]

	total_changes = 0
	for rel_path in server_files + client_files:
		full_path = os.path.join(ROOT_DIR, rel_path)
		if os.path.exists(full_path):
			total_changes += process_file(full_path, dry_run)
		else:
			log(f"\nWARN: {rel_path} not found")

	log(f"\nTotal: ~{total_changes} lines changed across {len(server_files) + len(client_files)} files")

	if dry_run:
		log("\nDry run complete. No changes made.")

	log_path = os.path.join(OUTPUT_DIR, f"refactor_item_attributes_{'dry_run' if dry_run else 'apply'}.log")
	with open(log_path, "w", encoding="utf-8") as f:
		f.write("\n".join(log_lines))
	log(f"\nLog written to: {log_path}")


if __name__ == "__main__":
	main()
