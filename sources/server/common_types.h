// common_types.h: Cross-platform type definitions
//
// This header replaces Windows-specific typedefs (DWORD, WORD, BYTE) with
// standard C++ types (uint32_t, uint16_t, uint8_t) for portability.
//
// Windows API types (HWND, HANDLE, HRESULT) are preserved for platform-specific code.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <cstring>
#include <chrono>
#include <cstdio>
#include "game_geometry.h"
#include "net_constants.h"

// ============================================================================
// Windows API types (platform-specific)
// ============================================================================
#ifdef _WIN32
    // Windows types are included by platform-specific headers as needed.
#endif

// ============================================================================
// Memory Operations
// ============================================================================
// Windows ZeroMemory() should be replaced with std::memset().
// Old: ZeroMemory(&struct, sizeof(struct));
// New: std::memset(&struct, 0, sizeof(struct));

// ============================================================================
// Timing System
// ============================================================================
// GameClock: Cross-platform timing system
// Replaces Windows timeGetTime() with std::chrono for cross-platform compatibility.
class GameClock {
private:
    static std::chrono::steady_clock::time_point s_startTime;
    static bool s_initialized;

public:
    static void initialize() {
        if (!s_initialized) {
            s_startTime = std::chrono::steady_clock::now();
            s_initialized = true;
        }
    }

    static uint32_t GetTimeMS() {
        if (!s_initialized) initialize();

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - s_startTime);
        return static_cast<uint32_t>(elapsed.count());
    }

};
