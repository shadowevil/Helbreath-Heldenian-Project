#pragma once
 
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>
#include "primitive_types.h"

namespace hb::shared::render { class IRenderer; }

// ============================================================================
// Profiling stage IDs - add new stages here
// ============================================================================

enum class ProfileStage {
	update,              // UpdateScreen logic
	ClearBuffer,         // ClearBackB4
	draw_background,     // Map tiles
	draw_effect_lights,  // Lighting effects
	draw_objects,        // Characters, NPCs, items
	draw_effects,        // Particle effects
	DrawWeather,         // Weather effects
	DrawChat,            // Chat messages
	DrawDialogs,         // Dialog boxes/UI
	DrawMisc,            // Misc rendering (tooltips, cursor, etc.)
	Flip,                // flip to display
	FrameTotal,          // Total frame time
	COUNT                // Must be last
};

// ============================================================================
// FrameTiming: Per-frame delta timing and stage profiling
//
// FPS and frame counting are handled engine-side (hb::shared::render::IRenderer::get_fps).
// FrameTiming provides: Per-frame delta and per-stage profiling.
//
// Usage:
//   FrameTiming::initialize();  // Call once at startup
//
//   // In game loop:
//   FrameTiming::begin_frame();
//   // ... update and render ...
//   FrameTiming::end_frame();
//
//   double dt = FrameTiming::get_delta_time();  // Seconds since last frame
//
// Profiling Usage:
//   FrameTiming::begin_profile(ProfileStage::draw_objects);
//   draw_objects();
//   FrameTiming::end_profile(ProfileStage::draw_objects);
//
//   double ms = FrameTiming::get_profile_time_ms(ProfileStage::draw_objects);
// ============================================================================

class FrameTiming
{
public:
	static void initialize();
	static void begin_frame();
	static void end_frame();

	// Accessors
	static double get_delta_time();        // Seconds since last frame
	static double get_delta_time_ms();     // Milliseconds since last frame

	// Profiling
	static void set_profiling_enabled(bool enabled);
	static bool is_profiling_enabled();
	static void set_frame_rendered(bool rendered);  // Call after skip check in RenderFrame
	static void begin_profile(ProfileStage stage);
	static void end_profile(ProfileStage stage);
	static double get_profile_time_ms(ProfileStage stage);      // Current frame time
	static double get_profile_avg_time_ms(ProfileStage stage);  // Averaged over ~1 second
	static const char* get_stage_name(ProfileStage stage);

private:
	using Clock = std::chrono::steady_clock;
	using TimePoint = std::chrono::steady_clock::time_point;

	static TimePoint s_frameStart;        // Frame start time
	static TimePoint s_lastFrame;         // Previous frame start

	static double s_deltaTime;            // Current delta in seconds
	static double s_accumulator;          // Time accumulator for profile averaging
	static uint32_t s_profileFrameCount;  // Frames accumulated for averaging

	// Profiling data
	static constexpr int STAGE_COUNT = static_cast<int>(ProfileStage::COUNT);
	static bool s_profilingEnabled;
	static bool s_bFrameRendered;  // True when RenderFrame actually presents
	static TimePoint s_stageStart[STAGE_COUNT];
	static double s_stageTimeMS[STAGE_COUNT];     // Current frame timing
	static double s_stageAccumMS[STAGE_COUNT];    // Accumulated for averaging
	static double s_stageAvgMS[STAGE_COUNT];      // Averaged timing (updated each second)
};

// ============================================================================
// performance_monitor: Compact overlay bar drawn at the top of the viewport.
// Shows FPS, latency, and frame timing as modular float-left items.
// Hover over the frame time item to expand a profiling breakdown panel
// with a stacked bar graph and per-stage timing.
// ============================================================================

class performance_monitor
{
public:
	static performance_monitor& get();

	// Render the bar overlay and optional hover panel.
	// Call after all game/UI content, before cursor and flip.
	void render(hb::shared::render::IRenderer* renderer, int latency_ms);

private:
	performance_monitor() = default;
	~performance_monitor() = default;
	performance_monitor(const performance_monitor&) = delete;
	performance_monitor& operator=(const performance_monitor&) = delete;

	// Bar layout
	static constexpr int bar_height = 18;
	static constexpr int item_margin = 12;
	static constexpr int bar_padding_x = 8;
	static constexpr int font_size = 11;

	// Hover panel layout
	static constexpr int panel_padding = 10;
	static constexpr int panel_line_height = 12;
	static constexpr int graph_height = 60;
	static constexpr int graph_width = 280;
	static constexpr int graph_history_size = 120; // seconds of history for line graph

	struct bar_item
	{
		std::string text;
		hb::shared::render::Color color;
		int width = 0;
		int x = 0;
	};

	// Hover state (persists across frames for smooth panel)
	bool m_hover_active = false;

	// Frame time history ring buffer (one sample per second, averaged)
	double m_frame_history[graph_history_size] = {};
	int m_history_write = 0;
	bool m_history_filled = false;

	// Per-second accumulator
	double m_accum_ms = 0.0;
	int m_accum_frames = 0;
	double m_accum_timer = 0.0;

	void update_history();  // Called each frame, pushes averaged sample each second
	void render_hover_panel(hb::shared::render::IRenderer* renderer, int anchor_x);
};
