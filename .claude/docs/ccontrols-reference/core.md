# CControls: core pattern and controls

*Part of the CControls reference; index and rules: `.claude/docs/ccontrols-reference.md`.*

Static C++20 UI control library (`sources/ccontrols/`). Namespace `cc::`. No game/engine dependencies.

**All menu screen UI (buttons, textboxes, focus, tooltips) MUST use CControls.** Do NOT write manual `is_mouse_in_rect()` hit-testing, manual `m_cur_focus` tracking, manual per-control rendering loops, or manual edge detection.

## Include

```cpp
#include "ccontrols.h"   // umbrella header — pulls in all types
```

## Core Pattern

Every screen gets one `cc::control_collection` member. All control creation, callbacks, render handlers, and focus order go in `on_initialize()`. The `on_update()` and `on_render()` methods become trivial.

```cpp
class Screen_Example : public IGameScreen
{
    cc::control_collection m_controls;

    enum { BTN_OK = 1, BTN_CANCEL = 2, TXT_NAME = 3 };

    void on_initialize() override
    {
        // Default click sound for all buttons
        m_controls.set_click_sound([this]{ play_game_sound('E', 14, 5); });

        // Create controls — returns raw pointer for further setup
        auto* btn = m_controls.add<cc::button>(BTN_OK, {100, 200, 80, 20});
        btn->set_on_click([this](int) { do_action(); });
        btn->set_render_handler([this](const cc::control& c) {
            auto sb = c.screen_bounds();
            int frame = (c.is_held() || c.is_focused()) ? 1 : 0;
            m_game->m_sprite[SPRITE_ID]->draw(sb.x, sb.y, frame);
        });

        m_controls.set_focus_order({BTN_OK, BTN_CANCEL, TXT_NAME});
    }

    void on_update() override
    {
        cc::input_state input;
        hb::client::fill_input_state(input);  // fills mouse, keys, typed chars
        m_controls.update(input, GameClock::get_time_ms());
    }

    void on_render() override
    {
        draw_background();
        m_controls.render();   // ONE CALL: walks tree, invokes all render handlers
        draw_overlays();
    }
};
```

## Control Types

| Type | Purpose | Focusable |
|------|---------|-----------|
| `cc::button` | Clickable region with hover/press/focus | Yes |
| `cc::text_input` | Text input with cursor, selection, clipboard | Yes |
| `cc::textbox` | Text input + validation (extends `text_input`) | Yes |
| `cc::label` | Static text display | No |
| `cc::toggle_button` | Paired -/+ buttons for value adjustment | Yes |
| `cc::panel` | Invisible grouping container | No |
| `cc::control_collection` | Screen-level owner (update + render) | N/A |

## Control Construction

All controls take `(int id, cc::rect local_bounds, ...)`:

```cpp
// Button
m_controls.add<cc::button>(ID, {x, y, w, h});

// Textbox (with max character length)
m_controls.add<cc::textbox>(ID, {x, y, w, h}, max_length);

// Label (with initial text)
m_controls.add<cc::label>(ID, {x, y, w, h}, "text");

// Toggle button (with minus/plus sub-regions LOCAL to the control)
m_controls.add<cc::toggle_button>(ID, {x, y, w, h}, minus_rect, plus_rect);

// Panel (invisible container for grouping)
auto* group = m_controls.add<cc::panel>(ID, {x, y, w, h});
group->add<cc::button>(CHILD_ID, {rx, ry, rw, rh});  // child is relative to group
```

## Parent/Child Tree

Any control can own children via `add<T>()`. Coordinates are relative to the parent.

```
root (panel at 0,0)
  +-- stats_group (panel at 60, 260)        <- relative to root
  |     +-- str_toggle (at 176, 16)         <- screen pos = 60+176, 260+16
  |     +-- vit_toggle (at 176, 31)
  +-- action_group (panel at 0, 445)
        +-- btn_create (at 384, 0)
```

**State cascading**: `group->set_visible(false)` hides all children. `group->set_enabled(false)` disables all children. No need to touch individual controls.

## Input Model

The game passes **raw level states** (is the key/button down right now). The library internally tracks the previous frame to compute edges (pressed/released). This prevents bugs and simplifies game-side wiring.

Use `hb::client::fill_input_state(input)` to fill everything from the engine in one call. The helper fills mouse, navigation keys, modifier keys, editing keys, and typed characters.

```cpp
cc::input_state input;
hb::client::fill_input_state(input);  // fills all fields from engine
m_controls.update(input, GameClock::get_time_ms());
```

## Click Model

- **Visual depression** on mouse-down: `is_held()` returns true while mouse is down over the control
- **Action fires** on mouse-up while still over the control (cancel by dragging off)
- **Enter key** on a focused button fires immediately
- The `on_click` callback is invoked by `control_collection`, not by the button itself

## Render Handlers

Set once in `on_initialize()`. The library drives *when* to render; the game decides *how*.

### Sprite Button
```cpp
btn->set_render_handler([this, normal, focused](const cc::control& c) {
    auto sb = c.screen_bounds();
    int frame = (c.is_held() || c.is_focused()) ? focused : normal;
    m_game->m_sprite[SPRITE]->draw(sb.x, sb.y, frame);
});
```

### Textbox with Validation
```cpp
tb->set_render_handler([](const cc::control& c) {
    auto& field = static_cast<const cc::text_input&>(c);
    hb::client::draw_text_field(field, GameClock::get_time_ms(),
        valid_style, invalid_style);  // handles all states: text, cursor, selection
});
```

### Group Panel with Background
```cpp
group->set_render_handler([this](const cc::control& c) {
    auto sb = c.screen_bounds();
    draw_filled_rect(sb.x, sb.y, sb.w, sb.h, bg_color);
});
// Children render AFTER parent (depth-first), so they draw on top.
```

## State Queries

Available in render handlers and callbacks:

| Method | Description |
|--------|-------------|
| `is_hovered()` | Mouse is over this control |
| `is_focused()` | Currently focused (keyboard/hover) |
| `is_pressed()` | Mouse just went down this frame (edge) |
| `is_held()` | Mouse is held down while hovered (for visual depression) |
| `is_enabled()` | Local enabled state |
| `is_visible()` | Local visible state |
| `is_effectively_enabled()` | Self AND all ancestors enabled |
| `is_effectively_visible()` | Self AND all ancestors visible |
| `screen_bounds()` | Absolute position (walks parent chain) |

## Toggle Button

Paired -/+ regions with sub-region state for rendering:

```cpp
auto* toggle = m_controls.add<cc::toggle_button>(ID,
    {x, y, w, h},           // overall bounds
    {0, 0, 12, 12},         // minus sub-region (local to toggle)
    {w - 12, 0, 12, 12});   // plus sub-region (local to toggle)

toggle->set_on_change([this](int id, int delta) {
    // delta = -1 (minus clicked/left arrow) or +1 (plus clicked/right arrow)
    adjust_stat(id, delta);
});

toggle->set_render_handler([this](const cc::control& c) {
    auto& t = static_cast<const cc::toggle_button&>(c);
    // t.is_minus_hovered(), t.is_plus_hovered()
    // t.is_minus_held(), t.is_plus_held()
});
```

Key repeat: Holding left/right arrow on a focused toggle auto-repeats with configurable delay. Set via `m_controls.set_key_repeat(initial_ms, interval_ms)`.

## Click Sound

Default sound set on collection; individual controls can override or silence:

```cpp
// Default for all buttons/toggles on this screen
m_controls.set_click_sound([this]{ play_sound(14); });

// Override for a specific button
btn->set_click_sound([this]{ play_sound(99); });

// Silence a specific button (no sound at all)
btn->set_click_sound(nullptr);
```

## Opacity

Controls have a float opacity (0.0 - 1.0, default 1.0). Render handlers use it for fade effects. Set per-frame in `on_update()`, read in render handlers.

```cpp
ctrl->set_opacity(0.5f);   // half transparent
ctrl->opacity();            // returns 0.5f
```

Values are clamped to [0.0, 1.0]. Opacity does NOT affect child rendering or hit-testing — it's purely a value for render handlers to read.

## Collection-Level Settings

```cpp
m_controls.set_screen_size(640, 480);      // root dimensions for alignment (default: 0x0)
m_controls.set_hover_focus(false);           // hover auto-focuses (default: false)
m_controls.set_tooltip_delay_ms(500);       // tooltip appears after 500ms hover (default)
m_controls.set_key_repeat(400, 80);         // initial 400ms, then 80ms repeat (default)
m_controls.set_click_sound(callback);       // default click sound for all controls
m_controls.set_enter_advances_focus(true); // Enter on textbox advances focus (default: false)
m_controls.set_default_button(BTN_LOGIN);  // auto-fire this button when Enter-advance lands on it
```

## Default Button

When `set_default_button(id)` is set, Enter on a textbox can auto-fire that button:

- **With `set_enter_advances_focus(true)`**: Enter advances focus. If the new focused control IS the default button, it fires automatically. This gives natural form flow — Enter on the last textbox submits.
- **Without `set_enter_advances_focus`**: Enter on any textbox fires the default button directly (no focus change).

```cpp
// Login screen: Enter on name → advance to password, Enter on password → submit
m_controls.set_enter_advances_focus(true);
m_controls.set_default_button(BTN_LOGIN);
m_controls.set_focus_order({TXT_NAME, TXT_PASSWORD, BTN_LOGIN, BTN_CANCEL});
```

## Collection Queries

```cpp
m_controls.active_tooltip();   // tooltip text of hovered control (after delay), or nullptr
m_controls.enter_pressed();    // enter was pressed and not consumed by a textbox
m_controls.escape_pressed();   // escape was pressed
m_controls.focused_id();       // ID of currently focused control
```

## Tooltips

```cpp
btn->set_tooltip("Click to start");
// In on_render(), after m_controls.render():
if (auto* tip = m_controls.active_tooltip())
    draw_tooltip(tip);
```

## Debug Outlines

```cpp
#ifdef _DEBUG
m_controls.debug_walk([this](const cc::control& c) {
    auto sb = c.screen_bounds();
    draw_rect_outline(sb.x, sb.y, sb.w, sb.h, fuchsia);
});
#endif
```

## What NOT To Do

- Do NOT write `is_mouse_in_rect()` for menu screen buttons
- Do NOT track `m_cur_focus` / `m_max_focus` manually
- Do NOT use `text_input_manager` for CControls screens — text input is fully integrated
- Do NOT duplicate coordinates between update and render
- Do NOT pass edge-detected input — pass raw levels only
- Do NOT call `fire_click()` directly — `control_collection::update()` handles it
