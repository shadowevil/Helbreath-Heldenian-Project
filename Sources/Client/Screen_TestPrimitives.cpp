// Screen_TestPrimitives.cpp: Visual Test Scene for Primitive Drawing
//
// Renders labeled rows exercising every hb::shared::render::IRenderer primitive method.
// Layout adapts to logical resolution (640x480 or 800x600).
//
//////////////////////////////////////////////////////////////////////

#include "Screen_TestPrimitives.h"
#include "Game.h"
#include "GameModeManager.h"
#include "GameFonts.h"
#include "TextLibExt.h"
#include "IInput.h"
#include "ResolutionConfig.h"
#include <cmath>

Screen_TestPrimitives::Screen_TestPrimitives(CGame* game)
	: IGameScreen(game)
{
}

void Screen_TestPrimitives::on_initialize()
{
}
void Screen_TestPrimitives::on_update()
{
	if (hb::shared::input::get() && hb::shared::input::get()->is_key_pressed(KeyCode::Escape))
	{
		hb::shared::render::Window::close();
		return;
	}
}

void Screen_TestPrimitives::on_render()
{
	auto& res = hb::shared::render::ResolutionConfig::get();
	int W = res.logical_width();
	int H = res.logical_height();

	// Dark background
	m_game->m_Renderer->draw_rect_filled(0, 0, W, H, hb::shared::render::Color(20, 20, 30));

	hb::shared::text::begin_batch();

	render_header();

	// Distribute 7 rows evenly across screen height below header
	int headerH = 30;
	int rowH = (H - headerH) / 7;

	int rowStarts[7];
	for (int i = 0; i < 7; i++)
		rowStarts[i] = headerH + rowH * i;

	// draw divider lines between rows
	for (int i = 1; i < 7; i++)
	{
		int divY = rowStarts[i] - 1;
		m_game->m_Renderer->draw_line(10, divY, W - 10, divY, hb::shared::render::Color(50, 50, 70), hb::shared::render::BlendMode::Alpha);
	}

	render_pixel_tests(rowStarts[0]);
	render_line_alpha_tests(rowStarts[1]);
	render_line_additive_tests(rowStarts[2]);
	render_rect_filled_tests(rowStarts[3]);
	render_rect_outline_tests(rowStarts[4]);
	render_rounded_rect_filled_tests(rowStarts[5]);
	render_rounded_rect_outline_tests(rowStarts[6]);

	hb::shared::text::end_batch();
}

void Screen_TestPrimitives::render_header()
{
	auto& res = hb::shared::render::ResolutionConfig::get();
	int W = res.logical_width();

	hb::shared::text::draw_text(GameFont::Default, W / 2 - 100, 5, "PRIMITIVE RENDERING TEST",
		hb::shared::text::TextStyle::from_color(hb::shared::render::Color(255, 220, 100)));
	hb::shared::text::draw_text(GameFont::Default, W - 80, 5, "ESC=Quit",
		hb::shared::text::TextStyle::from_color(hb::shared::render::Color(160, 160, 160)));
}

// Row 1: draw_pixel
void Screen_TestPrimitives::render_pixel_tests(int y)
{
	auto& res = hb::shared::render::ResolutionConfig::get();
	int W = res.logical_width();
	int rowH = (res.logical_height() - 30) / 7;

	hb::shared::text::draw_text(GameFont::Default, 10, y + 2, "draw_pixel",
		hb::shared::text::TextStyle::from_color(hb::shared::render::Color(180, 180, 255)));

	// Section 1: Cross pattern (left third)
	int s1 = W / 6;
	int cy = y + rowH / 2;
	m_game->m_Renderer->draw_pixel(s1, cy - 2, hb::shared::render::Color::White());
	m_game->m_Renderer->draw_pixel(s1 - 2, cy, hb::shared::render::Color::White());
	m_game->m_Renderer->draw_pixel(s1, cy, hb::shared::render::Color::White());
	m_game->m_Renderer->draw_pixel(s1 + 2, cy, hb::shared::render::Color::White());
	m_game->m_Renderer->draw_pixel(s1, cy + 2, hb::shared::render::Color::White());

	// Section 2: Colored pixels (middle third)
	int s2 = W / 3;
	int spacing = W / 20;
	hb::shared::render::Color pixColors[] = {
		{255, 0, 0}, {0, 255, 0}, {0, 0, 255},
		{255, 255, 0}, {0, 255, 255}, {255, 0, 255}
	};
	for (int i = 0; i < 6; i++)
	{
		int px = s2 + i * spacing;
		// 3x3 block so it's visible
		for (int dy = -1; dy <= 1; dy++)
			for (int dx = -1; dx <= 1; dx++)
				m_game->m_Renderer->draw_pixel(px + dx, cy + dy, pixColors[i]);
	}

	// Section 3: Alpha pixels (right third)
	int s3 = W * 2 / 3;
	int alphaSpacing = W / 10;
	hb::shared::text::draw_text(GameFont::Default, s3, y + 2, "Alpha: 255  192  128  64",
		hb::shared::text::TextStyle::from_color(hb::shared::render::Color(120, 120, 120)));
	for (int i = 0; i < 4; i++)
	{
		int px = s3 + i * alphaSpacing;
		uint8_t a = static_cast<uint8_t>(255 - i * 63);
		for (int dy = -2; dy <= 2; dy++)
			for (int dx = -2; dx <= 2; dx++)
				m_game->m_Renderer->draw_pixel(px + dx, cy + dy, hb::shared::render::Color(255, 255, 255, a));
	}
}

// Row 2: draw_line — Alpha Blend
void Screen_TestPrimitives::render_line_alpha_tests(int y)
{
	auto& res = hb::shared::render::ResolutionConfig::get();
	int W = res.logical_width();
	int rowH = (res.logical_height() - 30) / 7;
	int contentH = rowH - 16;

	hb::shared::text::draw_text(GameFont::Default, 10, y + 2, "draw_line (Alpha)",
		hb::shared::text::TextStyle::from_color(hb::shared::render::Color(180, 180, 255)));

	// Section 1: Fan of 8 lines (left third)
	int cx = W / 6, cy = y + rowH / 2 + 4;
	int fanR = contentH / 2;
	hb::shared::render::Color fanColors[] = {
		{255, 0, 0}, {255, 128, 0}, {255, 255, 0}, {0, 255, 0},
		{0, 255, 255}, {0, 128, 255}, {128, 0, 255}, {255, 0, 255}
	};
	for (int i = 0; i < 8; i++)
	{
		float angle = (i * 3.14159f * 2.0f) / 8.0f;
		int ex = cx + static_cast<int>(fanR * 1.5f * cosf(angle));
		int ey = cy + static_cast<int>(fanR * sinf(angle));
		m_game->m_Renderer->draw_line(cx, cy, ex, ey, fanColors[i], hb::shared::render::BlendMode::Alpha);
	}

	// Section 2: Lines with varying alpha (middle third)
	int lx = W / 3;
	int lineW = W / 5;
	hb::shared::text::draw_text(GameFont::Default, lx, y + 2, "Alpha: 255  192  128  64",
		hb::shared::text::TextStyle::from_color(hb::shared::render::Color(120, 120, 120)));
	int lineSpacing = contentH / 4;
	m_game->m_Renderer->draw_line(lx, y + 16 + lineSpacing * 0, lx + lineW, y + 16 + lineSpacing * 0, hb::shared::render::Color(255, 255, 0, 255), hb::shared::render::BlendMode::Alpha);
	m_game->m_Renderer->draw_line(lx, y + 16 + lineSpacing * 1, lx + lineW, y + 16 + lineSpacing * 1, hb::shared::render::Color(255, 255, 0, 192), hb::shared::render::BlendMode::Alpha);
	m_game->m_Renderer->draw_line(lx, y + 16 + lineSpacing * 2, lx + lineW, y + 16 + lineSpacing * 2, hb::shared::render::Color(255, 255, 0, 128), hb::shared::render::BlendMode::Alpha);
	m_game->m_Renderer->draw_line(lx, y + 16 + lineSpacing * 3, lx + lineW, y + 16 + lineSpacing * 3, hb::shared::render::Color(255, 255, 0, 64), hb::shared::render::BlendMode::Alpha);

	// Section 3: Diagonal and vertical lines (right third)
	int dx = W * 2 / 3;
	hb::shared::text::draw_text(GameFont::Default, dx, y + 2, "Diagonal / Vertical",
		hb::shared::text::TextStyle::from_color(hb::shared::render::Color(120, 120, 120)));
	m_game->m_Renderer->draw_line(dx, y + 16, dx + contentH, y + 16 + contentH, hb::shared::render::Color(0, 255, 128), hb::shared::render::BlendMode::Alpha);
	int midX = dx + contentH + 20;
	m_game->m_Renderer->draw_line(midX, y + 16, midX, y + 16 + contentH, hb::shared::render::Color(255, 128, 0), hb::shared::render::BlendMode::Alpha);
	m_game->m_Renderer->draw_line(midX + 20, y + 16 + contentH, midX + 20 + contentH, y + 16, hb::shared::render::Color(128, 128, 255), hb::shared::render::BlendMode::Alpha);
}

// Row 3: draw_line — Additive Blend
void Screen_TestPrimitives::render_line_additive_tests(int y)
{
	auto& res = hb::shared::render::ResolutionConfig::get();
	int W = res.logical_width();
	int rowH = (res.logical_height() - 30) / 7;
	int contentH = rowH - 16;

	hb::shared::text::draw_text(GameFont::Default, 10, y + 2, "draw_line (Additive)",
		hb::shared::text::TextStyle::from_color(hb::shared::render::Color(180, 180, 255)));

	// Section 1: Fan with additive blending (left third)
	int cx = W / 6, cy = y + rowH / 2 + 4;
	int fanR = contentH / 2;
	hb::shared::render::Color fanColors[] = {
		{255, 0, 0}, {255, 128, 0}, {255, 255, 0}, {0, 255, 0},
		{0, 255, 255}, {0, 128, 255}, {128, 0, 255}, {255, 0, 255}
	};
	for (int i = 0; i < 8; i++)
	{
		float angle = (i * 3.14159f * 2.0f) / 8.0f;
		int ex = cx + static_cast<int>(fanR * 1.5f * cosf(angle));
		int ey = cy + static_cast<int>(fanR * sinf(angle));
		m_game->m_Renderer->draw_line(cx, cy, ex, ey, fanColors[i], hb::shared::render::BlendMode::Additive);
	}

	// Section 2: Overlapping lines (middle third)
	int lx = W / 3;
	int lineW = W / 5;
	hb::shared::text::draw_text(GameFont::Default, lx, y + 2, "Overlapping (accumulation)",
		hb::shared::text::TextStyle::from_color(hb::shared::render::Color(120, 120, 120)));
	for (int i = 0; i < 6; i++)
	{
		m_game->m_Renderer->draw_line(lx, y + 16 + i * (contentH / 6), lx + lineW, y + 16 + contentH - i * (contentH / 12),
			hb::shared::render::Color(80, 80, 120), hb::shared::render::BlendMode::Additive);
	}
	for (int i = 0; i < 6; i++)
	{
		m_game->m_Renderer->draw_line(lx + lineW, y + 16 + i * (contentH / 6), lx, y + 16 + contentH - i * (contentH / 12),
			hb::shared::render::Color(120, 80, 80), hb::shared::render::BlendMode::Additive);
	}

	// Section 3: RGB additive overlap (right third)
	int rx = W * 2 / 3;
	int rgbW = W / 5;
	hb::shared::text::draw_text(GameFont::Default, rx, y + 2, "RGB overlap",
		hb::shared::text::TextStyle::from_color(hb::shared::render::Color(120, 120, 120)));
	for (int i = 0; i < 5; i++)
	{
		m_game->m_Renderer->draw_line(rx, y + 16 + contentH / 4 + i * 2, rx + rgbW, y + 16 + contentH / 2 + i * 2,
			hb::shared::render::Color(200, 0, 0), hb::shared::render::BlendMode::Additive);
		m_game->m_Renderer->draw_line(rx, y + 16 + contentH * 3 / 4 + i * 2, rx + rgbW, y + 16 + contentH / 2 + i * 2,
			hb::shared::render::Color(0, 200, 0), hb::shared::render::BlendMode::Additive);
		m_game->m_Renderer->draw_line(rx + rgbW / 3, y + 16 + i * 2, rx + rgbW * 2 / 3, y + 16 + contentH + i * 2,
			hb::shared::render::Color(0, 0, 200), hb::shared::render::BlendMode::Additive);
	}
}

// Row 4: draw_rect_filled
void Screen_TestPrimitives::render_rect_filled_tests(int y)
{
	auto& res = hb::shared::render::ResolutionConfig::get();
	int W = res.logical_width();
	int rowH = (res.logical_height() - 30) / 7;
	int contentH = rowH - 16;
	int rectH = contentH - 14;

	hb::shared::text::draw_text(GameFont::Default, 10, y + 2, "draw_rect_filled",
		hb::shared::text::TextStyle::from_color(hb::shared::render::Color(180, 180, 255)));

	// Section 1: Alpha blending over colored strip (left third)
	int s1 = W / 20;
	int rectW = (W / 3 - s1 - 20) / 4;
	int stripW = rectW * 4 + 30;
	m_game->m_Renderer->draw_rect_filled(s1, y + 16, stripW, rectH, hb::shared::render::Color(100, 60, 60));
	for (int i = 0; i < 4; i++)
	{
		uint8_t a = static_cast<uint8_t>(255 - i * 63);
		m_game->m_Renderer->draw_rect_filled(s1 + i * (rectW + 4), y + 16, rectW, rectH, hb::shared::render::Color(0, 0, 0, a));
	}
	hb::shared::text::draw_text(GameFont::Default, s1, y + rowH - 14, "a=255  a=192  a=128  a=64",
		hb::shared::text::TextStyle::from_color(hb::shared::render::Color(120, 120, 120)));

	// Section 2: 4 colored filled rects (middle third)
	int s2 = W / 3 + 10;
	int rect_w = (W / 3 - 40) / 4;
	m_game->m_Renderer->draw_rect_filled(s2, y + 16, rect_w, rectH, hb::shared::render::Color(255, 0, 0));
	m_game->m_Renderer->draw_rect_filled(s2 + rect_w + 6, y + 16, rect_w, rectH, hb::shared::render::Color(0, 255, 0));
	m_game->m_Renderer->draw_rect_filled(s2 + (rect_w + 6) * 2, y + 16, rect_w, rectH, hb::shared::render::Color(0, 0, 255));
	m_game->m_Renderer->draw_rect_filled(s2 + (rect_w + 6) * 3, y + 16, rect_w, rectH, hb::shared::render::Color(255, 255, 0));

	// Section 3: Semi-transparent overlap (right third)
	int s3 = W * 2 / 3 + 10;
	int oRectW = W / 8;
	m_game->m_Renderer->draw_rect_filled(s3, y + 16, oRectW, rectH, hb::shared::render::Color(255, 0, 0));
	m_game->m_Renderer->draw_rect_filled(s3 + oRectW / 2, y + 16 + rectH / 4, oRectW, rectH, hb::shared::render::Color(0, 0, 255, 128));
	hb::shared::text::draw_text(GameFont::Default, s3, y + rowH - 14, "alpha overlap",
		hb::shared::text::TextStyle::from_color(hb::shared::render::Color(120, 120, 120)));
}

// Row 5: draw_rect_outline
void Screen_TestPrimitives::render_rect_outline_tests(int y)
{
	auto& res = hb::shared::render::ResolutionConfig::get();
	int W = res.logical_width();
	int rowH = (res.logical_height() - 30) / 7;
	int contentH = rowH - 16;
	int rectH = contentH - 14;

	hb::shared::text::draw_text(GameFont::Default, 10, y + 2, "draw_rect_outline",
		hb::shared::text::TextStyle::from_color(hb::shared::render::Color(180, 180, 255)));

	// Section 1: Three thicknesses (left quarter)
	int spacing = W / 12;
	int rectW = spacing - 10;
	int x0 = W / 20;
	m_game->m_Renderer->draw_rect_outline(x0, y + 16, rectW, rectH, hb::shared::render::Color::White(), 1);
	hb::shared::text::draw_text(GameFont::Default, x0, y + rowH - 14, "t=1",
		hb::shared::text::TextStyle::from_color(hb::shared::render::Color(120, 120, 120)));

	m_game->m_Renderer->draw_rect_outline(x0 + spacing, y + 16, rectW, rectH, hb::shared::render::Color(0, 255, 0), 2);
	hb::shared::text::draw_text(GameFont::Default, x0 + spacing, y + rowH - 14, "t=2",
		hb::shared::text::TextStyle::from_color(hb::shared::render::Color(120, 120, 120)));

	m_game->m_Renderer->draw_rect_outline(x0 + spacing * 2, y + 16, rectW, rectH, hb::shared::render::Color(100, 150, 255), 3);
	hb::shared::text::draw_text(GameFont::Default, x0 + spacing * 2, y + rowH - 14, "t=3",
		hb::shared::text::TextStyle::from_color(hb::shared::render::Color(120, 120, 120)));

	// Section 2: Nested outlines (middle)
	int nx = W / 3 + W / 8;
	int nw = W / 6;
	m_game->m_Renderer->draw_rect_outline(nx, y + 16, nw, rectH, hb::shared::render::Color(255, 100, 100), 1);
	m_game->m_Renderer->draw_rect_outline(nx + nw / 6, y + 16 + rectH / 5, nw * 2 / 3, rectH * 3 / 5, hb::shared::render::Color(100, 255, 100), 1);
	m_game->m_Renderer->draw_rect_outline(nx + nw / 3, y + 16 + rectH * 2 / 5, nw / 3, rectH / 5, hb::shared::render::Color(100, 100, 255), 1);
	hb::shared::text::draw_text(GameFont::Default, nx, y + rowH - 14, "nested",
		hb::shared::text::TextStyle::from_color(hb::shared::render::Color(120, 120, 120)));

	// Section 3: Alpha overlap (right third)
	int ax = W * 2 / 3 + 10;
	int aRectW = W / 7;
	m_game->m_Renderer->draw_rect_outline(ax, y + 16, aRectW, rectH, hb::shared::render::Color(255, 255, 0, 255), 2);
	m_game->m_Renderer->draw_rect_outline(ax + aRectW / 3, y + 16 + rectH / 6, aRectW, rectH, hb::shared::render::Color(255, 255, 0, 128), 2);
	hb::shared::text::draw_text(GameFont::Default, ax, y + rowH - 14, "alpha overlap",
		hb::shared::text::TextStyle::from_color(hb::shared::render::Color(120, 120, 120)));
}

// Row 6: draw_rounded_rect_filled
void Screen_TestPrimitives::render_rounded_rect_filled_tests(int y)
{
	auto& res = hb::shared::render::ResolutionConfig::get();
	int W = res.logical_width();
	int rowH = (res.logical_height() - 30) / 7;
	int contentH = rowH - 16;
	int rectH = contentH - 14;

	hb::shared::text::draw_text(GameFont::Default, 10, y + 2, "draw_rounded_rect_filled",
		hb::shared::text::TextStyle::from_color(hb::shared::render::Color(180, 180, 255)));

	// Section 1: Varying radii (left 2/3)
	int spacing = W / 7;
	int rectW = spacing - 16;
	int x0 = W / 20;
	int radii[] = { 0, 5, 10, 17 };
	const char* labels[] = { "r=0", "r=5", "r=10", "r=17" };
	for (int i = 0; i < 4; i++)
	{
		int rx = x0 + i * spacing;
		m_game->m_Renderer->draw_rounded_rect_filled(rx, y + 16, rectW, rectH, radii[i], hb::shared::render::Color(200, 100, 50));
		hb::shared::text::draw_text(GameFont::Default, rx, y + rowH - 14, labels[i],
			hb::shared::text::TextStyle::from_color(hb::shared::render::Color(120, 120, 120)));
	}

	// Section 2: Alpha overlap (right third)
	int ax = W * 2 / 3 + 10;
	int oRectW = W / 6;
	m_game->m_Renderer->draw_rounded_rect_filled(ax, y + 16, oRectW, rectH, 10, hb::shared::render::Color(0, 200, 255));
	m_game->m_Renderer->draw_rounded_rect_filled(ax + oRectW / 3, y + 16 + rectH / 4, oRectW, rectH, 10, hb::shared::render::Color(255, 100, 0, 128));
	hb::shared::text::draw_text(GameFont::Default, ax, y + rowH - 14, "alpha overlap",
		hb::shared::text::TextStyle::from_color(hb::shared::render::Color(120, 120, 120)));
}

// Row 7: draw_rounded_rect_outline
void Screen_TestPrimitives::render_rounded_rect_outline_tests(int y)
{
	auto& res = hb::shared::render::ResolutionConfig::get();
	int W = res.logical_width();
	int rowH = (res.logical_height() - 30) / 7;
	int contentH = rowH - 16;
	int rectH = contentH - 14;

	hb::shared::text::draw_text(GameFont::Default, 10, y + 2, "draw_rounded_rect_outline",
		hb::shared::text::TextStyle::from_color(hb::shared::render::Color(180, 180, 255)));

	// Section 1: Varying radius and thickness (left 2/3)
	int spacing = W / 6;
	int rectW = spacing - 16;
	int x0 = W / 20;

	m_game->m_Renderer->draw_rounded_rect_outline(x0, y + 16, rectW, rectH, 10, hb::shared::render::Color(0, 255, 200), 1);
	hb::shared::text::draw_text(GameFont::Default, x0, y + rowH - 14, "r=10 t=1",
		hb::shared::text::TextStyle::from_color(hb::shared::render::Color(120, 120, 120)));

	m_game->m_Renderer->draw_rounded_rect_outline(x0 + spacing, y + 16, rectW, rectH, 10, hb::shared::render::Color(0, 255, 200), 2);
	hb::shared::text::draw_text(GameFont::Default, x0 + spacing, y + rowH - 14, "r=10 t=2",
		hb::shared::text::TextStyle::from_color(hb::shared::render::Color(120, 120, 120)));

	m_game->m_Renderer->draw_rounded_rect_outline(x0 + spacing * 2, y + 16, rectW, rectH, 15, hb::shared::render::Color(0, 255, 200), 3);
	hb::shared::text::draw_text(GameFont::Default, x0 + spacing * 2, y + rowH - 14, "r=15 t=3",
		hb::shared::text::TextStyle::from_color(hb::shared::render::Color(120, 120, 120)));

	// Section 2: RGB color variants (right third)
	int cx = W * 2 / 3 + 10;
	int rect_w = W / 8;
	int cSpacing = rect_w / 2;
	m_game->m_Renderer->draw_rounded_rect_outline(cx, y + 16, rect_w, rectH, 12, hb::shared::render::Color(255, 100, 100), 2);
	m_game->m_Renderer->draw_rounded_rect_outline(cx + cSpacing, y + 16, rect_w, rectH, 12, hb::shared::render::Color(100, 255, 100), 2);
	m_game->m_Renderer->draw_rounded_rect_outline(cx + cSpacing * 2, y + 16, rect_w, rectH, 12, hb::shared::render::Color(100, 100, 255), 2);
}
