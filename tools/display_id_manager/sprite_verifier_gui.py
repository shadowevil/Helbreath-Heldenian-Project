#!/usr/bin/env python3
"""
Sprite Verifier GUI — scroll through per-item PAK files to visually verify
that all 24 sprites (12 male + 12 female) look correct.

Usage:
    python sprite_verifier_gui.py
"""

import tkinter as tk
from tkinter import ttk
import json
import sys
from pathlib import Path
from PIL import Image, ImageTk, ImageDraw, ImageFont
import io

_SCRIPT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(_SCRIPT_DIR))
from paklib import PAKFile

DEFAULT_ITEMS_DIR = _SCRIPT_DIR / 'sprites' / 'items'
DEFAULT_METADATA = _SCRIPT_DIR / 'contents' / 'item_sprite_metadata.json'

BODY_POSE_NAMES = [
    "Standing Peace",    # 0
    "Standing Combat",   # 1
    "Walking Peace",     # 2
    "Walking Combat",    # 3
    "Running",           # 4
    "Attack Standing",   # 5
    "Attack Walk 1H",    # 6
    "Attack Walk 2H",    # 7
    "Magic Cast",        # 8
    "Pickup",            # 9
    "Take Damage",       # 10
    "Dying",             # 11
]

BG_DARK = '#1e1e1e'
BG_MID = '#2d2d2d'
BG_LIGHT = '#3c3c3c'
FG_TEXT = '#d4d4d4'
FG_DIM = '#808080'
GREEN = '#4ec94e'
RED = '#cc4444'
BLUE = '#4a9eff'
ORANGE = '#ff8c00'
YELLOW = '#cccc00'

THUMB_SIZE = 80  # thumbnail box size


def make_checker_bg(width, height, tile_size=8):
    tile = Image.new('RGBA', (tile_size * 2, tile_size * 2), (45, 45, 45, 255))
    draw = ImageDraw.Draw(tile)
    draw.rectangle([tile_size, 0, tile_size * 2, tile_size], fill=(60, 60, 60, 255))
    draw.rectangle([0, tile_size, tile_size, tile_size * 2], fill=(60, 60, 60, 255))
    bg = Image.new('RGBA', (width, height))
    for y in range(0, height, tile_size * 2):
        for x in range(0, width, tile_size * 2):
            bg.paste(tile, (x, y))
    return bg


def extract_frame_thumb(sprite, rect_idx=0, size=THUMB_SIZE):
    """Extract a single frame from a sprite and return a thumbnail."""
    if not sprite.rectangles:
        # Blank sprite — return red-tinted placeholder
        img = Image.new('RGBA', (size, size), (80, 20, 20, 255))
        draw = ImageDraw.Draw(img)
        draw.text((size // 4, size // 3), "BLANK", fill=(200, 60, 60))
        return img

    try:
        src_img = sprite.get_image().convert('RGBA')
    except Exception:
        img = Image.new('RGBA', (size, size), (80, 20, 20, 255))
        draw = ImageDraw.Draw(img)
        draw.text((size // 4, size // 3), "ERR", fill=(200, 60, 60))
        return img

    if rect_idx >= len(sprite.rectangles):
        rect_idx = 0

    r = sprite.rectangles[rect_idx]

    # Clamp to image bounds
    x1 = max(0, r.x)
    y1 = max(0, r.y)
    x2 = min(src_img.width, r.x + r.width)
    y2 = min(src_img.height, r.y + r.height)

    if x2 <= x1 or y2 <= y1:
        # Degenerate rect
        frame = src_img
    else:
        frame = src_img.crop((x1, y1, x2, y2))

    # Composite on checker background
    bg = make_checker_bg(frame.width, frame.height)
    bg.paste(frame, (0, 0), frame)

    # Scale to fit thumbnail
    bg.thumbnail((size, size), Image.Resampling.NEAREST if max(bg.size) < size else Image.Resampling.LANCZOS)

    # Center on thumbnail canvas
    thumb = Image.new('RGBA', (size, size), (30, 30, 30, 255))
    ox = (size - bg.width) // 2
    oy = (size - bg.height) // 2
    thumb.paste(bg, (ox, oy), bg)

    return thumb


class SpriteVerifierApp:
    def __init__(self, root):
        self.root = root
        self.root.title("Sprite Verifier")
        self.root.configure(bg=BG_DARK)
        self.root.geometry("1400x900")

        self.items_dir = DEFAULT_ITEMS_DIR
        self.metadata_path = DEFAULT_METADATA
        self.metadata = []
        self.equippable = []  # Items with pak_file
        self.pak_cache = {}
        self.tk_images = []  # prevent GC

        self._load_metadata()
        self._build_ui()
        self._populate_list()

        if self.equippable:
            self.item_list.selection_set(0)
            self.item_list.event_generate("<<ListboxSelect>>")

    def _load_metadata(self):
        if self.metadata_path.exists():
            with open(self.metadata_path) as f:
                self.metadata = json.load(f)
        # Filter to equippable items that have a PAK file
        self.equippable = [
            m for m in self.metadata
            if m.get("pak_file") and (self.items_dir / m["pak_file"]).exists()
        ]
        # Sort by pak_file for easy browsing
        self.equippable.sort(key=lambda m: m["pak_file"])

    def _build_ui(self):
        # Main horizontal split
        left = tk.Frame(self.root, bg=BG_DARK, width=280)
        left.pack(side=tk.LEFT, fill=tk.Y, padx=4, pady=4)
        left.pack_propagate(False)

        right = tk.Frame(self.root, bg=BG_DARK)
        right.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=4, pady=4)

        # ---- Left: Search + Item List ----
        search_frame = tk.Frame(left, bg=BG_DARK)
        search_frame.pack(fill=tk.X, pady=(0, 4))

        tk.Label(search_frame, text="Search:", bg=BG_DARK, fg=FG_TEXT).pack(side=tk.LEFT)
        self.search_var = tk.StringVar()
        self.search_var.trace_add('write', lambda *_: self._filter_list())
        self.search_entry = tk.Entry(search_frame, textvariable=self.search_var,
                                     bg=BG_MID, fg=FG_TEXT, insertbackground=FG_TEXT)
        self.search_entry.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=4)

        list_frame = tk.Frame(left, bg=BG_DARK)
        list_frame.pack(fill=tk.BOTH, expand=True)

        self.item_list = tk.Listbox(list_frame, bg=BG_MID, fg=FG_TEXT,
                                    selectbackground=BLUE, selectforeground='white',
                                    font=('Consolas', 9))
        scrollbar = tk.Scrollbar(list_frame, command=self.item_list.yview)
        self.item_list.config(yscrollcommand=scrollbar.set)
        self.item_list.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        self.item_list.bind('<<ListboxSelect>>', self._on_select)

        # Count label
        self.count_label = tk.Label(left, text="", bg=BG_DARK, fg=FG_DIM, font=('Consolas', 9))
        self.count_label.pack(fill=tk.X, pady=(4, 0))

        # Nav buttons
        nav = tk.Frame(left, bg=BG_DARK)
        nav.pack(fill=tk.X, pady=4)
        tk.Button(nav, text="< Prev", command=self._prev, bg=BG_LIGHT, fg=FG_TEXT,
                  relief=tk.FLAT).pack(side=tk.LEFT, expand=True, fill=tk.X, padx=2)
        tk.Button(nav, text="Next >", command=self._next, bg=BG_LIGHT, fg=FG_TEXT,
                  relief=tk.FLAT).pack(side=tk.LEFT, expand=True, fill=tk.X, padx=2)

        # ---- Right: Sprite Grid ----
        self.info_label = tk.Label(right, text="Select an item", bg=BG_DARK, fg=FG_TEXT,
                                   font=('Consolas', 12, 'bold'), anchor='w')
        self.info_label.pack(fill=tk.X, pady=(0, 4))

        self.detail_label = tk.Label(right, text="", bg=BG_DARK, fg=FG_DIM,
                                     font=('Consolas', 9), anchor='w')
        self.detail_label.pack(fill=tk.X, pady=(0, 8))

        # Scrollable grid area
        canvas_frame = tk.Frame(right, bg=BG_DARK)
        canvas_frame.pack(fill=tk.BOTH, expand=True)

        self.grid_canvas = tk.Canvas(canvas_frame, bg=BG_DARK, highlightthickness=0)
        self.grid_scroll = tk.Scrollbar(canvas_frame, orient=tk.VERTICAL,
                                        command=self.grid_canvas.yview)
        self.grid_canvas.config(yscrollcommand=self.grid_scroll.set)
        self.grid_scroll.pack(side=tk.RIGHT, fill=tk.Y)
        self.grid_canvas.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        self.grid_inner = tk.Frame(self.grid_canvas, bg=BG_DARK)
        self.grid_canvas.create_window((0, 0), window=self.grid_inner, anchor='nw')
        self.grid_inner.bind('<Configure>',
                             lambda e: self.grid_canvas.config(scrollregion=self.grid_canvas.bbox('all')))

        # Bind mousewheel
        self.grid_canvas.bind('<Enter>',
                              lambda e: self.grid_canvas.bind_all('<MouseWheel>', self._on_mousewheel))
        self.grid_canvas.bind('<Leave>',
                              lambda e: self.grid_canvas.unbind_all('<MouseWheel>'))

        # Keyboard nav
        self.root.bind('<Up>', lambda e: self._prev())
        self.root.bind('<Down>', lambda e: self._next())

        # Displayed items mapping (for filtering)
        self._displayed = list(range(len(self.equippable)))

    def _on_mousewheel(self, event):
        self.grid_canvas.yview_scroll(-1 * (event.delta // 120), 'units')

    def _populate_list(self):
        self.item_list.delete(0, tk.END)
        self._displayed = []
        search = self.search_var.get().lower()
        for i, m in enumerate(self.equippable):
            pak = m.get("pak_file", "")
            name = m.get("name", pak.replace(".pak", ""))
            label = f"{name}"
            if search and search not in label.lower() and search not in pak.lower():
                continue
            self._displayed.append(i)
            self.item_list.insert(tk.END, label)
        self.count_label.config(text=f"{len(self._displayed)} / {len(self.equippable)} items")

    def _filter_list(self):
        sel_idx = self._get_selected_data_idx()
        self._populate_list()
        # Try to re-select the same item
        if sel_idx is not None:
            try:
                list_idx = self._displayed.index(sel_idx)
                self.item_list.selection_set(list_idx)
                self.item_list.see(list_idx)
            except ValueError:
                pass

    def _get_selected_data_idx(self):
        sel = self.item_list.curselection()
        if not sel:
            return None
        return self._displayed[sel[0]]

    def _on_select(self, event=None):
        idx = self._get_selected_data_idx()
        if idx is None:
            return
        self._show_item(self.equippable[idx])

    def _prev(self):
        sel = self.item_list.curselection()
        if not sel:
            return
        new = max(0, sel[0] - 1)
        self.item_list.selection_clear(0, tk.END)
        self.item_list.selection_set(new)
        self.item_list.see(new)
        self.item_list.event_generate("<<ListboxSelect>>")

    def _next(self):
        sel = self.item_list.curselection()
        if not sel:
            return
        new = min(self.item_list.size() - 1, sel[0] + 1)
        self.item_list.selection_clear(0, tk.END)
        self.item_list.selection_set(new)
        self.item_list.see(new)
        self.item_list.event_generate("<<ListboxSelect>>")

    def _get_pak(self, pak_file):
        if pak_file not in self.pak_cache:
            path = self.items_dir / pak_file
            if not path.exists():
                return None
            try:
                self.pak_cache[pak_file] = PAKFile.read(path)
            except Exception as e:
                print(f"Error reading {pak_file}: {e}")
                return None
        return self.pak_cache[pak_file]

    def _show_item(self, meta):
        # Clear previous
        for w in self.grid_inner.winfo_children():
            w.destroy()
        self.tk_images.clear()

        pak_file = meta.get("pak_file", "?")
        name = meta.get("name", pak_file.replace(".pak", ""))
        equip_type = meta.get("equip_type", "?")
        has_male = "male" in meta
        has_female = "female" in meta

        self.info_label.config(text=f"{name}  ({pak_file})")

        gender_info = []
        if has_male:
            gender_info.append("Male (0-11)")
        if has_female:
            gender_info.append("Female (12-23)")
        self.detail_label.config(
            text=f"Type: {equip_type}  |  Genders: {', '.join(gender_info)}  |  "
                 f"ID: {meta.get('id', '?')}")

        pak = self._get_pak(pak_file)
        if not pak:
            tk.Label(self.grid_inner, text=f"Cannot load {pak_file}",
                     bg=BG_DARK, fg=RED, font=('Consolas', 11)).grid(row=0, column=0)
            return

        # ---- Header row ----
        tk.Label(self.grid_inner, text="BodyPose", bg=BG_DARK, fg=FG_DIM,
                 font=('Consolas', 9, 'bold'), width=18, anchor='w').grid(row=0, column=0, padx=2)
        if has_male:
            tk.Label(self.grid_inner, text="Male (south)", bg=BG_DARK, fg=BLUE,
                     font=('Consolas', 9, 'bold')).grid(row=0, column=1, padx=2)
            tk.Label(self.grid_inner, text="Rects", bg=BG_DARK, fg=FG_DIM,
                     font=('Consolas', 8)).grid(row=0, column=2, padx=2)
        if has_female:
            f_col = 3 if has_male else 1
            tk.Label(self.grid_inner, text="Female (south)", bg=BG_DARK, fg=ORANGE,
                     font=('Consolas', 9, 'bold')).grid(row=0, column=f_col, padx=2)
            tk.Label(self.grid_inner, text="Rects", bg=BG_DARK, fg=FG_DIM,
                     font=('Consolas', 8)).grid(row=0, column=f_col + 1, padx=2)

        # ---- Sprite rows ----
        for pose_idx in range(12):
            row = pose_idx + 1
            pose_name = BODY_POSE_NAMES[pose_idx] if pose_idx < len(BODY_POSE_NAMES) else f"Pose {pose_idx}"

            # Pose label
            tk.Label(self.grid_inner, text=f"{pose_idx:2d}. {pose_name}",
                     bg=BG_DARK, fg=FG_TEXT, font=('Consolas', 9),
                     anchor='w', width=18).grid(row=row, column=0, padx=2, pady=1, sticky='w')

            # Male sprite
            if has_male:
                self._add_sprite_cell(pak, pose_idx, row, 1, 2)

            # Female sprite
            if has_female:
                f_col = 3 if has_male else 1
                self._add_sprite_cell(pak, pose_idx + 12, row, f_col, f_col + 1)

        # Update scroll region
        self.grid_inner.update_idletasks()
        self.grid_canvas.config(scrollregion=self.grid_canvas.bbox('all'))
        self.grid_canvas.yview_moveto(0)

    def _add_sprite_cell(self, pak, sprite_idx, grid_row, img_col, info_col):
        """Add a sprite thumbnail + info to the grid."""
        if sprite_idx >= len(pak.sprites):
            tk.Label(self.grid_inner, text="N/A", bg=BG_DARK, fg=RED,
                     font=('Consolas', 8)).grid(row=grid_row, column=img_col)
            return

        sprite = pak.sprites[sprite_idx]
        n_rects = len(sprite.rectangles)

        # Choose a representative frame:
        # For south-facing: typically rect 0, or rect 24 for sprites with 8*N layout
        if n_rects == 0:
            rect_idx = 0
        elif n_rects > 24:
            rect_idx = 24  # south-facing, second frame
        else:
            rect_idx = 0

        thumb = extract_frame_thumb(sprite, rect_idx=rect_idx, size=THUMB_SIZE)
        tk_img = ImageTk.PhotoImage(thumb)
        self.tk_images.append(tk_img)

        lbl = tk.Label(self.grid_inner, image=tk_img, bg=BG_DARK, bd=1,
                       relief=tk.SOLID)
        lbl.grid(row=grid_row, column=img_col, padx=2, pady=1)

        # Rect count label (color coded)
        if n_rects == 0:
            color = RED
            text = "blank"
        elif n_rects < 8:
            color = YELLOW
            text = f"{n_rects}r"
        else:
            color = GREEN
            text = f"{n_rects}r"

        tk.Label(self.grid_inner, text=text, bg=BG_DARK, fg=color,
                 font=('Consolas', 8)).grid(row=grid_row, column=info_col, padx=2)


def main():
    root = tk.Tk()
    app = SpriteVerifierApp(root)
    root.mainloop()


if __name__ == "__main__":
    main()
