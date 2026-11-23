#pragma once

#include <functional>

namespace cc {

struct point { int x = 0, y = 0; };
struct rect { int x = 0, y = 0, w = 0, h = 0; };

inline bool rect_contains(const rect& r, int px, int py)
{
	return px >= r.x && px < r.x + r.w && py >= r.y && py < r.y + r.h;
}

// Alignment enums
namespace align {

enum class horizontal { none, left, center, right };
enum class vertical { none, top, center, bottom };

} // namespace align

enum class flow_direction { none, horizontal, vertical };

// Forward declaration
class control;

// Render handler type — set per-control by the game
using render_handler = std::function<void(const control& c)>;

// Sound callback type
using sound_callback = std::function<void()>;

} // namespace cc
