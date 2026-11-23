// item_sprite_metadata.cpp: JSON loading for atlas-based item sprite metadata
//
//////////////////////////////////////////////////////////////////////

#include "item_sprite_metadata.h"
#include "json.hpp"
#include "log.h"

#include <cstring>
#include <fstream>

using json = nlohmann::json;

equip_type::type equip_type::from_string(const char* str)
{
	if (!str) return equip_type::none;
	if (strcmp(str, "weapon") == 0) return equip_type::weapon;
	if (strcmp(str, "shield") == 0) return equip_type::shield;
	if (strcmp(str, "armor") == 0) return equip_type::armor;
	if (strcmp(str, "helm") == 0) return equip_type::helm;
	if (strcmp(str, "pants") == 0) return equip_type::pants;
	if (strcmp(str, "boots") == 0) return equip_type::boots;
	if (strcmp(str, "mantle") == 0) return equip_type::mantle;
	if (strcmp(str, "arm_armor") == 0) return equip_type::arm_armor;
	return equip_type::none;
}

item_sprite_manager& item_sprite_manager::get()
{
	static item_sprite_manager instance;
	return instance;
}

bool item_sprite_manager::load(const std::string& json_path)
{
	std::ifstream file(json_path);
	if (!file.is_open())
	{
		hb::logger::warn("ItemSpriteMetadata: failed to open {}", json_path);
		return false;
	}

	json root;
	try
	{
		root = json::parse(file);
	}
	catch (const json::parse_error& e)
	{
		hb::logger::error("ItemSpriteMetadata: JSON parse error: {}", e.what());
		return false;
	}

	if (!root.is_array())
	{
		hb::logger::error("ItemSpriteMetadata: root is not an array");
		return false;
	}

	m_entries.clear();

	for (const auto& obj : root)
	{
		if (!obj.contains("id")) continue;

		item_sprite_entry entry{};
		entry.id = obj["id"].get<int16_t>();

		if (obj.contains("pak_file") && obj["pak_file"].is_string())
			entry.pak_file = obj["pak_file"].get<std::string>();

		// Parse equip_type if present
		if (obj.contains("equip_type") && obj["equip_type"].is_string())
			entry.equip_slot = equip_type::from_string(obj["equip_type"].get<std::string>().c_str());

		// Equippable items have a pak_file and male/female sub-objects
		bool has_pak = obj.contains("pak_file") && obj["pak_file"].is_string();
		if (has_pak)
		{
			entry.is_equippable = true;

			if (obj.contains("male"))
			{
				const auto& m = obj["male"];
				entry.male.equip_frame = m.value("equip_frame_index", (int16_t)-1);
				entry.male.ground_frame = m.value("ground_frame_index", (int16_t)-1);
				entry.male.pack_frame = m.value("pack_frame_index", (int16_t)-1);
				entry.male.pak_start = m.value("pak_index_start", (int16_t)0);
				entry.male.pak_end = m.value("pak_index_end", (int16_t)0);
			}

			if (obj.contains("female"))
			{
				const auto& f = obj["female"];
				entry.female.equip_frame = f.value("equip_frame_index", (int16_t)-1);
				entry.female.ground_frame = f.value("ground_frame_index", (int16_t)-1);
				entry.female.pack_frame = f.value("pack_frame_index", (int16_t)-1);
				entry.female.pak_start = f.value("pak_index_start", (int16_t)0);
				entry.female.pak_end = f.value("pak_index_end", (int16_t)0);
			}
		}
		else
		{
			// Non-equippable items (potions, scrolls, etc.)
			entry.is_equippable = false;
			entry.inventory_frame = obj.value("inventory_frame_index", (int16_t)-1);
			entry.ground_frame = obj.value("ground_frame_index", (int16_t)-1);
		}

		m_entries[entry.id] = std::move(entry);
	}

	hb::logger::log("ItemSpriteMetadata: loaded {} entries from {}", m_entries.size(), json_path);
	return true;
}

const item_sprite_entry* item_sprite_manager::find(int16_t display_id) const
{
	auto it = m_entries.find(display_id);
	if (it == m_entries.end()) return nullptr;
	return &it->second;
}

bool item_sprite_manager::has(int16_t display_id) const
{
	return m_entries.find(display_id) != m_entries.end();
}
