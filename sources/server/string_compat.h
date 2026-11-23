#pragma once
#include <cstring>

#ifdef _WIN32
	#define hb_stricmp  _stricmp
	#define hb_strnicmp _strnicmp
#else
	#include <strings.h>
	#define hb_stricmp  strcasecmp
	#define hb_strnicmp strncasecmp
	#define strtok_s    strtok_r
#endif
