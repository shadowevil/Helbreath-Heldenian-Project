// IGameScreen.h: Interface for game screens with lifecycle management
//
// Provides an object-oriented approach to screen management with:
// - Lifecycle methods: on_initialize, on_uninitialize, on_update, on_render
// - Type identification via SCREEN_TYPE macro
// - Helper methods that delegate to CGame for common operations
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "CommonTypes.h"
#include "GameMode.h"
#include <cstdint>
#include <string>

enum class KeyCode : int;
class CGame;
class GameModeManager;
namespace cc { class control_collection; }

// Macro to define screen type - converts class name to string automatically
// Usage: Add SCREEN_TYPE(ClassName) in the public section of your screen class
#define SCREEN_TYPE(ClassName) \
    static constexpr ScreenTypeId screen_type_id = #ClassName; \
    ScreenTypeId get_type_id() const override { return screen_type_id; }

class IGameScreen
{
public:
    using ScreenTypeId = const char*;

    explicit IGameScreen(CGame* game);
    virtual ~IGameScreen() = default;

    // Prevent copying/moving - screens own state and should not be copied
    IGameScreen(const IGameScreen&) = delete;
    IGameScreen& operator=(const IGameScreen&) = delete;
    IGameScreen(IGameScreen&&) = delete;
    IGameScreen& operator=(IGameScreen&&) = delete;

    // ============== Core Lifecycle (pure virtual) ==============
    // These must be implemented by each screen

    // Called once when screen becomes active (after previous screen's on_uninitialize)
    virtual void on_initialize() = 0;

    // Called once when screen is about to be replaced (before new screen's on_initialize)
    virtual void on_uninitialize() {}

    // Called each frame to update logic, handle input, process state changes
    virtual void on_update() = 0;

    // Called each frame to render the screen (after begin_frame, before end_frame)
    virtual void on_render() = 0;

    // Returns the screen type identifier for runtime type checking
    virtual ScreenTypeId get_type_id() const = 0;

    // Returns the GameMode this screen represents (for auto-registration).
    // Override in Screen classes; overlays should return the default GameMode::Null.
    virtual GameMode get_game_mode() const;

    // Whether the overlay system should draw a full-screen dim behind this overlay.
    // Override to false for overlays that draw their own background (e.g. DevConsole).
    virtual bool wants_background_dim() const { return true; }

    // Called when a text character is entered (WM_CHAR / SFML TextEntered).
    // Return true if handled (e.g. auto-activated chat input).
    // The universal on_text_char forwarding happens regardless of return value.
    virtual bool on_text_input(uint32_t codepoint) { return false; }

    // Called on key press/release events routed from CGame::on_key_event.
    // Return true if handled, false to fall through.
    virtual bool on_key_down(KeyCode key) { return false; }
    virtual bool on_key_up(KeyCode key) { return false; }

    // Called when a server response arrives in log_response_handler.
    // Return true if this screen handled the response (stops further processing),
    // false to fall through to default handling. Optional — not all screens need this.
    virtual bool on_net_response(uint16_t response_type, char* data) { return false; }

    // Called when a game server message arrives in game_recv_msg_handler.
    // Return true if this screen handled the message (stops further processing),
    // false to fall through to default handling. Optional — not all screens need this.
    virtual bool on_game_msg(uint32_t msg_id, uint16_t msg_type, char* data, uint32_t msg_size) { return false; }

    // Per-screen transition timing (overridable). Defaults to DEFAULT_FADE_DURATION.
    virtual float get_fade_in_duration() const;
    virtual float get_fade_out_duration() const;

    // Called just before fade-out begins when this screen is being replaced.
    // Use for cleanup or state saving that must happen before transition starts.
    virtual void on_transition_out() {}

protected:
    // ============== Helper Methods (delegate to CGame) ==============
    // These provide convenient access to common CGame functionality

    // Drawing helpers
    void draw_new_dialog_box(char type, int sX, int sY, int frame,
                          bool is_no_color_key = false, bool is_trans = false);
    // Computes centered position for a dialog sprite frame within the logical resolution
    void get_centered_dialog_pos(char type, int frame, int& outX, int& outY);
    // Computes centered position AND draws the dialog box in one call
    void draw_centered_dialog_box(char type, int frame, int& out_x, int& out_y,
                                  bool is_no_color_key = false, bool is_trans = false);
    void put_string(int iX, int iY, const char* string, const hb::shared::render::Color& color);
    void put_aligned_string(int x1, int x2, int iY, const char* string,
                          const hb::shared::render::Color& color = GameColors::UIBlack);
    void put_string_spr_font(int iX, int iY, const char* str, uint8_t r, uint8_t g, uint8_t b);
    void draw_version();

    // Event/message helpers
    void add_event_list(const char* txt, char color = 0, bool dup_allow = true);

    // Screen transition helper - request transition to a new screen
    // This delegates to GameModeManager::set_screen<T>()
    template<typename T, typename... Args>
    void set_screen(Args&&... args);

    // Overlay helpers - show/hide overlay screens on top of base screen
    // Overlays automatically block input to the base screen
    template<typename T, typename... Args>
    void set_overlay(Args&&... args);

    void clear_overlay();

    // CControls input helpers — replace 3-line fill_input_state + update/discard pattern
    void update_controls(cc::control_collection& controls);
    void discard_pending_controls_input(cc::control_collection& controls);

    // Timing helper - returns milliseconds since this screen/overlay was initialized
    uint32_t get_elapsed_ms() const;

    // Access to owning game instance
    CGame* m_game;
};

// Template implementation - defined inline since it just forwards to GameModeManager
// The actual implementation requires GameModeManager.h, so screens must include both headers
