#!/usr/bin/env python3
"""
Item Database Viewer — visualizes how each DB item renders using its display_id mapping.

Shows all items from gameconfigs.db with:
  - Equipped animation preview (direction cycling from per-item PAK)
  - Equip icon, Ground icon, Inventory icon (from item_atlas.pak)
  - Display ID assignment for unmapped items
  - Click icon sections to open Frame Mapper GUI for remapping

Usage:
    python item_db_viewer.py
    python item_db_viewer.py --db <path> --metadata <path> --items-dir <path>
"""

import tkinter as tk
from tkinter import ttk, messagebox
import json
import sys
import os
import subprocess
import argparse
import sqlite3
from pathlib import Path
from PIL import Image, ImageTk, ImageDraw

# paklib.py lives alongside this script
_SCRIPT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(_SCRIPT_DIR))
from paklib import PAKFile

DEFAULT_DB = _SCRIPT_DIR.parent.parent / 'Binaries' / 'Server' / 'gamedata.db'
DEFAULT_METADATA = _SCRIPT_DIR / 'contents' / 'ItemSpriteMetadata.json'
DEFAULT_ITEMS_DIR = _SCRIPT_DIR / 'sprites' / 'items'
DEFAULT_ATLAS = _SCRIPT_DIR / 'sprites' / 'item_atlas.pak'

# Colors
BG_DARK = '#1e1e1e'
BG_MID = '#2d2d2d'
BG_LIGHT = '#3c3c3c'
FG_TEXT = '#d4d4d4'
FG_DIM = '#808080'
GREEN = '#4ec94e'
YELLOW = '#cccc00'
RED = '#cc4444'
BLUE = '#4a9eff'
ORANGE = '#ff8c00'

DIR_NAMES = ['N', 'NE', 'E', 'SE', 'S', 'SW', 'W', 'NW']

# Atlas sprite indices
ATLAS_EQUIP = 0   # Sprite 0: equip icons
ATLAS_GROUND = 1  # Sprite 1: ground icons
ATLAS_PACK = 2    # Sprite 2: pack/inventory icons

EQUIP_POS_NAMES = {
	0: 'None',
	1: 'Head',
	2: 'Body',
	3: 'Arms',
	4: 'Pants',
	5: 'Boots',
	7: 'LeftHand',
	8: 'RightHand',
	9: 'TwoHand',
	10: 'Neck',
	11: 'Ring',
	12: 'Back',
	13: 'FullBody',
	14: 'Accessory',
}


def extract_atlas_frame(atlas_pak, atlas_idx, rect_idx):
	"""Extract a single frame from the atlas PAK by sprite index and rectangle index."""
	if atlas_idx >= len(atlas_pak.sprites):
		return None
	sprite = atlas_pak.sprites[atlas_idx]
	if rect_idx < 0 or rect_idx >= len(sprite.rectangles):
		return None
	img = sprite.get_image().convert('RGBA')
	r = sprite.rectangles[rect_idx]
	return img.crop((r.x, r.y, r.x + r.width, r.y + r.height))


class ItemDBViewer:
	def __init__(self, root, db_path, metadata_path, items_dir, atlas_path):
		self.root = root
		self.root.title('Item Database Viewer')
		self.root.configure(bg=BG_DARK)
		self.root.geometry('1200x750')

		self.db_path = Path(db_path)
		self.metadata_path = Path(metadata_path)
		self.items_dir = Path(items_dir)
		self.pak_cache = {}
		self._preview_timer = None
		self._preview_dir = 0
		self._preview_tk = None    # prevent GC
		self._equip_tk = None      # prevent GC
		self._ground_tk = None     # prevent GC
		self._pack_tk = None       # prevent GC
		self._current_item = None  # currently displayed DB item

		# Load data
		self._load_db(db_path)
		self._load_metadata(metadata_path)
		self._load_atlas(atlas_path)
		self._build_meta_choices()
		self._build_ui()
		self._populate_list()

		# Start metadata file polling (check every 2s)
		self._meta_mtime = self._get_meta_mtime()
		self._poll_metadata()

	def _get_meta_mtime(self):
		try:
			return os.path.getmtime(self.metadata_path)
		except OSError:
			return 0

	def _load_db(self, db_path):
		"""Load all items from the database."""
		db = sqlite3.connect(str(db_path))
		db.row_factory = sqlite3.Row
		cur = db.cursor()
		cur.execute(
			'SELECT item_id, name, equip_pos, '
			'gender_requirement, display_id '
			'FROM items ORDER BY item_id'
		)
		self.db_items = [dict(row) for row in cur.fetchall()]
		db.close()

	def _load_metadata(self, metadata_path):
		"""Load ItemSpriteMetadata and build display_id -> entry lookup."""
		with open(metadata_path) as f:
			meta = json.load(f)
		self.meta_by_id = {}
		self.meta_list = meta
		for entry in meta:
			self.meta_by_id[entry['id']] = entry

	def _load_atlas(self, atlas_path):
		"""Load the item atlas PAK (equip/ground/pack sprite sheets)."""
		self.atlas = None
		path = Path(atlas_path)
		if path.exists():
			self.atlas = PAKFile.read(path)

	def _build_meta_choices(self):
		"""Build the list of metadata choices for the display_id combobox."""
		self.meta_choices = []
		self.meta_choice_ids = []
		for entry in sorted(self.meta_list, key=lambda e: e['id']):
			pak = entry.get('pak_file') or ''
			name = entry.get('name') or pak.replace('.pak', '') or f"Item #{entry['id']}"
			etype = entry.get('equip_type', '?')
			label = f"{entry['id']:>3d} | {etype:6s} | {name}"
			self.meta_choices.append(label)
			self.meta_choice_ids.append(entry['id'])

	def _get_meta_mtime(self):
		"""Get modification time of the metadata file."""
		try:
			return os.path.getmtime(self.metadata_path)
		except OSError:
			return 0

	def _poll_metadata(self):
		"""Check if metadata file changed on disk and reload if so."""
		mtime = self._get_meta_mtime()
		if mtime != self._meta_mtime:
			self._meta_mtime = mtime
			self._reload_metadata()
		self.root.after(2000, self._poll_metadata)

	def _reload_metadata(self):
		"""Reload metadata from disk and refresh the current view."""
		try:
			self._load_metadata(self.metadata_path)
			self._build_meta_choices()
		except Exception as e:
			print(f'Metadata reload error: {e}')
			return
		# Refresh current item if one is selected
		sel = self.listbox.curselection()
		if sel:
			idx = self._visible_indices[sel[0]]
			item = self.db_items[idx]
			self._show_item(item)

	def _build_ui(self):
		# Top filter bar
		filter_frame = tk.Frame(self.root, bg=BG_MID, padx=5, pady=3)
		filter_frame.pack(fill=tk.X)

		tk.Label(filter_frame, text='Filter:', bg=BG_MID, fg=FG_TEXT,
				 font=('Consolas', 10)).pack(side=tk.LEFT)
		self._filter_var = tk.StringVar()
		self._filter_var.trace_add('write', lambda *_: self._on_filter_change())
		filter_entry = tk.Entry(filter_frame, textvariable=self._filter_var,
								bg=BG_LIGHT, fg=FG_TEXT, insertbackground=FG_TEXT,
								font=('Consolas', 10), relief=tk.FLAT)
		filter_entry.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(5, 10))

		# Stats label
		self._stats_label = tk.Label(filter_frame, text='', bg=BG_MID, fg=FG_DIM,
									 font=('Consolas', 9))
		self._stats_label.pack(side=tk.RIGHT)

		# Filter buttons
		self._filter_mode = tk.StringVar(value='all')
		for text, val in [('All', 'all'), ('Mapped', 'mapped'),
						  ('Unmapped', 'unmapped'), ('Equippable', 'equip')]:
			rb = tk.Radiobutton(filter_frame, text=text, variable=self._filter_mode,
								value=val, bg=BG_MID, fg=FG_TEXT,
								selectcolor=BG_LIGHT, activebackground=BG_MID,
								activeforeground=FG_TEXT, font=('Consolas', 9),
								command=self._on_filter_change)
			rb.pack(side=tk.LEFT, padx=2)

		# Main paned window
		paned = ttk.PanedWindow(self.root, orient=tk.HORIZONTAL)
		paned.pack(fill=tk.BOTH, expand=True)

		# Left: item list
		left = tk.Frame(paned, bg=BG_DARK)
		paned.add(left, weight=1)

		self.listbox = tk.Listbox(left, bg=BG_MID, fg=FG_TEXT,
								  selectbackground=BLUE, selectforeground='white',
								  font=('Consolas', 9), activestyle='none',
								  relief=tk.FLAT, borderwidth=0)
		scrollbar = tk.Scrollbar(left, orient=tk.VERTICAL, command=self.listbox.yview)
		self.listbox.configure(yscrollcommand=scrollbar.set)
		scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
		self.listbox.pack(fill=tk.BOTH, expand=True)
		self.listbox.bind('<<ListboxSelect>>', self._on_select)

		# Right: detail panel
		right = tk.Frame(paned, bg=BG_DARK)
		paned.add(right, weight=2)

		# Item info section
		info_frame = tk.Frame(right, bg=BG_MID, padx=10, pady=8)
		info_frame.pack(fill=tk.X, padx=5, pady=(5, 0))

		self._info_name = tk.Label(info_frame, text='', bg=BG_MID, fg=FG_TEXT,
								   font=('Consolas', 14, 'bold'), anchor='w')
		self._info_name.pack(fill=tk.X)

		self._info_details = tk.Label(info_frame, text='', bg=BG_MID, fg=FG_DIM,
									  font=('Consolas', 10), anchor='w', justify=tk.LEFT)
		self._info_details.pack(fill=tk.X, pady=(4, 0))

		self._info_mapping = tk.Label(info_frame, text='', bg=BG_MID, fg=GREEN,
									  font=('Consolas', 10), anchor='w', justify=tk.LEFT)
		self._info_mapping.pack(fill=tk.X, pady=(4, 0))

		# Display ID assignment row
		assign_frame = tk.Frame(info_frame, bg=BG_MID)
		assign_frame.pack(fill=tk.X, pady=(6, 0))

		tk.Label(assign_frame, text='Display ID:', bg=BG_MID, fg=FG_TEXT,
				 font=('Consolas', 9)).pack(side=tk.LEFT)

		self._did_var = tk.StringVar()
		self._did_combo = ttk.Combobox(assign_frame, textvariable=self._did_var,
										values=self.meta_choices, font=('Consolas', 9),
										width=40)
		self._did_combo.pack(side=tk.LEFT, padx=(5, 3))
		self._did_combo.bind('<KeyRelease>', self._on_did_filter)

		self._did_set_btn = tk.Button(assign_frame, text='Set', font=('Consolas', 9),
									   command=self._on_set_display_id, padx=8)
		self._did_set_btn.pack(side=tk.LEFT, padx=2)

		self._did_clear_btn = tk.Button(assign_frame, text='Clear', font=('Consolas', 9),
										 fg=RED, command=self._on_clear_display_id, padx=6)
		self._did_clear_btn.pack(side=tk.LEFT, padx=2)

		# Bottom section: icons (left) + animated preview (right)
		bottom = tk.Frame(right, bg=BG_DARK)
		bottom.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)

		# Icon panel (equip, ground, inventory) — clickable to open Frame Mapper
		icon_panel = tk.Frame(bottom, bg=BG_DARK)
		icon_panel.pack(side=tk.LEFT, fill=tk.Y, padx=(0, 5))

		# Equip icon
		equip_lf = tk.LabelFrame(icon_panel, text=' Equip (click to remap) ',
								 bg=BG_DARK, fg=FG_DIM,
								 font=('Consolas', 9), padx=4, pady=4)
		equip_lf.pack(fill=tk.X, pady=(0, 4))
		self._equip_canvas = tk.Canvas(equip_lf, bg=BG_MID, highlightthickness=0,
									   width=120, height=120, cursor='hand2')
		self._equip_canvas.pack()
		self._equip_canvas.bind('<Button-1>',
								lambda e: self._on_icon_click(ATLAS_EQUIP))

		# Ground icon
		ground_lf = tk.LabelFrame(icon_panel, text=' Ground (click to remap) ',
								  bg=BG_DARK, fg=FG_DIM,
								  font=('Consolas', 9), padx=4, pady=4)
		ground_lf.pack(fill=tk.X, pady=(0, 4))
		self._ground_canvas = tk.Canvas(ground_lf, bg=BG_MID, highlightthickness=0,
										width=120, height=120, cursor='hand2')
		self._ground_canvas.pack()
		self._ground_canvas.bind('<Button-1>',
								 lambda e: self._on_icon_click(ATLAS_GROUND))

		# Pack/inventory icon
		pack_lf = tk.LabelFrame(icon_panel, text=' Inventory (click to remap) ',
								bg=BG_DARK, fg=FG_DIM,
								font=('Consolas', 9), padx=4, pady=4)
		pack_lf.pack(fill=tk.X, pady=(0, 4))
		self._pack_canvas = tk.Canvas(pack_lf, bg=BG_MID, highlightthickness=0,
									  width=120, height=120, cursor='hand2')
		self._pack_canvas.pack()
		self._pack_canvas.bind('<Button-1>',
							   lambda e: self._on_icon_click(ATLAS_PACK))

		# Animated preview (equipped, direction cycling)
		preview_frame = tk.LabelFrame(bottom, text=' Equipped Animation ',
									  bg=BG_DARK, fg=FG_DIM,
									  font=('Consolas', 10), padx=5, pady=5)
		preview_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

		self.preview_canvas = tk.Canvas(preview_frame, bg=BG_DARK,
										highlightthickness=0)
		self.preview_canvas.pack(fill=tk.BOTH, expand=True)

	def _populate_list(self):
		"""Fill the listbox based on current filter, preserving selection."""
		# Remember current selection
		selected_item_id = None
		if self._current_item is not None:
			selected_item_id = self._current_item['item_id']

		self.listbox.delete(0, tk.END)
		self._visible_indices = []
		text_filter = self._filter_var.get().lower().strip()
		mode = self._filter_mode.get()

		mapped_count = 0
		shown_count = 0
		reselect_idx = None

		for i, item in enumerate(self.db_items):
			has_mapping = item['display_id'] is not None
			if has_mapping:
				mapped_count += 1

			# Mode filter
			if mode == 'mapped' and not has_mapping:
				continue
			if mode == 'unmapped' and has_mapping:
				continue
			if mode == 'equip' and item['equip_pos'] == 0:
				continue

			# Text filter
			if text_filter:
				search = f"{item['item_id']} {item['name']}".lower()
				if text_filter not in search:
					continue

			# Format line
			did = item['display_id']
			marker = f'd={did:3d}' if did is not None else '  ---'
			equip = EQUIP_POS_NAMES.get(item['equip_pos'], '?')
			line = f"{item['item_id']:4d} | {marker} | {equip:9s} | {item['name']}"
			self.listbox.insert(tk.END, line)
			self._visible_indices.append(i)
			shown_count += 1

			# Track re-selection
			if selected_item_id is not None and item['item_id'] == selected_item_id:
				reselect_idx = self.listbox.size() - 1

			# Color coding
			idx = self.listbox.size() - 1
			if not has_mapping:
				self.listbox.itemconfig(idx, fg=RED)
			elif item['equip_pos'] == 0:
				self.listbox.itemconfig(idx, fg=FG_DIM)
			else:
				self.listbox.itemconfig(idx, fg=GREEN)

		self._stats_label.config(
			text=f'{shown_count}/{len(self.db_items)} shown | '
				 f'{mapped_count} mapped | {len(self.db_items) - mapped_count} unmapped'
		)

		# Restore selection
		if reselect_idx is not None:
			self.listbox.selection_set(reselect_idx)
			self.listbox.see(reselect_idx)

	def _on_filter_change(self):
		self._populate_list()

	def _on_select(self, event):
		sel = self.listbox.curselection()
		if not sel:
			return
		try:
			idx = self._visible_indices[sel[0]]
			item = self.db_items[idx]
			self._show_item(item)
		except Exception as e:
			import traceback
			traceback.print_exc()
			messagebox.showerror('Error', f'Failed to show item:\n\n{e}')

	def _show_item(self, item):
		"""Show item details, icons, and preview."""
		self._current_item = item

		# Info
		self._info_name.config(text=item['name'])

		equip = EQUIP_POS_NAMES.get(item['equip_pos'], f"Unknown ({item['equip_pos']})")
		gender = {0: 'Both', 1: 'Male', 2: 'Female'}.get(item['gender_requirement'], '?')
		details = (
			f"DB ID: {item['item_id']}  |  Equip: {equip}  |  Gender: {gender}\n"
			f"Display ID: {item['display_id']}"
		)
		self._info_details.config(text=details)

		# Display ID combo — reset filter and pre-fill
		self._did_combo['values'] = self.meta_choices
		did = item['display_id']

		if did is not None and did in self.meta_by_id:
			meta = self.meta_by_id[did]
			pak = meta.get('pak_file', 'None')
			etype = meta.get('equip_type', '?')
			self._info_mapping.config(
				text=f"Display ID: {did}  |  PAK: {pak}  |  Type: {etype}",
				fg=GREEN
			)
			# Pre-fill combo with current mapping
			try:
				cidx = self.meta_choice_ids.index(did)
				self._did_var.set(self.meta_choices[cidx])
			except ValueError:
				self._did_var.set(str(did))
			self._show_icons(meta, item['equip_pos'])
			has_pak = bool(meta.get('pak_file'))
			if has_pak and ('male' in meta or 'female' in meta):
				self._start_preview(meta)
			else:
				self._stop_preview()
				self.preview_canvas.delete('all')
				cw = max(self.preview_canvas.winfo_width(), 50)
				ch = max(self.preview_canvas.winfo_height(), 50)
				self.preview_canvas.create_text(
					cw // 2, ch // 2, text='No equipped animation',
					fill=FG_DIM, font=('Consolas', 12)
				)
		else:
			if did is not None:
				self._info_mapping.config(
					text=f"Display ID: {did}  (metadata entry not found!)", fg=RED
				)
			else:
				self._info_mapping.config(text='No display_id mapping', fg=RED)
			self._did_var.set('')
			self._clear_icons()
			self._stop_preview()
			self.preview_canvas.delete('all')
			cw = max(self.preview_canvas.winfo_width(), 50)
			ch = max(self.preview_canvas.winfo_height(), 50)
			self.preview_canvas.create_text(
				cw // 2, ch // 2, text='No visual mapping',
				fill=FG_DIM, font=('Consolas', 12)
			)

	# --- Display ID assignment ---

	def _on_did_filter(self, event):
		"""Filter combobox dropdown as user types."""
		text = self._did_var.get().lower().strip()
		if not text:
			self._did_combo['values'] = self.meta_choices
			return
		filtered = [c for c in self.meta_choices if text in c.lower()]
		self._did_combo['values'] = filtered

	def _on_set_display_id(self):
		"""Set the display_id for the current item from the combobox selection."""
		if self._current_item is None:
			return
		sel = self._did_var.get().strip()
		if not sel:
			messagebox.showwarning('No Selection', 'Select a metadata entry first.')
			return
		# Parse ID from "123 | type | name" format, or bare integer
		try:
			did = int(sel.split('|')[0].strip())
		except (ValueError, IndexError):
			try:
				did = int(sel)
			except ValueError:
				messagebox.showerror('Invalid',
									f'Cannot parse display ID from: {sel}')
				return
		if did not in self.meta_by_id:
			messagebox.showerror('Invalid',
								f'Display ID {did} not found in metadata.')
			return

		item_id = self._current_item['item_id']
		self._write_display_id(item_id, did)
		self._current_item['display_id'] = did
		self._populate_list()
		self._show_item(self._current_item)

	def _on_clear_display_id(self):
		"""Clear the display_id for the current item (set to NULL)."""
		if self._current_item is None:
			return
		if self._current_item['display_id'] is None:
			return
		item_id = self._current_item['item_id']
		self._write_display_id(item_id, None)
		self._current_item['display_id'] = None
		self._populate_list()
		self._show_item(self._current_item)

	def _write_display_id(self, item_id, display_id):
		"""Write display_id back to the database."""
		db = sqlite3.connect(str(self.db_path))
		db.execute('UPDATE items SET display_id = ? WHERE item_id = ?',
				   (display_id, item_id))
		db.commit()
		db.close()

	# --- Icon click → launch Frame Mapper ---

	def _on_icon_click(self, atlas_idx):
		"""Launch the Frame Mapper GUI, navigating to the current item's display_id."""
		if self._current_item is None:
			return
		did = self._current_item['display_id']
		if did is None:
			messagebox.showinfo('No Mapping',
								'Set a display_id first, then click to open the Frame Mapper.')
			return
		self._launch_frame_mapper(did, atlas_idx)

	def _launch_frame_mapper(self, display_id, atlas_idx=0):
		"""Launch item_frame_mapper_gui.py, auto-selecting the given display_id and atlas."""
		mapper_script = _SCRIPT_DIR / 'item_frame_mapper_gui.py'
		if not mapper_script.exists():
			messagebox.showerror('Error',
								f'Frame Mapper not found:\n{mapper_script}')
			return
		cmd = [
			sys.executable, str(mapper_script),
			'--select-id', str(display_id),
			'--atlas-tab', str(atlas_idx),
		]
		subprocess.Popen(cmd)

	# --- Metadata file polling ---

	def _poll_metadata(self):
		"""Check if metadata file changed on disk and reload if so."""
		mtime = self._get_meta_mtime()
		if mtime != self._meta_mtime:
			self._meta_mtime = mtime
			self._reload_metadata()
		self.root.after(2000, self._poll_metadata)

	def _reload_metadata(self):
		"""Reload metadata from disk and refresh the current view."""
		try:
			self._load_metadata(self.metadata_path)
			self._build_meta_choices()
			self._did_combo['values'] = self.meta_choices
		except Exception as e:
			print(f'Metadata reload error: {e}')
			return
		# Refresh current item display if one is selected
		if self._current_item is not None:
			self._show_item(self._current_item)

	# --- Icon rendering ---

	def _clear_icons(self):
		"""Clear all icon canvases."""
		for canvas in (self._equip_canvas, self._ground_canvas, self._pack_canvas):
			canvas.delete('all')
			cw = canvas.winfo_width() or 120
			ch = canvas.winfo_height() or 120
			canvas.create_text(cw // 2, ch // 2, text='---',
							   fill=FG_DIM, font=('Consolas', 9))
		self._equip_tk = None
		self._ground_tk = None
		self._pack_tk = None

	def _render_icon(self, canvas, tk_attr, atlas_idx, rect_idx, label):
		"""Render a single atlas icon onto a canvas."""
		canvas.delete('all')
		cw = canvas.winfo_width() or 120
		ch = canvas.winfo_height() or 120

		if self.atlas is None:
			canvas.create_text(cw // 2, ch // 2, text='No atlas',
							   fill=RED, font=('Consolas', 9))
			return

		if rect_idx < 0:
			canvas.create_text(cw // 2, ch // 2, text=f'Not mapped\n(idx={rect_idx})',
							   fill=FG_DIM, font=('Consolas', 9), justify=tk.CENTER)
			return

		frame = extract_atlas_frame(self.atlas, atlas_idx, rect_idx)
		if frame is None:
			canvas.create_text(cw // 2, ch // 2,
							   text=f'Invalid\n(atlas={atlas_idx}, rect={rect_idx})',
							   fill=RED, font=('Consolas', 9), justify=tk.CENTER)
			return

		# Scale to fit canvas with some padding
		pad = 10
		label_h = 14
		avail_w = cw - pad * 2
		avail_h = ch - pad * 2 - label_h
		scale = min(avail_w / max(frame.width, 1), avail_h / max(frame.height, 1), 6.0)
		sw = max(1, int(frame.width * scale))
		sh = max(1, int(frame.height * scale))
		scaled = frame.resize((sw, sh), Image.NEAREST)

		# Composite with label
		comp = Image.new('RGBA', (cw, ch), (45, 45, 45, 255))
		x = (cw - sw) // 2
		y = label_h + (avail_h - sh) // 2 + pad
		comp.paste(scaled, (x, y), scaled)

		draw = ImageDraw.Draw(comp)
		draw.text((cw // 2, 2), f'{label} [{rect_idx}]',
				  fill=(160, 160, 160, 255), anchor='mt')

		tk_img = ImageTk.PhotoImage(comp)
		setattr(self, tk_attr, tk_img)  # prevent GC
		canvas.create_image(0, 0, anchor=tk.NW, image=tk_img)

	def _show_icons(self, meta, equip_pos=0):
		"""Show equip, ground, and inventory icons from atlas."""
		has_gender = 'male' in meta or 'female' in meta

		# Try gender data first (equippable items with equipped sprites)
		equip_idx = -1
		ground_idx = -1
		pack_idx = -1
		if has_gender:
			for g in ('male', 'female'):
				if g not in meta:
					continue
				gdata = meta[g]
				if equip_idx < 0:
					equip_idx = gdata.get('equip_frame_index', -1)
				if ground_idx < 0:
					ground_idx = gdata.get('ground_frame_index', -1)
				if pack_idx < 0:
					pack_idx = gdata.get('pack_frame_index', -1)

		# Fall back to top-level indices (non-equippable, or accessories like rings/necklaces)
		if ground_idx < 0:
			ground_idx = meta.get('ground_frame_index', -1)
		if pack_idx < 0:
			pack_idx = meta.get('inventory_frame_index', -1)

		# Equip icon — only show for items that actually have equipped sprites
		if equip_idx >= 0:
			self._render_icon(self._equip_canvas, '_equip_tk',
							  ATLAS_EQUIP, equip_idx, 'Equip')
		else:
			self._equip_canvas.delete('all')
			cw = self._equip_canvas.winfo_width() or 120
			ch = self._equip_canvas.winfo_height() or 120
			self._equip_canvas.create_text(cw // 2, ch // 2, text='N/A',
										   fill=FG_DIM, font=('Consolas', 10))
			self._equip_tk = None

		self._render_icon(self._ground_canvas, '_ground_tk',
						  ATLAS_GROUND, ground_idx, 'Ground')
		self._render_icon(self._pack_canvas, '_pack_tk',
						  ATLAS_PACK, pack_idx, 'Inventory')

	# --- Animated preview ---

	def _start_preview(self, meta_entry):
		"""Start direction-cycling preview for a metadata entry."""
		self._stop_preview()
		self._preview_dir = 0
		self._preview_meta = meta_entry
		self._render_preview()
		self._schedule_next_dir()

	def _stop_preview(self):
		if self._preview_timer is not None:
			self.root.after_cancel(self._preview_timer)
			self._preview_timer = None

	def _schedule_next_dir(self):
		self._preview_timer = self.root.after(500, self._cycle_dir)

	def _cycle_dir(self):
		self._preview_dir = (self._preview_dir + 1) % 8
		self._render_preview()
		self._schedule_next_dir()

	def _get_pak(self, pak_file):
		"""Load and cache a PAK file."""
		if pak_file not in self.pak_cache:
			pak_path = self.items_dir / pak_file
			if not pak_path.exists():
				return None
			self.pak_cache[pak_file] = PAKFile.read(pak_path)
		return self.pak_cache[pak_file]

	def _get_dir_rect_idx(self, pak, sprite_idx, direction):
		"""Get rectangle index for first frame of a direction (0-7)."""
		if sprite_idx >= len(pak.sprites):
			return 0
		sprite = pak.sprites[sprite_idx]
		n = len(sprite.rectangles)
		if n == 0:
			return 0
		frames_per_dir = n // 8 if n >= 8 else 1
		return min(direction * frames_per_dir, n - 1)

	def _extract_frame(self, pak, sprite_idx, rect_idx=0):
		"""Extract a single frame (cropped rectangle) from a PAK sprite."""
		if sprite_idx >= len(pak.sprites):
			return None
		sprite = pak.sprites[sprite_idx]
		if not sprite.rectangles:
			return None
		if rect_idx >= len(sprite.rectangles):
			rect_idx = 0
		img = sprite.get_image().convert('RGBA')
		r = sprite.rectangles[rect_idx]
		return img.crop((r.x, r.y, r.x + r.width, r.y + r.height))

	def _render_preview(self):
		"""Render the current direction preview frame."""
		meta = getattr(self, '_preview_meta', None)
		if meta is None:
			return

		self.preview_canvas.delete('all')
		cw = max(self.preview_canvas.winfo_width(), 50)
		ch = max(self.preview_canvas.winfo_height(), 50)

		pak_file = meta.get('pak_file')
		if not pak_file:
			self.preview_canvas.create_text(cw // 2, ch // 2, text='No PAK (non-equippable)',
											fill=FG_DIM, font=('Consolas', 10))
			return

		pak = self._get_pak(pak_file)
		if pak is None:
			self.preview_canvas.create_text(cw // 2, ch // 2, text=f'PAK missing: {pak_file}',
											fill=RED, font=('Consolas', 10))
			return

		d = self._preview_dir
		has_male = 'male' in meta
		has_female = 'female' in meta
		male_frame = None
		female_frame = None

		if has_male:
			idx = meta['male'].get('pak_index_start', 0)
			rect = self._get_dir_rect_idx(pak, idx, d)
			male_frame = self._extract_frame(pak, idx, rect_idx=rect)
		if has_female:
			idx = meta['female'].get('pak_index_start', 12)
			rect = self._get_dir_rect_idx(pak, idx, d)
			female_frame = self._extract_frame(pak, idx, rect_idx=rect)

		frames = []
		labels = []
		if male_frame:
			frames.append(male_frame)
			labels.append('M')
		if female_frame:
			frames.append(female_frame)
			labels.append('F')

		if not frames:
			self.preview_canvas.create_text(cw // 2, ch // 2, text='No sprites',
											fill=FG_DIM, font=('Consolas', 10))
			return

		# Scale both frames uniformly to fit side-by-side
		dir_label_h = 20
		spacing = 16
		label_h = 18
		avail_w = (cw - spacing * (len(frames) + 1)) // len(frames)
		avail_h = ch - spacing * 2 - label_h - dir_label_h
		max_w = max(f.width for f in frames)
		max_h = max(f.height for f in frames)
		scale = min(avail_w / max(max_w, 1), avail_h / max(max_h, 1), 6.0)

		# Composite onto a single image
		scaled = []
		for f in frames:
			sw = max(1, int(f.width * scale))
			sh = max(1, int(f.height * scale))
			scaled.append(f.resize((sw, sh), Image.NEAREST))

		total_w = sum(s.width for s in scaled) + spacing * (len(scaled) - 1)
		row_h = max(s.height for s in scaled)
		comp_w = max(total_w, 1)
		comp_h = max(row_h + label_h + dir_label_h, 1)
		comp = Image.new('RGBA', (comp_w, comp_h), (30, 30, 30, 255))
		draw = ImageDraw.Draw(comp)

		# Direction label at top center
		dir_name = DIR_NAMES[d]
		draw.text((comp_w // 2, 2), dir_name, fill=(200, 200, 200, 255), anchor='mt')

		x = 0
		for i, s in enumerate(scaled):
			y_off = (row_h - s.height) // 2
			comp.paste(s, (x, y_off + label_h + dir_label_h), s)
			lbl_x = x + s.width // 2
			color = (74, 158, 255, 255) if labels[i] == 'M' else (255, 140, 0, 255)
			draw.text((lbl_x, dir_label_h), labels[i], fill=color, anchor='mt')
			x += s.width + spacing

		self._preview_tk = ImageTk.PhotoImage(comp)
		self.preview_canvas.create_image(cw // 2, ch // 2, image=self._preview_tk)


def main():
	parser = argparse.ArgumentParser(description='Item Database Viewer')
	parser.add_argument('--db', default=str(DEFAULT_DB), help='Path to gameconfigs.db')
	parser.add_argument('--metadata', default=str(DEFAULT_METADATA),
						help='Path to ItemSpriteMetadata.json')
	parser.add_argument('--items-dir', default=str(DEFAULT_ITEMS_DIR),
						help='Path to per-item PAK directory')
	parser.add_argument('--atlas', default=str(DEFAULT_ATLAS),
						help='Path to item_atlas.pak')
	args = parser.parse_args()

	root = tk.Tk()
	app = ItemDBViewer(root, args.db, args.metadata, args.items_dir, args.atlas)
	root.mainloop()


if __name__ == '__main__':
	main()
