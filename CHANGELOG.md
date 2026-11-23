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
