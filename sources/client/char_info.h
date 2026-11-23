// char_info.h: interface for the CCharInfo class.
//
//////////////////////////////////////////////////////////////////////

#pragma once
#include <string>

#include "appearance.h"
#include <cstdint>

class CCharInfo
{
public:
	inline CCharInfo() = default;

	inline virtual ~CCharInfo()
	{
	}

	std::string m_name;
	std::string m_map_name;
	short m_skin_color = 0, m_sex = 0;
	hb::shared::entity::PlayerAppearance m_appearance{};
	short	m_str = 0, m_vit = 0, m_dex = 0, m_int = 0, m_mag = 0, m_chr = 0;
	int	m_level = 0;
	uint32_t   m_exp = 0;
	int   m_year = 0, m_month = 0, m_day = 0, m_hour = 0, m_minute = 0;
};
