#pragma once

#include "button.h"

#include <functional>
#include <string>
#include <vector>

namespace cc {

using selection_change_callback = std::function<void(int control_id, int index)>;

// A collapsed field showing the current option; a click, Enter or Space opens a list of the options below it
// (above it when the list would leave the screen: the collection's set_screen_size). While the list is open the
// collection hands it all input and draws it over the other controls (control_collection::render_overlay):
// a click on a row picks that option, a click elsewhere or Esc closes it unchanged, the wheel and a drag on the
// scrollbar scroll a long list, Up / Down move the hot row and Enter or Space picks it.
// The control's render handler draws the field; set_list_render_handler draws the open list.
// Clicks, focus, the click sound and tooltips work as for cc::button; on_click is not used.
class dropdown : public button
{
public:
	dropdown(int id, rect local_bounds, std::vector<std::string> options = {}, int selected = 0);

	// ===== Options =====
	void set_options(std::vector<std::string> options, int selected = 0);   // closes the list
	const std::vector<std::string>& options() const;
	int option_count() const;
	int selected() const;                       // -1 when there are no options
	const std::string& selected_text() const;   // "" when there are no options
	void set_selected(int index);               // clamped; fires no callback

	// Fired when a pick changes the selection (picking the current option only closes the list)
	void set_on_change(selection_change_callback cb);
	void set_list_render_handler(render_handler handler);

	// ===== List layout =====
	void set_max_visible_rows(int rows);   // default 8; more options scroll
	void set_row_height(int h);            // default 15
	void set_list_padding(int p);          // default 3: the list's bevel, inside it on every side
	void set_list_gap(int g);              // default 1: between the field and the list
	void set_scrollbar_width(int w);       // default 15; taken from the rows only while the list scrolls

	// ===== Open list state (for the list render handler) =====
	bool is_open() const;
	void open();
	void close();
	bool opens_upward() const;
	rect list_bounds() const;         // screen rect of the list, bevel included
	int first_visible() const;        // option index of the top row
	int visible_rows() const;
	rect row_bounds(int index) const; // screen rect of a visible option's row
	int hot_row() const;              // option under the mouse or chosen by the keys; -1 none
	bool is_scrollable() const;
	rect scrollbar_bounds() const;    // screen rect of the scrollbar; {} when the list does not scroll
	float scroll_pos() const;         // 0 top .. 1 bottom

protected:
	void fire_click() override;   // opens or closes the list

private:
	friend class control_collection;

	// One frame of the open list on the raw input (the collection masks it from every other control).
	// Returns the option picked this frame, else -1; the list is then closed and the collection calls
	// commit(index) last, after the click sound.
	int update_list(const input_state& input, const input_state& prev_input);
	void commit(int index);
	void render_list() const;

	int max_first() const;
	void scroll_to(int first);
	void make_visible(int index);
	int row_at(int x, int y) const;

	std::vector<std::string> m_options;
	int m_selected = -1;
	selection_change_callback m_on_change;
	render_handler m_list_handler;

	int m_max_rows = 8;
	int m_row_h = 15;
	int m_pad = 3;
	int m_gap = 1;
	int m_scrollbar_w = 15;

	bool m_open = false;
	int m_first = 0;
	int m_hot = -1;
	int m_armed = -1;          // row pressed, picked if released over it
	bool m_dragging = false;   // the scrollbar follows the mouse
	int m_wheel_rest = 0;      // wheel delta short of a notch
};

} // namespace cc
