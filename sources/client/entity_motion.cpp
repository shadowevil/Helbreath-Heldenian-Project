// entity_motion.cpp: Smooth entity movement interpolation implementation
//
//////////////////////////////////////////////////////////////////////

#include "entity_motion.h"
#include "action_id.h"
#include <cmath>
#include <algorithm>


using namespace hb::shared::action;
using namespace hb::shared::direction;

//=============================================================================
// start_move - Begin movement interpolation in a direction
//=============================================================================
void EntityMotion::start_move(direction dir, uint32_t currentTime, uint32_t duration)
{
    m_direction = dir;
    m_start_time = currentTime;
    m_duration = duration;
    m_progress = 0.0f;
    m_is_moving = true;

    // get starting offset based on direction
    // Entity is moving TO this tile FROM the neighboring tile
    int16_t startX, startY;
    get_direction_start_offset(dir, startX, startY);
    m_start_offset_x = static_cast<float>(startX);
    m_start_offset_y = static_cast<float>(startY);

    // Initial position is at the start offset
    m_current_offset_x = m_start_offset_x;
    m_current_offset_y = m_start_offset_y;
}

//=============================================================================
// start_move_with_offset - Begin movement with custom initial offset
// Used for seamless tile transitions during continuous movement
// The starting offset can be outside normal [-32, 0] range to ensure visual continuity
//=============================================================================
void EntityMotion::start_move_with_offset(direction dir, uint32_t currentTime, uint32_t duration, float offsetX, float offsetY)
{
    m_direction = dir;
    m_start_time = currentTime;
    m_is_moving = true;

    // Use custom start offsets (allows values like -33.6 for seamless transitions)
    m_start_offset_x = offsetX;
    m_start_offset_y = offsetY;

    // Adjust duration proportionally to the extra distance
    // If covering 33.6 pixels instead of 32, take proportionally longer
    float normalDist = 32.0f;
    float actualDistX = std::abs(offsetX);
    float actualDistY = std::abs(offsetY);
    float actualDist = std::max(actualDistX, actualDistY);
    if (actualDist > 0.0f && actualDist != normalDist) {
        float scale = actualDist / normalDist;
        m_duration = static_cast<uint32_t>(duration * scale);
    } else {
        m_duration = duration;
    }

    m_progress = 0.0f;

    // Initial position at the custom offset
    m_current_offset_x = offsetX;
    m_current_offset_y = offsetY;
}

//=============================================================================
// queue_move - Queue a follow-up movement for seamless chaining
//=============================================================================
void EntityMotion::queue_move(direction dir, uint32_t duration)
{
    m_has_pending = true;
    m_pending_direction = dir;
    m_pending_duration = duration;
}

//=============================================================================
// update - update interpolation each frame
//=============================================================================
void EntityMotion::update(uint32_t currentTime)
{
    if (!m_is_moving) return;
    if (m_duration == 0) {
        // Prevent division by zero
        stop();
        return;
    }

    // Calculate elapsed time
    uint32_t elapsed = currentTime - m_start_time;

    // Calculate m_progress (0.0 to 1.0)
    m_progress = static_cast<float>(elapsed) / static_cast<float>(m_duration);

    // Check for completion
    if (m_progress >= 1.0f) {
        if (m_has_pending) {
            // Chain into next movement seamlessly
            // Overshoot time carries into next movement
            uint32_t overshoot = elapsed - m_duration;
            direction pendDir = m_pending_direction;
            uint32_t pendDur = m_pending_duration;
            m_has_pending = false;
            start_move(pendDir, currentTime - overshoot, pendDur);
            // Recursively update with remaining time
            update(currentTime);
            return;
        }
        else {
            // No pending: snap to tile center
            m_progress = 1.0f;
            m_is_moving = false;
            m_current_offset_x = 0.0f;
            m_current_offset_y = 0.0f;
            return;
        }
    }

    // Linear interpolation from start offset to (0, 0)
    // Formula: current = start * (1 - m_progress)
    // Keep as float for smooth sub-pixel interpolation
    float invProgress = 1.0f - m_progress;
    m_current_offset_x = m_start_offset_x * invProgress;
    m_current_offset_y = m_start_offset_y * invProgress;
}

//=============================================================================
// stop - stop movement at current position
//=============================================================================
void EntityMotion::stop()
{
    m_is_moving = false;
    // Keep current offset - don't snap
}

//=============================================================================
// bump - Hard snap back to tile center (collision)
//=============================================================================
void EntityMotion::bump()
{
    // Classic bump behavior - immediate snap to tile center
    m_is_moving = false;
    m_progress = 0.0f;
    m_current_offset_x = 0.0f;
    m_current_offset_y = 0.0f;
    m_start_offset_x = 0.0f;
    m_start_offset_y = 0.0f;
    m_has_pending = false;
}

//=============================================================================
// reset - reset to default state
//=============================================================================
void EntityMotion::reset()
{
    m_is_moving = false;
    m_direction = direction{};
    m_progress = 0.0f;
    m_start_time = 0;
    m_duration = 0;
    m_start_offset_x = 0.0f;
    m_start_offset_y = 0.0f;
    m_current_offset_x = 0.0f;
    m_current_offset_y = 0.0f;
    m_has_pending = false;
    m_pending_direction = direction{};
    m_pending_duration = 0;
}

//=============================================================================
// get_direction_start_offset - get pixel offset for movement start position
//
// When an entity moves in direction D, they came FROM the opposite tile.
// This returns the pixel offset from the current tile's center where
// the entity should START their interpolation.
//
// Direction mapping:
//   1 = North     (entity came from South)    -> offset (0, +32)
//   2 = NorthEast (entity came from SouthWest)-> offset (-32, +32)
//   3 = East      (entity came from West)     -> offset (-32, 0)
//   4 = SouthEast (entity came from NorthWest)-> offset (-32, -32)
//   5 = South     (entity came from North)    -> offset (0, -32)
//   6 = SouthWest (entity came from NorthEast)-> offset (+32, -32)
//   7 = West      (entity came from East)     -> offset (+32, 0)
//   8 = NorthWest (entity came from SouthEast)-> offset (+32, +32)
//=============================================================================
void EntityMotion::get_direction_start_offset(direction dir, int16_t& outX, int16_t& outY)
{
    constexpr int16_t T = MovementTiming::TILE_SIZE;

    switch (dir) {
        case north: // North - came from South
            outX = 0;
            outY = T;
            break;
        case northeast: // NorthEast - came from SouthWest
            outX = -T;
            outY = T;
            break;
        case east: // East - came from West
            outX = -T;
            outY = 0;
            break;
        case southeast: // SouthEast - came from NorthWest
            outX = -T;
            outY = -T;
            break;
        case south: // South - came from North
            outX = 0;
            outY = -T;
            break;
        case southwest: // SouthWest - came from NorthEast
            outX = T;
            outY = -T;
            break;
        case west: // West - came from East
            outX = T;
            outY = 0;
            break;
        case northwest: // NorthWest - came from SouthEast
            outX = T;
            outY = T;
            break;
        default:
            outX = 0;
            outY = 0;
            break;
    }
}

//=============================================================================
// get_duration_for_action - get movement duration for an action type
//=============================================================================
uint32_t EntityMotion::get_duration_for_action(int action, bool hasHaste, bool frozen)
{
    uint32_t baseDuration;

    switch (action) {
        case Type::Move:
            baseDuration = MovementTiming::WALK_DURATION_MS;
            break;
        case Type::Run:
            baseDuration = MovementTiming::RUN_DURATION_MS;
            break;
        case Type::DamageMove:
            baseDuration = MovementTiming::DAMAGE_MOVE_DURATION_MS;
            break;
        case Type::AttackMove:
            baseDuration = MovementTiming::ATTACK_MOVE_DURATION_MS;
            break;
        default:
            baseDuration = MovementTiming::WALK_DURATION_MS;
            break;
    }

    // Apply status modifiers
    if (hasHaste) {
        // Haste speeds up movement by ~30%
        baseDuration = static_cast<uint32_t>(baseDuration * 0.7f);
    }
    if (frozen) {
        // Frozen slows movement by ~25%
        baseDuration = static_cast<uint32_t>(baseDuration * 1.25f);
    }

    return baseDuration;
}
