#pragma once

// Agent/automation testing hooks - the MCP agent bridge is the ONLY client testing surface - compiled in debug
// builds only. DEF_CLAUDE_TESTING derives from _DEBUG (d-027); never define it by hand.
#ifdef _DEBUG
#define DEF_CLAUDE_TESTING
#endif

#if defined(DEF_CLAUDE_TESTING) && !defined(_DEBUG)
#error "DEF_CLAUDE_TESTING must never be defined by hand; it derives from _DEBUG (d-027)"
#endif
