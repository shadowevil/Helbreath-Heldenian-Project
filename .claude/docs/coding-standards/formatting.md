# Coding standards: formatting, braces, headers, comments (sect. 1, 2, 4, 7)

*Part of the coding standards; index and quick reference: `.claude/docs/coding-standards.md`. Section numbers are the original ones.*

## 1. Indentation & Whitespace

- **Tabs** for indentation (single `\t` character).
- No trailing whitespace.
- Blank line between method definitions.
- Blank line between logical sections within a method.
- Space after keywords: `if (`, `for (`, `while (`, `switch (`, `return `.
- Space around binary operators: `a + b`, `x == y`, `i < count`.
- No space after unary operators: `!flag`, `++i`, `*ptr`.
- No space inside parentheses: `foo(a, b)` not `foo( a, b )`.

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

## 4. Header Files

- **`#pragma once`** -- universal, no exceptions.
- **No `using namespace` in headers** -- ever. This pollutes all includers.
- **Include order** (separated by blank lines):
  1. Own header (`#include "my_class.h"`)
  2. Project headers (`#include "player.h"`)
  3. Shared/dependency headers (`#include "net_messages.h"`, `#include "item/item_enums.h"`)
  4. Standard library (`#include <vector>`)
  5. Platform headers (`#include <windows.h>`)

```cpp
#pragma once

#include "combat_system.h"

#include "player.h"
#include "weapon_config.h"

#include "item/item_enums.h"

#include <cstdint>
#include <memory>
#include <vector>
```

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
