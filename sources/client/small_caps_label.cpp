// small_caps_label.cpp: A label in simulated small caps, baked once into textures (HH-0089)
//
//////////////////////////////////////////////////////////////////////

#include "small_caps_label.h"
#include "renderer.h"
#include "text_renderer.h"
#include "log.h"
#include <cctype>
#include <cmath>
#include <cstddef>
#include <utility>

namespace
{

constexpr int pad_px = 2;   // baked texels around the ink

namespace render = hb::shared::render;

// A UI position snapped to a whole window pixel, so the baked texels land 1:1
float snap(float value, float scale)
{
	return std::round(value * scale) / scale;
}

} // namespace

namespace hb::client
{

void small_caps_label::release()
{
	m_pieces.clear();
	m_width = 0.0f;
}

void small_caps_label::bake(const std::string& text, const style& style, float ui_scale)
{
	release();
	m_ui_scale = ui_scale;
	m_glow_intensity = style.glow_intensity;
	hb::shared::text::ITextRenderer* renderer = hb::shared::text::GetTextRenderer();
	if (!renderer || style.face <= 0)
		return;

	const int big_px = static_cast<int>(std::lround(style.big_px));
	const int small_px = static_cast<int>(std::lround(style.big_px * style.small_ratio));
	const hb::shared::text::text_run big_run{style.face, big_px, style.spacing_px};
	const hb::shared::text::text_run small_run{style.face, small_px, style.spacing_px};
	const bool glow = style.glow_dilate > 0;
	const int halo_pad = glow ? hb::shared::text::ITextRenderer::glow_pad(style.glow_dilate, style.glow_blur) : 0;

	// A piece's advance: where an "H" lands after it, less the "H"'s own ink edge. It carries the run's
	// tracking after each letter.
	const auto advance = [renderer](const std::string& piece_text, const hb::shared::text::text_run& run)
	{
		return renderer->measure_run((piece_text + "H").c_str(), run).right - renderer->measure_run("H", run).right;
	};

	float cursor = 0.0f;
	const auto add_piece = [&](const std::string& piece_text, const hb::shared::text::text_run& run)
	{
		const hb::shared::text::ink_box ink = renderer->measure_run(piece_text.c_str(), run);
		piece p;
		p.text.reset(renderer->bake_run(piece_text.c_str(), run, render::Color::White(), pad_px));
		if (glow)
			p.halo.reset(renderer->bake_glow(piece_text.c_str(), run, render::Color::White(), style.glow_dilate, style.glow_blur));
		if (!p.text)
		{
			// Leave a gap rather than shifting the following pieces left
			hb::logger::error("Label piece '{}' not rendered", piece_text);
			cursor += advance(piece_text, run);
			return;
		}
		// The text origin's baseline is run.pixel_size below the origin; the texture's ink starts pad in
		const float baseline = static_cast<float>(run.pixel_size);
		p.x = cursor + ink.left - static_cast<float>(pad_px);
		p.y = ink.top - baseline - static_cast<float>(pad_px);
		p.halo_x = cursor + ink.left - static_cast<float>(halo_pad);
		p.halo_y = ink.top - baseline - static_cast<float>(halo_pad);
		m_pieces.push_back(std::move(p));
		cursor += advance(piece_text, run);
	};

	std::size_t pos = 0;
	while (pos < text.size())
	{
		if (text[pos] == ' ')
		{
			cursor += advance(" ", small_run);
			pos++;
			continue;
		}
		const std::size_t end = text.find(' ', pos);
		std::string word = text.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
		for (char& c : word)
			c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
		add_piece(word.substr(0, 1), big_run);
		if (word.size() > 1)
			add_piece(word.substr(1), small_run);
		pos += word.size();
	}
	m_width = cursor - style.spacing_px;   // no space after the last letter
}

void small_caps_label::draw(render::IRenderer& renderer, float x, float y, anchor where,
	const render::Color& color, float alpha, float shadow, bool glow) const
{
	const float scale = renderer.ui_scale();
	const float per_px = 1.0f / m_ui_scale;   // UI units per baked texel
	const float width = m_width * per_px;
	const float left = snap(where == anchor::centre ? x - width / 2.0f : where == anchor::right ? x - width : x, scale);
	const float baseline = snap(y, scale);

	const auto put = [&](const render::ITexture* texture, float dx, float dy, float offset, const render::Color& tint, float a,
		render::BlendMode blend)
	{
		if (!texture)
			return;
		const float w = static_cast<float>(texture->get_width());
		const float h = static_cast<float>(texture->get_height());
		// draw_texture_region takes a mutable texture; the draw does not change it
		renderer.draw_texture_region(const_cast<render::ITexture*>(texture), 0.0f, 0.0f, w, h,
			left + dx * per_px + offset, baseline + dy * per_px + offset, w * per_px, h * per_px, a,
			render::draw_tint{tint, blend});
	};

	// The halo is added to the backdrop (white * its smooth falloff * intensity), so it fades to nothing
	if (glow)
	{
		for (const piece& p : m_pieces)
			put(p.halo.get(), p.halo_x, p.halo_y, 0.0f, render::Color::White(), alpha * m_glow_intensity, render::BlendMode::Additive);
	}
	for (const piece& p : m_pieces)
		put(p.text.get(), p.x, p.y, shadow, render::Color::Black(), alpha, render::BlendMode::Alpha);
	for (const piece& p : m_pieces)
		put(p.text.get(), p.x, p.y, 0.0f, color, alpha, render::BlendMode::Alpha);
}

} // namespace hb::client
