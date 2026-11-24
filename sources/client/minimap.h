// minimap.h: Two-tier minimap for the corner guide map
//
// Loads sprites/minimaps/<map>/02.jpg (mid) and 03.jpg (coarsest, the whole map); the finest level's
// role belongs to the parchment map view (d-033). The widget shows a fixed window of 170 level pixels
// along the long side (the native size of level 03), centred on the player and clamped to the level,
// mapped into the widget. So the coarse tier shows the whole map and the mid one about a third.
// A map without art leaves the minimap empty: nothing is drawn and the caller treats the area as world.
//////////////////////////////////////////////////////////////////////

#pragma once

#include <array>
#include <memory>
#include <string>

namespace hb::shared::render
{
	class IRenderer;
	class ITexture;
}

namespace hb::client
{
	class minimap
	{
	public:
		static constexpr int tier_count = 2;
		static constexpr int tier_full_map = 0;    // level 03, the whole map
		static constexpr int tier_mid = 1;         // level 02

		minimap();
		~minimap();

		// Drops the previous textures, then loads the two levels of map_name (lowercased) from
		// <sprite_dir>/minimaps/<map>/. A missing folder or any level failing leaves it empty.
		void load(hb::shared::render::IRenderer& renderer, const std::string& sprite_dir, const std::string& map_name);
		void unload();
		bool is_empty() const { return m_levels[0] == nullptr; }

		// 0 = whole map, 1 = mid; clamped
		void set_zoom_level(int level);
		int get_zoom_level() const { return m_zoom_level; }

		// Draw into the square widget at (widget_x, widget_y), widget_size units wide, centred on the player.
		// alpha is opacity (1 = opaque).
		void draw(hb::shared::render::IRenderer& renderer, int widget_x, int widget_y, int widget_size,
			int map_tiles_x, int map_tiles_y, int player_tile_x, int player_tile_y, float alpha = 1.0f) const;

		// The exact mappings draw() uses (same window, same clamp). Widget units are UI units.
		// tile_to_widget gives the centre of the tile and false when it is outside the drawn window.
		bool tile_to_widget(int widget_x, int widget_y, int widget_size, int map_tiles_x, int map_tiles_y,
			int player_tile_x, int player_tile_y, int tile_x, int tile_y, float& out_x, float& out_y) const;
		// false when the point is outside the drawn map area (the letterbox included)
		bool widget_to_tile(int widget_x, int widget_y, int widget_size, int map_tiles_x, int map_tiles_y,
			int player_tile_x, int player_tile_y, float point_x, float point_y, int& out_tile_x, int& out_tile_y) const;

	private:
		// The window for one frame, in level pixels (source) and widget units (dest)
		struct view
		{
			bool valid = false;
			int level = 0;
			float px_per_tile_x = 0.0f;  // level pixels per tile
			float px_per_tile_y = 0.0f;
			float source_x = 0.0f;       // fractional window origin in the level
			float source_y = 0.0f;
			float window_w = 0.0f;       // window size in level pixels
			float window_h = 0.0f;
			float scale = 0.0f;          // widget units per level pixel
			float dest_x = 0.0f;         // where the window sits in the widget (letterboxed)
			float dest_y = 0.0f;
		};

		view make_view(int widget_x, int widget_y, int widget_size, int map_tiles_x, int map_tiles_y,
			int player_tile_x, int player_tile_y) const;
		int level_index() const;

		// index 0 = 02.jpg (mid), 1 = 03.jpg (whole map)
		std::array<std::unique_ptr<hb::shared::render::ITexture>, tier_count> m_levels;
		int m_zoom_level = tier_mid;
	};
}
