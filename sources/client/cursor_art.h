// cursor_art.h: High-resolution software cursor art
//
// Draws the cursor frames from sprites/interface/cursors_hq.png (frame table in cursors_hq.json)
// at the original cursor frame's logical size and hotspot, sampled smoothly from the high-resolution
// source. A frame without HQ art, or a missing/invalid asset, makes draw() return false so the
// caller draws the original sprite.
//////////////////////////////////////////////////////////////////////

#pragma once

#include <memory>
#include <string>
#include <vector>

namespace hb::shared::render
{
	class IRenderer;
	class ITexture;
}

class cursor_art
{
public:
	cursor_art();
	~cursor_art();

	// Loads the sheet and table; fails soft (returns false, draw() then always returns false)
	bool load(hb::shared::render::IRenderer& renderer, const std::string& png_path, const std::string& json_path);
	void unload();
	bool is_loaded() const { return m_texture != nullptr; }

	// Draw frame with its hotspot at (x, y) in logical units; false when the frame has no HQ art
	bool draw(hb::shared::render::IRenderer& renderer, int frame, int x, int y) const;

private:
	struct frame_info
	{
		bool valid = false;
		float src_x = 0.0f;
		float src_y = 0.0f;
		float src_w = 0.0f;
		float src_h = 0.0f;
		float hotspot_x = 0.0f;  // source pixels from the src top-left
		float hotspot_y = 0.0f;
		float scale = 0.0f;      // logical units per source pixel (uniform fit into the original frame)
	};

	std::unique_ptr<hb::shared::render::ITexture> m_texture;
	std::vector<frame_info> m_frames;
};
