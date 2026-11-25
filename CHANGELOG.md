# Runtime minimap, graphics menu fixes, new versioning, build layout and database generator

## 2026-03-04

### Added — Runtime Minimap Renderer
- New `minimap_renderer` class generates minimap textures dynamically from `m_tile[]` data at runtime, replacing static pre-rendered sprites from `telescope.pak`.
- LRU cache (3 slots) — re-entering a map reuses cached texture without regeneration.
- 4 discrete zoom levels (+/- keys): overview, 2/3, 1/3, 1/5 of full map.
- Player-following viewport with edge clamping.
- Entity marker system (blinking red dot for monster events, white dot for player).
- Compile-time switch `#define MINIMAP_DIALOG` in `GlobalDef.h` — original `DialogBox_GuideMap` untouched.

### Added — Shared Render Interface Extensions
- `ITexture`: render-target operations (`begin_render`, `end_render`, `set_view`, `reset_view`, `get_render_surface`, `clear`).
- `IRenderer`: `draw_texture_region` (texture blitting) and `draw_circle_filled` (marker primitive).
- SFMLEngine implementations using `sf::RenderTexture`, `sf::View`, `sf::CircleShape`.

### Changed
- `ConfigManager`: added `minimap_zoom_level` (persisted to `settings.json`).
- `Screen_OnGame`: owns `minimap_renderer` instance, generates on map load.
- `DialogBoxManager`: `#ifdef MINIMAP_DIALOG` switches between old/new minimap dialog registration.
- Zoom hotkeys (+/-): under `#ifdef MINIMAP_DIALOG`, cycle through 4 zoom levels via `ConfigManager`.

Fixed the graphics menu scrollbar so clicking it matches what is drawn, and brought the new smoothing toggles up to standard. Made image downscaling and texture loading safer, and the build is now warning-free. The backup, search and build tools work in this checkout again, including with newer Visual Studio, and the minimap helper scripts are fixed. The Claude docs moved into the project's docs folder.

Adopted the new versioning foundation: separate client, server and database release numbers, a Build ID derived from the source state instead of a hand-bumped counter, and a signed build proof so release servers only admit matching release clients. Test and production builds now refuse to build from uncommitted source, and the database tables are now kept as plain SQL files in the source tree.

Login now checks the protocol version, and release servers also check the Build ID and build proof, with a one-time clock-skew retry for players whose clock is off. Refused clients see the version-mismatch screen instead of hanging, and the game port only admits a player who has just passed the versioned enter-game step from the same address with the same password.

Build output moved out of the source folder into separate build and bin folders, the solution moved to the repository root, and a new deploy step assembles the playable game folder from the built files and the staged static files without touching accounts, logs or settings. The old hand-bumped version system was removed.

The game and map databases can now be generated from the SQL source folder; bans, admins and command permissions are carried over on every rebuild, and a database edited outside the generator is never silently overwritten.

Screen transitions are now 50% faster: every screen fade runs at 1.5x speed, set by one `transition_speed` constant in `game_mode_manager.h`.

The splash screen now also shows in Debug builds, and a left click skips it once it has been up for one second.

The client now ships one font, Wine's Tahoma replacement (LGPL 2.1, licence beside it in `fonts/`), and never uses a system font. The unused Arial, Segoe UI and Microsoft Tahoma copies are gone, and the client refuses to start with "Unable to load default font" if `fonts/wine_tahoma.ttf` is missing.

The world and the interface now draw separately: the game world still renders at 800x600 and is scaled to the window, while menus, dialogs, the HUD, fades, the cursor and the performance monitor draw directly at the window's resolution. Full-screen backgrounds follow the smooth-upscaling setting; other interface art stays sharp. Screenshots capture the full window. (First step of the world/screen split; text and floating names follow in later steps.)

Interface text is now drawn at the window's real resolution, so menus, dialogs and the HUD stay sharp at large window sizes; layout and line wrapping are unchanged. Names, chat bubbles and damage numbers over characters still use the old path until the next step.

New high-quality cursors: every cursor (pointer, sword, magic circles, gauntlets, hourglass, target brackets) is redrawn from new artwork stored at four times its in-game size, so it stays sharp up to 4K while keeping its old size and click point. If the new art is missing, the old cursor is used.

Names, chat bubbles and damage numbers over characters are now drawn after the world in screen space: they are sharp at any window size and always appear above spell effects and weather. The ground-item name stays under dialogs and the HUD. The HUD action bar is prepared to span the full world area when the world and interface areas differ.

In stretch mode on a window that is not 4:3, the world and the HUD action bar now fill the window while menus and dialogs keep a uniform, centred 4:3 scale. Mouse clicks are mapped separately for the interface, the world and the action bar, so clicks land where they point in each. Names and chat bubbles follow the stretched world. Letterbox mode and 4:3 windows (800x600, 1440x1080) behave exactly as before.

In that same widescreen stretch mode, the dimmed backgrounds behind pop-ups, the chat background, the top message bar and the performance monitor bar now reach both edges of the window instead of stopping at the centred 4:3 area.

Linux Debug builds of the client now behave like Windows Debug builds: they connect to a local server (127.0.0.1) instead of the public test server, skip the auto-update check, and enable the debug-only tools (tile grid, spell target overlay, frame profiling). Release builds are unchanged.

Groundwork for an agent bridge in Debug builds (HH-0035). Debug clients accept `--agent-bridge-port=N` (8891-8898, default 8891) and `--no-agent-bridge`. Linux now passes the whole command line to the game. None of this is compiled into Release.

The Debug client now runs the agent bridge (HH-0037). It listens on 127.0.0.1 only and answers `ping`, `get_state` and `screenshot`, which saves a PNG of the window into `build/screenshots/` under a 20 MB folder cap. Malformed or hostile input is refused without stalling the game.

The Debug agent bridge can now drive the client (HH-0038): mouse moves, clicks, wheel, keys, typing, setting text, held modifiers, waiting for a state, and a coordinate probe. Input goes through the same path as the real mouse and keyboard, one presented frame per step, and it is held during screen fades. Text typed into password fields never appears in replies or logs. The window-to-interface mapping now has one shared, exact implementation for both directions.

An MCP server for the Debug client is ported from Helbreath-SDL into `.claude/tools/agent_mcp/` and registered in `.mcp.json` (HH-0039). An agent can attach to a running Debug client to read its state, take screenshots and send input. Tools the client does not support yet stay hidden, and scanning for clients never takes over a client another session is driving.

The Debug agent bridge can list and click named controls (HH-0040): every interactive control on the 11 menu screens and overlays has a stable name (`login.ok`, `main_menu.login`, ...). Dialogs and the HUD are listed as windows, and clicks report what consumed them. Password fields report only that they are masked.

The Debug agent bridge keeps an event journal (HH-0041) covering:
- screen changes, dialogs and overlays opening and closing, and clicks;
- map changes and entering the game;
- packet ids and sizes, never their contents;
- chat lines, and client warnings and errors.

An agent can read it, follow it, or wait for a specific event. The MCP server's journal tools follow it.

The Debug agent bridge can act in the game world (HH-0042):
- list nearby entities;
- map a tile to the screen;
- walk to a tile (it refuses when a real click would, and stops on timeout);
- close windows (the HUD is protected);
- change the volume for the session without saving it.

In-world `get_state` and `probe_point` report HP, MP, level, target, tile and object. Every bridge command now rejects unknown arguments.

Fixed: in any text field, pressing Ctrl+A on an empty field and then typing no longer loses the first character (HH-0044). Character and account names typed that way used to lose their first letter.

The corner minimap now uses Helbreath-SDL's detailed minimap art for the 47 outdoor maps, in three zoom levels: the whole map, a mid view, and a close view that follows the player (HH-0045).
- Step between them with + and - or the mouse wheel over the minimap; the level is remembered.
- The Map Zoom option in the system menu switches between the whole map and the mid view.
- Building names on hover, double-click to walk, the monster alert and the transparency option all still work.
- Maps without art, such as building interiors, show no minimap, and clicks in that corner reach the world.
- The images ship as high-quality JPGs (29 MB), and the old telescope minimap art is no longer loaded.

Fixed: using the Map item crashed the game server (HH-0046). The Map now works as in the original game:
- double-clicking it opens the map at once, with no need for free hands or a target click;
- elsewhere it says there is no map for the area;
- it can be used again straight away.

A tool without a linked skill can no longer reach the skill code on the server.

New tool `tools/parchment_map/make_parchment.py` draws a parchment map in the style of the original Map item maps for any map file, with no text. It works on newly generated maps too (HH-0047). The project skill `parchment-map` explains how to use it. The game does not use the generated maps yet.

The Map item now shows a high-quality parchment map, built in the game from a map kit, for all 79 maps, interiors included (HH-0048).
- **What it shows:** the user's parchment paper, the map's ink, and icons for buildings, monster areas, portals, dungeon entrances, barrack stairs and exits.
- **Where it works:** it also opens on maps the server has no map for.
- **Maps without a kit** still show the old map picture.
- **New tool:** `tools/parchment_map/build_kit.py` rebuilds the kit.
- **Creatures:** each monster area shows up to three of the creatures that spawn there, standing and facing south-east, in place of the sword (HH-0050). The kit has 48 creatures; 22 structures, guards and summons were left out at the user's request (HH-0049). Areas with no creature art, such as the barracks training dummies, keep the sword.
- **Layout:** the stairs icons are a third smaller, and map icons no longer overlap (HH-0051). An icon that would touch another is moved to the nearest free spot instead of being dropped. Across the 79 maps 17 icons moved and none were dropped; the old layout dropped 3.

The corner minimap now has two zoom levels, the whole map and the mid view. The close view was dropped because the parchment map takes its place (d-033).

The client now ships Liberation Serif (regular, bold, italic, bold italic) in `fonts/`, the open-licence counterpart to Times New Roman with identical letter widths, beside Wine Tahoma (HH-0053). No screen uses it yet.

Every sprite `.pak` (361 files) is now also extracted beside itself into a folder of the same name, holding `manifest.json` (every header field, sprite, frame and pivot) and the sprite images exactly as stored (HH-0052). `sprites/index.json` lists every asset by the name the client loads. New tool `.claude/tools/scripts/pak_extract.py` rebuilds each pak from its folder and byte-compares it (361 of 361 identical); it never overwrites an existing folder. The client still loads the paks until the folder loader lands (HH-0056).

The game and its tools no longer use `.pak` files (HH-0055).
- **Client (HH-0056):** sprites load from the sprite folders through `sprites/index.json` and each folder's `manifest.json`. There are no directory scans, and textures still load on first draw. An image that can't be read logs one warning and is not retried. A missing or broken `index.json` logs one warning at startup. Unsafe paths in the data are refused. `PAK.h` and every pak name in the code are gone. In the item sprite metadata, `pak_file` is now `sprite_folder`, and `pak_index_start`/`pak_index_end` are now `sprite_index_start`/`sprite_index_end`. The sprite-font test screen now loads its fonts from the right folder.
- **Startup:** loading the manifests makes the loading screen slower in Debug, about 4.3 s against 0.64 s with paks. This is accepted until the new asset system lands.
- **Tools (HH-0057):**
  - **Folders:** the map kit builder, the map renderer, the asset builder and the display_id_manager tools read the folders through one shared reader, `tools/sprite_folder/sprite_folder.py`. The map kit still rebuilds byte-for-byte.
  - **Renamed:** the item and minimap generators now write folders, as `sprite_generator.py` and `build_minimap_sprites.py`. When they rewrite a folder they list leftover image files and never delete them.
  - **Removed:** the pak-only tools, both `paklib.py` copies, `pak_to_png.py`, `pak_extract.py` and `fetch_korea_2024.py`.
- **Files (HH-0058):** all 361 `.pak` files are removed, each only after its folder rebuilt it byte-for-byte. Deploy removes the deployed copies. The unused prebuilt libraries in `sources/dependencies/client/libs/` are removed too.

New dialog and button kit (HH-0054). It is built from the user's `panel_building.png` and `button_panel.png` sheets.
- **Kit:** `staged/client/sprites/panelkit/` holds one atlas (83 pieces) and a json with the recipes. It covers every background, border, corner, bevel and bevel circle, plus flourishes, the scrollbar and the buttons. The json also holds the text effects as draw lists, including the wide-button labels: Liberation Serif 14 px with a darker bold layer underneath and an 80% shadow.
- **Tool:** `tools/panel_kit/build_panel_kit.py` rebuilds the kit, and `--check` confirms it is unchanged. `compose.py` previews panels.
- **Plans:** the client plans are `.claude/design/plans/panel-kit-runtime.md` and `hud-kit-runtime.md` (HH-0059 to HH-0068).
- **In the game:** nothing draws with the kit yet.

The game's text buttons now draw from the new dialog and button kit (HH-0060 to HH-0062). This covers the menu screens, the overlays and the dialogs, in normal and yellow hover states, and clicks behave as before.
- **Renderer:** it can load crisp textures, tint and additively blend texture draws, and mirror pieces. Text can be drawn in extra fonts at exact pixel sizes, with letter spacing.
- **Kit classes:** the new `panel_kit` classes in the client give preset button layouts, where the caller supplies only text and position, plus custom buttons. They also give preset or custom dialog frames that each dialog fills with its own layout.
- **CControls:** the kit only draws, and CControls keeps all behaviour through its existing render hook.
- **Kit text:** it is always smoothed. The Antialias Font option still controls all other text.
- **Missing kit:** the client falls back to the old button sprites.

The character dialog now draws from the kit (HH-0063). It has the red title, both headings with flourishes centred on the text, the label and value fields, the slot rings and the kit buttons, all at their original positions. Clicks behave as before. Kit headings no longer stack their shadow twice, so they read sharper. The kit text styles accept per-style smoothing settings, though none needed changing. With no kit, the dialog uses the old sprite.

The chat history dialog now draws from the kit (HH-0064). It has the "History" title with flourishes, an inset text area and the kit scrollbar. The scrollbar's click and drag area matches the drawn groove, so the thumb stays under the cursor. Dragging the thumb and both dialogs is checked in game. The dialogs share one transparency helper.

The system menu now draws from the kit (HH-0065). That covers the frame, title, value boxes, slider grooves and thumbs, and the graphics scrollbar, whose click area matches what is drawn.
- **Titles:** dialog titles sit at the original heights again (History was 4 px high).
- **Flourishes:** they default to black like the original, and a dialog can override their colour and blend. The character heading keeps its warm look.
- **Text shadows:** they now land the same distance from the text on every line and every letter. The offset is applied in whole screen pixels after the text position is rounded, separately for each axis. Before, chat history lines got a 1 px or 2 px shadow depending on where they fell at the 1.6x interface scale.

The HUD action bar now draws from the new art (HH-0068).
- **Gauges:** each one draws its trough, then the fill cut straight at the value, then the glass caps on top. HP, MP, SP and EXP empty toward the left, and hunger empties from the top.
- **Other parts:** the orb, its socket and the peace-mode hover come from the new art. The menu buttons have normal and hover states.
- **Behaviour:** clicks, tooltips and values are unchanged, and the old sprites are used if the kit is missing.
- **Tool:** `tools/panel_kit/build_hud_kit.py` builds `staged/client/sprites/hudkit/`.

Six small message boxes now draw their frame from the kit (HH-0066, batch 1): item drop, drop amount, resurrect, confirm exchange, fishing and the NPC action prompt.
- **Look:** a cloth fill, light wood border and gold corners, the closest match to the old backgrounds. The old sprite is used if the kit is missing.
- **Behaviour:** click areas, text and buttons are unchanged.
- **HUD:** two small cleanups in the action bar code, with no change in behaviour.

The magic dialog's engraved hexagram-in-a-ring is back as a transparent kit piece (HH-0071). It was redrawn from the original art at 50% opacity, and `tools/panel_kit/magic_circle.py` generates it into the panel kit as `magic_circle`. The client does not draw it yet.

The skill and magic dialogs now draw plain kit frames (HH-0066, batch 2), and the magic dialog draws the engraved circle behind its list (HH-0071).
- **Skill dialog:** the portrait is gone, and the scrollbar uses the kit's groove and thumb over the same travel as before.
- **Kit:** a new `piece()` call draws one named kit frame at its own size and skips it if the kit lacks it.

The shop, sell list, sell-or-repair, repair-all and warehouse dialogs now draw plain kit frames with silver-ball corners, the red title and the kit scrollbar (HH-0066, batch 3). Two shared helpers, `draw_shop_frame` and `draw_shop_scrollbar`, replace the repeated sprite draws, and the old sprites stay as the fallback.

The tester menu draws a kit frame and the standard red title with black flourishes on every page, and its level readout uses the standard font (HH-0072).

The magic shop, NPC talk, text, quest, help, party and shutdown-notice dialogs now draw plain kit frames, without their old pictures (HH-0066, batch 4).
- **Shared code:** every tall dialog uses `draw_tall_frame` and `draw_tall_scrollbar`, and the repeated title colour and panel sizes are now shared constants.
- **NPC talk:** the kit version shows a working scrollbar thumb, where the original drew only the track.

The city hall, command hall, crusade job and both guild dialogs now draw plain kit frames through the shared tall-frame helper (HH-0066, batch 5). Crusade job was not seen in game; it only opens for a citizen.

The level-up and majestic stat dialogs, item upgrade and manufacture now draw plain kit frames (HH-0066, batch 6). Manufacture loses its hammer picture, and the stat dialogs keep their stat-table sprite on top for now.

The magic circle is fainter: 35% opacity instead of 50%, so it no longer competes with the text over it (HH-0071 follow-up).

The commander, constructor and soldier crusade dialogs and change-password now draw kit frames (HH-0066, batch 7). Change-password gets kit input boxes and loses its axe picture.
- **Wider panel:** tall dialogs without a scrollbar, the tester menu included, use a 225 px panel like the originals. The six scrollbar dialogs keep 220.
- **Titles:** all kit titles come from one table through `find_title()`.

The magic shop draws the magic circle behind its spell list, centred where its old hexagram was (HH-0075).

The item creator draws a kit frame and the standard titles "Create Item" and "Configure Item", with no portrait (HH-0076).

The warning box, the shutdown notice and the eight connection and status popups now share one kit notice frame, without the ruined-city picture (HH-0066, batch 8). That finishes the dialog conversion; alchemy and crafting keep their cauldron art.

The panel and HUD kits are now required: the client loads them first and stops with an error if either is missing, and every old-art fallback is gone (HH-0067). The old title strips, the old HUD icon bar, the old Game1 frames and the retired minimap art are deleted; their sheet entries stay as empty placeholders so no other sprite moves. Unused frames inside sheets that are still partly used are listed in HH-0079 for later removal.

Copperplate CC Bold (SIL OFL 1.1, a free revival of Copperplate Gothic) now ships with the client fonts, unmodified, with its licence (HH-0081). Nothing draws with it yet.

The connection, login-result and status popups, the warning box and the shutdown notice now draw their headings in the kit title font instead of the old bitmap font (HH-0077).

The main menu and loading screen use the new 1920x1080 art (HH-0082). Wide screens show the whole picture, and 4:3 shows its centre. The menu buttons always sit on their plates, with black labels that turn gold on hover, placed where the original buttons were. "Loading Data ..." is now drawn from Liberation Serif, rendered once at load, and stays on screen even on ultra-wide displays. The magic-circle numerals and HUD digits now come from a 4x high-resolution sheet with refitted frames, so no glyph is clipped (HH-0083). A sprite can now declare an `art_scale` in its manifest to ship art at a higher resolution than it is drawn. The replaced sprites are archived under `working/archive/`.

The spell name a caster shouts now looks as it did in 3.82: dark red in the default font, drawn twice one pixel apart, with no shadow (HH-0084).

The create-account screen uses the new wide art (HH-0087). The form sits on the stone tablet with kit input fields and kit buttons, under a parchment "New Account" title with a soft dark glow. Validation, the per-field help text, tab and enter, Create, Clear and Cancel work as before. Every front-end screen except the splash now shows two lines at the bottom: "Copyright © 2004 Siementech Co., Ltd" and "Recreation © 2026 The Heldenian Project - open source" (HH-0088). The splash no longer shows the version number. Text that is rendered once and then reused now reads UTF-8, so the © sign draws correctly on Windows and Linux.

The login screen is recreated on the new wide art (HH-0089). The original login panel sits where it always did and fades in after half a second. Its labels are gold small caps in Copperplate CC Bold with the game's classic black drop shadow, and the fields are kit input boxes. Connect and Cancel get a soft additive white glow on hover; Connect only glows once both fields are valid, as before. Text and glows are rebuilt only after a window resize settles.

The shop's +/- buttons, the manufacture dialog's round arrow buttons and slot rings, and the skill list's small arrows now come from the panel kit, so the old game_dialogs sheet 001 is retired and archived (HH-0091). Horizontal slider knobs, such as the System Menu volume sliders, are now the kit's framed down-arrow box.

The old text buttons in dialogs and login overlays are now drawn from the panel kit instead of game_dialogs sheet 010, and their sizes come from fixed constants rather than the sheet (HH-0097). The sheet still loads for the character-select screen's banner and cards and the create-character header while those are retired.

The character-select and create-character screens no longer draw a second copy of their "Character List" and "Create New Character" banners over the background, which already shows them (HH-0097).
