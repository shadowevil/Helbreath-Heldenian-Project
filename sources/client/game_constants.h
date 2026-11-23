#pragma once
#include <cstdint>

namespace game_limits {
    constexpr int max_sprites          = 25000;
    constexpr int max_tiles            = 1000;
    constexpr int max_effect_sprites   = 300;
    constexpr int max_sound_effects    = 200;
    constexpr int max_chat_msgs        = 500;
    constexpr int max_whisper_msgs     = 5;
    constexpr int max_chat_scroll_msgs = 80;
    constexpr int max_effects          = 300;
    constexpr int max_menu_items       = 140;
    constexpr int max_text_dlg_lines   = 300;
    constexpr int max_magic_types      = 100;
    constexpr int max_skill_types      = 60;
    constexpr int max_weather_objects  = 600;
    constexpr int max_game_msgs        = 300;
    constexpr int max_guild_names      = 100;
    constexpr int max_sell_list        = 12;
    constexpr int socket_block_limit   = 300;
}

namespace ui_layout {
    constexpr int btn_size_x    = 74;
    constexpr int btn_size_y    = 20;
    constexpr int left_btn_x    = 30;
    constexpr int right_btn_x   = 154;
    constexpr int btn_y         = 292;
}

namespace input_config {
    constexpr int double_click_time_ms  = 300;
    constexpr int double_click_tolerance = 4;
}

// CursorStatus enum is defined in cursor_target.h

enum class ServerType : uint8_t {
    Game = 1,
    Log  = 2
};

// hb::shared::limits::MaxMagicType and hb::shared::limits::MaxSkillType are defined in net_constants.h (shared)

// Pixel height per NPC/entity type for vertical effect offset positioning.
// Indexed by hb::shared::owner type ID. Used by effect code to place hit
// particles, auras, and arrows at the correct height on each sprite.
namespace entity_visual
{
	inline constexpr int attacker_height[] = {
		0,   // 0: (unused)
		35,  // 1: Player body 1
		35,  // 2: Player body 2
		35,  // 3: Player body 3
		35,  // 4: Player body 4
		35,  // 5: Player body 5
		35,  // 6: Player body 6
		0,   // 7: (unused)
		0,   // 8: (unused)
		0,   // 9: (unused)
		5,   // 10: Slime
		35,  // 11: Skeleton
		40,  // 12: Stone-Golem
		45,  // 13: Cyclops
		35,  // 14: OrcMage
		35,  // 15: ShopKeeper
		5,   // 16: GiantAnt
		8,   // 17: Scorpion
		35,  // 18: Zombie
		35,  // 19: Gandalf
		35,  // 20: Howard
		35,  // 21: Guard
		10,  // 22: Amphis
		38,  // 23: Clay-Golem
		35,  // 24: Tom
		35,  // 25: William
		35,  // 26: Kennedy
		35,  // 27: Hellhound
		50,  // 28: Troll
		45,  // 29: Ogre
		55,  // 30: Liche
		65,  // 31: Demon
		46,  // 32: Unicorn
		49,  // 33: WereWolf
		55,  // 34: Dummy
		35,  // 35: EnergySphere
		75,  // 36: Arrow Guard Tower
		75,  // 37: Cannon Guard Tower
		50,  // 38: Mana Collector
		50,  // 39: Detector
		50,  // 40: Energy Shield Generator
		50,  // 41: Grand Magic Generator
		50,  // 42: ManaStone
		40,  // 43: Light War Beetle
		35,  // 44: GHK
		40,  // 45: GHKABS
		35,  // 46: TK
		60,  // 47: BG
		40,  // 48: Stalker
		70,  // 49: HellClaw
		85,  // 50: Tigerworm
		50,  // 51: Catapult
		85,  // 52: Gargoyle
		70,  // 53: Beholder
		40,  // 54: Dark-Elf
		20,  // 55: Bunny
		20,  // 56: Cat
		40,  // 57: Giant-Frog
		80,  // 58: Mountain-Giant
		85,  // 59: Ettin
		50,  // 60: Cannibal-Plant
		50,  // 61: Rudolph
		80,  // 62: Direboar
		90,  // 63: Frost
		40,  // 64: Crops
		80,  // 65: IceGolem
		190, // 66: Wyvern
		35,  // 67: NPC 67
		35,  // 68: NPC 68
		35,  // 69: NPC 69
		100, // 70: Dragon
		90,  // 71: Centaur
		75,  // 72: ClawTurtle
		200, // 73: FireWyvern
		80,  // 74: GiantCrayfish
		120, // 75: GiantLizard
		100, // 76: GiantTree
		100, // 77: MasterOrc
		80,  // 78: Minaus
		100, // 79: Nizie
		25,  // 80: Tentacle
		200, // 81: Abaddon
		60,  // 82: Sorceress
		60,  // 83: ATK
		70,  // 84: MasterElf
		60,  // 85: DSK
		50,  // 86: HBT
		60,  // 87: CT
		60,  // 88: Barbarian
		60,  // 89: AGC
		35,  // 90: Gail
		35,  // 91: Gate
	};
}
