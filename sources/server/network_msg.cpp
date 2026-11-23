// network_msg.cpp: implementation of the CMsg class.
//
//////////////////////////////////////////////////////////////////////

#include "common_types.h"
#include "network_msg.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CMsg::CMsg()
{
	m_data  = 0;
	m_size = 0;
}

CMsg::~CMsg()						   
{
	if (m_data != 0) delete m_data;
}

bool CMsg::put(char cFrom, char * data, size_t size, int index, char key)
{
	m_data = new char [size + 1];
	if (m_data == 0) return false;
	std::memset(m_data, 0, size + 1);
	memcpy(m_data, data, size);

	m_size = size;
	m_from  = cFrom;
	m_index = index;
	m_key   = key;

	return true;
}

void CMsg::get(char * pFrom, char * data, size_t* size, int * index, char * key)
{
	*pFrom  = m_from;
	memcpy(data, m_data, m_size);
	*size  = m_size;
	*index = m_index;
	*key   = m_key;
}
