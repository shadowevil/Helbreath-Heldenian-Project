// game_fonts.h: Game-specific font identifiers and spacing data
//
// Defines the font IDs used by the game. These IDs are passed to TextLib
// functions. TextLib owns the font objects, game owns the ID definitions.
//////////////////////////////////////////////////////////////////////

#pragma once

#include "text_lib.h"

// Game-specific font identifiers
// Using plain enum (not enum class) for implicit conversion to int
namespace GameFont
{
	enum ID : int
	{
		// TTF Font (hb::shared::text::bundled_font_path, Wine Tahoma)
		Default = hb::shared::text::FONT_ID_DEFAULT,

		// Bitmap Fonts (loaded during Screen_Loading)
		Bitmap1 = 1,        // INTERFACE_FONT1 - main bitmap font
		Bitmap2 = 2,        // INTERFACE_FONT2 - secondary bitmap font
		Numbers = 3,        // Number font from INTERFACE_ADDINTERFACE frames 6-15
		// Pixel fonts: TTF recreations of the old sprite_fonts sheet 003 (loaded during Screen_Loading)
		SprFont3_0 = 4,     // large, fonts/deathblow.ttf
		SprFont3_1 = 5,     // medium, fonts/woundcut.ttf
		SprFont3_2 = 6,     // small, fonts/scratchmark.ttf
	};

	// A pixel font's face and the bitmap layout it keeps: 16 px design grid, the baseline below the
	// draw position, and the 5 px space of the old dynamic bitmap fonts
	struct pixel_font_spec
	{
		ID id;
		const char* path;
		int baseline;
	};
	constexpr int pixel_font_size = 16;
	constexpr int pixel_font_space = 5;
	inline constexpr pixel_font_spec pixel_fonts[] = {
		{SprFont3_0, "fonts/deathblow.ttf", 11},
		{SprFont3_1, "fonts/woundcut.ttf", 11},
		{SprFont3_2, "fonts/scratchmark.ttf", 9},
	};

	// Register every pixel font with TextLib; the path of the first that fails to load, or nullptr
	inline const char* load_pixel_fonts()
	{
		const char* failed = nullptr;
		for (const pixel_font_spec& spec : pixel_fonts)
		{
			if (!hb::shared::text::load_pixel_font(spec.id, spec.path, pixel_font_size, spec.baseline, pixel_font_space)
				&& !failed)
				failed = spec.path;
		}
		return failed;
	}

	// Font spacing data for Bitmap1 ('!' to 'z')
	inline const int Bitmap1Widths[] = {
		8,8,8,8,8,8,8,8,8,8, 8,8,8,8,8, 8,6,8,7,8,8,9,10,9,7, 8,8,8,8,8, 8,8,
		15,16,12,17,14,15,14,16,10,13, 19,10,17,17,15,14,15,16,13,17, 16,16,20,17,16,14,
		8,8,8,8,8,8, 8,6,7,8,7,7,7,7,4,7,7, 4,11,7,8,8,7,8,6,5,8,9,14,8,9,8, 8,8,8,8,
		8,8,8,8,8,8,8
	};

	// Font spacing data for Numbers ('0' to '9')
	inline const int NumbersWidths[] = { 6, 4, 6, 6, 6, 6, 6, 6, 6, 6 };

	// get FontSpacing for a given font ID
	// Returns spacing data for fonts that need explicit widths
	// Returns empty spacing (useDynamicSpacing=true) for dynamic-width fonts
	inline hb::shared::text::FontSpacing GetFontSpacing(ID fontId)
	{
		hb::shared::text::FontSpacing spacing;

		switch (fontId)
		{
			case Bitmap1:
				spacing.charWidths.assign(Bitmap1Widths,
					Bitmap1Widths + sizeof(Bitmap1Widths) / sizeof(Bitmap1Widths[0]));
				spacing.defaultWidth = 5;
				break;

			case Numbers:
				spacing.charWidths.assign(NumbersWidths,
					NumbersWidths + sizeof(NumbersWidths) / sizeof(NumbersWidths[0]));
				spacing.defaultWidth = 6;
				break;

			default:
				// Bitmap2 uses dynamic spacing
				spacing.useDynamicSpacing = true;
				break;
		}

		return spacing;
	}
}
