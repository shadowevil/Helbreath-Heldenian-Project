# CControls: text input

*Part of the CControls reference; index and rules: `.claude/docs/ccontrols-reference.md`.*

## Text Input

`cc::text_input` is the base class for all text input controls. It owns the text buffer, cursor, selection, and character handling. Engine-agnostic — no rendering, no OS calls.

`cc::textbox` extends `text_input` with validation.

```cpp
auto* tb = m_controls.add<cc::textbox>(ID, bounds, max_length);
tb->set_hidden(true);                              // password masking (shows *)
tb->set_character_filter("abc123");                // whitelist — only these chars accepted
tb->set_validator([](const std::string& s) {       // live validation (colors field)
    return s.length() >= 3;
});
// tb->text()            — current text (std::string&)
// tb->display_text()    — masked text if hidden, raw text otherwise
// tb->cursor_pos()      — byte offset of cursor
// tb->is_valid()        — last validation result
// tb->has_selection()   — selection active
// tb->is_cursor_visible(time_ms) — blink state
```

### Character Filter

Whitelist of allowed characters. Characters not in the string are silently rejected. Empty string = no filter.

```cpp
#include "text_field_renderer.h"  // for hb::client:: presets

tb->set_character_filter(hb::client::username_allowed_chars);  // preset
tb->set_character_filter("0123456789");                        // digits only
```

Preset constants in `hb::client` namespace (`text_field_renderer.h`):
- `username_allowed_chars` — alphanumeric + `!@#$%^&*_-.`
- `password_allowed_chars` — alphanumeric + `!@#$%^&*_-.`
- `email_allowed_chars` — alphanumeric + `@.`
- `character_name_allowed_chars` — alphanumeric + `_-`

### Clipboard

Wire clipboard once on the collection. CControls handles Ctrl+C/X/V routing internally.

```cpp
m_controls.set_clipboard_provider(
    [] { return hb::shared::input::get_clipboard_text(); },
    [](const std::string& s) { hb::shared::input::set_clipboard_text(s); }
);
```

### Built-in Keyboard Handling

All handled internally by `control_collection::update()` when a `text_input` is focused:

| Key | Action |
|-----|--------|
| Typed characters | Insert at cursor (filtered by character filter) |
| Backspace | Delete before cursor (with key repeat) |
| Delete | Delete after cursor (with key repeat) |
| Left/Right | Move cursor (with key repeat) |
| Shift+Left/Right | Extend selection |
| Home/End | Jump to start/end |
| Shift+Home/End | Select to start/end |
| Ctrl+A | Select all |
| Ctrl+C | Copy selection |
| Ctrl+X | Cut selection |
| Ctrl+V | Paste at cursor |

### Text Field Rendering

Use the `hb::client::draw_text_field()` helper. Draws text, cursor blink, and selection highlight.

```cpp
#include "text_field_renderer.h"

tb->set_render_handler([](const cc::control& c) {
    auto& field = static_cast<const cc::text_input&>(c);
    hb::client::draw_text_field(field, GameClock::get_time_ms(),
        hb::shared::text::TextStyle::with_shadow(GameColors::InputValid),
        hb::shared::text::TextStyle::with_shadow(GameColors::InputInvalid));
});
```

### Input State Helper

Use `hb::client::fill_input_state()` to fill all input fields in one call:

```cpp
#include "input_state_helper.h"

void on_update() override
{
    cc::input_state input;
    hb::client::fill_input_state(input);
    m_controls.update(input, GameClock::get_time_ms());
}
```
