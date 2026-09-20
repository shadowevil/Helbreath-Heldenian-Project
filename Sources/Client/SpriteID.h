#pragma once

namespace hb::client::sprite_id
{

constexpr int MouseCursor                   = 0;

// Interface sprite IDs
constexpr int InterfaceSprFonts             = 22;
constexpr int InterfaceFont1                = 30;
constexpr int InterfaceFont2                = 31;
constexpr int InterfaceSprFonts3            = 34;

constexpr int InterfaceNewMaps1             = 35;
constexpr int InterfaceNewMaps2             = 36;
constexpr int InterfaceNewMaps3             = 37;
constexpr int InterfaceNewMaps4             = 38;
constexpr int InterfaceNewMaps5             = 39;

constexpr int InterfaceMonster              = 50;
constexpr int InterfaceNdLoading            = 51;
constexpr int InterfaceNdMainMenu           = 52;
constexpr int InterfaceNdLogin              = 53;
constexpr int InterfaceNdNewAccount         = 54;
constexpr int InterfaceNdQuit               = 55;
constexpr int InterfaceNdSelectChar         = 57;
constexpr int InterfaceNdNewChar            = 58;
constexpr int InterfaceNdNewExchange        = 59;
constexpr int InterfaceNdGame1              = 60;
constexpr int InterfaceNdGame2              = 61;
constexpr int InterfaceNdGame3              = 62;
constexpr int InterfaceNdGame4              = 63;
constexpr int InterfaceNdIconPanel          = 64;
constexpr int InterfaceNdInventory          = 67;
constexpr int InterfaceNdText               = 70;
constexpr int InterfaceNdButton             = 71;
constexpr int InterfaceNdCrusade            = 72;
constexpr int InterfaceGuideMap             = 420;

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

// Splash screen
constexpr int SplashScreen                  = 18491;

} // namespace hb::client::sprite_id

// Equipment sprite index calculation for the new per-item pak system.
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
