#!/usr/bin/env python3
"""
Item Frame Mapper GUI — assigns atlas frame indices to ItemSpriteMetadata.json entries.

Usage:
    python item_frame_mapper_gui.py
    python item_frame_mapper_gui.py --metadata <path> --atlas <path> --items-dir <path>

Workflow:
    1. Select an item from the left panel
    2. Click an 'Assign' button next to the field you want to set
    3. Click a frame rectangle on the atlas view
    4. The frame index is assigned. Ctrl+S to save.
"""

import tkinter as tk
from tkinter import ttk, messagebox, filedialog
import json
import sys
import argparse
from pathlib import Path
from PIL import Image, ImageTk, ImageDraw, ImageFont
import io

# paklib.py lives alongside this script
_SCRIPT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(_SCRIPT_DIR))
from paklib import PAKFile
DEFAULT_ATLAS = _SCRIPT_DIR / 'sprites' / 'item_atlas.pak'
DEFAULT_ITEMS_DIR = _SCRIPT_DIR / 'sprites' / 'items'
DEFAULT_METADATA = _SCRIPT_DIR / 'contents' / 'item_sprite_metadata.json'
DEFAULT_DISPLAY_NAMES = _SCRIPT_DIR / 'item_display_names.json'

ATLAS_NAMES = ['Equip Atlas (0)', 'Ground Atlas (1)', 'Pack Atlas (2)']

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
HIGHLIGHT = '#ffff00'


def center_on_parent(dialog, parent, width, height):
    """Position a Toplevel dialog centered over its parent window."""
    parent.update_idletasks()
    px = parent.winfo_rootx()
    py = parent.winfo_rooty()
    pw = parent.winfo_width()
    ph = parent.winfo_height()
    x = px + (pw - width) // 2
    y = py + (ph - height) // 2
    dialog.geometry(f'{width}x{height}+{x}+{y}')


def make_checker_bg(width, height, tile_size=16):
    """Fast checkerboard background via tiling."""
    tile = Image.new('RGBA', (tile_size * 2, tile_size * 2), (45, 45, 45, 255))
    draw = ImageDraw.Draw(tile)
    draw.rectangle([tile_size, 0, tile_size * 2, tile_size], fill=(60, 60, 60, 255))
    draw.rectangle([0, tile_size, tile_size, tile_size * 2], fill=(60, 60, 60, 255))
    bg = Image.new('RGBA', (width, height))
    for y in range(0, height, tile_size * 2):
        for x in range(0, width, tile_size * 2):
            bg.paste(tile, (x, y))
    return bg


class AtlasView(tk.Frame):
    """Zoomable, pannable atlas sprite viewer with clickable frame rectangles."""

    def __init__(self, parent, atlas_data, on_rect_click, on_rect_hover):
        """
        atlas_data: list of (PIL.Image, [SpriteRectangle])
        """
        super().__init__(parent, bg=BG_DARK)
        self.atlas_data = atlas_data
        self.on_rect_click = on_rect_click
        self.on_rect_hover = on_rect_hover

        self.current_atlas = 0
        self.zoom = 1.0
        self.pan_x = 0.0
        self.pan_y = 0.0
        self._drag_start = None
        self._dragged = False
        self.hover_rect = -1

        # Overlay state (set by parent)
        self.assigned_rects = {}     # {atlas_idx: {rect_idx: item_id}}
        self.current_item_rects = {} # {atlas_idx: set(rect_idx)}

        self._img_cache = None
        self._img_cache_key = None

        self._build()

    # --- UI setup ---

    def _build(self):
        top = tk.Frame(self, bg=BG_MID)
        top.pack(fill=tk.X)

        self.tab_btns = []
        for i, name in enumerate(ATLAS_NAMES):
            b = tk.Button(top, text=name, font=('Segoe UI', 9),
                          command=lambda idx=i: self.switch_atlas(idx),
                          relief=tk.FLAT, padx=10, pady=2)
            b.pack(side=tk.LEFT, padx=1)
            self.tab_btns.append(b)

        # Zoom controls
        zf = tk.Frame(top, bg=BG_MID)
        zf.pack(side=tk.RIGHT, padx=6)
        tk.Button(zf, text='\u2212', width=2, command=self.zoom_out, font=('Segoe UI', 9)).pack(side=tk.LEFT)
        self.zoom_lbl = tk.Label(zf, text='100%', width=6, font=('Consolas', 9), bg=BG_MID, fg=FG_TEXT)
        self.zoom_lbl.pack(side=tk.LEFT)
        tk.Button(zf, text='+', width=2, command=self.zoom_in, font=('Segoe UI', 9)).pack(side=tk.LEFT)
        tk.Button(zf, text='Fit', command=self.zoom_fit, font=('Segoe UI', 9), padx=6).pack(side=tk.LEFT, padx=4)

        self.canvas = tk.Canvas(self, bg=BG_DARK, highlightthickness=0, cursor='crosshair')
        self.canvas.pack(fill=tk.BOTH, expand=True)

        self.canvas.bind('<ButtonPress-1>', self._press)
        self.canvas.bind('<B1-Motion>', self._drag)
        self.canvas.bind('<ButtonRelease-1>', self._release)
        self.canvas.bind('<Motion>', self._motion)
        self.canvas.bind('<MouseWheel>', self._wheel)
        self.canvas.bind('<Button-4>', lambda e: self.zoom_in())   # Linux scroll up
        self.canvas.bind('<Button-5>', lambda e: self.zoom_out())  # Linux scroll down
        self.canvas.bind('<Configure>', lambda e: self.redraw())

        self._style_tabs()

    def _style_tabs(self):
        for i, b in enumerate(self.tab_btns):
            if i == self.current_atlas:
                b.config(bg=BLUE, fg='white', relief=tk.SUNKEN)
            else:
                b.config(bg=BG_LIGHT, fg=FG_TEXT, relief=tk.FLAT)

    # --- Atlas switching & zoom ---

    def switch_atlas(self, idx):
        self.current_atlas = idx
        self._img_cache = None
        self._style_tabs()
        self.zoom_fit()

    def zoom_in(self):
        self.zoom = min(self.zoom * 1.3, 10.0)
        self._img_cache = None
        self.redraw()

    def zoom_out(self):
        self.zoom = max(self.zoom / 1.3, 0.05)
        self._img_cache = None
        self.redraw()

    def zoom_fit(self):
        if not self.atlas_data:
            return
        img = self.atlas_data[self.current_atlas][0]
        cw = max(self.canvas.winfo_width(), 100)
        ch = max(self.canvas.winfo_height(), 100)
        self.zoom = min(cw / img.width, ch / img.height) * 0.95
        self.pan_x = (cw - img.width * self.zoom) / 2
        self.pan_y = (ch - img.height * self.zoom) / 2
        self._img_cache = None
        self.redraw()

    # --- Mouse handling ---

    def _press(self, e):
        self._drag_start = (e.x, e.y)
        self._dragged = False

    def _drag(self, e):
        if self._drag_start:
            dx = e.x - self._drag_start[0]
            dy = e.y - self._drag_start[1]
            if abs(dx) > 3 or abs(dy) > 3:
                self._dragged = True
            self.pan_x += dx
            self.pan_y += dy
            self._drag_start = (e.x, e.y)
            self.redraw()

    def _release(self, e):
        if not self._dragged:
            idx = self._hit_test(e.x, e.y)
            if idx >= 0:
                self.on_rect_click(self.current_atlas, idx)
        self._drag_start = None
        self._dragged = False

    def _motion(self, e):
        idx = self._hit_test(e.x, e.y)
        if idx != self.hover_rect:
            self.hover_rect = idx
            self.on_rect_hover(self.current_atlas, idx)
            self.redraw()

    def _wheel(self, e):
        # Zoom toward cursor
        old_zoom = self.zoom
        if e.delta > 0:
            self.zoom = min(self.zoom * 1.2, 10.0)
        else:
            self.zoom = max(self.zoom / 1.2, 0.05)
        # Adjust pan so cursor stays on same image point
        factor = self.zoom / old_zoom
        self.pan_x = e.x - factor * (e.x - self.pan_x)
        self.pan_y = e.y - factor * (e.y - self.pan_y)
        self._img_cache = None
        self.redraw()

    def _hit_test(self, cx, cy):
        """Find rect index at canvas coords. Returns smallest containing rect."""
        if not self.atlas_data:
            return -1
        _, rects = self.atlas_data[self.current_atlas]
        ix = (cx - self.pan_x) / self.zoom
        iy = (cy - self.pan_y) / self.zoom
        best = -1
        best_area = float('inf')
        for i, r in enumerate(rects):
            if r.x <= ix <= r.x + r.width and r.y <= iy <= r.y + r.height:
                area = r.width * r.height
                if area < best_area:
                    best = i
                    best_area = area
        return best

    # --- Drawing ---

    def redraw(self):
        self.canvas.delete('all')
        if not self.atlas_data:
            return

        img, rects = self.atlas_data[self.current_atlas]
        z = self.zoom
        self.zoom_lbl.config(text=f'{int(z * 100)}%')

        # Render scaled image (cached)
        nw = max(1, int(img.width * z))
        nh = max(1, int(img.height * z))
        cache_key = (self.current_atlas, nw, nh)
        if self._img_cache_key != cache_key:
            method = Image.NEAREST if z >= 2 else Image.BILINEAR
            scaled = img.resize((nw, nh), method)
            self._tk_img = ImageTk.PhotoImage(scaled)
            self._img_cache_key = cache_key

        self.canvas.create_image(self.pan_x, self.pan_y, anchor=tk.NW, image=self._tk_img)

        # Draw rect overlays
        a = self.current_atlas
        assigned = self.assigned_rects.get(a, {})
        current = self.current_item_rects.get(a, set())
        show_labels = z >= 0.35

        for i, r in enumerate(rects):
            x1 = self.pan_x + r.x * z
            y1 = self.pan_y + r.y * z
            x2 = x1 + r.width * z
            y2 = y1 + r.height * z

            # Skip offscreen rects
            cw = self.canvas.winfo_width()
            ch = self.canvas.winfo_height()
            if x2 < 0 or y2 < 0 or x1 > cw or y1 > ch:
                continue

            if i == self.hover_rect:
                color, width = HIGHLIGHT, 2
            elif i in current:
                color, width = GREEN, 2
            elif i in assigned:
                color, width = BLUE, 1
            else:
                color, width = '#555555', 1

            self.canvas.create_rectangle(x1, y1, x2, y2, outline=color, width=width)

            if show_labels:
                font_size = max(7, min(11, int(9 * z)))
                self.canvas.create_text(
                    x1 + 2 * z, y1 + 1 * z, text=str(i),
                    anchor=tk.NW, fill=color, font=('Consolas', font_size))


class ItemFrameMapperApp:
    """Main application window."""

    def __init__(self, root, metadata_path, atlas_path, items_dir, display_names_path=None, select_id=None, atlas_tab=None):
        self.root = root
        self.root.title('Item Frame Mapper')
        self.root.geometry('1500x950')
        self.root.configure(bg=BG_DARK)

        self.metadata_path = Path(metadata_path)
        self.atlas_path = Path(atlas_path)
        self.items_dir = Path(items_dir)
        self.display_names_path = Path(display_names_path or DEFAULT_DISPLAY_NAMES)
        self._select_id = select_id
        self._atlas_tab = atlas_tab

        self.items = []
        self.display_names = {}       # {str(id): display_name} — tool-only, separate file
        self.current_idx = -1
        self.assigning_field = None   # e.g. 'male.equip_frame_index' or 'inventory_frame_index'
        self.auto_assign = False      # auto-advance through fields on click
        self.atlas_data = []          # [(Image, [rects])]
        self.pak_cache = {}
        self._pak_overrides = {}  # {item_id: new_pak_value_or_None} — applied at save time
        self.unsaved = False
        self.listbox_map = []         # listbox row index -> items[] index
        self._preview_dir = 0         # current direction index (0-7)
        self._preview_timer = None    # after() id for cycling

        self._load()
        self._build_ui()
        self._populate_list()
        self.root.after(100, self._initial_layout)

    def _initial_layout(self):
        """Set the atlas/detail sash to 70/30 split and fit the atlas view."""
        self.right_pw.update_idletasks()
        h = self.right_pw.winfo_height()
        if h > 1:
            self.right_pw.sash_place(0, 0, int(h * 0.65))
        self.atlas_view.zoom_fit()
        # Auto-select and switch tab if requested via command line
        if self._select_id is not None:
            self._select_item_by_id(self._select_id)
        if self._atlas_tab is not None:
            self.atlas_view.switch_atlas(self._atlas_tab)

    def _select_item_by_id(self, item_id):
        """Find and select the item with the given metadata ID."""
        for lb_idx, items_idx in enumerate(self.listbox_map):
            if self.items[items_idx].get('id') == item_id:
                self.listbox.selection_clear(0, tk.END)
                self.listbox.selection_set(lb_idx)
                self.listbox.see(lb_idx)
                self.current_idx = items_idx
                self._show_details()
                return

    # --- Data loading ---

    def _load(self):
        with open(self.metadata_path, 'r', encoding='utf-8') as f:
            self.items = json.load(f)

        # Load tool-only display names (separate from game metadata)
        if self.display_names_path.exists():
            with open(self.display_names_path, 'r', encoding='utf-8') as f:
                self.display_names = json.load(f)
        else:
            self.display_names = {}

        atlas_pak = PAKFile.read(self.atlas_path)
        for sprite in atlas_pak.sprites:
            img = sprite.get_image().convert('RGBA')
            bg = make_checker_bg(img.width, img.height)
            bg.paste(img, (0, 0), img)
            self.atlas_data.append((bg, sprite.rectangles))

        self._rebuild_maps()

    @staticmethod
    def _is_equippable(item):
        """Equippable if it has a pak file (equipped animation sprites)."""
        return bool(item.get('pak_file'))

    def _rebuild_maps(self):
        """Build atlas assignment lookup: {atlas_idx: {rect_idx: (item_id, field)}}."""
        self.assign_map = {0: {}, 1: {}, 2: {}}

        for i, item in enumerate(self.items):
            iid = item.get('id', i)
            if not self._is_equippable(item):
                v = item.get('inventory_frame_index', -1)
                if v >= 0:
                    self.assign_map[2][v] = iid
                v = item.get('ground_frame_index', -1)
                if v >= 0:
                    self.assign_map[1][v] = iid
            else:
                for g in ('male', 'female'):
                    if g not in item:
                        continue
                    v = item[g].get('equip_frame_index', -1)
                    if v >= 0:
                        self.assign_map[0][v] = iid
                    v = item[g].get('ground_frame_index', -1)
                    if v >= 0:
                        self.assign_map[1][v] = iid
                    v = item[g].get('pack_frame_index', -1)
                    if v >= 0:
                        self.assign_map[2][v] = iid

    # --- UI construction ---

    def _build_ui(self):
        # Keybinds
        self.root.bind('<Control-s>', lambda e: self._save())
        self.root.bind('<Escape>', lambda e: self._cancel_assign())
        self.root.protocol('WM_DELETE_WINDOW', self._on_close)

        # --- Menu ---
        menu = tk.Menu(self.root)
        fm = tk.Menu(menu, tearoff=0)
        fm.add_command(label='Save', command=self._save, accelerator='Ctrl+S')
        fm.add_command(label='Save As...', command=self._save_as)
        fm.add_separator()
        fm.add_command(label='Exit', command=self._on_close)
        menu.add_cascade(label='File', menu=fm)

        em = tk.Menu(menu, tearoff=0)
        em.add_command(label='New Equippable Entry...', command=self._new_equip_entry)
        em.add_command(label='New Non-Equippable Entry...', command=self._new_nonequip_entry)
        em.add_separator()
        em.add_command(label='Delete Current Entry', command=self._delete_entry)
        menu.add_cascade(label='Edit', menu=em)

        self.root.config(menu=menu)

        # --- Main layout: left list | right (atlas + details) ---
        pw = tk.PanedWindow(self.root, orient=tk.HORIZONTAL, bg=BG_DARK,
                            sashwidth=4, sashrelief=tk.FLAT)
        pw.pack(fill=tk.BOTH, expand=True)

        # Left panel
        left = tk.Frame(pw, bg=BG_DARK, width=300)
        pw.add(left, minsize=200)

        # Search
        sf = tk.Frame(left, bg=BG_DARK)
        sf.pack(fill=tk.X, padx=4, pady=(4, 0))
        tk.Label(sf, text='Search:', bg=BG_DARK, fg=FG_TEXT, font=('Segoe UI', 9)).pack(side=tk.LEFT)
        self.search_var = tk.StringVar()
        self.search_var.trace_add('write', lambda *_: self._populate_list())
        se = tk.Entry(sf, textvariable=self.search_var, font=('Consolas', 9))
        se.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=4)

        # Filter row
        ff = tk.Frame(left, bg=BG_DARK)
        ff.pack(fill=tk.X, padx=4, pady=2)
        tk.Label(ff, text='Type:', bg=BG_DARK, fg=FG_TEXT, font=('Segoe UI', 9)).pack(side=tk.LEFT)
        self.type_var = tk.StringVar(value='all')
        types = ['all', 'unassigned', 'partial'] + sorted({i.get('equip_type', 'none') for i in self.items})
        ttk.Combobox(ff, textvariable=self.type_var, values=types, state='readonly', width=14,
                     font=('Consolas', 9)).pack(side=tk.LEFT, padx=4)
        self.type_var.trace_add('write', lambda *_: self._populate_list())

        # Count + New button row
        cnt_row = tk.Frame(left, bg=BG_DARK)
        cnt_row.pack(fill=tk.X, padx=4)
        self.count_lbl = tk.Label(cnt_row, text='', bg=BG_DARK, fg=FG_DIM, font=('Consolas', 8),
                                  anchor=tk.W)
        self.count_lbl.pack(side=tk.LEFT, padx=4)
        tk.Button(cnt_row, text='+ New', font=('Consolas', 8), fg=GREEN,
                  command=self._new_entry_menu).pack(side=tk.RIGHT, padx=4)

        # Listbox
        lf = tk.Frame(left, bg=BG_DARK)
        lf.pack(fill=tk.BOTH, expand=True, padx=4, pady=(0, 4))
        sb = tk.Scrollbar(lf, orient=tk.VERTICAL)
        sb.pack(side=tk.RIGHT, fill=tk.Y)
        self.listbox = tk.Listbox(lf, font=('Consolas', 9), bg=BG_MID, fg=FG_TEXT,
                                  selectbackground=BLUE, selectforeground='white',
                                  yscrollcommand=sb.set, activestyle='none')
        self.listbox.pack(fill=tk.BOTH, expand=True)
        sb.config(command=self.listbox.yview)
        self.listbox.bind('<<ListboxSelect>>', self._on_select)
        self.listbox.bind('<Up>', self._on_arrow)
        self.listbox.bind('<Down>', self._on_arrow)

        # Right panel — vertical split: atlas top (70%), details bottom (30%)
        self.right_pw = tk.PanedWindow(pw, orient=tk.VERTICAL, bg=BG_DARK,
                                       sashwidth=4, sashrelief=tk.FLAT)
        pw.add(self.right_pw)

        # Atlas view
        self.atlas_view = AtlasView(self.right_pw, self.atlas_data,
                                    self._on_atlas_click, self._on_atlas_hover)
        self.right_pw.add(self.atlas_view, minsize=300, stretch='always')

        # Bottom detail panel
        bot = tk.Frame(self.right_pw, bg=BG_DARK, height=200)
        self.right_pw.add(bot, minsize=150, stretch='never')

        # Detail info (left of bottom)
        det = tk.Frame(bot, bg=BG_DARK)
        det.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=8, pady=4)

        # Info grid — mix of labels and editable fields
        info_grid = tk.Frame(det, bg=BG_DARK)
        info_grid.pack(fill=tk.X)
        info_grid.columnconfigure(1, weight=1)
        self.info = {}

        # Row 0: Display Name (editable, tool-only)
        tk.Label(info_grid, text='Name:', font=('Consolas', 9, 'bold'),
                 bg=BG_DARK, fg=FG_TEXT, anchor=tk.W).grid(row=0, column=0, sticky=tk.W)
        name_row = tk.Frame(info_grid, bg=BG_DARK)
        name_row.grid(row=0, column=1, sticky=tk.EW, padx=(8, 0))
        self._name_var = tk.StringVar()
        self._name_entry = tk.Entry(name_row, textvariable=self._name_var, font=('Consolas', 9),
                                    bg=BG_LIGHT, fg=FG_TEXT, insertbackground=FG_TEXT,
                                    relief=tk.FLAT, width=30)
        self._name_entry.pack(side=tk.LEFT, fill=tk.X, expand=True)
        tk.Button(name_row, text='Set', font=('Consolas', 8), padx=4,
                  command=self._on_set_display_name).pack(side=tk.LEFT, padx=2)

        # Row 1: ID (read-only label)
        tk.Label(info_grid, text='ID:', font=('Consolas', 9, 'bold'),
                 bg=BG_DARK, fg=FG_TEXT, anchor=tk.W).grid(row=1, column=0, sticky=tk.W)
        self.info['ID'] = tk.Label(info_grid, text='-', font=('Consolas', 9),
                                   bg=BG_DARK, fg=FG_DIM, anchor=tk.W)
        self.info['ID'].grid(row=1, column=1, sticky=tk.W, padx=(8, 0))

        # Row 2: Type (read-only label)
        tk.Label(info_grid, text='Type:', font=('Consolas', 9, 'bold'),
                 bg=BG_DARK, fg=FG_TEXT, anchor=tk.W).grid(row=2, column=0, sticky=tk.W)
        self.info['Type'] = tk.Label(info_grid, text='-', font=('Consolas', 9),
                                     bg=BG_DARK, fg=FG_DIM, anchor=tk.W)
        self.info['Type'].grid(row=2, column=1, sticky=tk.W, padx=(8, 0))

        # Row 3: PAK file (editable combobox)
        tk.Label(info_grid, text='PAK:', font=('Consolas', 9, 'bold'),
                 bg=BG_DARK, fg=FG_TEXT, anchor=tk.W).grid(row=3, column=0, sticky=tk.W)
        pak_row = tk.Frame(info_grid, bg=BG_DARK)
        pak_row.grid(row=3, column=1, sticky=tk.EW, padx=(8, 0))
        self._pak_var = tk.StringVar()
        self._pak_combo = ttk.Combobox(pak_row, textvariable=self._pak_var,
                                       values=self._available_paks(),
                                       font=('Consolas', 9), width=28)
        self._pak_combo.pack(side=tk.LEFT, fill=tk.X, expand=True)
        tk.Button(pak_row, text='Set', font=('Consolas', 8), padx=4,
                  command=self._on_set_pak_file).pack(side=tk.LEFT, padx=2)
        tk.Button(pak_row, text='Clear', font=('Consolas', 8), fg=RED, padx=4,
                  command=self._on_clear_pak_file).pack(side=tk.LEFT, padx=2)

        ttk.Separator(det, orient=tk.HORIZONTAL).pack(fill=tk.X, pady=4)

        # Assignment frame (rebuilt per item)
        self.assign_area = tk.Frame(det, bg=BG_DARK)
        self.assign_area.pack(fill=tk.X)

        # Instruction label
        self.instr_lbl = tk.Label(det, text='', font=('Consolas', 8, 'italic'),
                                  bg=BG_DARK, fg=ORANGE, anchor=tk.W, wraplength=500)
        self.instr_lbl.pack(fill=tk.X, pady=(4, 0))

        # Preview panel (right of bottom) — 3 separate boxes
        pf = tk.Frame(bot, bg=BG_DARK)
        pf.pack(side=tk.RIGHT, fill=tk.Y, padx=4, pady=4)

        ef = tk.LabelFrame(pf, text='Equip', font=('Segoe UI', 8),
                           bg=BG_DARK, fg=FG_TEXT)
        ef.pack(side=tk.LEFT, padx=2, fill=tk.Y)
        self.equip_canvas = tk.Canvas(ef, bg=BG_MID, highlightthickness=0,
                                      width=140, height=150)
        self.equip_canvas.pack(padx=2, pady=2)

        gf2 = tk.LabelFrame(pf, text='Ground', font=('Segoe UI', 8),
                            bg=BG_DARK, fg=FG_TEXT)
        gf2.pack(side=tk.LEFT, padx=2, fill=tk.Y)
        self.ground_canvas = tk.Canvas(gf2, bg=BG_MID, highlightthickness=0,
                                       width=100, height=150)
        self.ground_canvas.pack(padx=2, pady=2)

        pkf = tk.LabelFrame(pf, text='Pack', font=('Segoe UI', 8),
                            bg=BG_DARK, fg=FG_TEXT)
        pkf.pack(side=tk.LEFT, padx=2, fill=tk.Y)
        self.pack_canvas = tk.Canvas(pkf, bg=BG_MID, highlightthickness=0,
                                     width=100, height=150)
        self.pack_canvas.pack(padx=2, pady=2)

        # Status bar
        self.status_var = tk.StringVar(value='Loading...')
        tk.Label(self.root, textvariable=self.status_var, font=('Consolas', 9),
                 bg=BG_LIGHT, fg=FG_TEXT, anchor=tk.W, padx=6, pady=2).pack(fill=tk.X, side=tk.BOTTOM)

        self._refresh_status()

    # --- Item list ---

    def _item_display(self, item, idx):
        iid = str(item.get('id', idx))
        name = (self.display_names.get(iid)
                or item.get('name')
                or ((item.get('pak_file') or '').replace('.pak', '').replace('_', ' ').title())
                or f'Item #{iid}')
        return name

    def _item_status(self, item):
        """Returns (assigned_count, total_count)."""
        total = assigned = 0
        if not self._is_equippable(item):
            for k in ('inventory_frame_index', 'ground_frame_index'):
                if k in item:
                    total += 1
                    if item[k] != -1:
                        assigned += 1
        else:
            for g in ('male', 'female'):
                if g in item:
                    for k in ('equip_frame_index', 'pack_frame_index', 'ground_frame_index'):
                        if k in item[g]:
                            total += 1
                            if item[g][k] != -1:
                                assigned += 1
        return assigned, total

    def _has_duplicate_genders(self, item):
        """Returns True if equipable item has both genders with identical frame assignments."""
        if not self._is_equippable(item):
            return False
        if 'male' not in item or 'female' not in item:
            return False
        for k in ('equip_frame_index', 'ground_frame_index', 'pack_frame_index'):
            m = item['male'].get(k, -1)
            f = item['female'].get(k, -1)
            if m != f:
                return False
        return True

    def _populate_list(self):
        search = self.search_var.get().lower()
        type_f = self.type_var.get()

        self.listbox.delete(0, tk.END)
        self.listbox_map = []

        for i, item in enumerate(self.items):
            name = self._item_display(item, i)
            et = item.get('equip_type', 'none')
            asgn, tot = self._item_status(item)

            # Filters
            if type_f == 'unassigned':
                if asgn > 0:
                    continue
            elif type_f == 'partial':
                if asgn == 0 or asgn == tot:
                    continue
            elif type_f not in ('all',) and et != type_f:
                continue

            if search and search not in name.lower() and search not in str(item.get('id', '')):
                continue

            if tot == 0:
                marker = ' '
            elif asgn == tot:
                marker = '*'
            elif asgn > 0:
                marker = '~'
            else:
                marker = ' '

            display = f"[{marker}] {item.get('id', i):>3d}  {name}"
            self.listbox.insert(tk.END, display)
            self.listbox_map.append(i)

            if marker == '*' and self._has_duplicate_genders(item):
                self.listbox.itemconfig(tk.END, fg=YELLOW)
            elif marker == '*':
                self.listbox.itemconfig(tk.END, fg=GREEN)
            elif marker == '~':
                self.listbox.itemconfig(tk.END, fg=YELLOW)

        self.count_lbl.config(text=f'{len(self.listbox_map)} items shown')

    def _on_select(self, event):
        sel = self.listbox.curselection()
        if not sel:
            return
        self.current_idx = self.listbox_map[sel[0]]
        self.assigning_field = None
        self.auto_assign = False
        self._show_details()
        # Auto-begin first unassigned field, but don't advance to next item
        self.root.after(50, self._begin_first_unassigned)

    def _on_arrow(self, event):
        """Handle arrow keys in listbox to auto-update details."""
        self.root.after(50, lambda: self._on_select(event))

    # --- Detail panel ---

    def _show_details(self):
        if self.current_idx < 0:
            return
        item = self.items[self.current_idx]
        name = self._item_display(item, self.current_idx)

        self._name_var.set(name)
        self.info['ID'].config(text=str(item.get('id', '?')), fg=FG_TEXT)
        self.info['Type'].config(text=item.get('equip_type', 'none'), fg=FG_TEXT)
        effective_pak = self._get_effective_pak(item)
        self._pak_var.set(effective_pak or '')

        self.instr_lbl.config(text='')

        # Rebuild assignment rows
        for w in self.assign_area.winfo_children():
            w.destroy()
        self._assign_widgets = {}

        row = 0
        if not bool(effective_pak):
            for field, atlas_idx, label in [
                ('ground_frame_index', 1, 'Ground'),
                ('inventory_frame_index', 2, 'Inventory'),
            ]:
                val = item.get(field, -1)
                self._add_assign_row(row, field, atlas_idx, label, val)
                row += 1
        else:
            for gender in ('male', 'female'):
                if gender not in item:
                    continue
                tk.Label(self.assign_area, text=f'{gender.upper()}:',
                         font=('Consolas', 9, 'bold'), bg=BG_DARK, fg=FG_TEXT).grid(
                    row=row, column=0, columnspan=5, sticky=tk.W, pady=(4, 0))
                row += 1

                for field, atlas_idx, label in [
                    ('equip_frame_index', 0, 'Equip'),
                    ('ground_frame_index', 1, 'Ground'),
                    ('pack_frame_index', 2, 'Pack'),
                ]:
                    val = item[gender].get(field, -1)
                    key = f'{gender}.{field}'
                    self._add_assign_row(row, key, atlas_idx, label, val)
                    row += 1

        self._update_highlights()
        self._show_preview(item)

    def _add_assign_row(self, row, field_key, atlas_idx, label, value):
        f = self.assign_area
        tk.Label(f, text=f'  {label}:', font=('Consolas', 9),
                 bg=BG_DARK, fg=FG_TEXT).grid(row=row, column=0, sticky=tk.W)

        val_color = GREEN if value >= 0 else RED
        val_text = str(value) if value >= 0 else '-1'
        vlbl = tk.Label(f, text=val_text, font=('Consolas', 9, 'bold'),
                        bg=BG_DARK, fg=val_color, width=5, anchor=tk.E)
        vlbl.grid(row=row, column=1, padx=4)

        btn = tk.Button(f, text='Assign', font=('Consolas', 8), padx=4,
                        command=lambda fk=field_key, ai=atlas_idx: self._begin_assign(fk, ai))
        btn.grid(row=row, column=2, padx=2)

        # Jump button — if assigned, jump atlas view to that rect
        if value >= 0:
            jbtn = tk.Button(f, text='\u27a4', font=('Segoe UI', 8), width=2,
                             command=lambda ai=atlas_idx, v=value: self._jump_to_rect(ai, v))
            jbtn.grid(row=row, column=3, padx=1)

        clr = tk.Button(f, text='X', font=('Consolas', 8), fg=RED, width=2,
                        command=lambda fk=field_key: self._clear_field(fk))
        clr.grid(row=row, column=4, padx=2)

        self._assign_widgets[field_key] = {'btn': btn, 'vlbl': vlbl}

    def _begin_assign(self, field_key, atlas_idx):
        """Enter assignment mode: switch to correct atlas, wait for click."""
        self.assigning_field = field_key
        self.atlas_view.switch_atlas(atlas_idx)

        # Highlight the active button
        for fk, w in self._assign_widgets.items():
            if fk == field_key:
                w['btn'].config(bg=ORANGE, fg='white', relief=tk.SUNKEN)
            else:
                w['btn'].config(bg='SystemButtonFace', fg='black', relief=tk.RAISED)

        parts = field_key.split('.')
        readable = parts[-1].replace('_', ' ') if len(parts) == 1 else f"{parts[0]} {parts[1].replace('_', ' ')}"
        self.instr_lbl.config(text=f'Click a rect on {ATLAS_NAMES[atlas_idx]} to assign {readable}. Esc to cancel.')

    def _cancel_assign(self):
        self.assigning_field = None
        self.auto_assign = False
        self.instr_lbl.config(text='')
        if hasattr(self, '_assign_widgets'):
            for w in self._assign_widgets.values():
                w['btn'].config(bg='SystemButtonFace', fg='black', relief=tk.RAISED)

    def _get_assign_fields(self, item):
        """Get ordered list of (field_key, atlas_idx) for all assignable fields."""
        fields = []
        if not self._is_equippable(item):
            for field, atlas_idx in [('ground_frame_index', 1), ('inventory_frame_index', 2)]:
                if field in item:
                    fields.append((field, atlas_idx))
        else:
            for gender in ('male', 'female'):
                if gender not in item:
                    continue
                for field, atlas_idx in [('equip_frame_index', 0), ('ground_frame_index', 1), ('pack_frame_index', 2)]:
                    if field in item[gender]:
                        fields.append((f'{gender}.{field}', atlas_idx))
        return fields

    def _get_field_value(self, item, field_key):
        """Get the current value for a field_key."""
        if '.' in field_key:
            gender, field = field_key.split('.', 1)
            return item.get(gender, {}).get(field, -1)
        return item.get(field_key, -1)

    def _begin_first_unassigned(self):
        """Begin assign on first unassigned field, but stay on current item if all are done."""
        if self.current_idx < 0:
            return
        item = self.items[self.current_idx]
        fields = self._get_assign_fields(item)
        for field_key, atlas_idx in fields:
            if self._get_field_value(item, field_key) == -1:
                self.auto_assign = True
                self._begin_assign(field_key, atlas_idx)
                return
        # All assigned — just stay here

    def _auto_advance_to_next_field(self):
        """Find the next unassigned field and begin assign. If all done, advance to next item."""
        if not self.auto_assign or self.current_idx < 0:
            return
        item = self.items[self.current_idx]
        fields = self._get_assign_fields(item)
        for field_key, atlas_idx in fields:
            if self._get_field_value(item, field_key) == -1:
                self._begin_assign(field_key, atlas_idx)
                return
        # All fields assigned — advance to next item
        self._advance_to_next_item()

    def _advance_to_next_item(self):
        """Select the next item in the listbox and auto-start assigning."""
        sel = self.listbox.curselection()
        if not sel:
            return
        next_lb_idx = sel[0] + 1
        if next_lb_idx >= self.listbox.size():
            self.auto_assign = False
            self.instr_lbl.config(text='All items complete!')
            return
        self.listbox.selection_clear(0, tk.END)
        self.listbox.selection_set(next_lb_idx)
        self.listbox.see(next_lb_idx)
        self.current_idx = self.listbox_map[next_lb_idx]
        self.assigning_field = None
        self._show_details()
        self.root.after(50, self._auto_advance_to_next_field)

    def _clear_field(self, field_key):
        if self.current_idx < 0:
            return
        item = self.items[self.current_idx]
        if '.' in field_key:
            gender, field = field_key.split('.', 1)
            if gender in item:
                item[gender][field] = -1
        else:
            item[field_key] = -1
        self.unsaved = True
        self._rebuild_maps()
        self._show_details()
        self._refresh_status()
        self._refresh_listbox_row()

    # --- Display name & PAK file editing ---

    def _on_set_display_name(self):
        """Set the tool-only display name for the current item."""
        if self.current_idx < 0:
            return
        item = self.items[self.current_idx]
        iid = str(item.get('id', self.current_idx))
        name = self._name_var.get().strip()
        if name:
            self.display_names[iid] = name
        elif iid in self.display_names:
            del self.display_names[iid]
        self._save_display_names()
        self._refresh_listbox_row()

    def _save_display_names(self):
        """Save the display names map to disk."""
        with open(self.display_names_path, 'w', encoding='utf-8') as f:
            json.dump(self.display_names, f, indent=2, sort_keys=True)

    def _get_effective_pak(self, item):
        """Get pak_file for an item, considering pending overrides not yet saved."""
        iid = item.get('id')
        if iid is not None and iid in self._pak_overrides:
            return self._pak_overrides[iid]
        return item.get('pak_file')

    def _on_set_pak_file(self):
        """Stage a pak_file change (applied at save time)."""
        if self.current_idx < 0:
            return
        pak = self._pak_var.get().strip()
        if not pak:
            return
        item = self.items[self.current_idx]
        iid = item.get('id')
        if iid is None:
            return
        self._pak_overrides[iid] = pak
        self.unsaved = True
        self.pak_cache.pop(pak, None)
        self._show_details()
        self._refresh_listbox_row()

    def _on_clear_pak_file(self):
        """Stage a pak_file clear (applied at save time)."""
        if self.current_idx < 0:
            return
        item = self.items[self.current_idx]
        iid = item.get('id')
        if iid is None:
            return
        self._pak_overrides[iid] = None
        self.unsaved = True
        self._show_details()
        self._refresh_listbox_row()

    def _jump_to_rect(self, atlas_idx, rect_idx):
        """Pan atlas view to center on a specific rect."""
        if atlas_idx != self.atlas_view.current_atlas:
            self.atlas_view.switch_atlas(atlas_idx)
        _, rects = self.atlas_data[atlas_idx]
        if rect_idx < 0 or rect_idx >= len(rects):
            return
        r = rects[rect_idx]
        cw = max(self.atlas_view.canvas.winfo_width(), 100)
        ch = max(self.atlas_view.canvas.winfo_height(), 100)
        z = self.atlas_view.zoom
        cx = r.x + r.width / 2
        cy = r.y + r.height / 2
        self.atlas_view.pan_x = cw / 2 - cx * z
        self.atlas_view.pan_y = ch / 2 - cy * z
        self.atlas_view.redraw()

    # --- Atlas interaction ---

    def _on_atlas_click(self, atlas_idx, rect_idx):
        if rect_idx < 0 or self.current_idx < 0:
            return

        if self.assigning_field is None:
            # No assignment active — just show info
            assigned_to = self.assign_map.get(atlas_idx, {}).get(rect_idx)
            msg = f'Rect #{rect_idx}'
            if assigned_to is not None:
                msg += f' (assigned to item #{assigned_to})'
            msg += ' — click Assign to start assigning'
            self.status_var.set(msg)
            return

        # Perform assignment
        item = self.items[self.current_idx]
        fk = self.assigning_field

        if '.' in fk:
            gender, field = fk.split('.', 1)
            if gender in item:
                item[gender][field] = rect_idx
        else:
            item[fk] = rect_idx

        self.unsaved = True
        self.assigning_field = None
        self.instr_lbl.config(text='')

        self._rebuild_maps()
        self._show_details()
        self._refresh_status()
        self._refresh_listbox_row()

        # Auto-advance to next unassigned field
        if self.auto_assign:
            self.root.after(50, self._auto_advance_to_next_field)

    def _on_atlas_hover(self, atlas_idx, rect_idx):
        if rect_idx >= 0:
            assigned_to = self.assign_map.get(atlas_idx, {}).get(rect_idx)
            _, rects = self.atlas_data[atlas_idx]
            r = rects[rect_idx]
            msg = f'Rect #{rect_idx}  ({r.width}x{r.height} at {r.x},{r.y})'
            if assigned_to is not None:
                msg += f'  [item #{assigned_to}]'
            self.status_var.set(msg)

    def _update_highlights(self):
        """Tell atlas view which rects belong to current item."""
        cur = {0: set(), 1: set(), 2: set()}
        if self.current_idx >= 0:
            item = self.items[self.current_idx]
            if not self._is_equippable(item):
                v = item.get('ground_frame_index', -1)
                if v >= 0:
                    cur[1].add(v)
                v = item.get('inventory_frame_index', -1)
                if v >= 0:
                    cur[2].add(v)
            else:
                for g in ('male', 'female'):
                    if g not in item:
                        continue
                    v = item[g].get('equip_frame_index', -1)
                    if v >= 0:
                        cur[0].add(v)
                    v = item[g].get('ground_frame_index', -1)
                    if v >= 0:
                        cur[1].add(v)
                    v = item[g].get('pack_frame_index', -1)
                    if v >= 0:
                        cur[2].add(v)

        self.atlas_view.assigned_rects = self.assign_map
        self.atlas_view.current_item_rects = cur
        self.atlas_view.redraw()

    # --- Preview ---

    DIR_NAMES = ['N', 'NE', 'E', 'SE', 'S', 'SW', 'W', 'NW']

    def _extract_frame(self, pak, sprite_idx, rect_idx=0):
        """Extract a specific frame from a PAK sprite. Returns RGBA Image or None."""
        if sprite_idx >= len(pak.sprites):
            return None
        sprite = pak.sprites[sprite_idx]
        if not sprite.rectangles:
            return None
        # Clamp to valid range
        if rect_idx >= len(sprite.rectangles):
            rect_idx = 0
        img = sprite.get_image().convert('RGBA')
        r = sprite.rectangles[rect_idx]
        return img.crop((r.x, r.y, r.x + r.width, r.y + r.height))

    def _get_dir_rect_idx(self, pak, sprite_idx, direction):
        """Get the rectangle index for the first frame of a direction (0-7).
        Layout: 8 directions × N frames, direction d starts at d * frames_per_dir."""
        if sprite_idx >= len(pak.sprites):
            return 0
        sprite = pak.sprites[sprite_idx]
        n = len(sprite.rectangles)
        if n == 0:
            return 0
        frames_per_dir = n // 8 if n >= 8 else 1
        return min(direction * frames_per_dir, n - 1)

    def _stop_preview_cycle(self):
        if self._preview_timer is not None:
            self.root.after_cancel(self._preview_timer)
            self._preview_timer = None

    def _show_preview(self, item):
        self._stop_preview_cycle()
        self._preview_dir = 0
        self._preview_item = item

        # Equip preview (animated direction cycling, equippable items only)
        effective_pak = self._get_effective_pak(item)
        if effective_pak:
            self._render_equip_frame()
            self._schedule_next_dir()
        else:
            self.equip_canvas.delete('all')
            cw = max(self.equip_canvas.winfo_width(), 50)
            ch = max(self.equip_canvas.winfo_height(), 50)
            self.equip_canvas.create_text(cw // 2, ch // 2, text='N/A',
                                          fill=FG_DIM, font=('Consolas', 9))

        # Ground and Pack previews (static, from atlas)
        self._render_atlas_preview(item)

    def _schedule_next_dir(self):
        self._preview_timer = self.root.after(500, self._cycle_preview_dir)

    def _cycle_preview_dir(self):
        self._preview_dir = (self._preview_dir + 1) % 8
        self._render_equip_frame()
        self._schedule_next_dir()

    def _extract_atlas_frame(self, atlas_idx, frame_idx):
        """Extract a single frame from the atlas by index. Returns RGBA Image or None."""
        if atlas_idx < 0 or atlas_idx >= len(self.atlas_data):
            return None
        img, rects = self.atlas_data[atlas_idx]
        if frame_idx < 0 or frame_idx >= len(rects):
            return None
        r = rects[frame_idx]
        if r.width <= 0 or r.height <= 0:
            return None
        return img.crop((r.x, r.y, r.x + r.width, r.y + r.height))

    def _render_on_canvas(self, canvas, frames, labels, tk_attr):
        """Render one or two labeled frames centered on a canvas."""
        canvas.delete('all')
        cw = max(canvas.winfo_width(), 50)
        ch = max(canvas.winfo_height(), 50)

        if not frames:
            canvas.create_text(cw // 2, ch // 2, text='N/A',
                               fill=FG_DIM, font=('Consolas', 9))
            return

        label_h = 14 if any(labels) else 0
        spacing = 6
        avail_w = (cw - spacing * (len(frames) + 1)) // max(len(frames), 1)
        avail_h = ch - spacing * 2 - label_h
        max_w = max(f.width for f in frames)
        max_h = max(f.height for f in frames)
        scale = min(avail_w / max(max_w, 1), avail_h / max(max_h, 1), 5.0)

        scaled = []
        for f in frames:
            sw = max(1, int(f.width * scale))
            sh = max(1, int(f.height * scale))
            scaled.append(f.resize((sw, sh), Image.NEAREST))

        total_w = sum(s.width for s in scaled) + spacing * (len(scaled) - 1)
        row_h = max(s.height for s in scaled)
        comp_w = max(total_w, 1)
        comp_h = max(row_h + label_h, 1)
        comp = Image.new('RGBA', (comp_w, comp_h), (45, 45, 45, 255))
        draw = ImageDraw.Draw(comp)

        x = 0
        for i, s in enumerate(scaled):
            y_off = (row_h - s.height) // 2
            comp.paste(s, (x, y_off + label_h), s)
            if labels[i]:
                lbl_x = x + s.width // 2
                color = BLUE if labels[i] == 'M' else (ORANGE if labels[i] == 'F' else FG_TEXT)
                draw.text((lbl_x, 0), labels[i], fill=color, anchor='mt')
            x += s.width + spacing

        tk_img = ImageTk.PhotoImage(comp)
        setattr(self, tk_attr, tk_img)
        canvas.create_image(cw // 2, ch // 2, image=tk_img)

    def _render_atlas_preview(self, item):
        """Render ground and pack previews from the atlas."""
        ground_frames, ground_labels = [], []
        pack_frames, pack_labels = [], []

        if bool(self._get_effective_pak(item)):
            for gender, label in [('male', 'M'), ('female', 'F')]:
                if gender not in item:
                    continue
                gfi = item[gender].get('ground_frame_index', -1)
                f = self._extract_atlas_frame(1, gfi)
                if f:
                    ground_frames.append(f)
                    ground_labels.append(label)
                pfi = item[gender].get('pack_frame_index', -1)
                f = self._extract_atlas_frame(2, pfi)
                if f:
                    pack_frames.append(f)
                    pack_labels.append(label)
        else:
            gfi = item.get('ground_frame_index', -1)
            f = self._extract_atlas_frame(1, gfi)
            if f:
                ground_frames.append(f)
                ground_labels.append('')
            ifi = item.get('inventory_frame_index', -1)
            f = self._extract_atlas_frame(2, ifi)
            if f:
                pack_frames.append(f)
                pack_labels.append('')

        self._render_on_canvas(self.ground_canvas, ground_frames, ground_labels, '_ground_tk')
        self._render_on_canvas(self.pack_canvas, pack_frames, pack_labels, '_pack_tk')

    def _render_equip_frame(self):
        """Render one direction frame of the equip animation onto the equip canvas."""
        item = getattr(self, '_preview_item', None)
        if item is None:
            return

        self.equip_canvas.delete('all')
        cw = max(self.equip_canvas.winfo_width(), 50)
        ch = max(self.equip_canvas.winfo_height(), 50)

        pak_file = self._get_effective_pak(item)
        if not pak_file:
            self.equip_canvas.create_text(cw // 2, ch // 2, text='No PAK',
                                          fill=FG_DIM, font=('Consolas', 10))
            return

        pak_path = self.items_dir / pak_file
        if not pak_path.exists():
            self.equip_canvas.create_text(cw // 2, ch // 2, text='PAK missing',
                                          fill=RED, font=('Consolas', 9))
            return

        try:
            if pak_file not in self.pak_cache:
                self.pak_cache[pak_file] = PAKFile.read(pak_path)
            pak = self.pak_cache[pak_file]

            d = self._preview_dir
            frames = []
            labels = []

            if 'male' in item:
                idx = item['male'].get('pak_index_start', 0)
                rect = self._get_dir_rect_idx(pak, idx, d)
                f = self._extract_frame(pak, idx, rect_idx=rect)
                if f:
                    frames.append(f)
                    labels.append('M')
            if 'female' in item:
                idx = item['female'].get('pak_index_start', 12)
                rect = self._get_dir_rect_idx(pak, idx, d)
                f = self._extract_frame(pak, idx, rect_idx=rect)
                if f:
                    frames.append(f)
                    labels.append('F')

            if not frames:
                self.equip_canvas.create_text(cw // 2, ch // 2, text='No sprites',
                                              fill=FG_DIM, font=('Consolas', 9))
                return

            # Scale and composite with direction label
            dir_label_h = 16
            spacing = 8
            label_h = 14
            avail_w = (cw - spacing * (len(frames) + 1)) // len(frames)
            avail_h = ch - spacing * 2 - label_h - dir_label_h
            max_w = max(f.width for f in frames)
            max_h = max(f.height for f in frames)
            scale = min(avail_w / max(max_w, 1), avail_h / max(max_h, 1), 5.0)

            scaled = []
            for f in frames:
                sw = max(1, int(f.width * scale))
                sh = max(1, int(f.height * scale))
                scaled.append(f.resize((sw, sh), Image.NEAREST))

            total_w = sum(s.width for s in scaled) + spacing * (len(scaled) - 1)
            row_h = max(s.height for s in scaled)
            comp_w = max(total_w, 1)
            comp_h = max(row_h + label_h + dir_label_h, 1)
            comp = Image.new('RGBA', (comp_w, comp_h), (45, 45, 45, 255))
            draw = ImageDraw.Draw(comp)

            dir_name = self.DIR_NAMES[d]
            draw.text((comp_w // 2, 0), dir_name, fill=(200, 200, 200, 255), anchor='mt')

            x = 0
            for i, s in enumerate(scaled):
                y_off = (row_h - s.height) // 2
                comp.paste(s, (x, y_off + label_h + dir_label_h), s)
                lbl_x = x + s.width // 2
                draw.text((lbl_x, dir_label_h), labels[i],
                          fill=BLUE if labels[i] == 'M' else ORANGE, anchor='mt')
                x += s.width + spacing

            self._equip_tk = ImageTk.PhotoImage(comp)
            self.equip_canvas.create_image(cw // 2, ch // 2, image=self._equip_tk)

        except Exception as e:
            self.equip_canvas.create_text(cw // 2, ch // 2, text=f'Error:\n{e}',
                                          fill=RED, font=('Consolas', 8), width=cw - 10)

    # --- Status ---

    def _refresh_status(self):
        total = assigned = 0
        for item in self.items:
            a, t = self._item_status(item)
            total += t
            assigned += a
        marker = ' [UNSAVED]' if self.unsaved else ''
        self.status_var.set(f'Assigned: {assigned}/{total} frames{marker}')

    def _refresh_listbox_row(self):
        """Update the current listbox row's color after assignment change."""
        if self.current_idx < 0:
            return
        try:
            lb_idx = self.listbox_map.index(self.current_idx)
        except ValueError:
            return

        item = self.items[self.current_idx]
        a, t = self._item_status(item)
        name = self._item_display(item, self.current_idx)

        if t == 0:
            marker = ' '
        elif a == t:
            marker = '*'
        elif a > 0:
            marker = '~'
        else:
            marker = ' '

        display = f"[{marker}] {item.get('id', self.current_idx):>3d}  {name}"
        self.listbox.delete(lb_idx)
        self.listbox.insert(lb_idx, display)

        if marker == '*' and self._has_duplicate_genders(item):
            self.listbox.itemconfig(lb_idx, fg=YELLOW)
        elif marker == '*':
            self.listbox.itemconfig(lb_idx, fg=GREEN)
        elif marker == '~':
            self.listbox.itemconfig(lb_idx, fg=YELLOW)
        else:
            self.listbox.itemconfig(lb_idx, fg=FG_TEXT)

        self.listbox.selection_set(lb_idx)

    # --- Save / Close ---

    def _apply_pak_overrides(self):
        """Apply pending pak_file changes to items (called at save time)."""
        if not self._pak_overrides:
            return
        id_to_item = {item.get('id'): item for item in self.items}
        for iid, pak in self._pak_overrides.items():
            item = id_to_item.get(iid)
            if item is None:
                continue
            item['pak_file'] = pak
            # If transitioning to equippable, initialize gender data
            if pak and 'male' not in item and 'female' not in item:
                item['male'] = {
                    'pak_index_start': 0, 'pak_index_end': 11,
                    'equip_frame_index': -1, 'ground_frame_index': -1, 'pack_frame_index': -1,
                }
                item['female'] = {
                    'pak_index_start': 12, 'pak_index_end': 23,
                    'equip_frame_index': -1, 'ground_frame_index': -1, 'pack_frame_index': -1,
                }
        self._pak_overrides.clear()
        self._rebuild_maps()

    def _save(self):
        self._apply_pak_overrides()
        with open(self.metadata_path, 'w', encoding='utf-8') as f:
            json.dump(self.items, f, indent=2)
        self.unsaved = False
        self._refresh_status()
        self.status_var.set(f'Saved to {self.metadata_path}')

    def _save_as(self):
        path = filedialog.asksaveasfilename(
            defaultextension='.json',
            filetypes=[('JSON', '*.json')],
            initialfile='ItemSpriteMetadata.json')
        if path:
            self._apply_pak_overrides()
            with open(path, 'w', encoding='utf-8') as f:
                json.dump(self.items, f, indent=2)
            self.unsaved = False
            self._refresh_status()

    def _on_close(self):
        if self.unsaved:
            if not messagebox.askyesno('Unsaved Changes', 'Discard unsaved changes?',
                                       parent=self.root):
                return
        self.root.destroy()

    # --- Entry creation / deletion ---

    def _next_id(self):
        """Get the next available metadata ID."""
        if not self.items:
            return 1
        return max(e.get('id', 0) for e in self.items) + 1

    def _available_paks(self):
        """List PAK files in the items directory."""
        paks = []
        if self.items_dir.exists():
            for p in sorted(self.items_dir.iterdir()):
                if p.suffix == '.pak':
                    paks.append(p.name)
        return paks

    def _new_entry_menu(self):
        """Show a popup menu for choosing entry type."""
        menu = tk.Menu(self.root, tearoff=0)
        menu.add_command(label='Equippable (weapon/armor/etc.)',
                         command=self._new_equip_entry)
        menu.add_command(label='Non-Equippable (potion/material/etc.)',
                         command=self._new_nonequip_entry)
        menu.tk_popup(self.root.winfo_pointerx(), self.root.winfo_pointery())

    def _new_equip_entry(self):
        """Create a new equippable metadata entry via dialog."""
        dlg = NewEquipDialog(self.root, self._available_paks())
        if dlg.result is None:
            return

        name, equip_type, pak_file, has_male, has_female = dlg.result
        entry = {
            'id': self._next_id(),
            'name': name,
            'equip_type': equip_type,
            'pak_file': pak_file,
        }
        if has_male:
            entry['male'] = {
                'pak_index_start': 0,
                'pak_index_end': 11,
                'equip_frame_index': -1,
                'ground_frame_index': -1,
                'pack_frame_index': -1,
            }
        if has_female:
            entry['female'] = {
                'pak_index_start': 12,
                'pak_index_end': 23,
                'equip_frame_index': -1,
                'ground_frame_index': -1,
                'pack_frame_index': -1,
            }

        self.items.append(entry)
        self.display_names[str(entry['id'])] = name
        self._save_display_names()
        self.unsaved = True
        self._rebuild_maps()
        self._populate_list()
        self._refresh_status()

        # Select the new entry
        self._select_item_by_id(entry['id'])

    def _new_nonequip_entry(self):
        """Create a new non-equippable metadata entry via dialog."""
        dlg = NewNonEquipDialog(self.root)
        if dlg.result is None:
            return

        name = dlg.result
        entry = {
            'id': self._next_id(),
            'name': name,
            'equip_type': 'none',
            'pak_file': None,
            'inventory_frame_index': -1,
            'ground_frame_index': -1,
        }

        self.items.append(entry)
        self.display_names[str(entry['id'])] = name
        self._save_display_names()
        self.unsaved = True
        self._rebuild_maps()
        self._populate_list()
        self._refresh_status()

        # Select the new entry
        self._select_item_by_id(entry['id'])

    def _delete_entry(self):
        """Delete the currently selected metadata entry."""
        if self.current_idx < 0:
            return
        item = self.items[self.current_idx]
        name = item.get('name', f"Item #{item.get('id', '?')}")
        if not messagebox.askyesno('Delete Entry',
                                   f'Delete "{name}" (ID {item.get("id")})?\n\n'
                                   'This cannot be undone (until you save).',
                                   parent=self.root):
            return

        self.items.pop(self.current_idx)
        self.current_idx = -1
        self.unsaved = True
        self._rebuild_maps()
        self._populate_list()
        self._refresh_status()


class NewEquipDialog(tk.Toplevel):
    """Dialog for creating a new equippable metadata entry."""

    EQUIP_TYPES = ['weapon', 'shield', 'armor', 'helm', 'pants', 'boots', 'mantle', 'arm_armor']

    def __init__(self, parent, pak_files):
        super().__init__(parent)
        self.title('New Equippable Entry')
        self.configure(bg=BG_DARK)
        self.transient(parent)
        self.grab_set()
        center_on_parent(self, parent, 450, 320)
        self.result = None

        pad = dict(padx=8, pady=4)
        row = 0

        # Name
        tk.Label(self, text='Name:', bg=BG_DARK, fg=FG_TEXT,
                 font=('Consolas', 10)).grid(row=row, column=0, sticky=tk.W, **pad)
        self._name_var = tk.StringVar()
        tk.Entry(self, textvariable=self._name_var, font=('Consolas', 10),
                 width=30).grid(row=row, column=1, sticky=tk.EW, **pad)
        row += 1

        # Equip type
        tk.Label(self, text='Equip Type:', bg=BG_DARK, fg=FG_TEXT,
                 font=('Consolas', 10)).grid(row=row, column=0, sticky=tk.W, **pad)
        self._type_var = tk.StringVar(value='weapon')
        ttk.Combobox(self, textvariable=self._type_var, values=self.EQUIP_TYPES,
                     font=('Consolas', 10), width=28).grid(
            row=row, column=1, sticky=tk.EW, **pad)
        row += 1

        # PAK file
        tk.Label(self, text='PAK File:', bg=BG_DARK, fg=FG_TEXT,
                 font=('Consolas', 10)).grid(row=row, column=0, sticky=tk.W, **pad)
        self._pak_var = tk.StringVar()
        pak_combo = ttk.Combobox(self, textvariable=self._pak_var, values=pak_files,
                                 font=('Consolas', 10), width=28)
        pak_combo.grid(row=row, column=1, sticky=tk.EW, **pad)
        row += 1

        # Gender checkboxes
        tk.Label(self, text='Genders:', bg=BG_DARK, fg=FG_TEXT,
                 font=('Consolas', 10)).grid(row=row, column=0, sticky=tk.W, **pad)
        gf = tk.Frame(self, bg=BG_DARK)
        gf.grid(row=row, column=1, sticky=tk.W, **pad)
        self._male_var = tk.BooleanVar(value=True)
        self._female_var = tk.BooleanVar(value=True)
        tk.Checkbutton(gf, text='Male', variable=self._male_var, bg=BG_DARK,
                       fg=FG_TEXT, selectcolor=BG_LIGHT, activebackground=BG_DARK,
                       font=('Consolas', 10)).pack(side=tk.LEFT)
        tk.Checkbutton(gf, text='Female', variable=self._female_var, bg=BG_DARK,
                       fg=FG_TEXT, selectcolor=BG_LIGHT, activebackground=BG_DARK,
                       font=('Consolas', 10)).pack(side=tk.LEFT, padx=(10, 0))
        row += 1

        # Buttons
        bf = tk.Frame(self, bg=BG_DARK)
        bf.grid(row=row, column=0, columnspan=2, pady=12)
        tk.Button(bf, text='Create', command=self._on_ok, width=10,
                  font=('Consolas', 10)).pack(side=tk.LEFT, padx=4)
        tk.Button(bf, text='Cancel', command=self.destroy, width=10,
                  font=('Consolas', 10)).pack(side=tk.LEFT, padx=4)

        self.columnconfigure(1, weight=1)
        self.protocol('WM_DELETE_WINDOW', self.destroy)
        self.wait_window()

    def _on_ok(self):
        name = self._name_var.get().strip()
        if not name:
            messagebox.showwarning('Missing Name', 'Enter a name.', parent=self)
            return
        pak = self._pak_var.get().strip()
        if not pak:
            messagebox.showwarning('Missing PAK', 'Select a PAK file.', parent=self)
            return
        has_m = self._male_var.get()
        has_f = self._female_var.get()
        if not has_m and not has_f:
            messagebox.showwarning('No Gender', 'Select at least one gender.',
                                   parent=self)
            return
        self.result = (name, self._type_var.get(), pak, has_m, has_f)
        self.destroy()


class NewNonEquipDialog(tk.Toplevel):
    """Dialog for creating a new non-equippable metadata entry."""

    def __init__(self, parent):
        super().__init__(parent)
        self.title('New Non-Equippable Entry')
        self.configure(bg=BG_DARK)
        self.transient(parent)
        self.grab_set()
        center_on_parent(self, parent, 400, 140)
        self.result = None

        pad = dict(padx=8, pady=6)

        tk.Label(self, text='Name:', bg=BG_DARK, fg=FG_TEXT,
                 font=('Consolas', 10)).grid(row=0, column=0, sticky=tk.W, **pad)
        self._name_var = tk.StringVar()
        e = tk.Entry(self, textvariable=self._name_var, font=('Consolas', 10),
                     width=30)
        e.grid(row=0, column=1, sticky=tk.EW, **pad)
        e.focus_set()

        bf = tk.Frame(self, bg=BG_DARK)
        bf.grid(row=1, column=0, columnspan=2, pady=12)
        tk.Button(bf, text='Create', command=self._on_ok, width=10,
                  font=('Consolas', 10)).pack(side=tk.LEFT, padx=4)
        tk.Button(bf, text='Cancel', command=self.destroy, width=10,
                  font=('Consolas', 10)).pack(side=tk.LEFT, padx=4)

        self.columnconfigure(1, weight=1)
        self.protocol('WM_DELETE_WINDOW', self.destroy)
        self.wait_window()

    def _on_ok(self):
        name = self._name_var.get().strip()
        if not name:
            messagebox.showwarning('Missing Name', 'Enter a name.', parent=self)
            return
        self.result = name
        self.destroy()


def main():
    parser = argparse.ArgumentParser(description='Item Frame Mapper GUI')
    parser.add_argument('--metadata', default=str(DEFAULT_METADATA), help='ItemSpriteMetadata.json path')
    parser.add_argument('--atlas', default=str(DEFAULT_ATLAS), help='item_atlas.pak path')
    parser.add_argument('--items-dir', default=str(DEFAULT_ITEMS_DIR), help='Per-item PAK directory')
    parser.add_argument('--select-id', type=int, default=None, help='Auto-select item by metadata ID')
    parser.add_argument('--atlas-tab', type=int, default=None, choices=[0, 1, 2],
                        help='Switch to atlas tab (0=equip, 1=ground, 2=pack)')
    args = parser.parse_args()

    root = tk.Tk()
    app = ItemFrameMapperApp(root, args.metadata, args.atlas, args.items_dir,
                             select_id=args.select_id, atlas_tab=args.atlas_tab)
    root.mainloop()


if __name__ == '__main__':
    main()
