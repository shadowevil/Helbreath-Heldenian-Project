// copyright_notice.cpp: The two copyright lines at the bottom of the front-end screens (HH-0088)
//
//////////////////////////////////////////////////////////////////////

#include "copyright_notice.h"
#include "renderer.h"
#include "text_renderer.h"
#include "log.h"
#include <cmath>

namespace
{

// UTF-8 as explicit byte escapes, so the source compiles the same under MSVC and GCC whatever the source
// encoding or compiler flags: \xC2\xA9 is U+00A9, the copyright sign
constexpr const char* lines[] = {
	"Copyright \xC2\xA9 2004 Siementech Co., Ltd",
	"Recreation \xC2\xA9 2026 The Heldenian Project - open source",
};

constexpr int font_px = 12;                 // the default font's UI size
constexpr int pad_px = 2;                   // baked texels around the ink
constexpr float line_height = 13.0f;        // UI units per line
constexpr float bottom_margin = 12.0f;      // from the bottom of the visible area, on the version text's row
constexpr float text_alpha = 0.75f;
constexpr float shadow_alpha = 0.6f;
constexpr hb::shared::render::Color text_color{176, 166, 148};   // muted, so it reads on the dark art

// A UI position snapped to a whole window pixel, so the baked texels land 1:1
float snap(float value, float scale)
{
	return std::round(value * scale) / scale;
}

} // namespace

namespace hb::client
{

void copyright_notice::release()
{
	for (auto& line : m_lines)
		line.reset();
	m_scale = 0.0f;
}

void copyright_notice::bake(float scale)
{
	release();
	m_scale = scale;
	hb::shared::text::ITextRenderer* text = hb::shared::text::GetTextRenderer();
	if (!text)
		return;

	// Baked at the window's resolution (white; the colour is the draw's tint) and drawn back at 1/scale
	const hb::shared::text::text_run run{0, static_cast<int>(std::lround(static_cast<float>(font_px) * scale))};
	for (size_t i = 0; i < m_lines.size(); i++)
	{
		m_lines[i].reset(text->bake_run(lines[i], run, hb::shared::render::Color::White(), pad_px));
		if (!m_lines[i])
			hb::logger::error("Copyright line {} not rendered", i);
	}
}

void copyright_notice::draw(hb::shared::render::IRenderer& renderer, bool show_company)
{
	const float scale = renderer.ui_scale();
	if (m_scale == 0.0f || std::fabs(scale - m_scale) > 0.001f)
		bake(scale);

	const hb::shared::render::screen_extent extent = renderer.get_screen_extent();
	const size_t first = show_company ? 0 : 1;
	const float bottom = extent.y + extent.height - bottom_margin;
	for (size_t i = first; i < m_lines.size(); i++)
	{
		hb::shared::render::ITexture* texture = m_lines[i].get();
		if (!texture)
			continue;
		const float width = static_cast<float>(texture->get_width());
		const float height = static_cast<float>(texture->get_height());
		const float w = width / scale;
		const float h = height / scale;
		const float x = snap(extent.x + (extent.width - w) / 2.0f, scale);
		const float line_top = bottom - line_height * static_cast<float>(m_lines.size() - i);
		const float y = snap(line_top + (line_height - h) / 2.0f, scale);
		renderer.draw_texture_region(texture, 0.0f, 0.0f, width, height, x + 1.0f, y + 1.0f, w, h, shadow_alpha,
			hb::shared::render::draw_tint{hb::shared::render::Color::Black()});
		renderer.draw_texture_region(texture, 0.0f, 0.0f, width, height, x, y, w, h, text_alpha,
			hb::shared::render::draw_tint{text_color});
	}
}

} // namespace hb::client
