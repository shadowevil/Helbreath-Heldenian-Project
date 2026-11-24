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
- **For later use:** the kit also holds a standing, south-east-facing picture of each of the 70 killable creatures. These are not drawn yet.

The corner minimap now has two zoom levels, the whole map and the mid view. The close view was dropped because the parchment map takes its place (d-033).
