# Coding standards: ownership, errors, modern C++, control flow, class design, wire structs (sect. 5, 6, 8, 9, 10, 11)

*Part of the coding standards; index and quick reference: `.claude/docs/coding-standards.md`. Section numbers are the original ones.*

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
