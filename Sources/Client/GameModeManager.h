// GameModeManager.h: Manages game mode transitions with fade effects and screen objects
//
// Singleton manager for screen transitions with fade-out to black and fade-in effects
//
// Usage:
//   GameModeManager::initialize(game);  // Call once at startup
//   GameModeManager::set_screen<Screen_Login>();  // Transition to new screen
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <memory>
#include <functional>
#include <type_traits>
#include <tuple>

#include "IGameScreen.h"

class CGame;

// Transition state for fade animations
enum class TransitionState : uint8_t {
    None,       // No transition in progress - normal rendering
    FadeOut,    // Fading current screen to black
    Switching,  // At full black - switching screens
    FadeIn      // Fading from black to new screen
};

class GameModeManager {
public:
    // Fade duration constants (in seconds)
    static constexpr float FAST_FADE_DURATION = 0.15f;
    static constexpr float DEFAULT_FADE_DURATION = 0.25f;
    static constexpr float SLOW_FADE_DURATION = 0.5f;

    // Transition configuration
    struct TransitionConfig {
        float fade_out_duration = DEFAULT_FADE_DURATION;
        float fade_in_duration = DEFAULT_FADE_DURATION;
    };

    // Screen type identifier (for type checking without RTTI)
    using ScreenTypeId = const char*;

    // ============== Singleton Access ==============

    static GameModeManager& get();

    // ============== Initialization ==============

    static void initialize(CGame* game) { get().initialize_impl(game); }
    static void shutdown() { get().shutdown_impl(); }

    // ============== Template-Based Screen Creation (Static API) ==============

    template<typename T, typename... Args>
    static void set_screen(Args&&... args) {
        get().set_screen_impl<T>(std::forward<Args>(args)...);
    }

    // ============== Overlay System (Static API) ==============

    template<typename T, typename... Args>
    static void set_overlay(Args&&... args) {
        get().set_overlay_impl<T>(std::forward<Args>(args)...);
    }

    static void clear_overlay() { get().clear_overlay_impl(); }
    static bool has_overlay() { return get().m_pActiveOverlay != nullptr; }
    static IGameScreen* get_active_overlay() { return get().m_pActiveOverlay.get(); }

    template<typename T>
    static T* get_active_overlay_as() { return dynamic_cast<T*>(get().m_pActiveOverlay.get()); }

    template<typename T>
    static bool overlay_is() {
        static_assert(std::is_base_of_v<IGameScreen, T>, "T must derive from IGameScreen");
        auto& inst = get();
        if (!inst.m_pActiveOverlay) return false;
        return inst.m_pActiveOverlay->get_type_id() == T::screen_type_id;
    }

    // ============== Mode Tracking (Static API) ==============
    // Mode is auto-set from IGameScreen::get_game_mode() during screen transitions.
    // set_current_mode() is available for special cases (e.g., Game.cpp quit path).

    static GameMode get_mode() { return get().m_currentMode; }
    static int8_t get_mode_value() { return static_cast<int8_t>(get().m_currentMode); }
    static void set_current_mode(GameMode mode) { get().m_currentMode = mode; }

    // ============== Frame update (Static API) ==============

    static void update() { get().update_impl(); }
    static void update_screens() { get().update_screens_impl(); }
    static void render() { get().render_impl(); }

    // ============== Transition State Queries (Static API) ==============

    static bool is_transitioning() { return get().m_transitionState != TransitionState::None; }
    static TransitionState get_transition_state() { return get().m_transitionState; }
    static float get_fade_alpha() { return get().get_fade_alpha_impl(); }

    // ============== Active Screen Access (Static API) ==============

    static IGameScreen* get_active_screen() { return get().m_pCurrentScreen.get(); }

    template<typename T>
    static T* get_active_screen_as() { return dynamic_cast<T*>(get().m_pCurrentScreen.get()); }

    // ============== Type Checking (Static API) ==============

    static ScreenTypeId get_previous_screen_type() { return get().m_previousScreenType; }
    static ScreenTypeId get_current_screen_type() { return get().get_current_screen_type_impl(); }

    template<typename T>
    static bool previous_screen_is() {
        static_assert(std::is_base_of_v<IGameScreen, T>, "T must derive from IGameScreen");
        auto& inst = get();
        return inst.m_previousScreenType != nullptr && inst.m_previousScreenType == T::screen_type_id;
    }

    template<typename T>
    static bool current_screen_is() {
        static_assert(std::is_base_of_v<IGameScreen, T>, "T must derive from IGameScreen");
        auto& inst = get();
        if (!inst.m_pCurrentScreen) return false;
        return inst.m_pCurrentScreen->get_type_id() == T::screen_type_id;
    }

    // ============== Configuration (Static API) ==============

    static void set_transition_config(const TransitionConfig& config) { get().m_config = config; }
    static const TransitionConfig& get_transition_config() { return get().m_config; }

    // ============== Timing (Static API) ==============

    static uint32_t get_mode_start_time() { return get().m_modeStartTime; }

    // ============== CGame Access (for screens) ==============

    static CGame* get_game() { return get().m_game; }

private:
    // Private constructor/destructor for singleton
    GameModeManager();
    ~GameModeManager();
    GameModeManager(const GameModeManager&) = delete;
    GameModeManager& operator=(const GameModeManager&) = delete;

    // ============== Implementation Methods ==============

    void initialize_impl(CGame* game);
    void shutdown_impl();
    void update_impl();
    void update_screens_impl();
    void render_impl();
    float get_fade_alpha_impl() const;
    ScreenTypeId get_current_screen_type_impl() const;

    template<typename T, typename... Args>
    void set_screen_impl(Args&&... args) {
        static_assert(std::is_base_of_v<IGameScreen, T>, "T must derive from IGameScreen");

        // Ignore if already transitioning to the same screen type
        if (m_pendingScreenType == T::screen_type_id)
            return;

        // Notify current screen that it's transitioning out
        if (m_pCurrentScreen)
            m_pCurrentScreen->on_transition_out();

        // clear any active overlay when transitioning to a new screen
        clear_overlay_impl();

        // Track the pending screen type
        m_pendingScreenType = T::screen_type_id;

        // Store a factory that will create the screen during the Switching phase
        // Use tuple to capture variadic args (C++17 compatible)
        auto argsTuple = std::make_tuple(std::forward<Args>(args)...);
        m_pendingScreenFactory = [this, argsTuple = std::move(argsTuple)]() mutable {
            return std::apply([this](auto&&... capturedArgs) {
                return std::make_unique<T>(m_game, std::forward<decltype(capturedArgs)>(capturedArgs)...);
            }, std::move(argsTuple));
        };

        // Begin fade-out transition
        m_transitionState = TransitionState::FadeOut;
        m_transitionTime = 0.0f;
    }

    template<typename T, typename... Args>
    void set_overlay_impl(Args&&... args) {
        static_assert(std::is_base_of_v<IGameScreen, T>, "T must derive from IGameScreen");

        // clear existing overlay first
        clear_overlay_impl();

        // Create overlay immediately (no fade transition for overlays)
        m_pActiveOverlay = std::make_unique<T>(m_game, std::forward<Args>(args)...);
        m_pActiveOverlay->on_initialize();
    }

    void clear_overlay_impl();

    void apply_screen_change();

    // ============== State ==============

    CGame* m_game = nullptr;

    // Current screen object (owned) - null for legacy screens
    std::unique_ptr<IGameScreen> m_pCurrentScreen;

    // Overlay screen (owned) - displayed on top of current screen with shadow box
    // When active, base screen still updates but doesn't receive input
    std::unique_ptr<IGameScreen> m_pActiveOverlay;

    // Holds overlay being destroyed — defers destruction until update_screens_impl() finishes,
    // preventing use-after-free when clear_overlay() is called from within m_controls.update().
    std::unique_ptr<IGameScreen> m_dying_overlay;

    // Factory to create the next screen (set by set_screen<T>)
    std::function<std::unique_ptr<IGameScreen>()> m_pendingScreenFactory;

    // Screen type tracking
    ScreenTypeId m_previousScreenType = nullptr;
    ScreenTypeId m_pendingScreenType = nullptr;  // Type of screen being transitioned to

    // Current mode value (for code that checks what screen is active)
    GameMode m_currentMode = GameMode::Loading;

    // Transition state machine (time-based)
    TransitionState m_transitionState = TransitionState::None;
    TransitionConfig m_config;
    float m_transitionTime = 0.0f;

    // Timing
    uint32_t m_modeStartTime = 0;
};

// ============== IGameScreen Template Implementations ==============

#include "IGameScreen.h"

template<typename T, typename... Args>
void IGameScreen::set_screen(Args&&... args)
{
    GameModeManager::set_screen<T>(std::forward<Args>(args)...);
}

template<typename T, typename... Args>
void IGameScreen::set_overlay(Args&&... args)
{
    GameModeManager::set_overlay<T>(std::forward<Args>(args)...);
}

inline void IGameScreen::clear_overlay()
{
    GameModeManager::clear_overlay();
}
