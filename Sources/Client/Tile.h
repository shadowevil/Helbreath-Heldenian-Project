// Tile.h: interface for the CTile class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "EntityMotion.h"
#include "AnimationState.h"
#include <string>
#include "Appearance.h"
#include "PlayerStatusData.h"
#include "Item/ItemInstanceData.h"
#include <cstdint>

class CTile
{
public:
	inline void clear()
	{
		m_object_id     = 0;
		m_dead_object_id = 0;

		m_owner_type = 0;
		m_npc_config_id = -1;
		m_owner_name.clear();

		m_dead_owner_type = 0;
		m_dead_npc_config_id = -1;
		m_dead_owner_name.clear();

		m_dead_owner_frame = -1;
		m_dead_owner_time = 0;

		m_item_id = 0;
		m_item.clear();

		m_dynamic_object_type  = 0;
		m_dynamic_object_frame = 0;

		m_chat_msg     = 0;
		m_dead_chat_msg = 0;

		m_status.clear();
		m_deadStatus.clear();

		m_v1 = 0;
		m_v2 = 0;
		m_v3 = 0;

		m_effect_type  = 0;
		m_effect_frame = 0;
		m_effect_total_frame = 0;
		m_effect_time = 0;

		m_appearance.clear();
		m_dead_appearance.clear();

		m_animation.reset();
		m_motion.reset();
	}

	inline CTile()
	{
		m_owner_type = 0;
		m_npc_config_id = -1;
		m_dead_owner_type = 0;
		m_dead_npc_config_id = -1;
		m_dead_owner_frame     = -1;

		m_dynamic_object_type  = 0;
		m_dynamic_object_frame = 0;

		m_chat_msg       = 0;
		m_dead_chat_msg   = 0;

		m_object_id = 0;

		m_effect_type  = 0;
		m_effect_frame = 0;
		m_effect_total_frame = 0;
		m_effect_time = 0;
	}

	inline ~CTile()
	{
	}
	uint32_t m_effect_time;
	uint32_t m_dead_owner_time;
	uint32_t m_dynamic_object_time;

	int   m_chat_msg;
	int   m_effect_type;
	int   m_effect_frame, m_effect_total_frame;
	int   m_dead_chat_msg;

	uint16_t  m_dead_object_id;
	uint16_t  m_object_id;

	short m_owner_type;							// +B2C
	short m_npc_config_id;						// NPC config index (for name lookup, -1 if player)
	hb::shared::entity::PlayerStatus m_status;

	short m_dead_owner_type;						// +B3C
	short m_dead_npc_config_id;					// Dead NPC config index

	hb::shared::entity::PlayerStatus m_deadStatus;
	short m_v1;
	short m_v2;
	short m_v3;								// +B50
	short m_dynamic_object_type;

	int16_t m_item_id = 0;                       // Item ID (lookup key — separate from instance data)
	hb::shared::item::item_instance_data m_item;

	char  m_dead_owner_frame;
	direction m_dead_dir;

	char  m_dynamic_object_frame;
	char  m_dynamic_object_data_1, m_dynamic_object_data_2, m_dynamic_object_data_3, m_dynamic_object_data_4;
	std::string m_owner_name;
	std::string m_dead_owner_name;

	// Unpacked appearance data (extracted from m_sAppr1-4 at reception)
	hb::shared::entity::PlayerAppearance m_appearance;
	hb::shared::entity::PlayerAppearance m_dead_appearance;

	// Animation state (replaces m_cOwnerAction, m_cOwnerFrame, m_dir, m_dwOwnerTime)
	animation_state m_animation;

	// Smooth movement interpolation
	EntityMotion m_motion;
};
