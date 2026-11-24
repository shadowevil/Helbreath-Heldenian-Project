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
