# Helbreath 3.82 - Coding Standards

*Assessed 2026-11-23 (docs standard sect. 1): kept whole. Numbered sections are short and CLAUDE.md states the binding rules; re-assess when transcript metrics show repeated searches into one section.*

Contents: 1 Indentation - 2 Braces - 3 Naming (classes, interfaces, methods, members, struct data, params, constants, enums, namespaces, files, globals) - 4 Headers - 5 Ownership - 6 Error handling - 7 Comments - 8 Modern C++ - 9 Control flow - 10 Class design - 11 Wire protocol structs - Quick Reference.

**Convention: `snake_case` throughout**, matching the C++ standard library. All identifiers — types, functions, variables, constants, enum values — use `snake_case`. The only exceptions are template parameters (`T`, `U`) and macros (`SCREAMING_CASE`).

These standards apply to **all new code** and to **legacy code being actively refactored**. Do not reformat untouched legacy code purely for style compliance.

---

## 1. Indentation & Whitespace

- **Tabs** for indentation (single `\t` character).
- No trailing whitespace.
- Blank line between method definitions.
- Blank line between logical sections within a method.
- Space after keywords: `if (`, `for (`, `while (`, `switch (`, `return `.
- Space around binary operators: `a + b`, `x == y`, `i < count`.
- No space after unary operators: `!flag`, `++i`, `*ptr`.
- No space inside parentheses: `foo(a, b)` not `foo( a, b )`.

---

## 2. Braces

**Allman style** for all bodies — classes, functions, namespaces, control flow:

```cpp
class weather_manager
{
	void set_weather(bool start, char effect_type);
};

void weather_manager::set_weather(bool start, char effect_type)
{
	if (start)
	{
		m_is_active = true;
		m_effect_type = effect_type;
	}
	else
	{
		m_is_active = false;
	}
}

namespace hb::client::combat
{

void resolve_attack()
{
	// ...
}

} // namespace hb::client::combat
```

Single-statement bodies **may** omit braces only if both the condition and body fit on one line:

```cpp
if (ptr == nullptr) return false;
```

Multi-line conditions or bodies always use braces.

---

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
their folders are `snake_case`, and Markdown docs are lowercase-hyphenated. Existing PascalCase names
are renamed by Phase R of `.claude/design/plans/sdl-versioning-and-sql-source.md`. Until then they stay,
and every new file follows the rule.

### Globals

Minimize globals. When unavoidable, prefix with `g_`:

```cpp
game* g_game = nullptr;  // Legacy, to be eliminated
```

New code should use dependency injection or singleton access patterns instead.

---

## 4. Header Files

- **`#pragma once`** -- universal, no exceptions.
- **No `using namespace` in headers** -- ever. This pollutes all includers.
- **Include order** (separated by blank lines):
  1. Own header (`#include "my_class.h"`)
  2. Project headers (`#include "player.h"`)
  3. Shared/dependency headers (`#include <Shared/NetMessages.h>`)
  4. Standard library (`#include <vector>`)
  5. Platform headers (`#include <windows.h>`)

```cpp
#pragma once

#include "combat_system.h"

#include "player.h"
#include "weapon_config.h"

#include <shared/item/item_enums.h>

#include <cstdint>
#include <memory>
#include <vector>
```

---

## 5. Memory Management & Ownership

### Ownership: `std::unique_ptr` only

```cpp
class entity_manager
{
	std::unique_ptr<entity> m_entities[max_entities];

	// Factory method transfers ownership
	std::unique_ptr<entity> create_entity(int type_id);
};
```

### Non-owning access: References preferred, raw pointers when necessary

Use **references** (`&`) when the target is guaranteed to exist:

```cpp
void apply_damage(entity& target, int amount);     // target always valid
void render(const renderer& renderer);              // renderer always valid
```

Use **raw pointers** only for optional/nullable non-owning references or storage in fixed-size arrays:

```cpp
entity* find_entity(int id);                   // May return nullptr
entity* m_client_list[max_clients] = {};       // Non-owning, nullable slots

void process_entity(entity* entity)
{
	if (entity == nullptr) return;
	// ...
}
```

### Rules

- **Never** use `new` / `delete` in new code. Use `std::make_unique`.
- **Never** use `std::shared_ptr` unless there is a proven shared-ownership requirement (there almost never is).
- Raw pointers **never** own memory. If you hold a raw pointer, someone else owns the object.
- Prefer references over pointers in function parameters when null is not a valid input.
- Use `nullptr`, never `NULL` or `0` for null pointers.

---

## 6. Error Handling

### Critical failures: Exceptions

Use exceptions for conditions that indicate programmer error, corrupted state, or unrecoverable system failures:

```cpp
if (!renderer.initialize())
	throw std::runtime_error("Failed to initialize renderer");

if (map_file.empty())
	throw std::invalid_argument("Map file path cannot be empty");
```

### Recoverable errors: Bool returns

Use `bool` return values for operations that can legitimately fail at runtime:

```cpp
bool load_map(const std::string& path);      // File might not exist
bool send_packet(const packet& packet);      // Network might be down
bool equip_item(int slot_id);                // Item might not be equippable
```

### Prohibited

- **No `goto`** -- ever. Use RAII, early returns, or scope guards for cleanup.
- **No error codes as integers** -- use `bool` or exceptions only.

---

## 7. Comments

- Use `//` exclusively. No `/* */` block comments.
- Comment the **why**, not the **what**. The code explains what it does.
- Section dividers for logical groupings within large files:

```cpp
//=============================================================================
// Combat Resolution
//=============================================================================

//-----------------------------------------------------------------------------
// Private helpers
//-----------------------------------------------------------------------------
```

- Do not add comments to self-explanatory code.
- Do not add doc comments or comment headers to methods unless the behavior is non-obvious.

---

## 8. Modern C++ Usage

### Use

- `constexpr` over `#define` for constants
- `std::unique_ptr` for ownership
- `static_cast<>` over C-style casts
- Range-based `for` when iterating full containers
- `auto` for iterator types, `make_unique` results, and complex template return types
- `= default` / `= delete` for special member functions
- `override` on all virtual method overrides (no redundant `virtual` keyword)

### Avoid

- `auto` for primitive types or when the type isn't obvious from context
- `std::shared_ptr` (use `std::unique_ptr` + raw non-owning pointers)
- Multiple inheritance (single inheritance + interfaces is fine)
- Exceptions in hot paths (game loop, rendering, per-frame logic)
- Templates for the sake of templates -- use only when they eliminate real duplication

### `override` Convention

```cpp
class sfml_renderer : public renderer
{
	bool initialize(int width, int height) override;  // Correct
	void shutdown() override;                          // Correct
	~sfml_renderer() override = default;               // Correct

	// WRONG: virtual bool initialize(...) override;   // Redundant virtual
};
```

---

## 9. Control Flow

- Prefer early returns to reduce nesting:

```cpp
// Good
bool process_entity(entity* entity)
{
	if (entity == nullptr) return false;
	if (!entity->is_alive()) return false;

	// Main logic at low nesting level
	entity->update();
	return true;
}

// Bad
bool process_entity(entity* entity)
{
	if (entity != nullptr)
	{
		if (entity->is_alive())
		{
			entity->update();
			return true;
		}
	}
	return false;
}
```

- `switch` statements must have a `default` case (even if just a `break`).
- Avoid deeply nested conditionals. Extract helper methods when nesting exceeds 3 levels.

---

## 10. Class Design

### Declaration Order

```cpp
class my_system
{
public:
	// Constructors, destructor
	// Public interface methods

protected:
	// Protected interface (for subclasses)

private:
	// Private methods
	// Private data members (always last)
};
```

### Singletons

Use the Meyers singleton pattern:

```cpp
class audio_manager
{
public:
	static audio_manager& get()
	{
		static audio_manager instance;
		return instance;
	}

	audio_manager(const audio_manager&) = delete;
	audio_manager& operator=(const audio_manager&) = delete;

private:
	audio_manager() = default;
};
```

### Virtual Destructors

All base classes with virtual methods **must** have a virtual destructor:

```cpp
class renderer
{
public:
	virtual ~renderer() = default;
	virtual bool initialize() = 0;
};
```

---

## 11. Wire Protocol / Network Structs

Network-serialized structs are an **intentional exception** to member naming rules. They use Hungarian notation without `m_` prefix to match the binary protocol layout:

```cpp
#pragma pack(push, 1)
struct player_appearance
{
	int16_t iBodyType;
	int16_t iWeaponType;
	int16_t iShieldType;
	bool bFrozen;
	// ... matches wire format
};
#pragma pack(pop)
```

This convention is **only** for packed protocol structures. All other structs follow standard naming.

---

## Quick Reference

| Element | Convention | Example |
|---------|-----------|---------|
| Indentation | Tabs | `\t` |
| Braces | Allman | `{\n\t...\n}` |
| Classes/Structs | `snake_case` | `weather_manager` |
| Interfaces | `snake_case` (no prefix) | `renderer`, `game_screen` |
| Methods | `snake_case` | `get_map_index()` |
| Members | `m_` + `snake_case` | `m_position_x` |
| Struct data | `snake_case` (no `m_`) | `x`, `color`, `step` |
| Params/Locals | `snake_case` | `target_id` |
| Constants | `constexpr snake_case` | `constexpr int max_slots = 50;` |
| Enums | Namespace + `snake_case` | `attack_type::normal` |
| Enum (type-safe) | `enum class snake_case` | `screen_type::login` |
| Namespaces | `hb::<scope>::snake_case` | `hb::client::combat` |
| Files | `snake_case` + prefix | `screen_login.cpp` |
| Header guards | `#pragma once` | universal |
| Comments | `//` only | no `/* */` |
| Null | `nullptr` | never `NULL` or `0` |
| Ownership | `std::unique_ptr` | `std::make_unique<T>()` |
| Non-owning | Reference or raw ptr | `entity&` or `entity*` |
| Errors (fatal) | Exceptions | `throw std::runtime_error(...)` |
| Errors (recoverable) | `bool` return | `bool load_map(...)` |
