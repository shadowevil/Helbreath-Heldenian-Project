# Helbreath 3.82 - Coding Standards (index)

*Assessed 2026-11-23 (docs standard sect. 1) and split the same day: the old single file was 16 KB. CLAUDE.md states the binding rules; these are the detail. Section numbers are kept (sect. 3 is still naming) so citations resolve. The quick reference below is the shared context and stays here.*

**Convention: `snake_case` throughout**, matching the C++ standard library. All identifiers - types, functions, variables, constants, enum values - use `snake_case`. The only exceptions are template parameters (`T`, `U`) and macros (`SCREAMING_CASE`).

These standards apply to **all new code** and to **legacy code being actively refactored**. Do not reformat untouched legacy code purely for style compliance.

- `coding-standards/formatting.md` - sect. 1 indentation and whitespace, 2 Allman braces, 4 header files and include order, 7 comments; read when laying out a new file or unsure of include order.
- `coding-standards/naming.md` - sect. 3 naming for classes, interfaces, methods, members, struct data, params, constants, enums, namespaces, files and globals; read when naming anything new or judging a name in review.
- `coding-standards/language-use.md` - sect. 5 ownership, 6 error handling, 8 modern C++, 9 control flow, 10 class design (declaration order, singletons, virtual destructors), 11 wire protocol structs; read when writing logic or reviewing structure.

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
