#pragma once

// ResolutionConfig - Singleton that provides resolution-dependent values
//
// This allows the game to run at different base resolutions (640x480 or 800x600)
// while keeping all resolution-dependent calculations centralized.
//
// The base resolution determines:
// - Logical render target size
// - Number of visible tiles
//
// Window resolution (display size) is separate and can scale the base resolution.

// Hard-coded base resolution — change these to switch between 640x480 and 800x600
static constexpr int BASE_RESOLUTION_WIDTH  = 800;
static constexpr int BASE_RESOLUTION_HEIGHT = 600;

namespace hb::shared::render {

class ResolutionConfig
{
public:
	// initialize with window resolution - must be called before get()
	// Base resolution is compile-time constant (BASE_RESOLUTION_WIDTH x BASE_RESOLUTION_HEIGHT)
	// windowWidth/windowHeight: actual window size for centering calculations
	static void initialize(int windowWidth, int windowHeight);

	// get the singleton instance (must call initialize first)
	static ResolutionConfig& get();

	// Update window size (for when window is resized)
	void set_window_size(int windowWidth, int windowHeight);

	// Logical resolution (render target size)
	int logical_width() const { return m_width; }
	int logical_height() const { return m_height; }

	// Screen offset for centering content in window
	// Auto-calculated based on window size vs logical size
	int screen_x() const { return m_screenX; }
	int screen_y() const { return m_screenY; }

	// Max coordinates for bounds checking
	int logical_max_x() const { return m_width - 1; }
	int logical_max_y() const { return m_height - 1; }

	// Tile counts based on resolution
	// Tiles are 32x32 pixels
	int view_tile_width() const { return m_width / 32; }
	// view_tile_height accounts for HUD at bottom (53px)
	// 640x480: (480-53)/32 = 13 tiles, 800x600: (600-53)/32 = 17 tiles
	int view_tile_height() const { return (m_height - 53) / 32; }

	// Center tile position (player position on screen)
	// X is symmetric: player centered horizontally
	int view_center_tile_x() const { return view_tile_width() / 2; }
	// Y includes +1 offset for player sprite positioning (previously added in camera calc)
	// This gives symmetric view: 800x600 = 8 up/8 down, 640x480 = 6 up/6 down
	int view_center_tile_y() const { return view_tile_height() / 2 + 1; }

	// HUD panel dimensions
	int icon_panel_width() const { return m_width; }
	int icon_panel_height() const { return 53; }
	int icon_panel_offset_x() const { return 0; }

	// Chat input position
	int chat_input_x() const { return 10; }
	int chat_input_y() const { return m_height - icon_panel_height() - 16; }

	// Event list base Y position
	int event_list2_base_y() const { return chat_input_y() - (6 * 15) - 4; }

	// Level up text position
	int level_up_text_x() const { return m_width - 90; }
	int level_up_text_y() const { return event_list2_base_y() + (5 * 15); }

	// Check if using high resolution mode
	bool is_high_resolution() const { return m_width >= 800; }

	// Menu offset - for centering 640x480 menu backgrounds in larger resolutions
	// At 800x600: menu_offset_x = (800-640)/2 = 80, menu_offset_y = (600-480)/2 = 60
	// At 640x480: menu_offset_x = 0, menu_offset_y = 0
	int menu_offset_x() const { return 0; } //{ return (m_width - 640) / 2; }
	int menu_offset_y() const { return 0; } //{ return (m_height - 480) / 2; }

private:
	ResolutionConfig() = default;
	~ResolutionConfig() = default;
	ResolutionConfig(const ResolutionConfig&) = delete;
	ResolutionConfig& operator=(const ResolutionConfig&) = delete;

	void recalculate_screen_offset();

	int m_width = BASE_RESOLUTION_WIDTH;
	int m_height = BASE_RESOLUTION_HEIGHT;
	int m_windowWidth = BASE_RESOLUTION_WIDTH;
	int m_windowHeight = BASE_RESOLUTION_HEIGHT;
	int m_screenX = 0;
	int m_screenY = 0;

	static bool s_initialized;
};

} // namespace hb::shared::render
