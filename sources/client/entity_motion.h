// entity_motion.h: Smooth entity movement interpolation system
//
// Separates animation frames (sprite selection) from movement position (interpolation)
// Provides smooth per-pixel movement at any framerate
//////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include "direction_helpers.h"

using hb::shared::direction::direction;

//=============================================================================
// Movement Timing Constants
//=============================================================================
namespace MovementTiming {
    // Duration to cross one tile (milliseconds)
    // Must match animation frame timing: (maxFrame + 1) * frameTime
    // Movement duration = (maxFrame + 1) * frameTime
    constexpr uint32_t WALK_DURATION_MS = 560;         // 8 frames (0-7) * 70ms
    constexpr uint32_t RUN_DURATION_MS = 312;          // 8 frames (0-7) * 39ms
    constexpr uint32_t DAMAGE_MOVE_DURATION_MS = 200;  // 4 frames (0-3) * 50ms
    constexpr uint32_t ATTACK_MOVE_DURATION_MS = 1014; // 13 frames (0-12) * 78ms

    // Tile size in pixels (matches original offset range of ~30)
    constexpr int16_t TILE_SIZE = 32;
}

//=============================================================================
// EntityMotion - Interpolation state for a single entity
//=============================================================================
struct EntityMotion {
    //-------------------------------------------------------------------------
    // State
    //-------------------------------------------------------------------------
    bool m_is_moving = false;
    direction m_direction = direction{};  // 1-8 cardinal/ordinal directions

    //-------------------------------------------------------------------------
    // Interpolation
    //-------------------------------------------------------------------------
    float m_progress = 0.0f;         // 0.0 (start) to 1.0 (complete)

    //-------------------------------------------------------------------------
    // Timing
    //-------------------------------------------------------------------------
    uint32_t m_start_time = 0;       // When movement started (ms)
    uint32_t m_duration = 0;        // Total movement duration (ms)

    //-------------------------------------------------------------------------
    // Pixel Offsets (from current tile center)
    // start: offset where entity came FROM
    // Target: always (0, 0) - the tile center
    // Using floats to support seamless tile transitions with non-integer offsets
    //-------------------------------------------------------------------------
    float m_start_offset_x = 0.0f;
    float m_start_offset_y = 0.0f;

    //-------------------------------------------------------------------------
    // Calculated Output (updated each frame) - floats for smooth interpolation
    //-------------------------------------------------------------------------
    float m_current_offset_x = 0.0f;
    float m_current_offset_y = 0.0f;

    //-------------------------------------------------------------------------
    // Pending next movement (2-slot queue for seamless chaining)
    //-------------------------------------------------------------------------
    bool m_has_pending = false;
    direction m_pending_direction = direction{};
    uint32_t m_pending_duration = 0;

    //-------------------------------------------------------------------------
    // Methods
    //-------------------------------------------------------------------------

    // start movement in given direction
    // direction: 1=N, 2=NE, 3=E, 4=SE, 5=S, 6=SW, 7=W, 8=NW
    // currentTime: current game time in milliseconds
    // duration: time to complete movement in milliseconds
    void start_move(direction dir, uint32_t currentTime, uint32_t duration);

    // start movement with a custom initial offset (for seamless tile transitions)
    // This allows continuing from where a previous motion left off
    // offsetX, offsetY: The starting offset (can be outside normal [-32, 0] range for seamless transitions)
    void start_move_with_offset(direction dir, uint32_t currentTime, uint32_t duration, float offsetX, float offsetY);

    // Queue a follow-up move (called when move arrives while still interpolating)
    void queue_move(direction dir, uint32_t duration);

    // update interpolation - call every frame
    // currentTime: current game time in milliseconds
    void update(uint32_t currentTime);

    // stop movement smoothly (at current position)
    void stop();

    // Hard snap back to tile center (collision/bump)
    // Preserves the classic jarring bump feel
    void bump();

    // reset to default state
    void reset();

    //-------------------------------------------------------------------------
    // Queries
    //-------------------------------------------------------------------------
    bool is_complete() const { return !m_is_moving && m_progress >= 1.0f; }
    bool is_moving() const { return m_is_moving; }
    bool has_pending() const { return m_has_pending; }

    //-------------------------------------------------------------------------
    // Static Helpers
    //-------------------------------------------------------------------------

    // get starting pixel offset for a movement direction
    // Entity moving in direction D came FROM the opposite direction
    // Returns the pixel offset from tile center where movement starts
    static void get_direction_start_offset(direction dir, int16_t& outX, int16_t& outY);

    // get movement duration for an action type
    static uint32_t get_duration_for_action(int action, bool hasHaste = false, bool frozen = false);
};
