// sfml_renderer.cpp: SFML renderer implementation
//
// Part of SFMLEngine static library
//////////////////////////////////////////////////////////////////////

#include "sfml_renderer.h"
#include "sfml_window.h"
#include "renderer_factory.h"
#include <SFML/Graphics/Image.hpp>
#include "text_renderer.h"
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/ConvexShape.hpp>
#include <SFML/OpenGL.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <exception>
#include <thread>

// Fragment shader: adds a flat color offset to each pixel before output.
// Matches DDraw PutTransSpriteRGB: dest += clamp(src + (r, g, b))
static constexpr const char* ADDITIVE_OFFSET_FRAG_SRC = R"glsl(
uniform sampler2D texture;
uniform vec3 colorOffset;

void main()
{
    vec4 pixel = texture2D(texture, gl_TexCoord[0].xy);
    if (pixel.a < 0.01) discard;
    pixel.rgb = clamp(pixel.rgb + colorOffset, 0.0, 1.0);
    pixel.a *= gl_Color.a;
    gl_FragColor = pixel;
}
)glsl";

#include "platform_headers.h"

#ifdef _WIN32
#include <dxgi.h>
#pragma comment(lib, "dxgi.lib")
#endif

SFMLRenderer::SFMLRenderer()
    : m_render_window(nullptr)
    , m_texturesCreated(false)
    , m_width(640)  // Default, updated in CreateRenderTextures
    , m_height(480) // Default, updated in CreateRenderTextures
    , m_fullscreen(false)
    , m_fullscreen_stretch(false)
    , m_upscale_smoothing(true)
    , m_fps_limit(0)
    , m_vsync(false)
    , m_skip_frame(false)
    , m_lastPresentTime(std::chrono::steady_clock::now())
    , m_targetFrameDuration(std::chrono::steady_clock::duration::zero())
    , m_fps(0)
    , m_framesThisSecond(0)
    , m_delta_time(0.0)
    , m_fps_accumulator(0.0)
    , m_lastPresentedFrameTime(std::chrono::steady_clock::now())
    , m_ambient_light_level(1)
{
    m_clipArea = hb::shared::geometry::GameRectangle(0, 0, m_width, m_height);
}

SFMLRenderer::~SFMLRenderer()
{
    shutdown();
}

#ifdef _WIN32
// Enumerate GPUs and log their VRAM information
// Returns the index of the GPU with the most dedicated VRAM
static void LogGPUInfo()
{
    IDXGIFactory* factory = nullptr;
    HRESULT hr = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&factory);
    if (FAILED(hr) || !factory)
    {
        printf("[GPU] Failed to create DXGI factory\n");
        return;
    }

    printf("[GPU] Enumerating available graphics adapters:\n");

    IDXGIAdapter* adapter = nullptr;
    UINT adapterIndex = 0;
    UINT bestAdapterIndex = 0;
    SIZE_T maxDedicatedVRAM = 0;

    while (factory->EnumAdapters(adapterIndex, &adapter) != DXGI_ERROR_NOT_FOUND)
    {
        DXGI_ADAPTER_DESC desc;
        if (SUCCEEDED(adapter->GetDesc(&desc)))
        {
            // Convert wide string to narrow for printf
            char adapterName[128];
            WideCharToMultiByte(CP_ACP, 0, desc.Description, -1, adapterName, sizeof(adapterName), nullptr, nullptr);

            SIZE_T dedicatedVRAM_MB = desc.DedicatedVideoMemory / (1024 * 1024);
            SIZE_T sharedVRAM_MB = desc.SharedSystemMemory / (1024 * 1024);

            printf("[GPU]   Adapter %u: %s\n", adapterIndex, adapterName);
            printf("[GPU]     Dedicated VRAM: %zu MB\n", dedicatedVRAM_MB);
            printf("[GPU]     Shared Memory:  %zu MB\n", sharedVRAM_MB);

            if (desc.DedicatedVideoMemory > maxDedicatedVRAM)
            {
                maxDedicatedVRAM = desc.DedicatedVideoMemory;
                bestAdapterIndex = adapterIndex;
            }
        }
        adapter->Release();
        adapterIndex++;
    }

    if (adapterIndex > 0 && maxDedicatedVRAM > 0)
    {
        printf("[GPU] Adapter with most VRAM: Adapter %u (%zu MB)\n", bestAdapterIndex, maxDedicatedVRAM / (1024 * 1024));
        printf("[GPU] Note: NvOptimusEnablement and AmdPowerXpressRequestHighPerformance exports are set to prefer discrete GPU\n");
    }

    factory->Release();
}
#endif

bool SFMLRenderer::init(hb::shared::types::NativeWindowHandle hWnd)
{
    // Log GPU information for debugging/verification
#ifdef _WIN32
    LogGPUInfo();
#endif

    return true;
}

bool SFMLRenderer::CreateRenderTextures()
{
    if (m_texturesCreated)
        return true;

    // Update dimensions from hb::shared::render::ResolutionConfig (initialized before renderer creation)
    m_width = RENDER_LOGICAL_WIDTH();
    m_height = RENDER_LOGICAL_HEIGHT();
    m_clipArea = hb::shared::geometry::GameRectangle(0, 0, m_width, m_height);

    // Create back buffer render texture
    if (!m_backBuffer.resize({static_cast<unsigned int>(m_width), static_cast<unsigned int>(m_height)}))
    {
        printf("[ERROR] SFMLRenderer::CreateRenderTextures - Failed to create back buffer\n");
        return false;
    }

    // Disable texture smoothing for pixel-perfect scaling (matches DDraw sharpness)
    // This uses nearest-neighbor filtering instead of bilinear
    m_backBuffer.setSmooth(false);

    // Disable texture repeating to prevent edge artifacts
    m_backBuffer.setRepeated(false);

    // Set explicit views to ensure 1:1 pixel mapping and prevent edge artifacts
    // The view must match exactly the surface dimensions
    sf::View backBufferView(sf::FloatRect({0.f, 0.f}, {static_cast<float>(m_width), static_cast<float>(m_height)}));
    m_backBuffer.setView(backBufferView);

    // Clear buffer - use transparent for proper alpha compositing
    m_backBuffer.clear(sf::Color::Transparent);
    m_backBuffer.display();

    // Load additive offset shader for PutTransSpriteRGB emulation
    m_additive_offset_shader_loaded = m_additive_offset_shader.loadFromMemory(
        ADDITIVE_OFFSET_FRAG_SRC, sf::Shader::Type::Fragment);
    if (m_additive_offset_shader_loaded)
    {
        m_additive_offset_shader.setUniform("texture", sf::Shader::CurrentTexture);
        printf("[Shader] Additive offset shader loaded successfully\n");
    }
    else
    {
        printf("[Shader] WARNING: Failed to load additive offset shader (glare will degrade to plain additive)\n");
    }

    m_texturesCreated = true;
    return true;
}

const sf::Shader* SFMLRenderer::get_additive_offset_shader() const
{
    return m_additive_offset_shader_loaded ? &m_additive_offset_shader : nullptr;
}

void SFMLRenderer::SetRenderWindow(sf::RenderWindow* window)
{
    m_render_window = window;

    // Create render textures now that we have an OpenGL context
    if (window && !m_texturesCreated)
    {
        CreateRenderTextures();
    }
}

void SFMLRenderer::shutdown()
{
}

void SFMLRenderer::set_fullscreen(bool fullscreen)
{
    m_fullscreen = fullscreen;
}

bool SFMLRenderer::is_fullscreen() const
{
    return m_fullscreen;
}

void SFMLRenderer::set_fullscreen_stretch(bool stretch)
{
    m_fullscreen_stretch = stretch;
}

bool SFMLRenderer::is_fullscreen_stretch() const
{
    return m_fullscreen_stretch;
}

void SFMLRenderer::set_upscale_smoothing(bool enabled)
{
    m_upscale_smoothing = enabled;
}

bool SFMLRenderer::is_upscale_smoothing() const
{
    return m_upscale_smoothing;
}

void SFMLRenderer::SetFramerateLimit(int limit)
{
    m_fps_limit = limit;
    if (limit > 0)
        m_targetFrameDuration = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            std::chrono::microseconds(1000000 / limit));
    else
        m_targetFrameDuration = std::chrono::steady_clock::duration::zero();
}

int SFMLRenderer::GetFramerateLimit() const
{
    return m_fps_limit;
}

void SFMLRenderer::SetVSyncMode(bool enabled)
{
    m_vsync = enabled;
}

void SFMLRenderer::change_display_mode(hb::shared::types::NativeWindowHandle hWnd)
{
    // get the window through the hb::shared::render::Window factory
    hb::shared::render::IWindow* window = hb::shared::render::Window::get();
    if (!window)
        return;

    // Cast to SFMLWindow to access SFML-specific methods
    SFMLWindow* sfml_window = static_cast<SFMLWindow*>(window);

    // Apply the fullscreen setting to the window
    // This will recreate the window with the new mode
    sfml_window->set_fullscreen(m_fullscreen);

    // Update our render window pointer (window was recreated)
    m_render_window = sfml_window->GetRenderWindow();

    // Ensure the OpenGL context is active after window recreation
    if (m_render_window)
    {
        (void)m_render_window->setActive(true);
    }

    // Verify render textures are still valid, recreate if needed
    if (m_texturesCreated)
    {
        // Check if textures need to be recreated by verifying they're still valid
        sf::Vector2u backBufferSize = m_backBuffer.getSize();
        if (backBufferSize.x == 0 || backBufferSize.y == 0)
        {
            // Textures became invalid, recreate them
            m_texturesCreated = false;
            CreateRenderTextures();
        }
    }
}

void SFMLRenderer::begin_frame()
{
    // Ensure OpenGL context is active before any rendering operations
    if (m_render_window)
    {
        (void)m_render_window->setActive(true);
    }

    // Engine-owned frame limiting: skip this frame if not enough time has elapsed
    // Unlimited (0) always renders; VSync uses monitor refresh rate as the target
    if (m_fps_limit > 0)
    {
        auto now = std::chrono::steady_clock::now();
        if ((now - m_lastPresentTime) < m_targetFrameDuration)
        {
            m_skip_frame = true;
            // Sleep 1ms to avoid spinning the CPU (accurate with timeBeginPeriod(1))
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            return;
        }
    }
    m_skip_frame = false;

    // Reset the view to ensure 1:1 pixel mapping (prevents edge artifacts)
    sf::View pixelView(sf::FloatRect({0.f, 0.f}, {static_cast<float>(m_width), static_cast<float>(m_height)}));
    m_backBuffer.setView(pixelView);

    // Clear to TRANSPARENT black (alpha=0) so that sprite alpha compositing works correctly
    // When we present to the window, we'll draw over a black background
    m_backBuffer.clear(sf::Color::Transparent);

    // Enable scissor test to clip all drawing to the logical resolution
    // This prevents any content from being drawn outside the intended area
    glEnable(GL_SCISSOR_TEST);
    glScissor(0, 0, m_width, m_height);
}

void SFMLRenderer::end_frame()
{
    if (m_skip_frame)
        return;

    // Disable scissor test before presenting
    glDisable(GL_SCISSOR_TEST);

    m_backBuffer.display();

    if (m_render_window && m_render_window->isOpen())
    {
        m_render_window->clear(sf::Color::Black);

        // get actual window size in physical pixels
        float windowWidth, windowHeight;

#ifdef _WIN32
        // When DPI aware, use GetClientRect for accurate physical pixel dimensions
        HWND hWnd = static_cast<HWND>(m_render_window->getNativeHandle());
        if (hWnd)
        {
            RECT clientRect;
            if (GetClientRect(hWnd, &clientRect))
            {
                windowWidth = static_cast<float>(clientRect.right - clientRect.left);
                windowHeight = static_cast<float>(clientRect.bottom - clientRect.top);
            }
            else
            {
                sf::Vector2u size = m_render_window->getSize();
                windowWidth = static_cast<float>(size.x);
                windowHeight = static_cast<float>(size.y);
            }
        }
        else
#endif
        {
            sf::Vector2u size = m_render_window->getSize();
            windowWidth = static_cast<float>(size.x);
            windowHeight = static_cast<float>(size.y);
        }

        // Reset the view to match actual window size (1:1 pixel mapping)
        // This is crucial after window resize - SFML's default view changes with setSize()
        sf::View pixelView(sf::FloatRect({0.f, 0.f}, {windowWidth, windowHeight}));
        m_render_window->setView(pixelView);

        // Use explicit source rectangle to avoid including any texture padding
        // SFML render textures may have internal padding beyond the requested size
        sf::IntRect sourceRect({0, 0}, {m_width, m_height});
        sf::Sprite backBufferSprite(m_backBuffer.getTexture(), sourceRect);

        float scaleX = windowWidth / static_cast<float>(m_width);
        float scaleY = windowHeight / static_cast<float>(m_height);

        if (m_fullscreen && !m_fullscreen_stretch)
        {
            // Fullscreen letterbox: uniform scale to maintain aspect ratio
            float scale = (scaleY < scaleX) ? scaleY : scaleX;

            float destWidth = static_cast<float>(m_width) * scale;
            float destHeight = static_cast<float>(m_height) * scale;
            float offsetX = (windowWidth - destWidth) / 2.0f;
            float offsetY = (windowHeight - destHeight) / 2.0f;

            backBufferSprite.setScale({scale, scale});
            backBufferSprite.setPosition({offsetX, offsetY});
        }
        else
        {
            // Windowed or fullscreen stretch: fill entire window
            backBufferSprite.setScale({scaleX, scaleY});
            backBufferSprite.setPosition({0.0f, 0.0f});
        }

        // Toggle filtering for final upscale: smooth (bilinear) or crisp (nearest)
        // Applied only during present so internal logical rendering remains unchanged.
        const_cast<sf::Texture&>(m_backBuffer.getTexture()).setSmooth(m_upscale_smoothing);

        m_render_window->draw(backBufferSprite, sf::RenderStates(sf::BlendAlpha));

        // Restore nearest-neighbor for internal rendering
        const_cast<sf::Texture&>(m_backBuffer.getTexture()).setSmooth(false);

        m_render_window->display();

        // Track frame metrics at the actual point of present
        auto now = std::chrono::steady_clock::now();

        if (m_fps_limit > 0)
        {
            // Advance deadline by exact target duration to prevent timing drift
            // (snapping to 'now' would accumulate overshoot from sleep granularity)
            m_lastPresentTime += m_targetFrameDuration;

            // If we've fallen behind by more than one frame, snap to avoid catch-up burst
            if ((now - m_lastPresentTime) >= m_targetFrameDuration)
                m_lastPresentTime = now;
        }

        auto elapsed = std::chrono::duration<double>(now - m_lastPresentedFrameTime);
        m_delta_time = elapsed.count();
        m_lastPresentedFrameTime = now;

        m_framesThisSecond++;
        m_fps_accumulator += m_delta_time;
        if (m_fps_accumulator >= 1.0)
        {
            m_fps = m_framesThisSecond;
            m_framesThisSecond = 0;
            m_fps_accumulator -= 1.0;
        }
    }
}

bool SFMLRenderer::end_frame_check_lost_surface()
{
    if (m_skip_frame)
        return false;
    end_frame();
    // SFML doesn't have surface loss like DirectDraw
    return false;
}

bool SFMLRenderer::was_frame_presented() const
{
    return !m_skip_frame;
}

uint32_t SFMLRenderer::get_fps() const
{
    return m_fps;
}

double SFMLRenderer::get_delta_time() const
{
    return m_delta_time;
}

double SFMLRenderer::get_delta_time_ms() const
{
    return m_delta_time * 1000.0;
}

void SFMLRenderer::draw_pixel(int x, int y, const hb::shared::render::Color& color)
{
    if (x < m_clipArea.Left() || x >= m_clipArea.Right() ||
        y < m_clipArea.Top() || y >= m_clipArea.Bottom())
        return;

    sf::RectangleShape pixel({1.0f, 1.0f});
    pixel.setPosition({static_cast<float>(x), static_cast<float>(y)});
    pixel.setFillColor(sf::Color(color.r, color.g, color.b, color.a));
    m_backBuffer.draw(pixel);
}

void SFMLRenderer::draw_line(int x0, int y0, int x1, int y1, const hb::shared::render::Color& color, hb::shared::render::BlendMode blend)
{
    if ((x0 == x1) && (y0 == y1)) return;

    sf::Color lineColor(color.r, color.g, color.b, color.a);

    sf::VertexArray line(sf::PrimitiveType::Lines, 2);
    line[0].position = sf::Vector2f(static_cast<float>(x0), static_cast<float>(y0));
    line[0].color = lineColor;
    line[1].position = sf::Vector2f(static_cast<float>(x1), static_cast<float>(y1));
    line[1].color = lineColor;

    m_backBuffer.draw(line, (blend == hb::shared::render::BlendMode::Additive) ? sf::BlendAdd : sf::BlendAlpha);
}

void SFMLRenderer::draw_rect_filled(int x, int y, int w, int h, const hb::shared::render::Color& color)
{
    if (color.a == 0 || w <= 0 || h <= 0) return;

    sf::RectangleShape rect({static_cast<float>(w), static_cast<float>(h)});
    rect.setPosition({static_cast<float>(x), static_cast<float>(y)});
    rect.setFillColor(sf::Color(color.r, color.g, color.b, color.a));
    m_backBuffer.draw(rect);
}

void SFMLRenderer::draw_rect_outline(int x, int y, int w, int h, const hb::shared::render::Color& color, int thickness)
{
    if (color.a == 0 || w <= 0 || h <= 0 || thickness <= 0) return;

    sf::Color c(color.r, color.g, color.b, color.a);
    float fx = static_cast<float>(x);
    float fy = static_cast<float>(y);
    float fw = static_cast<float>(w);
    float fh = static_cast<float>(h);
    float ft = static_cast<float>(thickness);

    // Top edge
    sf::RectangleShape top({fw, ft});
    top.setPosition({fx, fy});
    top.setFillColor(c);
    m_backBuffer.draw(top);

    // Bottom edge
    sf::RectangleShape bottom({fw, ft});
    bottom.setPosition({fx, fy + fh - ft});
    bottom.setFillColor(c);
    m_backBuffer.draw(bottom);

    // Left edge (between top and bottom)
    sf::RectangleShape left({ft, fh - 2.0f * ft});
    left.setPosition({fx, fy + ft});
    left.setFillColor(c);
    m_backBuffer.draw(left);

    // Right edge (between top and bottom)
    sf::RectangleShape right({ft, fh - 2.0f * ft});
    right.setPosition({fx + fw - ft, fy + ft});
    right.setFillColor(c);
    m_backBuffer.draw(right);
}

// Helper: generate rounded rect corner points into an array, deduplicating coincident points.
// Returns the number of unique points written.
static int GenerateRoundedRectPoints(sf::Vector2f* out, int maxVerts,
                                     float fx, float fy, float fw, float fh, float fr)
{
    constexpr int kSegments = 8;
    constexpr float kPi = 3.14159265f;
    constexpr float kHalfPi = kPi * 0.5f;
    constexpr float kEpsilon = 0.01f;

    sf::Vector2f pts[kSegments * 4];
    int idx = 0;
    float cornerAngles[4] = { kPi, kHalfPi, 0.0f, -kHalfPi };
    float centerX[4] = { fx + fr, fx + fw - fr, fx + fw - fr, fx + fr };
    float centerY[4] = { fy + fr, fy + fr, fy + fh - fr, fy + fh - fr };

    for (int c = 0; c < 4; ++c)
    {
        for (int i = 0; i < kSegments; ++i)
        {
            float angle = cornerAngles[c] - kHalfPi * i / (kSegments - 1);
            pts[idx++] = { centerX[c] + fr * std::cos(angle),
                           centerY[c] - fr * std::sin(angle) };
        }
    }

    // Deduplicate adjacent coincident points
    int count = 0;
    for (int i = 0; i < idx && count < maxVerts; ++i)
    {
        if (count == 0 ||
            std::abs(pts[i].x - out[count - 1].x) > kEpsilon ||
            std::abs(pts[i].y - out[count - 1].y) > kEpsilon)
        {
            out[count++] = pts[i];
        }
    }
    // Check wrap-around (last vs first)
    if (count > 1 &&
        std::abs(out[count - 1].x - out[0].x) <= kEpsilon &&
        std::abs(out[count - 1].y - out[0].y) <= kEpsilon)
    {
        --count;
    }
    return count;
}

void SFMLRenderer::draw_rounded_rect_filled(int x, int y, int w, int h, int radius, const hb::shared::render::Color& color)
{
    if (color.a == 0 || w <= 0 || h <= 0) return;

    float fx = static_cast<float>(x);
    float fy = static_cast<float>(y);
    float fw = static_cast<float>(w);
    float fh = static_cast<float>(h);

    float maxRadius = (std::min)(fw, fh) * 0.5f;
    float fr = (std::min)(static_cast<float>(radius), maxRadius);

    if (fr <= 0.0f)
    {
        draw_rect_filled(x, y, w, h, color);
        return;
    }

    sf::Vector2f pts[32];
    int count = GenerateRoundedRectPoints(pts, 32, fx, fy, fw, fh, fr);
    if (count < 3) { draw_rect_filled(x, y, w, h, color); return; }

    // Use TriangleFan — avoids ConvexShape's edge-normal computation entirely
    sf::VertexArray fan(sf::PrimitiveType::TriangleFan, count + 2);
    sf::Color c(color.r, color.g, color.b, color.a);

    // Center point
    fan[0].position = { fx + fw * 0.5f, fy + fh * 0.5f };
    fan[0].color = c;
    for (int i = 0; i < count; ++i)
    {
        fan[i + 1].position = pts[i];
        fan[i + 1].color = c;
    }
    // Close the fan back to the first perimeter point
    fan[count + 1].position = pts[0];
    fan[count + 1].color = c;

    m_backBuffer.draw(fan);
}

void SFMLRenderer::draw_rounded_rect_outline(int x, int y, int w, int h, int radius,
                                          const hb::shared::render::Color& color, int thickness)
{
    if (color.a == 0 || w <= 0 || h <= 0 || thickness <= 0) return;

    float fx = static_cast<float>(x);
    float fy = static_cast<float>(y);
    float fw = static_cast<float>(w);
    float fh = static_cast<float>(h);

    float maxRadius = (std::min)(fw, fh) * 0.5f;
    float fr = (std::min)(static_cast<float>(radius), maxRadius);

    if (fr <= 0.0f)
    {
        draw_rect_outline(x, y, w, h, color, thickness);
        return;
    }

    float ft = static_cast<float>(thickness);

    // Inner rect dimensions (inset by thickness on all sides)
    float ifx = fx + ft;
    float ify = fy + ft;
    float ifw = fw - 2.0f * ft;
    float ifh = fh - 2.0f * ft;
    float ifr = (std::max)(0.0f, fr - ft);

    // If inner rect is degenerate, fall back to filled
    if (ifw <= 0.0f || ifh <= 0.0f)
    {
        draw_rounded_rect_filled(x, y, w, h, radius, color);
        return;
    }

    // Generate both perimeters with identical point counts using same angles
    constexpr int kSegments = 8;
    constexpr float kPi = 3.14159265f;
    constexpr float kHalfPi = kPi * 0.5f;
    constexpr int totalPts = kSegments * 4;

    sf::Color sfColor(color.r, color.g, color.b, color.a);

    float cornerAngles[4] = { kPi, kHalfPi, 0.0f, -kHalfPi };

    // Outer corner centers
    float oCX[4] = { fx + fr, fx + fw - fr, fx + fw - fr, fx + fr };
    float oCY[4] = { fy + fr, fy + fr, fy + fh - fr, fy + fh - fr };

    // Inner corner centers
    float cx[4] = { ifx + ifr, ifx + ifw - ifr, ifx + ifw - ifr, ifx + ifr };
    float cy[4] = { ify + ifr, ify + ifr, ify + ifh - ifr, ify + ifh - ifr };

    // TriangleStrip: for each perimeter point, emit outer then inner
    sf::VertexArray strip(sf::PrimitiveType::TriangleStrip, (totalPts + 1) * 2);

    for (int corner = 0; corner < 4; ++corner)
    {
        for (int i = 0; i < kSegments; ++i)
        {
            int pi = corner * kSegments + i;
            float angle = cornerAngles[corner] - kHalfPi * i / (kSegments - 1);
            float ca = std::cos(angle);
            float sa = std::sin(angle);

            strip[pi * 2].position = { oCX[corner] + fr * ca, oCY[corner] - fr * sa };
            strip[pi * 2].color = sfColor;

            strip[pi * 2 + 1].position = { cx[corner] + ifr * ca, cy[corner] - ifr * sa };
            strip[pi * 2 + 1].color = sfColor;
        }
    }

    // Close the strip back to the first point pair
    strip[totalPts * 2].position = strip[0].position;
    strip[totalPts * 2].color = sfColor;
    strip[totalPts * 2 + 1].position = strip[1].position;
    strip[totalPts * 2 + 1].color = sfColor;

    m_backBuffer.draw(strip);
}

void SFMLRenderer::begin_text_batch()
{
    // Delegate to TextLib for consistency
    hb::shared::text::ITextRenderer* text_renderer = hb::shared::text::GetTextRenderer();
    if (text_renderer)
        text_renderer->begin_batch();
}

void SFMLRenderer::end_text_batch()
{
    // Delegate to TextLib for consistency
    hb::shared::text::ITextRenderer* text_renderer = hb::shared::text::GetTextRenderer();
    if (text_renderer)
        text_renderer->end_batch();
}

void SFMLRenderer::draw_text(int x, int y, const char* text, const hb::shared::render::Color& color)
{
    if (!text || !m_texturesCreated)
        return;

    // Delegate to TextLib - single point of font handling
    hb::shared::text::ITextRenderer* text_renderer = hb::shared::text::GetTextRenderer();
    if (text_renderer)
        text_renderer->draw_text(x, y, text, color);
}

void SFMLRenderer::draw_text_rect(const hb::shared::geometry::GameRectangle& rect, const char* text, const hb::shared::render::Color& color)
{
    if (!text || !m_texturesCreated)
        return;

    // Delegate to TextLib - single point of font handling
    hb::shared::text::ITextRenderer* text_renderer = hb::shared::text::GetTextRenderer();
    if (text_renderer)
    {
        text_renderer->draw_text_aligned(rect.x, rect.y, rect.width, rect.height, text, color,
                                        hb::shared::text::Align::TopCenter);
    }
}

hb::shared::render::ITexture* SFMLRenderer::create_texture(uint16_t width, uint16_t height)
{
    return new SFMLTexture(width, height);
}

// Box-filter downscale an sf::Image by half (2:1 in each dimension).
// Averages 2x2 pixel blocks for high-quality minification.
static sf::Image downsample_half(const sf::Image& src)
{
    auto [sw, sh] = src.getSize();
    unsigned int dw = sw / 2;
    unsigned int dh = sh / 2;
    if (dw < 1) dw = 1;
    if (dh < 1) dh = 1;

    sf::Image dst({dw, dh}, sf::Color::Black);

    for (unsigned int dy = 0; dy < dh; ++dy)
    {
        for (unsigned int dx = 0; dx < dw; ++dx)
        {
            unsigned int sx = dx * 2;
            unsigned int sy = dy * 2;
            // Clamp neighbours so a 1-pixel-wide or -tall source never reads out of bounds
            unsigned int sx1 = std::min(sx + 1, sw - 1);
            unsigned int sy1 = std::min(sy + 1, sh - 1);

            // Average 2x2 block
            auto p00 = src.getPixel({sx, sy});
            auto p10 = src.getPixel({sx1, sy});
            auto p01 = src.getPixel({sx, sy1});
            auto p11 = src.getPixel({sx1, sy1});

            sf::Color avg;
            avg.r = static_cast<uint8_t>((p00.r + p10.r + p01.r + p11.r) / 4);
            avg.g = static_cast<uint8_t>((p00.g + p10.g + p01.g + p11.g) / 4);
            avg.b = static_cast<uint8_t>((p00.b + p10.b + p01.b + p11.b) / 4);
            avg.a = static_cast<uint8_t>((p00.a + p10.a + p01.a + p11.a) / 4);

            dst.setPixel({dx, dy}, avg);
        }
    }

    return dst;
}

hb::shared::render::ITexture* SFMLRenderer::create_texture_from_file(const char* path)
{
    if (!path)
    {
        return nullptr;
    }

    sf::Image image;
    if (!image.loadFromFile(path))
    {
        return nullptr;
    }

    // Downscale if image exceeds GPU max texture size
    unsigned int max_size = sf::Texture::getMaximumSize();
    auto [w, h] = image.getSize();
    while (w > max_size || h > max_size)
    {
        image = downsample_half(image);
        auto [nw, nh] = image.getSize();
        w = nw;
        h = nh;
    }

    // sf::Texture(image) throws on GPU upload failure; honour the nullptr-on-failure contract
    try
    {
        return new SFMLTexture(image);
    }
    catch (const std::exception&)
    {
        return nullptr;
    }
}

void SFMLRenderer::destroy_texture(hb::shared::render::ITexture* texture)
{
    delete texture;
}

void SFMLRenderer::set_clip_area(int x, int y, int w, int h)
{
    m_clipArea = hb::shared::geometry::GameRectangle(x, y, w, h);

    // Set SFML viewport/scissor
    // Note: SFML doesn't have direct scissor support, but we store it for manual clipping
}

hb::shared::geometry::GameRectangle SFMLRenderer::get_clip_area() const
{
    return m_clipArea;
}

bool SFMLRenderer::screenshot(const char* filename)
{
    if (!filename)
        return false;

    sf::Image screenshot = m_backBuffer.getTexture().copyToImage();
    return screenshot.saveToFile(filename);
}

int SFMLRenderer::get_width() const
{
    return m_width;
}

int SFMLRenderer::get_height() const
{
    return m_height;
}

int SFMLRenderer::get_width_mid() const
{
    return m_width / 2;
}

int SFMLRenderer::get_height_mid() const
{
    return m_height / 2;
}

void SFMLRenderer::resize_back_buffer(int width, int height)
{
    // The back buffer always stays at 640x480 (logical resolution)
    // hb::shared::render::Window scaling is handled in end_frame() when presenting to the window
    // This is intentionally a no-op for SFML
}

char SFMLRenderer::get_ambient_light_level() const
{
    return m_ambient_light_level;
}

void SFMLRenderer::set_ambient_light_level(char level)
{
    m_ambient_light_level = level;
}

void SFMLRenderer::color_transfer_rgb(uint32_t rgb, int* outR, int* outG, int* outB)
{
    // Extract RGB from COLORREF format (0x00BBGGRR) - keep full 8-bit values
    // SFML uses RGBA8888 natively so no conversion needed
    if (outR) *outR = static_cast<int>(rgb & 0xFF);
    if (outG) *outG = static_cast<int>((rgb >> 8) & 0xFF);
    if (outB) *outB = static_cast<int>((rgb >> 16) & 0xFF);
}

int SFMLRenderer::get_text_length(const char* text, int maxWidth)
{
    if (!text)
        return 0;

    // Delegate to TextLib - single point of font handling
    hb::shared::text::ITextRenderer* text_renderer = hb::shared::text::GetTextRenderer();
    if (text_renderer)
        return text_renderer->get_fitting_char_count(text, maxWidth);

    return 0;
}

int SFMLRenderer::get_text_width(const char* text)
{
    if (!text)
        return 0;

    // Delegate to TextLib - single point of font handling
    hb::shared::text::ITextRenderer* text_renderer = hb::shared::text::GetTextRenderer();
    if (text_renderer)
    {
        hb::shared::text::TextMetrics metrics = text_renderer->measure_text(text);
        return metrics.width;
    }

    return 0;
}

void* SFMLRenderer::get_back_buffer_native()
{
    return &m_backBuffer;
}

void* SFMLRenderer::get_native_renderer()
{
    // Return this renderer itself for SFML
    // Legacy code expecting DXC_ddraw* should check renderer type first
    return this;
}
