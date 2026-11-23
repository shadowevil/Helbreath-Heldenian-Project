#include "item_tooltip.h"

#include "game_fonts.h"
#include "common_types.h"
#include "render/renderer.h"
#include "text_lib.h"

void item_tooltip::clear()
{
	m_lines.clear();
}

void item_tooltip::add_line(const std::string& text, const hb::shared::render::Color& color)
{
	if (text.empty()) return;
	m_lines.push_back({text, color});
}

void item_tooltip::add_dual_line(const std::string& label, const hb::shared::render::Color& label_color,
	const std::string& value, const hb::shared::render::Color& value_color)
{
	if (label.empty() && value.empty()) return;
	tooltip_line line;
	line.text = label;
	line.color = label_color;
	line.value_text = value;
	line.value_color = value_color;
	line.has_value = true;
	m_lines.push_back(std::move(line));
}

void item_tooltip::draw(int x, int y, hb::shared::render::IRenderer* renderer) const
{
	if (m_lines.empty() || renderer == nullptr) return;

	int max_width = 0;
	for (const auto& line : m_lines)
	{
		int line_width = hb::shared::text::measure_text(GameFont::Default, line.text.c_str()).width;
		if (line.has_value && !line.value_text.empty())
			line_width += hb::shared::text::measure_text(GameFont::Default, line.value_text.c_str()).width;
		if (line_width > max_width) max_width = line_width;
	}

	int box_width = max_width + padding_x * 2;
	int box_height = static_cast<int>(m_lines.size()) * line_height + padding_y * 2;

	// Background
	renderer->draw_rect_filled(x, y, box_width, box_height, hb::shared::render::Color{0, 0, 0, 180});

	// Border
	renderer->draw_rect_outline(x, y, box_width, box_height, hb::shared::render::Color{120, 100, 70}, 1);

	// Text lines
	int text_y = y + padding_y;
	for (const auto& line : m_lines)
	{
		hb::shared::text::draw_text(GameFont::Default, x + padding_x, text_y, line.text.c_str(),
			hb::shared::text::TextStyle::with_shadow(line.color));

		if (line.has_value && !line.value_text.empty())
		{
			int label_width = hb::shared::text::measure_text(GameFont::Default, line.text.c_str()).width;
			hb::shared::text::draw_text(GameFont::Default, x + padding_x + label_width, text_y,
				line.value_text.c_str(), hb::shared::text::TextStyle::with_shadow(line.value_color));
		}

		text_y += line_height;
	}
}
