#pragma once

#include <cstdint>
#include "common_types.h"

// ---------------------------------------------------------------
// Floating text category enums
// ---------------------------------------------------------------

enum class chat_text_type : uint8_t {
	player_chat
};

enum class damage_text_type : uint8_t {
	Small,      // <12 pts  - small sprite font
	Medium,     // 12-39 pts, Immune, Failed - medium sprite font
	Large,      // 40+ pts, Critical! - large sprite font
};

enum class notify_text_type : uint8_t {
	skill_change,    // "+2% Mining" - yellow, delayed 650ms
	magic_cast_name,  // "Fire Ball!" - red default font, as 3.82
	LevelUp,        // "Level up!" - large sprite font
	enemy_kill,      // "Enemy Kill!" - large sprite font
};

// ---------------------------------------------------------------
// Animation parameters for each floating text type
// ---------------------------------------------------------------

struct AnimParams {
	uint32_t m_lifetime_ms;    // Total display duration
	uint32_t m_show_delay_ms;   // Delay before visible (0 = instant)
	int m_start_offset_y;        // Starting Y offset above entity foot (pixels)
	int m_rise_pixels;          // Total upward rise distance
	int m_rise_duration_ms;      // Time to reach final position
	int m_font_offset;          // Offset from SprFont3_0 (0=large, 1=medium, 2=small)
	hb::shared::render::Color m_color;              // Text color
	bool m_use_sprite_font;      // true = sprite font, false = renderer text
};

// ---------------------------------------------------------------
// Parameter tables (constexpr)
// ---------------------------------------------------------------

namespace FloatingTextParams {

inline constexpr AnimParams Chat[] = {
	// player_chat: white renderer text, 4s, fast rise
	{ 4000, 0, 55, 10, 200, 0, GameColors::UIWhite, false },
};

inline constexpr AnimParams Damage[] = {
	// Small:  yellow sprite font (small), 200ms, fast rise
	{ 500, 0, 55, 20, 200, 2, GameColors::UIDmgYellow, true },
	// Medium: yellow sprite font (medium), 200ms, fast rise
	{ 500, 0, 55, 20, 200, 1, GameColors::UIDmgYellow, true },
	// Large:  yellow sprite font (large), 200ms, fast rise
	{ 500, 0, 55, 20, 200, 0, GameColors::UIDmgYellow, true },
};

inline constexpr AnimParams Notify[] = {
	// skill_change: yellow renderer text, 4s, delayed 650ms
	{ 4000, 0, 55, 10, 80, 0, GameColors::UIDmgYellow, false },
	// magic_cast_name: red default font drawn twice 1 px apart, no shadow, 2s (3.82 DrawChatMsgBox types 41/42)
	{ 2000, 0, 55, 15, 200, 0, GameColors::UISpellNameRed, false },
	// LevelUp: yellow sprite font (large), 2s, fast rise
	{ 2000, 0, 55, 10, 80, 0, GameColors::UIDmgYellow, true },
	// enemy_kill: yellow sprite font (large), 2s, fast rise
	{ 2000, 0, 55, 10, 80, 0, GameColors::UIDmgYellow, true },
};

} // namespace FloatingTextParams
