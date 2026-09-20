// GameMode.h: Game mode enumeration
//
// Extracted from GameModeManager.h so that IGameScreen and derived classes
// can reference GameMode values without depending on the full GameModeManager.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>

// Game mode constants - matches DEF_GAMEMODE_* values for backwards compatibility
enum class GameMode : int8_t {
    Null = -2,
    Quit = -1,
    Splash = -3,    // Splash screen before loading
    Test = -4,      // Test screen for TextLib validation
    TestPrimitives = -5, // Test screen for primitive drawing
    MainMenu = 0,
    Connecting = 1,
    Loading = 2,
    WaitingInitData = 3,
    MainGame = 4,
    ConnectionLost = 5,
    Msg = 6,
    CreateNewAccount = 7,
    Login = 8,
    QueryForceLogin = 9,
    SelectCharacter = 10,
    CreateNewCharacter = 11,
    WaitingResponse = 12,
    QueryDeleteCharacter = 13,
    LogResMsg = 14,
    ChangePassword = 15,
    // 16 is skipped in original
    VersionNotMatch = 17,
    Introduction = 18,
    Agreement = 19,
    InputKeyCode = 20
};
