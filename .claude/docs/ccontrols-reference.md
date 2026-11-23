# CControls library reference (index)

*Assessed 2026-11-23 (docs standard sect. 1) and split the same day: the old single file was 19 KB. A screen build needs `core.md`; text boxes and alignment/flow are separate topics read only when used. Each sidecar stands alone. Check against usage once transcript metrics exist (noted 2026-11-23).*

Static C++20 UI control library (`sources/ccontrols/`), namespace `cc::`, no game or engine dependencies. The binding rule (all menu screen UI uses CControls, no manual hit-testing or focus tracking) is stated in CLAUDE.md "UI Controls".

- `ccontrols-reference/core.md` - include, the one-collection-per-screen pattern, control types and construction, parent/child tree, input and click model, render handlers, state queries, toggle button, click sound, opacity, collection settings, default button, queries, tooltips, debug outlines, what not to do; read first, before building or changing any menu screen.
- `ccontrols-reference/text-input.md` - `text_input` and `textbox`: character filter, validation, clipboard, built-in key handling, `draw_text_field`, the input-state helper; read when a screen has a text field.
- `ccontrols-reference/layout.md` - alignment, margins, flow layout, padding and overlap; read when positioning controls by alignment or stacking instead of fixed x/y.
