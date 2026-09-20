// GameModeManager.cpp: Implementation of game mode transitions with fade effects
//
//////////////////////////////////////////////////////////////////////

// Game.h must come first to ensure winsock2 is loaded before winsock
#include "Game.h"         // For CGame, m_Renderer, LOGICAL_MAX_X()/Y
#include "GameModeManager.h"
#include "IGameScreen.h"
#include "CommonTypes.h"  // For GameClock
#include "performance_monitor.h"  // For FrameTiming::GetDeltaTime()
#include "IInput.h"       // For hb::shared::input::SetSuppressed
#include "ConfigManager.h"
#include "RendererFactory.h"
#include "Log.h"

// ============== Singleton ==============

GameModeManager& GameModeManager::get()
{
    static GameModeManager instance;
    return instance;
}

// ============== Constructor/Destructor ==============

GameModeManager::GameModeManager()
{
    // Default initialization - actual setup happens in initialize_impl
}

GameModeManager::~GameModeManager()
{
    shutdown_impl();
}

// ============== Initialization ==============

void GameModeManager::initialize_impl(CGame* game)
{
    m_game = game;
    m_modeStartTime = GameClock::get_time_ms();
}

void GameModeManager::shutdown_impl()
{
    // clear overlay first
    clear_overlay_impl();
    m_dying_overlay.reset();

    // Ensure screen is properly uninitialized before destruction
    if (m_pCurrentScreen)
    {
        m_pCurrentScreen->on_uninitialize();
        m_pCurrentScreen.reset();
    }
    m_pendingScreenFactory = nullptr;
    m_game = nullptr;
}

// ============== Overlay Implementation ==============

void GameModeManager::clear_overlay_impl()
{
    if (m_pActiveOverlay)
    {
        m_pActiveOverlay->on_uninitialize();
        // Defer destruction — the overlay may still be on the call stack
        // (e.g., clear_overlay() called from a button handler inside m_controls.update()).
        // Moving to m_dying_overlay keeps it alive until update_screens_impl() finishes.
        m_dying_overlay = std::move(m_pActiveOverlay);
    }
}

// ============== Frame update ==============

void GameModeManager::update_impl()
{
    if (m_transitionState == TransitionState::None)
        return;

    float dt = static_cast<float>(FrameTiming::get_delta_time());

    switch (m_transitionState)
    {
    case TransitionState::FadeOut:
        m_transitionTime += dt;
        if (m_transitionTime >= m_config.fade_out_duration)
        {
            // Fade-out complete, enter switching phase
            m_transitionState = TransitionState::Switching;
            m_transitionTime = 0.0f;
        }
        break;

    case TransitionState::Switching:
        // Perform the actual screen switch at full opacity
        apply_screen_change();

        // Begin fade-in
        m_transitionState = TransitionState::FadeIn;
        m_transitionTime = 0.0f;
        break;

    case TransitionState::FadeIn:
        m_transitionTime += dt;
        if (m_transitionTime >= m_config.fade_in_duration)
        {
            // Transition complete
            m_transitionState = TransitionState::None;
            m_transitionTime = 0.0f;
        }
        break;

    case TransitionState::None:
        // Should not reach here, but handle it gracefully
        break;
    }
}

// ============== Screen update/render ==============

void GameModeManager::update_screens_impl()
{
    // Skip during switching phase (screen is being replaced)
    if (m_transitionState == TransitionState::Switching)
        return;

    bool transitioning = (m_transitionState != TransitionState::None);

    // Suppress input for base screen during transitions or when overlay is active
    if (transitioning || m_pActiveOverlay)
    {
        hb::shared::input::set_suppressed(true);
    }

    // update base screen (input suppressed if transitioning or overlay active)
    if (m_pCurrentScreen)
    {
        m_pCurrentScreen->on_update();
    }

    // Always restore input before overlay update
    hb::shared::input::set_suppressed(false);

    // update overlay (receives input only when not transitioning)
    if (m_pActiveOverlay && !transitioning)
    {
        m_pActiveOverlay->on_update();
    }

    // Destroy overlay that was cleared during this frame's update.
    // Safe now — we're no longer inside the overlay's call stack.
    m_dying_overlay.reset();
}

void GameModeManager::render_impl()
{
    // render base screen
    if (m_pCurrentScreen)
    {
        m_pCurrentScreen->on_render();
    }

    // render overlay on top with shadow box
    if (m_pActiveOverlay)
    {
        if (m_pActiveOverlay->wants_background_dim())
            m_game->m_Renderer->draw_rect_filled(0, 0, LOGICAL_MAX_X(), LOGICAL_MAX_Y(), hb::shared::render::Color::Black(128));
        m_pActiveOverlay->on_render();
    }
}

// ============== Fade Alpha ==============

float GameModeManager::get_fade_alpha_impl() const
{
    switch (m_transitionState)
    {
    case TransitionState::FadeOut:
    {
        // Fading out: 0.0 -> 1.0 (transparent to black)
        if (m_config.fade_out_duration <= 0.0f) return 1.0f;
        float progress = m_transitionTime / m_config.fade_out_duration;
        return progress > 1.0f ? 1.0f : progress;
    }

    case TransitionState::Switching:
        // At full black during switch
        return 1.0f;

    case TransitionState::FadeIn:
    {
        // Fading in: 1.0 -> 0.0 (black to transparent)
        if (m_config.fade_in_duration <= 0.0f) return 0.0f;
        float alpha = 1.0f - (m_transitionTime / m_config.fade_in_duration);
        return alpha < 0.0f ? 0.0f : alpha;
    }

    case TransitionState::None:
    default:
        return 0.0f;
    }
}

// ============== Type Checking ==============

GameModeManager::ScreenTypeId GameModeManager::get_current_screen_type_impl() const
{
    if (!m_pCurrentScreen) return nullptr;
    return m_pCurrentScreen->get_type_id();
}

// ============== Screen Application ==============

void GameModeManager::apply_screen_change()
{
    // Uninitialize current screen
    if (m_pCurrentScreen)
    {
        m_previousScreenType = m_pCurrentScreen->get_type_id();
        m_pCurrentScreen->on_uninitialize();
        m_pCurrentScreen.reset();
        m_game->m_active_screen = nullptr;
    }

    // Create and initialize new screen from factory
    if (m_pendingScreenFactory)
    {
        m_pCurrentScreen = m_pendingScreenFactory();
        m_pendingScreenFactory = nullptr;

        if (m_pCurrentScreen)
        {
            m_game->m_active_screen = m_pCurrentScreen.get();
            m_pCurrentScreen->on_initialize();

            // Auto-register game mode from screen
            GameMode screen_mode = m_pCurrentScreen->get_game_mode();
            if (screen_mode != GameMode::Null)
                m_currentMode = screen_mode;

            // Apply screen's preferred transition timing for fade-in
            m_config.fade_in_duration = m_pCurrentScreen->get_fade_in_duration();

            // Apply user's configured display settings on every screen transition
            hb::shared::render::IWindow* window = hb::shared::render::Window::get();
            if (window)
            {
                window->set_vsync_enabled(config_manager::get().is_vsync_enabled());
                window->set_framerate_limit(config_manager::get().get_fps_limit());
            }
        }
    }

    // clear pending screen type now that transition is complete
    m_pendingScreenType = nullptr;

    // reset timing for new screen
    m_modeStartTime = GameClock::get_time_ms();
}
