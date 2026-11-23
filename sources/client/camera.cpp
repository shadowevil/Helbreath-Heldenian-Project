// camera.cpp: Game camera/viewport management implementation
//
//////////////////////////////////////////////////////////////////////

#include "camera.h"

CCamera::CCamera()
{
    reset();
}

void CCamera::reset()
{
    m_position_x = 0;
    m_position_y = 0;
    m_destination_x = 0;
    m_destination_y = 0;
    m_last_update_time = 0;
    m_saved_position_x = 0;
    m_saved_position_y = 0;
    m_shake_degree = 0;
}

void CCamera::update(uint32_t currentTime)
{
    int dx = m_destination_x - m_position_x;
    int dy = m_destination_y - m_position_y;

    // Already at destination
    if (dx == 0 && dy == 0)
    {
        m_last_update_time = currentTime;
        return;
    }

    float dt = (currentTime - m_last_update_time) * 0.001f;
    m_last_update_time = currentTime;

    // First frame or large time gap — snap immediately
    if (dt <= 0.0f || dt > 0.5f)
    {
        m_position_x = m_destination_x;
        m_position_y = m_destination_y;
        return;
    }

    // Large distance (teleport) — snap immediately
    if (abs(dx) > 128 || abs(dy) > 128)
    {
        m_position_x = m_destination_x;
        m_position_y = m_destination_y;
        return;
    }

    // Exponential lerp: fast tracking with smooth micro-gap bridging
    constexpr float SMOOTH_SPEED = 18.0f;
    float t = 1.0f - expf(-SMOOTH_SPEED * dt);

    int moveX = static_cast<int>(dx * t);
    int moveY = static_cast<int>(dy * t);

    // Snap when sub-pixel close to avoid never arriving
    if (moveX == 0 && moveY == 0)
    {
        m_position_x = m_destination_x;
        m_position_y = m_destination_y;
    }
    else
    {
        m_position_x += moveX;
        m_position_y += moveY;
    }
}

void CCamera::set_position(int x, int y)
{
    m_position_x = x;
    m_position_y = y;
}

void CCamera::set_destination(int x, int y)
{
    m_destination_x = x;
    m_destination_y = y;
}

void CCamera::snap_to(int x, int y)
{
    m_position_x = x;
    m_position_y = y;
    m_destination_x = x;
    m_destination_y = y;
    m_last_update_time = 0;
}

void CCamera::move_destination(int dx, int dy)
{
    m_destination_x += dx;
    m_destination_y += dy;
}

void CCamera::center_on_tile(int tileX, int tileY, int viewCenterTileX, int viewCenterTileY)
{
    int x = (tileX - viewCenterTileX) * 32;
    int y = (tileY - (viewCenterTileY + 1)) * 32;
    snap_to(x, y);
}

bool CCamera::apply_shake()
{
    if (m_shake_degree > 0)
    {
        m_position_x += m_shake_degree - (rand() % (m_shake_degree * 2));
        m_position_y += m_shake_degree - (rand() % (m_shake_degree * 2));
        m_shake_degree--;
        return true;
    }
    return false;
}

void CCamera::save_position()
{
    m_saved_position_x = m_position_x;
    m_saved_position_y = m_position_y;
}

void CCamera::restore_position()
{
    m_position_x = m_saved_position_x;
    m_position_y = m_saved_position_y;
}
