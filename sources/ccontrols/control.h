#pragma once

#include "types.h"
#include "input_state.h"
#include <vector>
#include <memory>

namespace cc {

class focus_manager;
class control_collection;

class control
{
public:
	control(int id, rect local_bounds);
	virtual ~control() = default;

	// ===== Identity =====
	int id() const;

	// ===== Bounds =====
	const rect& local_bounds() const;
	void set_local_bounds(rect r);
	rect screen_bounds() const;
	point screen_offset() const;

	// ===== State flags =====
	bool is_hovered() const;
	bool is_focused() const;
	bool is_pressed() const;
	bool is_held() const;
	bool is_highlighted() const;
	bool is_enabled() const;
	bool is_visible() const;
	bool is_effectively_enabled() const;
	bool is_effectively_visible() const;

	void set_enabled(bool e);
	void set_visible(bool v);

	// ===== Alignment =====
	void set_horizontal_alignment(align::horizontal h, int margin = 0);
	void set_vertical_alignment(align::vertical v, int margin = 0);

	align::horizontal horizontal_alignment() const;
	align::vertical vertical_alignment() const;
	int horizontal_margin() const;
	int vertical_margin() const;

	// ===== Opacity =====
	void set_opacity(float o);
	float opacity() const;

	// ===== Flow layout (parent) =====
	void set_flow(flow_direction dir);
	flow_direction flow() const;

	// ===== Padding (gap after this control in flow) =====
	void set_padding(int p);
	int padding() const;

	// ===== Overlap (opt out of parent's flow) =====
	void set_allow_overlap(bool allow);
	bool allow_overlap() const;

	// ===== Render handler =====
	void set_render_handler(render_handler handler);
	bool has_render_handler() const;

	// ===== Tree structure =====
	control* parent() const;

	template<typename T, typename... Args>
	T* add(Args&&... args);

	control* find(int id);
	const control* find(int id) const;

	template<typename T>
	T* find_as(int id) { return dynamic_cast<T*>(find(id)); }

	const std::vector<std::unique_ptr<control>>& children() const;

	template<typename Func>
	void walk(Func&& fn);

	template<typename Func>
	void walk(Func&& fn) const;

	// ===== Focus =====
	virtual bool is_focusable() const { return false; }
	virtual void on_focus_gained() {}
	virtual void on_focus_lost() {}

	// ===== Update =====
	virtual void update(const input_state& input, const input_state& prev_input);

	// ===== Render =====
	void render() const;

	// ===== Tooltip =====
	void set_tooltip(const char* text);
	const char* tooltip() const;

protected:
	void set_hovered(bool h);
	void set_focused(bool f);
	void set_pressed(bool p);
	void set_held(bool h);

	friend class focus_manager;
	friend class control_collection;

private:
	int resolved_x() const;
	int resolved_y() const;

	int m_id;
	rect m_local_bounds;
	control* m_parent = nullptr;
	std::vector<std::unique_ptr<control>> m_children;

	bool m_hovered = false;
	bool m_focused = false;
	bool m_pressed = false;
	bool m_held = false;
	bool m_enabled = true;
	bool m_visible = true;
	const char* m_tooltip = nullptr;
	render_handler m_render_handler;

	align::horizontal m_h_align = align::horizontal::none;
	align::vertical m_v_align = align::vertical::none;
	int m_h_margin = 0;
	int m_v_margin = 0;

	float m_opacity = 1.0f;

	flow_direction m_flow = flow_direction::none;
	int m_padding = 0;
	bool m_allow_overlap = false;
};

// ===== Template implementations =====

template<typename T, typename... Args>
T* control::add(Args&&... args)
{
	auto child = std::make_unique<T>(std::forward<Args>(args)...);
	T* ptr = child.get();
	child->m_parent = this;
	m_children.push_back(std::move(child));
	return ptr;
}

template<typename Func>
void control::walk(Func&& fn)
{
	fn(*this);
	for (auto& child : m_children)
		child->walk(std::forward<Func>(fn));
}

template<typename Func>
void control::walk(Func&& fn) const
{
	fn(*this);
	for (const auto& child : m_children)
		child->walk(std::forward<Func>(fn));
}

} // namespace cc
