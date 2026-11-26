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

## d-037 Every inset uses the standard kit inset; a nested inset may get its own shade

- Date: 2026-11-25.
- Quote: "We should follow the standard for every inset. If we inset more than once, then we can talk about another shade" (with a capture of the character dialog).
- Decided: every recessed panel (inset) in a recreated dialog is the panel kit's standard inset with its standard background, as in the character dialog and the item-upgrade slot (leather). Old art's own shade is not matched per dialog. Only an inset drawn inside another inset is a case for a different shade, and that is raised with the user first.
- Recorded in: HH-0093 (item-upgrade slot).
