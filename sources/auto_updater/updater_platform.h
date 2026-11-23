// updater_platform.h: Centralized OS-specific header management for AutoUpdater.
//
// Include this instead of <windows.h> directly. On Windows, pulls in
// platform_headers.h (WIN32_LEAN_AND_MEAN, NOMINMAX, macro undefs) plus
// the additional Windows headers needed by the updater (winsock2, commctrl, process).
// On Linux, includes X11 and POSIX headers.
//
// .cpp files in AutoUpdater should include this; .h files should NOT.
//////////////////////////////////////////////////////////////////////

#pragma once

#ifdef _WIN32

// Use the shared platform_headers.h for consistent Windows setup
// (WIN32_LEAN_AND_MEAN, NOMINMAX, winsock2 before windows.h, macro undefs)
#include "platform_headers.h"

// Additional Windows headers needed by the updater
#include <ws2tcpip.h>
#include <commctrl.h>
#include <process.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "ws2_32.lib")

#else // Linux / POSIX

#include <unistd.h>
#include <limits.h>

#endif
