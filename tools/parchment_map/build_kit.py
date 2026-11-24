"""Export the runtime parchment-map kit the client composes when the Map dialog opens.

Usage:
  python tools/parchment_map/build_kit.py [--dry-run] [--out <dir>] [--work <dir>] [--maps a,b,...] [--jobs N]

Writes into --out (default staged/client/sprites/mapkit/), all names lowercase:
  paper.png              the HQ paper the tool uses (alpha fixed, edge tinted), 1200x1200
  icons.png              the user's icon atlas (working/sprite_work/map_icons.png), each frame copied
                         pixel for pixel onto a padded sheet so mipmaps do not bleed between frames, plus
                         the exit arrow baked in 8 rotations (arrow_0 ... arrow_315, clockwise from north)
  icons.json             frames by name: {image, x, y, w, h}
  creatures.png / .json  every killable creature (an NPC pak in the client's loading stage with the full
                         40-sprite action set, plus KILLABLE_SHORT_PAKS) standing, facing south-east, frame 0,
                         at native scale: {npc_type, npc_ids, npc_names, x, y, w, h, pivot_x, pivot_y}.
                         Monster areas in placements.json carry npc_ids, npc_types and creatures; nothing
                         draws them yet
  <map>/ink.png          the map's ink alone (make_parchment.py --ink-layer, --scale 4)
  <map>/placements.json  tile_to_px numbers and the icons to draw, already laid out: kit-pixel rects in
                         draw order (sized, merged, kept off the margin, overlaps dropped)
  index.json             the maps in the kit
Every map with an .amd in staged/shared/mapdata/ is included unless listed in EXCLUDE.

The client draws, in a square of the dialog: paper, ink, then each icon rect, then the player marker at
origin + (tile + 0.5) * px_per_tile, everything scaled by dialog size / 1200.

--dry-run renders nothing and writes nothing: it lists the maps and the files that would be written.
Exit status 0 on success, 1 when a map fails to render, 2 for a missing input.
Console text is ASCII apart from echoed file paths. Needs numpy and Pillow.
"""
import argparse
import contextlib
import io
import json
import re
import shutil
import subprocess
import sys
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

import numpy as np
from PIL import Image

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
sys.path.insert(0, str(HERE))
import make_parchment as mp  # noqa: E402

MAPDATA = REPO / "staged" / "shared" / "mapdata"
ATLAS_PNG = REPO / "working" / "sprite_work" / "map_icons.png"
ATLAS_JSON = REPO / "working" / "sprite_work" / "map_icons.json"
SCALE = 4
SIZE = mp.BASE * SCALE           # 1200
KIT_VERSION = 1

# maps left out of the kit, with the reason (see the report in --dry-run)
EXCLUDE = {}

# ---------------------------------------------------------------- icon layout (kit pixels, 1200 square)
BUILDING_FACTOR = 0.7            # one factor for every building, so their relative sizes stay as drawn
# drawn heights of the non-building icons (sword and portals: the old cut-outs' size x4)
EXIT_ARROW = "exit_arrow"        # the user's arrow, pointing north; baked as arrow_<deg>
ICON_HEIGHT = {"sword": 76, "portal_blue": 96, "portal_red": 96, "stairs_down": 64, "stairs_spiral": 84, EXIT_ARROW: 72}
KIND_ICON = {"entrance": "stairs_down", "stairs": "stairs_spiral"}   # kit art per kind, over the generator's old name
MARGIN = 14 * SCALE              # icons stay this far inside the square (as make_parchment --icons)
CLASH = 0.5                      # an icon mostly covering one already placed is dropped
MERGE_PX = 30 * SCALE            # doors of one building (or stair) closer than this share one icon
MERGE_KINDS = ("building", "stairs", "entrance", "portal")
# which icons win a clash: placed first; and draw order: drawn first is underneath
PRIORITY = {"exit": 0, "entrance": 1, "stairs": 2, "portal": 3, "building": 4, "monster_area": 5}
DRAW_ORDER = {"monster_area": 0, "building": 1, "portal": 2, "stairs": 3, "entrance": 4, "exit": 5}
SHEET_PAD = 32                   # transparent gutter around each frame on the kit sheets
SHEET_MAX_W = 2048
KIT_BUILDING = {"jail": "warehouse"}   # no jail art (and no door leads into a jail today)


def bleed(img, steps=24):
	"""Fill the RGB of fully transparent pixels with their nearest visible colour (alpha unchanged), so
	filtered and mipmapped downscaling shows no dark fringe around the art."""
	a = np.asarray(img.convert("RGBA"), np.float32).copy()
	known = a[:, :, 3] > 0
	if known.all() or not known.any():
		return img
	rgb = a[:, :, :3] * known[:, :, None]
	h, w = known.shape
	for _ in range(steps):
		acc = np.zeros_like(rgb)
		cnt = np.zeros(known.shape, np.float32)
		pk = np.pad(known, 1)
		pr = np.pad(rgb, ((1, 1), (1, 1), (0, 0)))
		for dy in (-1, 0, 1):
			for dx in (-1, 0, 1):
				if dx or dy:
					k = pk[1 + dy:1 + dy + h, 1 + dx:1 + dx + w]
					acc += pr[1 + dy:1 + dy + h, 1 + dx:1 + dx + w] * k[:, :, None]
					cnt += k
		new = ~known & (cnt > 0)
		if not new.any():
			break
		rgb[new] = acc[new] / cnt[new][:, None]
		known = known | new
	if not known.all():
		rgb[~known] = rgb[known].mean(0)
	a[:, :, :3] = np.where((a[:, :, 3] > 0)[:, :, None], a[:, :, :3], rgb)
	return Image.fromarray(np.clip(np.rint(a), 0, 255).astype(np.uint8), "RGBA")


def pack(frames, image_name, pad=SHEET_PAD):
	"""Shelf-pack the frames {name: RGBA image}, tallest first, with pad px gutters, in rows at most
	SHEET_MAX_W wide (well under any GPU's texture limit, so the client never downsamples the sheet).
	(sheet, {name: {image, x, y, w, h}})."""
	x, y, row_h, at = 0, 0, 0, {}
	for name, im in sorted(frames.items(), key=lambda kv: -kv[1].height):
		if x and x + im.width + 2 * pad > SHEET_MAX_W:
			x, y, row_h = 0, y + row_h, 0
		at[name] = (x + pad, y + pad)
		x += im.width + 2 * pad
		row_h = max(row_h, im.height + 2 * pad)
	width = max(at[n][0] + im.width + pad for n, im in frames.items())
	sheet = Image.new("RGBA", (width, y + row_h), (0, 0, 0, 0))
	rects = {}
	for name, im in frames.items():
		sheet.alpha_composite(im, at[name])
		rects[name] = {"image": image_name, "x": at[name][0], "y": at[name][1], "w": im.width, "h": im.height}
	return bleed(sheet), rects


def icon_sheets():
	"""(icons sheet, frames {name: rect}, building names, {name: draw factor}). The user's frames are
	copied pixel for pixel; the exit arrow (drawn pointing north) is also baked in 8 rotations, arrow_<deg>
	clockwise, since the renderer draws texture regions unrotated."""
	meta = json.loads(ATLAS_JSON.read_text(encoding="utf-8"))
	atlas = Image.open(ATLAS_PNG).convert("RGBA")
	user = {n: atlas.crop((r["x"], r["y"], r["x"] + r["w"], r["y"] + r["h"])) for n, r in meta["frames"].items()}
	buildings = sorted(set(user) & set(mp.OLD_BUILDING_ICON))   # the generator's building kinds
	for n in sorted(set(user) - set(buildings) - set(ICON_HEIGHT)):
		print(f"  note: atlas frame '{n}' is not used by the kit (not a known building or icon)")
	factors = {n: BUILDING_FACTOR for n in buildings}
	factors.update({n: h / user[n].height for n, h in ICON_HEIGHT.items() if n in user})
	frames = dict(user)
	if EXIT_ARROW in user:
		arrow = bleed(user[EXIT_ARROW])       # colour under the transparent pixels before resampling
		for deg in range(0, 360, 45):
			frames[f"arrow_{deg}"] = arrow.rotate(-deg, Image.BICUBIC, expand=True) if deg else arrow
			factors[f"arrow_{deg}"] = factors[EXIT_ARROW]   # one scale, so every direction is the same arrow
	icons, rects = pack(frames, "icons.png")
	return icons, rects, buildings, factors


def draw_size(name, frame, factors):
	"""Size an icon is drawn at on the 1200 kit square."""
	f = factors.get(name, 1.0)
	return frame["w"] * f, frame["h"] * f


def kit_icon(p):
	"""The kit frame name for a generator placement."""
	if p["kind"] == "building":
		return KIT_BUILDING.get(p["building"], p["building"])
	return KIND_ICON.get(p["kind"], p["icon"])


def merge_doors(placed):
	"""One icon per door: doors of the same kind and destination (a building's two doors, a stair's
	several teleport tiles) close together are averaged. Exits and monster areas are left as placed."""
	out = []
	for p in placed:
		if p["kind"] in MERGE_KINDS and p.get("dest"):
			for q in out:
				if q["kind"] == p["kind"] and q.get("dest") == p["dest"] and q.get("building") == p.get("building") \
						and np.hypot(q["x"] - p["x"], q["y"] - p["y"]) < MERGE_PX:
					n = q.get("_n", 1)
					q["x"], q["y"], q["_n"] = (q["x"] * n + p["x"]) / (n + 1), (q["y"] * n + p["y"]) / (n + 1), n + 1
					break
			else:
				out.append(dict(p))
		else:
			out.append(dict(p))
	return out


def layout(placed, frames, factors, configs=None, creatures=None):
	"""Icon rects in kit pixels, in draw order: sized, kept MARGIN inside the square, clashes dropped.
	Monster areas also list their generators' npc ids, npc types and creature atlas names."""
	boxes, kept = [], []
	for p in sorted(merge_doors(placed), key=lambda p: PRIORITY.get(p["kind"], 9)):
		name = kit_icon(p)
		if name not in frames:
			continue
		w, h = draw_size(name, frames[name], factors)
		left = p["x"] - w / 2
		top = p["y"] - h / 2 if p.get("anchor") == "center" else p["y"] - h
		left = min(max(left, MARGIN), SIZE - MARGIN - w)
		top = min(max(top, MARGIN), SIZE - MARGIN - h)
		box = (left, top, left + w, top + h)
		if any(max(0, min(b[2], box[2]) - max(b[0], box[0])) * max(0, min(b[3], box[3]) - max(b[1], box[1])) > CLASH * w * h for b in boxes):
			continue
		boxes.append(box)
		entry = {"icon": name, "x": round(left, 1), "y": round(top, 1), "w": round(w, 1), "h": round(h, 1), "kind": p["kind"]}
		for k in ("dest", "faction", "direction_deg"):
			if p.get(k) is not None:
				entry[k] = p[k]
		if p["kind"] == "monster_area" and creatures is not None:
			# what spawns there, so a kit can show the right creature (not drawn by default)
			ids = sorted({g["npc_config_id"] for g in p.get("generators", []) if g.get("npc_config_id") is not None})
			types = sorted({configs[i][1] for i in ids if i in configs})
			entry["npc_ids"] = ids
			entry["npc_types"] = types
			entry["creatures"] = [creatures[t] for t in types if t in creatures]
		kept.append(entry)
	kept.sort(key=lambda e: (DRAW_ORDER.get(e["kind"], 9), e["y"] + e["h"]))
	return kept


# ---------------------------------------------------------------- creatures
LOADING_CPP = REPO / "sources" / "client" / "screen_loading.cpp"
NPC_PAKS = REPO / "staged" / "client" / "sprites" / "npcs"
NPC_CONFIGS = REPO / "sources" / "database" / "gamedata" / "npc_configs.data.sql"
PAK_TO_PNG = REPO / ".claude" / "tools" / "scripts" / "pak_to_png.py"
NPC_TYPE_BASE = 10            # owner type = 10 + pak slot (equipment_indices.cpp CalcNpc: Mob + (type - 10) * 56)
FULL_ACTION_SET = 40          # 5 poses x 8 directions: stop, move, attack, damage, dying
# paks with fewer sprites whose NPCs are still fought and killed (npc_configs action_limit 0, or a
# destroyable crusade structure, 5); the rest of the short paks are townsfolk, quest givers and the gate
KILLABLE_SHORT_PAKS = {"wyvern", "fire-wyvern", "abaddon", "helbreath-tank", "crusade-barricade-turret", "apocalypse-cannon"}
STAND_SE_SPRITE = 0 * 8 + (4 - 1)   # pose 0 (stop, npc_renderer.cpp draw_stop) * 8 + direction 4 (SE) - 1
CREATURE_PAD = 4


def npc_paks():
	"""[(pak name, npc type, sprite count)] from the client's loading stage, in load order."""
	pat = re.compile(r'make_sprite\("npcs/([a-z0-9-]+)", Mob \+ 7 \* 8 \* (\d+), (\d+), true\)')
	return [(m.group(1), NPC_TYPE_BASE + int(m.group(2)), int(m.group(3))) for m in pat.finditer(LOADING_CPP.read_text(encoding="utf-8"))]


def npc_configs():
	"""{npc_id: (name, npc_type)} from the game database."""
	# sql_rows keeps SQL's doubled quote ('God''s Hand Knight'): undo it for names
	return {r["npc_id"]: (str(r["name"]).replace("''", "'"), r["npc_type"]) for r in mp.sql_rows(NPC_CONFIGS, "npc_configs")}


def creature_atlas(work, configs):
	"""(sheet, {name: entry}, excluded [(pak, reason)]): the standing, south-east frame 0 of every killable
	creature at native scale with its pivot. Extracts the paks with pak_to_png.py into work/npcs."""
	by_type = {}
	for nid, (_, t) in configs.items():
		by_type.setdefault(t, []).append(nid)
	chosen, excluded = [], []
	for pak, t, count in npc_paks():
		if count >= FULL_ACTION_SET or pak in KILLABLE_SHORT_PAKS:
			chosen.append((pak, t))
		else:
			excluded.append((pak, f"{count} sprites, not a killable creature"))
	out = Path(work) / "npcs"
	missing = [p for p, _ in chosen if not (out / p / "frames.json").exists()]
	if missing:
		subprocess.run([sys.executable, "-I", str(PAK_TO_PNG), str(NPC_PAKS), str(out), "--only", *missing],
		               check=True, stdout=subprocess.DEVNULL)
	frames = []
	for pak, t in chosen:
		meta = json.loads((out / pak / "frames.json").read_text(encoding="utf-8"))
		spr = meta["sprites"][STAND_SE_SPRITE]
		f = spr["frames"][0]
		im = Image.open(out / pak / spr["file"]).convert("RGBA").crop((f["x"], f["y"], f["x"] + f["w"], f["y"] + f["h"]))
		frames.append((pak.replace("-", "_"), t, im, f["pivot_x"], f["pivot_y"]))
	sheet, rects = pack({name: im for name, _, im, _, _ in frames}, "creatures.png", CREATURE_PAD)
	entries = {}
	for name, t, _, pvx, pvy in frames:
		r = rects[name]
		entries[name] = {"npc_type": t, "npc_ids": sorted(by_type.get(t, [])),
		                 "npc_names": sorted({configs[i][0] for i in by_type.get(t, [])}),
		                 "x": r["x"], "y": r["y"], "w": r["w"], "h": r["h"], "pivot_x": pvx, "pivot_y": pvy}
	return sheet, entries, excluded


def creature_for_type(entries):
	return {e["npc_type"]: n for n, e in entries.items()}


def render(job):
	"""Worker: render one map's ink layer and placements into the work dir. (map, error or None)."""
	name, work = job
	d = Path(work) / name
	argv = [str(MAPDATA / f"{name}.amd"), "-o", str(d / "composite.png"), "--scale", str(SCALE),
	        "--mapinfo", name, "--icons-json", str(d / "raw.json"), "--ink-layer", str(d / "ink.png")]
	buf = io.StringIO()
	with contextlib.redirect_stdout(buf), contextlib.redirect_stderr(buf):
		rc = mp.main(argv)
	return name, None if rc == 0 else buf.getvalue().strip()


def main(argv=None):
	ap = argparse.ArgumentParser(description="Export the runtime parchment-map kit.")
	ap.add_argument("--out", default=str(REPO / "staged" / "client" / "sprites" / "mapkit"))
	ap.add_argument("--work", default=str(REPO / "build" / "mapkit_work"), help="intermediate renders (composites, raw placements)")
	ap.add_argument("--maps", help="comma-separated subset of maps (default: every .amd)")
	ap.add_argument("--jobs", type=int, default=4)
	ap.add_argument("--dry-run", action="store_true")
	args = ap.parse_args(argv)
	for p in (MAPDATA, ATLAS_PNG, ATLAS_JSON, mp.DATA / mp.HQ_PAPER):
		if not p.exists():
			print(f"error: missing input {p}", file=sys.stderr)
			return 2
	names = sorted(p.stem.lower() for p in MAPDATA.glob("*.amd"))
	if args.maps:
		want = {n.strip().lower() for n in args.maps.split(",")}
		names = [n for n in names if n in want]
	maps = [n for n in names if n not in EXCLUDE]
	out, work = Path(args.out), Path(args.work)
	if args.dry_run:
		print(f"dry run: {len(maps)} maps -> {out}")
		print("  would write: paper.png, icons.png, icons.json, creatures.png, creatures.json, index.json")
		for n in maps:
			print(f"  would write: {n}/ink.png, {n}/placements.json")
		for n in names:
			if n in EXCLUDE:
				print(f"  excluded: {n} ({EXCLUDE[n]})")
		return 0

	work.mkdir(parents=True, exist_ok=True)
	failed = []
	with ProcessPoolExecutor(max_workers=max(1, args.jobs)) as pool:
		for name, err in pool.map(render, [(n, str(work)) for n in maps]):
			if err:
				failed.append(name)
				print(f"  FAILED {name}: {err}")
	maps = [n for n in maps if n not in failed]

	out.mkdir(parents=True, exist_ok=True)
	mp.configure(SCALE)
	rgb, alpha = mp.image_paper(mp.DATA / mp.HQ_PAPER, np.random.default_rng(1))
	paper = bleed(Image.fromarray(np.dstack([np.clip(np.rint(rgb), 0, 255).astype(np.uint8), alpha]), "RGBA"))
	paper.save(out / "paper.png", optimize=True)
	icons, frames, buildings, factors = icon_sheets()
	icons.save(out / "icons.png", optimize=True)
	(out / "icons.json").write_text(json.dumps({"frames": frames, "buildings": buildings,
	                                            "building_factor": BUILDING_FACTOR}, indent=1), encoding="utf-8")
	configs = npc_configs()
	cre_sheet, cre_entries, cre_excluded = creature_atlas(work, configs)
	cre_sheet.save(out / "creatures.png", optimize=True)
	(out / "creatures.json").write_text(json.dumps({
		"image": "creatures.png", "frame": "stop pose, facing south-east, frame 0, native scale",
		"pivot_note": "draw the frame's top-left at (anchor + pivot_x, anchor + pivot_y), as the client draws sprites",
		"creatures": cre_entries}, indent=1), encoding="utf-8")
	creatures = creature_for_type(cre_entries)
	for n in maps:
		raw = json.loads((work / n / "raw.json").read_text(encoding="utf-8"))
		(out / n).mkdir(exist_ok=True)
		shutil.copyfile(work / n / "ink.png", out / n / "ink.png")
		kit = {"map": n, "size": SIZE, "tiles": raw["tiles"], "px_per_tile": raw["px_per_tile"],
		       "origin_x": raw["origin_x"], "origin_y": raw["origin_y"],
		       "box": [round((SIZE - mp.BASE_BOX * SCALE) / 2, 1), round((SIZE + mp.BASE_BOX * SCALE) / 2, 1)],
		       "icons": layout(raw["placements"], frames, factors, configs, creatures)}
		(out / n / "placements.json").write_text(json.dumps(kit, indent=1), encoding="utf-8")
	listed = maps
	if args.maps:                 # a subset run keeps the maps already in the kit
		try:
			listed = sorted(set(json.loads((out / "index.json").read_text(encoding="utf-8"))["maps"]) | set(maps))
		except (OSError, ValueError, KeyError):
			pass
	(out / "index.json").write_text(json.dumps({"version": KIT_VERSION, "size": SIZE, "paper": "paper.png",
	                                            "icons": "icons.json", "maps": listed}, indent=1), encoding="utf-8")
	total = sum(p.stat().st_size for p in out.rglob("*") if p.is_file())
	print(f"kit: {len(maps)} maps, {len(cre_entries)} creatures ({cre_sheet.width}x{cre_sheet.height}) -> {out}, {total / 1048576:.1f} MiB"
	      + (f", {len(failed)} failed" if failed else ""))
	for pak, why in cre_excluded:
		print(f"  creature excluded: {pak} ({why})")
	for n in names:
		if n in EXCLUDE:
			print(f"  excluded: {n} ({EXCLUDE[n]})")
	return 1 if failed else 0


if __name__ == "__main__":
	sys.exit(main())
