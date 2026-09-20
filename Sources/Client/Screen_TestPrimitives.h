// Screen_TestPrimitives.h: Visual Test Scene for Primitive Drawing
//
// Exercises every hb::shared::render::IRenderer primitive method:
// draw_pixel, draw_line, draw_rect_filled, draw_rect_outline,
// draw_rounded_rect_filled, draw_rounded_rect_outline
//
// Press ESC to exit the game.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "IGameScreen.h"

class Screen_TestPrimitives : public IGameScreen
{
public:
	SCREEN_TYPE(Screen_TestPrimitives)

    GameMode get_game_mode() const override { return GameMode::TestPrimitives; }

	explicit Screen_TestPrimitives(CGame* game);
	~Screen_TestPrimitives() override = default;

	void on_initialize() override;
	void on_update() override;
	void on_render() override;

private:
	void render_header();
	void render_pixel_tests(int y);
	void render_line_alpha_tests(int y);
	void render_line_additive_tests(int y);
	void render_rect_filled_tests(int y);
	void render_rect_outline_tests(int y);
	void render_rounded_rect_filled_tests(int y);
	void render_rounded_rect_outline_tests(int y);
};
