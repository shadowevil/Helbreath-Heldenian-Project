// screen_quit.h: Quit Screen Interface
//
// Exit confirmation screen with 10-second auto-quit.
// Input is ignored for first 3 seconds to prevent accidental exit.
//
// The art is the HD exit sheet (HH-0103): interface/front_end/exit_screen.png, a 1920x1080 backdrop cover-fit
// like the other front-end screens, and exit_panel.png, the empty message panel. The message is drawn in code.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "game_screen.h"
#include "bake_debounce.h"
#include "primitive_types.h"
#include "texture.h"
#include <array>
#include <memory>

// The exit screen's backdrop, shared with the version-mismatch overlay
class exit_backdrop
{
public:
	void load(CGame* game);
	void release();
	// Draws it cover-fit and returns the fit, so the caller can place art on it
	hb::shared::render::art_fit draw(CGame* game);

private:
	std::unique_ptr<hb::shared::render::ITexture> m_art;
};

class Screen_Quit : public IGameScreen
{
public:
    SCREEN_TYPE(Screen_Quit)

    GameMode get_game_mode() const override { return GameMode::Quit; }

    explicit Screen_Quit(CGame* game);
    ~Screen_Quit() override = default;

    void on_initialize() override;
	void on_uninitialize() override;
    void on_update() override;
    void on_render() override;

private:
	void update_layout();   // this frame's fit; rebakes the message when the window's scale settles
	void bake_message();

	// One baked line of the message: window px from the line's centre to the texture's left, and from the
	// baseline to the texture's top
	struct message_line
	{
		std::unique_ptr<hb::shared::render::ITexture> text;
		float x = 0.0f;
		float y = 0.0f;
	};

    uint32_t m_dwStartTime = 0;
	exit_backdrop m_backdrop;
	std::unique_ptr<hb::shared::render::ITexture> m_panel;   // interface/front_end/exit_panel.png
	hb::shared::render::art_fit m_fit;
	std::array<message_line, 4> m_lines;
	float m_baked_scale = 0.0f;   // window px per art px the message was baked for; 0 = not yet
	float m_baked_ui_scale = 1.0f;
	hb::client::bake_debounce m_debounce;

    // Timing constants (milliseconds)
    static constexpr uint32_t FADE_IN_MS = 500;         // Fade in duration
    static constexpr uint32_t INPUT_ACTIVE_MS = 3000;   // 3 seconds before input is active
    static constexpr uint32_t AUTO_QUIT_MS = 10000;     // 10 seconds total
};
