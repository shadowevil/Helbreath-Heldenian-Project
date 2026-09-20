// Screen_Test.cpp: Text Rendering Test Screen Implementation
//
// Side-by-side comparison of legacy put_string* methods (left)
// and new hb::shared::text::draw_text methods (right) for visual validation.
//
//////////////////////////////////////////////////////////////////////

#include "Screen_Test.h"
#include "Game.h"
#include "GameModeManager.h"
#include "GameFonts.h"
#include "TextLibExt.h"
#include "BitmapFontFactory.h"
#include "SpriteLoader.h"
#include "IInput.h"
#include "RendererFactory.h"
using namespace hb::client::sprite_id;

Screen_Test::Screen_Test(CGame* game)
	: IGameScreen(game)
{
}

void Screen_Test::on_initialize()
{
	m_scrollOffset = 0;

	// Preload bitmap fonts for testing (normally done in Screen_Loading)
	load_bitmap_fonts();
}

void Screen_Test::load_bitmap_fonts()
{
	// Skip if already loaded
	if (hb::shared::text::is_bitmap_font_loaded(GameFont::Bitmap1))
		return;

	// load sprite_fonts sprites
	hb::shared::sprite::SpriteLoader::open_pak("sprite_fonts", [&](hb::shared::sprite::SpriteLoader& loader) {
		m_game->m_sprite[InterfaceFont1] = loader.get_sprite(0, false);
		m_game->m_sprite[InterfaceFont2] = loader.get_sprite(1, false);
		m_game->m_sprite[InterfaceSprFonts] = loader.get_sprite(2, false);
		m_game->m_sprite[InterfaceSprFonts3] = loader.get_sprite(3, false);
	});

	// Create bitmap fonts from the loaded sprites
	// Font 1: Characters '!' (33) to 'z' (122)
	if (m_game->m_sprite[InterfaceFont1])
	{
		hb::shared::text::load_bitmap_font(GameFont::Bitmap1,
			m_game->m_sprite[InterfaceFont1].get(), '!', 'z', 0,
			GameFont::GetFontSpacing(GameFont::Bitmap1));
	}

	// Font 2: Characters ' ' (32) to '~' (126) - uses dynamic spacing
	if (m_game->m_sprite[InterfaceFont2])
	{
		hb::shared::text::load_bitmap_font_dynamic(GameFont::Bitmap2,
			m_game->m_sprite[InterfaceFont2].get(), ' ', '~', 0);
	}

	// Number font: Digits '0' to '9', frame offset 30 in sprite_fonts sprite 2
	if (m_game->m_sprite[InterfaceSprFonts])
	{
		hb::shared::text::load_bitmap_font(GameFont::Numbers,
			m_game->m_sprite[InterfaceSprFonts].get(), '0', '9', 30,
			GameFont::GetFontSpacing(GameFont::Numbers));
	}

	// SPRFONTS3: Characters ' ' (32) to '~' (126), with 3 different sizes
	if (m_game->m_sprite[InterfaceSprFonts3])
	{
		for (int idx = 0; idx < 3; idx++)
		{
			int fontId = GameFont::SprFont3_0 + idx;
			hb::shared::text::load_bitmap_font_dynamic(fontId,
				m_game->m_sprite[InterfaceSprFonts3].get(), ' ', '~', 95 * idx);
		}
	}
}
void Screen_Test::on_update()
{
	// ESC to quit
	if (hb::shared::input::get() && hb::shared::input::get()->is_key_pressed(KeyCode::Escape))
	{
		hb::shared::render::Window::close();
		return;
	}

	// Scroll with arrow keys
	if (hb::shared::input::get())
	{
		if (hb::shared::input::get()->is_key_pressed(KeyCode::Up))
			m_scrollOffset = (m_scrollOffset > 0) ? m_scrollOffset - 1 : 0;
		if (hb::shared::input::get()->is_key_pressed(KeyCode::Down))
			m_scrollOffset++;
	}
}

void Screen_Test::on_render()
{
	// Fill with dark blue background for better shadow visibility
	m_game->m_Renderer->draw_rect_filled(0, 0, 640, 480, hb::shared::render::Color(32, 32, 48));

	render_header();

	// start text batch for efficiency
	hb::shared::text::begin_batch();

	int row = 0;

	// ============== TTF Font Tests ==============
	render_test_row(row++, "TTF: No Shadow",
	              &Screen_Test::Legacy_PutString_NoShadow,
	              &Screen_Test::New_DrawText_NoShadow);

	render_test_row(row++, "TTF: 3-Point Shadow (put_string bg=1)",
	              &Screen_Test::Legacy_PutString_WithShadow,
	              &Screen_Test::New_DrawText_ThreePoint);

	render_test_row(row++, "TTF: 3-Point Shadow (PutString2)",
	              &Screen_Test::Legacy_PutString2,
	              &Screen_Test::New_DrawText_ThreePoint2);

	render_test_row(row++, "TTF: Centered (put_aligned_string)",
	              &Screen_Test::Legacy_PutAlignedString,
	              &Screen_Test::New_DrawTextCentered);

	// ============== Bitmap Font Tests ==============
	// Only run these if bitmap fonts are loaded
	if (hb::shared::text::is_bitmap_font_loaded(GameFont::Bitmap1))
	{
		render_test_row(row++, "Bitmap1: Highlight (SprFont)",
		              &Screen_Test::Legacy_PutString_SprFont,
		              &Screen_Test::New_DrawText_Bitmap1_Highlight);

		render_test_row(row++, "Bitmap1: Raw Sprite Shadow (SprFont2)",
		              &Screen_Test::Legacy_PutString_SprFont2,
		              &Screen_Test::New_DrawText_Bitmap1_Integrated);
	}
	else
	{
		// Show placeholder if fonts not loaded
		int y = HEADER_HEIGHT + (row++ - m_scrollOffset) * ROW_HEIGHT;
		hb::shared::text::draw_text(GameFont::Default, LEFT_COLUMN_X, y, "Bitmap fonts not loaded - run through Loading screen first", hb::shared::text::TextStyle::from_color(hb::shared::render::Color(255, 100, 100)));
	}

	if (hb::shared::text::is_bitmap_font_loaded(GameFont::Bitmap2))
	{
		render_test_row(row++, "Bitmap2: Drop Shadow (SprFont3)",
		              &Screen_Test::Legacy_PutString_SprFont3,
		              &Screen_Test::New_DrawText_Bitmap2_DropShadow);

		render_test_row(row++, "Bitmap2: Transparent (SprFont3 trans)",
		              &Screen_Test::Legacy_PutString_SprFont3_Trans,
		              &Screen_Test::New_DrawText_Bitmap2_Transparent);
	}

	if (hb::shared::text::is_bitmap_font_loaded(GameFont::Numbers))
	{
		render_test_row(row++, "Numbers: SprNum",
		              &Screen_Test::Legacy_PutString_SprNum,
		              &Screen_Test::New_DrawText_Numbers);
	}

	// ============== Alignment Showcase ==============
	// Show only when scrolled down enough
	if (m_scrollOffset >= 5)
	{
		render_alignment_showcase();
	}
	else
	{
		int displayRow = row - m_scrollOffset;
		if (displayRow >= 0 && displayRow <= 10)
		{
			int y = HEADER_HEIGHT + displayRow * ROW_HEIGHT;
			hb::shared::text::draw_text(GameFont::Default, LEFT_COLUMN_X, y, "Scroll down for Alignment Showcase...", hb::shared::text::TextStyle::from_color(hb::shared::render::Color(100, 200, 255)));
		}
	}

	hb::shared::text::end_batch();
}

void Screen_Test::render_header()
{
	// Title
	hb::shared::text::draw_text(GameFont::Default, 220, 5, "TEXTLIB TEST SCREEN", hb::shared::text::TextStyle::from_color(GameColors::UITopMsgYellow));

	// Instructions
	hb::shared::text::draw_text(GameFont::Default, 10, 25, "ESC=Quit | UP/DOWN=Scroll", hb::shared::text::TextStyle::from_color(GameColors::InfoGrayLight));

	// Column headers
	hb::shared::text::draw_text(GameFont::Default, LEFT_COLUMN_X, 45, "LEGACY (put_string*)", hb::shared::text::TextStyle::from_color(hb::shared::render::Color(255, 150, 150)));
	hb::shared::text::draw_text(GameFont::Default, RIGHT_COLUMN_X, 45, "NEW (hb::shared::text::draw_text)", hb::shared::text::TextStyle::from_color(hb::shared::render::Color(150, 255, 150)));

	// Divider line (visual separator)
	int midX = 320;
	for (int y = HEADER_HEIGHT; y < 480; y += 2)
	{
		m_game->m_Renderer->draw_pixel(midX, y, hb::shared::render::Color(80, 80, 80));
	}
}

void Screen_Test::render_test_row(int row, const char* testName,
                                void (Screen_Test::*legacyFunc)(int x, int y),
                                void (Screen_Test::*newFunc)(int x, int y))
{
	int displayRow = row - m_scrollOffset;
	if (displayRow < 0 || displayRow > 10)
		return;  // Off screen

	int baseY = HEADER_HEIGHT + displayRow * ROW_HEIGHT;

	// Test name label (small, gray)
	hb::shared::text::draw_text(GameFont::Default, LEFT_COLUMN_X, baseY, testName, hb::shared::text::TextStyle::from_color(GameColors::UIDisabled));

	// render legacy version on left
	(this->*legacyFunc)(LEFT_COLUMN_X, baseY + 16);

	// render new version on right
	(this->*newFunc)(RIGHT_COLUMN_X, baseY + 16);
}

// ============== Legacy Test Functions ==============

void Screen_Test::Legacy_PutString_NoShadow(int x, int y)
{
	hb::shared::text::draw_text(GameFont::Default, x, y, "Hello World", hb::shared::text::TextStyle::from_color(GameColors::UIWhite));
}

void Screen_Test::Legacy_PutString_WithShadow(int x, int y)
{
	// put_string with cBGtype=1 for 3-point shadow
	hb::shared::text::draw_text(GameFont::Default, x, y, "Hello World", hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
}

void Screen_Test::Legacy_PutString2(int x, int y)
{
	hb::shared::text::draw_text(GameFont::Default, x, y, "Hello World", hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
}

void Screen_Test::Legacy_PutAlignedString(int x, int y)
{
	// Centered between x and x+COLUMN_WIDTH
	hb::shared::text::draw_text_aligned(GameFont::Default, x, y, x + COLUMN_WIDTH - x, 15, "Centered Text", hb::shared::text::TextStyle::from_color(hb::shared::render::Color(255, 200, 100)), hb::shared::text::Align::TopCenter);
}

void Screen_Test::Legacy_PutString_SprFont(int x, int y)
{
	if (hb::shared::text::is_bitmap_font_loaded(GameFont::Bitmap1))
	{
		hb::shared::text::draw_text(GameFont::Bitmap1, x, y, "Hello World", hb::shared::text::TextStyle::with_highlight(GameColors::UIDisabled));
	}
}

void Screen_Test::Legacy_PutString_SprFont2(int x, int y)
{
	if (hb::shared::text::is_bitmap_font_loaded(GameFont::Bitmap1))
	{
		hb::shared::text::draw_text(GameFont::Bitmap1, x, y, "Hello World", hb::shared::text::TextStyle::with_integrated_shadow(hb::shared::render::Color(255, 200, 100)));
	}
}

void Screen_Test::Legacy_PutString_SprFont3(int x, int y)
{
	if (hb::shared::text::is_bitmap_font_loaded(GameFont::Bitmap2))
	{
		hb::shared::text::draw_text(GameFont::Bitmap2, x, y, "Hello World", hb::shared::text::TextStyle::with_two_point_shadow(hb::shared::render::Color(100, 200, 255)));
	}
}

void Screen_Test::Legacy_PutString_SprFont3_Trans(int x, int y)
{
	if (hb::shared::text::is_bitmap_font_loaded(GameFont::Bitmap2))
	{
		hb::shared::text::draw_text(GameFont::Bitmap2, x, y, "Transparent", hb::shared::text::TextStyle::from_color(GameColors::UIWhite).with_alpha(0.7f));
	}
}

void Screen_Test::Legacy_PutString_SprNum(int x, int y)
{
	if (hb::shared::text::is_bitmap_font_loaded(GameFont::Numbers))
	{
		// Shadow first
		hb::shared::text::draw_text(GameFont::Numbers, x + 1, y + 1, "12345", hb::shared::text::TextStyle::from_color(GameColors::UIBlack));
		// Main text
		hb::shared::text::draw_text(GameFont::Numbers, x, y, "12345", hb::shared::text::TextStyle::from_color(GameColors::UIWhite));
	}
}

// ============== New TextLib Functions ==============

void Screen_Test::New_DrawText_NoShadow(int x, int y)
{
	hb::shared::text::draw_text(GameFont::Default, x, y, "Hello World",
	                  hb::shared::text::TextStyle::from_color(GameColors::UIWhite));
}

void Screen_Test::New_DrawText_ThreePoint(int x, int y)
{
	hb::shared::text::draw_text(GameFont::Default, x, y, "Hello World",
	                  hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
}

void Screen_Test::New_DrawText_ThreePoint2(int x, int y)
{
	hb::shared::text::draw_text(GameFont::Default, x, y, "Hello World",
	                  hb::shared::text::TextStyle::with_shadow(GameColors::UIWhite));
}

void Screen_Test::New_DrawTextCentered(int x, int y)
{
	hb::shared::text::draw_text_aligned(GameFont::Default, x, y, COLUMN_WIDTH, 20, "Centered Text",
	                         hb::shared::text::TextStyle::from_color(hb::shared::render::Color(255, 200, 100)), hb::shared::text::Align::HCenter);
}

void Screen_Test::New_DrawText_Bitmap1_Highlight(int x, int y)
{
	hb::shared::text::draw_text(GameFont::Bitmap1, x, y, "Hello World",
	                  hb::shared::text::TextStyle::with_highlight(GameColors::UIDisabled));
}

void Screen_Test::New_DrawText_Bitmap1_Integrated(int x, int y)
{
	hb::shared::text::draw_text(GameFont::Bitmap1, x, y, "Hello World",
	                  hb::shared::text::TextStyle::with_integrated_shadow(hb::shared::render::Color(255, 200, 100)));
}

void Screen_Test::New_DrawText_Bitmap2_DropShadow(int x, int y)
{
	hb::shared::text::draw_text(GameFont::Bitmap2, x, y, "Hello World",
	                  hb::shared::text::TextStyle::with_drop_shadow(hb::shared::render::Color(100, 200, 255)));
}

void Screen_Test::New_DrawText_Bitmap2_Transparent(int x, int y)
{
	hb::shared::text::draw_text(GameFont::Bitmap2, x, y, "Transparent",
	                  hb::shared::text::TextStyle::transparent(hb::shared::render::Color(255, 255, 255), 0.7f));
}

void Screen_Test::New_DrawText_Numbers(int x, int y)
{
	// With drop shadow like the legacy version
	hb::shared::text::draw_text(GameFont::Numbers, x, y, "12345",
	                  hb::shared::text::TextStyle::with_drop_shadow(hb::shared::render::Color(255, 255, 255)));
}

// ============== Alignment Showcase ==============

void Screen_Test::draw_rect_outline(int x, int y, int width, int height, uint8_t r, uint8_t g, uint8_t b)
{
	m_game->m_Renderer->draw_rect_outline(x, y, width, height, hb::shared::render::Color(r, g, b));
}

void Screen_Test::render_alignment_showcase()
{
	// Title
	hb::shared::text::draw_text(GameFont::Default, 180, 70, "draw_text_aligned with hb::shared::geometry::GameRectangle", hb::shared::text::TextStyle::from_color(GameColors::UITopMsgYellow));

	// Grid layout: 3 columns (Left, Center, Right) x 3 rows (Top, Middle, Bottom)
	constexpr int CELL_WIDTH = 180;
	constexpr int CELL_HEIGHT = 50;
	const int GRID_X = 50;
	const int GRID_Y = 100;
	constexpr int CELL_SPACING = 10;

	// Column labels
	const char* hLabels[] = { "Left", "Center", "Right" };
	const char* vLabels[] = { "Top", "Middle", "Bottom" };

	// draw column headers
	for (int col = 0; col < 3; col++)
	{
		int x = GRID_X + col * (CELL_WIDTH + CELL_SPACING) + CELL_WIDTH / 2 - 20;
		hb::shared::text::draw_text(GameFont::Default, x, GRID_Y - 20, hLabels[col], hb::shared::text::TextStyle::from_color(GameColors::UIDisabled));
	}

	// draw row labels
	for (int row = 0; row < 3; row++)
	{
		int y = GRID_Y + row * (CELL_HEIGHT + CELL_SPACING) + CELL_HEIGHT / 2 - 8;
		hb::shared::text::draw_text(GameFont::Default, GRID_X - 60, y, vLabels[row], hb::shared::text::TextStyle::from_color(GameColors::UIDisabled));
	}

	// Alignment combinations (use bitwise OR to combine)
	hb::shared::text::Align alignments[3][3] = {
		{ hb::shared::text::Align::TopLeft,    hb::shared::text::Align::TopCenter,    hb::shared::text::Align::TopRight },
		{ hb::shared::text::Align::MiddleLeft, hb::shared::text::Align::Center,       hb::shared::text::Align::MiddleRight },
		{ hb::shared::text::Align::BottomLeft, hb::shared::text::Align::BottomCenter, hb::shared::text::Align::BottomRight }
	};

	// draw 3x3 grid of alignment examples
	for (int row = 0; row < 3; row++)
	{
		for (int col = 0; col < 3; col++)
		{
			int cellX = GRID_X + col * (CELL_WIDTH + CELL_SPACING);
			int cellY = GRID_Y + row * (CELL_HEIGHT + CELL_SPACING);

			// draw rectangle outline to show bounds
			draw_rect_outline(cellX, cellY, CELL_WIDTH, CELL_HEIGHT, 100, 100, 100);

			// Create hb::shared::geometry::GameRectangle and draw aligned text
			hb::shared::geometry::GameRectangle rect(cellX, cellY, CELL_WIDTH, CELL_HEIGHT);
			hb::shared::text::draw_text_aligned(GameFont::Default, rect, "Text",
			                         hb::shared::text::TextStyle::from_color(GameColors::UIWhite),
			                         alignments[row][col]);
		}
	}

	// Show code example
	int exampleY = GRID_Y + 3 * (CELL_HEIGHT + CELL_SPACING) + 20;
	hb::shared::text::draw_text(GameFont::Default, GRID_X, exampleY, "Usage: hb::shared::geometry::GameRectangle rect(x, y, width, height);", hb::shared::text::TextStyle::from_color(hb::shared::render::Color(150, 200, 150)));
	hb::shared::text::draw_text(GameFont::Default, GRID_X, exampleY + 16, "       hb::shared::text::draw_text_aligned(fontId, rect, text, style, hAlign, vAlign);", hb::shared::text::TextStyle::from_color(hb::shared::render::Color(150, 200, 150)));
}
