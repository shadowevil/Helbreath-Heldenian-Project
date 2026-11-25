// creature_icons.cpp: Creature icons from the map kit
//
//////////////////////////////////////////////////////////////////////

#include "creature_icons.h"
#include "renderer.h"
#include "texture.h"
#include "json.hpp"
#include "log.h"

#include <algorithm>
#include <fstream>

using json = nlohmann::json;

namespace hb::client
{
	creature_icons::creature_icons() = default;
	creature_icons::~creature_icons() = default;

	void creature_icons::unload()
	{
		m_sheet.reset();
		m_icons.clear();
	}

	bool creature_icons::load(hb::shared::render::IRenderer& renderer, const std::string& sprite_dir)
	{
		unload();
		const std::string kit_dir = sprite_dir + "/mapkit";
		const std::string json_path = kit_dir + "/creatures.json";
		std::ifstream file(json_path);
		if (!file.is_open())
		{
			hb::logger::warn("creature_icons: {} not found", json_path);
			return false;
		}
		try
		{
			const json doc = json::parse(file);
			for (const auto& entry : doc.at("creatures"))
			{
				icon rect;
				rect.x = entry.at("x").get<float>();
				rect.y = entry.at("y").get<float>();
				rect.w = entry.at("w").get<float>();
				rect.h = entry.at("h").get<float>();
				for (const auto& id : entry.at("npc_ids"))
					m_icons[id.get<int>()] = rect;
			}
		}
		catch (const json::exception& e)
		{
			hb::logger::warn("creature_icons: invalid {} ({})", json_path, e.what());
			unload();
			return false;
		}

		const std::string sheet_path = kit_dir + "/creatures.png";
		m_sheet.reset(renderer.create_texture_from_file(sheet_path.c_str()));
		if (!m_sheet || m_sheet->get_width() == 0 || m_sheet->get_height() == 0)
		{
			hb::logger::warn("creature_icons: cannot load {}", sheet_path);
			unload();
			return false;
		}
		return true;
	}

	void creature_icons::draw(hb::shared::render::IRenderer& renderer, int npc_id, float centre_x, float centre_y, float box_size) const
	{
		if (!m_sheet)
			return;
		const auto it = m_icons.find(npc_id);
		if (it == m_icons.end())
			return;
		const icon& rect = it->second;
		if (rect.w <= 0.0f || rect.h <= 0.0f)
			return;
		const float scale = std::min(1.0f, box_size / std::max(rect.w, rect.h));
		const float dest_w = rect.w * scale;
		const float dest_h = rect.h * scale;
		renderer.draw_texture_region(m_sheet.get(), rect.x, rect.y, rect.w, rect.h,
			centre_x - dest_w * 0.5f, centre_y - dest_h * 0.5f, dest_w, dest_h, 1.0f);
	}
}
