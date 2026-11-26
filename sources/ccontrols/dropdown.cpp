#include "dropdown.h"

#include <algorithm>

namespace cc {

namespace {

constexpr int wheel_notch = 120;

bool edge(bool now, bool before)
{
	return now && !before;
}

} // namespace

dropdown::dropdown(int id, rect local_bounds, std::vector<std::string> options, int selected)
	: button(id, local_bounds)
{
	set_options(std::move(options), selected);
}

// ===== Options =====

void dropdown::set_options(std::vector<std::string> options, int selected)
{
	m_options = std::move(options);
	close();
	set_selected(selected);
}

const std::vector<std::string>& dropdown::options() const { return m_options; }
int dropdown::option_count() const { return static_cast<int>(m_options.size()); }
int dropdown::selected() const { return m_selected; }

const std::string& dropdown::selected_text() const
{
	static const std::string none;
	return m_selected >= 0 ? m_options[m_selected] : none;
}

void dropdown::set_selected(int index)
{
	m_selected = m_options.empty() ? -1 : std::clamp(index, 0, option_count() - 1);
}

void dropdown::set_on_change(selection_change_callback cb) { m_on_change = std::move(cb); }
void dropdown::set_list_render_handler(render_handler handler) { m_list_handler = std::move(handler); }

// ===== List layout =====

void dropdown::set_max_visible_rows(int rows) { m_max_rows = std::max(rows, 1); }
void dropdown::set_row_height(int h) { m_row_h = std::max(h, 1); }
void dropdown::set_list_padding(int p) { m_pad = std::max(p, 0); }
void dropdown::set_list_gap(int g) { m_gap = g; }
void dropdown::set_scrollbar_width(int w) { m_scrollbar_w = std::max(w, 0); }

// ===== Open list state =====

bool dropdown::is_open() const { return m_open; }

void dropdown::open()
{
	if (m_options.empty())
		return;
	m_open = true;
	m_hot = -1;
	m_armed = -1;
	m_dragging = false;
	m_wheel_rest = 0;
	scroll_to(m_selected - visible_rows() / 2);
}

void dropdown::close()
{
	m_open = false;
	m_hot = -1;
	m_armed = -1;
	m_dragging = false;
}

int dropdown::visible_rows() const { return std::min(option_count(), m_max_rows); }
int dropdown::first_visible() const { return m_first; }
int dropdown::hot_row() const { return m_hot; }
bool dropdown::is_scrollable() const { return option_count() > m_max_rows; }
int dropdown::max_first() const { return std::max(option_count() - m_max_rows, 0); }

float dropdown::scroll_pos() const
{
	const int last = max_first();
	return last > 0 ? static_cast<float>(m_first) / static_cast<float>(last) : 0.0f;
}

rect dropdown::list_bounds() const
{
	// Below the field, or above it when below would leave the screen (the root's size; a collection
	// without set_screen_size always opens below)
	const control* top = this;
	while (top->parent())
		top = top->parent();
	const int screen_h = top->local_bounds().h;
	const rect f = screen_bounds();
	const int h = visible_rows() * m_row_h + 2 * m_pad;
	const bool up = screen_h > 0 && f.y + f.h + m_gap + h > screen_h && f.y - m_gap - h >= 0;
	return {f.x, up ? f.y - m_gap - h : f.y + f.h + m_gap, f.w, h};
}

bool dropdown::opens_upward() const
{
	return list_bounds().y < screen_bounds().y;
}

rect dropdown::row_bounds(int index) const
{
	const rect l = list_bounds();
	const int w = l.w - 2 * m_pad - (is_scrollable() ? m_scrollbar_w : 0);
	return {l.x + m_pad, l.y + m_pad + (index - m_first) * m_row_h, w, m_row_h};
}

rect dropdown::scrollbar_bounds() const
{
	if (!is_scrollable())
		return {};
	const rect l = list_bounds();
	return {l.x + l.w - m_pad - m_scrollbar_w, l.y + m_pad, m_scrollbar_w, l.h - 2 * m_pad};
}

// ===== Input =====

void dropdown::fire_click()
{
	if (!is_effectively_enabled())
		return;
	if (m_open)
		close();
	else
		open();
}

void dropdown::scroll_to(int first)
{
	m_first = std::clamp(first, 0, max_first());
}

void dropdown::make_visible(int index)
{
	if (index < m_first)
		scroll_to(index);
	else if (index >= m_first + visible_rows())
		scroll_to(index - visible_rows() + 1);
}

int dropdown::row_at(int x, int y) const
{
	const rect r = row_bounds(m_first);
	if (x < r.x || x >= r.x + r.w || y < r.y || y >= r.y + visible_rows() * m_row_h)
		return -1;
	return m_first + (y - r.y) / m_row_h;
}

int dropdown::update_list(const input_state& input, const input_state& prev_input)
{
	const int mx = input.mouse_x;
	const int my = input.mouse_y;
	const bool pressed = edge(input.mouse_left_down, prev_input.mouse_left_down);
	const bool released = edge(prev_input.mouse_left_down, input.mouse_left_down);

	// Keys: Esc closes, Up / Down move the hot row (from the current option), Enter or Space picks it
	if (edge(input.key_escape, prev_input.key_escape))
	{
		close();
		return -1;
	}
	if (edge(input.key_up, prev_input.key_up) || edge(input.key_down, prev_input.key_down))
	{
		const int step = input.key_up ? -1 : 1;
		m_hot = m_hot < 0 ? m_selected : std::clamp(m_hot + step, 0, option_count() - 1);
		make_visible(m_hot);
	}
	if (edge(input.key_enter, prev_input.key_enter) || edge(input.key_space, prev_input.key_space))
	{
		const int pick = m_hot;
		close();
		return pick;
	}

	// Wheel: a row per whole notch, away from the user scrolls up
	m_wheel_rest += input.mouse_wheel - prev_input.mouse_wheel;
	const int notches = m_wheel_rest / wheel_notch;
	m_wheel_rest -= notches * wheel_notch;
	scroll_to(m_first - notches);

	// Scrollbar drag: the top row follows the mouse down the bar
	if (pressed && rect_contains(scrollbar_bounds(), mx, my))
		m_dragging = true;
	if (!input.mouse_left_down)
		m_dragging = false;
	if (m_dragging)
	{
		const rect bar = scrollbar_bounds();
		scroll_to((my - bar.y) * (max_first() + 1) / std::max(bar.h, 1));
	}

	// The mouse sets the hot row when it moves or the rows move under it; the keys keep theirs otherwise
	const int row = row_at(mx, my);
	if (notches != 0 || m_dragging || mx != prev_input.mouse_x || my != prev_input.mouse_y)
		m_hot = row;

	if (pressed && !m_dragging)
	{
		if (row >= 0)
			m_armed = row;
		else if (!rect_contains(list_bounds(), mx, my))
		{
			close();   // the field included: a click on it closes the list too
			return -1;
		}
	}
	if (released)
	{
		const int armed = m_armed;
		m_armed = -1;
		if (armed >= 0 && armed == row)
		{
			close();
			return armed;
		}
	}
	return -1;
}

void dropdown::commit(int index)
{
	const bool changed = index != m_selected;
	m_selected = index;
	// Last: the callback may rebuild the screen's controls
	if (changed && m_on_change)
		m_on_change(id(), index);
}

void dropdown::render_list() const
{
	if (m_list_handler)
		m_list_handler(*this);
}

} // namespace cc
