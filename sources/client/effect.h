// effect.h: interface for the CEffect class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "common_types.h"
#include "effect_type.h"
#include "direction_helpers.h"

using hb::shared::direction::direction;

class CEffect
{
public:
	inline CEffect() = default;

	inline virtual ~CEffect()
	{

	}

	EffectType m_type = EffectType::INVALID;
	char  m_frame = -1, m_max_frame = 0;
	direction m_dir = direction{};
	uint32_t m_time = 0, m_frame_time = 0;
	int   m_x = 0, m_y = 0, m_dest_x = 0, m_dest_y = 0;
	int   m_move_x = 0, m_move_y = 0, m_move_x2 = 0, m_move_y2 = 0, m_move_x3 = 0, m_move_y3 = 0;
	int   m_error = 0;
	int   m_render_x = 0, m_render_y = 0;
	int   m_value1 = 0;
};
