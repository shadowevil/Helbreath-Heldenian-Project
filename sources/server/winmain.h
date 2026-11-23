// winmain.h

#pragma once

#include "common_types.h"

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>

//void OnBotAccept();
bool InitApplication(HINSTANCE hInstance);
bool InitInstance(HINSTANCE hInstance, int nCmdShow);
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
void OnAccept();
void OnAcceptLogin();
void OnKeyUp(WPARAM wParam, LPARAM lParam);
WPARAM  EventLoop();
void OnDestroy();
void initialize();
void CALLBACK _TimerFunc(UINT wID, UINT wUser, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2);
MMRESULT _StartTimer(uint32_t dwTime);
void _StopTimer(MMRESULT timerid);

// Extern declarations for async networking globals
namespace hb::shared::net { class IOServicePool; }
extern hb::shared::net::IOServicePool* G_pIOPool;
#endif
