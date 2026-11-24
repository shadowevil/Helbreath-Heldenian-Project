// minimap.cpp: Two-tier minimap for the corner guide map
//
//////////////////////////////////////////////////////////////////////

#include "minimap.h"
#include "renderer.h"
#include "texture.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace hb::client
{
	namespace
	{
		// Level pixels shown along the long side: the widget-native size of level 03
		constexpr float window_pixels = 170.0f;
		constexpr float tile_centre = 0.5f;

		struct span
		{
			int first;
			int count;
			float lead_in;  // source pixels between the first whole pixel and the true start
		};

		// The whole source pixels covering [start, start + length), kept inside the level; the dest is
		// pulled back by the lead-in and clipped, so the view slides by sub-pixel steps
		span cover_span(float start, float length, int level_extent)
		{
			const int first = std::clamp(static_cast<int>(std::floor(start)), 0, level_extent);
			const int last = std::clamp(static_cast<int>(std::ceil(start + length)), first, level_extent);
			return span{first, last - first, start - static_cast<float>(first)};
		}
	}

	minimap::minimap() = default;
	minimap::~minimap() = default;

	void minimap::unload()
	{
		for (auto& level : m_levels)
			level.reset();
	}

	void minimap::load(hb::shared::render::IRenderer& renderer, const std::string& sprite_dir, const std::string& map_name)
	{
		unload();

		std::string folder = map_name;
		std::transform(folder.begin(), folder.end(), folder.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		if (folder.empty())
			return;

		// SDL's level files are 01 (finest) .. 03 (whole map); 01 is not shipped (d-033)
		constexpr int first_level_file = 2;
		for (int i = 0; i < tier_count; i++)
		{
			const std::string path = sprite_dir + "/minimaps/" + folder + "/0" + std::to_string(i + first_level_file) + ".jpg";
			m_levels[i].reset(renderer.create_texture_from_file(path.c_str()));
			if (!m_levels[i] || m_levels[i]->get_width() == 0 || m_levels[i]->get_height() == 0)
			{
				// no art for this map (an interior, say) or a damaged set: stay empty
				unload();
				return;
			}
		}
	}

	void minimap::set_zoom_level(int level)
	{
		m_zoom_level = std::clamp(level, 0, tier_count - 1);
	}

	int minimap::level_index() const
	{
		return tier_count - 1 - m_zoom_level;
	}

	minimap::view minimap::make_view(int widget_x, int widget_y, int widget_size, int map_tiles_x, int map_tiles_y,
		int player_tile_x, int player_tile_y) const
	{
		view v;
		if (is_empty() || map_tiles_x <= 0 || map_tiles_y <= 0 || widget_size <= 0)
			return v;

		v.level = level_index();
		const float level_w = static_cast<float>(m_levels[v.level]->get_width());
		const float level_h = static_cast<float>(m_levels[v.level]->get_height());
		v.px_per_tile_x = level_w / static_cast<float>(map_tiles_x);
		v.px_per_tile_y = level_h / static_cast<float>(map_tiles_y);
		v.scale = static_cast<float>(widget_size) / window_pixels;
		v.window_w = std::min(window_pixels, level_w);
		v.window_h = std::min(window_pixels, level_h);

		// A window centred on the player, clamped to the level: the one clamp gives the whole map at the
		// coarsest tier and a sliding view whose marker leaves the centre only at the map edges
		const float player_px = (static_cast<float>(player_tile_x) + tile_centre) * v.px_per_tile_x;
		const float player_py = (static_cast<float>(player_tile_y) + tile_centre) * v.px_per_tile_y;
		v.source_x = std::clamp(player_px - v.window_w / 2.0f, 0.0f, level_w - v.window_w);
		v.source_y = std::clamp(player_py - v.window_h / 2.0f, 0.0f, level_h - v.window_h);

		// Letterbox a window smaller than the widget (a non-square map at the coarsest tier)
		v.dest_x = static_cast<float>(widget_x) + (static_cast<float>(widget_size) - v.window_w * v.scale) / 2.0f;
		v.dest_y = static_cast<float>(widget_y) + (static_cast<float>(widget_size) - v.window_h * v.scale) / 2.0f;
		v.valid = true;
		return v;
	}

	void minimap::draw(hb::shared::render::IRenderer& renderer, int widget_x, int widget_y, int widget_size,
		int map_tiles_x, int map_tiles_y, int player_tile_x, int player_tile_y, float alpha) const
	{
		const view v = make_view(widget_x, widget_y, widget_size, map_tiles_x, map_tiles_y, player_tile_x, player_tile_y);
		if (!v.valid)
			return;

		const auto& texture = m_levels[v.level];
		const span span_x = cover_span(v.source_x, v.window_w, texture->get_width());
		const span span_y = cover_span(v.source_y, v.window_h, texture->get_height());
		if (span_x.count <= 0 || span_y.count <= 0)
			return;

		// Clip to the window; the covering pixels overhang it by the lead-in
		const int clip_x = static_cast<int>(std::floor(v.dest_x));
		const int clip_y = static_cast<int>(std::floor(v.dest_y));
		const int clip_w = static_cast<int>(std::ceil(v.dest_x + v.window_w * v.scale)) - clip_x;
		const int clip_h = static_cast<int>(std::ceil(v.dest_y + v.window_h * v.scale)) - clip_y;
		const auto old_clip = renderer.get_clip_area();
		renderer.set_clip_area(clip_x, clip_y, clip_w, clip_h);
		renderer.draw_texture_region(texture.get(),
			static_cast<float>(span_x.first), static_cast<float>(span_y.first),
			static_cast<float>(span_x.count), static_cast<float>(span_y.count),
			v.dest_x - span_x.lead_in * v.scale, v.dest_y - span_y.lead_in * v.scale,
			static_cast<float>(span_x.count) * v.scale, static_cast<float>(span_y.count) * v.scale, alpha);
		renderer.set_clip_area(old_clip.x, old_clip.y, old_clip.width, old_clip.height);
	}

	bool minimap::tile_to_widget(int widget_x, int widget_y, int widget_size, int map_tiles_x, int map_tiles_y,
		int player_tile_x, int player_tile_y, int tile_x, int tile_y, float& out_x, float& out_y) const
	{
		const view v = make_view(widget_x, widget_y, widget_size, map_tiles_x, map_tiles_y, player_tile_x, player_tile_y);
		if (!v.valid)
			return false;

		const float px = (static_cast<float>(tile_x) + tile_centre) * v.px_per_tile_x;
		const float py = (static_cast<float>(tile_y) + tile_centre) * v.px_per_tile_y;
		if (px < v.source_x || px >= v.source_x + v.window_w || py < v.source_y || py >= v.source_y + v.window_h)
			return false;

		out_x = v.dest_x + (px - v.source_x) * v.scale;
		out_y = v.dest_y + (py - v.source_y) * v.scale;
		return true;
	}

	bool minimap::widget_to_tile(int widget_x, int widget_y, int widget_size, int map_tiles_x, int map_tiles_y,
		int player_tile_x, int player_tile_y, float point_x, float point_y, int& out_tile_x, int& out_tile_y) const
	{
		const view v = make_view(widget_x, widget_y, widget_size, map_tiles_x, map_tiles_y, player_tile_x, player_tile_y);
		if (!v.valid)
			return false;

		const float rel_x = (point_x - v.dest_x) / v.scale;
		const float rel_y = (point_y - v.dest_y) / v.scale;
		if (rel_x < 0.0f || rel_x >= v.window_w || rel_y < 0.0f || rel_y >= v.window_h)
			return false;

		out_tile_x = std::clamp(static_cast<int>(std::floor((v.source_x + rel_x) / v.px_per_tile_x)), 0, map_tiles_x - 1);
		out_tile_y = std::clamp(static_cast<int>(std::floor((v.source_y + rel_y) / v.px_per_tile_y)), 0, map_tiles_y - 1);
		return true;
	}
}
