#include "native_types.h"
#include "game.h"
#include "application.h"
#include "renderer_factory.h"
#include "resource.h"
#include "auto_updater.h"
#include <memory>
#include <cstdlib>
#include <ctime>
#include <string>
#include <string_view>
#include "benchmark.h"

// --------------------------------------------------------------
// Platform-independent core
// cmd_line holds every argument after the program name, space-separated (WinMain's lpCmdLine form)
int GameMain(hb::shared::types::NativeInstance native_instance, int icon_resource_id, [[maybe_unused]] std::string_view cmd_line)
{
#ifdef _DEBUG
	DebugConsole::allocate();
#endif

#ifndef _DEBUG
	// Check for updates before any SFML initialization
	auto update_status = hb::updater::check_for_updates();
	if (update_status == hb::updater::update_result::restart_required)
		hb::updater::relaunch(); // Re-exec; next launch swaps .update → exe
#endif

	using namespace hb::shared::render;
    srand(static_cast<unsigned>(time(0)));

    // Create game application
    auto game = application::create<CGame>(native_instance, icon_resource_id);

    // Create and attach window (not yet realized — just allocated)
    auto* window = Window::create();
    if (!window)
    {
        Window::show_error("ERROR", "Failed to allocate window!");
        return 1;
    }
    game->attach_window(window);

    // initialize (pre-realize: loads config, stages window params, loads game data)
    if (!game->initialize())
    {
        return 1;
    }

#ifdef DEF_CLAUDE_TESTING
    // Parsed after initialize() so a bad value reaches the log; the bridge starts inside run()
    game->m_agent_bridge_config = hb::client::parse_agent_bridge_flags(cmd_line);
#endif

    // Run the main loop (realizes window, creates renderer, loops, shuts down)
    return game->run();
}

// --------------------------------------------------------------
// Platform-specific entry points

// Joins argv[1..argc) with single spaces, the form WinMain's lpCmdLine already has
// Argument boundaries are lost here, so a later flag must not take a value that contains spaces
static std::string join_arguments(int argc, char* argv[])
{
	std::string joined;
	for (int i = 1; i < argc; ++i)
	{
		if (i > 1)
		{
			joined += ' ';
		}
		joined += argv[i];
	}
	return joined;
}

#ifdef _WIN32
#include "platform_headers.h" // included here for WinMain types; unconditional include is in native_types.h

// GPU Selection - Force discrete GPU on hybrid systems
extern "C"
{
    __declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

static void InitDpiAwareness()
{
    if (!SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
        SetProcessDPIAware();
    }
}

int APIENTRY WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
    //InitDpiAwareness();
    return GameMain(hInstance, IDI_ICON1, lpCmdLine ? lpCmdLine : "");
}

int main(int argc, char* argv[])
{
    //InitDpiAwareness();
    return GameMain(GetModuleHandle(nullptr), IDI_ICON1, join_arguments(argc, argv));
}
#else
int main(int argc, char* argv[])
{
    return GameMain(nullptr, 0, join_arguments(argc, argv));
}
#endif
