// EntityManager.cpp: implementation of the CEntityManager class.

#include "CommonTypes.h"
#include "EntityManager.h"
#include "Game.h"
#include "StatusEffectManager.h"
#include "WarManager.h"
#include "MagicManager.h"
#include "ItemManager.h"
#include "CombatManager.h"
#include "QuestManager.h"
#include "DelayEventManager.h"
#include <cstdio>
#include "Log.h"
#include "StringCompat.h"
#include "TimeUtils.h"

using namespace hb::shared::net;
using namespace hb::shared::action;
using namespace hb::server::config;
using namespace hb::server::npc;
using namespace hb::server::net;
using namespace hb::shared::direction;
namespace smap = hb::server::map;
namespace sdelay = hb::server::delay_event;

using namespace hb::shared::item;

extern char G_cTxt[512];
extern char _tmp_cTmpDirX[9];
extern char _tmp_cTmpDirY[9];
extern int ITEMSPREAD_FIEXD_COORD[25][2];

// Construction/Destruction

CEntityManager::CEntityManager()
{
    // Allocate entity array (EntityManager OWNS this)
    m_npc_list = new CNpc*[MaxNpcs];
    for(int i = 0; i < MaxNpcs; i++) {
        m_npc_list[i] = nullptr;
        m_entity_guid[i] = 0;
    }

    // Allocate active entity tracking list for performance
    m_active_entity_list = new int[MaxNpcs];
    m_active_entity_count = 0;

    m_map_list = nullptr;
    m_game = nullptr;
    m_max_maps = 0;
    m_total_entities = 0;
    m_next_guid = 1; // start GUIDs at 1 (0 = invalid)
    m_initialized = false;
}

CEntityManager::~CEntityManager()
{
    // Delete all entities (EntityManager owns them)
    if (m_npc_list != nullptr) {
        for(int i = 0; i < MaxNpcs; i++) {
            if (m_npc_list[i] != nullptr) {
                delete m_npc_list[i];
                m_npc_list[i] = nullptr;
            }
        }
        delete[] m_npc_list;
        m_npc_list = NULL;
    }

    // Delete active entity tracking list
    if (m_active_entity_list != nullptr) {
        delete[] m_active_entity_list;
        m_active_entity_list = nullptr;
    }
}

// ========================================================================
// Configuration
// ========================================================================

void CEntityManager::set_map_list(CMap** map_list, int max_maps)
{
    m_map_list = map_list;
    m_max_maps = max_maps;
}

void CEntityManager::set_game(CGame* game)
{
    m_game = game;
    m_initialized = (m_npc_list != nullptr && m_map_list != nullptr && m_game != nullptr);
}

// ========================================================================
// Core Spawn System - STUBS
// ========================================================================

void CEntityManager::process_spawns()
{
    if (!m_initialized || m_map_list == nullptr || m_game == nullptr)
        return;

    if (m_game->m_on_exit_process)
        return;

    process_random_spawns(0);

    // Loop through all maps and process their spot spawn generators
    for(int i = 0; i < m_max_maps; i++) {
        if (m_map_list[i] != nullptr) {
            process_spot_spawns(i);
        }
    }
}

int CEntityManager::create_entity(
    int npc_config_id, char* name, char* map_name,
    short sClass, char sa, char move_type,
    int* offset_x, int* offset_y,
    char* waypoint_list, hb::shared::geometry::GameRectangle* area,
    int spot_mob_index, char change_side,
    bool hide_gen_mode, bool is_summoned,
    bool firm_berserk, bool is_master,
    bool bypass_mob_limit)
{
    if (!m_initialized) return -1;
    if (m_game == nullptr) return -1;
    if (strlen(name) == 0) return -1;
    if (npc_config_id < 0 || npc_config_id >= MaxNpcTypes) return -1;

    int t, map_index;
    char tmp_name[11];
    short sX, sY;
    hb::time::local_time SysTime{};

    SysTime = hb::time::local_time::now();
    std::memset(tmp_name, 0, sizeof(tmp_name));
    strcpy(tmp_name, map_name);
    map_index = -1;

    // Find map index
    for(int i = 0; i < m_max_maps; i++)
        if (m_map_list[i] != nullptr) {
            if (memcmp(m_map_list[i]->m_name, tmp_name, 10) == 0)
                map_index = i;
        }

    if (map_index == -1) return -1;

    // Find free entity slot
    for(int i = 1; i < MaxNpcs; i++)
        if (m_npc_list[i] == nullptr) {
            m_npc_list[i] = new CNpc(name);

            // initialize NPC attributes from config
            if (init_entity_attributes(m_npc_list[i], npc_config_id, sClass, sa) == false) {
                hb::logger::log("Invalid NPC creation request (config_id={}), ignored", npc_config_id);
                delete m_npc_list[i];
                m_npc_list[i] = nullptr;
                return -1;
            }

            // Day of week check
            if (m_npc_list[i]->m_day_of_week_limit < 10) {
                if (m_npc_list[i]->m_day_of_week_limit != SysTime.day_of_week) {
                    delete m_npc_list[i];
                    m_npc_list[i] = nullptr;
                    return -1;
                }
            }

            // Determine spawn location, validate, and apply hide-gen check
            if (!find_spawn_position(map_index, move_type, offset_x, offset_y,
                    waypoint_list, area, hide_gen_mode, sX, sY)) {
                delete m_npc_list[i];
                m_npc_list[i] = nullptr;
                return -1;
            }

            // Set entity position
            m_npc_list[i]->m_x = sX;
            m_npc_list[i]->m_y = sY;
            m_npc_list[i]->m_vx = sX;
            m_npc_list[i]->m_vy = sY;

            // Set waypoints
            if (waypoint_list != nullptr) {
                for (t = 0; t < 10; t++)
                    m_npc_list[i]->m_waypoint_index[t] = waypoint_list[t];
            } else {
                for (t = 0; t < 10; t++)
                    m_npc_list[i]->m_waypoint_index[t] = -1;
            }

            m_npc_list[i]->m_total_waypoint = 0;
            for (t = 0; t < 10; t++)
                if (m_npc_list[i]->m_waypoint_index[t] != -1) m_npc_list[i]->m_total_waypoint++;

            // Set random area if provided
            if (area != 0) {
                m_npc_list[i]->m_random_area = *area;
            }

            // Set destination based on move type
            switch (move_type) {
            case MoveType::Guard:
                m_npc_list[i]->m_dx = m_npc_list[i]->m_x;
                m_npc_list[i]->m_dy = m_npc_list[i]->m_y;
                break;

            case MoveType::SeqWaypoint:
                m_npc_list[i]->m_cur_waypoint = 1;
                m_npc_list[i]->m_dx = (short)m_map_list[map_index]->m_waypoint_list[m_npc_list[i]->m_waypoint_index[m_npc_list[i]->m_cur_waypoint]].x;
                m_npc_list[i]->m_dy = (short)m_map_list[map_index]->m_waypoint_list[m_npc_list[i]->m_waypoint_index[m_npc_list[i]->m_cur_waypoint]].y;
                break;

            case MoveType::RandomWaypoint:
                m_npc_list[i]->m_cur_waypoint = (rand() % (m_npc_list[i]->m_total_waypoint - 1)) + 1;
                m_npc_list[i]->m_dx = (short)m_map_list[map_index]->m_waypoint_list[m_npc_list[i]->m_waypoint_index[m_npc_list[i]->m_cur_waypoint]].x;
                m_npc_list[i]->m_dy = (short)m_map_list[map_index]->m_waypoint_list[m_npc_list[i]->m_waypoint_index[m_npc_list[i]->m_cur_waypoint]].y;
                break;

            case MoveType::RandomArea:
                m_npc_list[i]->m_cur_waypoint = 0;
                m_npc_list[i]->m_dx = (short)((rand() % m_npc_list[i]->m_random_area.width) + m_npc_list[i]->m_random_area.x);
                m_npc_list[i]->m_dy = (short)((rand() % m_npc_list[i]->m_random_area.height) + m_npc_list[i]->m_random_area.y);
                break;

            case MoveType::Random:
                m_npc_list[i]->m_dx = (short)((rand() % (m_map_list[map_index]->m_size_x - 50)) + 15);
                m_npc_list[i]->m_dy = (short)((rand() % (m_map_list[map_index]->m_size_y - 50)) + 15);
                break;
            }

            m_npc_list[i]->m_tmp_error = 0;
            m_npc_list[i]->m_move_type = move_type;

            // Set behavior based on action limit
            switch (m_npc_list[i]->m_action_limit) {
            case 2:
            case 3:
            case 5:
                m_npc_list[i]->m_behavior = Behavior::stop;

                switch (m_npc_list[i]->m_type) {
                case 15: // ShopKeeper-W
                case 19: // Gandlf
                case 20: // Howard
                case 24: // Tom
                case 25: // William
                case 26: // Kennedy
                    m_npc_list[i]->m_dir = static_cast<direction>(4 + m_game->dice(1, 3) - 1);
                    break;

                default:
                    m_npc_list[i]->m_dir = static_cast<direction>(m_game->dice(1, 8));
                    break;
                }
                break;

            default:
                m_npc_list[i]->m_behavior = Behavior::Move;
                m_npc_list[i]->m_dir = direction::south;
                break;
            }

            m_npc_list[i]->m_follow_owner_index = 0;
            m_npc_list[i]->m_target_index = 0;
            m_npc_list[i]->m_turn = (rand() % 2);

            setup_entity_appearance(m_npc_list[i]);

            // Set entity properties
            m_npc_list[i]->m_map_index = (char)map_index;
            m_npc_list[i]->m_time = GameClock::GetTimeMS() + (rand() % 10000);
			m_npc_list[i]->m_action_time += (rand() % (300 * GameTickMultiplier));
            m_npc_list[i]->m_mp_up_time = GameClock::GetTimeMS();
            m_npc_list[i]->m_hp_up_time = m_npc_list[i]->m_mp_up_time;
            m_npc_list[i]->m_behavior_turn_count = 0;
            m_npc_list[i]->m_is_summoned = is_summoned;
            m_npc_list[i]->m_bypass_mob_limit = bypass_mob_limit;
            m_npc_list[i]->m_is_master = is_master;
            if (is_summoned)
                m_npc_list[i]->m_summoned_time = GameClock::GetTimeMS();

            if (firm_berserk) {
                m_npc_list[i]->m_magic_effect_status[hb::shared::magic::Berserk] = 1;
                m_npc_list[i]->m_status.berserk = true;
            }

            if (change_side != -1) m_npc_list[i]->m_side = change_side;

            m_npc_list[i]->m_bravery = (rand() % 3) + m_npc_list[i]->m_min_bravery;
            m_npc_list[i]->m_spot_mob_index = spot_mob_index;

            // Generate and assign GUID
            m_entity_guid[i] = generate_entity_guid();

            // Register with map
            m_map_list[map_index]->set_owner(i, hb::shared::owner_class::Npc, sX, sY);
            if (!bypass_mob_limit) {
                m_map_list[map_index]->m_total_active_object++;
            }
            m_map_list[map_index]->m_total_alive_object++;
            m_total_entities++;

            // Special handling for crusade structures and crops
            if (is_crusade_structure(m_npc_list[i]->m_type)) {
                m_map_list[map_index]->add_crusade_structure_info(static_cast<char>(m_npc_list[i]->m_type), sX, sY, m_npc_list[i]->m_side);
            }
            else if (m_npc_list[i]->m_type == 64) {
                m_map_list[map_index]->add_crops_total_sum();
            }

            // Add to active entity list for efficient iteration (Performance!)
            add_to_active_list(i);

            m_game->send_event_to_near_client_type_a(i, hb::shared::owner_class::Npc, MsgId::EventLog, MsgType::Confirm, 0, 0, 0);
            return i; // Return entity handle (SUCCESS)
        }

    // No free slots - log diagnostic info
    int used_slots = 0;
    for(int idx = 1; idx < MaxNpcs; idx++) {
        if (m_npc_list[idx] != nullptr) used_slots++;
    }
    hb::logger::log("No free entity slots: used={}/{} active={} total={}", used_slots, MaxNpcs - 1, m_active_entity_count, m_total_entities);
    return -1; // No free slots
}

bool CEntityManager::find_spawn_position(int map_index, char move_type,
    int* offset_x, int* offset_y, char* waypoint_list,
    hb::shared::geometry::GameRectangle* area,
    bool hide_gen_mode, short& out_x, short& out_y)
{
    bool flag;

    switch (move_type) {
    case MoveType::Guard:
    case MoveType::Random:
        if ((offset_x != nullptr) && (offset_y != nullptr) && (*offset_x != 0) && (*offset_y != 0)) {
            out_x = *offset_x;
            out_y = *offset_y;
        }
        else {
            flag = false;
            for (int j = 0; j <= 30; j++) {
                out_x = (rand() % (m_map_list[map_index]->m_size_x - 50)) + 15;
                out_y = (rand() % (m_map_list[map_index]->m_size_y - 50)) + 15;

                flag = true;
                for (int k = 0; k < smap::MaxMgar; k++)
                    if (m_map_list[map_index]->m_mob_generator_avoid_rect[k].x != -1) {
                        if ((out_x >= m_map_list[map_index]->m_mob_generator_avoid_rect[k].Left()) &&
                            (out_x <= m_map_list[map_index]->m_mob_generator_avoid_rect[k].Right()) &&
                            (out_y >= m_map_list[map_index]->m_mob_generator_avoid_rect[k].Top()) &&
                            (out_y <= m_map_list[map_index]->m_mob_generator_avoid_rect[k].Bottom())) {
                            flag = false;
                        }
                    }
                if (flag) break;
            }
            if (!flag) return false;
        }
        break;

    case MoveType::RandomArea:
        out_x = (short)((rand() % area->width) + area->x);
        out_y = (short)((rand() % area->height) + area->y);
        break;

    case MoveType::RandomWaypoint:
        out_x = (short)m_map_list[map_index]->m_waypoint_list[waypoint_list[m_game->dice(1, 10) - 1]].x;
        out_y = (short)m_map_list[map_index]->m_waypoint_list[waypoint_list[m_game->dice(1, 10) - 1]].y;
        break;

    default:
        if ((offset_x != nullptr) && (offset_y != nullptr) && (*offset_x != 0) && (*offset_y != 0)) {
            out_x = *offset_x;
            out_y = *offset_y;
        }
        else {
            out_x = (short)m_map_list[map_index]->m_waypoint_list[waypoint_list[0]].x;
            out_y = (short)m_map_list[map_index]->m_waypoint_list[waypoint_list[0]].y;
        }
        break;
    }

    if (m_game->get_empty_position(&out_x, &out_y, map_index) == false)
        return false;

    if ((hide_gen_mode) && (m_game->get_player_number_on_spot(out_x, out_y, map_index, 7) != 0))
        return false;

    if ((offset_x != nullptr) && (offset_y != nullptr)) {
        *offset_x = out_x;
        *offset_y = out_y;
    }

    return true;
}

void CEntityManager::setup_entity_appearance(CNpc* npc)
{
    switch (npc->m_type) {
    case 1: case 2: case 3: case 4: case 5: case 6:
        npc->m_appearance.sub_type = static_cast<uint8_t>(rand() % 13);
        npc->m_appearance.special_frame = static_cast<uint8_t>(rand() % 9);
        break;
    case 36: case 37: case 38: case 39:
        npc->m_appearance.special_frame = 3;
        break;
    case 64:
        npc->m_appearance.special_frame = 1;
        break;
    default:
        npc->m_appearance.clear();
        break;
    }
}

void CEntityManager::award_kill_experience(int entity_handle, short attacker_h, char attacker_type)
{
    CNpc* entity = m_npc_list[entity_handle];
    short type = entity->m_type;

    if ((entity->m_is_summoned != true) && (attacker_type == hb::shared::owner_class::Player) &&
        (m_game->m_client_list[attacker_h] != nullptr)) {
        double tmp1, tmp2, tmp3;
        uint32_t exp = (entity->m_exp / 3);

        if (entity->m_no_die_remain_exp > 0) {
            exp += entity->m_no_die_remain_exp;
        }

        if (m_game->m_client_list[attacker_h]->m_add_exp != 0) {
            tmp1 = (double)m_game->m_client_list[attacker_h]->m_add_exp;
            tmp2 = (double)exp;
            tmp3 = (tmp1 / 100.0f) * tmp2;
            exp += (uint32_t)tmp3;
        }

        if (type == 81) {
            for(int i = 1; i < MaxClients; i++) {
                if (m_game->m_client_list[i] != nullptr) {
                    m_game->send_notify_msg(attacker_h, i, Notify::AbaddonKilled,
                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
                }
            }
        }

        if (m_game->m_is_crusade_mode) {
            if (exp > 10) exp = exp / 3;
        }

        m_game->get_exp(attacker_h, exp, true);

        int quest_index = m_game->m_client_list[attacker_h]->m_quest;
        if (quest_index != 0 && m_game->m_quest_manager->m_quest_config_list[quest_index] != nullptr) {
            switch (m_game->m_quest_manager->m_quest_config_list[quest_index]->m_type) {
            case hb::server::quest::Type::MonsterHunt:
                if (m_game->m_client_list[attacker_h]->m_quest_match_flag_loc &&
                    m_game->m_quest_manager->m_quest_config_list[quest_index]->m_target_config_id == entity->m_npc_config_id) {
                    m_game->m_client_list[attacker_h]->m_cur_quest_count++;
                    char quest_remain = (m_game->m_quest_manager->m_quest_config_list[quest_index]->m_max_count -
                        m_game->m_client_list[attacker_h]->m_cur_quest_count);
                    m_game->send_notify_msg(0, attacker_h, Notify::QuestCounter, quest_remain, 0, 0, 0);
                    m_game->m_quest_manager->check_is_quest_completed(attacker_h);
                }
                break;
            }
        }
    }

    // Rating adjustments (player only)
    if (attacker_type == hb::shared::owner_class::Player) {
        switch (type) {
        case 32:
            m_game->m_client_list[attacker_h]->m_rating -= 5;
            if (m_game->m_client_list[attacker_h]->m_rating < -10000)
                m_game->m_client_list[attacker_h]->m_rating = 0;
            if (m_game->m_client_list[attacker_h]->m_rating > 10000)
                m_game->m_client_list[attacker_h]->m_rating = 0;
            break;
        case 33:
            break;
        }
    }
}

void CEntityManager::apply_crusade_contribution(int entity_handle, short attacker_h, char attacker_type)
{
    CNpc* entity = m_npc_list[entity_handle];
    short type = entity->m_type;
    int map_index = entity->m_map_index;

    struct crusade_contribution { int type; int construction; int war; };
    static constexpr crusade_contribution contribution_table[] = {
        {1, 50, 100}, {2, 50, 100}, {3, 50, 100}, {4, 50, 100}, {5, 50, 100}, {6, 50, 100},
        {36, 700, 4000}, {37, 700, 4000},
        {38, 500, 2000}, {39, 500, 2000},
        {40, 1500, 5000},
        {41, 5000, 10000},
        {43, 500, 1000},
        {44, 1000, 2000}, {45, 1500, 3000},
        {46, 1000, 2000}, {47, 1500, 3000},
    };

    int construction_point = 0;
    int war_contribution = 0;

    if (type == 64) {
        m_map_list[map_index]->remove_crops_total_sum();
    }

    for (const auto& entry : contribution_table) {
        if (entry.type == type) {
            construction_point = entry.construction;
            war_contribution = entry.war;
            break;
        }
    }

    if (construction_point != 0) {
        switch (attacker_type) {
        case hb::shared::owner_class::Player:
            if (m_game->m_client_list[attacker_h]->m_side != entity->m_side) {
                m_game->m_client_list[attacker_h]->m_construction_point += construction_point;
                if (m_game->m_client_list[attacker_h]->m_construction_point > MaxConstructionPoint)
                    m_game->m_client_list[attacker_h]->m_construction_point = MaxConstructionPoint;

                m_game->m_client_list[attacker_h]->m_war_contribution += war_contribution;
                if (m_game->m_client_list[attacker_h]->m_war_contribution > MaxWarContribution)
                    m_game->m_client_list[attacker_h]->m_war_contribution = MaxWarContribution;

                hb::logger::log("Enemy NPC killed by player, construction +{}, war contribution +{}", construction_point, war_contribution);

                m_game->send_notify_msg(0, attacker_h, Notify::ConstructionPoint,
                    m_game->m_client_list[attacker_h]->m_construction_point,
                    m_game->m_client_list[attacker_h]->m_war_contribution, 0, 0);
            }
            else {
                m_game->m_client_list[attacker_h]->m_war_contribution -= (war_contribution * 2);
                if (m_game->m_client_list[attacker_h]->m_war_contribution < 0)
                    m_game->m_client_list[attacker_h]->m_war_contribution = 0;

                hb::logger::log("Friendly NPC killed by player, war contribution -{}", war_contribution);

                m_game->send_notify_msg(0, attacker_h, Notify::ConstructionPoint,
                    m_game->m_client_list[attacker_h]->m_construction_point,
                    m_game->m_client_list[attacker_h]->m_war_contribution, 0, 0);
            }
            break;

        case hb::shared::owner_class::Npc:
            if (m_game->m_npc_list[attacker_h]->m_side != 0) {
                if (m_game->m_npc_list[attacker_h]->m_side != entity->m_side) {
                    for(int i = 1; i < MaxClients; i++) {
                        if ((m_game->m_client_list[i] != nullptr) &&
                            (m_game->m_client_list[i]->m_side == m_game->m_npc_list[attacker_h]->m_side) &&
                            (m_game->m_client_list[i]->m_crusade_duty == 3)) {
                            m_game->m_client_list[i]->m_construction_point += construction_point;
                            if (m_game->m_client_list[i]->m_construction_point > MaxConstructionPoint)
                                m_game->m_client_list[i]->m_construction_point = MaxConstructionPoint;

                            hb::logger::log("Enemy NPC killed by NPC, construction +{}", construction_point);
                            m_game->send_notify_msg(0, i, Notify::ConstructionPoint,
                                m_game->m_client_list[i]->m_construction_point,
                                m_game->m_client_list[i]->m_war_contribution, 0, 0);
                            break;
                        }
                    }
                }
            }
            break;
        }
    }
}

bool CEntityManager::try_magic_attack(int npc_h, int target_h, char target_type, short sX, short sY, short dX, short dY, bool& out_skip_ranged)
{
	out_skip_ranged = false;

	// Positive magic level: spell selection by tier
	if ((m_npc_list[npc_h]->m_magic_level > 0) && (m_game->dice(1, 2) == 1) &&
		(abs(sX - dX) <= 9) && (abs(sY - dY) <= 7)) {
		int magic_type = -1;
		switch (m_npc_list[npc_h]->m_magic_level) {
		case 1:
			if (m_game->m_magic_config_list[0]->m_value_1 <= m_npc_list[npc_h]->m_mana)
				magic_type = 0;
			break;

		case 2:
			if (m_game->m_magic_config_list[10]->m_value_1 <= m_npc_list[npc_h]->m_mana)
				magic_type = 10;
			else if (m_game->m_magic_config_list[0]->m_value_1 <= m_npc_list[npc_h]->m_mana)
				magic_type = 0;
			break;

		case 3: // Orc-Mage
			if (m_game->m_magic_config_list[20]->m_value_1 <= m_npc_list[npc_h]->m_mana)
				magic_type = 20;
			else if (m_game->m_magic_config_list[10]->m_value_1 <= m_npc_list[npc_h]->m_mana)
				magic_type = 10;
			break;

		case 4:
			if (m_game->m_magic_config_list[30]->m_value_1 <= m_npc_list[npc_h]->m_mana)
				magic_type = 30;
			else if (m_game->m_magic_config_list[37]->m_value_1 <= m_npc_list[npc_h]->m_mana)
				magic_type = 37;
			else if (m_game->m_magic_config_list[20]->m_value_1 <= m_npc_list[npc_h]->m_mana)
				magic_type = 20;
			else if (m_game->m_magic_config_list[10]->m_value_1 <= m_npc_list[npc_h]->m_mana)
				magic_type = 10;
			break;

		case 5: // Rudolph, Cannibal-Plant, Cyclops
			if (m_game->m_magic_config_list[43]->m_value_1 <= m_npc_list[npc_h]->m_mana)
				magic_type = 43;
			else if (m_game->m_magic_config_list[30]->m_value_1 <= m_npc_list[npc_h]->m_mana)
				magic_type = 30;
			else if (m_game->m_magic_config_list[37]->m_value_1 <= m_npc_list[npc_h]->m_mana)
				magic_type = 37;
			else if (m_game->m_magic_config_list[20]->m_value_1 <= m_npc_list[npc_h]->m_mana)
				magic_type = 20;
			else if (m_game->m_magic_config_list[10]->m_value_1 <= m_npc_list[npc_h]->m_mana)
				magic_type = 10;
			break;

		case 6: // Tentocle, Liche
			if (m_game->m_magic_config_list[51]->m_value_1 <= m_npc_list[npc_h]->m_mana)
				magic_type = 51;
			else if (m_game->m_magic_config_list[43]->m_value_1 <= m_npc_list[npc_h]->m_mana)
				magic_type = 43;
			else if (m_game->m_magic_config_list[30]->m_value_1 <= m_npc_list[npc_h]->m_mana)
				magic_type = 30;
			else if (m_game->m_magic_config_list[37]->m_value_1 <= m_npc_list[npc_h]->m_mana)
				magic_type = 37;
			else if (m_game->m_magic_config_list[20]->m_value_1 <= m_npc_list[npc_h]->m_mana)
				magic_type = 20;
			else if (m_game->m_magic_config_list[10]->m_value_1 <= m_npc_list[npc_h]->m_mana)
				magic_type = 10;
			break;

		case 7: // Barlog, Fire-Wyvern, MasterMage-Orc, LightWarBeatle, GHK, GHKABS, TK, BG
			// Sor, Gagoyle, Demon
			if ((m_game->m_magic_config_list[70]->m_value_1 <= m_npc_list[npc_h]->m_mana) && (m_game->dice(1, 5) == 3))
				magic_type = 70;
			else if (m_game->m_magic_config_list[61]->m_value_1 <= m_npc_list[npc_h]->m_mana)
				magic_type = 61;
			else if (m_game->m_magic_config_list[60]->m_value_1 <= m_npc_list[npc_h]->m_mana)
				magic_type = 60;
			else if (m_game->m_magic_config_list[51]->m_value_1 <= m_npc_list[npc_h]->m_mana)
				magic_type = 51;
			else if (m_game->m_magic_config_list[43]->m_value_1 <= m_npc_list[npc_h]->m_mana)
				magic_type = 43;
			break;

		case 8: // Unicorn, Centaurus
			if ((m_game->m_magic_config_list[35]->m_value_1 <= m_npc_list[npc_h]->m_mana) && (m_game->dice(1, 3) == 2))
				magic_type = 35;
			else if (m_game->m_magic_config_list[60]->m_value_1 <= m_npc_list[npc_h]->m_mana)
				magic_type = 60;
			else if (m_game->m_magic_config_list[51]->m_value_1 <= m_npc_list[npc_h]->m_mana)
				magic_type = 51;
			else if (m_game->m_magic_config_list[43]->m_value_1 <= m_npc_list[npc_h]->m_mana)
				magic_type = 43;
			break;

		case 9: // Tigerworm
			if ((m_game->m_magic_config_list[74]->m_value_1 <= m_npc_list[npc_h]->m_mana) && (m_game->dice(1, 3) == 2))
				magic_type = 74; // Lightning-Strike
			break;

		case 10: // Frost, Nizie
			break;

		case 11: // Ice-Golem
			break;

		case 12: // Wyvern
			if ((m_game->m_magic_config_list[91]->m_value_1 <= m_npc_list[npc_h]->m_mana) && (m_game->dice(1, 3) == 2))
				magic_type = 91; // Blizzard
			else if (m_game->m_magic_config_list[63]->m_value_1 <= m_npc_list[npc_h]->m_mana)
				magic_type = 63; // Mass-Chill-Wind
			break;

		case 13: // Abaddon
			if ((m_game->m_magic_config_list[96]->m_value_1 <= m_npc_list[npc_h]->m_mana) && (m_game->dice(1, 3) == 2))
				magic_type = 96; // Earth Shock Wave
			else if (m_game->m_magic_config_list[81]->m_value_1 <= m_npc_list[npc_h]->m_mana)
				magic_type = 81; // Metoer Strike
			break;
		}

		if (magic_type != -1) {
			// AI level >= 2: check if target has magic protection
			if (m_npc_list[npc_h]->m_ai_level >= 2) {
				switch (target_type) {
				case hb::shared::owner_class::Player:
					if (m_game->m_client_list[target_h]->m_magic_effect_status[hb::shared::magic::Protect] == 2) {
						if ((abs(sX - dX) > m_npc_list[npc_h]->m_attack_range) || (abs(sY - dY) > m_npc_list[npc_h]->m_attack_range)) {
							m_npc_list[npc_h]->m_behavior_turn_count = 0;
							m_npc_list[npc_h]->m_behavior = Behavior::Move;
							return true;
						}
						else { out_skip_ranged = true; break; }
					}
					if ((magic_type == 35) && (m_game->m_client_list[target_h]->m_magic_effect_status[hb::shared::magic::HoldObject] != 0)) { out_skip_ranged = true; break; }
					break;

				case hb::shared::owner_class::Npc:
					if (m_npc_list[target_h]->m_magic_effect_status[hb::shared::magic::Protect] == 2) {
						if ((abs(sX - dX) > m_npc_list[npc_h]->m_attack_range) || (abs(sY - dY) > m_npc_list[npc_h]->m_attack_range)) {
							m_npc_list[npc_h]->m_behavior_turn_count = 0;
							m_npc_list[npc_h]->m_behavior = Behavior::Move;
							return true;
						}
						else { out_skip_ranged = true; break; }
					}
					if ((magic_type == 35) && (m_npc_list[target_h]->m_magic_effect_status[hb::shared::magic::HoldObject] != 0)) { out_skip_ranged = true; break; }
					break;
				}
			}

			if (!out_skip_ranged) {
				direction dir = m_npc_list[npc_h]->m_dir;
				m_game->send_event_to_near_client_type_a(npc_h, hb::shared::owner_class::Npc, MsgId::EventMotion, Type::Attack, m_npc_list[npc_h]->m_x + _tmp_cTmpDirX[dir], m_npc_list[npc_h]->m_y + _tmp_cTmpDirY[dir], 1);
				npc_magic_handler(npc_h, dX, dY, magic_type);
				m_npc_list[npc_h]->m_time = GameClock::GetTimeMS() + 2000;
				return true;
			}
		}
	}

	// Negative magic level: limited spell set (43, 37, 0)
	if (!out_skip_ranged && (m_npc_list[npc_h]->m_magic_level < 0) && (m_game->dice(1, 2) == 1) &&
		(abs(sX - dX) <= 9) && (abs(sY - dY) <= 7)) {
		int magic_type = -1;
		if (m_game->m_magic_config_list[43]->m_value_1 <= m_npc_list[npc_h]->m_mana)
			magic_type = 43;
		else if (m_game->m_magic_config_list[37]->m_value_1 <= m_npc_list[npc_h]->m_mana)
			magic_type = 37;
		else if (m_game->m_magic_config_list[0]->m_value_1 <= m_npc_list[npc_h]->m_mana)
			magic_type = 0;

		if (magic_type != -1) {
			direction dir = m_npc_list[npc_h]->m_dir;
			m_game->send_event_to_near_client_type_a(npc_h, hb::shared::owner_class::Npc, MsgId::EventMotion, Type::Attack, m_npc_list[npc_h]->m_x + _tmp_cTmpDirX[dir], m_npc_list[npc_h]->m_y + _tmp_cTmpDirY[dir], 1);
			npc_magic_handler(npc_h, dX, dY, magic_type);
			m_npc_list[npc_h]->m_time = GameClock::GetTimeMS() + 2000;
			return true;
		}
	}

	return false;
}

bool CEntityManager::try_ranged_attack(int npc_h, int target_h, char target_type, short sX, short sY, short dX, short dY)
{
	if (m_npc_list[npc_h]->m_attack_range <= 1)
		return false;
	if ((abs(sX - dX) > m_npc_list[npc_h]->m_attack_range) || (abs(sY - dY) > m_npc_list[npc_h]->m_attack_range))
		return false;

	direction dir = CMisc::get_next_move_dir(sX, sY, dX, dY);
	if (dir == 0) return false;
	m_npc_list[npc_h]->m_dir = dir;

	uint32_t time = GameClock::GetTimeMS();

	if (m_npc_list[npc_h]->m_action_limit == 5) {
		switch (m_npc_list[npc_h]->m_type) {
		case 36: // Crossbow Guard Tower
			m_game->send_event_to_near_client_type_a(npc_h, hb::shared::owner_class::Npc, MsgId::EventMotion, Type::Attack, dX, dY, 2);
			m_game->m_combat_manager->calculate_attack_effect(target_h, target_type, npc_h, hb::shared::owner_class::Npc, dX, dY, 2);
			break;

		case 37: // Cannon Guard Tower
			m_game->send_event_to_near_client_type_a(npc_h, hb::shared::owner_class::Npc, MsgId::EventMotion, Type::Attack, dX, dY, 1);
			m_npc_list[npc_h]->m_magic_hit_ratio = 1000;
			npc_magic_handler(npc_h, dX, dY, 61);
			break;
		}
	}
	else {
		switch (m_npc_list[npc_h]->m_type) {
		case 51: // v2.05 Catapult
			m_game->send_event_to_near_client_type_a(npc_h, hb::shared::owner_class::Npc, MsgId::EventMotion, Type::Attack, dX, dY, 1);
			m_npc_list[npc_h]->m_magic_hit_ratio = 1000;
			npc_magic_handler(npc_h, dX, dY, 61);
			break;

		case 54: // Dark Elf
			m_game->send_event_to_near_client_type_a(npc_h, hb::shared::owner_class::Npc, MsgId::EventMotion, Type::Attack, dX, dY, 2);
			m_game->m_combat_manager->calculate_attack_effect(target_h, target_type, npc_h, hb::shared::owner_class::Npc, dX, dY, 2);
			break;

		case 63: // Frost
		case 79: // Nizie
			switch (target_type) {
			case hb::shared::owner_class::Player:
				if (m_game->m_client_list[target_h] != 0) {
					if ((m_game->m_magic_config_list[57]->m_value_1 <= m_npc_list[npc_h]->m_mana) && (m_game->dice(1, 3) == 2))
						npc_magic_handler(npc_h, dX, dY, 57);
					if ((m_game->m_client_list[target_h]->m_hp > 0) &&
						(m_game->m_combat_manager->check_resisting_ice_success(m_npc_list[npc_h]->m_dir, target_h, hb::shared::owner_class::Player, m_npc_list[npc_h]->m_magic_hit_ratio) == false)) {
						if (m_game->m_client_list[target_h]->m_magic_effect_status[hb::shared::magic::Ice] == 0) {
							m_game->m_client_list[target_h]->m_magic_effect_status[hb::shared::magic::Ice] = 1;
							m_game->m_status_effect_manager->set_ice_flag(target_h, hb::shared::owner_class::Player, true);
							m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Ice, time + (5 * 1000),
								target_h, hb::shared::owner_class::Player, 0, 0, 0, 1, 0, 0);
							m_game->send_notify_msg(0, target_h, Notify::MagicEffectOn, hb::shared::magic::Ice, 1, 0, 0);
						}
					}
				}
				break;

			case hb::shared::owner_class::Npc:
				if (m_npc_list[target_h] != 0) {
					if ((m_game->m_magic_config_list[57]->m_value_1 <= m_npc_list[npc_h]->m_mana) && (m_game->dice(1, 3) == 2))
						npc_magic_handler(npc_h, dX, dY, 57);
					if ((m_npc_list[target_h]->m_hp > 0) &&
						(m_game->m_combat_manager->check_resisting_ice_success(m_npc_list[npc_h]->m_dir, target_h, hb::shared::owner_class::Npc, m_npc_list[npc_h]->m_magic_hit_ratio) == false)) {
						if (m_npc_list[target_h]->m_magic_effect_status[hb::shared::magic::Ice] == 0) {
							m_npc_list[target_h]->m_magic_effect_status[hb::shared::magic::Ice] = 1;
							m_game->m_status_effect_manager->set_ice_flag(target_h, hb::shared::owner_class::Npc, true);
							m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Ice, time + (5 * 1000),
								target_h, hb::shared::owner_class::Npc, 0, 0, 0, 1, 0, 0);
						}
					}
				}
				break;
			}
			// Fall through to Beholder case (Frost/Nizie also deal ranged physical damage)

		case 53: // Beholder
			switch (target_type) {
			case hb::shared::owner_class::Player:
				if (m_game->m_client_list[target_h] != 0) {
					if ((m_game->m_client_list[target_h]->m_hp > 0) &&
						(m_game->m_combat_manager->check_resisting_ice_success(m_npc_list[npc_h]->m_dir, target_h, hb::shared::owner_class::Player, m_npc_list[npc_h]->m_magic_hit_ratio) == false)) {
						if (m_game->m_client_list[target_h]->m_magic_effect_status[hb::shared::magic::Ice] == 0) {
							m_game->m_client_list[target_h]->m_magic_effect_status[hb::shared::magic::Ice] = 1;
							m_game->m_status_effect_manager->set_ice_flag(target_h, hb::shared::owner_class::Player, true);
							m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Ice, time + (5 * 1000),
								target_h, hb::shared::owner_class::Player, 0, 0, 0, 1, 0, 0);
							m_game->send_notify_msg(0, target_h, Notify::MagicEffectOn, hb::shared::magic::Ice, 1, 0, 0);
						}
					}
				}
				break;

			case hb::shared::owner_class::Npc:
				if (m_npc_list[target_h] != 0) {
					if ((m_npc_list[target_h]->m_hp > 0) &&
						(m_game->m_combat_manager->check_resisting_ice_success(m_npc_list[npc_h]->m_dir, target_h, hb::shared::owner_class::Npc, m_npc_list[npc_h]->m_magic_hit_ratio) == false)) {
						if (m_npc_list[target_h]->m_magic_effect_status[hb::shared::magic::Ice] == 0) {
							m_npc_list[target_h]->m_magic_effect_status[hb::shared::magic::Ice] = 1;
							m_game->m_status_effect_manager->set_ice_flag(target_h, hb::shared::owner_class::Npc, true);
							m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Ice, time + (5 * 1000),
								target_h, hb::shared::owner_class::Npc, 0, 0, 0, 1, 0, 0);
						}
					}
				}
				break;
			}
			m_game->send_event_to_near_client_type_a(npc_h, hb::shared::owner_class::Npc, MsgId::EventMotion, Type::Attack, dX, dY, 20);
			m_game->m_combat_manager->calculate_attack_effect(target_h, target_type, npc_h, hb::shared::owner_class::Npc, dX, dY, 20);
			break;

		default:
			m_game->send_event_to_near_client_type_a(npc_h, hb::shared::owner_class::Npc, MsgId::EventMotion, Type::Attack, dX, dY, 20);
			m_game->m_combat_manager->calculate_attack_effect(target_h, target_type, npc_h, hb::shared::owner_class::Npc, dX, dY, 20);
			break;
		}
	}
	m_npc_list[npc_h]->m_attack_count++;

	if ((m_npc_list[npc_h]->m_is_perm_attack_mode == false) && (m_npc_list[npc_h]->m_action_limit == 0)) {
		switch (m_npc_list[npc_h]->m_attack_strategy) {
		case AttackAI::ExchangeAttack:
			m_npc_list[npc_h]->m_behavior_turn_count = 0;
			m_npc_list[npc_h]->m_behavior = Behavior::Flee;
			break;

		case AttackAI::TwoByOneAttack:
			if (m_npc_list[npc_h]->m_attack_count >= 2) {
				m_npc_list[npc_h]->m_behavior_turn_count = 0;
				m_npc_list[npc_h]->m_behavior = Behavior::Flee;
			}
			break;
		}
	}
	return true;
}

void CEntityManager::spawn_follower_mobs(int map_index, int npc_config_id, int total_mob,
	bool firm_berserk, int prob_sa, int kind_sa,
	const char* master_name, int pX, int pY)
{
	char cName_Slave[11];
	char waypoint[11] = {};

	for (int j = 0; j < total_mob; j++) {
		int naming_value = m_map_list[map_index]->get_empty_naming_value();
		if (naming_value != -1) {
			std::memset(cName_Slave, 0, sizeof(cName_Slave));
			sprintf(cName_Slave, "XX%d", naming_value);
			cName_Slave[0] = 95; // '_'
			cName_Slave[1] = map_index + 65;

			char sa = 0;
			if (m_game->dice(1, 100) <= static_cast<uint32_t>(prob_sa)) {
				sa = m_game->get_special_ability(kind_sa);
			}

			if (create_entity(npc_config_id, cName_Slave, m_map_list[map_index]->m_name, (rand() % 3), sa,
				MoveType::Random, &pX, &pY, waypoint, 0, 0, -1, false, false, firm_berserk, false, 0) <= 0) {
				m_map_list[map_index]->set_naming_value_empty(naming_value);
			}
			else {
				set_npc_follow_mode(cName_Slave, const_cast<char*>(master_name), hb::shared::owner_class::Npc);
			}
		}
	}
}

void CEntityManager::delete_entity(int entity_handle)
{
    if (!is_valid_entity(entity_handle))
        return;

    if (m_game == nullptr)
        return;

    delete_npc_internal(entity_handle);

    remove_from_active_list(entity_handle);
    m_entity_guid[entity_handle] = 0;
    if (m_total_entities > 0)
        m_total_entities--;
}

void CEntityManager::on_entity_killed(int entity_handle, short attacker_h, char attacker_type, short damage)
{
    // Extracted from Game.cpp:10810-11074 (NpcKilledHandler)

    if (!is_valid_entity(entity_handle)) {
        return;
    }

    CNpc* entity = m_npc_list[entity_handle];

    // Check if already killed
    if (entity->m_is_killed) {
        return;
    }

    if (m_game == nullptr || m_map_list == nullptr) {
        return;
    }

    // ========================================================================
    // 1. Mark entity as killed and set death state
    // ========================================================================
    entity->m_is_killed = true;
    entity->m_hp = 0;
    entity->m_last_damage = damage;

    short type = entity->m_type;
    int map_index = entity->m_map_index;

    // Decrement alive object counter
    m_map_list[map_index]->m_total_alive_object--;

    // ========================================================================
    // 2. Remove from target lists and release followers
    // ========================================================================
    m_game->m_combat_manager->remove_from_target(entity_handle, hb::shared::owner_class::Npc);
    m_game->release_follow_mode(entity_handle, hb::shared::owner_class::Npc);

    entity->m_target_index = 0;
    entity->m_target_type = 0;

    // ========================================================================
    // 3. Send death animation event
    // ========================================================================
    short attacker_weapon;
    if (attacker_type == hb::shared::owner_class::Player) {
        if (m_game->m_client_list[attacker_h] != nullptr)
            attacker_weapon = hb::shared::item::to_int(m_game->m_client_list[attacker_h]->get_equipped_weapon_class());
        else
            attacker_weapon = 1;
    }
    else {
        attacker_weapon = 1;
    }

    m_game->send_event_to_near_client_type_a(entity_handle, hb::shared::owner_class::Npc, MsgId::EventMotion,
        Type::Dying, damage, attacker_weapon, 0);

    // ========================================================================
    // 4. Update map tiles
    // ========================================================================
    m_map_list[map_index]->clear_owner(10, entity_handle, hb::shared::owner_class::Npc, entity->m_x, entity->m_y);
    m_map_list[map_index]->set_dead_owner(entity_handle, hb::shared::owner_class::Npc, entity->m_x, entity->m_y);

    // ========================================================================
    // 5. Set death behavior and timer
    // ========================================================================
    entity->m_behavior = Behavior::Dead;
    entity->m_behavior_turn_count = 0;
    entity->m_dead_time = GameClock::GetTimeMS();

    // ========================================================================
    // 6. Check for no-penalty/no-reward maps
    // ========================================================================
    if (m_map_list[map_index]->m_type == smap::MapType::NoPenaltyNoReward) {
        return;
    }

    // ========================================================================
    // 7. Generate item drops (delegate to CGame for now)
    // ========================================================================
    npc_dead_item_generator(entity_handle, attacker_h, attacker_type);

    // 8-9. Award experience, quest progress, and rating adjustments
    award_kill_experience(entity_handle, attacker_h, attacker_type);

    // 10. Crusade construction points / war contribution
    apply_crusade_contribution(entity_handle, attacker_h, attacker_type);

    // ========================================================================
    // 11. Handle special ability death triggers (explosive NPCs)
    // ========================================================================
    if (entity->m_special_ability == 7) {
        // Explosive ability - triggers magic on death
        entity->m_mana = 100;
        entity->m_magic_hit_ratio = 100;
        npc_magic_handler(entity_handle, entity->m_x, entity->m_y, 30);
    }
    else if (entity->m_special_ability == 8) {
        // Powerful explosive ability
        entity->m_mana = 100;
        entity->m_magic_hit_ratio = 100;
        npc_magic_handler(entity_handle, entity->m_x, entity->m_y, 61);
    }

    // ========================================================================
    // 12. Heldenian mode tower tracking
    // ========================================================================
    if (m_game->m_is_heldenian_mode &&
        m_map_list[map_index]->m_is_heldenian_map &&
        m_game->m_heldenian_mode_type == 1) {
        int helden_map_index = entity->m_map_index;
        if (type == 87 || type == 89) {
            if (entity->m_side == 1) {
                m_game->m_heldenian_aresden_left_tower--;
                std::snprintf(G_cTxt, sizeof(G_cTxt), "Aresden Tower Broken, Left TOWER %d", m_game->m_heldenian_aresden_left_tower);
            }
            else if (entity->m_side == 2) {
                m_game->m_heldenian_elvine_left_tower--;
            }
            hb::logger::log("Elvine tower destroyed, remaining: {}", m_game->m_heldenian_elvine_left_tower);
            m_game->m_war_manager->update_heldenian_status();
        }

        if ((m_game->m_heldenian_elvine_left_tower == 0) ||
            (m_game->m_heldenian_aresden_left_tower == 0)) {
            m_game->m_war_manager->global_end_heldenian_mode();
        }
    }

}

// ========================================================================
// Update & Behavior System - STUBS
// ========================================================================

void CEntityManager::process_entities()
{
    if (m_game == nullptr)
        return;

    if (m_game->m_on_exit_process)
        return;

    uint32_t time, action_time;

    time = GameClock::GetTimeMS();

    for(int i = 1; i < MaxNpcs; i++) {
        if (m_npc_list[i] == nullptr)
            continue;

        if (i % 10 == 0) {
            extern void PollAllSockets();
            PollAllSockets();
        }

        if (m_npc_list[i]->m_behavior == Behavior::Attack) {
            switch (m_game->dice(1, 7)) {
            case 1: action_time = m_npc_list[i]->m_action_time; break;
            case 2: action_time = m_npc_list[i]->m_action_time - (100 * GameTickMultiplier); break;
            case 3: action_time = m_npc_list[i]->m_action_time - (150 * GameTickMultiplier); break;
            case 4: action_time = m_npc_list[i]->m_action_time - (250 * GameTickMultiplier); break;
            case 5: action_time = m_npc_list[i]->m_action_time - (350 * GameTickMultiplier); break;
            case 6: action_time = m_npc_list[i]->m_action_time - (450 * GameTickMultiplier); break;
            case 7: action_time = m_npc_list[i]->m_action_time - (550 * GameTickMultiplier); break;
            }
            if (action_time < (100 * GameTickMultiplier)) action_time = (100 * GameTickMultiplier);
        }
        else {
            action_time = m_npc_list[i]->m_action_time;
        }

        if (m_npc_list[i]->m_magic_effect_status[hb::shared::magic::Ice] != 0)
            action_time += (action_time / 2);

        if ((time - m_npc_list[i]->m_time) > action_time) {
            m_npc_list[i]->m_time = time;

            if (abs(m_npc_list[i]->m_magic_level) > 0) {
                if ((time - m_npc_list[i]->m_mp_up_time) > MpUpTime) {
                    m_npc_list[i]->m_mp_up_time += MpUpTime;
                    if (time - m_npc_list[i]->m_mp_up_time > MpUpTime) m_npc_list[i]->m_mp_up_time = time;
                    m_npc_list[i]->m_mana += m_game->dice(1, (m_npc_list[i]->m_max_mana / 5));

                    if (m_npc_list[i]->m_mana > m_npc_list[i]->m_max_mana)
                        m_npc_list[i]->m_mana = m_npc_list[i]->m_max_mana;
                }
            }

            if (((time - m_npc_list[i]->m_hp_up_time) > HpUpTime) && (m_npc_list[i]->m_is_killed == false)) {
                m_npc_list[i]->m_hp_up_time += HpUpTime;
                if (time - m_npc_list[i]->m_hp_up_time > HpUpTime) m_npc_list[i]->m_hp_up_time = time;

                if (m_npc_list[i]->m_hp < m_npc_list[i]->m_max_hp) {
                    if (m_npc_list[i]->m_is_summoned == false) {
                        int regen = m_game->dice(1, (m_npc_list[i]->m_hp_max - m_npc_list[i]->m_hp_min) / 2 + 5);
                        m_npc_list[i]->m_hp += regen;
                    }
                    if (m_npc_list[i]->m_hp > m_npc_list[i]->m_max_hp) m_npc_list[i]->m_hp = m_npc_list[i]->m_max_hp;
                    if (m_npc_list[i]->m_hp <= 0)     m_npc_list[i]->m_hp = 1;
                }
            }

            switch (m_npc_list[i]->m_behavior) {
            case Behavior::Dead:
                update_dead_behavior(i);
                break;
            case Behavior::stop:
                update_stop_behavior(i);
                break;
            case Behavior::Move:
                update_move_behavior(i);
                break;
            case Behavior::Attack:
                update_attack_behavior(i);
                break;
            case Behavior::Flee:
                update_flee_behavior(i);
                break;
            }

            if ((m_npc_list[i] != nullptr) && (m_npc_list[i]->m_hp != 0) && (m_npc_list[i]->m_is_summoned)) {
                switch (m_npc_list[i]->m_type) {
                case 29:
                    if ((time - m_npc_list[i]->m_summoned_time) > 1000 * 90)
                        on_entity_killed(i, 0, 0, 0);
                    break;

                default:
                    if ((time - m_npc_list[i]->m_summoned_time) > SummonTime)
                        on_entity_killed(i, 0, 0, 0);
                    break;
                }
            }
        }
    }
}

void CEntityManager::update_dead_behavior(int entity_handle)
{
    npc_behavior_dead(entity_handle);
}

void CEntityManager::update_move_behavior(int entity_handle)
{
    npc_behavior_move(entity_handle);
}

void CEntityManager::update_attack_behavior(int entity_handle)
{
    npc_behavior_attack(entity_handle);
}

void CEntityManager::update_stop_behavior(int entity_handle)
{
    npc_behavior_stop(entity_handle);
}

void CEntityManager::update_flee_behavior(int entity_handle)
{
    npc_behavior_flee(entity_handle);
}

// ========================================================================
// NPC Behavior & Helpers (ported from CGame)
// ========================================================================

void CEntityManager::npc_behavior_move(int npc_h)
{
	direction dir;
	short sX, sY, dX, dY, absX, absY;
	short target, distance;
	char  target_type;

	if (m_npc_list[npc_h] == nullptr) return;
	if (m_npc_list[npc_h]->m_is_killed) return;
	if ((m_npc_list[npc_h]->m_is_summoned) &&
		(m_npc_list[npc_h]->m_summon_control_mode == 1)) return;
	if (m_npc_list[npc_h]->m_magic_effect_status[hb::shared::magic::HoldObject] != 0) return;

	switch (m_npc_list[npc_h]->m_action_limit) {
	case 2:
	case 3:
	case 5:
		m_npc_list[npc_h]->m_behavior = Behavior::stop;
		m_npc_list[npc_h]->m_behavior_turn_count = 0;
		return;
	}

	// v1.432-2
	int st_x, st_y;
	if (m_map_list[m_npc_list[npc_h]->m_map_index] != 0) {
		st_x = m_npc_list[npc_h]->m_x / 20;
		st_y = m_npc_list[npc_h]->m_y / 20;
		m_map_list[m_npc_list[npc_h]->m_map_index]->m_temp_sector_info[st_x][st_y].monster_activity++;
	}

	m_npc_list[npc_h]->m_behavior_turn_count++;
	if (m_npc_list[npc_h]->m_behavior_turn_count > 5) {
		m_npc_list[npc_h]->m_behavior_turn_count = 0;

		absX = abs(m_npc_list[npc_h]->m_vx - m_npc_list[npc_h]->m_x);
		absY = abs(m_npc_list[npc_h]->m_vy - m_npc_list[npc_h]->m_y);

		if ((absX <= 2) && (absY <= 2)) {
			calc_next_waypoint_destination(npc_h);
		}

		m_npc_list[npc_h]->m_vx = m_npc_list[npc_h]->m_x;
		m_npc_list[npc_h]->m_vy = m_npc_list[npc_h]->m_y;
	}

	target_search(npc_h, &target, &target_type);
	if (target != 0) {
		if (m_npc_list[npc_h]->m_action_time < 1000) {
			if (m_game->dice(1, 3) == 3) {
				m_npc_list[npc_h]->m_behavior = Behavior::Attack;
				m_npc_list[npc_h]->m_behavior_turn_count = 0;
				m_npc_list[npc_h]->m_target_index = target;
				m_npc_list[npc_h]->m_target_type = target_type;
				return;
			}
		}
		else {
			m_npc_list[npc_h]->m_behavior = Behavior::Attack;
			m_npc_list[npc_h]->m_behavior_turn_count = 0;
			m_npc_list[npc_h]->m_target_index = target;
			m_npc_list[npc_h]->m_target_type = target_type;
			return;
		}
	}

	if ((m_npc_list[npc_h]->m_is_master) && (m_game->dice(1, 3) == 2)) return;

	if (m_npc_list[npc_h]->m_move_type == MoveType::Follow) {
		sX = m_npc_list[npc_h]->m_x;
		sY = m_npc_list[npc_h]->m_y;
		switch (m_npc_list[npc_h]->m_follow_owner_type) {
		case hb::shared::owner_class::Player:
			if (m_game->m_client_list[m_npc_list[npc_h]->m_follow_owner_index] == 0) {
				m_npc_list[npc_h]->m_move_type = MoveType::Random;
				return;
			}

			dX = m_game->m_client_list[m_npc_list[npc_h]->m_follow_owner_index]->m_x;
			dY = m_game->m_client_list[m_npc_list[npc_h]->m_follow_owner_index]->m_y;
			break;
		case hb::shared::owner_class::Npc:
			if (m_npc_list[m_npc_list[npc_h]->m_follow_owner_index] == 0) {
				m_npc_list[npc_h]->m_move_type = MoveType::Random;
				m_npc_list[npc_h]->m_follow_owner_index = 0;
				//bSerchMaster(npc_h);
				return;
			}

			dX = m_npc_list[m_npc_list[npc_h]->m_follow_owner_index]->m_x;
			dY = m_npc_list[m_npc_list[npc_h]->m_follow_owner_index]->m_y;
			break;
		}

		if (abs(sX - dX) >= abs(sY - dY))
			distance = abs(sX - dX);
		else distance = abs(sY - dY);

		if (distance >= 3) {
			dir = m_game->get_next_move_dir(sX, sY, dX, dY, m_npc_list[npc_h]->m_map_index, m_npc_list[npc_h]->m_turn, &m_npc_list[npc_h]->m_tmp_error);
			if (dir == 0) {
			}
			else {
				dX = m_npc_list[npc_h]->m_x + _tmp_cTmpDirX[dir];
				dY = m_npc_list[npc_h]->m_y + _tmp_cTmpDirY[dir];

				m_map_list[m_npc_list[npc_h]->m_map_index]->clear_owner(3, npc_h, hb::shared::owner_class::Npc, m_npc_list[npc_h]->m_x, m_npc_list[npc_h]->m_y);
				m_map_list[m_npc_list[npc_h]->m_map_index]->set_owner(npc_h, hb::shared::owner_class::Npc, dX, dY);
				m_npc_list[npc_h]->m_x = dX;
				m_npc_list[npc_h]->m_y = dY;
				m_npc_list[npc_h]->m_dir = dir;

				m_game->send_event_to_near_client_type_a(npc_h, hb::shared::owner_class::Npc, MsgId::EventMotion, Type::Move, 0, 0, 0);
			}
		}
	}
	else
	{
		dir = m_game->get_next_move_dir(m_npc_list[npc_h]->m_x, m_npc_list[npc_h]->m_y,
			m_npc_list[npc_h]->m_dx, m_npc_list[npc_h]->m_dy,
			m_npc_list[npc_h]->m_map_index, m_npc_list[npc_h]->m_turn, &m_npc_list[npc_h]->m_tmp_error);

		if (dir == 0) {
			if (m_game->dice(1, 10) == 3) calc_next_waypoint_destination(npc_h);
		}
		else {
			dX = m_npc_list[npc_h]->m_x + _tmp_cTmpDirX[dir];
			dY = m_npc_list[npc_h]->m_y + _tmp_cTmpDirY[dir];

			m_map_list[m_npc_list[npc_h]->m_map_index]->clear_owner(4, npc_h, hb::shared::owner_class::Npc, m_npc_list[npc_h]->m_x, m_npc_list[npc_h]->m_y);
			m_map_list[m_npc_list[npc_h]->m_map_index]->set_owner(npc_h, hb::shared::owner_class::Npc, dX, dY);
			m_npc_list[npc_h]->m_x = dX;
			m_npc_list[npc_h]->m_y = dY;
			m_npc_list[npc_h]->m_dir = dir;

			m_game->send_event_to_near_client_type_a(npc_h, hb::shared::owner_class::Npc, MsgId::EventMotion, Type::Move, 0, 0, 0);
		}
	}
}

void CEntityManager::target_search(int npc_h, short* target, char* target_type)
{
	int pk_count;
	short sX, sY, rX, rY, dX, dY;
	short owner, target_owner, distance, temp_distance;
	char  owner_type, local_target_type, target_side;
	int   inv;

	target_owner = 0;
	local_target_type = 0;
	distance = 100;

	sX = m_npc_list[npc_h]->m_x;
	sY = m_npc_list[npc_h]->m_y;

	rX = m_npc_list[npc_h]->m_x - m_npc_list[npc_h]->m_target_search_range;
	rY = m_npc_list[npc_h]->m_y - m_npc_list[npc_h]->m_target_search_range;

	for(int ix = rX; ix < rX + m_npc_list[npc_h]->m_target_search_range * 2 + 1; ix++)
		for(int iy = rY; iy < rY + m_npc_list[npc_h]->m_target_search_range * 2 + 1; iy++) {

			m_map_list[m_npc_list[npc_h]->m_map_index]->get_owner(&owner, &owner_type, ix, iy);
			if (owner != 0) {
				if ((owner == npc_h) && (owner_type == hb::shared::owner_class::Npc)) break;

				pk_count = 0;
				switch (owner_type) {
				case hb::shared::owner_class::Player:
					if (m_game->m_client_list[owner] == nullptr) {
						m_map_list[m_npc_list[npc_h]->m_map_index]->clear_owner(5, owner, hb::shared::owner_class::Player, ix, iy);
					}
					else {
						// Skip GM mode and admin invisible players
						if (m_game->m_client_list[owner]->m_is_gm_mode || m_game->m_client_list[owner]->m_is_admin_invisible)
							continue;
						dX = m_game->m_client_list[owner]->m_x;
						dY = m_game->m_client_list[owner]->m_y;
						target_side = m_game->m_client_list[owner]->m_side;
						pk_count = m_game->m_client_list[owner]->m_player_kill_count;
						inv = m_game->m_client_list[owner]->m_magic_effect_status[hb::shared::magic::Invisibility];
					}
					break;

				case hb::shared::owner_class::Npc:
					if (m_npc_list[owner] == nullptr) {
						m_map_list[m_npc_list[npc_h]->m_map_index]->clear_owner(6, owner, hb::shared::owner_class::Npc, ix, iy);
					}
					else {
						dX = m_npc_list[owner]->m_x;
						dY = m_npc_list[owner]->m_y;
						target_side = m_npc_list[owner]->m_side;
						pk_count = 0;
						inv = m_npc_list[owner]->m_magic_effect_status[hb::shared::magic::Invisibility];

						if (m_npc_list[npc_h]->m_type == 21) {
							if (m_game->calc_player_num(m_npc_list[owner]->m_map_index, dX, dY, 2) != 0) {
								owner = 0;
								owner_type = 0;
							}
						}
					}
					break;
				}

				// Summoned NPCs never attack their own summoner
				if (m_npc_list[npc_h]->m_is_summoned &&
					m_npc_list[npc_h]->m_follow_owner_type == owner_type &&
					m_npc_list[npc_h]->m_follow_owner_index == owner) {
					continue;
				}

				if (m_npc_list[npc_h]->m_side < 10) {
					// NPC
					if (target_side == 0) {
						if (pk_count == 0) continue;
					}
					else {
						if ((pk_count == 0) && (target_side == m_npc_list[npc_h]->m_side)) continue;
						if (m_npc_list[npc_h]->m_side == 0) continue;
					}
				}
				else {
					if ((owner_type == hb::shared::owner_class::Npc) && (target_side == 0)) continue;
					if (target_side == m_npc_list[npc_h]->m_side) continue;
				}

				if ((inv != 0) && (m_npc_list[npc_h]->m_special_ability != 1)) continue;

				if (abs(sX - dX) >= abs(sY - dY))
					temp_distance = abs(sX - dX);
				else temp_distance = abs(sY - dY);

				if (temp_distance < distance) {
					distance = temp_distance;
					target_owner = owner;
					local_target_type = owner_type;
				}
			}
		}

	*target = target_owner;
	*target_type = local_target_type;
	return;
}

void CEntityManager::npc_behavior_attack(int npc_h)
{
	short sX, sY, dX, dY;
	direction dir;

	if (m_npc_list[npc_h] == nullptr) return;
	if (m_npc_list[npc_h]->m_magic_effect_status[hb::shared::magic::HoldObject] != 0) return;
	if (m_npc_list[npc_h]->m_is_killed) return;

	switch (m_npc_list[npc_h]->m_action_limit) {
	case 1:
	case 2:
	case 3:
	case 4:
		return;

	case 5:
		if (m_npc_list[npc_h]->m_build_count > 0) return;
	}

	// v1.432-2
	int st_x, st_y;
	if (m_map_list[m_npc_list[npc_h]->m_map_index] != 0) {
		st_x = m_npc_list[npc_h]->m_x / 20;
		st_y = m_npc_list[npc_h]->m_y / 20;
		m_map_list[m_npc_list[npc_h]->m_map_index]->m_temp_sector_info[st_x][st_y].monster_activity++;
	}

	if (m_npc_list[npc_h]->m_behavior_turn_count == 0)
		m_npc_list[npc_h]->m_attack_count = 0;

	m_npc_list[npc_h]->m_behavior_turn_count++;
	if (m_npc_list[npc_h]->m_behavior_turn_count > 20) {
		m_npc_list[npc_h]->m_behavior_turn_count = 0;

		if ((m_npc_list[npc_h]->m_is_perm_attack_mode == false))
			m_npc_list[npc_h]->m_behavior = Behavior::Move;

		return;
	}

	sX = m_npc_list[npc_h]->m_x;
	sY = m_npc_list[npc_h]->m_y;

	switch (m_npc_list[npc_h]->m_target_type) {
	case hb::shared::owner_class::Player:
		if (m_game->m_client_list[m_npc_list[npc_h]->m_target_index] == 0) {
			m_npc_list[npc_h]->m_behavior_turn_count = 0;
			m_npc_list[npc_h]->m_behavior = Behavior::Move;
			return;
		}
		dX = m_game->m_client_list[m_npc_list[npc_h]->m_target_index]->m_x;
		dY = m_game->m_client_list[m_npc_list[npc_h]->m_target_index]->m_y;
		break;

	case hb::shared::owner_class::Npc:
		if (m_npc_list[m_npc_list[npc_h]->m_target_index] == 0) {
			m_npc_list[npc_h]->m_behavior_turn_count = 0;
			m_npc_list[npc_h]->m_behavior = Behavior::Move;
			return;
		}
		dX = m_npc_list[m_npc_list[npc_h]->m_target_index]->m_x;
		dY = m_npc_list[m_npc_list[npc_h]->m_target_index]->m_y;
		break;
	}

	if ((m_game->m_combat_manager->get_danger_value(npc_h, dX, dY) > m_npc_list[npc_h]->m_bravery) &&
		(m_npc_list[npc_h]->m_is_perm_attack_mode == false) &&
		(m_npc_list[npc_h]->m_action_limit != 5)) {

		m_npc_list[npc_h]->m_behavior_turn_count = 0;
		m_npc_list[npc_h]->m_behavior = Behavior::Flee;
		return;
	}

	if ((m_npc_list[npc_h]->m_hp <= 2) && (m_game->dice(1, m_npc_list[npc_h]->m_bravery) <= 3) &&
		(m_npc_list[npc_h]->m_is_perm_attack_mode == false) &&
		(m_npc_list[npc_h]->m_action_limit != 5)) {

		m_npc_list[npc_h]->m_behavior_turn_count = 0;
		m_npc_list[npc_h]->m_behavior = Behavior::Flee;
		return;
	}

	if ((abs(sX - dX) <= 1) && (abs(sY - dY) <= 1)) {

		dir = CMisc::get_next_move_dir(sX, sY, dX, dY);
		if (dir == 0) return;
		m_npc_list[npc_h]->m_dir = dir;

		if (m_npc_list[npc_h]->m_action_limit == 5) {
			switch (m_npc_list[npc_h]->m_type) {
			case 89:
				m_game->send_event_to_near_client_type_a(npc_h, hb::shared::owner_class::Npc, MsgId::EventMotion, Type::Attack, dX, dY, 1);
				m_npc_list[npc_h]->m_magic_hit_ratio = 1000;
				npc_magic_handler(npc_h, dX, dY, 61);
				break;

			case 87:
				m_game->send_event_to_near_client_type_a(npc_h, hb::shared::owner_class::Npc, MsgId::EventMotion, Type::Attack, dX, dY, 2);
				m_game->m_combat_manager->calculate_attack_effect(m_npc_list[npc_h]->m_target_index, m_npc_list[npc_h]->m_target_type, npc_h, hb::shared::owner_class::Npc, dX, dY, 2);
				break;

			case 36: // Crossbow Guard Tower:
				m_game->send_event_to_near_client_type_a(npc_h, hb::shared::owner_class::Npc, MsgId::EventMotion, Type::Attack, m_npc_list[npc_h]->m_x + _tmp_cTmpDirX[dir], m_npc_list[npc_h]->m_y + _tmp_cTmpDirY[dir], 2);
				m_game->m_combat_manager->calculate_attack_effect(m_npc_list[npc_h]->m_target_index, m_npc_list[npc_h]->m_target_type, npc_h, hb::shared::owner_class::Npc, dX, dY, 2, false, false, false);
				break;

			case 37: // Cannon Guard Tower: 
				m_game->send_event_to_near_client_type_a(npc_h, hb::shared::owner_class::Npc, MsgId::EventMotion, Type::Attack, dX, dY, 1);
				m_npc_list[npc_h]->m_magic_hit_ratio = 1000;
				npc_magic_handler(npc_h, dX, dY, 61);
				break;
			}
		}
		else {
			m_game->send_event_to_near_client_type_a(npc_h, hb::shared::owner_class::Npc, MsgId::EventMotion, Type::Attack, m_npc_list[npc_h]->m_x + _tmp_cTmpDirX[dir], m_npc_list[npc_h]->m_y + _tmp_cTmpDirY[dir], 1);
			m_game->m_combat_manager->calculate_attack_effect(m_npc_list[npc_h]->m_target_index, m_npc_list[npc_h]->m_target_type, npc_h, hb::shared::owner_class::Npc, dX, dY, 1, false, false);
		}
		m_npc_list[npc_h]->m_attack_count++;

		if ((m_npc_list[npc_h]->m_is_perm_attack_mode == false) && (m_npc_list[npc_h]->m_action_limit == 0)) {
			switch (m_npc_list[npc_h]->m_attack_strategy) {
			case AttackAI::ExchangeAttack:
				m_npc_list[npc_h]->m_behavior_turn_count = 0;
				m_npc_list[npc_h]->m_behavior = Behavior::Flee;
				break;

			case AttackAI::TwoByOneAttack:
				if (m_npc_list[npc_h]->m_attack_count >= 2) {
					m_npc_list[npc_h]->m_behavior_turn_count = 0;
					m_npc_list[npc_h]->m_behavior = Behavior::Flee;
				}
				break;
			}
		}
	}
	else {
		bool skip_to_chase = false;
		dir = CMisc::get_next_move_dir(sX, sY, dX, dY);
		if (dir == 0) return;
		m_npc_list[npc_h]->m_dir = dir;

		// Try magic attack (positive and negative magic levels)
		if (try_magic_attack(npc_h, m_npc_list[npc_h]->m_target_index, m_npc_list[npc_h]->m_target_type, sX, sY, dX, dY, skip_to_chase))
			return;

		// Try ranged attack (guard towers, catapult, dark elf, frost/nizie, beholder, default ranged)
		if (!skip_to_chase && try_ranged_attack(npc_h, m_npc_list[npc_h]->m_target_index, m_npc_list[npc_h]->m_target_type, sX, sY, dX, dY))
			return;

		if (m_npc_list[npc_h]->m_action_limit != 0) return;

		m_npc_list[npc_h]->m_attack_count = 0;

		{
			dir = m_game->get_next_move_dir(sX, sY, dX, dY, m_npc_list[npc_h]->m_map_index, m_npc_list[npc_h]->m_turn, &m_npc_list[npc_h]->m_tmp_error);
			if (dir == 0) {
				return;
			}
			dX = m_npc_list[npc_h]->m_x + _tmp_cTmpDirX[dir];
			dY = m_npc_list[npc_h]->m_y + _tmp_cTmpDirY[dir];
			m_map_list[m_npc_list[npc_h]->m_map_index]->clear_owner(9, npc_h, hb::shared::owner_class::Npc, m_npc_list[npc_h]->m_x, m_npc_list[npc_h]->m_y);
			m_map_list[m_npc_list[npc_h]->m_map_index]->set_owner(npc_h, hb::shared::owner_class::Npc, dX, dY);
			m_npc_list[npc_h]->m_x = dX;
			m_npc_list[npc_h]->m_y = dY;
			m_npc_list[npc_h]->m_dir = dir;
			m_game->send_event_to_near_client_type_a(npc_h, hb::shared::owner_class::Npc, MsgId::EventMotion, Type::Move, 0, 0, 0);
		}
	}
}

void CEntityManager::npc_behavior_flee(int npc_h)
{
	direction dir;
	short sX, sY, dX, dY;
	short target;
	char  target_type;

	if (m_npc_list[npc_h] == nullptr) return;
	if (m_npc_list[npc_h]->m_is_killed) return;

	m_npc_list[npc_h]->m_behavior_turn_count++;

	switch (m_npc_list[npc_h]->m_attack_strategy) {
	case AttackAI::ExchangeAttack:
	case AttackAI::TwoByOneAttack:
		if (m_npc_list[npc_h]->m_behavior_turn_count >= 2) {
			m_npc_list[npc_h]->m_behavior = Behavior::Attack;
			m_npc_list[npc_h]->m_behavior_turn_count = 0;
			return;
		}
		break;

	default:
		if (m_game->dice(1, 2) == 1) npc_request_assistance(npc_h);
		break;
	}

	if (m_npc_list[npc_h]->m_behavior_turn_count > 10) {
		m_npc_list[npc_h]->m_behavior_turn_count = 0;
		m_npc_list[npc_h]->m_behavior = Behavior::Move;
		m_npc_list[npc_h]->m_tmp_error = 0;
		if (m_npc_list[npc_h]->m_hp <= 3) {
			int regen = m_game->dice(1, (m_npc_list[npc_h]->m_hp_max - m_npc_list[npc_h]->m_hp_min) / 2 + 5);
			m_npc_list[npc_h]->m_hp += regen;
			if (m_npc_list[npc_h]->m_hp <= 0) m_npc_list[npc_h]->m_hp = 1;
		}
		return;
	}

	target_search(npc_h, &target, &target_type);
	if (target != 0) {
		m_npc_list[npc_h]->m_target_index = target;
		m_npc_list[npc_h]->m_target_type = target_type;
	}

	sX = m_npc_list[npc_h]->m_x;
	sY = m_npc_list[npc_h]->m_y;
	switch (m_npc_list[npc_h]->m_target_type) {
	case hb::shared::owner_class::Player:
		dX = m_game->m_client_list[m_npc_list[npc_h]->m_target_index]->m_x;
		dY = m_game->m_client_list[m_npc_list[npc_h]->m_target_index]->m_y;
		break;
	case hb::shared::owner_class::Npc:
		dX = m_npc_list[m_npc_list[npc_h]->m_target_index]->m_x;
		dY = m_npc_list[m_npc_list[npc_h]->m_target_index]->m_y;
		break;
	}
	dX = sX - (dX - sX);
	dY = sY - (dY - sY);

	dir = m_game->get_next_move_dir(sX, sY, dX, dY, m_npc_list[npc_h]->m_map_index, m_npc_list[npc_h]->m_turn, &m_npc_list[npc_h]->m_tmp_error);
	if (dir == 0) {
	}
	else {
		dX = m_npc_list[npc_h]->m_x + _tmp_cTmpDirX[dir];
		dY = m_npc_list[npc_h]->m_y + _tmp_cTmpDirY[dir];
		m_map_list[m_npc_list[npc_h]->m_map_index]->clear_owner(11, npc_h, hb::shared::owner_class::Npc, m_npc_list[npc_h]->m_x, m_npc_list[npc_h]->m_y);
		m_map_list[m_npc_list[npc_h]->m_map_index]->set_owner(npc_h, hb::shared::owner_class::Npc, dX, dY);
		m_npc_list[npc_h]->m_x = dX;
		m_npc_list[npc_h]->m_y = dY;
		m_npc_list[npc_h]->m_dir = dir;
		m_game->send_event_to_near_client_type_a(npc_h, hb::shared::owner_class::Npc, MsgId::EventMotion, Type::Move, 0, 0, 0);
	}
}

void CEntityManager::npc_behavior_stop(int npc_h)
{
	char  target_type;
	short target = 0;
	bool  flag;

	if (m_npc_list[npc_h] == nullptr) return;

	m_npc_list[npc_h]->m_behavior_turn_count++;

	switch (m_npc_list[npc_h]->m_action_limit) {
	case 5:
		switch (m_npc_list[npc_h]->m_type) {
		case 38:
			if (m_npc_list[npc_h]->m_behavior_turn_count >= 3) {
				m_npc_list[npc_h]->m_behavior_turn_count = 0;
				flag = npc_behavior_mana_collector(npc_h);
				if (flag) {
					m_game->send_event_to_near_client_type_a(npc_h, hb::shared::owner_class::Npc, MsgId::EventMotion, Type::Attack, m_npc_list[npc_h]->m_x, m_npc_list[npc_h]->m_y, 1);
				}
			}
			break;

		case 39: // Detector
			if (m_npc_list[npc_h]->m_behavior_turn_count >= 3) {
				m_npc_list[npc_h]->m_behavior_turn_count = 0;
				flag = npc_behavior_detector(npc_h);

				if (flag) {
					m_game->send_event_to_near_client_type_a(npc_h, hb::shared::owner_class::Npc, MsgId::EventMotion, Type::Attack, m_npc_list[npc_h]->m_x, m_npc_list[npc_h]->m_y, 1);
				}
			}
			break;

		case 40: // Energy Shield Generator
			break;

		case 41: // Grand Magic Generator
			if (m_npc_list[npc_h]->m_behavior_turn_count >= 3) {
				m_npc_list[npc_h]->m_behavior_turn_count = 0;
				npc_behavior_grand_magic_generator(npc_h);
			}
			break;

		case 42: // ManaStone: v2.05    5 .
			m_npc_list[npc_h]->m_behavior_turn_count = 0;
			m_npc_list[npc_h]->m_v1 += 5;
			if (m_npc_list[npc_h]->m_v1 >= 5) m_npc_list[npc_h]->m_v1 = 5;
			break;

		default:
			target_search(npc_h, &target, &target_type);
			break;
		}
		break;
	}

	if ((target != 0)) {

		m_npc_list[npc_h]->m_behavior = Behavior::Attack;
		m_npc_list[npc_h]->m_behavior_turn_count = 0;
		m_npc_list[npc_h]->m_target_index = target;
		m_npc_list[npc_h]->m_target_type = target_type;
		return;
	}
}

void CEntityManager::npc_behavior_dead(int npc_h)
{
	uint32_t time;

	if (m_npc_list[npc_h] == nullptr) return;

	time = GameClock::GetTimeMS();
	m_npc_list[npc_h]->m_behavior_turn_count++;
	if (m_npc_list[npc_h]->m_behavior_turn_count > 5) {
		m_npc_list[npc_h]->m_behavior_turn_count = 0;
	}

	uint32_t time_since_death = time - m_npc_list[npc_h]->m_dead_time;
	if (time_since_death > m_npc_list[npc_h]->m_regen_time) {
		delete_entity(npc_h);
	}
}

void CEntityManager::calc_next_waypoint_destination(int npc_h)
{
	short sX, sY;
	int j, map_index;
	bool flag;

	switch (m_npc_list[npc_h]->m_move_type) {
	case MoveType::Guard:
		break;

	case MoveType::SeqWaypoint:

		m_npc_list[npc_h]->m_cur_waypoint++;
		if (m_npc_list[npc_h]->m_cur_waypoint >= m_npc_list[npc_h]->m_total_waypoint)
			m_npc_list[npc_h]->m_cur_waypoint = 1;
		m_npc_list[npc_h]->m_dx = (short)(m_map_list[m_npc_list[npc_h]->m_map_index]->m_waypoint_list[m_npc_list[npc_h]->m_waypoint_index[m_npc_list[npc_h]->m_cur_waypoint]].x);
		m_npc_list[npc_h]->m_dy = (short)(m_map_list[m_npc_list[npc_h]->m_map_index]->m_waypoint_list[m_npc_list[npc_h]->m_waypoint_index[m_npc_list[npc_h]->m_cur_waypoint]].y);
		break;

	case MoveType::RandomWaypoint:

		m_npc_list[npc_h]->m_cur_waypoint = (short)((rand() % (m_npc_list[npc_h]->m_total_waypoint - 1)) + 1);
		m_npc_list[npc_h]->m_dx = (short)(m_map_list[m_npc_list[npc_h]->m_map_index]->m_waypoint_list[m_npc_list[npc_h]->m_waypoint_index[m_npc_list[npc_h]->m_cur_waypoint]].x);
		m_npc_list[npc_h]->m_dy = (short)(m_map_list[m_npc_list[npc_h]->m_map_index]->m_waypoint_list[m_npc_list[npc_h]->m_waypoint_index[m_npc_list[npc_h]->m_cur_waypoint]].y);
		break;

	case MoveType::RandomArea:

		m_npc_list[npc_h]->m_dx = (short)((rand() % m_npc_list[npc_h]->m_random_area.width) + m_npc_list[npc_h]->m_random_area.x);
		m_npc_list[npc_h]->m_dy = (short)((rand() % m_npc_list[npc_h]->m_random_area.height) + m_npc_list[npc_h]->m_random_area.y);
		break;

	case MoveType::Random:
		//m_npc_list[npc_h]->m_dx = (rand() % (m_map_list[m_npc_list[npc_h]->m_map_index]->m_size_x - 50)) + 15;
		//m_npc_list[npc_h]->m_dy = (rand() % (m_map_list[m_npc_list[npc_h]->m_map_index]->m_size_y - 50)) + 15;
		map_index = m_npc_list[npc_h]->m_map_index;

		for(int i = 0; i <= 30; i++) {
			sX = (rand() % (m_map_list[map_index]->m_size_x - 50)) + 15;
			sY = (rand() % (m_map_list[map_index]->m_size_y - 50)) + 15;

			flag = true;
			for (j = 0; j < smap::MaxMgar; j++)
				if (m_map_list[map_index]->m_mob_generator_avoid_rect[j].x != -1) {
					if ((sX >= m_map_list[map_index]->m_mob_generator_avoid_rect[j].Left()) &&
						(sX <= m_map_list[map_index]->m_mob_generator_avoid_rect[j].Right()) &&
						(sY >= m_map_list[map_index]->m_mob_generator_avoid_rect[j].Top()) &&
						(sY <= m_map_list[map_index]->m_mob_generator_avoid_rect[j].Bottom())) {
						// Avoid Rect     .
						flag = false;
					}
				}
			if (flag) break;
		}
		if (!flag) {
			// Fail!
			m_npc_list[npc_h]->m_tmp_error = 0;
			return;
		}
		m_npc_list[npc_h]->m_dx = sX;
		m_npc_list[npc_h]->m_dy = sY;
		break;
	}

	m_npc_list[npc_h]->m_tmp_error = 0; // @@@ !!! @@@
}

void CEntityManager::npc_magic_handler(int npc_h, short dX, short dY, short type)
{
	short  owner_h;
	char   owner_type;
	int err, sX, sY, tX, tY, result, whether_bonus, magic_attr;
	bool no_effect = false;
	uint32_t  time = GameClock::GetTimeMS();

	if (m_npc_list[npc_h] == nullptr) return;
	if ((dX < 0) || (dX >= m_map_list[m_npc_list[npc_h]->m_map_index]->m_size_x) ||
		(dY < 0) || (dY >= m_map_list[m_npc_list[npc_h]->m_map_index]->m_size_y)) return;

	if ((type < 0) || (type >= 100))     return;
	if (m_game->m_magic_config_list[type] == nullptr) return;

	if (m_map_list[m_npc_list[npc_h]->m_map_index]->m_is_attack_enabled == false) return;

	result = m_npc_list[npc_h]->m_magic_hit_ratio;

	whether_bonus = m_game->m_magic_manager->get_weather_magic_bonus_effect(type, m_map_list[m_npc_list[npc_h]->m_map_index]->m_weather_status);

	magic_attr = m_game->m_magic_config_list[type]->m_attribute;

	if (m_game->m_magic_config_list[type]->m_delay_time == 0) {
		switch (m_game->m_magic_config_list[type]->m_type) {
		case hb::shared::magic::Invisibility:
			switch (m_game->m_magic_config_list[type]->m_value_4) {
			case 1:
				m_map_list[m_npc_list[npc_h]->m_map_index]->get_owner(&owner_h, &owner_type, dX, dY);

				switch (owner_type) {
				case hb::shared::owner_class::Player:
					if (m_game->m_client_list[owner_h] == nullptr) { no_effect = true; break; }
					if (m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Invisibility] != 0) { no_effect = true; break; }
					m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Invisibility] = (char)m_game->m_magic_config_list[type]->m_value_4;
					m_game->m_status_effect_manager->set_invisibility_flag(owner_h, owner_type, true);
					m_game->m_combat_manager->remove_from_target(owner_h, hb::shared::owner_class::Player);
					break;

				case hb::shared::owner_class::Npc:
					if (m_npc_list[owner_h] == nullptr) { no_effect = true; break; }
					if (m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::Invisibility] != 0) { no_effect = true; break; }
					m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::Invisibility] = (char)m_game->m_magic_config_list[type]->m_value_4;
					m_game->m_status_effect_manager->set_invisibility_flag(owner_h, owner_type, true);
					// NPC    .
					m_game->m_combat_manager->remove_from_target(owner_h, hb::shared::owner_class::Npc);
					break;
				}

				if (!no_effect) {
					m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Invisibility, time + (m_game->m_magic_config_list[type]->m_last_time * 1000),
						owner_h, owner_type, 0, 0, 0, m_game->m_magic_config_list[type]->m_value_4, 0, 0);

					if (owner_type == hb::shared::owner_class::Player)
						m_game->send_notify_msg(0, owner_h, Notify::MagicEffectOn, hb::shared::magic::Invisibility, m_game->m_magic_config_list[type]->m_value_4, 0, 0);
				}
				break;

			case 2:
				// dX, dY  8  Invisibility  Object   .
				for(int ix = dX - 8; ix <= dX + 8 && !no_effect; ix++)
					for(int iy = dY - 8; iy <= dY + 8 && !no_effect; iy++) {
						m_map_list[m_npc_list[npc_h]->m_map_index]->get_owner(&owner_h, &owner_type, ix, iy);
						if (owner_h != 0) {
							switch (owner_type) {
							case hb::shared::owner_class::Player:
								if (m_game->m_client_list[owner_h] == nullptr) { no_effect = true; break; }
								if (m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Invisibility] != 0) {
									if (m_game->m_client_list[owner_h]->m_type != 66) {
										m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Invisibility] = 0;
										m_game->m_status_effect_manager->set_invisibility_flag(owner_h, owner_type, false);
										m_game->m_delay_event_manager->remove_from_delay_event_list(owner_h, owner_type, hb::shared::magic::Invisibility);
									}
								}
								break;

							case hb::shared::owner_class::Npc:
								if (m_npc_list[owner_h] == nullptr) { no_effect = true; break; }
								if (m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::Invisibility] != 0) {
									if (m_game->m_client_list[owner_h]->m_type != 66) {
										m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::Invisibility] = 0;
										m_game->m_status_effect_manager->set_invisibility_flag(owner_h, owner_type, false);
										m_game->m_delay_event_manager->remove_from_delay_event_list(owner_h, owner_type, hb::shared::magic::Invisibility);
									}
								}
								break;
							}
						}
					}
				break;
			}
			break;

		case hb::shared::magic::HoldObject:
			m_map_list[m_npc_list[npc_h]->m_map_index]->get_owner(&owner_h, &owner_type, dX, dY);
			if (m_game->m_combat_manager->check_resisting_magic_success(m_npc_list[npc_h]->m_dir, owner_h, owner_type, result) == false) {

				switch (owner_type) {
				case hb::shared::owner_class::Player:
					if (m_game->m_client_list[owner_h] == nullptr) { no_effect = true; break; }
					if (m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::HoldObject] != 0) { no_effect = true; break; }
					if (m_game->m_client_list[owner_h]->m_add_poison_resistance >= 500) { no_effect = true; break; }
					m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::HoldObject] = (char)m_game->m_magic_config_list[type]->m_value_4;
					break;

				case hb::shared::owner_class::Npc:
					if (m_npc_list[owner_h] == nullptr) { no_effect = true; break; }
					if (m_npc_list[owner_h]->m_magic_level >= 6) { no_effect = true; break; }
					if (m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::HoldObject] != 0) { no_effect = true; break; }
					m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::HoldObject] = (char)m_game->m_magic_config_list[type]->m_value_4;
					break;
				}

				if (!no_effect) {
					m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::HoldObject, time + (m_game->m_magic_config_list[type]->m_last_time * 1000),
						owner_h, owner_type, 0, 0, 0, m_game->m_magic_config_list[type]->m_value_4, 0, 0);

					if (owner_type == hb::shared::owner_class::Player)
						m_game->send_notify_msg(0, owner_h, Notify::MagicEffectOn, hb::shared::magic::HoldObject, m_game->m_magic_config_list[type]->m_value_4, 0, 0);
				}
			}
			break;

		case hb::shared::magic::DamageLinear:
			sX = m_npc_list[npc_h]->m_x;
			sY = m_npc_list[npc_h]->m_y;

			for(int i = 2; i < 10; i++) {
				err = 0;
				CMisc::GetPoint2(sX, sY, dX, dY, &tX, &tY, &err, i);

				// tx, ty
				m_map_list[m_npc_list[npc_h]->m_map_index]->get_owner(&owner_h, &owner_type, tX, tY);
				if (m_game->m_combat_manager->check_resisting_magic_success(m_npc_list[npc_h]->m_dir, owner_h, owner_type, result) == false)
					m_game->m_combat_manager->effect_damage_spot(npc_h, hb::shared::owner_class::Npc, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);

				m_map_list[m_npc_list[npc_h]->m_map_index]->get_dead_owner(&owner_h, &owner_type, tX, tY);
				if ((owner_type == hb::shared::owner_class::Player) && (m_game->m_client_list[owner_h] != nullptr) &&
					(m_game->m_client_list[owner_h]->m_hp > 0)) {
					if (m_game->m_combat_manager->check_resisting_magic_success(m_npc_list[npc_h]->m_dir, owner_h, owner_type, result) == false)
						m_game->m_combat_manager->effect_damage_spot(npc_h, hb::shared::owner_class::Npc, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);
				}

				// tx-1, ty
				m_map_list[m_npc_list[npc_h]->m_map_index]->get_owner(&owner_h, &owner_type, tX - 1, tY);
				if (m_game->m_combat_manager->check_resisting_magic_success(m_npc_list[npc_h]->m_dir, owner_h, owner_type, result) == false)
					m_game->m_combat_manager->effect_damage_spot(npc_h, hb::shared::owner_class::Npc, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);

				m_map_list[m_npc_list[npc_h]->m_map_index]->get_dead_owner(&owner_h, &owner_type, tX - 1, tY);
				if ((owner_type == hb::shared::owner_class::Player) && (m_game->m_client_list[owner_h] != nullptr) &&
					(m_game->m_client_list[owner_h]->m_hp > 0)) {
					if (m_game->m_combat_manager->check_resisting_magic_success(m_npc_list[npc_h]->m_dir, owner_h, owner_type, result) == false)
						m_game->m_combat_manager->effect_damage_spot(npc_h, hb::shared::owner_class::Npc, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);
				}

				// tx+1, ty
				m_map_list[m_npc_list[npc_h]->m_map_index]->get_owner(&owner_h, &owner_type, tX + 1, tY);
				if (m_game->m_combat_manager->check_resisting_magic_success(m_npc_list[npc_h]->m_dir, owner_h, owner_type, result) == false)
					m_game->m_combat_manager->effect_damage_spot(npc_h, hb::shared::owner_class::Npc, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);

				m_map_list[m_npc_list[npc_h]->m_map_index]->get_dead_owner(&owner_h, &owner_type, tX + 1, tY);
				if ((owner_type == hb::shared::owner_class::Player) && (m_game->m_client_list[owner_h] != nullptr) &&
					(m_game->m_client_list[owner_h]->m_hp > 0)) {
					if (m_game->m_combat_manager->check_resisting_magic_success(m_npc_list[npc_h]->m_dir, owner_h, owner_type, result) == false)
						m_game->m_combat_manager->effect_damage_spot(npc_h, hb::shared::owner_class::Npc, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);
				}

				// tx, ty-1
				m_map_list[m_npc_list[npc_h]->m_map_index]->get_owner(&owner_h, &owner_type, tX, tY - 1);
				if (m_game->m_combat_manager->check_resisting_magic_success(m_npc_list[npc_h]->m_dir, owner_h, owner_type, result) == false)
					m_game->m_combat_manager->effect_damage_spot(npc_h, hb::shared::owner_class::Npc, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);

				m_map_list[m_npc_list[npc_h]->m_map_index]->get_dead_owner(&owner_h, &owner_type, tX, tY - 1);
				if ((owner_type == hb::shared::owner_class::Player) && (m_game->m_client_list[owner_h] != nullptr) &&
					(m_game->m_client_list[owner_h]->m_hp > 0)) {
					if (m_game->m_combat_manager->check_resisting_magic_success(m_npc_list[npc_h]->m_dir, owner_h, owner_type, result) == false)
						m_game->m_combat_manager->effect_damage_spot(npc_h, hb::shared::owner_class::Npc, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);
				}

				// tx, ty+1
				m_map_list[m_npc_list[npc_h]->m_map_index]->get_owner(&owner_h, &owner_type, tX, tY + 1);
				if (m_game->m_combat_manager->check_resisting_magic_success(m_npc_list[npc_h]->m_dir, owner_h, owner_type, result) == false)
					m_game->m_combat_manager->effect_damage_spot(npc_h, hb::shared::owner_class::Npc, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);

				m_map_list[m_npc_list[npc_h]->m_map_index]->get_dead_owner(&owner_h, &owner_type, tX, tY + 1);
				if ((owner_type == hb::shared::owner_class::Player) && (m_game->m_client_list[owner_h] != nullptr) &&
					(m_game->m_client_list[owner_h]->m_hp > 0)) {
					if (m_game->m_combat_manager->check_resisting_magic_success(m_npc_list[npc_h]->m_dir, owner_h, owner_type, result) == false)
						m_game->m_combat_manager->effect_damage_spot(npc_h, hb::shared::owner_class::Npc, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);
				}

				if ((abs(tX - dX) <= 1) && (abs(tY - dY) <= 1)) break;
			}

			for(int iy = dY - m_game->m_magic_config_list[type]->m_value_3; iy <= dY + m_game->m_magic_config_list[type]->m_value_3; iy++)
				for(int ix = dX - m_game->m_magic_config_list[type]->m_value_2; ix <= dX + m_game->m_magic_config_list[type]->m_value_2; ix++) {
					m_map_list[m_npc_list[npc_h]->m_map_index]->get_owner(&owner_h, &owner_type, ix, iy);
					if (m_game->m_combat_manager->check_resisting_magic_success(m_npc_list[npc_h]->m_dir, owner_h, owner_type, result) == false)
						m_game->m_combat_manager->effect_damage_spot(npc_h, hb::shared::owner_class::Npc, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);

					m_map_list[m_npc_list[npc_h]->m_map_index]->get_dead_owner(&owner_h, &owner_type, ix, iy);
					if ((owner_type == hb::shared::owner_class::Player) && (m_game->m_client_list[owner_h] != nullptr) &&
						(m_game->m_client_list[owner_h]->m_hp > 0)) {
						if (m_game->m_combat_manager->check_resisting_magic_success(m_npc_list[npc_h]->m_dir, owner_h, owner_type, result) == false)
							m_game->m_combat_manager->effect_damage_spot(npc_h, hb::shared::owner_class::Npc, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);
					}
				}

			// dX, dY
			m_map_list[m_npc_list[npc_h]->m_map_index]->get_owner(&owner_h, &owner_type, dX, dY);
			if (m_game->m_combat_manager->check_resisting_magic_success(m_npc_list[npc_h]->m_dir, owner_h, owner_type, result) == false)
				m_game->m_combat_manager->effect_damage_spot(npc_h, hb::shared::owner_class::Npc, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6 + whether_bonus, false, magic_attr);

			m_map_list[m_npc_list[npc_h]->m_map_index]->get_dead_owner(&owner_h, &owner_type, dX, dY);
			if ((owner_type == hb::shared::owner_class::Player) && (m_game->m_client_list[owner_h] != nullptr) &&
				(m_game->m_client_list[owner_h]->m_hp > 0)) {
				if (m_game->m_combat_manager->check_resisting_magic_success(m_npc_list[npc_h]->m_dir, owner_h, owner_type, result) == false)
					m_game->m_combat_manager->effect_damage_spot(npc_h, hb::shared::owner_class::Npc, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6 + whether_bonus, false, magic_attr);
			}
			break;

		case hb::shared::magic::DamageSpot:
			m_map_list[m_npc_list[npc_h]->m_map_index]->get_owner(&owner_h, &owner_type, dX, dY);
			if (m_game->m_combat_manager->check_resisting_magic_success(m_npc_list[npc_h]->m_dir, owner_h, owner_type, result) == false)
				m_game->m_combat_manager->effect_damage_spot(npc_h, hb::shared::owner_class::Npc, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6 + whether_bonus, true, magic_attr);

			m_map_list[m_npc_list[npc_h]->m_map_index]->get_dead_owner(&owner_h, &owner_type, dX, dY);
			if ((owner_type == hb::shared::owner_class::Player) && (m_game->m_client_list[owner_h] != nullptr) &&
				(m_game->m_client_list[owner_h]->m_hp > 0)) {
				if (m_game->m_combat_manager->check_resisting_magic_success(m_npc_list[npc_h]->m_dir, owner_h, owner_type, result) == false)
					m_game->m_combat_manager->effect_damage_spot(npc_h, hb::shared::owner_class::Npc, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6 + whether_bonus, true, magic_attr);
			}
			break;

		case hb::shared::magic::HpUpSpot:
			m_map_list[m_npc_list[npc_h]->m_map_index]->get_owner(&owner_h, &owner_type, dX, dY);
			m_game->m_combat_manager->effect_hp_up_spot(npc_h, hb::shared::owner_class::Npc, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6);
			break;

		case hb::shared::magic::DamageArea:
			m_map_list[m_npc_list[npc_h]->m_map_index]->get_owner(&owner_h, &owner_type, dX, dY);
			if (m_game->m_combat_manager->check_resisting_magic_success(m_npc_list[npc_h]->m_dir, owner_h, owner_type, result) == false)
				m_game->m_combat_manager->effect_damage_spot(npc_h, hb::shared::owner_class::Npc, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6 + whether_bonus, true, magic_attr);

			m_map_list[m_npc_list[npc_h]->m_map_index]->get_dead_owner(&owner_h, &owner_type, dX, dY);
			if ((owner_type == hb::shared::owner_class::Player) && (m_game->m_client_list[owner_h] != nullptr) &&
				(m_game->m_client_list[owner_h]->m_hp > 0)) {
				if (m_game->m_combat_manager->check_resisting_magic_success(m_npc_list[npc_h]->m_dir, owner_h, owner_type, result) == false)
					m_game->m_combat_manager->effect_damage_spot(npc_h, hb::shared::owner_class::Npc, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6 + whether_bonus, true, magic_attr);
			}

			for(int iy = dY - m_game->m_magic_config_list[type]->m_value_3; iy <= dY + m_game->m_magic_config_list[type]->m_value_3; iy++)
				for(int ix = dX - m_game->m_magic_config_list[type]->m_value_2; ix <= dX + m_game->m_magic_config_list[type]->m_value_2; ix++) {
					m_map_list[m_npc_list[npc_h]->m_map_index]->get_owner(&owner_h, &owner_type, ix, iy);
					if (m_game->m_combat_manager->check_resisting_magic_success(m_npc_list[npc_h]->m_dir, owner_h, owner_type, result) == false)
						m_game->m_combat_manager->effect_damage_spot_damage_move(npc_h, hb::shared::owner_class::Npc, owner_h, owner_type, dX, dY, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);

					m_map_list[m_npc_list[npc_h]->m_map_index]->get_dead_owner(&owner_h, &owner_type, ix, iy);
					if ((owner_type == hb::shared::owner_class::Player) && (m_game->m_client_list[owner_h] != nullptr) &&
						(m_game->m_client_list[owner_h]->m_hp > 0)) {
						if (m_game->m_combat_manager->check_resisting_magic_success(m_npc_list[npc_h]->m_dir, owner_h, owner_type, result) == false)
							m_game->m_combat_manager->effect_damage_spot_damage_move(npc_h, hb::shared::owner_class::Npc, owner_h, owner_type, dX, dY, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);
					}
				}
			break;

		case hb::shared::magic::DamageAreaNoSpot:
			for(int iy = dY - m_game->m_magic_config_list[type]->m_value_3; iy <= dY + m_game->m_magic_config_list[type]->m_value_3; iy++)
				for(int ix = dX - m_game->m_magic_config_list[type]->m_value_2; ix <= dX + m_game->m_magic_config_list[type]->m_value_2; ix++) {
					m_map_list[m_npc_list[npc_h]->m_map_index]->get_owner(&owner_h, &owner_type, ix, iy);
					if (m_game->m_combat_manager->check_resisting_magic_success(m_npc_list[npc_h]->m_dir, owner_h, owner_type, result) == false)
						m_game->m_combat_manager->effect_damage_spot_damage_move(npc_h, hb::shared::owner_class::Npc, owner_h, owner_type, dX, dY, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);

					m_map_list[m_npc_list[npc_h]->m_map_index]->get_dead_owner(&owner_h, &owner_type, ix, iy);
					if ((owner_type == hb::shared::owner_class::Player) && (m_game->m_client_list[owner_h] != nullptr) &&
						(m_game->m_client_list[owner_h]->m_hp > 0)) {
						if (m_game->m_combat_manager->check_resisting_magic_success(m_npc_list[npc_h]->m_dir, owner_h, owner_type, result) == false)
							m_game->m_combat_manager->effect_damage_spot_damage_move(npc_h, hb::shared::owner_class::Npc, owner_h, owner_type, dX, dY, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);
					}
				}
			break;

		case hb::shared::magic::SpDownArea:
			m_map_list[m_npc_list[npc_h]->m_map_index]->get_owner(&owner_h, &owner_type, dX, dY);
			if (m_game->m_combat_manager->check_resisting_magic_success(m_npc_list[npc_h]->m_dir, owner_h, owner_type, result) == false)
				m_game->m_combat_manager->effect_sp_down_spot(npc_h, hb::shared::owner_class::Npc, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6);
			for(int iy = dY - m_game->m_magic_config_list[type]->m_value_3; iy <= dY + m_game->m_magic_config_list[type]->m_value_3; iy++)
				for(int ix = dX - m_game->m_magic_config_list[type]->m_value_2; ix <= dX + m_game->m_magic_config_list[type]->m_value_2; ix++) {
					m_map_list[m_npc_list[npc_h]->m_map_index]->get_owner(&owner_h, &owner_type, ix, iy);
					if (m_game->m_combat_manager->check_resisting_magic_success(m_npc_list[npc_h]->m_dir, owner_h, owner_type, result) == false)
						m_game->m_combat_manager->effect_sp_down_spot(npc_h, hb::shared::owner_class::Npc, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9);
				}
			break;

		case hb::shared::magic::SpUpArea:
			m_map_list[m_npc_list[npc_h]->m_map_index]->get_owner(&owner_h, &owner_type, dX, dY);
			m_game->m_combat_manager->effect_sp_up_spot(npc_h, hb::shared::owner_class::Npc, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6);
			for(int iy = dY - m_game->m_magic_config_list[type]->m_value_3; iy <= dY + m_game->m_magic_config_list[type]->m_value_3; iy++)
				for(int ix = dX - m_game->m_magic_config_list[type]->m_value_2; ix <= dX + m_game->m_magic_config_list[type]->m_value_2; ix++) {
					m_map_list[m_npc_list[npc_h]->m_map_index]->get_owner(&owner_h, &owner_type, ix, iy);
					m_game->m_combat_manager->effect_sp_up_spot(npc_h, hb::shared::owner_class::Npc, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9);
				}
			break;

		}
	}
	else {
		// Casting

	}

	// Mana .
	m_npc_list[npc_h]->m_mana -= m_game->m_magic_config_list[type]->m_value_1; // value1 Mana Cost
	if (m_npc_list[npc_h]->m_mana < 0)
		m_npc_list[npc_h]->m_mana = 0;

	// .  + 100
	m_game->send_event_to_near_client_type_b(MsgId::EventCommon, CommonType::Magic, m_npc_list[npc_h]->m_map_index,
		m_npc_list[npc_h]->m_x, m_npc_list[npc_h]->m_y, dX, dY, (type + 100), m_npc_list[npc_h]->m_type);

}

EntityRelationship CEntityManager::get_npc_relationship(int npc_h, int viewer_h)
{
	if (m_game->m_client_list[viewer_h] == nullptr) return EntityRelationship::Neutral;
	if (m_npc_list[npc_h] == nullptr) return EntityRelationship::Neutral;

	int npcSide = m_npc_list[npc_h]->m_side;
	int viewerSide = m_game->m_client_list[viewer_h]->m_side;

	// Side 10 = always hostile (monsters, aggressive NPCs)
	if (npcSide == 10) return EntityRelationship::Enemy;

	// NPC side 0 = neutral (townfolk, shopkeepers) or viewer has no faction
	if (npcSide == 0 || viewerSide == 0) return EntityRelationship::Neutral;

	// Same faction = friendly, different = enemy
	if (npcSide == viewerSide) return EntityRelationship::Friendly;
	return EntityRelationship::Enemy;
}

void CEntityManager::npc_request_assistance(int npc_h)
{
	int sX, sY;
	short owner_h;
	char  owner_type;

	// iNpc     NPC  .
	if (m_npc_list[npc_h] == nullptr) return;

	sX = m_npc_list[npc_h]->m_x;
	sY = m_npc_list[npc_h]->m_y;

	for(int ix = sX - 8; ix <= sX + 8; ix++)
		for(int iy = sY - 8; iy <= sY + 8; iy++) {
			m_map_list[m_npc_list[npc_h]->m_map_index]->get_owner(&owner_h, &owner_type, ix, iy);
			if ((owner_h != 0) && (m_npc_list[owner_h] != nullptr) && (owner_type == hb::shared::owner_class::Npc) &&
				(npc_h != owner_h) && (m_npc_list[owner_h]->m_side == m_npc_list[npc_h]->m_side) &&
				(m_npc_list[owner_h]->m_is_perm_attack_mode == false) && (m_npc_list[owner_h]->m_behavior == Behavior::Move)) {

				// NPC .
				m_npc_list[owner_h]->m_behavior = Behavior::Attack;
				m_npc_list[owner_h]->m_behavior_turn_count = 0;
				m_npc_list[owner_h]->m_target_index = m_npc_list[npc_h]->m_target_index;
				m_npc_list[owner_h]->m_target_type = m_npc_list[npc_h]->m_target_type;

				return;
			}
		}
}

bool CEntityManager::npc_behavior_mana_collector(int npc_h)
{
	int dX, dY, max_mp, total;
	short owner_h;
	char  owner_type;
	double v1, v2, v3;
	bool ret;

	if (m_npc_list[npc_h] == nullptr) return false;
	if (m_npc_list[npc_h]->m_appearance.HasSpecialState()) return false;

	ret = false;
	for (dX = m_npc_list[npc_h]->m_x - 5; dX <= m_npc_list[npc_h]->m_x + 5; dX++)
		for (dY = m_npc_list[npc_h]->m_y - 5; dY <= m_npc_list[npc_h]->m_y + 5; dY++) {
			m_map_list[m_npc_list[npc_h]->m_map_index]->get_owner(&owner_h, &owner_type, dX, dY);
			if (owner_h != 0) {
				switch (owner_type) {
				case hb::shared::owner_class::Player:
					if (m_npc_list[npc_h]->m_side == m_game->m_client_list[owner_h]->m_side) {
						max_mp = m_game->get_max_mp(owner_h);
						if (m_game->m_client_list[owner_h]->m_mp < max_mp) {
							total = m_game->dice(1, (m_game->m_client_list[owner_h]->m_mag + m_game->m_client_list[owner_h]->m_angelic_mag));
							if (m_game->m_client_list[owner_h]->m_add_mp != 0) {
								v2 = (double)total;
								v3 = (double)m_game->m_client_list[owner_h]->m_add_mp;
								v1 = (v3 / 100.0f) * v2;
								total += (int)v1;
							}

							m_game->m_client_list[owner_h]->m_mp += total;

							if (m_game->m_client_list[owner_h]->m_mp > max_mp)
								m_game->m_client_list[owner_h]->m_mp = max_mp;

							m_game->send_notify_msg(0, owner_h, Notify::Mp, 0, 0, 0, 0);
						}
					}
					break;

				case hb::shared::owner_class::Npc:
					if ((m_npc_list[owner_h]->m_type == 42) && (m_npc_list[owner_h]->m_v1 > 0)) {
						if (m_npc_list[owner_h]->m_v1 >= 3) {
							m_game->m_collected_mana[m_npc_list[npc_h]->m_side] += 3;
							m_npc_list[owner_h]->m_v1 -= 3;
							ret = true;
						}
						else {
							m_game->m_collected_mana[m_npc_list[npc_h]->m_side] += m_npc_list[owner_h]->m_v1;
							m_npc_list[owner_h]->m_v1 = 0;
							ret = true;
						}
					}
					break;
				}
			}
	}
	return ret;
}

void CEntityManager::npc_behavior_grand_magic_generator(int npc_h)
{
	switch (m_npc_list[npc_h]->m_side) {
	case 1:
		if (m_game->m_aresden_mana > GmgManaConsumeUnit) {
			m_game->m_aresden_mana = 0;
			m_npc_list[npc_h]->m_mana_stock++;
			if (m_npc_list[npc_h]->m_mana_stock > m_npc_list[npc_h]->m_max_mana) {
				m_game->m_war_manager->grand_magic_launch_msg_send(1, 1);
				m_game->m_war_manager->meteor_strike_msg_handler(1);
				m_npc_list[npc_h]->m_mana_stock = 0;
				m_game->m_aresden_mana = 0;
			}
			hb::logger::log("Aresden GMG {}/{}", m_npc_list[npc_h]->m_mana_stock, m_npc_list[npc_h]->m_max_mana);
		}
		break;

	case 2:
		if (m_game->m_elvine_mana > GmgManaConsumeUnit) {
			m_game->m_elvine_mana = 0;
			m_npc_list[npc_h]->m_mana_stock++;
			if (m_npc_list[npc_h]->m_mana_stock > m_npc_list[npc_h]->m_max_mana) {
				m_game->m_war_manager->grand_magic_launch_msg_send(1, 2);
				m_game->m_war_manager->meteor_strike_msg_handler(2);
				m_npc_list[npc_h]->m_mana_stock = 0;
				m_game->m_elvine_mana = 0;
			}
			hb::logger::log("Elvine GMG {}/{}", m_npc_list[npc_h]->m_mana_stock, m_npc_list[npc_h]->m_max_mana);
		}
		break;
	}
}

bool CEntityManager::npc_behavior_detector(int npc_h)
{
	int dX, dY;
	short owner_h;
	char  owner_type, side;
	bool  flag = false;

	if (m_npc_list[npc_h] == nullptr) return false;
	if (m_npc_list[npc_h]->m_appearance.HasSpecialState()) return false;

	for (dX = m_npc_list[npc_h]->m_x - 10; dX <= m_npc_list[npc_h]->m_x + 10; dX++)
		for (dY = m_npc_list[npc_h]->m_y - 10; dY <= m_npc_list[npc_h]->m_y + 10; dY++) {
			m_map_list[m_npc_list[npc_h]->m_map_index]->get_owner(&owner_h, &owner_type, dX, dY);

			side = 0;
			if (owner_h != 0) {
				switch (owner_type) {
				case hb::shared::owner_class::Player:
					side = m_game->m_client_list[owner_h]->m_side;
					break;

				case hb::shared::owner_class::Npc:
					side = m_npc_list[owner_h]->m_side;
					break;
				}
			}

			if ((side != 0) && (side != m_npc_list[npc_h]->m_side)) {
				switch (owner_type) {
				case hb::shared::owner_class::Player:
					if (m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Invisibility] != 0) {
						m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Invisibility] = 0;
						m_game->m_status_effect_manager->set_invisibility_flag(owner_h, owner_type, false);
					}
					break;

				case hb::shared::owner_class::Npc:
					if (m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::Invisibility] != 0) {
						m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::Invisibility] = 0;
						m_game->m_status_effect_manager->set_invisibility_flag(owner_h, owner_type, false);
					}
					break;
				}

				flag = true;
			}
		}

	return flag;
}

// ========================================================================
// NPC Spawns & Drops (ported from CGame)
// ========================================================================

bool CEntityManager::set_npc_follow_mode(char* name, char* follow_name, char follow_owner_type)
{
	int index, map_index, follow_index;
	char tmp_name[11], follow_side;

	std::memset(tmp_name, 0, sizeof(tmp_name));
	map_index = -1;
	follow_index = -1;

	for(int i = 1; i < MaxNpcs; i++)
		if ((m_npc_list[i] != nullptr) && (memcmp(m_npc_list[i]->m_name, name, 5) == 0)) {
			index = i;
			map_index = m_npc_list[i]->m_map_index;
			break;
		}

	switch (follow_owner_type) {
	case hb::shared::owner_class::Npc:
		for(int i = 1; i < MaxNpcs; i++)
			if ((m_npc_list[i] != nullptr) && (memcmp(m_npc_list[i]->m_name, follow_name, 5) == 0)) {
				if (m_npc_list[i]->m_map_index != map_index) return false;
				follow_index = i;
				follow_side = m_npc_list[i]->m_side;
				break;
			}
		break;

	case hb::shared::owner_class::Player:
		for(int i = 1; i < MaxClients; i++)
			if ((m_game->m_client_list[i] != nullptr) && (hb_strnicmp(m_game->m_client_list[i]->m_char_name, follow_name, hb::shared::limits::CharNameLen - 1) == 0)) {
				if (m_game->m_client_list[i]->m_map_index != map_index) return false;
				follow_index = i;
				follow_side = m_game->m_client_list[i]->m_side;
				break;
			}
		break;
	}

	if ((index == -1) || (follow_index == -1)) return false;

	m_npc_list[index]->m_move_type = MoveType::Follow;
	m_npc_list[index]->m_follow_owner_type = follow_owner_type;
	m_npc_list[index]->m_follow_owner_index = follow_index;
	m_npc_list[index]->m_side = follow_side;

	return true;
}

void CEntityManager::set_npc_attack_mode(char* name, int target_h, char target_type, bool is_perm_attack)
{
	int index;

	index = -1;
	for(int i = 1; i < MaxNpcs; i++)
		if ((m_npc_list[i] != nullptr) && (memcmp(m_npc_list[i]->m_name, name, 5) == 0)) {
			index = i;
			break;

			//testcode
			//PutLogList("set_npc_attack_mode - Npc found");
		}
	// NPC .
	if (index == -1) return;

	switch (target_type) {
	case hb::shared::owner_class::Player:
		if (m_game->m_client_list[target_h] == nullptr) return;
		break;

	case hb::shared::owner_class::Npc:
		if (m_npc_list[target_h] == nullptr) return;
		break;
	}

	m_npc_list[index]->m_behavior = Behavior::Attack;
	m_npc_list[index]->m_behavior_turn_count = 0;
	m_npc_list[index]->m_target_index = target_h;
	m_npc_list[index]->m_target_type = target_type;

	m_npc_list[index]->m_is_perm_attack_mode = is_perm_attack;

	//testcode
	//PutLogList("set_npc_attack_mode - complete");
}

bool CEntityManager::is_crusade_structure(int npc_type)
{
	switch (npc_type) {
	case 36: case 37: case 38: case 39: case 42:
		return true;
	default:
		return false;
	}
}

void CEntityManager::delete_npc_internal(int npc_h)
{
	int naming_value;
	char tmp[21];
	uint32_t time;
	if (m_npc_list[npc_h] == nullptr) return;

	time = GameClock::GetTimeMS();

	m_game->send_event_to_near_client_type_a(npc_h, hb::shared::owner_class::Npc, MsgId::EventLog, MsgType::Reject, 0, 0, 0);
	m_map_list[m_npc_list[npc_h]->m_map_index]->clear_owner(11, npc_h, hb::shared::owner_class::Npc, m_npc_list[npc_h]->m_x, m_npc_list[npc_h]->m_y);

	std::memset(tmp, 0, sizeof(tmp));
	strcpy(tmp, (char*)(m_npc_list[npc_h]->m_name + 2));
	// NPC NamigValue    .
	naming_value = atoi(tmp);

	// NamingValue     .
	m_map_list[m_npc_list[npc_h]->m_map_index]->set_naming_value_empty(naming_value);
	if (!m_npc_list[npc_h]->m_bypass_mob_limit) {
		m_map_list[m_npc_list[npc_h]->m_map_index]->m_total_active_object--;
	}

	// If NPC was never killed via on_entity_killed() (e.g. summoned NPC cleanup,
	// crop harvest, energy sphere, crusade structure removal), the alive counter
	// was never decremented — do it here to prevent m_total_alive_object drift.
	if (!m_npc_list[npc_h]->m_is_killed) {
		m_map_list[m_npc_list[npc_h]->m_map_index]->m_total_alive_object--;
	}

	// Spot-mob-generator
	if (m_npc_list[npc_h]->m_spot_mob_index != 0) {
		int spot_idx = m_npc_list[npc_h]->m_spot_mob_index;
		int map_idx = m_npc_list[npc_h]->m_map_index;
		m_map_list[map_idx]->m_spot_mob_generator[spot_idx].cur_mobs--;
	}

	m_game->m_combat_manager->remove_from_target(npc_h, hb::shared::owner_class::Npc);

	if (is_crusade_structure(m_npc_list[npc_h]->m_type)) {
		m_map_list[m_npc_list[npc_h]->m_map_index]->remove_crusade_structure_info(m_npc_list[npc_h]->m_x, m_npc_list[npc_h]->m_y);
	}
	else if (m_npc_list[npc_h]->m_type == 64) {
		m_map_list[m_npc_list[npc_h]->m_map_index]->remove_crops_total_sum();
	}

	// DelayEvent
	m_game->m_delay_event_manager->remove_from_delay_event_list(npc_h, hb::shared::owner_class::Npc, 0);
	delete m_npc_list[npc_h];
	m_npc_list[npc_h] = nullptr;
}

// Helper to apply drop rate multiplier (capped at 10000 = 100%)
static uint32_t ApplyDropMultiplier(uint32_t baseChance, float multiplier)
{
	double result = static_cast<double>(baseChance) * static_cast<double>(multiplier);
	if (result > 10000.0) return 10000;
	if (result < 0.0) return 0;
	return static_cast<uint32_t>(result);
}

// Base drop chances (out of 10000 = 100%)
static constexpr uint32_t BASE_PRIMARY_DROP_CHANCE = 1000;   // 10% base primary item drop chance
static constexpr uint32_t BASE_GOLD_DROP_CHANCE = 3000;      // 30% base gold drop chance
static constexpr uint32_t BASE_SECONDARY_DROP_CHANCE = 500;  // 5% base secondary/bonus drop chance

void CEntityManager::npc_dead_item_generator(int npc_h, short attacker_h, char attacker_type)
{
	if (m_npc_list[npc_h] == nullptr) return;
	if ((attacker_type != hb::shared::owner_class::Player) || (m_npc_list[npc_h]->m_is_summoned)) return;
	if (m_npc_list[npc_h]->m_is_unsummoned) return;

	switch (m_npc_list[npc_h]->m_type) {
	case 21: // Guard
	case 34: // Dummy
	case 64: // Crop
		return;
	}

	const DropTable* table = m_game->m_item_manager->get_drop_table(m_npc_list[npc_h]->m_drop_table_id);

	// Apply drop rate multipliers to base chances
	// At 1.0: normal, at 1.5: 150% more likely, at 2.0: 200%, etc.
	uint32_t primaryChance = ApplyDropMultiplier(BASE_PRIMARY_DROP_CHANCE, m_game->m_primary_drop_rate);
	uint32_t goldChance = ApplyDropMultiplier(BASE_GOLD_DROP_CHANCE, m_game->m_gold_drop_rate);

	bool droppedGold = false;
	if (m_game->dice(1, 10000) <= goldChance) {
		int minGold = static_cast<int>(m_npc_list[npc_h]->m_gold_dice_min);
		int maxGold = static_cast<int>(m_npc_list[npc_h]->m_gold_dice_max);
		if (minGold < 0) minGold = 0;
		if (maxGold < minGold) maxGold = minGold;
		if (maxGold > 0) {
			int amount = minGold;
			if (maxGold > minGold) {
				amount = m_game->dice(1, (maxGold - minGold)) + minGold;
			}
			if (amount > 0) {
				if ((attacker_type == hb::shared::owner_class::Player) &&
					(m_game->m_client_list[attacker_h] != nullptr) &&
					(m_game->m_client_list[attacker_h]->m_add_gold != 0)) {
					double bonus = (double)m_game->m_client_list[attacker_h]->m_add_gold;
					amount += static_cast<int>((bonus / 100.0f) * static_cast<double>(amount));
				}
				spawn_npc_drop_item(npc_h, 90, amount, amount);
				droppedGold = true;
			}
		}
	}

	// Primary item drop (from drop table tier 1) - uses same primary chance
	if (!droppedGold && table != nullptr) {
		if (m_game->dice(1, 10000) <= primaryChance) {
			int min_count = 1;
			int max_count = 1;
			int item_id = roll_drop_table_item(table, 1, min_count, max_count);
			if (item_id != 0) {
				if (item_id == 90) {
					min_count = static_cast<int>(m_npc_list[npc_h]->m_gold_dice_min);
					max_count = static_cast<int>(m_npc_list[npc_h]->m_gold_dice_max);
				}
				spawn_npc_drop_item(npc_h, item_id, min_count, max_count);
			}
		}
	}

	// Secondary/bonus drop (from drop table tier 2) - affected by secondary multiplier
	if (table != nullptr) {
		// Base secondary chance, modified by player rating
		double ratingModifier = 0.0;
		if (m_game->m_client_list[attacker_h] != nullptr) {
			ratingModifier = m_game->m_client_list[attacker_h]->m_rating * m_game->m_rep_drop_modifier;
			if (ratingModifier > 1000) ratingModifier = 1000;
			if (ratingModifier < -1000) ratingModifier = -1000;
		}

		// Calculate effective secondary drop chance with rating modifier and multiplier
		double baseSecondary = static_cast<double>(BASE_SECONDARY_DROP_CHANCE) - ratingModifier;
		double effectiveSecondary = baseSecondary * static_cast<double>(m_game->m_secondary_drop_rate);
		if (effectiveSecondary > 10000.0) effectiveSecondary = 10000.0;
		if (effectiveSecondary < 0.0) effectiveSecondary = 0.0;

		if (m_game->dice(1, 10000) <= static_cast<uint32_t>(effectiveSecondary)) {
			int min_count = 1;
			int max_count = 1;
			int item_id = roll_drop_table_item(table, 2, min_count, max_count);
			if (item_id != 0) {
				if (item_id == 90) {
					min_count = static_cast<int>(m_npc_list[npc_h]->m_gold_dice_min);
					max_count = static_cast<int>(m_npc_list[npc_h]->m_gold_dice_max);
				}
				spawn_npc_drop_item(npc_h, item_id, min_count, max_count);
			}
		}
	}
}

int CEntityManager::roll_drop_table_item(const DropTable* table, int tier, int& outMinCount, int& outMaxCount) const
{
	if (table == nullptr) return 0;
	if (tier < 1 || tier > 2) return 0;
	int total = table->total_weight[tier];
	if (total <= 0) return 0;
	int roll = m_game->dice(1, total);
	int cumulative = 0;
	for (const auto& entry : table->tierEntries[tier]) {
		cumulative += entry.weight;
		if (roll <= cumulative) {
			outMinCount = entry.min_count;
			outMaxCount = entry.max_count;
			return entry.item_id;
		}
	}
	return 0;
}

bool CEntityManager::spawn_npc_drop_item(int npc_h, int item_id, int min_count, int max_count)
{
	if (item_id <= 0) return false;
	if (m_npc_list[npc_h] == nullptr) return false;

	if (min_count < 0) min_count = 0;
	if (max_count < min_count) max_count = min_count;

	CItem* item = new CItem;
	if (m_game->m_item_manager->init_item_attr(item, item_id) == false) {
		delete item;
		return false;
	}

	uint32_t count = 1;
	if (item_id == 90) {
		if (max_count == 0) {
			delete item;
			return false;
		}
		count = static_cast<uint32_t>(max_count);
		if (max_count > min_count) {
			count = static_cast<uint32_t>(m_game->dice(1, (max_count - min_count)) + min_count);
		}
	} else {
		if (min_count <= 0) min_count = 1;
		count = static_cast<uint32_t>(max_count);
		if (max_count > min_count) {
			count = static_cast<uint32_t>(m_game->dice(1, (max_count - min_count)) + min_count);
		}
	}
	if (count == 0) {
		delete item;
		return false;
	}
	item->m_instance.count = count;
	m_game->m_item_manager->generate_item_attributes(item);
	item->set_touch_effect_type(TouchEffectType::ID);
	item->m_instance.touch_effect_value1 = static_cast<short>(m_game->dice(1, 100000));
	item->m_instance.touch_effect_value2 = static_cast<short>(m_game->dice(1, 100000));
	item->m_instance.touch_effect_value3 = (short)GameClock::GetTimeMS();
	m_map_list[m_npc_list[npc_h]->m_map_index]->set_item(m_npc_list[npc_h]->m_x, m_npc_list[npc_h]->m_y, item);
	m_game->send_ground_item_event(CommonType::ItemDrop, m_npc_list[npc_h]->m_map_index,
		m_npc_list[npc_h]->m_x, m_npc_list[npc_h]->m_y, item);
	m_game->m_item_manager->item_log(ItemLogAction::NewGenDrop, 0, m_npc_list[npc_h]->m_npc_name, item);
	return true;
}

int CEntityManager::get_entity_handle_by_guid(uint32_t guid) const
{
    if (guid == 0)
        return -1;

    for(int i = 1; i < MaxNpcs; i++) {
        if (m_entity_guid[i] == guid && m_npc_list[i] != nullptr)
            return i;
    }

    return -1;
}

uint32_t CEntityManager::get_entity_guid(int entity_handle) const
{
    if (entity_handle < 1 || entity_handle >= MaxNpcs)
        return 0;

    return m_entity_guid[entity_handle];
}

int CEntityManager::get_total_active_entities() const
{
    return m_total_entities;
}

int CEntityManager::get_map_entity_count(int map_index) const
{
    if (!m_initialized || map_index < 0 || map_index >= m_max_maps)
        return 0;

    if (m_map_list[map_index] == nullptr)
        return 0;

    return m_map_list[map_index]->m_total_active_object;
}

int CEntityManager::find_entity_by_name(const char* name) const
{
    if (name == nullptr)
        return -1;

    for(int i = 1; i < MaxNpcs; i++) {
        if (m_npc_list[i] != nullptr) {
            if (memcmp(m_npc_list[i]->m_name, name, 6) == 0)
                return i;
        }
    }

    return -1;
}

// ========================================================================
// Internal Helpers - STUBS
// ========================================================================

bool CEntityManager::init_entity_attributes(CNpc* npc, int npc_config_id, short sClass, char sa)
{
    if (m_game == nullptr)
        return false;

    return m_game->init_npc_attr(npc, npc_config_id, sClass, sa);
}

int CEntityManager::get_free_entity_slot() const
{
    for(int i = 1; i < MaxNpcs; i++) {
        if (m_npc_list[i] == nullptr)
            return i;
    }
    return -1; // No free slots
}

bool CEntityManager::is_valid_entity(int entity_handle) const
{
    if (entity_handle < 1 || entity_handle >= MaxNpcs)
        return false;

    return (m_npc_list[entity_handle] != nullptr);
}

void CEntityManager::generate_entity_loot(int entity_handle, short attacker_h, char attacker_type)
{
    if (m_game == nullptr)
        return;

    npc_dead_item_generator(entity_handle, attacker_h, attacker_type);
}

// ========================================================================
// Active Entity List Management (Performance Optimization)
// ========================================================================

void CEntityManager::add_to_active_list(int entity_handle)
{
    // Add entity to active list for fast iteration
    // This allows us to iterate only active entities (e.g., 70) instead of all slots (5000)

    if (entity_handle < 1 || entity_handle >= MaxNpcs) {
        return; // Invalid handle
    }

    // Check if already in list (shouldn't happen, but safety check)
    for(int i = 0; i < m_active_entity_count; i++) {
        if (m_active_entity_list[i] == entity_handle) {
            return; // Already in list
        }
    }

    // Add to end of list
    m_active_entity_list[m_active_entity_count] = entity_handle;
    m_active_entity_count++;

}

void CEntityManager::remove_from_active_list(int entity_handle)
{
    // Remove entity from active list when deleted
    // Uses swap-and-pop for O(1) removal

    if (entity_handle < 1 || entity_handle >= MaxNpcs) {
        return; // Invalid handle
    }

    // Find entity in list
    for(int i = 0; i < m_active_entity_count; i++) {
        if (m_active_entity_list[i] == entity_handle) {
            // Swap with last element and decrement count (O(1) removal)
            m_active_entity_list[i] = m_active_entity_list[m_active_entity_count - 1];
            m_active_entity_count--;

            return;
        }
    }

}

// ========================================================================
// Spawn Point Management - STUBS
// ========================================================================

void CEntityManager::process_random_spawns(int map_index)
{
	int naming_value, result;
	char npc_name[hb::shared::limits::NpcNameLen], cName_Master[11];
	bool firm_berserk, is_special_event = false;

	if (m_game == nullptr || m_game->m_on_exit_process) return;

	for(int i = 0; i < m_max_maps; i++) {
		// Random Mob Generator

		int result_num = 0;
		if (m_map_list[i] != nullptr) {
			result_num = (m_map_list[i]->m_maximum_object - 30);
		}

		if ( (m_map_list[i] != nullptr) && (m_map_list[i]->m_random_mob_generator ) && (result_num > m_map_list[i]->m_total_active_object) ) {
			if ((m_game->m_middleland_map_index != -1) && (m_game->m_middleland_map_index == i) && (m_game->m_is_crusade_mode )) break;

			naming_value = m_map_list[i]->get_empty_naming_value();
			int mob_dice_sides = 0, total_mob = 0, npc_id = 0;
			bool master = false;
			int pX = 0, pY = 0;
			int prob_sa = 0, kind_sa = 0, npc_config_id = 0;
			if (naming_value != -1) {
				std::memset(cName_Master, 0, sizeof(cName_Master));
				sprintf(cName_Master, "XX%d", naming_value);
				cName_Master[0] = '_';
				cName_Master[1] = i + 65;

				std::memset(npc_name, 0, sizeof(npc_name));

				firm_berserk = false;
				is_special_event = false;
				result = m_game->dice(1,100);
				switch (m_map_list[i]->m_random_mob_generator_level) {

				case 1: // arefarm, elvfarm, aresden, elvine
					if ((result >= 1) && (result < 20)) {
						result = 1; // Slime
					}
					else if ((result >= 20) && (result < 40)) {
						result = 2; // Giant-Ant
					}
					else if ((result >= 40) && (result < 85)) {
						result = 24; // Rabbit
					}
					else if ((result >= 85) && (result < 95)) {
						result = 25; // Cat
					}
					else if ((result >= 95) && (result <= 100)) {
						result = 3; // Orc
					}
					break;

				case 2:
					if ((result >= 1) && (result < 40)) {
						result = 1;
					}
					else if ((result >= 40) && (result < 80)) {
						result = 2;
					}
					else result = 10;
					break;

				case 3:
					if ((result >= 1) && (result < 20)) {
						switch (m_game->dice(1,2)) {
				case 1: result = 3;  break;
				case 2: result = 4;  break;
						}
					}
					else if ((result >= 20) && (result < 25)) {
						result = 30;
					}
					else if ((result >= 25) && (result < 50)) {
						switch (m_game->dice(1,3)) {	
				case 1: result = 5;  break;
				case 2: result = 6;  break;
				case 3:	result = 7;  break;
						}
					}
					else if ((result >= 50) && (result < 75)) {
						switch (m_game->dice(1,7)) {
				case 1:
				case 2:	result = 8;  break;
				case 3:	result = 11; break;
				case 4: result = 12; break;
				case 5:	result = 18; break;
				case 6:	result = 26; break;
				case 7: result = 28; break;
						}
					}
					else if ((result >= 75) && (result <= 100)) {
						switch (m_game->dice(1,5)) {	
				case 1:
				case 2: result = 9;  break;
				case 3:	result = 13; break;
				case 4: result = 14; break;
				case 5:	result = 27; break;
						}
					}
					break;

				case 4:
					if ((result >= 1) && (result < 50)) {
						switch (m_game->dice(1,2)) {
				case 1:	result = 2;  break;
				case 2: result = 10; break;
						}
					}
					else if ((result >= 50) && (result < 80)) {
						switch (m_game->dice(1,2)) {
				case 1: result = 8;  break;
				case 2: result = 11; break;
						}
					}
					else if ((result >= 80) && (result <= 100)) {
						switch (m_game->dice(1,2)) {
				case 1: result = 14; break;
				case 2:	result = 9;  break;
						}
					}
					break;

				case 5:
					if ((result >= 1) && (result < 30)) {
						switch (m_game->dice(1,5)) {
				case 1:
				case 2: 
				case 3:
				case 4: 
				case 5: result = 2;  break;
						}
					}
					else if ((result >= 30) && (result < 60)) {
						switch (m_game->dice(1,2)) {
				case 1: result = 3;  break;
				case 2: result = 4;  break;
						}
					}
					else if ((result >= 60) && (result < 80)) {
						switch (m_game->dice(1,2)) {
				case 1: result = 5;  break;
				case 2: result = 7;  break;
						}
					}
					else if ((result >= 80) && (result < 95)) {
						switch (m_game->dice(1,3)) {
				case 1:
				case 2: result = 8;  break;
				case 3:	result = 11; break;
						}
					}
					else if ((result >= 95) && (result <= 100)) {
						switch (m_game->dice(1,3)) {
				case 1: result = 11; break;
				case 2: result = 14; break;
				case 3: result = 9;  break;
						}
					}
					break;

				case 6: // huntzone3, huntzone4
					if ((result >= 1) && (result < 60)) {
						switch (m_game->dice(1,4)) {
				case 1: result = 5;  break; // Skeleton
				case 2:	result = 6;  break; // Orc-Mage
				case 3: result = 12; break; // Cyclops
				case 4: result = 11; break; // Troll
						}
					}
					else if ((result >= 60) && (result < 90)) {
						switch (m_game->dice(1,5)) {
				case 1:
				case 2: result = 8;  break; // Stone-Golem
				case 3:	result = 11; break; // Troll
				case 4:	result = 12; break; // Cyclops 
				case 5:	result = 43; break; // Tentocle
						}
					}
					else if ((result >= 90) && (result <= 100)) {
						switch (m_game->dice(1,9)) {
				case 1:	result = 26; break;
				case 2:	result = 9;  break;
				case 3: result = 13; break;
				case 4: result = 14; break;
				case 5:	result = 18; break;
				case 6:	result = 28; break;
				case 7: result = 27; break;
				case 8: result = 29; break;
						}
					}
					break;

				case 7: // areuni, elvuni
					if ((result >= 1) && (result < 50)) {
						switch (m_game->dice(1,5)) {
						case 1: result = 3;  break; // Orc
						case 2: result = 6;  break; // Orc-Mage
						case 3: result = 10; break; // Amphis
						case 4: result = 3;  break; // Orc
						case 5: result = 50; break; // Giant-Tree
						}
					}
					else if ((result >= 50) && (result < 85)) {
						switch (m_game->dice(1,4)) {
						case 1: result = 50; break; // Giant-Tree
						case 2: 
						case 3: result = 6;  break; // Orc-Mage
						case 4: result = 12; break; // Troll
						}
					}
					else if ((result >= 85) && (result <= 100)) {
						switch (m_game->dice(1,4)) {
				case 1: result = 12;  break; // Troll
				case 2:
				case 3:
					if (m_game->dice(1,100) < 3) 
						result = 17; // Unicorn
					else result = 12; // Troll
					break;
				case 4: result = 29;  break; // Cannibal-Plant
						}
					}
					break;

				case 8:
					if ((result >= 1) && (result < 70)) {
						switch (m_game->dice(1,2)) {
						case 1:	result = 4;  break;
						case 2: result = 5;  break;
						}
					}
					else if ((result >= 70) && (result < 95)) {
						switch (m_game->dice(1,2)) {
						case 1: result = 8;  break;
						case 2: result = 11; break;
						}
					}
					else if ((result >= 95) && (result <= 100)) {
						result = 14; break;
					}
					break;

				case 9:
					if ((result >= 1) && (result < 70)) {
						switch (m_game->dice(1,2)) {
				case 1:	result = 4;  break;
				case 2: result = 5;  break;
						}
					}
					else if ((result >= 70) && (result < 95)) {
						switch (m_game->dice(1,3)) {
				case 1: result = 8;  break;
				case 2: result = 9;  break;
				case 3: result = 13; break;
						}
					}
					else if ((result >= 95) && (result <= 100)) {
						switch (m_game->dice(1,6)) {
				case 1: 
				case 2: 
				case 3: result = 9;  break;
				case 4: 
				case 5: result = 14; break;
				case 6: result = 15; break;
						}
					}

					if ((m_game->dice(1,3) == 1) && (result != 16)) firm_berserk = true;
					break;

				case 10:
					if ((result >= 1) && (result < 70)) {
						switch (m_game->dice(1,3)) {
				case 1:	result = 9; break;
				case 2: result = 5; break;
				case 3: result = 8; break;
						}
					}
					else if ((result >= 70) && (result < 95)) {
						switch (m_game->dice(1,3)) {
				case 1:
				case 2:	result = 13; break;
				case 3: result = 14; break;
						}
					}
					else if ((result >= 95) && (result <= 100)) {
						switch (m_game->dice(1,3)) {
				case 1:
				case 2: result = 14; break;
				case 3: result = 15; break;
						}
					}
					if ((m_game->dice(1,3) == 1) && (result != 16)) firm_berserk = true;
					break;

				case 11:
					if ((result >= 1) && (result < 30)) {
						switch (m_game->dice(1,5)) {
				case 1:
				case 2: 
				case 3:
				case 4: 
				case 5: result = 2; break;
						}
					}
					else if ((result >= 30) && (result < 60)) {
						switch (m_game->dice(1,2)) {
				case 1: result = 3; break;
				case 2: result = 4; break;
						}
					}
					else if ((result >= 60) && (result < 80)) {
						switch (m_game->dice(1,2)) {
				case 1: result = 5; break;
				case 2: result = 7; break;
						}
					}
					else if ((result >= 80) && (result < 95)) {
						switch (m_game->dice(1,3)) {
				case 1:
				case 2: result = 10;  break;
				case 3:	result = 11; break;
						}
					}
					else if ((result >= 95) && (result <= 100)) {
						switch (m_game->dice(1,3)) {
				case 1: result = 11; break;
				case 2: result = 7; break;
				case 3: result = 8; break;
						}
					}
					break;

				case 12:
					if ((result >= 1) && (result < 50)) {
						switch (m_game->dice(1,3)) {
				case 1:	result = 1 ; break;
				case 2: result = 2 ; break;
				case 3: result = 10; break;
						}
					}
					else if ((result >= 50) && (result < 85)) {
						switch (m_game->dice(1,2)) {
				case 1: result = 5; break;
				case 2: result = 4; break;
						}
					}
					else if ((result >= 85) && (result <= 100)) {
						switch (m_game->dice(1,3)) {
				case 1: result = 8; break;
				case 2: result = 11; break;
				case 3: result = 26; break;
						}
					}
					break;

				case 13:
					if ((result >= 1) && (result < 15)) {
						result = 4;
						firm_berserk = true;
						total_mob = 4 - (m_game->dice(1,2) - 1);
						break;
					}
					else if ((result >= 15) && (result < 40)) {
						result = 14;
						firm_berserk = true;
						total_mob = 4 - (m_game->dice(1,2) - 1);
						break;
					}
					else if ((result >= 40) && (result < 60)) {
						result = 9;
						firm_berserk = true;
						total_mob = 4 - (m_game->dice(1,2) - 1);
						break;
					}						
					else if ((result >= 60) && (result < 75)) {
						result = 13;
						firm_berserk = true;
						total_mob = 4 - (m_game->dice(1,2) - 1);
						break;
					}
					else if ((result >= 75) && (result < 95)) {
						result = 23;
					}
					else if ((result >= 95) && (result <= 100)) {
						result = 22;
					}
					break;

				case 14: // icebound
					if ((result >= 1) && (result < 30)) {
						result = 23; // Dark-Elf
					}
					else if ((result >= 30) && (result < 50)) {
						result = 31; // Ice-Golem
					}
					else if ((result >= 50) && (result < 70)) {
						result = 22; // Beholder
						firm_berserk = true;
						total_mob = 4 - (m_game->dice(1,2) - 1);
					}
					else if ((result >= 70) && (result < 90)) {
						result = 32; // DireBoar
					}
					else if ((result >= 90) && (result <= 100)) {
						result = 33; // Frost
					}
					break;

				case 15:
					if ((result >= 1) && (result < 35)) {
						result = 23; 
						firm_berserk = true;
					}
					else if ((result >= 35) && (result < 50)) {
						result = 22;
						firm_berserk = true;
					}
					else if ((result >= 50) && (result < 80)) {
						result = 15;
					}
					else if ((result >= 80) && (result <= 100)) {
						result = 21;
					}
					break;

				case 16: // 2ndmiddle, huntzone1, huntzone2, 
					if ((result >= 1) && (result < 40)) {
						switch (m_game->dice(1,3)) {
						case 1:	result = 7;  break; // Scorpion
						case 2: result = 2;  break; // Giant-Ant
						case 3: result = 10; break; // Amphis
						}
					}
					else if ((result >= 40) && (result < 50)) {
						result = 30; // Rudolph
					}
					else if ((result >= 50) && (result < 85)) {
						switch (m_game->dice(1,2)) {
						case 1: result = 5;  break; // Skeleton
						case 2: result = 4;  break; // Zombie
						}
					}
					else if ((result >= 85) && (result <= 100)) {
						switch (m_game->dice(1,3)) {
						case 1: result = 8;  break; // Stone-Golem
						case 2: result = 11; break; // Clay-Golem
						case 3: result = 7;  break; // Scorpion
						}
					}
					break;

				case 17:
					if ((result >= 1) && (result < 30)) {
						switch (m_game->dice(1,4)) {
						case 1:	result = 22;  break; // Giant-Frog
						case 2: result = 8;   break; // Stone-Golem
						case 3: result = 24;  break; // Rabbit
						case 4: result = 5;   break;
						}
					}
					else if ((result >= 30) && (result < 40)) {
						result = 30;
					}
					else if ((result >= 40) && (result < 70)) {
						result = 32;
					}
					else if ((result >= 70) && (result < 90)) {
						result = 31;
						if (m_game->dice(1,5) == 1) {
							firm_berserk = true;
						}
					}
					else if ((result >= 90) && (result <= 100)) {
						result = 33;
					}
					break;

				case 18: // druncncity
					if ((result >= 1) && (result < 2)) {
						result = 39; // Tentocle
					}
					else if ((result >= 2) && (result < 12)) {
						result = 44; // ClawTurtle
					}
					else if ((result >= 12) && (result < 50)) {
						result = 48; // Nizie
					}
					else if ((result >= 50) && (result < 80)) {
						result = 45; // Giant-Crayfish
					}
					else if ((result >= 80) && (result < 90)) {
						result = 34; // Stalker
					}			
					else if ((result >= 90) && (result <= 100)) {
						result = 26; // Giant-Frog
					}					
					break;

				case 19:
					if ((result >= 1) && (result < 15)) {
						result = 44;
					}
					else if ((result >= 15) && (result < 25)) {
						result = 46;
					}
					else if ((result >= 25) && (result < 35)) {
						result = 21;
					}
					else if ((result >= 35) && (result < 60)) {
						result = 43;
					}				
					else if ((result >= 60) && (result < 85)) {
						result = 23;
					}		
					else if ((result >= 85) && (result <= 100)) {
						result = 22;
					}
					break;

				case 20:
					if ((result >= 1) && (result < 2)) {
						result = 41;
					}
					else if ((result >= 2) && (result < 3)) {
						result = 40;
					}
					else if ((result >= 3) && (result < 8)) {
						result = 53;
					}
					else if ((result >= 8) && (result < 9)) {
						result = 39;
					}
					else if ((result >= 9) && (result < 20)) {
						result = 21;
					}
					else if ((result >= 20) && (result < 35)) {
						result = 16;
					}
					else if ((result >= 35) && (result < 45)) {
						result = 44;
					}
					else if ((result >= 45) && (result < 55)) {
						result = 45;
					}
					else if ((result >= 55) && (result < 75)) {
						result = 28;
					}
					else if ((result >= 75) && (result < 95)) {
						result = 43;
					}
					else if ((result >= 95) && (result < 100)) {
						result = 22;
					}
					break;

				case 21:
					if ((result >= 1) && (result < 94)) {
						result = 17; // Unicorn
						firm_berserk = true;
					}
					else if ((result >= 94) && (result < 95)) {
						result = 36; // Wyvern
					}
					else if ((result >= 95) && (result < 96)) {
						result = 37; // Fire-Wyvern
					}
					else if ((result >= 96) && (result < 97)) {
						result = 47; // MasterMage-Orc
					}
					else if ((result >= 97) && (result < 98)) {
						result = 35; // Hellclaw
					}				
					else if ((result >= 98) && (result < 99)) {
						result = 49; // Tigerworm
					}		
					else if ((result >= 99) && (result <= 100)) {
						result = 51; // Abaddon
					}
					break;
	
				}			

				pX = 0;
				pY = 0;

//				is_special_event = true;
				if ((m_game->m_is_special_event_time ) && (m_game->dice(1,10) == 3)) is_special_event = true;

				if (is_special_event ) {
					switch (m_game->m_special_event_type) {
					case 1:
						if (m_map_list[i]->m_top_player_sector_x != 0) {
							pX = m_map_list[i]->m_top_player_sector_x*20 +10;
							pY = m_map_list[i]->m_top_player_sector_y*20 +10;

							if (pX < 0) pX = 0;
							if (pY < 0) pY = 0;

							if (m_game->m_is_crusade_mode ) {
								if (strcmp(m_map_list[i]->m_name, "aresden") == 0)
									switch(m_game->dice(1,6)) {
									case 1: result = 20; break;
									case 2: result = 53; break;
									case 3: result = 55; break;
									case 4: result = 57; break;
									case 5: result = 59; break;
									case 6: result = 61; break;
								}
								else if (strcmp(m_map_list[i]->m_name, "elvine") == 0)
									switch(m_game->dice(1,6)) {
									case 1: result = 19; break;
									case 2: result = 52; break;
									case 3: result = 54; break;
									case 4: result = 56; break;
									case 5: result = 58; break;
									case 6: result = 60; break;
								}
							}
							sprintf(G_cTxt, "(!) Mob-Event Map(%s)[%d (%d,%d)]", m_map_list[i]->m_name, result, pX, pY);
						}
						break;

					case 2:
						if (m_game->dice(1,3) == 2) {
							if ((memcmp(m_map_list[i]->m_location_name, "aresden", 7)   == 0) ||
								(memcmp(m_map_list[i]->m_location_name, "middled1n", 9) == 0) ||
								(memcmp(m_map_list[i]->m_location_name, "arefarm", 7) == 0) ||
								(memcmp(m_map_list[i]->m_location_name, "elvfarm", 7) == 0) ||
								(memcmp(m_map_list[i]->m_location_name, "elvine", 6)    == 0)) {
									if (m_game->dice(1,30) == 5) 
										result = 16;
									else result = 5;
								}
							else result = 16;
						}
						else result = 17;

						m_game->m_is_special_event_time = false;
						break;
					}
				}

				// Random monster spawn attribute table
				// {result_id, name, npc_id, prob_sa, kind_sa, mob_dice_sides}
				// mob_dice_sides: >0 = dice(1,N)-1, <0 = fixed abs value, 0 = fixed 0
				struct random_spawn_info { int id; const char* name; int npc_id; int prob_sa; int kind_sa; int mob_dice; };
				static constexpr random_spawn_info spawn_table[] = {
					{ 1, "Slime",            10,  5, 1,  5}, { 2, "Giant-Ant",       16, 10, 2,  5},
					{ 3, "Orc",              14, 15, 1,  5}, { 4, "Zombie",           18, 15, 3,  3},
					{ 5, "Skeleton",         11, 35, 8,  3}, { 6, "Orc-Mage",         14, 30, 7,  3},
					{ 7, "Scorpion",         17, 15, 3,  3}, { 8, "Stone-Golem",      12, 25, 5,  2},
					{ 9, "Cyclops",          13, 35, 8,  2}, {10, "Amphis",            22, 20, 3,  5},
					{11, "Clay-Golem",       23, 20, 5,  3}, {12, "Troll",             28, 25, 3,  5},
					{13, "Orge",             29, 25, 1,  3}, {14, "Hellbound",         27, 25, 8,  2},
					{15, "Liche",            30, 30, 8,  3}, {16, "Demon",             31, 20, 8,  2},
					{17, "Unicorn",          32, 35, 7,  2}, {18, "WereWolf",          33, 25, 1,  5},
					{19, "YB-Aresden",       -1, 15, 1,  2}, {20, "YB-Elvine",        -1, 15, 1,  2},
					{21, "Gagoyle",          52, 20, 8,  5}, {22, "Beholder",          53, 20, 5,  2},
					{23, "Dark-Elf",         54, 20, 3,  2}, {24, "Rabbit",            -1,  5, 1,  4},
					{25, "Cat",              -1, 10, 2,  2}, {26, "Giant-Frog",        57, 10, 2,  3},
					{27, "Mountain-Giant",   58, 25, 1,  3}, {28, "Ettin",             59, 20, 8,  3},
					{29, "Cannibal-Plant",   60, 20, 5,  5}, {30, "Rudolph",           -1, 20, 5,  3},
					{31, "Ice-Golem",        65, 35, 8,  3}, {32, "DireBoar",          62, 20, 5, -1},
					{33, "Frost",            63, 30, 8, -1}, {34, "Stalker",           48, 20, 1, -1},
					{35, "Hellclaw",         49, 20, 1, -1}, {36, "Wyvern",            66, 20, 1, -1},
					{37, "Fire-Wyvern",      -1, 20, 1, -1}, {38, "Barlog",            -1, 20, 1, -1},
					{39, "Tentocle",         -1, 20, 1, -1}, {40, "Centaurus",         -1, 20, 1, -1},
					{41, "Giant-Lizard",     -1, 20, 1, -1}, {42, "Minotaurs",         -1, 20, 1,  3},
					{43, "Tentocle",         -1, 20, 1, -1}, {44, "Claw-Turtle",       -1, 20, 1,  3},
					{45, "Giant-Crayfish",   -1, 20, 1, -1}, {46, "Giant-Plant",       -1, 20, 1,  0},
					{47, "MasterMage-Orc",   -1, 20, 1,  0}, {48, "Nizie",             -1, 20, 1,  0},
					{49, "Tigerworm",        50, 20, 1,  0}, {50, "Giant-Plant",       -1, 20, 1,  0},
					{51, "Abaddon",          -1, 20, 1,  0}, {52, "YW-Aresden",        -1, 15, 1,  0},
					{53, "YW-Elvine",        -1, 15, 1,  0}, {54, "YY-Aresden",        -1, 15, 1,  0},
					{55, "YY-Elvine",        -1, 15, 1,  0}, {56, "XB-Aresden",        -1, 15, 1,  0},
					{57, "XB-Elvine",        -1, 15, 1,  0}, {58, "XW-Aresden",        -1, 15, 1,  0},
					{59, "XW-Elvine",        -1, 15, 1,  0}, {60, "XY-Aresden",        -1, 15, 1,  0},
					{61, "XY-Elvine",        -1, 15, 1,  0},
				};

				std::memset(npc_name, 0, sizeof(npc_name));
				// Default values (Orc)
				strcpy(npc_name, "Orc");
				npc_id = 14;
				prob_sa = 15; kind_sa = 1;

				for (const auto& entry : spawn_table) {
					if (entry.id == result) {
						strcpy(npc_name, entry.name);
						npc_id = entry.npc_id;
						prob_sa = entry.prob_sa;
						kind_sa = entry.kind_sa;
						mob_dice_sides = entry.mob_dice;
						break;
					}
				}

				npc_config_id = m_game->get_npc_config_id_by_name(npc_name);
				char sa = 0;
				if (m_game->dice(1, 100) <= static_cast<uint32_t>(prob_sa)) {
					sa = m_game->get_special_ability(kind_sa);
				}

				char waypoint[11] = {};
				master = (create_entity(npc_config_id, cName_Master, m_map_list[i]->m_name, (rand() % 3), sa,
					MoveType::Random, &pX, &pY, waypoint, 0, 0, -1, false, false, firm_berserk, true, 0) > 0);
				if (!master) {
					m_map_list[i]->set_naming_value_empty(naming_value);
				}
			}

			// Determine follower count from spawn table
			if (mob_dice_sides > 0)
				total_mob = m_game->dice(1, mob_dice_sides) - 1;
			else if (mob_dice_sides < 0)
				total_mob = -mob_dice_sides;

			if (!master) total_mob = 0;

			if (total_mob > 2) {
				switch (result) {
				case 1:  // Slime 
				case 2:  // Giant-Ant
				case 3:  // Orc
				case 4:  // Zombie
				case 5:  // Skeleton
				case 6:  // Orc-Mage
				case 7:  // Scorpion
				case 8:  // Stone-Golem
				case 9:  // Cyclops
				case 10: // Amphis
				case 11: // Clay-Golem
				case 12: // Troll
				case 13: // Orge
				case 14: // Hellbound
				case 15: // Liche
				case 16: // Demon
				case 17: // Unicorn
				case 18: // WereWolf
				case 19:
				case 20:
				case 21:
				case 22:
				case 23:
				case 24:
				case 25:
				case 26:
				case 27:
				case 28:
				case 29:
				case 30:
				case 31:
				case 32:
					if (m_game->dice(1,5) == 1) total_mob = 0;
					break;

				case 33:
				case 34:
				case 35:
				case 36:
				case 37:
				case 38:
				case 39:
				case 40:
				case 41:
				case 42:
				case 44:
				case 45:
				case 46:
				case 47:
				case 48:
				case 49:
					if (m_game->dice(1,5) != 1) total_mob = 0;
					break;
				}
			}

			if (is_special_event ) {
				switch (m_game->m_special_event_type) {
				case 1:
					if ((result != 35) && (result != 36) && (result != 37) && (result != 49) 
						&& (result != 51) && (result != 15) && (result != 16) && (result != 21)) total_mob = 12;
					for (int x = 1; x < MaxClients; x++)
					if ((npc_id != -1) && (m_game->m_client_list[x] != nullptr) && (m_game->m_client_list[x]->m_is_init_complete )) {
						m_game->send_notify_msg(0, x, Notify::SpawnEvent, pX, pY, npc_id, 0, 0, 0);
					}
					break;

				case 2:
					if ((memcmp(m_map_list[i]->m_location_name, "aresden", 7) == 0) ||
						(memcmp(m_map_list[i]->m_location_name, "elvine",  6) == 0) ||
						(memcmp(m_map_list[i]->m_location_name, "elvfarm",  7) == 0) ||
						(memcmp(m_map_list[i]->m_location_name, "arefarm",  7) == 0) ) {
							total_mob = 0;
						}
						break;
				}
				m_game->m_is_special_event_time = false;
			}

			spawn_follower_mobs(i, npc_config_id, total_mob, firm_berserk, prob_sa, kind_sa, cName_Master, pX, pY);
		}

		
	}
}

void CEntityManager::process_spot_spawns(int map_index)
{
    // Extracted from Game.cpp:26370-26518 (MobGenerator spot spawn logic)

    if (map_index < 0 || map_index >= m_max_maps)
        return;

    if (m_map_list[map_index] == nullptr)
        return;

    // Check if map has room for more objects
    if (m_map_list[map_index]->m_maximum_object <= m_map_list[map_index]->m_total_active_object) {
        return;
    }

    int naming_value, pX, pY;
    char cName_Master[11], waypoint[11];
    char sa;

    // Loop through all spot mob generators
    for(int j = 1; j < smap::MaxSpotMobGenerator; j++) {
        if (!m_map_list[map_index]->m_spot_mob_generator[j].is_defined) continue;

        if (m_map_list[map_index]->m_spot_mob_generator[j].max_mobs <=
            m_map_list[map_index]->m_spot_mob_generator[j].cur_mobs) {
            continue;
        }

        if (m_game->dice(1, 3) != 2) continue;

        naming_value = m_map_list[map_index]->get_empty_naming_value();
        if (naming_value == -1) continue;

        int npc_config_id = m_map_list[map_index]->m_spot_mob_generator[j].npc_config_id;
        int prob_sa = m_map_list[map_index]->m_spot_mob_generator[j].prob_sa;
        int kind_sa = m_map_list[map_index]->m_spot_mob_generator[j].kind_sa;

        // Generate entity name
        std::memset(cName_Master, 0, sizeof(cName_Master));
        std::snprintf(cName_Master, sizeof(cName_Master), "XX%d", naming_value);
        cName_Master[0] = '_';
        cName_Master[1] = map_index + 65;

        // Determine special ability
        sa = 0;
        if (prob_sa > 0 && (m_game->dice(1, 100) <= static_cast<uint32_t>(prob_sa))) {
            sa = m_game->get_special_ability(kind_sa);
        }

        // Create entity based on generator type
        int result = -1;
        pX = 0;
        pY = 0;
        std::memset(waypoint, 0, sizeof(waypoint));

        switch (m_map_list[map_index]->m_spot_mob_generator[j].type) {
            case 1: // RECT-based spawn (RANDOMAREA)
                result = create_entity(
                    npc_config_id, cName_Master, m_map_list[map_index]->m_name,
                    (rand() % 3), sa, MoveType::RandomArea,
                    &pX, &pY, waypoint,
                    &m_map_list[map_index]->m_spot_mob_generator[j].rcRect,
                    j, -1, false, false, false, false, 0
                );
                break;

            case 2: // Waypoint-based spawn (RANDOMWAYPOINT)
                for(int k = 0; k < 10; k++) {
                    waypoint[k] = (char)m_map_list[map_index]->m_spot_mob_generator[j].waypoints[k];
                }
                result = create_entity(
                    npc_config_id, cName_Master, m_map_list[map_index]->m_name,
                    (rand() % 3), sa, MoveType::RandomWaypoint,
                    &pX, &pY, waypoint, 0,
                    j, -1, false, false, false, false, 0
                );
                break;
        }

        if (result == -1) {
            m_map_list[map_index]->set_naming_value_empty(naming_value);
        }
        else {
            m_map_list[map_index]->m_spot_mob_generator[j].cur_mobs++;
        }
    }
}

bool CEntityManager::can_spawn_at_spot(int map_index, int spot_index) const
{
    // TODO: Implement spawn condition checking
    // This will be implemented in Phase 2
    return false;
}

uint32_t CEntityManager::generate_entity_guid()
{
    uint32_t guid = m_next_guid++;

    // Handle wraparound (extremely unlikely, but safe)
    if (m_next_guid == 0)
        m_next_guid = 1;

    return guid;
}
