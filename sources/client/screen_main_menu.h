// screen_main_menu.h: Main Menu Screen
//
// Handles the initial menu (Login, New Account, Quit). The backdrop is 16:9 art (1920x1080) covering
// the window: the full image in a widescreen window, its centre 4:3 crop in letterbox or a 4:3 window.
// The buttons keep the original 800x600 menu's places, mapped onto the art's centre 4:3 region, and are
// re-laid out from the visible extent each frame.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "game_screen.h"
#include "ccontrols.h"
#include "primitive_types.h"
#include "texture.h"
#include <memory>

class Screen_MainMenu : public IGameScreen
{
public:
	SCREEN_TYPE(Screen_MainMenu)

	GameMode get_game_mode() const override { return GameMode::MainMenu; }

	explicit Screen_MainMenu(CGame* game);
	~Screen_MainMenu() override = default;

	void on_initialize() override;
	void on_uninitialize() override;
	void on_update() override;
	void on_render() override;

	// Public so the menu's item table (screen_main_menu.cpp) can name each button's control
	enum control_id { BTN_LOGIN = 1, BTN_NEW_ACCOUNT = 2, BTN_QUIT = 3 };

#ifdef DEF_CLAUDE_TESTING
	cc::control_collection* agent_controls() override { return &m_controls; }
	std::span<const agent_control_name> agent_control_names() const override
	{
		static constexpr agent_control_name names[] = {
			{BTN_LOGIN, "main_menu.login"},
			{BTN_NEW_ACCOUNT, "main_menu.new_account"},
			{BTN_QUIT, "main_menu.quit"},
		};
		return names;
	}
#endif

private:
	// Fits the art to the window and moves the buttons onto it
	void update_layout();
	// One button: its plate, then the label (black, or gold when hovered or focused)
	void draw_item(int index, bool highlighted) const;

	cc::control_collection m_controls;
	std::unique_ptr<hb::shared::render::ITexture> m_art;  // staged interface/front_end/main_menu.png
	hb::shared::render::art_fit m_fit;
};
