// renderer_factory.h: Factory functions for creating renderer and window instances
//
// This header provides a way to create renderer/window instances without
// coupling the client to a specific implementation.
//////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer.h"
#include "window.h"
#include "sprite_factory.h"

namespace hb::shared::render {

// Available renderer backends
enum class RendererType
{
    SFML,
};

// Creates a renderer instance
// The actual implementation is provided by the linked renderer library
IRenderer* CreateRenderer();

// Destroys a renderer instance created by CreateRenderer
void DestroyRenderer(IRenderer* renderer);

// Creates a window instance appropriate for the current renderer type
// Note: Named CreateGameWindow to avoid conflict with Win32 CreateWindow macro
IWindow* CreateGameWindow();

// Destroys a window instance
void DestroyGameWindow(IWindow* window);

// Creates a sprite factory instance for the current renderer
// The renderer must be initialized first
hb::shared::sprite::ISpriteFactory* CreateSpriteFactory(IRenderer* renderer);

// Destroys a sprite factory instance
void DestroySpriteFactory(hb::shared::sprite::ISpriteFactory* factory);

// Renderer management class - provides static access to the active renderer
class Renderer
{
public:
    // Set the renderer type and create the renderer
    // Returns true on success
    static bool Set(RendererType type);

    // get the current renderer instance
    static IRenderer* get();

    // Destroy the current renderer
    static void Destroy();

    // get the underlying renderer pointer
    static void* GetNative();

    // get the current renderer type
    static RendererType GetType();

private:
    static IRenderer* s_renderer;
    static RendererType s_type;
};

// Window management class - provides static access to the main window
class Window
{
public:
    // Allocate the window implementation (stages default params, no OS window yet)
    // Configure via get()->set_title/set_size/etc., then call realize().
    static IWindow* create();

    // Create the OS window from staged params + initialize input system
    static bool realize();

    // get the current window instance
    static IWindow* get();

    // Destroy the current window
    static void destroy();

    // get the window handle (HWND on Windows)
    static hb::shared::types::NativeWindowHandle get_handle();

    // Convenience: Check if window is open and active
    static bool is_active();

    // Request window close
    static void close();

    // Show an error message box (platform-appropriate)
    static void show_error(const char* title, const char* message);

private:
    static IWindow* s_window;
};

} // namespace hb::shared::render
