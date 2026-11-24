// parchment_kit.cpp: The Map item's parchment map, composed at runtime from the map kit
//
//////////////////////////////////////////////////////////////////////

#include "parchment_kit.h"
#include "renderer.h"
#include "texture.h"
#include "json.hpp"
#include "log.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>

using json = nlohmann::json;

namespace hb::client
{
	namespace
	{
		constexpr float tile_centre = 0.5f;

		// The kit folder for a map: the name lowercased, or empty when it holds anything but [a-z0-9_]
		// (the name comes from the server, so a path separator or ".." never reaches the filesystem)
		std::string lowercase(std::string text)
		{
			for (char& c : text)
			{
				c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
				if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_')
					return {};
			}
			return text;
		}

		std::string kit_dir_of(const std::string& sprite_dir)
		{
			return sprite_dir + "/mapkit";
		}

		bool read_json(const std::string& path, json& out)
		{
			std::ifstream file(path);
			if (!file.is_open())
			{
				hb::logger::warn("parchment_kit: {} not found", path);
				return false;
			}
			try
			{
				out = json::parse(file);
				return true;
			}
			catch (const json::exception& e)
			{
				hb::logger::warn("parchment_kit: invalid {} ({})", path, e.what());
				return false;
			}
		}

		std::unique_ptr<hb::shared::render::ITexture> load_texture(hb::shared::render::IRenderer& renderer, const std::string& path)
		{
			std::unique_ptr<hb::shared::render::ITexture> texture(renderer.create_texture_from_file(path.c_str()));
			if (!texture || texture->get_width() == 0 || texture->get_height() == 0)
			{
				hb::logger::warn("parchment_kit: cannot load {}", path);
				return nullptr;
			}
			return texture;
		}

		// The whole texture into the square
		void draw_full(hb::shared::render::IRenderer& renderer, hb::shared::render::ITexture* texture, int x, int y, int size, float alpha)
		{
			renderer.draw_texture_region(texture, 0.0f, 0.0f,
				static_cast<float>(texture->get_width()), static_cast<float>(texture->get_height()),
				static_cast<float>(x), static_cast<float>(y), static_cast<float>(size), static_cast<float>(size), alpha);
		}
	}

	parchment_kit::parchment_kit() = default;
	parchment_kit::~parchment_kit() = default;

	bool parchment_kit::has_map(const std::string& sprite_dir, const std::string& map_name)
	{
		const std::string name = lowercase(map_name);
		if (name.empty())
			return false;
		std::error_code ec;
		return std::filesystem::is_regular_file(kit_dir_of(sprite_dir) + "/" + name + "/placements.json", ec);
	}

	void parchment_kit::unload()
	{
		m_map_name.clear();
		m_paper.reset();
		m_ink.reset();
		m_icons.clear();
		m_sheets.clear();
		m_size = 0.0f;
	}

	bool parchment_kit::load(hb::shared::render::IRenderer& renderer, const std::string& sprite_dir, const std::string& map_name)
	{
		unload();
		const std::string name = lowercase(map_name);
		if (name.empty())
			return false;

		const std::string kit_dir = kit_dir_of(sprite_dir);
		const std::string map_dir = kit_dir + "/" + name;
		json placements;
		if (!read_json(map_dir + "/placements.json", placements))
			return false;
		try
		{
			m_size = placements.at("size").get<float>();
			m_px_per_tile = placements.at("px_per_tile").get<float>();
			m_origin_x = placements.at("origin_x").get<float>();
			m_origin_y = placements.at("origin_y").get<float>();
			const json& box = placements.at("box");
			m_box_lo = box.at(0).get<float>();
			m_box_hi = box.at(1).get<float>();
		}
		catch (const json::exception& e)
		{
			hb::logger::warn("parchment_kit: invalid {}/placements.json ({})", map_dir, e.what());
			unload();
			return false;
		}
		if (m_size <= 0.0f || m_px_per_tile <= 0.0f)
		{
			hb::logger::warn("parchment_kit: {}/placements.json has no size or scale", map_dir);
			unload();
			return false;
		}

		m_paper = load_texture(renderer, kit_dir + "/paper.png");
		auto ink = load_texture(renderer, map_dir + "/ink.png");
		json atlas;
		if (!m_paper || !ink || !read_json(kit_dir + "/icons.json", atlas))
		{
			unload();
			return false;
		}
		try
		{
			const json& frames = atlas.at("frames");
			for (const json& entry : placements.at("icons"))
			{
				const std::string frame_name = entry.at("icon").get<std::string>();
				if (!frames.contains(frame_name))
					continue;
				const json& frame = frames.at(frame_name);
				const std::string sheet_name = frame.at("image").get<std::string>();
				auto it = m_sheets.find(sheet_name);
				if (it == m_sheets.end())
					it = m_sheets.emplace(sheet_name, load_texture(renderer, kit_dir + "/" + sheet_name)).first;
				if (!it->second)
					continue;   // a missing sheet loses its icons, not the map

				icon placed;
				placed.sheet = it->second.get();
				placed.src_x = frame.at("x").get<float>();
				placed.src_y = frame.at("y").get<float>();
				placed.src_w = frame.at("w").get<float>();
				placed.src_h = frame.at("h").get<float>();
				placed.x = entry.at("x").get<float>();
				placed.y = entry.at("y").get<float>();
				placed.w = entry.at("w").get<float>();
				placed.h = entry.at("h").get<float>();
				m_icons.push_back(placed);
			}
		}
		catch (const json::exception& e)
		{
			hb::logger::warn("parchment_kit: invalid icon data for {} ({})", map_dir, e.what());
			unload();
			return false;
		}
		m_ink = std::move(ink);
		m_map_name = name;
		return true;
	}

	void parchment_kit::draw(hb::shared::render::IRenderer& renderer, int x, int y, int size, float alpha) const
	{
		if (!is_loaded() || size <= 0)
			return;
		draw_full(renderer, m_paper.get(), x, y, size, alpha);
		draw_full(renderer, m_ink.get(), x, y, size, alpha);

		const float scale = static_cast<float>(size) / m_size;
		for (const icon& placed : m_icons)
		{
			renderer.draw_texture_region(placed.sheet, placed.src_x, placed.src_y, placed.src_w, placed.src_h,
				static_cast<float>(x) + placed.x * scale, static_cast<float>(y) + placed.y * scale,
				placed.w * scale, placed.h * scale, alpha);
		}
	}

	bool parchment_kit::tile_to_square(int x, int y, int size, int tile_x, int tile_y, float& out_x, float& out_y) const
	{
		if (!is_loaded() || size <= 0)
			return false;
		const float scale = static_cast<float>(size) / m_size;
		const float px = std::clamp(m_origin_x + (static_cast<float>(tile_x) + tile_centre) * m_px_per_tile, m_box_lo, m_box_hi);
		const float py = std::clamp(m_origin_y + (static_cast<float>(tile_y) + tile_centre) * m_px_per_tile, m_box_lo, m_box_hi);
		out_x = static_cast<float>(x) + px * scale;
		out_y = static_cast<float>(y) + py * scale;
		return true;
	}
}
