// cursor_target.h: Cursor targeting and object focus system
//
// Provides static global access to targeting state with frame-based semantics.
// Handles mouse hit testing, cursor appearance, and focused object tracking.
//////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <string>
#include "sprite_types.h"
#include "appearance.h"
#include "player_status_data.h"
#include "direction_helpers.h"

using hb::shared::direction::direction;

//=============================================================================
// Cursor Types — maps 1:1 to sprite frames in MouseCursor sprite sheet
//=============================================================================
enum class cursor_type : int
{
    arrow           = 0,   // Standard arrow cursor
    grab_open       = 1,   // Open hand — item pickup (animation frame 1)
    grab_closed     = 2,   // Closed hand — item pickup (animation frame 2)
    attack          = 3,   // Sword — target is attackable / friendly attack toggled
    spell_friendly  = 4,   // Magic cast on friendly target
    spell_hostile   = 5,   // Magic cast on enemy target
    arrow_blue      = 6,   // Arrow with blue outline — friendly/neutral hover
    arrow_red       = 7,   // Arrow with red outline — unused, reserved
    hourglass       = 8,   // Waiting / loading
    item_target     = 9,   // Item-on-item targeting (animation frame 1 of 3)
    item_target_2   = 10,  // Item-on-item targeting (animation frame 2 of 3)
    item_target_3   = 11,  // Item-on-item targeting (animation frame 3 of 3)
};

//=============================================================================
// Focused Object Type
//=============================================================================
enum class FocusedObjectType {
    None,
    Player,
    NPC,
    DeadBody,
    GroundItem,
    DynamicObject
};

//=============================================================================
// Focused Object Information
//=============================================================================
struct FocusedObject {
    bool m_valid = false;

    // Identification
    uint16_t m_object_id = 0;
    short m_map_x = 0, m_map_y = 0;
    short m_screen_x = 0, m_screen_y = 0;
    short m_data_x = 0, m_data_y = 0;  // Map data array indices

    // Type info
    FocusedObjectType m_type = FocusedObjectType::None;
    short m_owner_type = 0;
    short m_npc_config_id = -1;
    char m_action = 0;
    direction m_direction = direction{};
    char m_frame = 0;

    // Display info
    std::string m_name;
    hb::shared::entity::PlayerAppearance m_appearance;
    hb::shared::entity::PlayerStatus m_status;

    // Query helpers
    bool is_dead() const {
        return m_type == FocusedObjectType::DeadBody;
    }
};

//=============================================================================
// Object Info for Testing (passed during iteration)
//=============================================================================
struct TargetObjectInfo {
    uint16_t m_object_id;
    short m_map_x, m_map_y;
    short m_screen_x, m_screen_y;
    short m_data_x, m_data_y;  // Map data array indices
    short m_owner_type;
    short m_npc_config_id;
    char m_action, m_frame;
    direction m_direction;
    const char* m_name;  // Points to existing string, no copy
    hb::shared::entity::PlayerAppearance m_appearance;
    hb::shared::entity::PlayerStatus m_status;
    FocusedObjectType m_type;
};

//=============================================================================
// UI Selection Types (for item/dialog dragging)
//=============================================================================
enum class SelectedObjectType : char {
    None = 0,
    DialogBox = 1,
    Item = 2
};

//=============================================================================
// Cursor Interaction Status (mouse button state machine)
// Values match DEF_CURSORSTATUS_* defines in game.h
//=============================================================================
enum class CursorStatus : char {
    Null = 0,      // No interaction
    Pressed = 1,   // Mouse pressed outside dialog
    Selected = 2,  // Mouse pressed on dialog (selection started)
    Dragging = 3   // Dragging item/dialog
};

//=============================================================================
// Cursor Target System API
//=============================================================================
namespace CursorTarget {
    //-------------------------------------------------------------------------
    // Frame Lifecycle
    //-------------------------------------------------------------------------
    // Call at start of draw_objects() to reset state
    void begin_frame();

    // Call at end of draw_objects() to finalize cursor type
    // relationship: server-computed relationship of focused entity (Neutral if no focus)
    // commandType: m_point_command_type
    // commandAvailable: m_command_available
    // get_pointing_mode: m_is_get_pointing_mode
    void end_frame(EntityRelationship relationship, int commandType, bool commandAvailable, bool get_pointing_mode);

    //-------------------------------------------------------------------------
    // Object Testing (called during draw_objects iteration)
    //-------------------------------------------------------------------------
    // Test if mouse is over object's bounding rect
    // screenY used for depth sorting (lower Y = further back)
    // maxScreenY: bottom boundary for valid targeting (typically LOGICAL_HEIGHT() - 49)
    void test_object(const hb::shared::sprite::BoundRect& bounds, const TargetObjectInfo& info, int screenY, int maxScreenY);

    // Test ground item with circular proximity (13px radius)
    void test_ground_item(int screenX, int screenY, int maxScreenY);

    // Test dynamic object (minerals, etc)
    void test_dynamic_object(const hb::shared::sprite::BoundRect& bounds, short mapX, short mapY, int maxScreenY);

    //-------------------------------------------------------------------------
    // Query Functions
    //-------------------------------------------------------------------------
    const FocusedObject& GetFocusedObject();
    bool has_focused_object();

    cursor_type get_cursor_type();

    // Map coordinates (for m_mcx/m_mcy compatibility)
    short get_focused_map_x();
    short get_focused_map_y();
    const char* get_focused_name();  // Returns pointer to internal name buffer

    // Ground item hover state
    bool is_over_ground_item();

    //-------------------------------------------------------------------------
    // Focus Highlight Data (for redrawing focused object)
    //-------------------------------------------------------------------------
    // Returns full appearance data needed to redraw focused object
    // with transparency highlight
    bool get_focus_highlight_data(
        short& outScreenX, short& outScreenY,
        uint16_t& outObjectID,
        short& outOwnerType, short& outNpcConfigId,
        char& outAction, direction& outDir, char& outFrame,
        hb::shared::entity::PlayerAppearance& outAppearance, hb::shared::entity::PlayerStatus& outStatus,
        short& outDataX, short& outDataY
    );

    // get focus status for relationship lookup
    const hb::shared::entity::PlayerStatus& GetFocusStatus();

    //-------------------------------------------------------------------------
    // Utilities
    //-------------------------------------------------------------------------
    bool point_in_rect(int x, int y, const hb::shared::sprite::BoundRect& rect);
    bool point_in_circle(int x, int y, int cx, int cy, int radius);

    //-------------------------------------------------------------------------
    // UI Selection State (for item/dialog dragging)
    //-------------------------------------------------------------------------
    // Set selection when user clicks/drags an item or dialog
    void set_selection(SelectedObjectType type, short objectID, short distX, short distY);
    void clear_selection();

    // Query selection state
    SelectedObjectType GetSelectedType();
    short get_selected_id();
    short get_drag_dist_x();
    short get_drag_dist_y();
    bool has_selection();

    // Selection click tracking (for double-click detection)
    void record_selection_click(short x, short y, uint32_t time);
    void reset_selection_click_time();
    uint32_t get_selection_click_time();
    short get_selection_click_x();
    short get_selection_click_y();

    // Previous position tracking (for drag delta)
    void set_prev_position(short x, short y);
    short get_prev_x();
    short get_prev_y();

    // Cursor interaction status (mouse button state machine)
    void set_cursor_status(CursorStatus status);
    CursorStatus GetCursorStatus();
}
