# Coding standards: naming (sect. 3)

*Part of the coding standards; index and quick reference: `.claude/docs/coding-standards.md`. Section numbers are the original ones.*

## 3. Naming Conventions

### Classes & Structs

**`snake_case`**, no prefix.

```cpp
class weather_manager;       // Not WeatherManager, not CWeatherManager
class combat_system;         // Not CombatSystem
struct player_appearance;    // No prefix
struct weather_particle;     // Not WeatherParticle
```

Do not rename legacy `C`-prefixed or `PascalCase` classes unless actively refactoring them.

### Interfaces (Abstract Base Classes)

**`snake_case`**, no prefix. The name describes the abstraction. Implementations get descriptive names.

```cpp
class renderer;              // Abstract interface (was IRenderer)
class game_screen;           // Abstract interface (was IGameScreen)
class dialog_box;            // Abstract interface (was IDialogBox)

class sfml_renderer;         // Concrete implementation
class login_screen;          // Concrete implementation
```

If disambiguation is genuinely needed, suffix with `_base` or `_interface`:

```cpp
class renderer_base;         // Only if 'renderer' alone is ambiguous
```

Legacy `I`-prefixed classes retain their names until actively refactored.

### Methods & Free Functions

**`snake_case`**, no Hungarian return-type prefixes.

```cpp
bool initialize();           // Not Initialize(), not bInitialize()
int get_map_index();         // Not GetMapIndex(), not iGetMapIndex()
void process_entities();     // Not ProcessEntities()
uint32_t get_item_count();   // Not GetItemCount(), not dwGetItemCount()
void draw();                 // Not Draw()
void update(uint32_t time);  // Not Update()
```

### Member Variables

**`m_` prefix + `snake_case`**. No type encoding.

```cpp
class player
{
	int m_position_x;                // Not m_iPositionX
	bool m_is_visible;               // Not m_bIsVisible
	uint32_t m_last_time;            // Not m_dwLastTime
	std::unique_ptr<item> m_weapon;  // Not m_pWeapon
	char m_name[20];                 // Not m_cName or m_szName
	std::string m_guild_name;        // Not m_guildName (camelCase)
};
```

### Struct Data Members (Non-Class)

Plain data structs (no methods or private state) use **`snake_case`** without `m_` prefix:

```cpp
struct weather_particle
{
	short x = 0;             // Not sX
	short y = 0;             // Not sY
	short base_x = 0;        // Not sBX
	char step = 0;           // Not cStep
};

struct event_entry
{
	uint32_t time = 0;       // Not dwTime
	char color = 0;          // Not cColor
	char text[96]{};         // Not cTxt
};
```

### Parameters & Local Variables

**`snake_case`**. No Hungarian prefixes.

```cpp
void apply_damage(int target_id, int damage_amount, bool is_critical)
{
	int final_damage = damage_amount * multiplier;
	bool target_alive = true;
}

void enable_dialog(int box_id, int type, int value1, int value2, char* text)
{
	// Not (int iBoxID, int cType, int sV1, int sV2, char* pString)
}
```

**Names must be self-documenting.** Expand abbreviations: `mouse_x` not `ms_x`, `direction` not `dir`, `current_time` not `time`. A reader should understand what a variable holds from its name alone, without needing type prefixes or surrounding context. Follow C#-style descriptive naming in snake_case.

### Constants

**`constexpr`** preferred. **`snake_case`** name within appropriate scope.

```cpp
// Class-scoped
class player
{
	static constexpr int max_inventory_slots = 50;
	static constexpr int max_bank_slots = 1000;
};

// Namespace-scoped
namespace hb::shared::limits
{
	constexpr int max_players = 2000;
	constexpr int max_maps = 100;
	constexpr int view_range_x = 25;
	constexpr int view_range_y = 17;
}
```

Do not use `#define` for new constants. Existing `DEF_` macros in legacy code remain until that code is refactored.

### Enums

**Namespace-wrapped unscoped enum** with explicit underlying type and **`snake_case`** values:

```cpp
namespace attack_type
{
	enum type : int
	{
		normal = 1,
		strong = 20,
		super_attack = 30,
		critical = 40,
	};
}

// Usage - auto-converts to int, works in switch statements:
int attack = attack_type::normal;

switch (attack)
{
case attack_type::normal:
	break;
case attack_type::super_attack:
	break;
}
```

For enums that should **not** implicitly convert (type safety is more important), use `enum class`:

```cpp
enum class screen_type : uint8_t
{
	splash,
	main_menu,
	login,
	on_game,
};
// Requires static_cast for conversion - intentional.
```

### Namespaces

**`snake_case`**, structured as `hb::<scope>::<name>`:

```cpp
namespace hb::shared::net      { }  // Shared networking
namespace hb::shared::limits   { }  // Shared game limits
namespace hb::shared::item     { }  // Shared item definitions
namespace hb::client::combat   { }  // Client combat system
namespace hb::client::ui       { }  // Client UI utilities
namespace hb::server::entity   { }  // Server entity management
namespace hb::server::database { }  // Server database layer
```

Scopes:
- `hb::shared::` -- Code used by both client and server
- `hb::client::` -- Client-only code
- `hb::server::` -- Server-only code

### Files

**`snake_case`** with system prefixes for categorized files:

```
screen_login.cpp              // Screen implementations
dialog_box_inventory.cpp      // Dialog implementations
network_messages_combat.cpp   // Network handler categories
overlay_connecting.cpp        // Overlay screens
```

Standalone systems use plain `snake_case`:

```
weather_manager.cpp
combat_system.cpp
audio_manager.cpp
```

Names follow the global naming rule in `~/.claude/CLAUDE.md` (2026-11-23): C++ and Python files and
their folders are `snake_case`, and Markdown docs are lowercase-hyphenated. Phase R of
`.claude/design/plans/sdl-versioning-and-sql-source.md` renamed the files and folders on 2026-11-23.
Legacy class and identifier names (`CGame`, `IGameScreen`, ...) stay until the code is refactored,
and every new file and identifier follows the rule.

### Globals

Minimize globals. When unavoidable, prefix with `g_`:

```cpp
game* g_game = nullptr;  // Legacy, to be eliminated
```

New code should use dependency injection or singleton access patterns instead.
