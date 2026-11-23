#pragma once

#include "render/primitive_types.h"

#include <string>
#include <vector>

namespace hb::shared::render { class IRenderer; }

struct tooltip_line
{
	std::string text;
	hb::shared::render::Color color;
	std::string value_text;
	hb::shared::render::Color value_color{0, 0, 0, 0};
	bool has_value = false;
};

class item_tooltip
{
public:
	void clear();
	void add_line(const std::string& text, const hb::shared::render::Color& color);
	void add_dual_line(const std::string& label, const hb::shared::render::Color& label_color,
		const std::string& value, const hb::shared::render::Color& value_color);
	void draw(int x, int y, hb::shared::render::IRenderer* renderer) const;
	bool empty() const { return m_lines.empty(); }

private:
	static constexpr int padding_x = 6;
	static constexpr int padding_y = 4;
	static constexpr int line_height = 15;

	std::vector<tooltip_line> m_lines;
};
