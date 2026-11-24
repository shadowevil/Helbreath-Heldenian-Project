---
name: parchment-map
description: Make a Helbreath "parchment" map (the picture the Map item opens, interface/newmaps style) from a map's .amd file, including a generated tile map with no other art, at the client's 300x300 or in high quality (1200x1200 with --scale 4). Use when asked to make, rebuild or preview a parchment / world map / newmaps frame for a map, or to pass a generated tile map in and get a parchment map out. No text labels and, by default, no icons are drawn; icon positions can be written as JSON for an icon kit. Also use to rebuild the client's runtime map kit (staged/client/sprites/mapkit) with build_kit.py.
---

# Parchment map

Tool: `tools/parchment_map/make_parchment.py` (product tooling; numpy + Pillow; console text is ASCII apart
from echoed file paths). Data it ships with: `tools/parchment_map/data/` (the high-quality paper
`paper_hq.png`, tile colour palette, old paper patches and torn-edge alphas, rim profile, old icon cut-outs).
`build_assets.py` regenerates the derived data; a normal run does not need it.

## Command

```
python tools/parchment_map/make_parchment.py <path/to/map.amd> -o <out.png> --scale 4 --mapinfo <map name> --icons-json <out.json> -v
```

Exit 0 on success. Exit 2 with one `error: ...` line on stderr for a missing, truncated or non-.amd input
(MAPSIZEX/MAPSIZEY must be 1..4096), `--scale` outside 1..8, a missing `--paper` image, or a reference that
is not a readable 300x300 image.

Options:
- `--scale N` - output is 300*N px square: 1 (default) is the size the client uses today, 4 = 1200x1200 is the
  project's high-quality art size. The ink is computed at the output resolution (not upscaled): the same
  strokes as scale 1, drawn as crisp, slightly rough dark-brown ink.
- `--paper hq|patches|<image>` - `hq` (default) is the user's high-quality parchment; `patches` is the old
  paper quilted from the originals; an image path uses that texture (RGBA with its own torn edge, or opaque,
  in which case one of the originals' edges is used).
- `--mapinfo <value>` - database rows for exits, doors and monster areas. If a directory exists at that
  path, rows for the .amd's file name are read from its `*.data.sql` files. Otherwise the value is a map
  name, and the rows for that name (not the .amd's file name) are read from `sources/database/mapinfo/`.
  Omit it for a map with no rows yet.
- `--icons-json <path>` - write the icon placements as JSON (see below). Nothing is drawn for them.
- `--icons` - also draw the old cut-out icons (off by default; LANCZOS-upscaled above scale 1, so soft).
- `--ink-layer <png>` - also write the ink alone as a straight-alpha RGBA PNG at the output size. Drawn
  source-over on the same paper it reproduces the written map (measured max 8 levels, mean 0.46, at scale 4);
  the hatching's emboss highlight is the part it cannot carry. This is what the runtime kit ships per map.
- `--minimap <dir>` - minimap art (`01.jpg`/`02.jpg`/`03.jpg`). Optional; it only changes which neighbouring
  tile sheets count as one terrain. A missing folder or images prints one `note:` line and uses the palette.
- `--seed N` - torn edge choice and ink mottling; the same seed gives a byte-identical file (default 1).
- `--reference <png>` - a 300x300 original; the ink NCC against it is printed (a scaled render is scored on
  the 300 px grid), with or without `--sheet`.
- `--sheet <png>` - review sheet (its folder is created): with `--reference`, original | generated | ink
  overlay (red original, green generated, yellow both); without, the generated map alone.
- `-v` lists every placement and the reason for it.

Outputs go under the session's `.claude/scratch/`; packing into `interface/newmaps.pak` is not part of this tool.

## Icon placements (for the icon kit)

Icons are not drawn: the user is making an icon kit to place on these maps. `--icons-json` writes
`{map, image, size, scale, tiles, px_per_tile, tile_to_px, origin_x, origin_y, placements: [...]}`
(tile centre px = origin + (tile + 0.5) * px_per_tile). Each placement has `kind`
(`exit`, `portal`, `building`, `stairs`, `entrance`, `monster_area`), `x`/`y` in pixels of the written PNG (so at
the chosen `--scale`), `anchor` (`center`, or `bottom` = the icon's bottom edge centred there), the old `icon`
name (the cut-out `--icons` draws), and where known: `dest` (destination map), `direction_deg` (exits, 0 = north,
clockwise), `faction` (portals), `building` (the true building: city_hall, cathedral, command_hall, guild_hall,
wizard_tower, shop, warehouse, blacksmith, barrack, jail), `tile`, and for monster areas the merged `generators`.

| Kind | Rule | Source |
|---|---|---|
| exit | Teleport cluster near the edge of the land; arrow pointing outward, pulled 6 px (x scale) inland. | .amd teleport flag (0x40) |
| portal | Teleport to an outdoor map away from the edge; `faction` elvine / aresden. | DB `map_teleport_locations` + `maps.location_name` |
| building | Teleport into a building; bottom at the door. By destination prefix: `cityhall_` city_hall, `cath_` cathedral, `cmdhall_` command_hall, `gldhall_` guild_hall, `wzdtwr_` wizard_tower, `gshop_` shop, `wrhus_`/`arewrhus`/`elvwrhus` warehouse, `bsmith_` blacksmith, `arebrk`/`elvbrk` barrack, `*jail` jail. | Destination name prefix, else DB `maps.display_name` words |
| stairs | Teleport from one barrack map to another (a floor change inside the barracks). | Source and destination both `arebrk*`/`elvbrk*` |
| entrance | Destination named Dungeon / Mine / Tower of Hell. | DB, else name pattern |
| monster_area | `map_spot_mob_generators` rects (not 1-tile spots, guard posts with max_mobs <= 2, or whole-map generators), merged within 25 px (x scale), at most 6. | DB only |

Without DB rows: edge clusters -> exit; clusters in open ground -> portal (no faction); clusters hemmed in by
blocked tiles -> entrance (it may be a shop door or a stair; there is no way to tell). No monster areas.

## What the map itself shows

| Element | Rule | Source |
|---|---|---|
| Framing | Whole map in a 245 px (x scale) square, centred (fitted on 7 originals). If the main walkable area (the walkable region holding the most teleports) is under half the map, that area x1.4 is framed instead (interiors, arenas). | .amd |
| Strokes | Outline of the walkable land, 2 tiles into the blocked side; plus borders between two blocked terrains (e.g. wall top vs roof), inside the smaller one, so walls show as double lines. Walkable-walkable borders are left bare. At least about 2.2 px (x scale) wide; drawn on sub-tiles when zoomed in. | .amd blocked flags + tile sprite ids; colour (palette or minimap) only merges look-alike sheets |
| Hatching | Embossed dash texture over blocked rock/cliff sheets near walkable land (`HATCH_SHEETS`: 28, 29, 14, 340, 25, 18, 26). | .amd tile sprite ids |

Unknown tile sprite ids (not in the palette) still get outlines; they are never hatched.

## How to judge the output

1. Look at it next to an original (`--sheet` with `--reference`); at scale 4 also look at a 1:1 crop.
   Originals: `interface/newmaps.pak` frames: aresden, elvine (sheet 0), middleland, default = beginner zone
   (sheet 1), huntzone2, huntzone1 (sheet 2), huntzone4, huntzone3 (sheet 3), arefarm, elvfarm (sheet 4).
2. Ink NCC against the original. Measured with the shipped settings at scale 1: default 0.45, huntzones
   0.44-0.51, farms 0.26-0.34 (scale 4 scores about the same: default 0.48, huntzone1 0.52). aresden, elvine
   and middleland score about 0 although their placements land within about 6-15 px of the originals' icons:
   those originals are dominated by the city road grid, a walkable-walkable border the stroke rule leaves bare
   (it must, or grass/dirt borders flood the hunting zones). Judge cities by eye, not by the score.
3. Check: no text or icons; strokes trace coasts, walls and cliffs, not grass-to-dirt borders; nothing inked
   past the margin; at scale 4, crisp stroke edges and no orange glow around the torn edge.
4. For placements, open the JSON beside the PNG; `-v` prints each with its tile and destination.

## Limits

- No labels and no icons by default; the icon kit is still to come, and only the five kinds above are placed.
- The HQ paper's translucent torn-edge fringe is orange-red in the source art; it is pulled 75% towards burnt
  brown (`EDGE_TAME`) so it does not glow over the game's dark background.
- City road grids (paved roads on walkable ground) are not drawn, so city parchments are sparser than the
  originals in the town centre.
- Known look gaps: water is never hatched; dense mazes (toh2, toh3, dglv4, maze) are a busy field of strokes;
  a walkable strip along a map edge draws as a straight line; interiors (no originals) mark exits as portals.
- The palette and the hatch table are keyed on the client's tile sprite ids; a generated map that uses new
  sprite sheets gets outlines but no hatching until `HATCH_SHEETS` and `build_assets.py palette` are updated.
- The client composes the runtime kit (below) in the Map dialog; the old 300x300 newmaps frames remain only for
  maps the kit lacks.

## Runtime kit

`python tools/parchment_map/build_kit.py [--dry-run] [--maps a,b] [--jobs N]` writes
`staged/client/sprites/mapkit/`: `paper.png` (1200), `<map>/ink.png` (`--ink-layer` at `--scale 4`),
`<map>/placements.json` (tile_to_px numbers and icons already laid out as kit-pixel rects in draw order),
`icons.png` + `icons.json` (the user's atlas `working/sprite_work/map_icons.png`, frames copied onto a padded
sheet, plus its north-pointing `exit_arrow` baked in 8 rotations `arrow_<deg>`), `creatures.png` + `creatures.json`
(every killable creature standing, facing south-east, native scale, with pivot and npc ids; monster areas in
placements.json list `npc_ids`/`npc_types`/`creatures`, not drawn) and `index.json`. Intermediates go to
`build/mapkit_work/`. Icon sizes (building factor, other heights) and layout rules are constants at the top of
build_kit.py. Re-run it after editing the atlas, the paper, a map or the database, then deploy. `working/` is
gitignored, so the atlas exists only on the user's machine; without it, keep the committed `icons.png`/`icons.json`.
