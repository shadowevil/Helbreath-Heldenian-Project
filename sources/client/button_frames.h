// button_frames.h: The size and placement offset of each frame of the old Button sheet (game_dialogs sprite 10)
//
// Text buttons now draw from the panel kit (CGame::draw_text_button); this table keeps the geometry the sheet's
// frames had, so every button draws at the same place and size and every hit area is unchanged. A frame is drawn
// at its caller's (x, y) plus (pivot_x, pivot_y). Generated from the sheet's manifest (HH-0097).
//////////////////////////////////////////////////////////////////////

#pragma once

#include <array>
#include <cstddef>

namespace hb::client::button_frame
{
	struct rect
	{
		int width;
		int height;
		int pivotX;
		int pivotY;
	};

	// Frames that are not text buttons but are measured by dialogs (the system menu's tabs, value boxes and slider)
	constexpr int tab = 70;          // the first tab frame
	constexpr int box_wide = 78;     // value box, wide
	constexpr int box_small = 79;    // value box, small
	constexpr int slider_track = 80;
	constexpr int box_large = 81;    // value box, large

	constexpr std::array<rect, 83> rects{{
	{74, 20, 0, 0},   // 0
	{74, 20, 0, 0},   // 1
	{74, 20, 0, 0},   // 2
	{74, 20, 0, 0},   // 3
	{74, 20, 0, 0},   // 4
	{74, 20, 0, 0},   // 5
	{74, 20, 0, 0},   // 6
	{74, 20, 0, 0},   // 7
	{74, 20, 0, 0},   // 8
	{74, 20, 0, 0},   // 9
	{74, 20, 0, 0},   // 10
	{74, 20, 0, 0},   // 11
	{74, 20, 0, 0},   // 12
	{74, 20, 0, 0},   // 13
	{74, 20, 0, 0},   // 14
	{74, 20, 0, 0},   // 15
	{74, 20, 0, 0},   // 16
	{74, 20, 0, 0},   // 17
	{74, 20, 0, 0},   // 18
	{74, 20, 0, 0},   // 19
	{74, 20, 0, 0},   // 20
	{74, 20, 0, 0},   // 21
	{74, 20, 0, 0},   // 22
	{74, 20, 0, 0},   // 23
	{74, 20, 0, 0},   // 24
	{74, 20, 0, 0},   // 25
	{74, 20, 0, 0},   // 26
	{74, 20, 0, 0},   // 27
	{74, 20, 0, 0},   // 28
	{74, 20, 0, 0},   // 29
	{74, 20, 0, 0},   // 30
	{74, 20, 0, 0},   // 31
	{74, 20, 0, 0},   // 32
	{74, 20, 0, 0},   // 33
	{74, 20, 0, 0},   // 34
	{74, 20, 0, 0},   // 35
	{74, 20, 0, 0},   // 36
	{74, 20, 0, 0},   // 37
	{74, 20, 0, 0},   // 38
	{74, 20, 0, 0},   // 39
	{74, 20, 0, 0},   // 40
	{74, 20, 0, 0},   // 41
	{74, 20, 0, 0},   // 42
	{74, 20, 0, 0},   // 43
	{74, 20, 0, 0},   // 44
	{74, 20, 0, 0},   // 45
	{74, 20, 0, 0},   // 46
	{74, 20, 0, 0},   // 47
	{74, 20, 0, 0},   // 48
	{74, 20, 0, 0},   // 49
	{171, 30, 230, 7},   // 50
	{176, 24, 364, 288},   // 51
	{176, 24, 364, 318},   // 52
	{176, 24, 364, 348},   // 53
	{176, 24, 364, 378},   // 54
	{176, 24, 364, 408},   // 55
	{176, 24, 364, 288},   // 56
	{176, 24, 364, 318},   // 57
	{176, 24, 364, 348},   // 58
	{176, 24, 364, 378},   // 59
	{176, 24, 364, 408},   // 60
	{107, 187, 0, 0},   // 61
	{107, 187, 0, 0},   // 62
	{74, 20, 0, 0},   // 63
	{74, 20, 0, 0},   // 64
	{74, 20, 0, 0},   // 65
	{74, 20, 0, 0},   // 66
	{74, 20, 0, 0},   // 67
	{74, 20, 0, 0},   // 68
	{255, 31, 192, 7},   // 69
	{74, 20, 0, 0},   // 70
	{74, 20, 0, 0},   // 71
	{74, 20, 0, 0},   // 72
	{74, 20, 0, 0},   // 73
	{74, 20, 0, 0},   // 74
	{74, 20, 0, 0},   // 75
	{74, 20, 0, 0},   // 76
	{74, 20, 0, 0},   // 77
	{117, 16, 0, 0},   // 78
	{32, 16, 0, 0},   // 79
	{102, 3, 0, 0},   // 80
	{160, 16, 0, 0},   // 81
	{117, 63, 0, 0},   // 82
	}};

	constexpr rect rect_of(int frame)
	{
		return (frame >= 0 && static_cast<std::size_t>(frame) < rects.size()) ? rects[static_cast<std::size_t>(frame)] : rect{0, 0, 0, 0};
	}
}
