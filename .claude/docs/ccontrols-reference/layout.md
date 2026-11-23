# CControls: alignment and flow layout

*Part of the CControls reference; index and rules: `.claude/docs/ccontrols-reference.md`.*

## Alignment

Controls can declare where they should sit relative to their parent, with the library computing the actual pixel position automatically. By default, alignment is `none` (explicit x/y from `local_bounds`).

```cpp
// Set screen size on the root (required for root-level alignment)
m_controls.set_screen_size(640, 480);

// Center a button horizontally, 200px from top
auto* btn = m_controls.add<cc::button>(ID, {0, 200, 164, 22});
btn->set_horizontal_alignment(cc::align::horizontal::center);

// Right-align within a panel, 10px from the right edge
auto* group = m_controls.add<cc::panel>(GRP_ID, {0, 100, 400, 300});
auto* cancel = group->add<cc::button>(ID, {0, 0, 80, 20});
cancel->set_horizontal_alignment(cc::align::horizontal::right, 10);

// Center both axes within parent
auto* logo = group->add<cc::label>(ID, {0, 0, 200, 50});
logo->set_horizontal_alignment(cc::align::horizontal::center);
logo->set_vertical_alignment(cc::align::vertical::center);

// Bottom-right corner of screen, 20px margin
auto* quit = m_controls.add<cc::button>(ID, {0, 0, 80, 20});
quit->set_horizontal_alignment(cc::align::horizontal::right, 20);
quit->set_vertical_alignment(cc::align::vertical::bottom, 20);
```

### Alignment Enums

| Enum | Values |
|------|--------|
| `cc::align::horizontal` | `none`, `left`, `center`, `right` |
| `cc::align::vertical` | `none`, `top`, `center`, `bottom` |

### Margin Semantics

| Alignment | Margin effect |
|-----------|---------------|
| `left` / `top` | distance from left/top edge |
| `center` | offset from center (positive = right/down) |
| `right` / `bottom` | distance inward from right/bottom edge |

### Getters

```cpp
ctrl->horizontal_alignment();   // cc::align::horizontal
ctrl->vertical_alignment();     // cc::align::vertical
ctrl->horizontal_margin();      // int
ctrl->vertical_margin();        // int
```

## Flow Layout

Parents can set a flow direction to automatically stack their children. Children participate in flow by default; set `allow_overlap(true)` to opt out and use alignment directly.

```cpp
// Vertical flow — children stack top-to-bottom
auto* group = m_controls.add<cc::panel>(GRP_ID, {50, 50, 300, 400});
group->set_flow(cc::flow_direction::vertical);

auto* btn1 = group->add<cc::button>(ID1, {0, 0, 120, 25});
btn1->set_horizontal_alignment(cc::align::horizontal::left);
btn1->set_padding(5);  // 5px gap after this button

auto* btn2 = group->add<cc::button>(ID2, {0, 0, 120, 25});
btn2->set_horizontal_alignment(cc::align::horizontal::left);
// btn2 appears at y = 30 (25 height + 5 padding from btn1)

// Horizontal flow — children stack left-to-right
auto* toolbar = m_controls.add<cc::panel>(TB_ID, {0, 0, 640, 40});
toolbar->set_flow(cc::flow_direction::horizontal);

auto* tool1 = toolbar->add<cc::button>(T1, {0, 0, 32, 32});
tool1->set_padding(4);

auto* tool2 = toolbar->add<cc::button>(T2, {0, 0, 32, 32});
// tool2 appears at x = 36 (32 width + 4 padding from tool1)
```

### Flow + Alignment Interaction

- **Flow axis**: Position is computed by stacking (sum of preceding siblings' sizes + paddings). Margin on the flow axis adds extra offset from the stacked position.
- **Cross axis**: Position is computed by alignment + margin as normal.
- **`allow_overlap(true)`**: Control ignores flow and uses alignment on both axes.

```cpp
// Centered column — vertical flow with centered horizontal alignment
auto* col = m_controls.add<cc::panel>(COL_ID, {0, 0, 400, 600});
col->set_flow(cc::flow_direction::vertical);

auto* title = col->add<cc::label>(ID, {0, 0, 200, 30});
title->set_horizontal_alignment(cc::align::horizontal::center);
title->set_padding(20);  // 20px gap below title

auto* btn = col->add<cc::button>(ID, {0, 0, 100, 25});
btn->set_horizontal_alignment(cc::align::horizontal::center);

// A floating overlay that ignores the flow
auto* overlay = col->add<cc::label>(ID, {0, 0, 100, 20});
overlay->set_horizontal_alignment(cc::align::horizontal::right, 10);
overlay->set_vertical_alignment(cc::align::vertical::bottom, 10);
overlay->set_allow_overlap(true);  // floats freely, doesn't affect flow
```

### Margin vs Padding

| Property | Purpose | Set via |
|----------|---------|---------|
| Margin | Offset from parent edge / aligned position | `set_horizontal_alignment(align, margin)`, `set_vertical_alignment(align, margin)` |
| Padding | Gap after this control before the next sibling in flow | `set_padding(int)` |

### Flow Direction Enum

| Value | Effect |
|-------|--------|
| `cc::flow_direction::none` | Default — children use explicit positioning or alignment |
| `cc::flow_direction::horizontal` | Children stack left-to-right |
| `cc::flow_direction::vertical` | Children stack top-to-bottom |
