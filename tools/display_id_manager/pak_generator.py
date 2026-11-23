#!/usr/bin/env python3
"""
PAK Generator — Create 24-sprite per-item equipment PAK files from old-style PAKs.

Converts old Helbreath PAK files (one per gender) into the unified format:
  Sprites 0-11:  Male body poses (standing_peace through dying)
  Sprites 12-23: Female body poses

Processing by equip type:
  Armor/Helm/Pants/Boots/Mantle/Arm_armor:
    Source PAK has 12 sprites (one per body pose, 8 dirs baked in). Direct copy.
  Shield:
    Source PAK has 7 sprites (one per shield pose). Mapped to 12 body poses.
  Weapon:
    Source PAK has 56 sprites (7 poses x 8 directions). 8 dir sprites composited
    into 1 per body pose.

Usage:
    python pak_generator.py
"""

import tkinter as tk
from tkinter import ttk, filedialog, messagebox
import sys
from pathlib import Path

_SCRIPT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(_SCRIPT_DIR))
from paklib import PAKFile, Sprite, SpriteRectangle
from PIL import Image, ImageTk, ImageDraw
import io

# Defaults
DEFAULT_ITEMS_DIR = _SCRIPT_DIR / 'sprites' / 'items'

# Body pose names (12 total)
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

EQUIP_TYPES = ['weapon', 'weapon_bow', 'shield', 'armor', 'helm', 'pants', 'boots', 'mantle', 'arm_armor']

# Weapon/shield pose mapping: bodyPose index -> weapon/shield pose index (-1 = not drawn)
# Pose 6 = melee attack (swords, axes, hammers, wands)
# Pose 7 = bow attack (archery weapons only)
# Melee weapons get attack sprite at pose 6 only; bows get it at pose 7 only.
WEAPON_POSE_MELEE = [0, 1, 2, 3, 6, -1, 4, -1, -1, -1, 5, -1]
WEAPON_POSE_BOW   = [0, 1, 2, 3, 6, -1, -1, 4, -1, -1, 5, -1]
SHIELD_POSE       = [0, 1, 2, 3, 6, -1, 4, 4, -1, -1, 5, -1]

# Colors (consistent with other tools)
BG_DARK = '#1e1e1e'
BG_MID = '#2d2d2d'
BG_LIGHT = '#3c3c3c'
FG_TEXT = '#d4d4d4'
FG_DIM = '#808080'
GREEN = '#4ec94e'
RED = '#cc4444'
BLUE = '#4a9eff'
ORANGE = '#ff8c00'


# ---------------------------------------------------------------------------
# Processing helpers
# ---------------------------------------------------------------------------

def make_blank_sprite():
    """Create a 1x1 transparent sprite with 0 rects."""
    img = Image.new("RGBA", (1, 1), (0, 0, 0, 0))
    return Sprite.from_image(img, [])


def pack_directions_vertical(direction_sprites, spacing=1):
    """Combine 8 direction sprites into 1 by stacking vertically."""
    if not direction_sprites:
        return make_blank_sprite()

    dir_images = []
    dir_heights = []
    for ds in direction_sprites:
        img = ds.get_image().convert("RGBA")
        dir_images.append(img)
        dir_heights.append(img.height)

    canvas_width = max(img.width for img in dir_images)
    canvas_height = sum(dir_heights) + spacing * (len(direction_sprites) - 1)
    canvas = Image.new("RGBA", (canvas_width, canvas_height), (0, 0, 0, 0))

    combined_rects = []
    y_offset = 0
    for i, ds in enumerate(direction_sprites):
        canvas.paste(dir_images[i], (0, y_offset))
        for r in ds.rectangles:
            combined_rects.append(SpriteRectangle(
                x=r.x, y=r.y + y_offset,
                width=r.width, height=r.height,
                pivot_x=r.pivot_x, pivot_y=r.pivot_y,
            ))
        y_offset += dir_heights[i] + spacing

    return Sprite.from_image(canvas, combined_rects)


def process_armor(pak, offset=0):
    """Armor: 12 sprites starting at offset, one per body pose. Direct copy."""
    sprites = []
    for i in range(12):
        idx = offset + i
        if idx < len(pak.sprites):
            sprites.append(pak.sprites[idx])
        else:
            sprites.append(make_blank_sprite())
    return sprites


def process_shield(pak, offset=0):
    """Shield: 7 shield poses mapped to 12 body poses."""
    sprites = []
    for pose_idx in range(12):
        sp = SHIELD_POSE[pose_idx]
        if sp == -1:
            sprites.append(make_blank_sprite())
        else:
            idx = offset + sp
            if idx < len(pak.sprites):
                sprites.append(pak.sprites[idx])
            else:
                sprites.append(make_blank_sprite())
    return sprites


def _process_weapon_with_pose_map(pak, pose_map, offset=0):
    """Weapon: 7 poses x 8 dirs = 56 sprites. Composite 8 dirs into 1 per pose."""
    sprites = []
    for pose_idx in range(12):
        wp = pose_map[pose_idx]
        if wp == -1:
            sprites.append(make_blank_sprite())
        else:
            base = offset + wp * 8
            dir_sprites = []
            valid = True
            for d in range(8):
                idx = base + d
                if idx < len(pak.sprites):
                    dir_sprites.append(pak.sprites[idx])
                else:
                    valid = False
                    break
            if valid:
                sprites.append(pack_directions_vertical(dir_sprites))
            else:
                sprites.append(make_blank_sprite())
    return sprites


def process_weapon_melee(pak, offset=0):
    """Melee weapon: attack sprite at pose 6 only."""
    return _process_weapon_with_pose_map(pak, WEAPON_POSE_MELEE, offset)


def process_weapon_bow(pak, offset=0):
    """Bow weapon: attack sprite at pose 7 only."""
    return _process_weapon_with_pose_map(pak, WEAPON_POSE_BOW, offset)


PROCESSORS = {
    'weapon': process_weapon_melee,
    'weapon_bow': process_weapon_bow,
    'shield': process_shield,
    'armor': process_armor,
    'helm': process_armor,
    'pants': process_armor,
    'boots': process_armor,
    'mantle': process_armor,
    'arm_armor': process_armor,
}


def generate_pak(male_path, female_path, equip_type, male_offset=0, female_offset=0):
    """Generate a 24-sprite PAK. Returns (PAKFile, summary_str)."""
    processor = PROCESSORS[equip_type]
    summary_lines = []

    # Male sprites (0-11)
    if male_path:
        male_pak = PAKFile.read(Path(male_path))
        summary_lines.append(f"Male source: {Path(male_path).name} ({len(male_pak.sprites)} sprites, offset {male_offset})")
        male_sprites = processor(male_pak, male_offset)
    else:
        summary_lines.append("Male: blanks (no source)")
        male_sprites = [make_blank_sprite() for _ in range(12)]

    # Female sprites (12-23)
    if female_path:
        female_pak = PAKFile.read(Path(female_path))
        summary_lines.append(f"Female source: {Path(female_path).name} ({len(female_pak.sprites)} sprites, offset {female_offset})")
        female_sprites = processor(female_pak, female_offset)
    else:
        summary_lines.append("Female: blanks (no source)")
        female_sprites = [make_blank_sprite() for _ in range(12)]

    # Count real vs blank
    male_real = sum(1 for s in male_sprites if len(s.rectangles) > 0)
    female_real = sum(1 for s in female_sprites if len(s.rectangles) > 0)
    summary_lines.append(f"Result: {male_real}/12 male + {female_real}/12 female real sprites")

    # Build output PAK
    out_pak = PAKFile()
    for s in male_sprites:
        out_pak.add_sprite(s)
    for s in female_sprites:
        out_pak.add_sprite(s)

    return out_pak, '\n'.join(summary_lines)


# ---------------------------------------------------------------------------
# GUI
# ---------------------------------------------------------------------------

def center_on_parent(dialog, parent, width, height):
    parent.update_idletasks()
    px = parent.winfo_rootx()
    py = parent.winfo_rooty()
    pw = parent.winfo_width()
    ph = parent.winfo_height()
    x = px + (pw - width) // 2
    y = py + (ph - height) // 2
    dialog.geometry(f'{width}x{height}+{x}+{y}')


class PakGeneratorApp:
    def __init__(self, root):
        self.root = root
        self.root.title('PAK Generator — Old-Style to Per-Item')
        self.root.configure(bg=BG_DARK)
        self.root.resizable(False, False)

        # Center on screen
        w, h = 560, 420
        sx = (self.root.winfo_screenwidth() - w) // 2
        sy = (self.root.winfo_screenheight() - h) // 2
        self.root.geometry(f'{w}x{h}+{sx}+{sy}')

        self._build_ui()

    def _build_ui(self):
        pad = dict(padx=10, pady=5)
        row = 0

        # --- Male PAK ---
        self._male_enabled = tk.BooleanVar(value=True)
        tk.Checkbutton(self.root, text='Male', variable=self._male_enabled,
                       bg=BG_DARK, fg=BLUE, selectcolor=BG_LIGHT,
                       activebackground=BG_DARK, font=('Consolas', 10, 'bold'),
                       command=self._toggle_male).grid(row=row, column=0, sticky=tk.W, **pad)
        self._male_path = tk.StringVar()
        self._male_entry = tk.Entry(self.root, textvariable=self._male_path,
                                    font=('Consolas', 9), width=38, bg=BG_LIGHT,
                                    fg=FG_TEXT, insertbackground=FG_TEXT)
        self._male_entry.grid(row=row, column=1, sticky=tk.EW, **pad)
        self._male_browse = tk.Button(self.root, text='Browse...', font=('Consolas', 9),
                                      command=lambda: self._browse('male'))
        self._male_browse.grid(row=row, column=2, **pad)
        row += 1

        # Male offset
        tk.Label(self.root, text='Offset:', bg=BG_DARK, fg=FG_DIM,
                 font=('Consolas', 9)).grid(row=row, column=0, sticky=tk.E, padx=(10, 2))
        self._male_offset = tk.StringVar(value='0')
        tk.Entry(self.root, textvariable=self._male_offset, font=('Consolas', 9),
                 width=6, bg=BG_LIGHT, fg=FG_TEXT, insertbackground=FG_TEXT
                 ).grid(row=row, column=1, sticky=tk.W, padx=10)
        tk.Label(self.root, text='(sprite index start for batch PAKs)',
                 bg=BG_DARK, fg=FG_DIM, font=('Consolas', 8)
                 ).grid(row=row, column=1, columnspan=2, sticky=tk.E, padx=10)
        row += 1

        # --- Female PAK ---
        self._female_enabled = tk.BooleanVar(value=True)
        tk.Checkbutton(self.root, text='Female', variable=self._female_enabled,
                       bg=BG_DARK, fg=ORANGE, selectcolor=BG_LIGHT,
                       activebackground=BG_DARK, font=('Consolas', 10, 'bold'),
                       command=self._toggle_female).grid(row=row, column=0, sticky=tk.W, **pad)
        self._female_path = tk.StringVar()
        self._female_entry = tk.Entry(self.root, textvariable=self._female_path,
                                      font=('Consolas', 9), width=38, bg=BG_LIGHT,
                                      fg=FG_TEXT, insertbackground=FG_TEXT)
        self._female_entry.grid(row=row, column=1, sticky=tk.EW, **pad)
        self._female_browse = tk.Button(self.root, text='Browse...', font=('Consolas', 9),
                                        command=lambda: self._browse('female'))
        self._female_browse.grid(row=row, column=2, **pad)
        row += 1

        # Female offset
        tk.Label(self.root, text='Offset:', bg=BG_DARK, fg=FG_DIM,
                 font=('Consolas', 9)).grid(row=row, column=0, sticky=tk.E, padx=(10, 2))
        self._female_offset = tk.StringVar(value='0')
        tk.Entry(self.root, textvariable=self._female_offset, font=('Consolas', 9),
                 width=6, bg=BG_LIGHT, fg=FG_TEXT, insertbackground=FG_TEXT
                 ).grid(row=row, column=1, sticky=tk.W, padx=10)
        row += 1

        ttk.Separator(self.root, orient=tk.HORIZONTAL).grid(
            row=row, column=0, columnspan=3, sticky=tk.EW, padx=10, pady=8)
        row += 1

        # --- Equip Type ---
        tk.Label(self.root, text='Equip Type:', bg=BG_DARK, fg=FG_TEXT,
                 font=('Consolas', 10)).grid(row=row, column=0, sticky=tk.W, **pad)
        self._equip_type = tk.StringVar(value='armor')
        ttk.Combobox(self.root, textvariable=self._equip_type, values=EQUIP_TYPES,
                     state='readonly', font=('Consolas', 10), width=20
                     ).grid(row=row, column=1, sticky=tk.W, **pad)
        row += 1

        # --- Output Filename ---
        tk.Label(self.root, text='Output Name:', bg=BG_DARK, fg=FG_TEXT,
                 font=('Consolas', 10)).grid(row=row, column=0, sticky=tk.W, **pad)
        self._output_name = tk.StringVar(value='new_item.pak')
        tk.Entry(self.root, textvariable=self._output_name, font=('Consolas', 10),
                 width=30, bg=BG_LIGHT, fg=FG_TEXT, insertbackground=FG_TEXT
                 ).grid(row=row, column=1, sticky=tk.EW, **pad)
        row += 1

        # --- Output Directory ---
        tk.Label(self.root, text='Output Dir:', bg=BG_DARK, fg=FG_TEXT,
                 font=('Consolas', 10)).grid(row=row, column=0, sticky=tk.W, **pad)
        self._output_dir = tk.StringVar(value=str(DEFAULT_ITEMS_DIR))
        tk.Entry(self.root, textvariable=self._output_dir, font=('Consolas', 9),
                 width=38, bg=BG_LIGHT, fg=FG_TEXT, insertbackground=FG_TEXT
                 ).grid(row=row, column=1, sticky=tk.EW, **pad)
        tk.Button(self.root, text='Browse...', font=('Consolas', 9),
                  command=self._browse_output_dir).grid(row=row, column=2, **pad)
        row += 1

        ttk.Separator(self.root, orient=tk.HORIZONTAL).grid(
            row=row, column=0, columnspan=3, sticky=tk.EW, padx=10, pady=8)
        row += 1

        # --- Buttons ---
        bf = tk.Frame(self.root, bg=BG_DARK)
        bf.grid(row=row, column=0, columnspan=3, pady=8)
        tk.Button(bf, text='Generate', font=('Consolas', 11, 'bold'),
                  fg='white', bg=GREEN, activebackground='#3aa63a',
                  width=14, command=self._on_generate).pack(side=tk.LEFT, padx=8)
        tk.Button(bf, text='Close', font=('Consolas', 11),
                  width=10, command=self.root.destroy).pack(side=tk.LEFT, padx=8)
        row += 1

        # --- Status ---
        self._status = tk.StringVar(value='Select source PAK file(s) and configure output.')
        tk.Label(self.root, textvariable=self._status, font=('Consolas', 9),
                 bg=BG_LIGHT, fg=FG_TEXT, anchor=tk.W, padx=6, pady=2
                 ).grid(row=row, column=0, columnspan=3, sticky=tk.EW)

        self.root.columnconfigure(1, weight=1)

    def _toggle_male(self):
        state = tk.NORMAL if self._male_enabled.get() else tk.DISABLED
        self._male_entry.config(state=state)
        self._male_browse.config(state=state)

    def _toggle_female(self):
        state = tk.NORMAL if self._female_enabled.get() else tk.DISABLED
        self._female_entry.config(state=state)
        self._female_browse.config(state=state)

    def _browse(self, gender):
        path = filedialog.askopenfilename(
            title=f'Select {gender} PAK file',
            filetypes=[('PAK files', '*.pak'), ('All files', '*.*')],
            parent=self.root)
        if path:
            if gender == 'male':
                self._male_path.set(path)
            else:
                self._female_path.set(path)
            # Auto-fill output name from first file selected
            if self._output_name.get() == 'new_item.pak':
                stem = Path(path).stem
                # Strip m/w prefix
                for prefix in ('m', 'w', 'nm', 'nw'):
                    if stem.startswith(prefix) and len(stem) > len(prefix):
                        stem = stem[len(prefix):]
                        break
                self._output_name.set(f'{stem}.pak')

    def _browse_output_dir(self):
        path = filedialog.askdirectory(title='Select output directory', parent=self.root)
        if path:
            self._output_dir.set(path)

    def _on_generate(self):
        # Validate
        has_male = self._male_enabled.get() and self._male_path.get().strip()
        has_female = self._female_enabled.get() and self._female_path.get().strip()

        if not has_male and not has_female:
            messagebox.showwarning('No Source', 'Enable and select at least one PAK file.',
                                   parent=self.root)
            return

        output_name = self._output_name.get().strip()
        if not output_name:
            messagebox.showwarning('No Name', 'Enter an output filename.', parent=self.root)
            return
        if not output_name.endswith('.pak'):
            output_name += '.pak'

        output_dir = Path(self._output_dir.get().strip())
        if not output_dir.exists():
            messagebox.showwarning('Bad Dir', f'Output directory does not exist:\n{output_dir}',
                                   parent=self.root)
            return

        output_path = output_dir / output_name
        if output_path.exists():
            if not messagebox.askyesno('Overwrite',
                                       f'{output_name} already exists.\nOverwrite?',
                                       parent=self.root):
                return

        equip_type = self._equip_type.get()
        male_path = self._male_path.get().strip() if has_male else None
        female_path = self._female_path.get().strip() if has_female else None

        try:
            male_offset = int(self._male_offset.get().strip() or '0')
        except ValueError:
            male_offset = 0
        try:
            female_offset = int(self._female_offset.get().strip() or '0')
        except ValueError:
            female_offset = 0

        # Validate source files exist
        if male_path and not Path(male_path).exists():
            messagebox.showerror('File Not Found', f'Male PAK not found:\n{male_path}',
                                 parent=self.root)
            return
        if female_path and not Path(female_path).exists():
            messagebox.showerror('File Not Found', f'Female PAK not found:\n{female_path}',
                                 parent=self.root)
            return

        self._status.set('Generating...')
        self.root.update()

        try:
            out_pak, summary = generate_pak(male_path, female_path, equip_type,
                                            male_offset, female_offset)
            out_pak.write(output_path)

            self._status.set(f'Written: {output_path.name} ({len(out_pak.sprites)} sprites)')
            messagebox.showinfo('Success',
                                f'Generated {output_name}\n\n{summary}\n\n'
                                f'Saved to:\n{output_path}',
                                parent=self.root)
        except Exception as e:
            self._status.set(f'Error: {e}')
            messagebox.showerror('Error', f'Generation failed:\n\n{e}', parent=self.root)


def main():
    root = tk.Tk()
    PakGeneratorApp(root)
    root.mainloop()


if __name__ == '__main__':
    main()
