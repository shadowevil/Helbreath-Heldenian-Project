# front_end

Generators for the HD front-end screen art.

- `make_select_backdrop.py` derives `staged/client/sprites/interface/front_end/character_select.png` (tracked output) from the user's `working/classic_ui/hd/character_selection_screen.png` (read only), baking the original screen's recessed help, account and footer boxes into the HD plank (HH-0108). `--dry-run` prints the box rects and writes nothing. Re-run it after the user changes the source art.
