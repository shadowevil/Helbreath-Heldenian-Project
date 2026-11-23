// entity_render_state.h: Temporary state for the entity currently being rendered
//
// This class encapsulates the _tmp_* global variables that were used to hold
// the current entity's data during the render loop. It provides a clean
// interface for accessing entity properties during drawing operations.
//////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <array>
#include <cstring>

#include "appearance.h"
#include "player_status_data.h"
#include "owner_type.h"
#include "direction_helpers.h"

using hb::shared::direction::direction;

// Maximum length for entity names
constexpr int ENTITY_NAME_LENGTH = 12;

class CEntityRenderState
{
public:
    CEntityRenderState() { reset(); }

    void reset()
    {
        m_object_id = 0;
        m_owner_type = 0;
        m_npc_config_id = -1;
        m_appearance.clear();
        m_status.clear();
        m_action = 0;
        m_dir = direction{};
        m_frame = 0;
        m_name.fill('\0');
        m_chat_index = 0;
        m_move_offset_x = 0;
        m_move_offset_y = 0;
        m_data_x = 0;
        m_data_y = 0;
        m_effect_type = 0;
        m_effect_frame = 0;
    }

    // get name as C-string
    const char* get_name() const { return m_name.data(); }

    //------------------------------------------------------------------
    // Entity Type Helpers
    //------------------------------------------------------------------

    bool is_player() const { return hb::shared::owner::is_player(m_owner_type); }
    bool is_npc() const { return hb::shared::owner::is_npc(m_owner_type); }
    bool is_male() const { return hb::shared::owner::is_male(m_owner_type); }
    bool is_female() const { return hb::shared::owner::is_female(m_owner_type); }

    //------------------------------------------------------------------
    // Member Variables
    //------------------------------------------------------------------

    // Identification
    uint16_t m_object_id;       // Entity's unique object ID
    short m_owner_type;         // Entity type (player/mob/NPC type ID)
    short m_npc_config_id;       // NPC config index (-1 if player or unknown)

    // Unpacked appearance (named fields extracted from sAppr1-4 at packet reception)
    hb::shared::entity::PlayerAppearance m_appearance;

    // State
    hb::shared::entity::PlayerStatus m_status;
    int8_t m_action;           // Current action (idle, walk, attack, etc.)
    direction m_dir;           // Facing direction (1-8)
    int8_t m_frame;            // Current animation frame

    // Display
    std::array<char, ENTITY_NAME_LENGTH> m_name;  // Entity name
    int m_chat_index;           // Index into chat message array (0 = no message)

    // Position/Movement
    int m_move_offset_x;         // Pixel offset during movement (was _tmp_dx)
    int m_move_offset_y;         // Pixel offset during movement (was _tmp_dy)
    int m_data_x;               // Map data array X index (was _tmp_dX)
    int m_data_y;               // Map data array Y index (was _tmp_dY)

    // Effects
    int m_effect_type;          // Visual effect type (aura, spell, etc.)
    int m_effect_frame;         // Visual effect animation frame

};
