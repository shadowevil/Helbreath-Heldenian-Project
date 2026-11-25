# UI look decisions

## d-034 Text uses the classic drop shadow, rarely an outline

- Date: 2026-11-25.
- Quotes: "the font you have shows an outline instead of a drop shadow like the original which usually is just a copy of the drawn text colored black and moved 1px to the left and down 1px" / "sorry 1px to the right*"; "Typically in this game we won't use outlines very much".
- Decided: recreated text uses the game's classic drop shadow: a black copy of the text drawn 1 original pixel right and 1 down, beneath it, scaled with the art (2 art px at 1.8x). Outlines around text are the exception, not the default.
- Recorded in: HH-0089 (login recreation).

## d-035 An HD counterpart replaces the lower-quality version

- Date: 2026-11-25.
- Quotes: "We should also be replacing lower quality versions if we have a hd counterpart"; "Archiving the old versions".
- Decided: where an HD version of an art asset exists, the game draws the HD version at every scale, and the lower-quality version is retired. The old version is archived byte-identical under `working/archive/` and never deleted. This narrows the earlier "keep the 4:3 version as well" (HUD sheet): the 4:3 layout stays, but not the low-resolution pixels.
- Recorded in: HH-0100 (HUD HD atlas).

## d-036 No fallbacks to old versions; fallbacks only at the user's discretion

- Date: 2026-11-25.
- Quote: "We should not have fallbacks if we have created new versions - fall backs are only at my discression".
- Decided: once a new version of an asset or code path exists, the old one is removed rather than kept as a fallback. A fallback exists only when the user asks for it. Extends d-035.
- Recorded in: HH-0105 (newmaps fallback removed).
