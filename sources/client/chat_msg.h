// Msg.h: interface for the CMsg class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include <cstring>
#include "common_types.h"

class CMsg
{
public:
	inline CMsg(int type, const char * pMsg, uint32_t time)
	{
		m_cType = type;

		m_pMsg = 0;
		const size_t msgLen = (pMsg != 0) ? strlen(pMsg) : 0;
		m_pMsg = new char [msgLen + 1];
		std::memset(m_pMsg, 0, msgLen + 1);
		if (pMsg != 0) {
			std::memcpy(m_pMsg, pMsg, msgLen);
		}
		m_time = time;
		m_object_id = -1;
	}

	CMsg(const CMsg&) = delete;
	CMsg& operator=(const CMsg&) = delete;

	inline virtual ~CMsg()
	{
		if (m_pMsg != 0) delete[] m_pMsg;
	}

	char   m_cType;
	char * m_pMsg;
	short  m_x, m_y;
	uint32_t  m_time;

	int    m_object_id;

};
