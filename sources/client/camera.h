// camera.h: Game camera/viewport management
//
// Handles smooth camera movement, tracking player position, and viewport calculations.
// The camera smoothly interpolates towards its target destination with deceleration.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <cstdlib>
#include <cmath>

class CCamera
{
public:
    CCamera();

    // reset camera to initial state
    void reset();

    // update camera position (smooth interpolation towards destination)
    // Call this once per frame with current game time in milliseconds
    void update(uint32_t currentTime);

    //------------------------------------------------------------------
    // Position Accessors
    //------------------------------------------------------------------

    // get current camera position in world pixels
    int get_x() const { return m_position_x; }
    int get_y() const { return m_position_y; }

    // get target destination in world pixels
    int get_destination_x() const { return m_destination_x; }
    int get_destination_y() const { return m_destination_y; }

    //------------------------------------------------------------------
    // Position Modifiers
    //------------------------------------------------------------------

    // Snap camera immediately to position (no interpolation)
    void set_position(int x, int y);

    // Set target destination (camera will smoothly move there)
    void set_destination(int x, int y);

    // Snap both position and destination (teleport)
    void snap_to(int x, int y);

    // Move destination by delta (relative movement)
    void move_destination(int dx, int dy);

    //------------------------------------------------------------------
    // Utility Methods
    //------------------------------------------------------------------

    // Center camera on a tile position
    // viewCenterTileX/Y are the center offsets (typically VIEW_CENTER_TILE_X()/Y)
    void center_on_tile(int tileX, int tileY, int viewCenterTileX, int viewCenterTileY);

    // Convert world coordinates to screen coordinates
    int world_to_screen_x(int worldX) const { return worldX - m_position_x; }
    int world_to_screen_y(int worldY) const { return worldY - m_position_y; }

    // Convert screen coordinates to world coordinates
    int screen_to_world_x(int screenX) const { return screenX + m_position_x; }
    int screen_to_world_y(int screenY) const { return screenY + m_position_y; }

    //------------------------------------------------------------------
    // Camera Effects
    //------------------------------------------------------------------

    // Set shake intensity (will decrement over time)
    void set_shake(int degree) { m_shake_degree = degree; }

    // get current shake degree
    int get_shake_degree() const { return m_shake_degree; }

    // Apply and update camera shake effect
    // Saves position, applies random shake based on current degree, then decrements degree
    // Returns true if shake was applied (caller should restore_position after rendering)
    bool apply_shake();

    // save current position (for shake effect)
    void save_position();

    // Restore saved position (after shake effect)
    void restore_position();

private:
    // Current camera position in world pixels
    int m_position_x;
    int m_position_y;

    // Target destination in world pixels
    int m_destination_x;
    int m_destination_y;

    // Last update timestamp for delta time calculation
    uint32_t m_last_update_time;

    // Saved position for shake effect restoration
    int m_saved_position_x;
    int m_saved_position_y;

    // Camera shake degree (decrements each frame)
    int m_shake_degree;
};
