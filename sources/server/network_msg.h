// Msg.h: interface for the CMsg class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "common_types.h"

namespace hb::server::msg
{
namespace Source
{
	enum : int
	{
		Client    = 1,
		LogServer = 2,
		Bot       = 4,
	};
}
} // namespace hb::server::msg

class CMsg  								 
{
public:
	void get(char * pFrom, char * data, size_t* size, int * index, char * key);
	bool put(char cFrom, char * data, size_t size, int index, char key);
	CMsg();
	virtual ~CMsg();

	char   m_from;

	char * m_data;
	size_t  m_size;

	int    m_index;
	char   m_key;   // v1.4
};
