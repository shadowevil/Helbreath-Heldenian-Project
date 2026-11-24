---
name: parchment-map
description: Make a Helbreath "parchment" map (the 300x300 RGBA picture the Map item opens, interface/newmaps style) from a map's .amd file, including a generated tile map with no other art. Use when asked to make, rebuild or preview a parchment / world map / newmaps frame for a map, or to pass a generated tile map in and get a parchment map out. No text labels are drawn.
---

# Parchment map

Tool: `tools/parchment_map/make_parchment.py` (product tooling; numpy + Pillow; console text is ASCII apart
from echoed file paths). Data it ships with: `tools/parchment_map/data/` (tile colour palette, paper patches,
torn-edge alphas, rim profile, icon cut-outs). `build_assets.py` regenerates that data; a normal run does not need it.

## Command

```
python tools/parchment_map/make_parchment.py <path/to/map.amd> -o <out.png> --mapinfo <map name> -v
```

Exit 0 on success. Exit 2 with one `error: ...` line on stderr for a missing, truncated or non-.amd input
(MAPSIZEX/MAPSIZEY must be 1..4096) or a reference that is not a readable 300x300 image.

Options:
- `--mapinfo <value>` - database rows for exits, doors and monster areas. If a directory exists at that
  path, rows for the .amd's file name are read from its `*.data.sql` files. Otherwise the value is a map
  name, and the rows for that name (not the .amd's file name) are read from `sources/database/mapinfo/`.
  Omit it for a map with no rows yet.
- `--minimap <dir>` - minimap art (`01.jpg`/`02.jpg`/`03.jpg`). Optional; it only changes which
  neighbouring tile sheets count as one terrain, and on the beginner zone it scores the same as without.
  A missing folder or missing images prints one `note:` line and falls back to the shipped palette.
- `--seed N` - paper, torn edge and ink mottling; the same seed gives a byte-identical file (default 1).
- `--reference <png>` - a 300x300 original; the ink NCC against it is printed, with or without `--sheet`.
- `--sheet <png>` - review sheet at 2x (its folder is created): with `--reference`, original | generated |
  ink overlay (red original, green generated, yellow both); without, the generated map alone.
- `--no-icons`, `-v` (list every icon and the reason for it).

Outputs go under the session's `.claude/scratch/`; packing into `interface/newmaps.pak` is not part of this tool.

## What it draws, and from what

| Element | Rule | Source |
|---|---|---|
| Framing | Whole map in a 245 px square, centred (fitted on 7 originals). If the main walkable area (the walkable region holding the most teleports) is under half the map, that area x1.4 is framed instead (interiors, arenas). | .amd |
| Strokes | Outline of the walkable land, 2 tiles into the blocked side; plus borders between two blocked terrains (e.g. wall top vs roof), inside the smaller one, so walls show as double lines. Walkable-walkable borders are left bare. Strokes are at least about 2.2 px wide, and drawn on sub-tiles when zoomed in so they stay about 3 px. | .amd blocked flags + tile sprite ids; colour from the shipped palette (or minimap) only to merge look-alike sheets |
| Hatching | Embossed dash texture over blocked rock/cliff sheets near walkable land (`HATCH_SHEETS`: 28, 29, 14, 340, 25, 18, 26). | .amd tile sprite ids |
| Exits | Teleport cluster near the edge of the land -> red arrow pointing outward. | .amd teleport flag (0x40) |
| Portals | Teleport to an outdoor map away from the edge -> portal glyph, blue for Elvine destinations, red otherwise. | DB `map_teleport_locations` + `maps.location_name` |
| Buildings | Teleport into a shop / blacksmith / warehouse / city hall / guild hall / church / barrack / magic tower / jail -> matching building icon, bottom at the door. | DB `maps.display_name`, else destination name prefix |
| Dungeon / entrance | Destination named Dungeon / Mine / Tower of Hell -> small diamond marker. | DB, else name pattern |
| Monster areas | `map_spot_mob_generators` rects (not 1-tile spots, guard posts with max_mobs <= 2, or whole-map generators), merged within 25 px, at most 6 -> white swords. | DB only |

Fallbacks with no DB rows: edge clusters -> arrows; clusters in open ground -> red portal; clusters hemmed in
by blocked tiles -> the neutral diamond (it may be a shop door or a stair; there is no way to tell). No swords.
Unknown tile sprite ids (not in the palette) still get outlines; they are never hatched.

## How to judge the output

1. Look at it at 2x next to an original (`--sheet` with `--reference`). Originals: `interface/newmaps.pak`
   frames: aresden, elvine (sheet 0), middleland, default = beginner zone (sheet 1), huntzone2, huntzone1
   (sheet 2), huntzone4, huntzone3 (sheet 3), arefarm, elvfarm (sheet 4).
2. Ink NCC against the original (printed whenever `--reference` is given). Measured with the shipped settings:
   default 0.45, huntzones 0.44-0.51, farms 0.26-0.34. aresden, elvine and middleland score about 0 although
   their icons land within about 6-15 px of the originals': those originals are dominated by the city road
   grid, a walkable-walkable border the stroke rule leaves bare (it must, or grass/dirt borders flood the
   hunting zones). Judge cities by eye, not by the score.
3. Check: no text anywhere; arrows on real exits and pointing out of the land; building icons on doors;
   strokes trace coasts, walls and cliffs, not grass-to-dirt borders; nothing inked past the margin.
4. `-v` lists each icon with its tile and destination; use it to explain a surprising icon.

## Limits

- No labels by design. Icons are cut from the originals, so only the icon kinds listed above exist.
- City road grids (paved roads on walkable ground) are not drawn, so city parchments are sparser than the
  originals in the town centre.
- Known look gaps: water is never hatched (the beginner-zone original leaves its sea bare; hunting-zone
  originals texture some lakes); dense mazes (toh2, toh3, dglv4, maze) are a busy field of strokes at 300 px;
  a walkable strip along a map edge draws as a straight line; interiors (no originals) show exits as portals.
- Portal colour needs the DB (`location_name`); without it every portal is red.
- The palette and the hatch table are keyed on the client's tile sprite ids; a generated map that uses new
  sprite sheets gets outlines but no hatching until `HATCH_SHEETS` and `build_assets.py palette` are updated.
