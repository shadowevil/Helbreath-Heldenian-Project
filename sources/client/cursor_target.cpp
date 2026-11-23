// cursor_target.cpp: Cursor targeting and object focus implementation
//
//////////////////////////////////////////////////////////////////////

#include "cursor_target.h"
#include "input.h"
#include <cstring>
#include <chrono>

using namespace hb::shared::direction;

// Internal state (static, not exposed)
namespace {
    // Mouse position (cached from hb::shared::input:: at begin_frame)
    int s_mouseX = 0;
    int s_mouseY = 0;

    // Current frame's focused object
    FocusedObject s_focusedObject;
    int s_bestHitY = -99999;  // Depth sorting - higher Y = closer

    // Ground item state
    bool s_overGroundItem = false;

    // Cursor state
    cursor_type s_cursorType = cursor_type::arrow;

    // Grab cursor animation
    int64_t s_grabAnimTime = 0;
    bool s_grabAnimOpen = true;

    // Item target cursor ping-pong animation (9 → 10 → 11 → 10 → 9 ...)
    int64_t s_itemTargetAnimTime = 0;
    int s_itemTargetStep = 0;                          // index into sequence
    constexpr cursor_type item_target_sequence[] = {
        cursor_type::item_target,
        cursor_type::item_target_2,
        cursor_type::item_target_3,
        cursor_type::item_target_2,
    };

    // UI Selection state (for item/dialog dragging)
    SelectedObjectType s_selectedType = SelectedObjectType::None;
    short s_selectedID = 0;
    short s_dragDistX = 0;
    short s_dragDistY = 0;
    uint32_t s_selectClickTime = 0;
    short s_clickX = 0;
    short s_clickY = 0;
    short s_prevX = 0;
    short s_prevY = 0;

    // Cursor interaction status (mouse button state machine)
    CursorStatus s_cursorStatus = CursorStatus::Null;
}

//-----------------------------------------------------------------------------
// Frame Lifecycle
//-----------------------------------------------------------------------------

void CursorTarget::begin_frame()
{
    // Cache mouse position
    s_mouseX = hb::shared::input::get_mouse_x();
    s_mouseY = hb::shared::input::get_mouse_y();

    // reset focus state
    s_focusedObject = FocusedObject{};
    s_bestHitY = -99999;

    // reset ground item state
    s_overGroundItem = false;

    // reset cursor (will be determined in end_frame)
    s_cursorType = cursor_type::arrow;
}

void CursorTarget::end_frame(EntityRelationship relationship, int commandType, bool commandAvailable, bool get_pointing_mode)
{
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    // Check pointing mode FIRST - spell/item targeting takes priority over ground items
    if (get_pointing_mode) {
        // Spell targeting mode (100-199) - takes priority over everything
        if (commandType >= 100 && commandType < 200) {
            if (commandAvailable) {
                if (s_focusedObject.m_valid && IsHostile(relationship))
                    s_cursorType = cursor_type::spell_hostile;
                else
                    s_cursorType = cursor_type::spell_friendly;
            } else {
                s_cursorType = cursor_type::hourglass;
            }
            return;
        }

        // Item use mode (0-49) — ping-pong animation: 9 → 10 → 11 → 10 → 9 ...
        if (commandType >= 0 && commandType < 50) {
            if (now - s_itemTargetAnimTime > 200) {
                s_itemTargetAnimTime = now;
                s_itemTargetStep = (s_itemTargetStep + 1) % 4;
            }
            s_cursorType = item_target_sequence[s_itemTargetStep];
            return;
        }
    }

    // Entity focus takes priority — mobs/NPCs/players over ground items
    if (s_focusedObject.m_valid) {
        if (IsHostile(relationship) || hb::shared::input::is_ctrl_down())
            s_cursorType = cursor_type::attack;
        else
            s_cursorType = cursor_type::arrow_blue;
        return;
    }

    // Ground item cursor — only when no entity is focused on the tile
    if (s_overGroundItem) {
        if (now - s_grabAnimTime > 200) {
            s_grabAnimTime = now;
            s_grabAnimOpen = !s_grabAnimOpen;
        }
        s_cursorType = s_grabAnimOpen ?
            cursor_type::grab_open : cursor_type::grab_closed;
        return;
    }

    // Default
    s_cursorType = cursor_type::arrow;
}

//-----------------------------------------------------------------------------
// Object Testing
//-----------------------------------------------------------------------------

void CursorTarget::test_object(const hb::shared::sprite::BoundRect& bounds, const TargetObjectInfo& info, int screenY, int maxScreenY)
{
    // Skip invalid bounds
    if (bounds.top == -1) return;

    // Skip if mouse is below the valid targeting area (UI region)
    if (s_mouseY > maxScreenY) return;

    // Hit test - check if mouse is within sprite bounds
    // Note: Original code used < and > exclusively, not <= and >=
    if (s_mouseX > bounds.left && s_mouseX < bounds.right &&
        s_mouseY > bounds.top && s_mouseY < bounds.bottom) {

        // Y-depth sorting: higher screenY = closer to camera = higher priority
        if (screenY < s_bestHitY) return;
        s_bestHitY = screenY;

        // Copy info to focused object
        s_focusedObject.m_valid = true;
        s_focusedObject.m_object_id = info.m_object_id;
        s_focusedObject.m_map_x = info.m_map_x;
        s_focusedObject.m_map_y = info.m_map_y;
        s_focusedObject.m_screen_x = info.m_screen_x;
        s_focusedObject.m_screen_y = info.m_screen_y;
        s_focusedObject.m_data_x = info.m_data_x;
        s_focusedObject.m_data_y = info.m_data_y;
        s_focusedObject.m_owner_type = info.m_owner_type;
        s_focusedObject.m_npc_config_id = info.m_npc_config_id;
        s_focusedObject.m_type = info.m_type;
        s_focusedObject.m_action = info.m_action;
        s_focusedObject.m_direction = info.m_direction;
        s_focusedObject.m_frame = info.m_frame;
        s_focusedObject.m_appearance = info.m_appearance;
        s_focusedObject.m_status = info.m_status;

        // Copy name
        if (info.m_name) {
            s_focusedObject.m_name = info.m_name;
        }
    }
}

void CursorTarget::test_ground_item(int screenX, int screenY, int maxScreenY)
{
    // Skip if mouse is below the valid targeting area
    if (s_mouseY > maxScreenY) return;

    // Check circular proximity (13px radius)
    if (point_in_circle(s_mouseX, s_mouseY, screenX, screenY, 13)) {
        s_overGroundItem = true;
    }
}

void CursorTarget::test_dynamic_object(const hb::shared::sprite::BoundRect& bounds, short mapX, short mapY, int maxScreenY)
{
    // Skip invalid bounds
    if (bounds.top == -1) return;

    // Skip if mouse is below the valid targeting area
    if (s_mouseY > maxScreenY) return;

    // Hit test
    if (s_mouseX > bounds.left && s_mouseX < bounds.right &&
        s_mouseY > bounds.top && s_mouseY < bounds.bottom) {

        // Dynamic objects (minerals) set focus without full object info
        s_focusedObject.m_valid = true;
        s_focusedObject.m_map_x = mapX;
        s_focusedObject.m_map_y = mapY;
        s_focusedObject.m_type = FocusedObjectType::DynamicObject;
        s_focusedObject.m_status.clear();
    }
}

//-----------------------------------------------------------------------------
// Query Functions
//-----------------------------------------------------------------------------

const FocusedObject& CursorTarget::GetFocusedObject()
{
    return s_focusedObject;
}

bool CursorTarget::has_focused_object()
{
    return s_focusedObject.m_valid;
}

cursor_type CursorTarget::get_cursor_type()
{
    return s_cursorType;
}

short CursorTarget::get_focused_map_x()
{
    return s_focusedObject.m_valid ? s_focusedObject.m_map_x : 0;
}

short CursorTarget::get_focused_map_y()
{
    return s_focusedObject.m_valid ? s_focusedObject.m_map_y : 0;
}

const char* CursorTarget::get_focused_name()
{
    return s_focusedObject.m_name.c_str();
}

bool CursorTarget::is_over_ground_item()
{
    return s_overGroundItem;
}

const hb::shared::entity::PlayerStatus& CursorTarget::GetFocusStatus()
{
    return s_focusedObject.m_status;
}

//-----------------------------------------------------------------------------
// Focus Highlight Data
//-----------------------------------------------------------------------------

bool CursorTarget::get_focus_highlight_data(
    short& outScreenX, short& outScreenY,
    uint16_t& outObjectID,
    short& outOwnerType, short& outNpcConfigId,
    char& outAction, direction& outDir, char& outFrame,
    hb::shared::entity::PlayerAppearance& outAppearance, hb::shared::entity::PlayerStatus& outStatus,
    short& outDataX, short& outDataY)
{
    if (!s_focusedObject.m_valid) {
        return false;
    }

    outScreenX = s_focusedObject.m_screen_x;
    outScreenY = s_focusedObject.m_screen_y;
    outObjectID = s_focusedObject.m_object_id;
    outOwnerType = s_focusedObject.m_owner_type;
    outNpcConfigId = s_focusedObject.m_npc_config_id;
    outAction = s_focusedObject.m_action;
    outDir = s_focusedObject.m_direction;
    outFrame = s_focusedObject.m_frame;
    outAppearance = s_focusedObject.m_appearance;
    outStatus = s_focusedObject.m_status;
    outDataX = s_focusedObject.m_data_x;
    outDataY = s_focusedObject.m_data_y;

    return true;
}

//-----------------------------------------------------------------------------
// Utilities
//-----------------------------------------------------------------------------

bool CursorTarget::point_in_rect(int x, int y, const hb::shared::sprite::BoundRect& rect)
{
    return x >= rect.left && x < rect.right &&
           y >= rect.top && y < rect.bottom;
}

bool CursorTarget::point_in_circle(int x, int y, int cx, int cy, int radius)
{
    int dx = x - cx;
    int dy = y - cy;
    return (dx * dx + dy * dy) <= (radius * radius);
}

//-----------------------------------------------------------------------------
// UI Selection State
//-----------------------------------------------------------------------------

void CursorTarget::set_selection(SelectedObjectType type, short objectID, short distX, short distY)
{
    s_selectedType = type;
    s_selectedID = objectID;
    s_dragDistX = distX;
    s_dragDistY = distY;
}

void CursorTarget::clear_selection()
{
    s_selectedType = SelectedObjectType::None;
    s_selectedID = 0;
    s_dragDistX = 0;
    s_dragDistY = 0;
}

SelectedObjectType CursorTarget::GetSelectedType()
{
    return s_selectedType;
}

short CursorTarget::get_selected_id()
{
    return s_selectedID;
}

short CursorTarget::get_drag_dist_x()
{
    return s_dragDistX;
}

short CursorTarget::get_drag_dist_y()
{
    return s_dragDistY;
}

bool CursorTarget::has_selection()
{
    return s_selectedType != SelectedObjectType::None;
}

void CursorTarget::record_selection_click(short x, short y, uint32_t time)
{
    s_selectClickTime = time;
    s_clickX = x;
    s_clickY = y;
}

void CursorTarget::reset_selection_click_time()
{
    s_selectClickTime = 0;
}

uint32_t CursorTarget::get_selection_click_time()
{
    return s_selectClickTime;
}

short CursorTarget::get_selection_click_x()
{
    return s_clickX;
}

short CursorTarget::get_selection_click_y()
{
    return s_clickY;
}

void CursorTarget::set_prev_position(short x, short y)
{
    s_prevX = x;
    s_prevY = y;
}

short CursorTarget::get_prev_x()
{
    return s_prevX;
}

short CursorTarget::get_prev_y()
{
    return s_prevY;
}

//-----------------------------------------------------------------------------
// Cursor Interaction Status
//-----------------------------------------------------------------------------

void CursorTarget::set_cursor_status(CursorStatus status)
{
    s_cursorStatus = status;
}

CursorStatus CursorTarget::GetCursorStatus()
{
    return s_cursorStatus;
}
