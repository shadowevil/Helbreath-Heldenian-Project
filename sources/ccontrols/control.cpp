#include "control.h"

namespace cc {

control::control(int id, rect local_bounds)
	: m_id(id)
	, m_local_bounds(local_bounds)
{
}

int control::id() const { return m_id; }

const rect& control::local_bounds() const { return m_local_bounds; }
void control::set_local_bounds(rect r) { m_local_bounds = r; }

void control::set_horizontal_alignment(align::horizontal h, int margin)
{
	m_h_align = h;
	m_h_margin = margin;
}

void control::set_vertical_alignment(align::vertical v, int margin)
{
	m_v_align = v;
	m_v_margin = margin;
}

align::horizontal control::horizontal_alignment() const { return m_h_align; }
align::vertical control::vertical_alignment() const { return m_v_align; }
int control::horizontal_margin() const { return m_h_margin; }
int control::vertical_margin() const { return m_v_margin; }

void control::set_opacity(float o)
{
	m_opacity = o < 0.0f ? 0.0f : (o > 1.0f ? 1.0f : o);
}

float control::opacity() const { return m_opacity; }

void control::set_flow(flow_direction dir) { m_flow = dir; }
flow_direction control::flow() const { return m_flow; }

void control::set_padding(int p) { m_padding = p; }
int control::padding() const { return m_padding; }

void control::set_allow_overlap(bool allow) { m_allow_overlap = allow; }
bool control::allow_overlap() const { return m_allow_overlap; }

int control::resolved_x() const
{
	// If parent has horizontal flow and this control participates
	if (m_parent && m_parent->m_flow == flow_direction::horizontal && !m_allow_overlap)
	{
		int x = 0;
		for (const auto& sibling : m_parent->m_children)
		{
			if (sibling.get() == this) break;
			if (sibling->m_allow_overlap) continue;
			x += sibling->m_local_bounds.w + sibling->m_padding;
		}
		return x + m_h_margin;
	}

	// Cross axis or no flow — use alignment
	if (m_h_align == align::horizontal::none)
		return m_local_bounds.x;

	int parent_w = m_parent ? m_parent->m_local_bounds.w : 0;
	switch (m_h_align)
	{
	case align::horizontal::left:   return m_h_margin;
	case align::horizontal::center: return (parent_w - m_local_bounds.w) / 2 + m_h_margin;
	case align::horizontal::right:  return parent_w - m_local_bounds.w - m_h_margin;
	default:                        return m_local_bounds.x;
	}
}

int control::resolved_y() const
{
	// If parent has vertical flow and this control participates
	if (m_parent && m_parent->m_flow == flow_direction::vertical && !m_allow_overlap)
	{
		int y = 0;
		for (const auto& sibling : m_parent->m_children)
		{
			if (sibling.get() == this) break;
			if (sibling->m_allow_overlap) continue;
			y += sibling->m_local_bounds.h + sibling->m_padding;
		}
		return y + m_v_margin;
	}

	// Cross axis or no flow — use alignment
	if (m_v_align == align::vertical::none)
		return m_local_bounds.y;

	int parent_h = m_parent ? m_parent->m_local_bounds.h : 0;
	switch (m_v_align)
	{
	case align::vertical::top:    return m_v_margin;
	case align::vertical::center: return (parent_h - m_local_bounds.h) / 2 + m_v_margin;
	case align::vertical::bottom: return parent_h - m_local_bounds.h - m_v_margin;
	default:                      return m_local_bounds.y;
	}
}

point control::screen_offset() const
{
	point offset{0, 0};
	const control* p = m_parent;
	while (p)
	{
		offset.x += p->resolved_x();
		offset.y += p->resolved_y();
		p = p->m_parent;
	}
	return offset;
}

rect control::screen_bounds() const
{
	auto off = screen_offset();
	return {resolved_x() + off.x, resolved_y() + off.y,
	        m_local_bounds.w, m_local_bounds.h};
}

bool control::is_hovered() const { return m_hovered; }
bool control::is_focused() const { return m_focused; }
bool control::is_pressed() const { return m_pressed; }
bool control::is_held() const { return m_held; }
bool control::is_highlighted() const { return m_hovered || m_focused || m_held; }
bool control::is_enabled() const { return m_enabled; }
bool control::is_visible() const { return m_visible; }

bool control::is_effectively_enabled() const
{
	if (!m_enabled) return false;
	if (m_parent) return m_parent->is_effectively_enabled();
	return true;
}

bool control::is_effectively_visible() const
{
	if (!m_visible) return false;
	if (m_parent) return m_parent->is_effectively_visible();
	return true;
}

void control::set_enabled(bool e) { m_enabled = e; }
void control::set_visible(bool v) { m_visible = v; }

void control::set_render_handler(render_handler handler) { m_render_handler = std::move(handler); }
bool control::has_render_handler() const { return static_cast<bool>(m_render_handler); }

control* control::parent() const { return m_parent; }

control* control::find(int id)
{
	if (m_id == id) return this;
	for (auto& child : m_children)
	{
		auto* result = child->find(id);
		if (result) return result;
	}
	return nullptr;
}

const control* control::find(int id) const
{
	if (m_id == id) return this;
	for (const auto& child : m_children)
	{
		const auto* result = child->find(id);
		if (result) return result;
	}
	return nullptr;
}

const std::vector<std::unique_ptr<control>>& control::children() const { return m_children; }

void control::update(const input_state& input, const input_state& prev_input)
{
	if (!is_effectively_visible() || !is_effectively_enabled())
	{
		m_hovered = false;
		m_pressed = false;
		m_held = false;
		return;
	}

	auto sb = screen_bounds();
	m_hovered = rect_contains(sb, input.mouse_x, input.mouse_y);

	bool mouse_just_down = input.mouse_left_down && !prev_input.mouse_left_down;
	m_pressed = m_hovered && mouse_just_down;

	// Held requires a press edge on this control — stays true while mouse is
	// down (even if cursor moves outside), clears on mouse release.
	// This prevents spurious held states at suppression boundaries.
	if (m_pressed)
		m_held = true;
	else if (!input.mouse_left_down)
		m_held = false;

	for (auto& child : m_children)
		child->update(input, prev_input);
}

void control::render() const
{
	if (!is_effectively_visible())
		return;

	if (m_render_handler)
		m_render_handler(*this);

	for (const auto& child : m_children)
		child->render();
}

void control::set_tooltip(const char* text) { m_tooltip = text; }
const char* control::tooltip() const { return m_tooltip; }

void control::set_hovered(bool h) { m_hovered = h; }
void control::set_focused(bool f) { m_focused = f; }
void control::set_pressed(bool p) { m_pressed = p; }
void control::set_held(bool h) { m_held = h; }

} // namespace cc
