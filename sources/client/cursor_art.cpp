// cursor_art.cpp: High-resolution software cursor art
//
//////////////////////////////////////////////////////////////////////

#include "cursor_art.h"
#include "renderer.h"
#include "texture.h"
#include "json.hpp"
#include "log.h"

#include <algorithm>
#include <fstream>

using json = nlohmann::json;

cursor_art::cursor_art() = default;

cursor_art::~cursor_art() = default;

bool cursor_art::load(hb::shared::render::IRenderer& renderer, const std::string& png_path, const std::string& json_path)
{
	unload();

	std::ifstream file(json_path);
	if (!file.is_open())
	{
		hb::logger::warn("cursor_art: {} not found, no cursor will be drawn", json_path);
		return false;
	}

	std::vector<frame_info> frames;
	try
	{
		json root = json::parse(file);
		for (const json& entry : root.at("frames"))
		{
			int index = entry.at("frame").get<int>();
			const json& src = entry.at("src");
			const json& hotspot = entry.at("hotspot");
			const json& logical = entry.at("logical_size");
			if (index < 0 || index > 255)
				continue;

			frame_info info;
			info.src_x = src.at(0).get<float>();
			info.src_y = src.at(1).get<float>();
			info.src_w = src.at(2).get<float>();
			info.src_h = src.at(3).get<float>();
			info.hotspot_x = hotspot.at(0).get<float>();
			info.hotspot_y = hotspot.at(1).get<float>();
			float logical_w = logical.at(0).get<float>();
			float logical_h = logical.at(1).get<float>();
			if (info.src_w <= 0.0f || info.src_h <= 0.0f || logical_w <= 0.0f || logical_h <= 0.0f)
				continue;

			// Same logical size as the original frame: uniform scale that fits the HQ figure inside it
			info.scale = (std::min)(logical_w / info.src_w, logical_h / info.src_h);
			info.valid = true;

			if (static_cast<size_t>(index) >= frames.size())
				frames.resize(static_cast<size_t>(index) + 1);
			frames[static_cast<size_t>(index)] = info;
		}
	}
	catch (const json::exception& e)
	{
		hb::logger::warn("cursor_art: invalid {} ({}), no cursor will be drawn", json_path, e.what());
		return false;
	}

	// Display texture: smooth with mipmaps, since the art is drawn well below its source size
	std::unique_ptr<hb::shared::render::ITexture> texture(renderer.create_texture_from_file(png_path.c_str()));
	if (!texture)
	{
		hb::logger::warn("cursor_art: {} not loaded, no cursor will be drawn", png_path);
		return false;
	}

	m_texture = std::move(texture);
	m_frames = std::move(frames);
	return true;
}

void cursor_art::unload()
{
	m_texture.reset();
	m_frames.clear();
}

bool cursor_art::draw(hb::shared::render::IRenderer& renderer, int frame, int x, int y) const
{
	if (!m_texture || frame < 0 || static_cast<size_t>(frame) >= m_frames.size())
		return false;

	const frame_info& info = m_frames[static_cast<size_t>(frame)];
	if (!info.valid)
		return false;

	// The figure's hotspot lands on (x, y); the UI view scales the logical rect to the window
	renderer.draw_texture_region(m_texture.get(), info.src_x, info.src_y, info.src_w, info.src_h,
		static_cast<float>(x) - info.hotspot_x * info.scale,
		static_cast<float>(y) - info.hotspot_y * info.scale,
		info.src_w * info.scale, info.src_h * info.scale);
	return true;
}
