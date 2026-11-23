// renderer_factory.cpp: SFML implementation of renderer factory functions
//
// Part of SFMLEngine static library
// Provides SFML-specific implementations of the factory functions
//////////////////////////////////////////////////////////////////////

#include "renderer_factory.h"
#include "sprite_factory.h"
#include "sfml_renderer.h"
#include "sfml_window.h"
#include "sfml_sprite_factory.h"
#include "sfml_text_renderer.h"
#include "sfml_bitmap_font.h"
#include "sfml_input.h"
#include "input.h"

#ifdef _WIN32
#include <imm.h>
#pragma comment(lib, "imm32.lib")
#endif


namespace hb::shared::render {

// Static member initialization (matches renderer_factory.h class declarations)
IRenderer* Renderer::s_renderer = nullptr;
RendererType Renderer::s_type = RendererType::SFML;
IWindow* Window::s_window = nullptr;

// Local static for sprite factory
static SFMLSpriteFactory* s_pSpriteFactory = nullptr;

// Local statics for text rendering
static hb::shared::text::SFMLTextRenderer* s_pTextRenderer = nullptr;
static hb::shared::text::SFMLBitmapFontFactory* s_pBitmapFontFactory = nullptr;


// Factory functions implementation
IRenderer* CreateRenderer()
{
    return new SFMLRenderer();
}

void DestroyRenderer(IRenderer* renderer)
{
    delete renderer;
}

IWindow* CreateGameWindow()
{
    // Disable IME for the process. SFML's shared GL context creates a hidden
    // window internally. When that window is destroyed during cleanup,
    // msctf.dll (Windows IME) hooks fire and access freed thread-local state,
    // causing an access violation. Disabling IME prevents those hooks.
#ifdef _WIN32
    static bool ime_disabled = [] { ImmDisableIME(static_cast<DWORD>(-1)); return true; }();
    (void)ime_disabled;
#endif
    return new SFMLWindow();
}

void DestroyGameWindow(IWindow* window)
{
    delete window;
}

hb::shared::sprite::ISpriteFactory* CreateSpriteFactory(IRenderer* renderer)
{
    if (!renderer)
        return nullptr;

    // Create SFML sprite factory with the renderer - uses PNG sprites
    SFMLRenderer* sfml_renderer = static_cast<SFMLRenderer*>(renderer);
    SFMLSpriteFactory* factory = new SFMLSpriteFactory(sfml_renderer);
    factory->SetSpritePath("sprites");
    return factory;
}

void DestroySpriteFactory(hb::shared::sprite::ISpriteFactory* factory)
{
    delete factory;
}

// Renderer class static methods
bool Renderer::Set(RendererType type)
{
    // Clean up existing renderer
    if (s_renderer)
    {
        Destroy();
    }

    s_type = type;

    if (type == RendererType::SFML)
    {
        s_renderer = CreateRenderer();
        if (s_renderer)
        {
            SFMLRenderer* sfmlRenderer = static_cast<SFMLRenderer*>(s_renderer);

            // Create and set sprite factory - SFML uses PNG sprites
            s_pSpriteFactory = new SFMLSpriteFactory(sfmlRenderer);
            s_pSpriteFactory->SetSpritePath("sprites");
            hb::shared::sprite::Sprites::set_factory(s_pSpriteFactory);

            // Create bitmap font factory
            s_pBitmapFontFactory = new hb::shared::text::SFMLBitmapFontFactory();
            hb::shared::text::SetBitmapFontFactory(s_pBitmapFontFactory);

            // If window already exists (created before renderer), link them now
            IWindow* window = Window::get();
            if (window)
            {
                SFMLWindow* sfmlWindow = static_cast<SFMLWindow*>(window);
                sfmlRenderer->SetRenderWindow(sfmlWindow->GetRenderWindow());

                // Create text renderer; it draws to the renderer's current target (loads the bundled font; check IsFontLoaded)
                s_pTextRenderer = new hb::shared::text::SFMLTextRenderer(sfmlRenderer);
                hb::shared::text::SetTextRenderer(s_pTextRenderer);
            }
        }
        return s_renderer != nullptr;
    }

    return false;
}

IRenderer* Renderer::get()
{
    return s_renderer;
}

void Renderer::Destroy()
{
    // Destroy text renderer
    if (s_pTextRenderer)
    {
        hb::shared::text::SetTextRenderer(nullptr);
        delete s_pTextRenderer;
        s_pTextRenderer = nullptr;
    }

    // Destroy bitmap font factory
    if (s_pBitmapFontFactory)
    {
        hb::shared::text::SetBitmapFontFactory(nullptr);
        delete s_pBitmapFontFactory;
        s_pBitmapFontFactory = nullptr;
    }

    // Destroy sprite factory
    if (s_pSpriteFactory)
    {
        hb::shared::sprite::Sprites::set_factory(nullptr);
        delete s_pSpriteFactory;
        s_pSpriteFactory = nullptr;
    }

    if (s_renderer)
    {
        DestroyRenderer(s_renderer);
        s_renderer = nullptr;
    }
}

void* Renderer::GetNative()
{
    return s_renderer;
}

RendererType Renderer::GetType()
{
    return s_type;
}

// Window class static methods
IWindow* Window::create()
{
    if (s_window)
    {
        destroy();
    }

    s_window = CreateGameWindow();
    // Returns the allocated-but-not-realized window.
    // Caller configures via set_title/set_size/etc., then calls realize().
    return s_window;
}

bool Window::realize()
{
    if (!s_window)
        return false;

    // Create OS window from staged params
    if (!s_window->realize())
    {
        delete s_window;
        s_window = nullptr;
        return false;
    }

    // Create input system (needs realized OS window)
    hb::shared::input::create();
    if (hb::shared::input::get())
    {
        SFMLInput* input = static_cast<SFMLInput*>(hb::shared::input::get());
        input->Initialize(s_window->get_handle());
        input->SetRenderWindow(static_cast<SFMLWindow*>(s_window)->GetRenderWindow());
    }

    // Link the SFML window's render window to the renderer (if renderer exists already)
    IRenderer* renderer = Renderer::get();
    if (renderer)
    {
        SFMLWindow* sfmlWindow = static_cast<SFMLWindow*>(s_window);
        SFMLRenderer* sfmlRenderer = static_cast<SFMLRenderer*>(renderer);
        sfmlRenderer->SetRenderWindow(sfmlWindow->GetRenderWindow());

        // Create text renderer now that the renderer has its targets
        if (!s_pTextRenderer)
        {
            s_pTextRenderer = new hb::shared::text::SFMLTextRenderer(sfmlRenderer);
            hb::shared::text::SetTextRenderer(s_pTextRenderer);
        }
    }

    return true;
}

IWindow* Window::get()
{
    return s_window;
}

void Window::destroy()
{
    // Destroy input system first
    hb::shared::input::destroy();

    if (s_window)
    {
        // Unlink from renderer
        IRenderer* renderer = Renderer::get();
        if (renderer)
        {
            SFMLRenderer* sfmlRenderer = static_cast<SFMLRenderer*>(renderer);
            sfmlRenderer->SetRenderWindow(nullptr);
        }

        DestroyGameWindow(s_window);
        s_window = nullptr;
    }
}

hb::shared::types::NativeWindowHandle Window::get_handle()
{
    return s_window ? s_window->get_handle() : hb::shared::types::NativeWindowHandle{};
}

bool Window::is_active()
{
    return s_window ? s_window->is_active() : false;
}

void Window::close()
{
    if (s_window)
    {
        s_window->close();
    }
}

void Window::set_size(int width, int height, bool center)
{
    if (s_window)
    {
        s_window->set_size(width, height, center);
    }
}

void Window::set_borderless(bool borderless)
{
    if (s_window)
    {
        s_window->set_borderless(borderless);
    }
}

void Window::show_error(const char* title, const char* message)
{
    if (s_window)
    {
        s_window->show_message_box(title, message);
    }
    else
    {
#ifdef _WIN32
        MessageBox(nullptr, message, title, MB_ICONEXCLAMATION | MB_OK);
#else
        fprintf(stderr, "%s: %s\n", title, message);
#endif
    }
}

// Sprite factory static storage (defined in sprite_factory.cpp in Shared)
// The implementation uses hb::shared::sprite::Sprites::set_factory/get_factory

} // namespace hb::shared::render
