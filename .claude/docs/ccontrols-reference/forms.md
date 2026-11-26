# CControls: dropdown and check box

*Part of the CControls reference; index and rules: `.claude/docs/ccontrols-reference.md`. Added 2026-11-25 (HH-0116) for the system menu rebuild (HH-0114). Read `core.md` first: both controls derive from `cc::button`, so clicks, focus, Enter/Space, the click sound, tooltips and the agent bridge's widget rows work as for a button. `on_click` is not used; use `set_on_change`.*

## Check box

```cpp
auto* vsync = m_controls.add<cc::check_box>(CHK_VSYNC, {x, y, 13, 13}, cfg.vsync);
vsync->set_on_change([this](int id, bool checked) { set_vsync(checked); });
hb::client::skin_check_box(*vsync, m_game->get_panel_kit());   // kit draw (client)
```

`is_checked()`, `set_checked(bool)` (no callback). A click, or Enter/Space when focused, toggles it and fires `on_change(id, checked)`. Disabled boxes ignore input.

## Dropdown

```cpp
auto* mode = m_controls.add<cc::dropdown>(DD_MODE, {x, y, 140, 16},
    std::vector<std::string>{"Windowed", "Borderless", "Exclusive"}, current);
mode->set_on_change([this](int id, int index) { stage_window_mode(index); });
mode->set_max_visible_rows(8);   // more options scroll
hb::client::skin_dropdown(*mode, m_game->get_panel_kit());   // field and list handlers
```

- **Options:** `set_options(vector, selected)` (closes the list), `options()`, `option_count()`, `selected()` (-1 if empty), `selected_text()`, `set_selected(i)` (clamped, no callback).
- **on_change(id, index)** fires only when a pick changes the selection; it fires last, so it may rebuild the controls.
- **Layout:** `set_row_height` (15), `set_list_padding` (3, the bevel), `set_list_gap` (1, field to list), `set_scrollbar_width` (15, taken from the rows only while the list scrolls), `set_max_visible_rows` (8).
- **Behaviour:** a click (or Enter/Space when focused) opens the list below the field, or above it when below would leave the screen. That needs `m_controls.set_screen_size(w, h)`; without it the list always opens below. A click on a row picks it. A click anywhere else (the field included) or Esc closes the list unchanged. The wheel scrolls one row per notch, dragging the scrollbar scrolls, and Up/Down move the hot row (starting from the current option) with Enter/Space picking it. Hiding or disabling the control closes it.
- **Modal while open:** the collection hands the open list all input. Every other control sees the mouse away and no key edges until the list closes and the mouse button is up, so the closing click does not land on a control underneath, and the Esc that closes it does not set `escape_pressed()`.
- **Render:** the control's render handler draws the field; `set_list_render_handler` draws the open list from `list_bounds()`, `first_visible()`, `visible_rows()`, `row_bounds(i)`, `hot_row()`, `selected()`, `is_scrollable()`, `scrollbar_bounds()` and `scroll_pos()`. `is_open()` and `opens_upward()` give the state.

## Overlay order

`m_controls.render()` draws the tree, then the open list (`render_overlay()`). A screen that draws over its controls after `render()` would cover the list. Such a screen calls `render_tree()`, draws its own layer, then calls `render_overlay()`.

## Wheel input

`cc::input_state::mouse_wheel` is a level: the running wheel total (120 per notch, positive = away from the user). The library takes the change between frames. `hb::client::fill_input_state` fills it from the engine delta plus the total that `commit_mouse_wheel()` (CGame, once per presented frame) folds in. Because of that, a frame updated several times before it is presented scrolls only once.

## Kit skins (client, `panel_kit.h`)

`skin_dropdown` and `skin_check_box` take a `hb::client::form_style`, whose defaults are the system menu mockup's (b2):
- **Field:** the standard `stone_field` inset shaded to `field_darken` 0.5 (0.75 when disabled). This is the nested-inset case of d-037.
- **Text:** light text in the default font, drawn at `text_x`.
- **Arrow:** `btn_sq_left` when collapsed, `btn_sq_down` when open, `btn_sq_left_dark` when disabled.
- **List:** `stone_cloth` darkened to 0.45 inside the kit inset bevel border, with a black drop shadow at distance 0 on the left, right and bottom only (`shadow` 1.0 = alpha 255, `shadow_blur` 6, `shadow_passes` 2: a Gaussian halo drawn twice). The hot row is filled in `row_hover` and the current option gets a gold diamond and gold text. The kit scrollbar appears when the list scrolls.
- **Check box:** a shaded inset; when checked, a raised square filled with gold.

The `panel_kit` members `fill`, `diamond`, `dropdown_field`, `list_box` and `check_box` draw these parts directly.
