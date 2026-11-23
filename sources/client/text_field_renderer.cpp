#include "text_field_renderer.h"
#include "textbox.h"
#include "game_fonts.h"
#include "text_lib_ext.h"
#include "renderer_factory.h"
#include "common_types.h"

namespace hb::client {

void draw_text_field(const cc::text_input& field, uint32_t time_ms,
                     const hb::shared::text::TextStyle& valid_style,
                     const hb::shared::text::TextStyle& invalid_style)
{
	auto sb = field.screen_bounds();
	std::string display = field.display_text();

	// Determine style — check if this is a textbox with validation
	auto style = valid_style;
	if (auto* tb = dynamic_cast<const cc::textbox*>(&field))
	{
		if (!tb->is_valid())
			style = invalid_style;
	}

	// Draw selection highlight if focused and has selection
	if (field.is_focused() && field.has_selection())
	{
		int sel_start = field.selection_start();
		int sel_end = field.selection_end();

		// Measure text up to selection boundaries for pixel positions
		std::string pre_sel = display.substr(0, sel_start);
		std::string sel_text = display.substr(sel_start, sel_end - sel_start);

		auto pre_metrics = hb::shared::text::measure_text(GameFont::Default, pre_sel.c_str());
		auto sel_metrics = hb::shared::text::measure_text(GameFont::Default, sel_text.c_str());
		int line_height = hb::shared::text::get_line_height(GameFont::Default);

		int sel_x = sb.x + pre_metrics.width;
		int sel_w = sel_metrics.width;

		auto* renderer = hb::shared::render::Renderer::get();
		if (renderer)
		{
			renderer->draw_rect_filled(sel_x, sb.y, sel_w, line_height,
				hb::shared::render::Color(80, 120, 200, 160));
		}
	}

	// Draw the text
	hb::shared::text::draw_text(GameFont::Default, sb.x, sb.y, display.c_str(), style);

	// Draw cursor if focused and visible
	if (field.is_focused() && field.is_cursor_visible(time_ms))
	{
		std::string pre_cursor = display.substr(0, field.cursor_pos());
		auto metrics = hb::shared::text::measure_text(GameFont::Default, pre_cursor.c_str());
		int cursor_x = sb.x + metrics.width;

		hb::shared::text::draw_text(GameFont::Default, cursor_x, sb.y, "_",
			hb::shared::text::TextStyle::with_shadow(GameColors::InputNormal));
	}
}

} // namespace hb::client
