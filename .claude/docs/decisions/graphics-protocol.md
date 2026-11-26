# Decisions: graphics protocol

## d-038 Display capabilities are part of the graphics protocol; every backend implements them

- Date: 2026-11-25.
- Quote: "Yes real rate, but it should be a graphics protocol addition not implemented for just SFML - all graphic frameworks that use the graphics protocol should implement their own way to match the spec".
- Decided: the display refresh rate is a real display setting, not only a frame cap (in exclusive fullscreen the monitor switches to the chosen rate). It is specified in the engine-neutral graphics protocol (the renderer and window interfaces the client talks to), and each graphics framework behind that protocol (today `sfml_engine`) implements it its own way to meet the spec. The client never calls a backend's own API for it. The same holds for the rest of the display model in HH-0114 (window modes, resolution and refresh lists, apply).
- Recorded in: HH-0114 (system menu and display overhaul), HH-0117 (display model).
