// Screen_OnGame.h: Main gameplay screen
//
// Handles in-game rendering and input processing when player is in the world
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "IGameScreen.h"
#include "GuildManager.h"
#include "DialogBoxManager.h"
#include "NetworkMessageManager.h"
#include "FishingManager.h"
#include "CraftingManager.h"
#include "QuestManager.h"
#include "PlayerRenderer.h"
#include "NpcRenderer.h"
#include "FloatingTextManager.h"
#include "PlayerStatusData.h"
#include <cstdint>
#include <memory>
#include "ChatMsg.h"
#include "GameConstants.h"
#include "GlobalDef.h"

class CPlayer;

class Screen_OnGame : public IGameScreen
{
public:
    SCREEN_TYPE(Screen_OnGame)

    explicit Screen_OnGame(CGame* game);
    ~Screen_OnGame() override = default;

    void on_initialize() override;
    void on_uninitialize() override;
    void on_update() override;
    void on_render() override;
    bool on_text_input(uint32_t codepoint) override;
    bool on_key_down(KeyCode key) override;
    bool on_key_up(KeyCode key) override;
    bool on_game_msg(uint32_t msg_id, uint16_t msg_type, char* data, uint32_t msg_size) override;

    void handle_create_new_guild_response(char* data);
    void handle_disband_guild_response(char* data);

    void item_drop_external_screen(char item_id, short mouse_x, short mouse_y);
    guild_manager& get_guild_manager() { return m_guild_manager; }
    DialogBoxManager& get_dialog_box_manager() { return *m_dialog_box_manager; }
    fishing_manager& get_fishing_manager() { return m_fishing_manager; }
    crafting_manager& get_crafting_manager() { return m_crafting_manager; }
    quest_manager& get_quest_manager() { return m_quest_manager; }
    CPlayerRenderer& get_player_renderer() { return m_player_renderer; }
    CNpcRenderer& get_npc_renderer() { return m_npc_renderer; }
    floating_text_manager& get_floating_text() { return m_floating_text; }

    // Player lifecycle — static because there's only ever one player
    static void create_player();
    static void destroy_player();
    static CPlayer* get_player() { return s_player.get(); }

    // Hotkey registration (Screen_OnGame.Hotkeys.cpp)
    void register_hotkeys();

    // Gameplay draw methods (Screen_OnGame.DrawObjects.cpp)
    void draw_objects(short pivot_x, short pivot_y, short div_x, short div_y, short mod_x, short mod_y, short mouse_x, short mouse_y);
    void draw_background(short div_x, short mod_x, short div_y, short mod_y);
    void draw_top_msg();
    static void draw_character_body(CGame& game, short sX, short sY, short type);
    void draw_object_name(short screen_x, short screen_y, const char* name, const hb::shared::entity::PlayerStatus& status, uint16_t object_id);
    void draw_npc_name(short screen_x, short screen_y, short owner_type, const hb::shared::entity::PlayerStatus& status, short npc_config_id = -1);
    void draw_object_foe(int ix, int iy, int frame);
    void draw_angel(int sprite, short sX, short sY, char frame, uint32_t time);
    void dk_glare(int weapon_color, int16_t weapon_item_id, int* weapon_glare);
    void abaddon_corpse(int sX, int sY);
    hb::shared::sprite::BoundRect draw_object_on_stop(int indexX, int indexY, int sX, int sY, bool trans, uint32_t time);
    hb::shared::sprite::BoundRect draw_object_on_move(int indexX, int indexY, int sX, int sY, bool trans, uint32_t time);
    hb::shared::sprite::BoundRect draw_object_on_run(int indexX, int indexY, int sX, int sY, bool trans, uint32_t time);
    hb::shared::sprite::BoundRect draw_object_on_attack(int indexX, int indexY, int sX, int sY, bool trans, uint32_t time);
    hb::shared::sprite::BoundRect draw_object_on_attack_move(int indexX, int indexY, int sX, int sY, bool trans, uint32_t time);
    hb::shared::sprite::BoundRect draw_object_on_magic(int indexX, int indexY, int sX, int sY, bool trans, uint32_t time);
    hb::shared::sprite::BoundRect draw_object_on_get_item(int indexX, int indexY, int sX, int sY, bool trans, uint32_t time);
    hb::shared::sprite::BoundRect draw_object_on_damage(int indexX, int indexY, int sX, int sY, bool trans, uint32_t time);
    hb::shared::sprite::BoundRect draw_object_on_damage_move(int indexX, int indexY, int sX, int sY, bool trans, uint32_t time);
    hb::shared::sprite::BoundRect draw_object_on_dying(int indexX, int indexY, int sX, int sY, bool trans, uint32_t time);
    hb::shared::sprite::BoundRect draw_object_on_dead(int indexX, int indexY, int sX, int sY, bool trans, uint32_t time);

private:
    // Complex hotkey handlers (Screen_OnGame.Hotkeys.cpp)
    void hotkey_use_health_potion();
    void hotkey_use_mana_potion();
    void hotkey_tab_toggle_combat();
    void hotkey_escape();
    void hotkey_special_ability();
    void hotkey_load_backup_chat();
    void hotkey_whisper_cycle_up();
    void hotkey_whisper_cycle_down();
    void hotkey_toggle_force_attack();
    void hotkey_cycle_detail_level();
    void hotkey_toggle_sound_and_music();
    void hotkey_whisper_target();

    void render_item_tooltip();
    void draw_tile_grid();           // Simple dark grid lines
    void draw_patching_grid();       // Debug grid with zone colors
    void draw_spell_target_overlay(); // Spell AoE targeting overlay (debug only)

public:
    // --- Gameplay state (moved from CGame) ---
    bool m_is_get_pointing_mode = false;
    bool m_wait_for_new_click = false;
    uint32_t m_magic_cast_time = 0;
    int m_point_command_type = -1;
    bool m_skill_using_status = false;
    bool m_item_using_status = false;
    bool m_using_slate = false;
    int m_down_skill_index = -1;
    int m_ilusion_owner_h = 0;
    char m_ilusion_owner_type = 0;
    int m_draw_flag = 0;
    bool m_is_crusade_mode = false;
    uint32_t m_env_effect_time = 0;
    std::array<std::unique_ptr<CMsg>, game_limits::max_text_dlg_lines> m_msg_text_list;
    std::array<std::unique_ptr<CMsg>, game_limits::max_text_dlg_lines> m_msg_text_list2;
    std::array<std::unique_ptr<CMsg>, game_limits::max_text_dlg_lines> m_agree_msg_text_list;
    int m_logout_count = -1;
    uint32_t m_logout_count_time = 0;
    int m_fightzone_number = 0;
    int m_fightzone_number_temp = 0;
    struct {
        bool is_quest_completed = false;
        short who = 0, quest_type = 0, contribution = 0, target_type = 0, target_count = 0, x = 0, y = 0, range = 0;
        short current_count = 0;
        std::string target_name;
    } m_quest;
    bool m_is_observer_mode = false;
    bool m_is_observer_commanded = false;
    uint32_t m_special_ability_setting_time = 0;
    bool m_is_f1_help_window_enabled = false;
    struct {
        short x = 0, y = 0;
        char type = 0;
        char side = 0;
    } m_crusade_structure_info[hb::shared::limits::MaxCrusadeStructures]{};
    uint32_t m_commander_command_requested_time = 0;
    unsigned char m_top_msg_last_sec = 0;
    uint32_t m_top_msg_time = 0;
    std::string m_top_msg;
    int m_gate_posit_x = -1;
    int m_gate_posit_y = -1;
    int m_heldenian_aresden_left_tower = -1;
    int m_heldenian_elvine_left_tower = -1;
    int m_heldenian_aresden_flags = -1;
    int m_heldenian_elvine_flags = -1;
    bool m_is_xmas = false;
    int m_total_party_member = 0;
    int m_party_status = 0;
    int m_gizon_item_upgrade_left = 0;

private:
    // Screen-specific state (previously file-scope static variables)
    short m_sMsX = 0;
    short m_sMsY = 0;
    short m_sMsZ = 0;
    char m_cLB = 0;
    char m_cRB = 0;
    uint32_t m_time = 0;
    short m_sDivX = 0;
    short m_sModX = 0;
    short m_sDivY = 0;
    short m_sModY = 0;
    short m_pivot_x = 0;
    short m_pivot_y = 0;
    uint32_t m_dwPrevChatTime = 0;
    uint32_t m_dwLastBubbleTime = 0;
    guild_manager m_guild_manager;
    fishing_manager m_fishing_manager;
    crafting_manager m_crafting_manager;
    quest_manager m_quest_manager;
    std::unique_ptr<DialogBoxManager> m_dialog_box_manager;
    std::unique_ptr<NetworkMessageManager> m_network_message_manager;

    // Entity renderers and floating text (gameplay-only)
    CPlayerRenderer m_player_renderer;
    CNpcRenderer m_npc_renderer;
    floating_text_manager m_floating_text;

    static std::unique_ptr<CPlayer> s_player;
};
