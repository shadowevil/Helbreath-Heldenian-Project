// EntityManager.h: interface for the CEntityManager class.
//
// Manages all NPC/entity spawning, deletion, and behavior.
// Extracted from Game.cpp to improve maintainability and fix spawn bugs.
//
//////////////////////////////////////////////////////////////////////

#pragma once
#include "CommonTypes.h"
#include "EntityRelationship.h"

#include "Npc.h"
#include "Map.h"
#include "Game.h"  // For hb::server::config::MaxNpcs

struct DropTable;

class CEntityManager
{
public:
    CEntityManager();
    ~CEntityManager();

    // ========================================================================
    // Core Spawn System
    // ========================================================================

    /**
     * Main spawn generation loop. Called each game tick.
     * Processes all maps and spawn points, creating new NPCs as needed.
     */
    void process_spawns();

    /**
     * Create a new NPC entity.
     *
     * @param npc_config_id - Index into m_npc_config_list[]
     * @param name - Unique ID (e.g., "_A0001")
     * @param map_name - Map to spawn on
     * @param sClass - Movement class
     * @param sa - Special ability
     * @param move_type - Movement behavior
     * @param offset_x, offset_y - Spawn position (output)
     * @param waypoint_list - Waypoint array
     * @param area - Spawn area bounds
     * @param spot_mob_index - Spawn point index (0=random, 1-99=spot)
     * @param change_side - Faction override
     * @param hide_gen_mode - Hide if players nearby
     * @param is_summoned - Summoned or natural spawn
     * @param firm_berserk - start with berserk
     * @param is_master - Guild war summon
     * @param guild_guid - Guild ID
     * @return NPC handle (1-4999) or -1 on failure
     */
    int create_entity(
        int npc_config_id, char* name, char* map_name,
        short sClass, char sa, char move_type,
        int* offset_x, int* offset_y,
        char* waypoint_list, hb::shared::geometry::GameRectangle* area,
        int spot_mob_index, char change_side,
        bool hide_gen_mode, bool is_summoned,
        bool firm_berserk, bool is_master,
        bool bypass_mob_limit = false
    );

    /**
     * Remove entity from world and free resources.
     * Handles item drops, counter updates, and cleanup.
     *
     * @param entity_handle - Entity to delete (1-4999)
     */
    void delete_entity(int entity_handle);

    /**
     * Handle entity death event.
     * Awards XP, updates quests, generates loot.
     *
     * @param entity_handle - Entity that died
     * @param attacker_h - Attacker handle
     * @param attacker_type - Attacker type (player/npc)
     * @param damage - Final damage dealt
     */
    void on_entity_killed(int entity_handle, short attacker_h, char attacker_type, short damage);

    // ========================================================================
    // Update & Behavior System
    // ========================================================================

    /**
     * Main entity update loop. Called each game tick.
     * Processes all active entities, updates AI, timers, etc.
     */
    void process_entities();

    /**
     * execute behavior for dead entity (respawn timer).
     */
    void update_dead_behavior(int entity_handle);

    /**
     * execute movement behavior.
     */
    void update_move_behavior(int entity_handle);

    /**
     * execute combat behavior.
     */
    void update_attack_behavior(int entity_handle);

    /**
     * execute idle behavior.
     */
    void update_stop_behavior(int entity_handle);

    /**
     * execute flee behavior.
     */
    void update_flee_behavior(int entity_handle);

    // ========================================================================
    // NPC Behavior & Helpers
    // ========================================================================

    void npc_behavior_move(int npc_h);
    void target_search(int npc_h, short* target, char* target_type);
    void npc_behavior_attack(int npc_h);
    void npc_behavior_flee(int npc_h);
    void npc_behavior_stop(int npc_h);
    void npc_behavior_dead(int npc_h);
    void calc_next_waypoint_destination(int npc_h);
    void npc_magic_handler(int npc_h, short dX, short dY, short type);
    EntityRelationship get_npc_relationship(int npc_h, int viewer_h);
    void npc_request_assistance(int npc_h);
    bool npc_behavior_mana_collector(int npc_h);
    bool npc_behavior_detector(int npc_h);
    void npc_behavior_grand_magic_generator(int npc_h);
    bool set_npc_follow_mode(char* name, char* follow_name, char follow_owner_type);
    void set_npc_attack_mode(char* name, int target_h, char target_type, bool is_perm_attack);

    // ========================================================================
    // Query & Access
    // ========================================================================

    /**
     * get entity pointer by handle.
     * @return CNpc pointer or NULL if invalid
     */
    CNpc* get_entity(int entity_handle) const;

    /**
     * get entity pointer by GUID.
     * @return CNpc pointer or NULL if not found
     */
    CNpc* get_entity_by_guid(uint32_t guid) const;

    /**
     * get entity handle by GUID.
     * @return Entity handle (1-4999) or -1 if not found
     */
    int get_entity_handle_by_guid(uint32_t guid) const;

    /**
     * get entity GUID by handle.
     * @return GUID or 0 if invalid handle
     */
    uint32_t get_entity_guid(int entity_handle) const;

    /**
     * get total active entities across all maps.
     */
    int get_total_active_entities() const;

    /**
     * get total entities on specific map.
     */
    int get_map_entity_count(int map_index) const;

    /**
     * Find entity by name.
     * @return Entity handle or -1 if not found
     */
    int find_entity_by_name(const char* name) const;

    /**
     * get active entity list for efficient iteration.
     * Returns array of active entity indices (not handles!).
     * Use with get_active_entity_count() to iterate only active entities.
     *
     * Performance: O(active_count) instead of O(hb::server::config::MaxNpcs)
     *
     * Example:
     *   int* pActiveList = m_entity_manager->get_active_entity_list();
     *   int count = m_entity_manager->get_active_entity_count();
     *   for(int i = 0; i < count; i++) {
     *       int handle = pActiveList[i];
     *       npc_process(handle);
     *   }
     */
    int* get_active_entity_list() const { return m_active_entity_list; }

    /**
     * get number of active entities.
     * Use with get_active_entity_list() for efficient iteration.
     */
    int get_active_entity_count() const { return m_active_entity_count; }

    /**
     * get direct access to entity array for backward compatibility.
     * WARNING: Direct array access bypasses EntityManager logic.
     * Prefer using get_entity() or get_active_entity_list().
     *
     * @return CNpc** array (indices 0-4999, index 0 unused)
     */
    CNpc** get_entity_array() const { return m_npc_list; }

    // ========================================================================
    // Configuration
    // ========================================================================

    /**
     * Set map list pointer (from CGame).
     * Required for spawn system to access map data.
     */
    void set_map_list(class CMap** map_list, int max_maps);

    /**
     * Set game instance pointer for callbacks.
     */
    void set_game(class CGame* game);

private:
    // ========================================================================
    // Internal Helpers
    // ========================================================================

    /**
     * initialize NPC attributes from configuration.
     */
    static bool is_crusade_structure(int npc_type);

    // Extracted helpers from long functions
    bool find_spawn_position(int map_index, char move_type,
        int* offset_x, int* offset_y, char* waypoint_list,
        hb::shared::geometry::GameRectangle* area,
        bool hide_gen_mode, short& out_x, short& out_y);
    void setup_entity_appearance(CNpc* npc);
    void award_kill_experience(int entity_handle, short attacker_h, char attacker_type);
    void apply_crusade_contribution(int entity_handle, short attacker_h, char attacker_type);
    bool try_magic_attack(int npc_h, int target_h, char target_type, short sX, short sY, short dX, short dY, bool& out_skip_ranged);
    bool try_ranged_attack(int npc_h, int target_h, char target_type, short sX, short sY, short dX, short dY);
    void spawn_follower_mobs(int map_index, int npc_config_id, int total_mob,
        bool firm_berserk, int prob_sa, int kind_sa,
        const char* master_name, int pX, int pY);

    bool init_entity_attributes(CNpc* npc, int npc_config_id, short sClass, char sa);

    /**
     * Find first available entity slot.
     * @return Slot index (1-4999) or -1 if full
     */
    int get_free_entity_slot() const;

    /**
     * Validate entity handle is in range and exists.
     */
    bool is_valid_entity(int entity_handle) const;

    /**
     * Generate item drops when entity dies.
     */
    void generate_entity_loot(int entity_handle, short attacker_h, char attacker_type);

    /**
     * Internal cleanup for entity deletion (drops, counters, naming value).
     */
    void delete_npc_internal(int npc_h);

    void npc_dead_item_generator(int npc_h, short attacker_h, char attacker_type);
    int roll_drop_table_item(const DropTable* table, int tier, int& outMinCount, int& outMaxCount) const;
    bool spawn_npc_drop_item(int npc_h, int item_id, int min_count, int max_count);

    // ========================================================================
    // Spawn Point Management
    // ========================================================================

    /**
     * Process random mob generators for a map.
     */
    void process_random_spawns(int map_index);

    /**
     * Process spot mob generators for a map.
     */
    void process_spot_spawns(int map_index);

    /**
     * Check if spawn point can spawn (limits, timers).
     */
    bool can_spawn_at_spot(int map_index, int spot_index) const;

    /**
     * Generate next unique GUID for new entity.
     */
    uint32_t generate_entity_guid();

    // ========================================================================
    // Data Members
    // ========================================================================

    // Entity Storage (OWNED by EntityManager)
    class CNpc** m_npc_list;                // Entity array (indices 0-4999, index 0 unused)
    uint32_t m_entity_guid[hb::server::config::MaxNpcs];  // GUID for each entity slot

    // Performance: Active Entity Tracking
    // Instead of iterating 5,000 slots, iterate only active entities
    int* m_active_entity_list;               // Indices of active entities
    int m_active_entity_count;               // Number of active entities

    // External References
    class CMap** m_map_list;                // Reference to map list (from CGame)
    class CGame* m_game;                   // Reference to game instance
    int m_max_maps;                         // Number of maps

    // Entity Statistics
    int m_total_entities;                   // Total active entities (same as m_active_entity_count)
    uint32_t m_next_guid;                     // Next GUID to assign (monotonically increasing)

    bool m_initialized;                    // Initialization flag

    // ========================================================================
    // Active Entity List Management (Private Helpers)
    // ========================================================================

    /**
     * Add entity to active list when created.
     * Called by create_entity().
     */
    void add_to_active_list(int entity_handle);

    /**
     * Remove entity from active list when deleted.
     * Called by delete_entity().
     */
    void remove_from_active_list(int entity_handle);
};
