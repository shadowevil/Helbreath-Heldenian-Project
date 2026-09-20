// CommonTypes.h: Cross-platform type definitions
//
// This header replaces Windows-specific typedefs (DWORD, WORD, BYTE) with
// standard C++ types (uint32_t, uint16_t, uint8_t) for cross-platform compatibility.
//
// Windows API types (HWND, HANDLE, HRESULT) are preserved for platform-specific code.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <cstring>
#include <chrono>

// ============================================================================
// Standard C++ Type Replacements
// ============================================================================
//
// The following Windows typedefs have been replaced throughout the codebase:
//   DWORD → uint32_t (32-bit unsigned integer)
//   WORD  → uint16_t (16-bit unsigned integer)
//   BYTE  → uint8_t  (8-bit unsigned integer)
//
// Hungarian notation is preserved (e.g., time remains time, but type is uint32_t)
//
// ============================================================================

#include "GameGeometry.h"
#include "PrimitiveTypes.h"

// ============================================================================
// Memory Operations
// ============================================================================

// Windows ZeroMemory() should be replaced with std::memset() throughout the codebase
// Old: ZeroMemory(&struct, sizeof(struct));
// New: std::memset(&struct, 0, sizeof(struct));

// ============================================================================
// Timing System
// ============================================================================

// GameClock: Cross-platform timing system
// Replaces Windows timeGetTime() with std::chrono for cross-platform compatibility
//
// Usage:
//   GameClock::initialize();  // Call once at startup
//   uint32_t time = GameClock::get_time_ms();  // get current time in milliseconds
//
// Note: Time wraps around after ~49.7 days (2^32 milliseconds), same as timeGetTime().
//       Use delta time calculations for robust timing that handles wraparound.
//
class GameClock {
private:
    static std::chrono::steady_clock::time_point s_startTime;
    static bool s_initialized;

public:
    // initialize the game clock - call once at application startup
    static void initialize() {
        if (!s_initialized) {
            s_startTime = std::chrono::steady_clock::now();
            s_initialized = true;
        }
    }

    // get milliseconds since initialization
    // Returns: uint32_t milliseconds (0 to 4,294,967,295, wraps after ~49.7 days)
    //
    // This replaces Windows timeGetTime() with equivalent behavior:
    //   Old: DWORD time = timeGetTime();
    //   New: uint32_t time = GameClock::get_time_ms();
    //
    static uint32_t get_time_ms() {
        if (!s_initialized) initialize();

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - s_startTime);
        return static_cast<uint32_t>(elapsed.count());
    }
};

// ============================================================================
// hb::shared::render::Color System (defined in PrimitiveTypes.h)
// ============================================================================

namespace GameColors
{
	// Pre-computed math colors (independent constants, not from palette)
	// original: m_wR[13]*2, m_wG[13]*2, m_wB[13]*2
	inline constexpr hb::shared::render::Color Yellow2x{ 160, 160, 16 };
	// original: m_wR[13]*4, m_wG[13]*4, m_wB[13]*4
	inline constexpr hb::shared::render::Color Yellow4x{ 255, 255, 32 };
	// original: m_wR[14]*4, m_wG[14]*4, m_wB[14]*4
	inline constexpr hb::shared::render::Color Red4x{ 255, 32, 32 };
	// original: m_wR[5]*11, m_wG[5]*11, m_wB[5]*11
	inline constexpr hb::shared::render::Color PoisonText{ 88, 255, 88 };
	// original: m_wR[5]*8, m_wG[5]*8, m_wB[5]*8
	inline constexpr hb::shared::render::Color PoisonLabel{ 64, 255, 64 };
	// Status effect additive colors (for AdditiveColored blend)
	// Berserk: creates a bright reddish/golden glow when added to destination
	inline constexpr hb::shared::render::Color BerserkGlow{ 255, 240, 240 };

	// ====================================================================
	// UI Text Colors
	// ====================================================================

	// ====================================================================
	// HUD / Name Plate Colors
	// ====================================================================

	// hb::shared::text::TextStyle::with_shadow (Game.cpp, Screen_OnGame.cpp)
	inline constexpr hb::shared::render::Color InfoGrayLight{ 180, 180, 180 };  // Lighter info text

	// ====================================================================
	// Additional UI Colors
	// ====================================================================

	inline constexpr hb::shared::render::Color UIMenuHighlight{ 250, 250, 0 };  // Teleport menu highlight (DialogBox_CityHallMenu, DialogBox_CommandHallMenu)
	inline constexpr hb::shared::render::Color UINoticeRed{ 100, 10, 10 };      // Notice message text (DialogBox_Noticement)
	inline constexpr hb::shared::render::Color UITooltip{ 250, 250, 220 };      // Tooltip text (DialogBox_HudPanel)
	inline constexpr hb::shared::render::Color UIDisabledMed{ 120, 120, 120 };      // Grayed out text (DialogBox_Manufacture)
	inline constexpr hb::shared::render::Color UISelectPurple{ 51, 0, 51 };     // Character select labels (Screen_SelectCharacter)
	inline constexpr hb::shared::render::Color UIFormLabel{ 100, 100, 200 };    // Form field labels (Screen_CreateAccount)
	inline constexpr hb::shared::render::Color UIProfileYellow{ 255, 255, 100 };// Profile overlay text (Screen_OnGame)
	inline constexpr hb::shared::render::Color UITopMsgYellow{ 255, 255, 0 };   // Top message text (Game.cpp)
	inline constexpr hb::shared::render::Color UIDmgYellow{ 255, 255, 20 };     // Damage text yellow (Game.cpp)
	inline constexpr hb::shared::render::Color UIDmgRed{ 255, 80, 80 };         // Damage text red (Game.cpp)
	inline constexpr hb::shared::render::Color UIBuildRed{ 180, 30, 30 };       // Build/craft warning (Game.cpp)
	inline constexpr hb::shared::render::Color ChatEventGreen{ 130, 255, 130 }; // Event history green (Game.cpp)
	inline constexpr hb::shared::render::Color UISlatesPink{ 220, 140, 160 };   // Slates effect text (DialogBox_Slates)
	inline constexpr hb::shared::render::Color UISlatesCyan{ 90, 220, 200 };    // Slates effect text (DialogBox_Slates)

	// Bitmap font button colors
	// hb::shared::text::TextStyle::with_highlight (various dialog boxes)
	inline constexpr hb::shared::render::Color BmpBtnNormal{ 0, 0, 7 };         // Normal bitmap button (DialogBox_NpcTalk, DialogBox_Exchange, DialogBox_Manufacture, DialogBox_Slates)
	inline constexpr hb::shared::render::Color BmpBtnHover{ 15, 15, 15 };       // Hover bitmap button (DialogBox_Exchange)
	inline constexpr hb::shared::render::Color BmpBtnActive{ 10, 10, 10 };      // Active bitmap button (Game.cpp, DialogBox_Manufacture)
	inline constexpr hb::shared::render::Color BmpBtnBlue{ 16, 16, 30 };        // Blue bitmap button (DialogBox_Manufacture)
	inline constexpr hb::shared::render::Color BmpBtnRed{ 20, 6, 6 };           // Red bitmap button (DialogBox_Manufacture)
	inline constexpr hb::shared::render::Color BmpBtnFishRed{ 10, 0, 0 };       // Fishing button (DialogBox_Fishing)

	// Minimap night colors (Game.cpp CMisc::ColorTransfer)
	inline constexpr hb::shared::render::Color NightBlueMid{ 50, 50, 100 };     // Night sky mid
	inline constexpr hb::shared::render::Color NightBlueDark{ 30, 30, 100 };    // Night sky dark
	inline constexpr hb::shared::render::Color NightBlueDeep{ 0, 0, 30 };       // Night deep
	inline constexpr hb::shared::render::Color NightBlueBright{ 50, 50, 200 };  // Night sky bright


	// Completed
	inline constexpr hb::shared::render::Color UIMagicBlue{ 4,0,50 };
	inline constexpr hb::shared::render::Color UIMagicPurple{ 60, 10, 60 };
	inline constexpr hb::shared::render::Color UIMagicDisabled{ 50, 15, 15 };  // INT requirement not met
	inline constexpr hb::shared::render::Color UIGuildGreen{ 130, 200, 130 };
	inline constexpr hb::shared::render::Color UIWorldChat{ 255, 130, 130 };
	inline constexpr hb::shared::render::Color UIFactionChat{ 130, 130, 255 };
	inline constexpr hb::shared::render::Color UIPartyChat{ 230, 230, 130 };
	inline constexpr hb::shared::render::Color UINormalChat{ 150, 150, 170 };
	inline constexpr hb::shared::render::Color UIGameMasterChat{ 180, 255, 180 };
	inline constexpr hb::shared::render::Color UILabel{ 25, 25, 25 };
	inline constexpr hb::shared::render::Color UIDisabled{ 65, 65, 65 };
	inline constexpr hb::shared::render::Color MonsterStatusEffect{ 240, 240, 70 };
	inline constexpr hb::shared::render::Color UIItemName_Special{ 0, 255, 50 };
	inline constexpr hb::shared::render::Color NeutralNamePlate{ 50, 50, 255 };
	inline constexpr hb::shared::render::Color EnemyNamePlate{ 255, 0, 0 };
	inline constexpr hb::shared::render::Color FriendlyNamePlate{ 30, 255, 30 };
	inline constexpr hb::shared::render::Color UIModifiedStat{ 0, 0, 192 };

	inline constexpr hb::shared::render::Color InputValid{ 100, 200, 100 };
	inline constexpr hb::shared::render::Color InputInvalid{ 200, 100, 100 };
	inline constexpr hb::shared::render::Color InputNormal{ 200, 200, 200 };

	inline constexpr hb::shared::render::Color UIBlack{ 0, 0, 0 };
	inline constexpr hb::shared::render::Color UIWhite{ 255, 255, 255 };
	inline constexpr hb::shared::render::Color UINearWhite{ 232, 232, 232 };
	inline constexpr hb::shared::render::Color UIDescription{ 150, 150, 150 };  // Item tooltips, stats, descriptions
	inline constexpr hb::shared::render::Color UIGreen{ 0, 255, 0 };
	inline constexpr hb::shared::render::Color UIDarkGreen{ 0, 55, 0 };
	inline constexpr hb::shared::render::Color UIRed{ 255, 0, 0 };
	inline constexpr hb::shared::render::Color UIDarkRed{ 58, 0, 0 };
	inline constexpr hb::shared::render::Color UIWarningRed{ 195, 25, 25 };
	inline constexpr hb::shared::render::Color UIOrange{ 220, 130, 45 };
	inline constexpr hb::shared::render::Color UIYellow{ 200, 200, 25 };
	inline constexpr hb::shared::render::Color UIPaleYellow{ 200, 200, 120 };
}
