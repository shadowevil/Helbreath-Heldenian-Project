// panel_kit.cpp: Dialog panels and text buttons composed at runtime from the staged panel kit
//
//////////////////////////////////////////////////////////////////////

#include "panel_kit.h"
#include "renderer.h"
#include "texture.h"
#include "text_renderer.h"
#include "text_lib.h"
#include "button.h"
#include "json.hpp"
#include "log.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <functional>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using json = nlohmann::json;
using hb::shared::render::Color;

namespace hb::client
{
	namespace
	{
		// Python's // and ceil division, for negative numerators too
		int floor_div(int a, int b)
		{
			int q = a / b;
			if ((a % b != 0) && ((a < 0) != (b < 0)))
				q--;
			return q;
		}

		int ceil_div(int a, int b)
		{
			return -floor_div(-a, b);
		}

		int positive_mod(int a, int b)
		{
			return ((a % b) + b) % b;
		}

		// [r, g, b], each 0..255 (a value outside it is a bad kit, not a colour to wrap)
		Color color_of(const json& value)
		{
			uint8_t c[3] = {};
			for (std::size_t i = 0; i < 3; i++)
			{
				const int v = value.at(i).get<int>();
				if (v < 0 || v > 255)
					throw std::runtime_error("colour channel out of range: " + std::to_string(v));
				c[i] = static_cast<uint8_t>(v);
			}
			return Color(c[0], c[1], c[2]);
		}

		Color scaled(const Color& c, float k)
		{
			return Color(static_cast<uint8_t>(c.r * k), static_cast<uint8_t>(c.g * k), static_cast<uint8_t>(c.b * k));
		}

		// fn(word) for each space-separated word, as views into text (no allocation)
		template <typename F>
		void for_each_word(std::string_view text, F&& fn)
		{
			std::size_t i = 0;
			while (i < text.size())
			{
				while (i < text.size() && text[i] == ' ')
					i++;
				std::size_t j = i;
				while (j < text.size() && text[j] != ' ')
					j++;
				if (j > i)
					fn(text.substr(i, j - i));
				i = j;
			}
		}

		constexpr kit_point dialog_sizes[] = {{270, 376}, {364, 162}, {331, 303}};

		// A button's rect at position: the style's size, or from the text where the style leaves it 0
		kit_rect button_rect(const panel_kit& kit, const button_style& s, std::string_view text, kit_point position)
		{
			const int w = s.w > 0 ? s.w : kit.text_width(s.recipe, text) + 2 * s.auto_pad;
			return {position.x, position.y, w, s.h > 0 ? s.h : 20};
		}
	}

	struct panel_kit::kit_data
	{
		struct piece
		{
			float x = 0.0f, y = 0.0f;
			int w = 0, h = 0;
		};

		struct text_draw
		{
			int dx = 0, dy = 0;
			Color color;
			float fill_scale = -1.0f;   // >= 0: the caller's fill times this
			float alpha = 1.0f;
			bool additive = false;
			bool bold = false;
		};

		// Ink box [left, right) x [top, bottom) of a run relative to its origin
		struct ink_rect
		{
			int left = 0, top = 0, right = 0, bottom = 0;
		};

		struct text_recipe
		{
			// Ink boxes of the texts drawn so far, by text (string_view lookup: nothing is built per frame).
			// A std::map, so an entry's key stays put and its c_str() can be handed to draw_run.
			mutable std::map<std::string, ink_rect, std::less<>> inks;
			int face = -1;
			int px = 0;
			float tracking = 0.0f;
			float bold_dx = 0.0f;
			bool bold = false;          // any draw bold: inks include bold_dx
			bool per_word = false;
			int word_gap = 0;
			int cap_mid = 0;            // centre of the cap height ("H") below the run origin
			Color fill{255, 255, 255};
			Color fill_hover{255, 255, 255};
			std::vector<text_draw> draws;
		};

		struct border_style
		{
			std::string top, bottom, left, right, shadow;
			int thickness = 0;
			std::vector<float> alpha_top_left, alpha_bottom_right;
		};

		struct corner_style
		{
			std::string tl, tr, bl, br;
		};

		struct flourish_spec
		{
			const piece* mask = nullptr;   // <name>_white, resolved at load (the pieces map is fixed by then)
			float scale = 1.0f;
			Color tint;
			bool additive = false;
			float opacity = 1.0f;
			bool emboss = false;
			Color emboss_color;
		};

		std::unique_ptr<hb::shared::render::ITexture> sheet;          // nearest: every piece
		std::unique_ptr<hb::shared::render::ITexture> sheet_smooth;   // smooth: scaled flourishes
		std::map<std::string, piece, std::less<>> pieces;
		std::map<std::string, std::string, std::less<>> backgrounds;
		std::map<std::string, border_style, std::less<>> borders;
		std::map<std::string, corner_style, std::less<>> corners;
		std::map<std::string, flourish_spec, std::less<>> flourishes;
		std::map<std::string, text_recipe, std::less<>> recipes;

		std::string inset_bevel, raised_bevel;
		int inset_margin = 3, raised_margin = 3;
		std::string rule_dark, rule_light, vrule_dark, vrule_light;
		int rule_height = 2, vrule_width = 2;
		std::string ring;
		std::string scroll_thumb;
		int scroll_track_w = 6;
		float scroll_darken = 0.8f;
		int label_gap = 4;
		button_style small, wide;

		// A button frame's 8 pieces, resolved once per frame prefix
		struct frame_set
		{
			const piece* tl = nullptr;
			const piece* t = nullptr;
			const piece* tr = nullptr;
			const piece* l = nullptr;
			const piece* r = nullptr;
			const piece* bl = nullptr;
			const piece* b = nullptr;
			const piece* br = nullptr;
		};

		// Memos (draw-path lookups and text measurement, filled on first use; never input state)
		mutable std::map<std::string, frame_set, std::less<>> frame_sets;

		const frame_set& frames_of(const std::string& prefix) const
		{
			auto it = frame_sets.find(prefix);
			if (it == frame_sets.end())
			{
				frame_set f{find(prefix + "_tl"), find(prefix + "_t"), find(prefix + "_tr"), find(prefix + "_l"),
					find(prefix + "_r"), find(prefix + "_bl"), find(prefix + "_b"), find(prefix + "_br")};
				it = frame_sets.emplace(prefix, f).first;
			}
			return it->second;
		}

		const piece* find(std::string_view name) const
		{
			auto it = pieces.find(name);
			return it == pieces.end() ? nullptr : &it->second;
		}

		const piece& need(const std::string& name) const
		{
			const piece* p = find(name);
			if (!p)
				throw std::runtime_error("missing frame " + name);
			return *p;
		}

		const text_recipe* recipe(std::string_view name) const
		{
			auto it = recipes.find(name);
			return it == recipes.end() ? nullptr : &it->second;
		}

		// ---------------------------------------------------------- the reference composer's primitives

		hb::shared::render::IRenderer* renderer = nullptr;      // not owned
		hb::shared::text::ITextRenderer* text = nullptr;        // not owned

		// Part of a piece (sx, sy, sw, sh in piece pixels) into a destination rect
		void region(const piece& p, int sx, int sy, int sw, int sh, int x, int y, int w, int h, float alpha) const
		{
			renderer->draw_texture_region(sheet.get(), p.x + static_cast<float>(sx), p.y + static_cast<float>(sy),
				static_cast<float>(sw), static_cast<float>(sh),
				static_cast<float>(x), static_cast<float>(y), static_cast<float>(w), static_cast<float>(h), alpha);
		}

		void draw(const piece* p, int x, int y, float alpha) const
		{
			if (p)
				region(*p, 0, 0, p->w, p->h, x, y, p->w, p->h, alpha);
		}

		void draw(std::string_view name, int x, int y, float alpha) const
		{
			draw(find(name), x, y, alpha);
		}

		void stretch(std::string_view name, int x, int y, int w, int h, float alpha) const
		{
			const piece* p = find(name);
			if (p && w > 0 && h > 0)
				region(*p, 0, 0, p->w, p->h, x, y, w, h, alpha);
		}

		// Repeat the piece over the rect, the tile grid starting at the origin, clipped to the rect
		void tile(std::string_view name, int x, int y, int w, int h, int origin_x, int origin_y, float alpha) const
		{
			tile(find(name), x, y, w, h, origin_x, origin_y, alpha);
		}

		void tile(const piece* p, int x, int y, int w, int h, int origin_x, int origin_y, float alpha) const
		{
			if (!p || w <= 0 || h <= 0 || p->w <= 0 || p->h <= 0)
				return;
			const int ox = positive_mod(x - origin_x, p->w);
			const int oy = positive_mod(y - origin_y, p->h);
			for (int ty = y - oy; ty < y + h; ty += p->h)
			{
				const int y0 = (std::max)(ty, y);
				const int y1 = (std::min)(ty + p->h, y + h);
				for (int tx = x - ox; tx < x + w; tx += p->w)
				{
					const int x0 = (std::max)(tx, x);
					const int x1 = (std::min)(tx + p->w, x + w);
					region(*p, x0 - tx, y0 - ty, x1 - x0, y1 - y0, x0, y0, x1 - x0, y1 - y0, alpha);
				}
			}
		}

		void nine_slice(std::string_view name, int x, int y, int w, int h, int m, float alpha) const
		{
			const piece* p = find(name);
			if (!p)
				return;
			const int xs[3][4] = {{0, m, 0, m}, {m, p->w - m, m, w - m}, {p->w - m, p->w, w - m, w}};
			const int ys[3][4] = {{0, m, 0, m}, {m, p->h - m, m, h - m}, {p->h - m, p->h, h - m, h}};
			for (const auto& yr : ys)
			{
				for (const auto& xr : xs)
				{
					if (xr[3] > xr[2] && yr[3] > yr[2])
						region(*p, xr[0], yr[0], xr[1] - xr[0], yr[1] - yr[0],
							x + xr[2], y + yr[2], xr[3] - xr[2], yr[3] - yr[2], alpha);
				}
			}
		}

		std::string_view tile_of(std::string_view background) const
		{
			auto it = backgrounds.find(background);
			return it == backgrounds.end() ? std::string_view() : std::string_view(it->second);
		}

		// The field tiles from origin: the panel's (1, 1) to line up with its cloth (kit_dialog passes it),
		// else the inset's own (x + 1, y + 1)
		void inset(const kit_rect& r, std::string_view field, float darken, float alpha, std::optional<kit_point> origin = {}) const
		{
			const kit_point o = origin.value_or(kit_point{r.x + 1, r.y + 1});
			if (!field.empty())
				tile(tile_of(field), r.x + 1, r.y + 1, r.w - 2, r.h - 2, o.x, o.y, alpha);
			if (darken > 0.0f)
				stretch("px_black", r.x + 1, r.y + 1, r.w - 2, r.h - 2, (1.0f - darken) * alpha);
			nine_slice(inset_bevel, r.x, r.y, r.w, r.h, inset_margin, alpha);
		}

		// Flourish size at a scale (the mask resized and rounded, as the reference does)
		kit_point flourish_size(std::string_view name, float scale) const
		{
			auto it = flourishes.find(name);
			if (it == flourishes.end())
				return {};
			const piece* p = it->second.mask;
			const float sc = scale > 0.0f ? scale : it->second.scale;
			return {static_cast<int>(std::lround(p->w * sc)), static_cast<int>(std::lround(p->h * sc))};
		}

		void flourish(std::string_view name, int cx, int cy, bool mirror, float scale, float alpha,
			const flourish_style* style) const
		{
			auto it = flourishes.find(name);
			if (it == flourishes.end())
				return;
			const flourish_spec& spec = it->second;
			const bool additive = style ? style->additive : spec.additive;
			const piece* p = spec.mask;
			const kit_point size = flourish_size(name, scale);
			const float sx = mirror ? p->x + static_cast<float>(p->w) : p->x;
			const float sw = mirror ? -static_cast<float>(p->w) : static_cast<float>(p->w);
			const int x = cx - size.x / 2;
			const int y = cy - size.y / 2;
			auto put = [&](int dx, int dy, const Color& tint, bool additive, float opacity)
			{
				renderer->draw_texture_region(sheet_smooth.get(), sx, p->y, sw, static_cast<float>(p->h),
					static_cast<float>(x + dx), static_cast<float>(y + dy), static_cast<float>(size.x), static_cast<float>(size.y),
					opacity * alpha, {tint, additive ? hb::shared::render::BlendMode::Additive : hb::shared::render::BlendMode::Alpha});
			};
			// The JSON's look unless the caller overrides it (tint, blend and opacity; an override has no emboss)
			if (!style && spec.emboss && !additive)
				put(1, 1, spec.emboss_color, false, 1.0f);
			if (style)
				put(0, 0, style->tint, additive, style->opacity);
			else
				put(0, 0, spec.tint, additive, spec.opacity);
		}

		// ---------------------------------------------------------- text recipes

		using ink_entry = std::pair<const std::string, ink_rect>;

		// The run's ink box in recipe r, faux bold included when the recipe is bold, measured once per text;
		// the entry's key is the text, kept alive for draw_run
		const ink_entry& ink(const text_recipe& r, std::string_view s) const
		{
			auto it = r.inks.find(s);
			if (it != r.inks.end())
				return *it;
			std::string key(s);
			const hb::shared::text::ink_box box = text->measure_run(key.c_str(), {r.face, r.px, r.tracking});
			const float bold = r.bold ? r.bold_dx : 0.0f;
			const ink_rect rect{static_cast<int>(std::floor(box.left)), static_cast<int>(std::floor(box.top)),
				static_cast<int>(std::ceil(box.right + bold)), static_cast<int>(std::ceil(box.bottom))};
			return *r.inks.emplace(std::move(key), rect).first;
		}

		void passes(const text_recipe& r, const std::string& s, int ox, int oy, const Color& fill, float alpha) const
		{
			const hb::shared::text::text_run run{r.face, r.px, r.tracking};
			for (const text_draw& d : r.draws)
			{
				Color c = d.fill_scale >= 0.0f ? scaled(fill, d.fill_scale) : d.color;
				float a = std::clamp(d.alpha * alpha, 0.0f, 1.0f);
				// text_fx emboldens one mask (the union of both copies) and draws it once at a; two copies
				// at a would stack to 1 - (1 - a)^2 where they overlap (2a when additive). Each copy at
				// 1 - sqrt(1 - a) (a / 2) stacks back to a there, which is nearly all of the glyph.
				if (d.bold)
					a = d.additive ? a * 0.5f : 1.0f - std::sqrt(1.0f - a);
				c.a = static_cast<uint8_t>(a * 255.0f);
				const auto blend = d.additive ? hb::shared::render::BlendMode::Additive : hb::shared::render::BlendMode::Alpha;
				const float x = static_cast<float>(ox + d.dx);
				const float y = static_cast<float>(oy + d.dy);
				text->draw_run(x, y, s.c_str(), run, c, blend);
				if (d.bold)
					text->draw_run(x + r.bold_dx, y, s.c_str(), run, c, blend);
			}
		}

		int width(const text_recipe& r, std::string_view s) const
		{
			if (!text)
				return 0;
			if (!r.per_word)
			{
				const ink_rect& i = ink(r, s).second;
				return i.right - i.left;
			}
			int total = 0;
			int words = 0;
			for_each_word(s, [&](std::string_view w)
			{
				const ink_rect& i = ink(r, w).second;
				total += i.right - i.left;
				words++;
			});
			return total + r.word_gap * (words > 0 ? words - 1 : 0);
		}

		// The run origin's y for ink box i: the cap height ("H") centred on y, or the ink box itself for middle
		static int origin_y(const text_recipe& r, const ink_rect& i, int y, text_anchor::type anchor)
		{
			if (anchor != text_anchor::middle || r.per_word)
				return y - r.cap_mid;
			return y - floor_div(i.top + i.bottom, 2);
		}

		void draw_text(const text_recipe& r, std::string_view s, int x, int y, text_anchor::type anchor,
			const Color& fill, float alpha) const
		{
			if (!text || s.empty())
				return;

			if (!r.per_word)
			{
				const auto& [run, i] = ink(r, s);
				const int ox = anchor == text_anchor::lcap ? x - i.left
					: anchor == text_anchor::rcap ? x - i.right : x - floor_div(i.left + i.right, 2);
				passes(r, run, ox, origin_y(r, i, y, anchor), fill, alpha);
				return;
			}

			// Per word: ink boxes laid out left to right with word_gap between them, the run anchored
			const int oy = y - r.cap_mid;
			const int total = width(r, s);
			int cx = anchor == text_anchor::lcap ? x : anchor == text_anchor::rcap ? x - total : x - ceil_div(total, 2);
			for_each_word(s, [&](std::string_view w)
			{
				const auto& [word, i] = ink(r, w);
				passes(r, word, cx - i.left, oy, fill, alpha);
				cx += i.right - i.left + r.word_gap;
			});
		}
	};

	panel_kit::panel_kit() = default;
	panel_kit::~panel_kit() = default;

	void panel_kit::unload()
	{
		m_data.reset();
	}

	bool panel_kit::load(hb::shared::render::IRenderer& renderer, const std::string& sprite_dir)
	{
		unload();
		const std::string dir = sprite_dir + "/panelkit";
		const std::string json_path = dir + "/panel_kit.json";
		std::ifstream file(json_path);
		if (!file.is_open())
		{
			hb::logger::warn("panel_kit: {} not found", json_path);
			return false;
		}

		auto data = std::make_unique<kit_data>();
		try
		{
			const json doc = json::parse(file);
			std::string sheet_name;
			for (const auto& [name, f] : doc.at("frames").items())
			{
				sheet_name = f.at("image").get<std::string>();
				data->pieces[name] = {f.at("x").get<float>(), f.at("y").get<float>(), f.at("w").get<int>(), f.at("h").get<int>()};
			}
			if (sheet_name.empty())
				throw std::runtime_error("no frames");

			for (const auto& [name, b] : doc.at("backgrounds").items())
			{
				const std::string tile = b.at("tile").get<std::string>();
				data->need(tile);
				data->backgrounds[name] = tile;
			}

			for (const auto& [name, b] : doc.at("border_styles").items())
			{
				kit_data::border_style s;
				s.top = b.at("top").get<std::string>();
				s.bottom = b.at("bottom").get<std::string>();
				s.left = b.at("left").get<std::string>();
				s.right = b.at("right").get<std::string>();
				s.thickness = b.at("thickness").get<int>();
				const json& sh = b.at("inner_shadow");
				s.shadow = sh.at("piece").get<std::string>();
				s.alpha_top_left = sh.at("alpha_top_left").get<std::vector<float>>();
				s.alpha_bottom_right = sh.at("alpha_bottom_right").get<std::vector<float>>();
				const std::size_t rows = sh.at("rows").get<std::size_t>();
				if (s.alpha_top_left.size() < rows || s.alpha_bottom_right.size() < rows)
					throw std::runtime_error("border " + name + ": fewer shadow alphas than rows");
				s.alpha_top_left.resize(rows);
				s.alpha_bottom_right.resize(rows);
				for (const std::string* n : {&s.top, &s.bottom, &s.left, &s.right, &s.shadow})
					data->need(*n);
				data->borders[name] = std::move(s);
			}

			for (const auto& [name, c] : doc.at("corner_styles").items())
			{
				kit_data::corner_style s{c.at("tl").get<std::string>(), c.at("tr").get<std::string>(),
					c.at("bl").get<std::string>(), c.at("br").get<std::string>()};
				for (const std::string* n : {&s.tl, &s.tr, &s.bl, &s.br})
					data->need(*n);
				data->corners[name] = std::move(s);
			}

			const json& inner = doc.at("inner");
			data->inset_bevel = inner.at("inset").at("bevel").get<std::string>();
			data->inset_margin = inner.at("inset").at("margin").get<int>();
			data->raised_bevel = inner.at("raised").at("bevel").get<std::string>();
			data->raised_margin = inner.at("raised").at("margin").get<int>();
			data->rule_dark = inner.at("rule").at("dark").get<std::string>();
			data->rule_light = inner.at("rule").at("light").get<std::string>();
			data->rule_height = inner.at("rule").at("height").get<int>();
			data->vrule_dark = inner.at("vrule").at("dark").get<std::string>();
			data->vrule_light = inner.at("vrule").at("light").get<std::string>();
			data->vrule_width = inner.at("vrule").at("width").get<int>();
			data->ring = inner.at("slot_ring").at("piece").get<std::string>();
			data->scroll_thumb = inner.at("scrollbar").at("thumb").get<std::string>();
			data->scroll_track_w = inner.at("scrollbar").at("track_w").get<int>();
			data->scroll_darken = inner.at("scrollbar").at("groove_darken").get<float>();
			for (const std::string* n : {&data->inset_bevel, &data->raised_bevel, &data->rule_dark, &data->rule_light,
				&data->vrule_dark, &data->vrule_light, &data->ring, &data->scroll_thumb})
				data->need(*n);
			data->need("px_black");

			for (const auto& [name, f] : doc.at("flourishes").items())
			{
				if (!f.is_object())
					continue;   // notes
				kit_data::flourish_spec s;
				s.scale = f.at("scale").get<float>();
				s.tint = color_of(f.at("tint"));
				s.additive = f.at("blend").get<std::string>() == "additive";
				s.opacity = f.at("opacity").get<float>();
				if (f.contains("emboss") && f.at("emboss").is_array())
				{
					s.emboss = true;
					s.emboss_color = color_of(f.at("emboss"));
				}
				s.mask = &data->need(name + "_white");
				data->flourishes[name] = s;
			}

			// Text recipes: only those with a TTF face (the "value" recipe is the client's default text)
			hb::shared::text::ITextRenderer* text = hb::shared::text::GetTextRenderer();
			if (!text)
				throw std::runtime_error("no text renderer");
			for (const auto& [name, r] : doc.at("text_recipes").items())
			{
				if (!r.is_object() || !r.contains("font"))
					continue;
				const std::string font = r.at("font").get<std::string>();
				if (font.size() < 4 || font.substr(font.size() - 4) != ".ttf" || font.find_first_of("/\\") != std::string::npos)
					continue;
				kit_data::text_recipe rec;
				// "smooth": linear glyph sampling (else nearest); "subpixel": keep a fractional bold_dx
				rec.face = text->load_face(("fonts/" + font).c_str(), r.value("smooth", true));
				if (rec.face < 0)
					throw std::runtime_error("cannot load fonts/" + font);
				rec.px = r.at("px").get<int>();
				rec.tracking = r.value("tracking", 0.0f);
				rec.bold_dx = r.value("bold_dx", 0.0f);
				if (!r.value("subpixel", true))
					rec.bold_dx = std::round(rec.bold_dx);
				rec.per_word = r.value("per_word", false);
				rec.word_gap = r.value("word_gap", 0);
				const hb::shared::text::ink_box cap = text->measure_run("H", {rec.face, rec.px, rec.tracking});
				rec.cap_mid = floor_div(static_cast<int>(std::floor(cap.top)) + static_cast<int>(std::ceil(cap.bottom)), 2);
				if (r.contains("fill"))
					rec.fill = color_of(r.at("fill"));
				rec.fill_hover = r.contains("fill_hover") ? color_of(r.at("fill_hover")) : rec.fill;
				const bool all_bold = r.value("bold_passes", std::string()) == "all";
				for (const json& d : r.at("draws"))
				{
					kit_data::text_draw draw;
					draw.dx = d.value("dx", 0);
					draw.dy = d.value("dy", 0);
					draw.alpha = d.value("alpha", 1.0f);
					draw.additive = d.value("blend", std::string("normal")) == "additive";
					draw.bold = rec.bold_dx > 0.0f && (all_bold || d.value("bold", false));
					const json& col = d.at("color");
					if (col.is_string())
					{
						// "fill" or "fill x <k>"
						const std::string spec = col.get<std::string>();
						const std::size_t x = spec.find(" x ");
						draw.fill_scale = (x == std::string::npos) ? 1.0f : std::stof(spec.substr(x + 3));
					}
					else
					{
						draw.color = color_of(col);
					}
					rec.bold = rec.bold || draw.bold;
					rec.draws.push_back(draw);
				}
				if (name == "field_label")
					data->label_gap = r.value("gap_to_value_box", 4);
				data->recipes[name] = std::move(rec);
			}

			// Predefined button layouts from the "buttons" block
			const json& b = doc.at("buttons");
			const std::string tl = b.at("frame").at("tl").get<std::string>();
			if (tl.size() < 3 || tl.substr(tl.size() - 3) != "_tl")
				throw std::runtime_error("button frame tl is not <prefix>_tl");
			button_style base;
			base.face_normal = b.at("states").at("normal").at("face").get<std::string>();
			base.face_hover = b.at("states").at("hover").at("face").get<std::string>();
			base.frame = tl.substr(0, tl.size() - 3);
			base.cap_left = b.at("caps").at("left").get<std::string>();
			base.cap_right = b.at("caps").at("right").get<std::string>();
			base.cap_inset = b.at("caps").at("inset").get<int>();
			for (const char* edge : {"_tl", "_t", "_tr", "_l", "_r", "_bl", "_b", "_br"})
				data->need(base.frame + edge);
			for (const std::string* n : {&base.face_normal, &base.face_hover, &base.cap_left, &base.cap_right})
				data->need(*n);
			data->small = base;
			data->wide = base;
			data->small.recipe = b.at("label_styles").at("small").get<std::string>();
			data->wide.recipe = b.at("label_styles").at("wide").get<std::string>();
			data->small.w = b.at("sizes").at("small").at(0).get<int>();
			data->small.h = b.at("sizes").at("small").at(1).get<int>();
			data->wide.w = b.at("sizes").at("wide").at(0).get<int>();
			data->wide.h = b.at("sizes").at("wide").at(1).get<int>();
			if (!data->recipe(data->small.recipe) || !data->recipe(data->wide.recipe))
				throw std::runtime_error("button label recipe missing");

			const std::string sheet_path = dir + "/" + sheet_name;
			data->sheet.reset(renderer.create_texture_from_file(sheet_path.c_str(), hb::shared::render::texture_filter::nearest));
			data->sheet_smooth.reset(renderer.create_texture_from_file(sheet_path.c_str(), hb::shared::render::texture_filter::smooth));
			if (!data->sheet || !data->sheet_smooth || data->sheet->get_width() == 0)
				throw std::runtime_error("cannot load " + sheet_path);
		}
		catch (const std::exception& e)
		{
			hb::logger::warn("panel_kit: invalid kit in {} ({})", dir, e.what());
			return false;
		}

		data->renderer = &renderer;
		data->text = hb::shared::text::GetTextRenderer();
		m_data = std::move(data);
		return true;
	}

	const button_style& panel_kit::style_of(button_layout::type layout) const
	{
		static const button_style none;
		if (!m_data)
			return none;
		return layout == button_layout::wide ? m_data->wide : m_data->small;
	}

	kit_point panel_kit::size_of(dialog_size::type size)
	{
		return dialog_sizes[size <= dialog_size::sys_menu ? size : dialog_size::character];
	}

	void panel_kit::compose(int x, int y, int w, int h, const panel_style& style, float alpha) const
	{
		if (!m_data || w <= 0 || h <= 0)
			return;
		const kit_data& k = *m_data;
		if (!style.background.empty())
			k.tile(k.tile_of(style.background), x + 1, y + 1, w - 2, h - 2, x + 1, y + 1, alpha);

		auto border = k.borders.find(style.border);
		if (border != k.borders.end())
		{
			const kit_data::border_style& b = border->second;
			const int t = b.thickness;
			for (std::size_t i = 0; i < b.alpha_top_left.size(); i++)
			{
				const int n = static_cast<int>(i);
				const float a1 = b.alpha_top_left[i] * alpha;
				const float a2 = b.alpha_bottom_right[i] * alpha;
				k.stretch(b.shadow, x + t, y + t + n, w - 2 * t, 1, a1);
				k.stretch(b.shadow, x + t, y + h - t - n - 1, w - 2 * t, 1, a2);
				k.stretch(b.shadow, x + t + n, y + t, 1, h - 2 * t, a1);
				k.stretch(b.shadow, x + w - t - n - 1, y + t, 1, h - 2 * t, a2);
			}
			k.tile(b.left, x, y, t, h, x, y, alpha);
			k.tile(b.right, x + w - t, y, t, h, x + w - t, y, alpha);
			k.tile(b.top, x, y, w, t, x, y, alpha);
			k.tile(b.bottom, x, y + h - t, w, t, x, y + h - t, alpha);
		}

		auto corner = k.corners.find(style.corner);
		if (corner != k.corners.end())
		{
			const kit_data::corner_style& c = corner->second;
			const kit_data::piece& tr = k.need(c.tr);
			const kit_data::piece& bl = k.need(c.bl);
			const kit_data::piece& br = k.need(c.br);
			k.draw(c.tl, x, y, alpha);
			k.draw(c.tr, x + w - tr.w, y, alpha);
			k.draw(c.bl, x, y + h - bl.h, alpha);
			k.draw(c.br, x + w - br.w, y + h - br.h, alpha);
		}
	}

	void panel_kit::inset(const kit_rect& r, std::string_view field, float darken, float alpha,
		std::optional<kit_point> tile_origin) const
	{
		if (m_data)
			m_data->inset(r, field, darken, alpha, tile_origin);
	}

	void panel_kit::raised(const kit_rect& r, float alpha) const
	{
		if (m_data)
			m_data->nine_slice(m_data->raised_bevel, r.x, r.y, r.w, r.h, m_data->raised_margin, alpha);
	}

	void panel_kit::rule(int x, int y, int w, float alpha) const
	{
		if (!m_data)
			return;
		const int h = m_data->rule_height;
		for (int i = 0; i < h; i++)
			m_data->stretch(i < h / 2 ? m_data->rule_dark : m_data->rule_light, x, y + i, w, 1, alpha);
	}

	void panel_kit::vrule(int x, int y, int h, float alpha) const
	{
		if (!m_data)
			return;
		const int w = m_data->vrule_width;
		for (int i = 0; i < w; i++)
			m_data->stretch(i < w / 2 ? m_data->vrule_dark : m_data->vrule_light, x + i, y, 1, h, alpha);
	}

	void panel_kit::slot_ring(int cx, int cy, float alpha) const
	{
		if (!m_data)
			return;
		const kit_data::piece& ring = m_data->need(m_data->ring);
		m_data->draw(m_data->ring, cx - ring.w / 2, cy - ring.h / 2, alpha);
	}

	void panel_kit::scrollbar(const kit_rect& r, float pos, float alpha) const
	{
		if (!m_data)
			return;
		const int tw = m_data->scroll_track_w;
		groove({r.x + (r.w - tw) / 2, r.y, tw, r.h}, alpha);
		const kit_data::piece& thumb = m_data->need(m_data->scroll_thumb);
		const int ty = r.y + static_cast<int>(std::lround((r.h - thumb.h) * std::clamp(pos, 0.0f, 1.0f)));
		m_data->draw(m_data->scroll_thumb, r.x + (r.w - thumb.w) / 2, ty, alpha);
	}

	void panel_kit::groove(const kit_rect& r, float alpha) const
	{
		if (m_data)
			m_data->inset(r, {}, m_data->scroll_darken, alpha);
	}

	void panel_kit::thumb(int cx, int cy, float alpha) const
	{
		if (!m_data)
			return;
		const kit_data::piece& t = m_data->need(m_data->scroll_thumb);
		m_data->draw(m_data->scroll_thumb, cx - t.w / 2, cy - t.h / 2, alpha);
	}

	bool panel_kit::piece(std::string_view name, int x, int y, float alpha) const
	{
		const kit_data::piece* p = m_data ? m_data->find(name) : nullptr;
		if (!p)
			return false;
		m_data->draw(p, x, y, alpha);
		return true;
	}

	float panel_kit::scrollbar_pos_at(const kit_rect& r, int y) const
	{
		if (!m_data)
			return 0.0f;
		const int th = m_data->need(m_data->scroll_thumb).h;
		if (r.h <= th)
			return 0.0f;
		return std::clamp(static_cast<float>(y - r.y - th / 2) / static_cast<float>(r.h - th), 0.0f, 1.0f);
	}

	void panel_kit::flourish(std::string_view name, int cx, int cy, bool mirror, float scale, float alpha,
		const flourish_style* style) const
	{
		if (m_data)
			m_data->flourish(name, cx, cy, mirror, scale, alpha, style);
	}

	kit_point panel_kit::flourish_size(std::string_view name, float scale) const
	{
		return m_data ? m_data->flourish_size(name, scale) : kit_point{};
	}

	void panel_kit::button(const button_style& style, std::string_view text, const kit_rect& r, bool hover, float alpha) const
	{
		if (!m_data)
			return;
		const kit_data& k = *m_data;
		const int x = r.x, y = r.y, w = r.w, h = r.h;
		k.tile(hover ? style.face_hover : style.face_normal, x + 1, y + 1, w - 2, h - 2, x + 1, y + 1, alpha);

		const kit_data::frame_set& f = k.frames_of(style.frame);
		if (f.t && f.b && f.l && f.r)
		{
			k.tile(f.t, x + 4, y, w - 8, f.t->h, x + 4, y, alpha);
			k.tile(f.b, x + 4, y + h - f.b->h, w - 8, f.b->h, x + 4, y + h - f.b->h, alpha);
			k.tile(f.l, x, y + 4, f.l->w, h - 8, x, y + 4, alpha);
			k.tile(f.r, x + w - f.r->w, y + 4, f.r->w, h - 8, x + w - f.r->w, y + 4, alpha);
		}
		k.draw(f.tl, x, y, alpha);
		k.draw(f.tr, x + w - 4, y, alpha);
		k.draw(f.bl, x, y + h - 4, alpha);
		k.draw(f.br, x + w - 4, y + h - 4, alpha);

		if (const kit_data::piece* cl = k.find(style.cap_left))
			k.draw(cl, x + style.cap_inset, y + (h - cl->h) / 2, alpha);
		if (const kit_data::piece* cr = k.find(style.cap_right))
			k.draw(cr, x + w - style.cap_inset - cr->w, y + (h - cr->h) / 2, alpha);

		if (const kit_data::text_recipe* recipe = k.recipe(style.recipe))
			k.draw_text(*recipe, text, x + w / 2, y + h / 2, text_anchor::cap, hover ? recipe->fill_hover : recipe->fill, alpha);
	}

	void panel_kit::text(std::string_view recipe, std::string_view text, int x, int y, text_anchor::type anchor,
		const Color& fill, float alpha) const
	{
		if (!m_data)
			return;
		if (const kit_data::text_recipe* r = m_data->recipe(recipe))
			m_data->draw_text(*r, text, x, y, anchor, fill, alpha);
	}

	Color panel_kit::recipe_fill(std::string_view recipe, bool hover) const
	{
		const kit_data::text_recipe* r = m_data ? m_data->recipe(recipe) : nullptr;
		if (!r)
			return {};
		return hover ? r->fill_hover : r->fill;
	}

	int panel_kit::label_gap() const
	{
		return m_data ? m_data->label_gap : 4;
	}

	int panel_kit::text_width(std::string_view recipe, std::string_view text) const
	{
		const kit_data::text_recipe* r = m_data ? m_data->recipe(recipe) : nullptr;
		return r ? m_data->width(*r, text) : 0;
	}

	int panel_kit::cap_middle(std::string_view recipe, std::string_view text, int y, text_anchor::type anchor) const
	{
		const kit_data::text_recipe* r = m_data ? m_data->recipe(recipe) : nullptr;
		if (!r || !m_data->text)
			return y;
		return kit_data::origin_y(*r, m_data->ink(*r, text).second, y, anchor) + r->cap_mid;
	}

	hb::shared::text::text_run panel_kit::recipe_run(std::string_view recipe) const
	{
		const kit_data::text_recipe* r = m_data ? m_data->recipe(recipe) : nullptr;
		if (!r)
			return {};
		return {r->face, r->px, r->tracking};
	}

	kit_rect panel_kit::text_ink(std::string_view recipe, std::string_view text, int x, int y, text_anchor::type anchor) const
	{
		const kit_data::text_recipe* r = m_data ? m_data->recipe(recipe) : nullptr;
		if (!r || !m_data->text || r->per_word || text.empty())
			return {};
		// The origin draw_text uses for a single run
		const kit_data::ink_rect& i = m_data->ink(*r, text).second;
		const int ox = anchor == text_anchor::lcap ? x - i.left
			: anchor == text_anchor::rcap ? x - i.right : x - floor_div(i.left + i.right, 2);
		const int oy = kit_data::origin_y(*r, i, y, anchor);
		return {ox + i.left, oy + i.top, i.right - i.left, i.bottom - i.top};
	}

	// ------------------------------------------------------------------ kit_button

	kit_button::kit_button(button_layout::type layout, std::string text, kit_point position)
		: m_layout(layout)
		, m_text(std::move(text))
		, m_position(position)
	{
	}

	kit_button::kit_button(button_style style, std::string text, kit_point position)
		: m_style(std::move(style))
		, m_text(std::move(text))
		, m_position(position)
	{
	}

	const button_style& kit_button::style(const panel_kit& kit) const
	{
		return m_style ? *m_style : kit.style_of(m_layout);
	}

	kit_rect kit_button::rect(const panel_kit& kit) const
	{
		return button_rect(kit, style(kit), m_text, m_position);
	}

	void kit_button::draw(const panel_kit& kit, bool hover, float alpha) const
	{
		draw_at(kit, m_position, hover, alpha);
	}

	void kit_button::draw_at(const panel_kit& kit, kit_point position, bool hover, float alpha) const
	{
		kit_rect r = rect(kit);
		r.x = position.x;
		r.y = position.y;
		kit.button(style(kit), m_text, r, hover, alpha);
	}

	cc::render_handler kit_button::render_handler(const panel_kit& kit) const
	{
		return [&kit, self = *this](const cc::control& c)
		{
			const cc::rect sb = c.screen_bounds();
			self.draw_at(kit, {sb.x, sb.y}, c.is_highlighted(), c.opacity());
		};
	}

	cc::button* kit_button::add_to(cc::control& parent, int id, const panel_kit& kit) const
	{
		const kit_rect r = rect(kit);
		cc::button* control = parent.add<cc::button>(id, cc::rect{r.x, r.y, r.w, r.h});
		control->set_render_handler(render_handler(kit));
		return control;
	}

	// ------------------------------------------------------------------ kit_dialog

	kit_dialog::kit_dialog(const panel_kit& kit, dialog_size::type size, kit_point position, float alpha,
		const panel_style& style)
		: kit_dialog(kit, panel_kit::size_of(size), position, alpha, style)
	{
	}

	kit_dialog::kit_dialog(const panel_kit& kit, kit_point size, kit_point position, float alpha, const panel_style& style)
		: m_kit(kit)
		, m_size(size)
		, m_origin(position)
		, m_alpha(alpha)
	{
		m_kit.compose(position.x, position.y, size.x, size.y, style, alpha);
	}

	void kit_dialog::title(std::string_view text, int y, const Color& fill, std::string_view flourish, int gap,
		const flourish_style* style) const
	{
		const int cx = m_origin.x + m_size.x / 2;
		const int cy = m_origin.y + y;
		m_kit.text("title_red", text, cx, cy, text_anchor::middle, fill, m_alpha);
		side_flourishes("title_red", text, cx, cy, flourish, gap, style);
	}

	void kit_dialog::side_flourishes(std::string_view recipe, std::string_view text, int cx, int cy,
		std::string_view flourish, int gap, const flourish_style* style) const
	{
		if (flourish.empty())
			return;
		// One each side, gap px from the ink, the right one mirrored, centred on the cap height of the
		// text as drawn (middle anchor), so descenders do not pull them down
		const int reach = m_kit.text_width(recipe, text) / 2 + gap + m_kit.flourish_size(flourish).x / 2;
		const int fy = m_kit.cap_middle(recipe, text, cy, text_anchor::middle);
		m_kit.flourish(flourish, cx - reach, fy, false, 0.0f, m_alpha, style);
		m_kit.flourish(flourish, cx + reach, fy, true, 0.0f, m_alpha, style);
	}

	void kit_dialog::heading(std::string_view text, int y, std::string_view flourish, int gap,
		const flourish_style* style) const
	{
		const int cx = m_origin.x + m_size.x / 2;
		const int cy = m_origin.y + y;
		m_kit.text("heading", text, cx, cy, text_anchor::middle, {}, m_alpha);
		side_flourishes("heading", text, cx, cy, flourish, gap, style);
	}

	void kit_dialog::field(std::string_view label, const kit_rect& box, std::string_view value, const Color& value_color) const
	{
		const kit_rect b = offset(box);
		m_kit.inset(b, "stone_field", 0.0f, m_alpha, tile_origin());
		m_kit.text("field_label", label, b.x - m_kit.label_gap(), b.y + b.h / 2, text_anchor::rcap, {}, m_alpha);
		if (!value.empty())
		{
			const std::string v(value);
			hb::shared::text::draw_text_aligned(hb::shared::text::FONT_ID_DEFAULT, b.x, b.y, b.w, b.h, v.c_str(),
				hb::shared::text::TextStyle(value_color, m_alpha), hb::shared::text::Align::Center);
		}
	}

	void kit_dialog::inset(const kit_rect& r, std::string_view field, float darken) const
	{
		m_kit.inset(offset(r), field, darken, m_alpha, tile_origin());
	}

	void kit_dialog::raised(const kit_rect& r) const
	{
		m_kit.raised(offset(r), m_alpha);
	}

	void kit_dialog::rule(int x, int y, int w) const
	{
		m_kit.rule(m_origin.x + x, m_origin.y + y, w, m_alpha);
	}

	void kit_dialog::vrule(int x, int y, int h) const
	{
		m_kit.vrule(m_origin.x + x, m_origin.y + y, h, m_alpha);
	}

	void kit_dialog::slot_ring(int cx, int cy) const
	{
		m_kit.slot_ring(m_origin.x + cx, m_origin.y + cy, m_alpha);
	}

	void kit_dialog::scrollbar(const kit_rect& r, float pos) const
	{
		m_kit.scrollbar(offset(r), pos, m_alpha);
	}

	float kit_dialog::scrollbar_pos_at(const kit_rect& r, int screen_y) const
	{
		return m_kit.scrollbar_pos_at(offset(r), screen_y);
	}

	void kit_dialog::groove(const kit_rect& r) const
	{
		m_kit.groove(offset(r), m_alpha);
	}

	void kit_dialog::thumb(int cx, int cy) const
	{
		m_kit.thumb(m_origin.x + cx, m_origin.y + cy, m_alpha);
	}

	bool kit_dialog::piece(std::string_view name, kit_point position) const
	{
		return m_kit.piece(name, m_origin.x + position.x, m_origin.y + position.y, m_alpha);
	}

	int kit_dialog::title_cap_middle(std::string_view text, int y) const
	{
		return m_kit.cap_middle("title_red", text, m_origin.y + y, text_anchor::middle) - m_origin.y;
	}

	void kit_dialog::flourish(std::string_view name, int cx, int cy, bool mirror, float scale,
		const flourish_style* style) const
	{
		m_kit.flourish(name, m_origin.x + cx, m_origin.y + cy, mirror, scale, m_alpha, style);
	}

	kit_rect kit_dialog::text_area(const kit_rect& r, float darken) const
	{
		const kit_rect a = offset(r);
		m_kit.inset(a, {}, darken, m_alpha, tile_origin());
		return {a.x + 3, a.y + 3, a.w - 6, a.h - 6};
	}

	void kit_dialog::button(button_layout::type layout, std::string_view text, kit_point position, bool hover) const
	{
		// Drawn directly, not through a kit_button: nothing is copied per frame
		const button_style& s = m_kit.style_of(layout);
		m_kit.button(s, text, button_rect(m_kit, s, text, {m_origin.x + position.x, m_origin.y + position.y}), hover, m_alpha);
	}

	void kit_dialog::button(const kit_button& b, bool hover) const
	{
		const kit_rect r = b.rect(m_kit);
		b.draw_at(m_kit, {m_origin.x + r.x, m_origin.y + r.y}, hover, m_alpha);
	}
}
