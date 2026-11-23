// game_geometry.h: Shared geometry types and algorithms for Client and Server
//
// Cross-platform point and rectangle types with optional Windows conversion helpers.
// Also includes shared geometry algorithms (Bresenham line).
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>

// Note: This header does NOT include <windows.h> to avoid winsock ordering
// issues on the server. The Windows conversion helpers (ToPOINT, FromPOINT,
// ToRECT, FromRECT) are available when <windows.h> has already been included
// before this header (detected via the _WINDEF_ include guard).

namespace hb::shared::geometry {

// ============================================================================
// Point Type
// ============================================================================

struct GamePoint
{
    int x = 0;
    int y = 0;

    constexpr GamePoint() = default;
    constexpr GamePoint(int x_, int y_) : x(x_), y(y_) {}

#ifdef _WINDEF_
    POINT ToPOINT() const { return POINT{ x, y }; }
    static GamePoint FromPOINT(const POINT& p) { return GamePoint(p.x, p.y); }
#endif
};

// ============================================================================
// Rectangle Type
// ============================================================================

// hb::shared::render::Renderer-agnostic rectangle for game use
// Can be converted to platform-specific types (RECT, sf::IntRect) as needed
struct GameRectangle
{
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    constexpr GameRectangle() = default;
    constexpr GameRectangle(int x_, int y_, int w, int h)
        : x(x_), y(y_), width(w), height(h) {}

    // Edge accessors for compatibility
    constexpr int Left() const { return x; }
    constexpr int Top() const { return y; }
    constexpr int Right() const { return x + width; }
    constexpr int Bottom() const { return y + height; }

#ifdef _WINDEF_
    // Convert to Windows RECT
    RECT ToRECT() const { return RECT{ x, y, x + width, y + height }; }

    // Construct from Windows RECT
    static GameRectangle FromRECT(const RECT& r) {
        return GameRectangle(r.left, r.top, r.right - r.left, r.bottom - r.top);
    }
#endif
};

// ============================================================================
// Bresenham Line Algorithm
// ============================================================================

// Steps count pixels along the Bresenham line from (x0,y0) toward (x1,y1).
// error_acc carries the accumulated error between calls for incremental stepping.
// On return, *pX/*pY hold the resulting position.
static inline void GetPoint2(int x0, int y0, int x1, int y1, int* pX, int* pY, int* error_acc, int count)
{
    int dx, dy, x_inc, y_inc, error, index;
    int result_x, result_y, cnt = 0;

    if ((x0 == x1) && (y0 == y1)) {
        *pX = x0;
        *pY = y0;
        return;
    }

    error = *error_acc;

    result_x = x0;
    result_y = y0;

    dx = x1 - x0;
    dy = y1 - y0;

    if (dx >= 0) {
        x_inc = 1;
    }
    else {
        x_inc = -1;
        dx = -dx;
    }

    if (dy >= 0) {
        y_inc = 1;
    }
    else {
        y_inc = -1;
        dy = -dy;
    }

    if (dx > dy) {
        for (index = 0; index <= dx; index++) {
            error += dy;
            if (error > dx) {
                error -= dx;
                result_y += y_inc;
            }
            result_x += x_inc;
            cnt++;
            if (cnt >= count)
                break;
        }
    }
    else {
        for (index = 0; index <= dy; index++) {
            error += dx;
            if (error > dy) {
                error -= dy;
                result_x += x_inc;
            }
            result_y += y_inc;
            cnt++;
            if (cnt >= count)
                break;
        }
    }

    *pX = result_x;
    *pY = result_y;
    *error_acc = error;
}

} // namespace hb::shared::geometry
