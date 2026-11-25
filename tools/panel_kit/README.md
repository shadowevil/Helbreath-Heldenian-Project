# panel_kit

Builds the runtime dialog and button kit the client composes panels from (HH-0054, client task HH-0059).

- `build_panel_kit.py` slices the art sheets and writes `staged/client/sprites/panelkit/panel_kit.png` and `panel_kit.json`.
  The PNG is the atlas; the JSON holds the frames and the recipes.
- `magic_circle.py` generates the `magic_circle` frame (200x200 RGBA hexagram-in-ring bevel for the magic dialog, HH-0071;
  needs numpy and scipy). The json frame carries a `placement` (dialog `magic`, top-left 28,62 in the 258x328 dialog).
- `compose.py` is the reference composer: what the client's `panel_kit` class must draw. It reads only the staged kit.
- `text_fx.py` draws the multi-pass button text used by `compose.py`.
- `build_hud_kit.py` slices the HUD action-bar sheet (`working/sprite_work/GameDialog/gamedialog_sprite_6.png`) into
  `staged/client/sprites/hudkit/hud_kit.png` and `hud_kit.json` (parts, bar-relative layout, gauge offsets), for the
  client's `hud_kit` class (HH-0068). Same `--check` / `--dry-run` and exit codes as `build_panel_kit.py`.

The client specs are `.claude/design/plans/panel-kit-runtime.md` and `.claude/design/plans/hud-kit-runtime.md`.

## Inputs

The art sheets live in `working/sprite_work/`. That folder is gitignored and exists only on the author's machine:

- `panel_building.png`: corners, bevels, rings, flourishes, wood bars, backgrounds and small buttons.
- `button_panel.png`: the button frame, faces and caps.

Both tools refuse to write under `working/`. Change art there, then rebuild. Never hand-edit the staged kit.

## Commands

```
python tools/panel_kit/build_panel_kit.py            # rebuild staged/client/sprites/panelkit/
python tools/panel_kit/build_panel_kit.py --check    # rebuild in memory, compare with the staged kit; writes nothing
python tools/panel_kit/build_panel_kit.py --dry-run  # list pieces and files; writes nothing
python tools/panel_kit/compose.py                    # preview to build/panel_kit_preview/preview.png
```

**`--check` exit codes:**
- 0: identical bytes.
- 1: the kit differs. If only the PNG encoding changed but the decoded pixels and the JSON match, it says so.
- 2: missing or unreadable art sheets, or a build error.

**Outputs:**
- Deterministic: the same sheets always give the same bytes.
- Each file is written to `.tmp` and then renamed, PNG first and JSON last, so a failed build cannot leave a mismatched pair.

## Needs

Python 3 with Pillow (both tools), numpy (`compose.py`, `text_fx.py`, `magic_circle.py`) and scipy (`magic_circle.py`). Console output is ASCII.
