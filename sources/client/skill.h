// skill.h: interface for the CSkill class.
//
//////////////////////////////////////////////////////////////////////

#pragma once
#include <string>

#include <cstring>

class CSkill
{
public:
	inline CSkill()
	{

		m_level = 0;
		m_is_useable = false;
		m_use_method = 0;
	}

	inline virtual ~CSkill()
	{
	}

	std::string m_name;

	int  m_level;
	bool m_is_useable;
	char m_use_method;
};
