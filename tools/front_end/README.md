# front_end

Generators for the HD front-end screen art.

- `make_select_backdrop.py` derives `staged/client/sprites/interface/front_end/character_select.png` (tracked output) from the user's `working/classic_ui/hd/character_selection_screen.png` (read only), baking the original screen's recessed help, account and footer boxes into the HD plank (HH-0108). `--dry-run` prints the box rects and writes nothing. Re-run it after the user changes the source art.
- `make_create_backdrop.py` derives `staged/client/sprites/interface/front_end/character_create.png` (tracked output) from the user's `working/classic_ui/hd/character_creation_screen.png` (read only), baking the name, stat and HP/MP/SP value-box recesses (the select screen's recess style and numbers) into the HD panels and keeping the arrow and +/- button strip beside the backdrop (HH-0122). `--dry-run` prints the box rects and writes nothing. The boxes must agree with `sources/client/screen_create_new_character.cpp`; re-run it after the user changes the source art.
