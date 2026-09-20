#include "DialogBox_SysMenu.h"
#include "Game.h"
#include "ChatManager.h"
#include "GlobalDef.h"
#include "lan_eng.h"
#include "AudioManager.h"
#include "ConfigManager.h"
#include "IInput.h"
#include "RendererFactory.h"
#include "ITextRenderer.h"
#include <chrono>
#include <cstring>
#include <ctime>
#include <format>
#include <string>
#include "Screen_OnGame.h"
using namespace hb::client::sprite_id;

// Content area constants
static const int CONTENT_X = 21;
static const int CONTENT_Y = 57;
static const int CONTENT_WIDTH = 297;
static const int CONTENT_HEIGHT = 234;

// Graphics tab scroll
static constexpr int GRAPHICS_LINE_HEIGHT = 18;
static constexpr int GRAPHICS_VISIBLE_ITEMS = 12;
static bool s_bDraggingGraphicsScroll = false;

// Slider tracking
static bool s_bDraggingMasterSlider = false;
static bool s_bDraggingEffectsSlider = false;
static bool s_bDraggingAmbientSlider = false;
static bool s_bDraggingUISlider = false;
static bool s_bDraggingMusicSlider = false;

// 4:3 resolutions from 640x480 to 1920x1440
const Resolution DialogBox_SysMenu::s_Resolutions[] = {
	//{ 640, 480 },
	{ 800, 600 },
	{ 1024, 768 },
	{ 1280, 960 },
	{ 1440, 1080 },
	{ 1920, 1440 }
};

const int DialogBox_SysMenu::s_NumResolutions = sizeof(s_Resolutions) / sizeof(s_Resolutions[0]);

int DialogBox_SysMenu::get_current_resolution_index()
{
	int currentWidth = config_manager::get().get_window_width();
	int currentHeight = config_manager::get().get_window_height();

	for (int i = 0; i < s_NumResolutions; i++) {
		if (s_Resolutions[i].width == currentWidth && s_Resolutions[i].height == currentHeight) {
			return i;
		}
	}
	return get_nearest_resolution_index(currentWidth, currentHeight);
}

int DialogBox_SysMenu::get_nearest_resolution_index(int width, int height)
{
	int bestIndex = 0;
	int bestDiff = abs(s_Resolutions[0].width - width) + abs(s_Resolutions[0].height - height);

	for (int i = 1; i < s_NumResolutions; i++) {
		int diff = abs(s_Resolutions[i].width - width) + abs(s_Resolutions[i].height - height);
		if (diff < bestDiff) {
			bestDiff = diff;
			bestIndex = i;
		}
	}
	return bestIndex;
}

void DialogBox_SysMenu::cycle_resolution()
{
	int currentIndex = get_current_resolution_index();
	int nextIndex = (currentIndex + 1) % s_NumResolutions;
	apply_resolution(nextIndex);
}

void DialogBox_SysMenu::apply_resolution(int index)
{
	if (index < 0 || index >= s_NumResolutions) return;

	int newWidth = s_Resolutions[index].width;
	int newHeight = s_Resolutions[index].height;

	config_manager::get().set_window_size(newWidth, newHeight);
	config_manager::get().save();

	hb::shared::render::Window::set_size(newWidth, newHeight, true);

	if (hb::shared::render::Renderer::get())
		hb::shared::render::Renderer::get()->resize_back_buffer(newWidth, newHeight);

	hb::shared::input::get()->set_window_active(true);
}

DialogBox_SysMenu::DialogBox_SysMenu(CGame* game)
	: IDialogBox(DialogBoxId::SystemMenu, game)
	, m_iActiveTab(TAB_GENERAL)
	, m_bFrameSizesInitialized(false)
	, m_iWideBoxWidth(0)
	, m_iWideBoxHeight(0)
	, m_iSmallBoxWidth(0)
	, m_iSmallBoxHeight(0)
	, m_iLargeBoxWidth(0)
	, m_iLargeBoxHeight(0)
{
	set_default_rect(237 , 67 , 331, 303);
}

void DialogBox_SysMenu::on_update()
{
	// Cache frame dimensions on first update (sprites loaded by now)
	if (!m_bFrameSizesInitialized && m_game->m_sprite[InterfaceNdButton] != nullptr)
	{
		hb::shared::sprite::SpriteRect wideRect = m_game->m_sprite[InterfaceNdButton]->GetFrameRect(78);
		hb::shared::sprite::SpriteRect smallRect = m_game->m_sprite[InterfaceNdButton]->GetFrameRect(79);
		hb::shared::sprite::SpriteRect largeRect = m_game->m_sprite[InterfaceNdButton]->GetFrameRect(81);

		// Only use if we got valid dimensions (not from NullSprite)
		if (wideRect.width > 0 && wideRect.height > 0 && smallRect.width > 0 && smallRect.height > 0)
		{
			m_iWideBoxWidth = wideRect.width;
			m_iWideBoxHeight = wideRect.height;
			m_iSmallBoxWidth = smallRect.width;
			m_iSmallBoxHeight = smallRect.height;
			m_iLargeBoxWidth = largeRect.width > 0 ? largeRect.width : 280;
			m_iLargeBoxHeight = largeRect.height > 0 ? largeRect.height : 16;
			m_bFrameSizesInitialized = true;
		}
	}
}

void DialogBox_SysMenu::on_draw()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	short z = static_cast<short>(hb::shared::input::get_mouse_wheel_delta());
	char lb = hb::shared::input::is_mouse_button_down(hb::shared::input::MouseButton::Left) ? 1 : 0;
	short sX = m_x;
	short sY = m_y;

	// draw dialog background
	draw_new_dialog_box(InterfaceNdGame1, sX, sY, 0);
	draw_new_dialog_box(InterfaceNdText, sX, sY, 6);

	// Handle mouse scroll over dialog to cycle tabs (or scroll graphics content)
	if (m_game->get_dialog_box_manager().get_top_id() == DialogBoxId::SystemMenu && z != 0)
	{
		if (m_iActiveTab == TAB_GRAPHICS)
		{
			int total_items = 11;
#ifdef _DEBUG
			total_items = 13;
#endif
			int max_scroll = total_items - GRAPHICS_VISIBLE_ITEMS;

			bool in_content = (mouse_x >= sX + CONTENT_X &&
				mouse_x <= sX + CONTENT_X + CONTENT_WIDTH &&
				mouse_y >= sY + CONTENT_Y &&
				mouse_y <= sY + CONTENT_Y + CONTENT_HEIGHT);

			if (max_scroll > 0 && in_content)
			{
				if (z > 0) m_graphics_scroll_offset--;
				if (z < 0) m_graphics_scroll_offset++;
				if (m_graphics_scroll_offset < 0) m_graphics_scroll_offset = 0;
				if (m_graphics_scroll_offset > max_scroll) m_graphics_scroll_offset = max_scroll;
			}
			else
			{
				int prev_tab = m_iActiveTab;
				if (z > 0) m_iActiveTab = (m_iActiveTab - 1 + TAB_COUNT) % TAB_COUNT;
				else m_iActiveTab = (m_iActiveTab + 1) % TAB_COUNT;
				if (m_iActiveTab != prev_tab) m_graphics_scroll_offset = 0;
			}
		}
		else
		{
			if (z > 0) m_iActiveTab = (m_iActiveTab - 1 + TAB_COUNT) % TAB_COUNT;
			else m_iActiveTab = (m_iActiveTab + 1) % TAB_COUNT;
		}
	}

	// Update graphics scrollbar drag while mouse is held
	if (s_bDraggingGraphicsScroll && lb != 0)
	{
		int total_items = 11;
#ifdef _DEBUG
		total_items = 13;
#endif
		int max_scroll = total_items - GRAPHICS_VISIBLE_ITEMS;
		hb::shared::sprite::SpriteRect thumb_rect = m_game->m_sprite[InterfaceNdGame1]->GetFrameRect(4);
		int track_top = sY + CONTENT_Y;
		int track_height = CONTENT_HEIGHT - thumb_rect.height;
		if (track_height < 1) track_height = 1;
		int rel_y = mouse_y - track_top;
		m_graphics_scroll_offset = (rel_y * max_scroll + track_height / 2) / track_height;
		if (m_graphics_scroll_offset < 0) m_graphics_scroll_offset = 0;
		if (m_graphics_scroll_offset > max_scroll) m_graphics_scroll_offset = max_scroll;
	}

	draw_tabs(sX, sY);
	draw_tab_content(sX, sY, mouse_x, mouse_y, lb);

	// save slider values to config_manager when drag ends (mouse released)
	if (lb == 0)
	{
		if (s_bDraggingGraphicsScroll)
		{
			s_bDraggingGraphicsScroll = false;
		}
		if (s_bDraggingMasterSlider)
		{
			config_manager::get().set_master_volume(audio_manager::get().get_master_volume());
		}
		if (s_bDraggingEffectsSlider)
		{
			config_manager::get().set_sound_volume(audio_manager::get().get_sound_volume());
		}
		if (s_bDraggingAmbientSlider)
		{
			config_manager::get().set_ambient_volume(audio_manager::get().get_ambient_volume());
		}
		if (s_bDraggingUISlider)
		{
			config_manager::get().set_ui_volume(audio_manager::get().get_ui_volume());
		}
		if (s_bDraggingMusicSlider)
		{
			config_manager::get().set_music_volume(audio_manager::get().get_music_volume());
		}
		s_bDraggingMasterSlider = false;
		s_bDraggingEffectsSlider = false;
		s_bDraggingAmbientSlider = false;
		s_bDraggingUISlider = false;
		s_bDraggingMusicSlider = false;
		m_is_scroll_selected = false;
	}
}

void DialogBox_SysMenu::draw_tabs(short sX, short sY)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	hb::shared::sprite::SpriteRect button_rect = m_game->m_sprite[InterfaceNdButton]->GetFrameRect(70);
	int btnY = sY + 33;

	const int tabFrames[TAB_COUNT][2] = {
		{70, 71},  // General
		{72, 73},  // Graphics
		{74, 76},  // Audio
		{75, 77}   // System
	};

	for (int i = 0; i < TAB_COUNT; i++)
	{
		int btnX = sX + 17 + (button_rect.width * i);
		bool hovered = (mouse_x >= btnX && mouse_x < btnX + button_rect.width && mouse_y >= btnY && mouse_y < btnY + button_rect.height);
		bool active = (m_iActiveTab == i);

		int frameIndex = (active || hovered) ? tabFrames[i][1] : tabFrames[i][0];
		draw_new_dialog_box(InterfaceNdButton, btnX, btnY, frameIndex);
	}
}

void DialogBox_SysMenu::draw_tab_content(short sX, short sY, short mouse_x, short mouse_y, char lb)
{
	switch (m_iActiveTab)
	{
	case TAB_GENERAL:
		draw_general_tab(sX, sY);
		break;
	case TAB_GRAPHICS:
		draw_graphics_tab(sX, sY);
		break;
	case TAB_AUDIO:
		draw_audio_tab(sX, sY, mouse_x, mouse_y, lb);
		break;
	case TAB_SYSTEM:
		draw_system_tab(sX, sY);
		break;
	}
}

void DialogBox_SysMenu::draw_toggle(int x, int y, bool enabled)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	// draw toggle background box at y-2
	const int boxY = y - 2;
	draw_new_dialog_box(InterfaceNdButton, x, boxY, 79);

	// Use cached dimensions or fallback values
	const int boxWidth = m_bFrameSizesInitialized ? m_iSmallBoxWidth : 36;
	const int boxHeight = m_bFrameSizesInitialized ? m_iSmallBoxHeight : 16;

	bool hover = (mouse_x >= x && mouse_x <= x + boxWidth && mouse_y >= boxY && mouse_y <= boxY + boxHeight);
	const hb::shared::render::Color& color = (enabled || hover) ? GameColors::UIWhite : GameColors::UIDisabled;

	// Center text horizontally and vertically in the box
	const char* text = enabled ? DRAW_DIALOGBOX_SYSMENU_ON : DRAW_DIALOGBOX_SYSMENU_OFF;
	hb::shared::text::TextMetrics metrics = hb::shared::text::GetTextRenderer()->measure_text(text);
	int textX = x + (boxWidth - metrics.width) / 2;
	int textY = boxY + (boxHeight - metrics.height) / 2;
	put_string(textX, textY, text, color);
}

bool DialogBox_SysMenu::is_in_toggle_area(int x, int y)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	const int boxWidth = m_bFrameSizesInitialized ? m_iSmallBoxWidth : 36;
	const int boxHeight = m_bFrameSizesInitialized ? m_iSmallBoxHeight : 16;
	return (mouse_x >= x && mouse_x <= x + boxWidth && mouse_y >= y - 2 && mouse_y <= y - 2 + boxHeight);
}

// =============================================================================
// GENERAL TAB
// =============================================================================
void DialogBox_SysMenu::draw_general_tab(short sX, short sY)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	const int contentX = sX + CONTENT_X;
	const int contentY = sY + CONTENT_Y;
	const int contentBottom = contentY + CONTENT_HEIGHT;
	const int centerX = contentX + (CONTENT_WIDTH / 2);

	// Server name at top left
	put_string(contentX + 5, contentY + 5, UPDATE_SCREEN_ON_SELECT_CHARACTER36, GameColors::UILabel);
	put_string(contentX + 6, contentY + 5, UPDATE_SCREEN_ON_SELECT_CHARACTER36, GameColors::UILabel);

	// Current time centered below server name (MM/DD/YYYY HH:MM AM/PM)
	auto now = std::chrono::system_clock::now();
	std::time_t t = std::chrono::system_clock::to_time_t(now);
	std::tm local_time{};
#ifdef _WIN32
	localtime_s(&local_time, &t);
#else
	localtime_r(&t, &local_time);
#endif
	std::string timeBuf;
	int hour12 = local_time.tm_hour % 12;
	if (hour12 == 0) hour12 = 12;
	const char* ampm = (local_time.tm_hour < 12) ? "AM" : "PM";
	timeBuf = std::format("{:02}/{:02}/{:04} {}:{:02} {}",
		local_time.tm_mon + 1, local_time.tm_mday, local_time.tm_year + 1900,
		hour12, local_time.tm_min, ampm);

	int textWidth = hb::shared::text::GetTextRenderer()->measure_text(timeBuf.c_str()).width;
	int timeX = centerX - (textWidth / 2);
	put_string(timeX, contentY + 25, timeBuf.c_str(), GameColors::UILabel);
	put_string(timeX + 1, contentY + 25, timeBuf.c_str(), GameColors::UILabel);

	// Buttons at bottom of content area
	int buttonY = contentBottom - 30;

	// Log-Out / Continue button (left side)
	if (m_game->on_game()->m_logout_count == -1) {
		bool hover = (mouse_x >= sX + ui_layout::left_btn_x && mouse_x <= sX + ui_layout::left_btn_x + ui_layout::btn_size_x &&
			mouse_y >= buttonY && mouse_y <= buttonY + ui_layout::btn_size_y);
		draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::left_btn_x, buttonY, hover ? 9 : 8);
	}
	else {
		bool hover = (mouse_x >= sX + ui_layout::left_btn_x && mouse_x <= sX + ui_layout::left_btn_x + ui_layout::btn_size_x &&
			mouse_y >= buttonY && mouse_y <= buttonY + ui_layout::btn_size_y);
		draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::left_btn_x, buttonY, hover ? 7 : 6);
	}

	// Restart button (right side, only when dead)
	if ((player().m_hp <= 0) && (m_game->m_restart_count == -1))
	{
		bool hover = (mouse_x >= sX + ui_layout::right_btn_x && mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x &&
			mouse_y >= buttonY && mouse_y <= buttonY + ui_layout::btn_size_y);
		draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, buttonY, hover ? 37 : 36);
	}
}

// =============================================================================
// GRAPHICS TAB
// =============================================================================
void DialogBox_SysMenu::draw_graphics_tab(short sX, short sY)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	const int contentX = sX + CONTENT_X;
	const int contentY = sY + CONTENT_Y;
	const int contentRight = contentX + CONTENT_WIDTH;
	const int labelX = contentX + 5;

	// Right margin for box alignment
	const int rightMargin = 8;
	const int boxRightEdge = contentRight - rightMargin;

	// Use cached dimensions or fallback values
	const int largeBoxWidth = m_bFrameSizesInitialized ? m_iLargeBoxWidth : 280;
	const int largeBoxHeight = m_bFrameSizesInitialized ? m_iLargeBoxHeight : 16;
	const int wideBoxWidth = m_bFrameSizesInitialized ? m_iWideBoxWidth : 175;
	const int wideBoxHeight = m_bFrameSizesInitialized ? m_iWideBoxHeight : 16;

	// Right-align the large box, then left-align all smaller boxes to its left edge
	const int largeBoxX = boxRightEdge - largeBoxWidth;
	const int wideBoxX = largeBoxX;
	const int smallBoxX = largeBoxX;

	const bool fullscreen = m_game->m_Renderer->is_fullscreen();

	// Scroll support
	int total_items = 11;
#ifdef _DEBUG
	total_items = 13;
#endif
	bool scrollable = total_items > GRAPHICS_VISIBLE_ITEMS;

	int lineY = contentY + 5 - (m_graphics_scroll_offset * GRAPHICS_LINE_HEIGHT);

	auto is_item_visible = [&](int ly) {
		return (ly >= contentY - 2) && (ly + 16 <= contentY + CONTENT_HEIGHT);
	};

	// --- FPS Limit --- large box (frame 81) with 5 options (disabled when VSync is on)
	if (is_item_visible(lineY))
	{
		const bool v_sync_on = config_manager::get().is_vsync_enabled();
		put_string(labelX, lineY, "FPS Limit:", GameColors::UILabel);
		put_string(labelX + 1, lineY, "FPS Limit:", GameColors::UILabel);

		const int fpsBoxY = lineY - 2;
		draw_new_dialog_box(InterfaceNdButton, largeBoxX, fpsBoxY, 81);

		static const int s_FpsOptions[] = { 60, 100, 144, 240, 0 };
		static const char* s_FpsLabels[] = { "60", "100", "144", "240", "Max" };
		static const int s_NumFpsOptions = 5;
		const int fpsRegionWidth = largeBoxWidth / s_NumFpsOptions;
		const int currentFps = config_manager::get().get_fps_limit();

		for (int i = 0; i < s_NumFpsOptions; i++)
		{
			int regionX = largeBoxX + (fpsRegionWidth * i);
			bool selected = (currentFps == s_FpsOptions[i]);
			bool hover = !v_sync_on && (mouse_x >= regionX && mouse_x < regionX + fpsRegionWidth && mouse_y >= fpsBoxY && mouse_y <= fpsBoxY + largeBoxHeight);

			hb::shared::text::TextMetrics fm = hb::shared::text::GetTextRenderer()->measure_text(s_FpsLabels[i]);
			int tx = regionX + (fpsRegionWidth - fm.width) / 2;
			int ty = fpsBoxY + (largeBoxHeight - fm.height) / 2;
			put_string(tx, ty, s_FpsLabels[i], v_sync_on ? GameColors::UIDisabled : (selected || hover) ? GameColors::UIWhite : GameColors::UIDisabled);
		}
	}

	lineY += 18;

	// --- Aspect Ratio --- wide box (Letterbox / Widescreen), only enabled in fullscreen
	if (is_item_visible(lineY))
	{
		put_string(labelX, lineY, "Aspect Ratio:", GameColors::UILabel);
		put_string(labelX + 1, lineY, "Aspect Ratio:", GameColors::UILabel);

		const int aspectBoxY = lineY - 2;
		draw_new_dialog_box(InterfaceNdButton, wideBoxX, aspectBoxY, 78);

		const bool stretch = config_manager::get().is_fullscreen_stretch_enabled();
		const int aspectRegionWidth = wideBoxWidth / 2;

		const char* letterboxText = "Letterbox";
		const char* widescreenText = "Widescreen";

		int leftRegion = wideBoxX;
		int rightRegion = wideBoxX + aspectRegionWidth;
		bool letterHover = fullscreen && (mouse_x >= leftRegion && mouse_x < rightRegion && mouse_y >= aspectBoxY && mouse_y <= aspectBoxY + wideBoxHeight);
		bool wideHover = fullscreen && (mouse_x >= rightRegion && mouse_x <= wideBoxX + wideBoxWidth && mouse_y >= aspectBoxY && mouse_y <= aspectBoxY + wideBoxHeight);

		hb::shared::text::TextMetrics lbm = hb::shared::text::GetTextRenderer()->measure_text(letterboxText);
		hb::shared::text::TextMetrics wsm = hb::shared::text::GetTextRenderer()->measure_text(widescreenText);
		int lbx = leftRegion + (aspectRegionWidth - lbm.width) / 2;
		int wsx = rightRegion + (aspectRegionWidth - wsm.width) / 2;
		int aty = aspectBoxY + (wideBoxHeight - lbm.height) / 2;

		if (!fullscreen) {
			put_string(lbx, aty, letterboxText, GameColors::UIDisabled);
			put_string(wsx, aty, widescreenText, GameColors::UIDisabled);
		} else {
			put_string(lbx, aty, letterboxText, (!stretch || letterHover) ? GameColors::UIWhite : GameColors::UIDisabled);
			put_string(wsx, aty, widescreenText, (stretch || wideHover) ? GameColors::UIWhite : GameColors::UIDisabled);
		}
	}

	lineY += 18;

	// --- VSync ---
	if (is_item_visible(lineY))
	{
		put_string(labelX, lineY, "VSync:", GameColors::UILabel);
		put_string(labelX + 1, lineY, "VSync:", GameColors::UILabel);
		draw_toggle(smallBoxX, lineY, config_manager::get().is_vsync_enabled());
	}

	lineY += 18;

	// --- Detail Level --- wide box with Low/Normal/High
	if (is_item_visible(lineY))
	{
		put_string(labelX, lineY, DRAW_DIALOGBOX_SYSMENU_DETAILLEVEL, GameColors::UILabel);
		put_string(labelX + 1, lineY, DRAW_DIALOGBOX_SYSMENU_DETAILLEVEL, GameColors::UILabel);

		const int detailLevel = config_manager::get().get_detail_level();
		const int boxY = lineY - 2;

		draw_new_dialog_box(InterfaceNdButton, wideBoxX, boxY, 78);

		const int regionWidth = wideBoxWidth / 3;

		hb::shared::text::TextMetrics lowMetrics = hb::shared::text::GetTextRenderer()->measure_text(DRAW_DIALOGBOX_SYSMENU_LOW);
		int textY = boxY + (wideBoxHeight - lowMetrics.height) / 2;

		hb::shared::text::TextMetrics normMetrics = hb::shared::text::GetTextRenderer()->measure_text(DRAW_DIALOGBOX_SYSMENU_NORMAL);
		hb::shared::text::TextMetrics highMetrics = hb::shared::text::GetTextRenderer()->measure_text(DRAW_DIALOGBOX_SYSMENU_HIGH);

		int lowX = wideBoxX + (regionWidth - lowMetrics.width) / 2;
		int normX = wideBoxX + regionWidth + (regionWidth - normMetrics.width) / 2;
		int highX = wideBoxX + (regionWidth * 2) + (regionWidth - highMetrics.width) / 2;

		bool lowHover = (mouse_x >= wideBoxX && mouse_x < wideBoxX + regionWidth && mouse_y >= boxY && mouse_y <= boxY + wideBoxHeight);
		bool normHover = (mouse_x >= wideBoxX + regionWidth && mouse_x < wideBoxX + (regionWidth * 2) && mouse_y >= boxY && mouse_y <= boxY + wideBoxHeight);
		bool highHover = (mouse_x >= wideBoxX + (regionWidth * 2) && mouse_x <= wideBoxX + wideBoxWidth && mouse_y >= boxY && mouse_y <= boxY + wideBoxHeight);

		put_string(lowX, textY, DRAW_DIALOGBOX_SYSMENU_LOW, (detailLevel == 0 || lowHover) ? GameColors::UIWhite : GameColors::UIDisabled);
		put_string(normX, textY, DRAW_DIALOGBOX_SYSMENU_NORMAL, (detailLevel == 1 || normHover) ? GameColors::UIWhite : GameColors::UIDisabled);
		put_string(highX, textY, DRAW_DIALOGBOX_SYSMENU_HIGH, (detailLevel == 2 || highHover) ? GameColors::UIWhite : GameColors::UIDisabled);
	}

	lineY += 18;

	// --- Dialog Transparency ---
	if (is_item_visible(lineY))
	{
		put_string(labelX, lineY, "Transparency:", GameColors::UILabel);
		put_string(labelX + 1, lineY, "Transparency:", GameColors::UILabel);
		draw_toggle(smallBoxX, lineY, config_manager::get().is_dialog_transparency_enabled());
	}

	lineY += 18;

	// --- Show FPS ---
	if (is_item_visible(lineY))
	{
		put_string(labelX, lineY, "Show FPS:", GameColors::UILabel);
		put_string(labelX + 1, lineY, "Show FPS:", GameColors::UILabel);
		draw_toggle(smallBoxX, lineY, config_manager::get().is_show_fps_enabled());
	}

	lineY += 18;

	// --- Show Latency ---
	if (is_item_visible(lineY))
	{
		put_string(labelX, lineY, "Show Latency:", GameColors::UILabel);
		put_string(labelX + 1, lineY, "Show Latency:", GameColors::UILabel);
		draw_toggle(smallBoxX, lineY, config_manager::get().is_show_latency_enabled());
	}

	lineY += 18;

	// --- Background FPS Throttle ---
	if (is_item_visible(lineY))
	{
		put_string(labelX, lineY, "Power Saving:", GameColors::UILabel);
		put_string(labelX + 1, lineY, "Power Saving:", GameColors::UILabel);
		draw_toggle(smallBoxX, lineY, config_manager::get().is_background_fps_throttle_enabled());
	}

	lineY += 18;

	// --- Map Zoom ---
	if (is_item_visible(lineY))
	{
		put_string(labelX, lineY, "Map Zoom:", GameColors::UILabel);
		put_string(labelX + 1, lineY, "Map Zoom:", GameColors::UILabel);
		draw_toggle(smallBoxX, lineY, config_manager::get().is_zoom_map_enabled());
	}

	lineY += 18;

#ifdef _DEBUG
	// Tile Grid (simple dark lines) - DEBUG ONLY
	if (is_item_visible(lineY))
	{
		put_string(labelX, lineY, "Tile Grid:", GameColors::UILabel);
		put_string(labelX + 1, lineY, "Tile Grid:", GameColors::UILabel);
		draw_toggle(smallBoxX, lineY, config_manager::get().is_tile_grid_enabled());
	}

	lineY += 18;

	// Patching Grid (debug with zone colors) - DEBUG ONLY
	if (is_item_visible(lineY))
	{
		put_string(labelX, lineY, "Patching Grid:", GameColors::UILabel);
		put_string(labelX + 1, lineY, "Patching Grid:", GameColors::UILabel);
		draw_toggle(smallBoxX, lineY, config_manager::get().is_patching_grid_enabled());
	}

	lineY += 18;
#endif

	// --- Display Mode --- wide box (Fullscreen / Windowed)
	if (is_item_visible(lineY))
	{
		put_string(labelX, lineY, "Display Mode:", GameColors::UILabel);
		put_string(labelX + 1, lineY, "Display Mode:", GameColors::UILabel);

		const int modeBoxY = lineY - 2;
		draw_new_dialog_box(InterfaceNdButton, wideBoxX, modeBoxY, 78);

		bool modeHover = (mouse_x >= wideBoxX && mouse_x <= wideBoxX + wideBoxWidth && mouse_y >= modeBoxY && mouse_y <= modeBoxY + wideBoxHeight);
		const hb::shared::render::Color& modeColor = modeHover ? GameColors::UIWhite : GameColors::UIDisabled;

		const char* modeText = fullscreen ? "Fullscreen" : "Windowed";
		hb::shared::text::TextMetrics modeMetrics = hb::shared::text::GetTextRenderer()->measure_text(modeText);
		int modeTextX = wideBoxX + (wideBoxWidth - modeMetrics.width) / 2;
		int modeTextY = modeBoxY + (wideBoxHeight - modeMetrics.height) / 2;
		put_string(modeTextX, modeTextY, modeText, modeColor);
	}

	lineY += 18;

	// --- hb::shared::render::Window Style --- wide box (Borderless / Bordered, disabled when fullscreen)
	if (is_item_visible(lineY))
	{
		put_string(labelX, lineY, "Window Style:", GameColors::UILabel);
		put_string(labelX + 1, lineY, "Window Style:", GameColors::UILabel);

		const int styleBoxY = lineY - 2;
		draw_new_dialog_box(InterfaceNdButton, wideBoxX, styleBoxY, 78);

		bool styleHover = !fullscreen && (mouse_x >= wideBoxX && mouse_x <= wideBoxX + wideBoxWidth && mouse_y >= styleBoxY && mouse_y <= styleBoxY + wideBoxHeight);
		const hb::shared::render::Color& styleColor = fullscreen ? GameColors::UIDisabled : (styleHover ? GameColors::UIWhite : GameColors::UIDisabled);

		const char* styleText = config_manager::get().is_borderless_enabled() ? "Borderless" : "Bordered";
		hb::shared::text::TextMetrics styleMetrics = hb::shared::text::GetTextRenderer()->measure_text(styleText);
		int styleTextX = wideBoxX + (wideBoxWidth - styleMetrics.width) / 2;
		int styleTextY = styleBoxY + (wideBoxHeight - styleMetrics.height) / 2;
		put_string(styleTextX, styleTextY, styleText, styleColor);
	}

	lineY += 18;

	// --- Resolution --- wide box with centered text (disabled when fullscreen)
	if (is_item_visible(lineY))
	{
		put_string(labelX, lineY, "Resolution:", GameColors::UILabel);
		put_string(labelX + 1, lineY, "Resolution:", GameColors::UILabel);

		int resWidth, resHeight;
		if (fullscreen) {
			resWidth = hb::platform::get_screen_width();
			resHeight = hb::platform::get_screen_height();
		}
		else {
			int resIndex = get_current_resolution_index();
			resWidth = s_Resolutions[resIndex].width;
			resHeight = s_Resolutions[resIndex].height;
		}

		std::string resBuf;
		resBuf = std::format("{}x{}", resWidth, resHeight);

		const int resBoxY = lineY - 2;
		draw_new_dialog_box(InterfaceNdButton, wideBoxX, resBoxY, 78);

		bool resHover = !fullscreen && (mouse_x >= wideBoxX && mouse_x <= wideBoxX + wideBoxWidth && mouse_y >= resBoxY && mouse_y <= resBoxY + wideBoxHeight);
		const hb::shared::render::Color& resColor = fullscreen ? GameColors::UIDisabled : (resHover ? GameColors::UIWhite : GameColors::UIDisabled);

		hb::shared::text::TextMetrics resMetrics = hb::shared::text::GetTextRenderer()->measure_text(resBuf.c_str());
		int resTextX = wideBoxX + (wideBoxWidth - resMetrics.width) / 2;
		int resTextY = resBoxY + (wideBoxHeight - resMetrics.height) / 2;
		put_string(resTextX, resTextY, resBuf.c_str(), resColor);
	}

	// --- Scrollbar ---
	if (scrollable)
	{
		int max_scroll = total_items - GRAPHICS_VISIBLE_ITEMS;

		// InterfaceNdGame1 frame 3 = track background, frame 4 = scrub/thumb (no pivot offsets)
		hb::shared::sprite::SpriteRect track_rect = m_game->m_sprite[InterfaceNdGame1]->GetFrameRect(3);
		hb::shared::sprite::SpriteRect thumb_rect = m_game->m_sprite[InterfaceNdGame1]->GetFrameRect(4);

		int scroll_x = contentX + CONTENT_WIDTH - (track_rect.width / 2);
		int track_top = contentY;
		int track_height = CONTENT_HEIGHT;

		// Draw track background (frame 3)
		draw_new_dialog_box(InterfaceNdGame1, scroll_x, track_top, 3);

		// Draw scrub/thumb (frame 4) at scroll position
		int thumb_y = track_top;
		if (max_scroll > 0)
			thumb_y = track_top + ((track_height - thumb_rect.height) * m_graphics_scroll_offset) / max_scroll;
		draw_new_dialog_box(InterfaceNdGame1, scroll_x - ((thumb_rect.width - track_rect.width) / 2), thumb_y, 4);
	}
}

// =============================================================================
// AUDIO TAB
// =============================================================================
void DialogBox_SysMenu::draw_audio_tab(short sX, short sY, short mouse_x, short mouse_y, char lb)
{
	const int contentX = sX + CONTENT_X;
	const int contentY = sY + CONTENT_Y;
	const int labelX = contentX + 5;
	const int toggleX = contentX + 68;
	const int sliderX = contentX + 110;

	bool available = audio_manager::get().is_sound_available();

	// --- Master: [On/Off] ---slider--- ---
	int lineY = contentY + 8;

	put_string(labelX, lineY, "Master:", GameColors::UILabel);
	put_string(labelX + 1, lineY, "Master:", GameColors::UILabel);

	if (available)
		draw_toggle(toggleX, lineY, audio_manager::get().is_master_enabled());
	else
		put_string(toggleX, lineY, DRAW_DIALOGBOX_SYSMENU_DISABLED, GameColors::UIDisabled);

	int masterVol = audio_manager::get().get_master_volume();
	draw_new_dialog_box(InterfaceNdButton, sliderX, lineY + 5, 80);
	draw_new_dialog_box(InterfaceNdGame2, sliderX + masterVol, lineY, 8);

	if (s_bDraggingMasterSlider && lb != 0)
	{
		int volume = mouse_x - sliderX;
		if (volume > 100) volume = 100;
		if (volume < 0) volume = 0;
		audio_manager::get().set_master_volume(volume);
	}

	// --- Effects: [On/Off] ---slider--- ---
	lineY = contentY + 52;

	put_string(labelX, lineY, "Effects:", GameColors::UILabel);
	put_string(labelX + 1, lineY, "Effects:", GameColors::UILabel);

	if (available)
		draw_toggle(toggleX, lineY, audio_manager::get().is_sound_enabled());
	else
		put_string(toggleX, lineY, DRAW_DIALOGBOX_SYSMENU_DISABLED, GameColors::UIDisabled);

	int effectsVol = audio_manager::get().get_sound_volume();
	draw_new_dialog_box(InterfaceNdButton, sliderX, lineY + 5, 80);
	draw_new_dialog_box(InterfaceNdGame2, sliderX + effectsVol, lineY, 8);

	if (s_bDraggingEffectsSlider && lb != 0)
	{
		int volume = mouse_x - sliderX;
		if (volume > 100) volume = 100;
		if (volume < 0) volume = 0;
		audio_manager::get().set_sound_volume(volume);
	}

	// --- Ambient: [On/Off] ---slider--- ---
	lineY = contentY + 92;

	put_string(labelX, lineY, "Ambient:", GameColors::UILabel);
	put_string(labelX + 1, lineY, "Ambient:", GameColors::UILabel);

	if (available)
		draw_toggle(toggleX, lineY, audio_manager::get().is_ambient_enabled());
	else
		put_string(toggleX, lineY, DRAW_DIALOGBOX_SYSMENU_DISABLED, GameColors::UIDisabled);

	int ambientVol = audio_manager::get().get_ambient_volume();
	draw_new_dialog_box(InterfaceNdButton, sliderX, lineY + 5, 80);
	draw_new_dialog_box(InterfaceNdGame2, sliderX + ambientVol, lineY, 8);

	if (s_bDraggingAmbientSlider && lb != 0)
	{
		int volume = mouse_x - sliderX;
		if (volume > 100) volume = 100;
		if (volume < 0) volume = 0;
		audio_manager::get().set_ambient_volume(volume);
	}

	// --- UI: [On/Off] ---slider--- ---
	lineY = contentY + 132;

	put_string(labelX, lineY, "UI:", GameColors::UILabel);
	put_string(labelX + 1, lineY, "UI:", GameColors::UILabel);

	if (available)
		draw_toggle(toggleX, lineY, audio_manager::get().is_ui_enabled());
	else
		put_string(toggleX, lineY, DRAW_DIALOGBOX_SYSMENU_DISABLED, GameColors::UIDisabled);

	int uiVol = audio_manager::get().get_ui_volume();
	draw_new_dialog_box(InterfaceNdButton, sliderX, lineY + 5, 80);
	draw_new_dialog_box(InterfaceNdGame2, sliderX + uiVol, lineY, 8);

	if (s_bDraggingUISlider && lb != 0)
	{
		int volume = mouse_x - sliderX;
		if (volume > 100) volume = 100;
		if (volume < 0) volume = 0;
		audio_manager::get().set_ui_volume(volume);
	}

	// --- Music: [On/Off] ---slider--- ---
	lineY = contentY + 172;

	put_string(labelX, lineY, DRAW_DIALOGBOX_SYSMENU_MUSIC, GameColors::UILabel);
	put_string(labelX + 1, lineY, DRAW_DIALOGBOX_SYSMENU_MUSIC, GameColors::UILabel);

	if (available)
		draw_toggle(toggleX, lineY, audio_manager::get().is_music_enabled());
	else
		put_string(toggleX, lineY, DRAW_DIALOGBOX_SYSMENU_DISABLED, GameColors::UIDisabled);

	int musicVol = audio_manager::get().get_music_volume();
	draw_new_dialog_box(InterfaceNdButton, sliderX, lineY + 5, 80);
	draw_new_dialog_box(InterfaceNdGame2, sliderX + musicVol, lineY, 8);

	if (s_bDraggingMusicSlider && lb != 0)
	{
		int volume = mouse_x - sliderX;
		if (volume > 100) volume = 100;
		if (volume < 0) volume = 0;
		audio_manager::get().set_music_volume(volume);
	}
}

// =============================================================================
// SYSTEM TAB
// =============================================================================
void DialogBox_SysMenu::draw_system_tab(short sX, short sY)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	const int contentX = sX + CONTENT_X;
	const int contentY = sY + CONTENT_Y;
	int lineY = contentY + 5;
	const int labelX = contentX + 5;
	const int valueX = contentX + 140;

	// Whisper toggle
	put_string(labelX, lineY, DRAW_DIALOGBOX_SYSMENU_WHISPER, GameColors::UILabel);
	put_string(labelX + 1, lineY, DRAW_DIALOGBOX_SYSMENU_WHISPER, GameColors::UILabel);
	draw_toggle(valueX, lineY, ChatManager::get().is_whisper_enabled());

	lineY += 20;

	// Shout toggle
	put_string(labelX, lineY, DRAW_DIALOGBOX_SYSMENU_SHOUT, GameColors::UILabel);
	put_string(labelX + 1, lineY, DRAW_DIALOGBOX_SYSMENU_SHOUT, GameColors::UILabel);
	draw_toggle(valueX, lineY, ChatManager::get().is_shout_enabled());

	lineY += 20;

	// Running Mode toggle
	put_string(labelX, lineY, "Running Mode:", GameColors::UILabel);
	put_string(labelX + 1, lineY, "Running Mode:", GameColors::UILabel);
	draw_toggle(valueX, lineY, config_manager::get().is_running_mode_enabled());

	lineY += 20;

	// Capture Mouse toggle
	put_string(labelX, lineY, "Capture Mouse:", GameColors::UILabel);
	put_string(labelX + 1, lineY, "Capture Mouse:", GameColors::UILabel);
	draw_toggle(valueX, lineY, config_manager::get().is_mouse_capture_enabled());

	lineY += 20;

	// Guide Map toggle
	put_string(labelX, lineY, DRAW_DIALOGBOX_SYSMENU_GUIDEMAP, GameColors::UILabel);
	put_string(labelX + 1, lineY, DRAW_DIALOGBOX_SYSMENU_GUIDEMAP, GameColors::UILabel);
	draw_toggle(valueX, lineY, m_game->get_dialog_box_manager().is_enabled(DialogBoxId::GuideMap));

	lineY += 20;

	// Reduced Motion toggle
	put_string(labelX, lineY, "Reduced Motion:", GameColors::UILabel);
	put_string(labelX + 1, lineY, "Reduced Motion:", GameColors::UILabel);
	draw_toggle(valueX, lineY, config_manager::get().is_reduced_motion_enabled());

	lineY += 20;

	// Toggle to Chat toggle
	put_string(labelX, lineY, "Toggle to Chat:", GameColors::UILabel);
	put_string(labelX + 1, lineY, "Toggle to Chat:", GameColors::UILabel);
	draw_toggle(valueX, lineY, config_manager::get().is_toggle_to_chat_enabled());
}

// =============================================================================
// CLICK HANDLERS
// =============================================================================
bool DialogBox_SysMenu::on_click()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	short sX = m_x;
	short sY = m_y;

	// Check tab button clicks
	hb::shared::sprite::SpriteRect button_rect = m_game->m_sprite[InterfaceNdButton]->GetFrameRect(70);
	int btnY = sY + 33;

	for (int i = 0; i < TAB_COUNT; i++)
	{
		int btnX = sX + 17 + (button_rect.width * i);
		if (mouse_x >= btnX && mouse_x < btnX + button_rect.width && mouse_y >= btnY && mouse_y < btnY + button_rect.height)
		{
			if (m_iActiveTab != i) m_graphics_scroll_offset = 0;
			m_iActiveTab = i;
			audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
			return true;
		}
	}

	// Handle clicks for active tab content
	switch (m_iActiveTab)
	{
	case TAB_GENERAL:
		return on_click_general(sX, sY);
	case TAB_GRAPHICS:
		return on_click_graphics(sX, sY);
	case TAB_AUDIO:
		return on_click_audio(sX, sY);
	case TAB_SYSTEM:
		return on_click_system(sX, sY);
	}

	return false;
}

PressResult DialogBox_SysMenu::on_press()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	short sX = m_x;
	short sY = m_y;

	// Graphics tab scrollbar drag
	if (m_iActiveTab == TAB_GRAPHICS)
	{
		int total_items = 11;
#ifdef _DEBUG
		total_items = 13;
#endif
		if (total_items > GRAPHICS_VISIBLE_ITEMS)
		{
			hb::shared::sprite::SpriteRect track_rect = m_game->m_sprite[InterfaceNdGame1]->GetFrameRect(3);
			int scroll_x = sX + CONTENT_X + CONTENT_WIDTH - track_rect.width;
			int track_top = sY + CONTENT_Y;
			int track_bottom = sY + CONTENT_Y + CONTENT_HEIGHT;
			if (mouse_x >= scroll_x && mouse_x <= scroll_x + track_rect.width &&
				mouse_y >= track_top && mouse_y <= track_bottom)
			{
				s_bDraggingGraphicsScroll = true;
				m_is_scroll_selected = true;
				return PressResult::ScrollClaimed;
			}
		}
	}

	// Only claim scroll for Audio tab slider areas
	if (m_iActiveTab != TAB_AUDIO)
		return PressResult::Normal;

	const int contentX = sX + CONTENT_X;
	const int contentY = sY + CONTENT_Y;
	const int sliderX = contentX + 110;
	const int sliderWidth = 100;

	// Helper lambda for slider hit detection
	auto checkSlider = [&](int sliderY, bool& dragFlag) -> bool {
		if ((mouse_x >= sliderX) && (mouse_x <= sliderX + sliderWidth + 10) &&
			(mouse_y >= sliderY - 5) && (mouse_y <= sliderY + 15))
		{
			dragFlag = true;
			m_is_scroll_selected = true;
			return true;
		}
		return false;
	};

	// Master slider at contentY + 8
	if (checkSlider(contentY + 8, s_bDraggingMasterSlider))
		return PressResult::ScrollClaimed;

	// Effects slider at contentY + 52
	if (checkSlider(contentY + 52, s_bDraggingEffectsSlider))
		return PressResult::ScrollClaimed;

	// Ambient slider at contentY + 92
	if (checkSlider(contentY + 92, s_bDraggingAmbientSlider))
		return PressResult::ScrollClaimed;

	// UI slider at contentY + 132
	if (checkSlider(contentY + 132, s_bDraggingUISlider))
		return PressResult::ScrollClaimed;

	// Music slider at contentY + 172
	if (checkSlider(contentY + 172, s_bDraggingMusicSlider))
		return PressResult::ScrollClaimed;

	return PressResult::Normal;
}

bool DialogBox_SysMenu::on_click_general(short sX, short sY)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	const int contentY = sY + CONTENT_Y;
	const int contentBottom = contentY + CONTENT_HEIGHT;
	int buttonY = contentBottom - 30;

	// Log-Out / Continue button
	if (mouse_x >= sX + ui_layout::left_btn_x && mouse_x <= sX + ui_layout::left_btn_x + ui_layout::btn_size_x &&
		mouse_y >= buttonY && mouse_y <= buttonY + ui_layout::btn_size_y)
	{
		if (!m_game->m_force_disconn)
		{
			if (m_game->on_game()->m_logout_count == -1) {
				m_game->on_game()->m_logout_count = 11;
				m_game->on_game()->m_logout_count_time = GameClock::get_time_ms();
			}
			else {
				m_game->on_game()->m_logout_count = -1;
				add_event_list(DLGBOX_CLICK_SYSMENU2, 10);
				disable_this_dialog();
			}
			audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
			return true;
		}
	}

	// Restart button (only when dead)
	if ((player().m_hp <= 0) && (m_game->m_restart_count == -1))
	{
		if (mouse_x >= sX + ui_layout::right_btn_x && mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x &&
			mouse_y >= buttonY && mouse_y <= buttonY + ui_layout::btn_size_y)
		{
			m_game->m_restart_count = 5;
			m_game->m_restart_count_time = GameClock::get_time_ms();
			disable_this_dialog();
			std::string restartBuf;
			restartBuf = std::format(DLGBOX_CLICK_SYSMENU1, m_game->m_restart_count);
			add_event_list(restartBuf.c_str(), 10);
			audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
			return true;
		}
	}

	return false;
}

bool DialogBox_SysMenu::on_click_graphics(short sX, short sY)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	const int contentX = sX + CONTENT_X;
	const int contentY = sY + CONTENT_Y;

	// Match draw positions - right-align large box, left-align others to its left edge
	const int contentRight = contentX + CONTENT_WIDTH;
	const int rightMargin = 8;
	const int boxRightEdge = contentRight - rightMargin;
	const int largeBoxWidth = m_bFrameSizesInitialized ? m_iLargeBoxWidth : 280;
	const int largeBoxHeight = m_bFrameSizesInitialized ? m_iLargeBoxHeight : 16;
	const int wideBoxWidth = m_bFrameSizesInitialized ? m_iWideBoxWidth : 175;
	const int wideBoxHeight = m_bFrameSizesInitialized ? m_iWideBoxHeight : 16;
	const int largeBoxX = boxRightEdge - largeBoxWidth;
	const int wideBoxX = largeBoxX;
	const int smallBoxX = largeBoxX;

	const bool fullscreen = m_game->m_Renderer->is_fullscreen();

	// Scroll offset applied to lineY
	int lineY = contentY + 5 - (m_graphics_scroll_offset * GRAPHICS_LINE_HEIGHT);

	auto is_item_visible = [&](int ly) {
		return (ly >= contentY - 2) && (ly + 16 <= contentY + CONTENT_HEIGHT);
	};

	// --- FPS Limit --- (disabled when VSync is on)
	if (is_item_visible(lineY))
	{
		const bool v_sync_on = config_manager::get().is_vsync_enabled();
		const int fpsBoxY = lineY - 2;
		static const int s_FpsOptions[] = { 60, 100, 144, 240, 0 };
		static const int s_NumFpsOptions = 5;
		const int fpsRegionWidth = largeBoxWidth / s_NumFpsOptions;

		if (!v_sync_on && mouse_y >= fpsBoxY && mouse_y <= fpsBoxY + largeBoxHeight && mouse_x >= largeBoxX && mouse_x <= largeBoxX + largeBoxWidth) {
			int clickedRegion = (mouse_x - largeBoxX) / fpsRegionWidth;
			if (clickedRegion >= 0 && clickedRegion < s_NumFpsOptions) {
				int newLimit = s_FpsOptions[clickedRegion];
				config_manager::get().set_fps_limit(newLimit);
				hb::shared::render::Window::get()->set_framerate_limit(newLimit);
				audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
				return true;
			}
		}
	}

	lineY += 18;

	// --- Aspect Ratio --- (only enabled when fullscreen)
	if (is_item_visible(lineY))
	{
		const int aspectBoxY = lineY - 2;
		const int aspectRegionWidth = wideBoxWidth / 2;
		if (fullscreen && mouse_y >= aspectBoxY && mouse_y <= aspectBoxY + wideBoxHeight && mouse_x >= wideBoxX && mouse_x <= wideBoxX + wideBoxWidth) {
			bool new_stretch = (mouse_x >= wideBoxX + aspectRegionWidth);
			config_manager::get().set_fullscreen_stretch_enabled(new_stretch);
			hb::shared::render::Window::get()->set_fullscreen_stretch(new_stretch);
			if (hb::shared::render::Renderer::get())
				hb::shared::render::Renderer::get()->set_fullscreen_stretch(new_stretch);
			audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
			return true;
		}
	}

	lineY += 18;

	// --- VSync toggle ---
	if (is_item_visible(lineY))
	{
		if (is_in_toggle_area(smallBoxX, lineY)) {
			bool enabled = config_manager::get().is_vsync_enabled();
			config_manager::get().set_vsync_enabled(!enabled);
			hb::shared::render::Window::get()->set_vsync_enabled(!enabled);
			audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
			return true;
		}
	}

	lineY += 18;

	// --- Detail Level --- wide box with three regions
	if (is_item_visible(lineY))
	{
		const int boxY = lineY - 2;
		const int regionWidth = wideBoxWidth / 3;
		if (mouse_y >= boxY && mouse_y <= boxY + wideBoxHeight && mouse_x >= wideBoxX && mouse_x <= wideBoxX + wideBoxWidth) {
			if (mouse_x < wideBoxX + regionWidth) {
				config_manager::get().set_detail_level(0);
				add_event_list(NOTIFY_MSG_DETAIL_LEVEL_LOW, 10);
				audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
				return true;
			}
			if (mouse_x < wideBoxX + (regionWidth * 2)) {
				config_manager::get().set_detail_level(1);
				add_event_list(NOTIFY_MSG_DETAIL_LEVEL_MEDIUM, 10);
				audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
				return true;
			}
			config_manager::get().set_detail_level(2);
			add_event_list(NOTIFY_MSG_DETAIL_LEVEL_HIGH, 10);
			audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
			return true;
		}
	}

	lineY += 18;

	// --- Dialog Transparency toggle ---
	if (is_item_visible(lineY))
	{
		if (is_in_toggle_area(smallBoxX, lineY)) {
			bool enabled = config_manager::get().is_dialog_transparency_enabled();
			config_manager::get().set_dialog_transparency_enabled(!enabled);
			audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
			return true;
		}
	}

	lineY += 18;

	// --- Show FPS toggle ---
	if (is_item_visible(lineY))
	{
		if (is_in_toggle_area(smallBoxX, lineY)) {
			bool enabled = config_manager::get().is_show_fps_enabled();
			config_manager::get().set_show_fps_enabled(!enabled);
			audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
			return true;
		}
	}

	lineY += 18;

	// --- Show Latency toggle ---
	if (is_item_visible(lineY))
	{
		if (is_in_toggle_area(smallBoxX, lineY)) {
			bool enabled = config_manager::get().is_show_latency_enabled();
			config_manager::get().set_show_latency_enabled(!enabled);
			audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
			return true;
		}
	}

	lineY += 18;

	// --- Background FPS Throttle toggle ---
	if (is_item_visible(lineY))
	{
		if (is_in_toggle_area(smallBoxX, lineY)) {
			bool enabled = config_manager::get().is_background_fps_throttle_enabled();
			config_manager::get().set_background_fps_throttle_enabled(!enabled);
			hb::shared::render::Window::get()->set_background_fps_limit(enabled ? 0 : 5);
			audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
			return true;
		}
	}

	lineY += 18;

	// --- Map Zoom toggle ---
	if (is_item_visible(lineY))
	{
		if (is_in_toggle_area(smallBoxX, lineY)) {
			bool enabled = config_manager::get().is_zoom_map_enabled();
			config_manager::get().set_zoom_map_enabled(!enabled);
			audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
			return true;
		}
	}

	lineY += 18;

#ifdef _DEBUG
	// Tile Grid toggle - DEBUG ONLY
	if (is_item_visible(lineY))
	{
		if (is_in_toggle_area(smallBoxX, lineY)) {
			bool enabled = config_manager::get().is_tile_grid_enabled();
			config_manager::get().set_tile_grid_enabled(!enabled);
			audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
			return true;
		}
	}

	lineY += 18;

	// Patching Grid toggle - DEBUG ONLY
	if (is_item_visible(lineY))
	{
		if (is_in_toggle_area(smallBoxX, lineY)) {
			bool enabled = config_manager::get().is_patching_grid_enabled();
			config_manager::get().set_patching_grid_enabled(!enabled);
			audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
			return true;
		}
	}

	lineY += 18;
#endif

	// --- Display Mode toggle --- (wide box, toggles windowed/fullscreen)
	if (is_item_visible(lineY))
	{
		const int modeBoxY = lineY - 2;
		if (mouse_x >= wideBoxX && mouse_x <= wideBoxX + wideBoxWidth && mouse_y >= modeBoxY && mouse_y <= modeBoxY + wideBoxHeight) {
			m_game->m_Renderer->set_fullscreen(!fullscreen);
			m_game->m_Renderer->change_display_mode(hb::shared::render::Window::get_handle());
			hb::shared::input::get()->set_window_active(true);
			config_manager::get().set_fullscreen_enabled(!fullscreen);
			config_manager::get().save();
			audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
			return true;
		}
	}

	lineY += 18;

	// --- hb::shared::render::Window Style toggle --- (wide box, only in windowed mode)
	if (is_item_visible(lineY))
	{
		const int styleBoxY = lineY - 2;
		if (!fullscreen && mouse_x >= wideBoxX && mouse_x <= wideBoxX + wideBoxWidth && mouse_y >= styleBoxY && mouse_y <= styleBoxY + wideBoxHeight) {
			bool borderless = config_manager::get().is_borderless_enabled();
			config_manager::get().set_borderless_enabled(!borderless);
			hb::shared::render::Window::set_borderless(!borderless);
			hb::shared::input::get()->set_window_active(true);
			config_manager::get().save();
			audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
			return true;
		}
	}

	lineY += 18;

	// --- Resolution click --- (wide box, only in windowed mode)
	if (is_item_visible(lineY))
	{
		const int resBoxY = lineY - 2;
		if (!fullscreen && mouse_x >= wideBoxX && mouse_x <= wideBoxX + wideBoxWidth && mouse_y >= resBoxY && mouse_y <= resBoxY + wideBoxHeight) {
			cycle_resolution();
			audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
			add_event_list("Resolution changed.", 10);
			return true;
		}
	}

	return false;
}

bool DialogBox_SysMenu::on_click_audio(short sX, short sY)
{
	const int contentX = sX + CONTENT_X;
	const int contentY = sY + CONTENT_Y;
	const int toggleX = contentX + 68;

	if (!audio_manager::get().is_sound_available())
		return false;

	// Master toggle (lineY = contentY + 8)
	int lineY = contentY + 8;
	if (is_in_toggle_area(toggleX, lineY)) {
		bool enabled = audio_manager::get().is_master_enabled();
		audio_manager::get().set_master_enabled(!enabled);
		config_manager::get().set_master_enabled(!enabled);
		config_manager::get().save();
		audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
		return true;
	}

	// Effects toggle (lineY = contentY + 52)
	lineY = contentY + 52;
	if (is_in_toggle_area(toggleX, lineY)) {
		bool enabled = audio_manager::get().is_sound_enabled();
		audio_manager::get().set_sound_enabled(!enabled);
		config_manager::get().set_sound_enabled(!enabled);
		config_manager::get().save();
		audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
		return true;
	}

	// Ambient toggle (lineY = contentY + 92)
	lineY = contentY + 92;
	if (is_in_toggle_area(toggleX, lineY)) {
		bool enabled = audio_manager::get().is_ambient_enabled();
		audio_manager::get().set_ambient_enabled(!enabled);
		config_manager::get().set_ambient_enabled(!enabled);
		config_manager::get().save();
		audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
		return true;
	}

	// UI toggle (lineY = contentY + 132)
	lineY = contentY + 132;
	if (is_in_toggle_area(toggleX, lineY)) {
		bool enabled = audio_manager::get().is_ui_enabled();
		audio_manager::get().set_ui_enabled(!enabled);
		config_manager::get().set_ui_enabled(!enabled);
		config_manager::get().save();
		audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
		return true;
	}

	// Music toggle (lineY = contentY + 172)
	lineY = contentY + 172;
	if (is_in_toggle_area(toggleX, lineY)) {
		if (audio_manager::get().is_music_enabled()) {
			audio_manager::get().set_music_enabled(false);
			config_manager::get().set_music_enabled(false);
			audio_manager::get().stop_music();
		}
		else {
			audio_manager::get().set_music_enabled(true);
			config_manager::get().set_music_enabled(true);
			m_game->start_bgm();
		}
		config_manager::get().save();
		audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
		return true;
	}

	return false;
}

bool DialogBox_SysMenu::on_click_system(short sX, short sY)
{
	const int contentX = sX + CONTENT_X;
	const int contentY = sY + CONTENT_Y;
	int lineY = contentY + 5;
	const int valueX = contentX + 140;

	// Whisper toggle
	if (is_in_toggle_area(valueX, lineY)) {
		if (ChatManager::get().is_whisper_enabled()) {
			ChatManager::get().set_whisper_enabled(false);
			add_event_list(BCHECK_LOCAL_CHAT_COMMAND7, 10);
		}
		else {
			ChatManager::get().set_whisper_enabled(true);
			add_event_list(BCHECK_LOCAL_CHAT_COMMAND6, 10);
		}
		audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
		return true;
	}

	lineY += 20;

	// Shout toggle
	if (is_in_toggle_area(valueX, lineY)) {
		if (ChatManager::get().is_shout_enabled()) {
			ChatManager::get().set_shout_enabled(false);
			add_event_list(BCHECK_LOCAL_CHAT_COMMAND9, 10);
		}
		else {
			ChatManager::get().set_shout_enabled(true);
			add_event_list(BCHECK_LOCAL_CHAT_COMMAND8, 10);
		}
		audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
		return true;
	}

	lineY += 20;

	// Running Mode toggle
	if (is_in_toggle_area(valueX, lineY)) {
		bool enabled = config_manager::get().is_running_mode_enabled();
		config_manager::get().set_running_mode_enabled(!enabled);
		audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
		return true;
	}

	lineY += 20;

	// Capture Mouse toggle
	if (is_in_toggle_area(valueX, lineY)) {
		bool enabled = config_manager::get().is_mouse_capture_enabled();
		config_manager::get().set_mouse_capture_enabled(!enabled);
		hb::shared::render::Window::get()->set_mouse_capture_enabled(!enabled);
		audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
		return true;
	}

	lineY += 20;

	// Guide Map toggle
	if (is_in_toggle_area(valueX, lineY)) {
		if (m_game->get_dialog_box_manager().is_enabled(DialogBoxId::GuideMap))
			disable_dialog_box(DialogBoxId::GuideMap);
		else
			enable_dialog_box(DialogBoxId::GuideMap, 0, 0, 0);
		audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
		return true;
	}

	lineY += 20;

	// Reduced Motion toggle
	if (is_in_toggle_area(valueX, lineY)) {
		bool enabled = config_manager::get().is_reduced_motion_enabled();
		config_manager::get().set_reduced_motion_enabled(!enabled);
		audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
		return true;
	}

	lineY += 20;

	// Toggle to Chat toggle
	if (is_in_toggle_area(valueX, lineY)) {
		bool enabled = config_manager::get().is_toggle_to_chat_enabled();
		config_manager::get().set_toggle_to_chat_enabled(!enabled);
		audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
		return true;
	}

	return false;
}
