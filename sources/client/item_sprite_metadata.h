// item_sprite_metadata.h: Atlas-based item sprite metadata manager
//
// Loads ItemSpriteMetadata.json and provides display_id → frame index lookups
// for the unified item_atlas.pak sprite sheets.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace item_atlas
{
	constexpr int equip = 0;
	constexpr int ground = 1;
	constexpr int pack = 2;
}

// Equipment type classification for equippable items
namespace equip_type
{
	enum type : int8_t
	{
		none = -1,
		weapon = 0,
		shield = 1,
		armor = 2,
		helm = 3,
		pants = 4,
		boots = 5,
		mantle = 6,
		arm_armor = 7,
	};

	type from_string(const char* str);
}

struct item_sprite_gender_data
{
	int16_t equip_frame = -1;
	int16_t ground_frame = -1;
	int16_t pack_frame = -1;
	int16_t pak_start = 0;
	int16_t pak_end = 0;
};

struct item_sprite_entry
{
	int16_t id;
	std::string pak_file;
	item_sprite_gender_data male;
	item_sprite_gender_data female;
	int16_t inventory_frame = -1;	// non-equippable items only
	int16_t ground_frame = -1;		// non-equippable items only
	bool is_equippable;
	equip_type::type equip_slot = equip_type::none;
};

class item_sprite_manager
{
public:
	static item_sprite_manager& get();

	bool load(const std::string& json_path);
	const item_sprite_entry* find(int16_t display_id) const;
	bool has(int16_t display_id) const;
	size_t count() const { return m_entries.size(); }

	// Iterate over all equippable entries (for equipment sprite loading)
	template<typename Func>
	void for_each_equippable(Func&& fn) const {
		for (const auto& [id, entry] : m_entries) {
			if (entry.is_equippable)
				fn(entry);
		}
	}

private:
	item_sprite_manager() = default;
	std::unordered_map<int16_t, item_sprite_entry> m_entries;
};
