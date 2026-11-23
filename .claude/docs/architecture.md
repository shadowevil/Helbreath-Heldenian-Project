# Architecture Reference

*Assessed 2026-11-23 (docs standard sect. 1): kept whole, about 6 KB, under the 10 KB review line. Read as one picture when working across subsystems; the Client and Server sections are short and the Shared section is context for both. Project structure moved here from CLAUDE.md 2026-11-23. Audited 2026-11-23 (HH-0020): file names and paths below are the post-Phase R snake_case ones; class names are mostly still the legacy ones (verified `CGame`, `CPlayer`, `CClient`, `CMapData`, `IGameScreen`, `IDialogBox`, `IRenderer`, `GameModeManager`, `DialogBoxManager`; `audio_manager` is already renamed), and counts (61 dialogs, 2000 clients) are unverified.*

Contents: Solution Structure - Runtime Layout - Client - Server - Shared Protocol - Key Anti-Patterns.

## Solution Structure
- **Client** (`sources/client/`) — Game client, C++20. Cross-platform (Windows + Linux). Depends on SFMLEngine and CControls.
- **CControls** (`sources/ccontrols/`) — UI control library, C++20. Pure logic, no game/engine dependencies. Static library (`cc::` namespace). API: `.claude/docs/ccontrols-reference.md`.
- **SFMLEngine** (`sources/sfml_engine/`) — Rendering abstraction over SFML. Cross-platform (Windows + Linux).
- **Server** (`sources/server/`) — Game server, C++20. Cross-platform (Windows + Linux). Standalone.
- **Shared** (`sources/dependencies/shared/`) — Protocol, enums, items, packets, networking; headers shared by both.
- **Build tools** (`sources/build_tools/`) — Build ID generators, `deploy.py`, `layout.props`/`layout.cmake`. **Database source** (`sources/database/`) — per-table SQL; see `.claude/docs/versioning/sql-source.md`.
- `staged/` — tracked source of truth for static runtime files (client, server, shared).
- `game/` — deployed game, assembled by deploy (gitignored); runtime state lives here.

## Runtime Layout
- `game/client/` — Client exe, configs, sprites, sounds, maps, fonts (assembled by deploy from `bin/client/` and `staged/`).
- `game/server/` — Server exe, SQLite DBs (`gamedata.db`, `mapinfo.db`, generated from `sources/database/`), `accounts/`.
- Deploy and the full layout: `.claude/docs/build.md`.

---

## Client

### Entry & Main Loop (`wmain.cpp`)
Decoupled update/render: `UpdateFrame()` every iteration (network, audio, game state); `RenderFrame()` gated by frame limiter.

### Game Class (`CGame`)
Central orchestrator: player, map, effects, networking, dialogs, camera, renderer, sprites, configs. Primary decomposition target — extracted managers: AudioManager, ConfigManager, GameModeManager, DialogBoxManager, WeatherManager, BuildItemManager, GuildManager, MagicCastingSystem, CombatSystem, ShopManager, InventoryManager, CrusadeManager, TeleportManager, TextInputManager, EventListManager, FloatingTextManager, EffectManager, HotkeyManager, CCamera, PlayerRenderer, NpcRenderer.

### Screen State Machine (`IGameScreen` + `GameModeManager`)
Type-safe screen management with fade transitions. Screens: Splash, MainMenu, Login, SelectCharacter, CreateCharacter, CreateAccount, Loading, OnGame. Overlays: Connecting, WaitingResponse, Msg.

### Dialog System (`IDialogBox` + `DialogBoxManager`)
61 dialog boxes by enum ID with z-order. Interface: `OnDraw`, `OnClick`, `OnDoubleClick`, `OnPress`, `OnItemDrop`, `OnUpdate`.

### Networking (`NetworkMessageManager` + `ASIOSocket`)
Polling mode, two sockets (game `m_pGSock`, login `m_pLSock`). Handlers split across ~20 `network_messages_*.cpp` files.

### Rendering (SFMLEngine)
Abstract interfaces (`IRenderer`, `IWindow`, `ISprite`, `IBitmapFont`) with SFML implementations. Render order: clear → tiles → lighting → entities (z-ordered) → dialogs → particles → present. Sprites from PAK files via `SFMLSpriteFactory`.

### Entity System
- `CPlayer` — Full player state. `CPlayerRenderer`/`CNpcRenderer` — separated rendering.
- `AnimationState` — 10 animation types with frame timing.
- `CMapData` — 60x55 tile grid with entity tracking, items, chat anchors.

### Audio (`AudioManager`)
Miniaudio. Pre-decoded WAV in 3 categories. Positional audio, 32 concurrent sounds.

---

## Server

### Entry & Game Loop (`wmain.cpp`)
Hidden window + console. `GameProcess()` every ~30ms, drain socket queues, poll console input.

### Game Class (`CGame`)
Manages: 2000 clients, 100 maps, 15000 NPCs, 5000 item types, 60000 dynamic objects/delay events. Extracted managers: FishingManager, MiningManager, CraftingManager, QuestManager, GuildManager, DelayEventManager, DynamicObjectManager, LootManager, CombatManager, ItemManager, MagicManager, SkillManager, WarManager, StatusEffectManager, EntityManager.

### Client Connections (`CClient`)
Connected player: character data, position, stats, inventory (50), bank (1000), equipped items, magic/skill mastery, guild, party, status effects, admin level. 30s timeout.

### Map/World (`CMap` + `CTile`)
Grid-based. Tiles track: owner entity, 12 items, dynamic objects, movement/teleport flags, faction occupy. Maps have: teleports, spawn points, waypoints, fishing/mining, no-attack zones, crusade structures.

### Entity Management (`EntityManager` + `CNpc`)
NPC lifecycle with active entity list optimization. Movement types (waypoint, random, follow, guard), behavior states, special abilities, AI levels, summon support.

### Database (SQLite)
- `AccountSqliteStore` — Per-account `.db` in `accounts/`. Characters, items, bank, equips, mastery.
- `GameConfigSqliteStore` — `gamedata.db` (config files in `gameconfigs/`). Items, NPCs, magic, skills, quests, drops, shops.
- `MapInfoSqliteStore` — `mapinfo.db`. Dimensions, teleports, spawns, waypoints, generators.

### Commands
Console: `ServerCommand` + `ServerCommandManager`. In-game: `GameChatCommand` with admin levels.

### Login (`LoginServer`)
Connect → validate → character list → select → redirect to game socket → load from SQLite → enter world.

---

## Shared Protocol (`dependencies/shared/`)
- `net/net_messages.h` — 100+ message IDs. `net/net_constants.h` — buffer sizes, limits, view range (25x17).
- `packet/` — 25 packet headers with byte-packing (counted 2026-11-23).
- `item/` — Unified item class, enums (35 effects, 15 equip positions, 13 types; counts unverified).
- `entity/appearance.h` — Player/NPC visuals. `entity/player_status_data.h` — Status flags.
- `entity/owner_type.h` — Entity type IDs. `net/asio_socket.h` — Shared networking (class `ASIOSocket`).
- Also `game/` (formula engine, shared calculations), `log/` (logging), `render/` (renderer interfaces), `util/` (hmac, sha256), `version.h`/`version.cpp`.

## Key Anti-Patterns
1. **God Objects** — CGame on both sides. Decompose into focused systems.
2. **Global State** — `G_pGame` etc. Move toward dependency injection.
3. **Mixed C/C++** — `char[]`/`std::string`, `memcpy`/STL, `new`/smart pointers. Standardize.
4. **Spaghetti Handlers** — Network handlers mix parsing, logic, UI. Separate concerns.
5. **Magic Numbers** — Use named constants. **Fixed Arrays** — `T*[N]` with linear search, consider containers.
