// MapData.h: interface for the CMapData class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdio>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "CommonTypes.h"
#include "Tile.h"
#include "Item/ItemInstanceData.h"
#include "ActionID.h"
#include "Game.h"
#include "TileSpr.h"
#include "ActionID_Client.h"
#include "ObjectIDRange.h"
#include <string>

using hb::shared::direction::direction;

namespace hb::client::config
{
constexpr int MapDataSizeX = 60;
constexpr int MapDataSizeY = 55;
} // namespace hb::client::config

class CMapData
{
public:
	CMapData(class CGame * game);
	virtual ~CMapData();
	void init();
	void open_map_data_file(char * fn);
	void get_owner_status_by_object_id(uint16_t object_id, char * owner_type, direction * dir, hb::shared::entity::PlayerAppearance * appearance, hb::shared::entity::PlayerStatus * status, std::string& name);
	void clear_dead_chat_msg(short sX, short sY);
	void clear_chat_msg(short sX, short sY);
	void shift_map_data(direction dir);
	void decode_map_info(char * header);
	bool set_chat_msg_owner(uint16_t object_id, short sX, short sY, int index);
	bool set_dead_owner(uint16_t object_id, short sX, short sY, short type, direction dir, const hb::shared::entity::PlayerAppearance& appearance, const hb::shared::entity::PlayerStatus& status, std::string& name, short npcConfigId = -1);
	bool set_owner(uint16_t object_id, int sX, int sY, int type, direction dir, const hb::shared::entity::PlayerAppearance& appearance, const hb::shared::entity::PlayerStatus& status, std::string& name, short action, short v1, short v2, short v3, int pre_loc = 0, int frame = 0, short npcConfigId = -1);
	bool get_owner(short sX, short sY, std::string& name, short * owner_type, hb::shared::entity::PlayerStatus * owner_status, uint16_t * object_id, short* npc_config_id = nullptr);
	bool set_dynamic_object(short sX, short sY, uint16_t id, short type, bool is_event);
	bool is_teleport_loc(short sX, short sY);
	bool get_is_locatable(short sX, short sY);
	bool set_item(short sX, short sY, short item_id, const hb::shared::item::item_instance_data& data, bool drop_effect = true);
	int  object_frame_counter(const std::string& player_name, short view_point_x, short view_point_y);

	class CTile m_data[hb::client::config::MapDataSizeX][hb::client::config::MapDataSizeY];
	class CTile m_tmp_data[hb::client::config::MapDataSizeX][hb::client::config::MapDataSizeY];
	class CTileSpr m_tile[752][752];
	class CGame * m_game;

	struct {
		short m_sMaxFrame;
		short m_sFrameTime;
	} m_stFrame[hb::client::config::TotalCharacters][hb::client::config::TotalAction];
	uint32_t m_frame_time;
	uint32_t m_dynamic_object_frame_time;
	uint32_t m_frame_check_time;
	int m_object_id_cache_loc_x[hb::shared::object_id::NpcMax];
	int m_object_id_cache_loc_y[hb::shared::object_id::NpcMax];
	uint32_t m_frame_adjust_time;
	short m_map_size_x, m_map_size_y;
	short m_rect_x, m_rect_y;
	short m_pivot_x, m_pivot_y;
};
