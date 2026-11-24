// parchment_kit.h: The Map item's parchment map, composed at runtime from the map kit
//
// Kit files (tools/parchment_map/build_kit.py) under <sprite_dir>/mapkit/:
//   paper.png, icons.json (frames by name, each naming its sheet), the sheets it names (icons.png,
//   placeholders.png), and per map <map>/ink.png and <map>/placements.json.
// placements.json holds the kit square's size (1200), tile_to_px (origin + (tile + 0.5) * px_per_tile)
// and the icons already laid out as kit-pixel rects in draw order, so drawing is a straight scale.
// Draw order: paper, ink, icons; the caller draws the player marker on top.
//////////////////////////////////////////////////////////////////////

#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace hb::shared::render
{
	class IRenderer;
	class ITexture;
}

namespace hb::client
{
	class parchment_kit
	{
	public:
		parchment_kit();
		~parchment_kit();

		// True when the kit has placements for map_name (lowercased); reads nothing but the file system
		static bool has_map(const std::string& sprite_dir, const std::string& map_name);

		// Drops what was loaded, then loads map_name's kit. False (and empty) on any missing or bad file.
		bool load(hb::shared::render::IRenderer& renderer, const std::string& sprite_dir, const std::string& map_name);
		void unload();
		bool is_loaded() const { return m_ink != nullptr; }
		const std::string& map_name() const { return m_map_name; }

		// Paper, ink and icons into the square at (x, y), size units wide. alpha is opacity (1 = opaque).
		void draw(hb::shared::render::IRenderer& renderer, int x, int y, int size, float alpha = 1.0f) const;

		// Centre of a tile in the square at (x, y), size units wide; kept inside the framed area (a zoomed-in
		// interior frames only part of its map). False when nothing is loaded.
		bool tile_to_square(int x, int y, int size, int tile_x, int tile_y, float& out_x, float& out_y) const;

	private:
		struct icon
		{
			hb::shared::render::ITexture* sheet = nullptr;   // owned by m_sheets
			float src_x = 0.0f, src_y = 0.0f, src_w = 0.0f, src_h = 0.0f;   // sheet pixels
			float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;                   // kit pixels
		};

		std::string m_map_name;
		std::unique_ptr<hb::shared::render::ITexture> m_paper;
		std::unique_ptr<hb::shared::render::ITexture> m_ink;
		std::map<std::string, std::unique_ptr<hb::shared::render::ITexture>> m_sheets;
		std::vector<icon> m_icons;
		float m_size = 0.0f;          // kit square in kit pixels
		float m_px_per_tile = 0.0f;
		float m_origin_x = 0.0f;
		float m_origin_y = 0.0f;
		float m_box_lo = 0.0f;        // the framed area (kit pixels), both axes
		float m_box_hi = 0.0f;
	};
}
