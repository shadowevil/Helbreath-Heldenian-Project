#pragma once

namespace hb::client::sprite_id
{

// Interface sprite IDs
constexpr int InterfaceSprFonts             = 22;
constexpr int InterfaceFont1                = 30;
constexpr int InterfaceFont2                = 31;

constexpr int InterfaceNdNewChar            = 58;
constexpr int InterfaceNdNewExchange        = 59;
constexpr int InterfaceNdGame3              = 62;
constexpr int InterfaceNdGame4              = 63;
constexpr int InterfaceNdInventory          = 67;
constexpr int InterfaceNdCrusade            = 72;

// Item pivot points
constexpr int ItemEquipPivotPoint           = 200;
constexpr int ItemDynamicPivotPoint         = 400;

// Mob base sprite ID
constexpr int Mob                           = 17000;

// Character equipment sprite bases (male/female)
constexpr int UndiesM                       = 1400;
constexpr int UndiesW                       = 11400;
constexpr int HairM                         = 1600;
constexpr int HairW                         = 11600;

// Angels
constexpr int TutelaryAngelsPivotPoint      = 10800;

// Holiday tile variants (stored at offset from normal tile index)
constexpr int HolidayTileOffset             = 10000;

} // namespace hb::client::sprite_id

// Equipment sprite index calculation for the per-item sprite folders (sprites/items/<folder>).
// Each equippable item gets 12 sprites per gender (one per body pose).
// Male sprites occupy [0, female_base), female sprites occupy [female_base, ...).
namespace equip_sprite {
	constexpr int sprites_per_item = 12;
	constexpr int male_base = 0;
	constexpr int female_base = 10000;

	inline int index(bool female, int16_t display_id, int pose) {
		if (display_id < 0) return -1;
		return (female ? female_base : male_base) + display_id * sprites_per_item + pose;
	}
}
