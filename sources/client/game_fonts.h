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
		// TTF Font (loaded from FONTS/default.ttf)
		Default = hb::shared::text::FONT_ID_DEFAULT,

		// Bitmap Fonts (loaded during Screen_Loading)
		Bitmap1 = 1,        // INTERFACE_FONT1 - main bitmap font
		Bitmap2 = 2,        // INTERFACE_FONT2 - secondary bitmap font
		Numbers = 3,        // Number font from INTERFACE_ADDINTERFACE frames 6-15
		SprFont3_0 = 4,     // SPRFONTS2 variant 0 (small)
		SprFont3_1 = 5,     // SPRFONTS2 variant 1 (medium)
		SprFont3_2 = 6,     // SPRFONTS2 variant 2 (large)
	};

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
				// Bitmap2, SprFont3_* use dynamic spacing
				spacing.useDynamicSpacing = true;
				break;
		}

		return spacing;
	}
}
