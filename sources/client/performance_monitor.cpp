#include "performance_monitor.h"
#include "config_manager.h"
#include "game_fonts.h"
#include "common_types.h"
#include "renderer.h"
#include "text_lib.h"
#include "render_constants.h"
#include "input.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <format>

// ============================================================================
// FrameTiming implementation
// ============================================================================

FrameTiming::TimePoint FrameTiming::s_frameStart;
FrameTiming::TimePoint FrameTiming::s_lastFrame;

double FrameTiming::s_deltaTime = 0.0;
double FrameTiming::s_accumulator = 0.0;
uint32_t FrameTiming::s_profileFrameCount = 0;

bool FrameTiming::s_profilingEnabled = false;
bool FrameTiming::s_bFrameRendered = false;
FrameTiming::TimePoint FrameTiming::s_stageStart[FrameTiming::STAGE_COUNT];
double FrameTiming::s_stageTimeMS[FrameTiming::STAGE_COUNT] = { 0 };
double FrameTiming::s_stageAccumMS[FrameTiming::STAGE_COUNT] = { 0 };
double FrameTiming::s_stageAvgMS[FrameTiming::STAGE_COUNT] = { 0 };

void FrameTiming::initialize()
{
	s_lastFrame = Clock::now();
	s_deltaTime = 0.0;
	s_accumulator = 0.0;
	s_profileFrameCount = 0;

	s_profilingEnabled = false;
	std::fill(s_stageTimeMS, s_stageTimeMS + STAGE_COUNT, 0.0);
	std::fill(s_stageAccumMS, s_stageAccumMS + STAGE_COUNT, 0.0);
	std::fill(s_stageAvgMS, s_stageAvgMS + STAGE_COUNT, 0.0);
}

void FrameTiming::begin_frame()
{
	s_frameStart = Clock::now();

	// Calculate delta time in seconds
	auto elapsed = std::chrono::duration<double>(s_frameStart - s_lastFrame);
	s_deltaTime = elapsed.count();

	// clamp delta to prevent spiral of death (e.g., debugger pause)
	if (s_deltaTime > 0.1) s_deltaTime = 0.1;

	// reset per-frame profiling times
	s_bFrameRendered = false;
	if (s_profilingEnabled)
	{
		std::fill(s_stageTimeMS, s_stageTimeMS + STAGE_COUNT, 0.0);
		begin_profile(ProfileStage::FrameTotal);
	}
}

void FrameTiming::end_frame()
{
	// End total frame profiling — only count frames that were actually rendered
	if (s_profilingEnabled && s_bFrameRendered)
	{
		end_profile(ProfileStage::FrameTotal);

		// Accumulate for averaging
		for (int i = 0; i < STAGE_COUNT; i++)
		{
			s_stageAccumMS[i] += s_stageTimeMS[i];
		}
		s_profileFrameCount++;
	}

	s_lastFrame = s_frameStart;
	s_accumulator += s_deltaTime;

	// Cap accumulator to prevent drift after long pauses (e.g., debugger, system sleep)
	if (s_accumulator > 2.0) s_accumulator = 0.0;

	// update profile averages every second
	if (s_accumulator >= 1.0)
	{
		s_accumulator -= 1.0;

		if (s_profilingEnabled && s_profileFrameCount > 0)
		{
			for (int i = 0; i < STAGE_COUNT; i++)
			{
				s_stageAvgMS[i] = s_stageAccumMS[i] / s_profileFrameCount;
				s_stageAccumMS[i] = 0.0;
			}
			s_profileFrameCount = 0;
		}
	}
}

double FrameTiming::get_delta_time()
{
	return s_deltaTime;
}

double FrameTiming::get_delta_time_ms()
{
	return s_deltaTime * 1000.0;
}

void FrameTiming::set_profiling_enabled(bool enabled)
{
	if (enabled && !s_profilingEnabled)
	{
		// reset when enabling
		s_profileFrameCount = 0;
		std::fill(s_stageTimeMS, s_stageTimeMS + STAGE_COUNT, 0.0);
		std::fill(s_stageAccumMS, s_stageAccumMS + STAGE_COUNT, 0.0);
		std::fill(s_stageAvgMS, s_stageAvgMS + STAGE_COUNT, 0.0);
	}
	s_profilingEnabled = enabled;
}

bool FrameTiming::is_profiling_enabled()
{
	return s_profilingEnabled;
}

void FrameTiming::set_frame_rendered(bool rendered)
{
	s_bFrameRendered = rendered;
}

void FrameTiming::begin_profile(ProfileStage stage)
{
	if (!s_profilingEnabled) return;
	int idx = static_cast<int>(stage);
	if (idx >= 0 && idx < STAGE_COUNT)
	{
		s_stageStart[idx] = Clock::now();
	}
}

void FrameTiming::end_profile(ProfileStage stage)
{
	if (!s_profilingEnabled) return;
	int idx = static_cast<int>(stage);
	if (idx >= 0 && idx < STAGE_COUNT)
	{
		auto now = Clock::now();
		auto elapsed = std::chrono::duration<double, std::milli>(now - s_stageStart[idx]);
		s_stageTimeMS[idx] += elapsed.count();  // += allows nested/multiple calls
	}
}

double FrameTiming::get_profile_time_ms(ProfileStage stage)
{
	int idx = static_cast<int>(stage);
	if (idx >= 0 && idx < STAGE_COUNT)
		return s_stageTimeMS[idx];
	return 0.0;
}

double FrameTiming::get_profile_avg_time_ms(ProfileStage stage)
{
	int idx = static_cast<int>(stage);
	if (idx >= 0 && idx < STAGE_COUNT)
		return s_stageAvgMS[idx];
	return 0.0;
}

const char* FrameTiming::get_stage_name(ProfileStage stage)
{
	switch (stage)
	{
	case ProfileStage::update:           return "update";
	case ProfileStage::ClearBuffer:      return "ClearBuf";
	case ProfileStage::draw_background:   return "Background";
	case ProfileStage::draw_effect_lights: return "Lights";
	case ProfileStage::draw_objects:      return "Objects";
	case ProfileStage::draw_effects:      return "Effects";
	case ProfileStage::DrawWeather:      return "Weather";
	case ProfileStage::DrawChat:         return "Chat";
	case ProfileStage::DrawDialogs:      return "Dialogs";
	case ProfileStage::DrawMisc:         return "Misc";
	case ProfileStage::Flip:             return "Flip";
	case ProfileStage::FrameTotal:       return "Total";
	default:                             return "Unknown";
	}
}

// ============================================================================
// performance_monitor implementation
// ============================================================================

performance_monitor& performance_monitor::get()
{
	static performance_monitor instance;
	return instance;
}

void performance_monitor::update_history()
{
	// Accumulate per-frame delta times, push one averaged sample every 250ms
	double dt = FrameTiming::get_delta_time();     // seconds
	double ms = FrameTiming::get_delta_time_ms();  // milliseconds

	m_accum_ms += ms;
	m_accum_frames++;
	m_accum_timer += dt;

	if (m_accum_timer >= 0.1)
	{
		double avg = (m_accum_frames > 0) ? (m_accum_ms / m_accum_frames) : 0.0;
		m_frame_history[m_history_write] = avg;
		m_history_write = (m_history_write + 1) % graph_history_size;
		if (m_history_write == 0)
			m_history_filled = true;

		m_accum_ms = 0.0;
		m_accum_frames = 0;
		m_accum_timer -= 0.1;
	}
}

void performance_monitor::render(hb::shared::render::IRenderer* renderer, int latency_ms)
{
	auto& config = config_manager::get();
	bool show_fps = config.is_show_fps_enabled();
	bool show_latency = config.is_show_latency_enabled();
	bool show_profiling = FrameTiming::is_profiling_enabled();

	// Accumulate frame times, push one averaged sample per second
	if (show_profiling)
		update_history();

	if (!show_fps && !show_latency && !show_profiling)
		return;

	// ====================================================================
	// Build items
	// ====================================================================

	std::vector<bar_item> items;

	if (show_fps)
	{
		uint32_t fps = renderer->get_fps();
		std::string text = std::format("FPS {}", fps);

		hb::shared::render::Color color;
		if (fps >= 50)      color = {100, 255, 100};
		else if (fps >= 30) color = {255, 255, 100};
		else                color = {255, 100, 100};

		int w = hb::shared::text::measure_text(GameFont::Default, text.c_str()).width;
		items.push_back({std::move(text), color, w, 0});
	}

	if (show_latency)
	{
		std::string text;
		hb::shared::render::Color color;

		if (latency_ms >= 0)
		{
			text = std::format("PING {}ms", latency_ms);
			if (latency_ms < 80)       color = {100, 255, 100};
			else if (latency_ms < 150)  color = {255, 255, 100};
			else                        color = {255, 100, 100};
		}
		else
		{
			text = "PING --";
			color = {120, 120, 120};
		}

		int w = hb::shared::text::measure_text(GameFont::Default, text.c_str()).width;
		items.push_back({std::move(text), color, w, 0});
	}

	// Frame time (total) — hoverable to expand profiling breakdown
	int frame_item_idx = -1;
	if (show_profiling)
	{
		double total_ms = FrameTiming::get_profile_avg_time_ms(ProfileStage::FrameTotal);
		int whole = static_cast<int>(total_ms);
		int frac = static_cast<int>((total_ms - whole) * 10);
		std::string text = std::format("FRAME {}.{}ms", whole, frac);

		hb::shared::render::Color color{200, 220, 255};
		int w = hb::shared::text::measure_text(GameFont::Default, text.c_str()).width;
		frame_item_idx = static_cast<int>(items.size());
		items.push_back({std::move(text), color, w, 0});
	}

	if (items.empty())
		return;

	// ====================================================================
	// Layout: float left
	// ====================================================================

	int x = bar_padding_x;
	for (auto& item : items)
	{
		item.x = x;
		x += item.width + item_margin;
	}

	// ====================================================================
	// Draw bar background (full viewport width, semi-transparent)
	// ====================================================================

	int vw = RENDER_LOGICAL_WIDTH();
	renderer->draw_rect_filled(0, 0, vw, bar_height,
	                           hb::shared::render::Color(0, 0, 0, 160));

	// Subtle bottom edge for definition
	renderer->draw_rect_filled(0, bar_height - 1, vw, 1,
	                           hb::shared::render::Color(255, 255, 255, 20));

	// ====================================================================
	// Hover detection (frame time item + its panel)
	// ====================================================================

	bool was_hover = m_hover_active;
	m_hover_active = false;

	if (frame_item_idx >= 0)
	{
		const auto& fi = items[frame_item_idx];
		int mx = hb::shared::input::get_mouse_x();
		int my = hb::shared::input::get_mouse_y();

		// Trigger: the frame time text area in the bar
		bool in_trigger = hb::shared::input::point_in_rect(
			mx, my, fi.x - 4, 0, fi.width + 8, bar_height);

		// Panel area: only check if panel was already open (prevents side-entry)
		bool in_panel = false;
		if (was_hover)
		{
			int bc = static_cast<int>(ProfileStage::COUNT) - 1;
			int pw = graph_width + panel_padding * 2;
			int ph = panel_padding + graph_height + 8
			       + bc * panel_line_height + 4 + panel_line_height + panel_padding;
			int ppx = fi.x - 4;
			if (ppx + pw > vw) ppx = vw - pw;
			if (ppx < 0) ppx = 0;

			in_panel = hb::shared::input::point_in_rect(
				mx, my, ppx, bar_height, pw, ph);
		}

		m_hover_active = in_trigger || in_panel;
	}

	// ====================================================================
	// Highlight hovered item
	// ====================================================================

	if (m_hover_active && frame_item_idx >= 0)
	{
		const auto& fi = items[frame_item_idx];
		renderer->draw_rect_filled(fi.x - 4, 0, fi.width + 8, bar_height,
		                           hb::shared::render::Color(255, 255, 255, 20));
	}

	// ====================================================================
	// Draw separators between items
	// ====================================================================

	for (size_t i = 1; i < items.size(); i++)
	{
		int sep_x = items[i - 1].x + items[i - 1].width + item_margin / 2;
		renderer->draw_rect_filled(sep_x, 4, 1, bar_height - 8,
		                           hb::shared::render::Color(255, 255, 255, 30));
	}

	// ====================================================================
	// Draw text items (left-aligned, vertically centered in bar)
	// ====================================================================

	for (const auto& item : items)
	{
		auto style = hb::shared::text::TextStyle::from_color(item.color)
		             .with_font_size(font_size)
		             .with_shadow_style(hb::shared::text::ShadowStyle::DropShadow);
		hb::shared::text::draw_text_aligned(GameFont::Default,
		                                    item.x, 0, item.width, bar_height,
		                                    item.text.c_str(), style,
		                                    hb::shared::text::Align::MiddleLeft);
	}

	// ====================================================================
	// Draw hover panel
	// ====================================================================

	if (m_hover_active && frame_item_idx >= 0)
	{
		render_hover_panel(renderer, items[frame_item_idx].x - 4);
	}
}

void performance_monitor::render_hover_panel(hb::shared::render::IRenderer* renderer, int anchor_x)
{
	// Per-stage colors for legend swatches and text
	static constexpr hb::shared::render::Color stage_colors[] = {
		{100, 180, 255},   // update
		{180, 180, 180},   // ClearBuffer
		{80, 200, 80},     // draw_background
		{255, 220, 80},    // draw_effect_lights
		{255, 140, 60},    // draw_objects
		{200, 80, 200},    // draw_effects
		{80, 180, 220},    // DrawWeather
		{220, 220, 100},   // DrawChat
		{140, 200, 140},   // DrawDialogs
		{200, 160, 120},   // DrawMisc
		{255, 100, 100},   // Flip
	};

	int breakdown_count = static_cast<int>(ProfileStage::COUNT) - 1; // exclude FrameTotal

	// Panel geometry
	int px = anchor_x;
	int py = bar_height;
	int pw = graph_width + panel_padding * 2;
	int ph = panel_padding + graph_height + 8
	       + breakdown_count * panel_line_height
	       + 4 + panel_line_height + panel_padding;

	// Clamp to viewport
	int vw = RENDER_LOGICAL_WIDTH();
	if (px + pw > vw) px = vw - pw;
	if (px < 0) px = 0;

	// Panel background
	renderer->draw_rect_filled(px, py, pw, ph,
	                           hb::shared::render::Color(0, 0, 0, 210));

	// Top highlight line
	renderer->draw_rect_filled(px, py, pw, 1,
	                           hb::shared::render::Color(255, 255, 255, 25));

	// ====================================================================
	// Line graph — frame time history
	// ====================================================================

	int gx = px + panel_padding;
	int gy = py + panel_padding;

	// Graph background
	renderer->draw_rect_filled(gx, gy, graph_width, graph_height,
	                           hb::shared::render::Color(10, 10, 20, 200));

	// Determine visible sample count and dynamic Y-axis scale
	int sample_count = m_history_filled ? graph_history_size : m_history_write;
	double max_ms = 0.0;
	for (int i = 0; i < sample_count; i++)
		max_ms = std::max(max_ms, m_frame_history[i]);
	// Floor is always 0, ceiling rounds up to nearest 5ms (minimum 5ms)
	max_ms = std::ceil(max_ms / 5.0) * 5.0;
	if (max_ms < 5.0) max_ms = 5.0;

	// Reference lines: 60fps (16.7ms) and 30fps (33.3ms)
	auto draw_ref_line = [&](double ms_val, hb::shared::render::Color color)
	{
		if (ms_val > max_ms) return;
		int ry = gy + graph_height - static_cast<int>((ms_val / max_ms) * graph_height);
		ry = std::clamp(ry, gy, gy + graph_height - 1);
		renderer->draw_line(gx, ry, gx + graph_width, ry, color);
	};

	draw_ref_line(16.67, {80, 200, 80, 60});   // 60fps — green
	draw_ref_line(33.33, {255, 100, 100, 60});  // 30fps — red

	// Reference labels
	auto ref_style = hb::shared::text::TextStyle::from_color({180, 180, 180, 120})
	                 .with_font_size(font_size);
	if (16.67 <= max_ms)
	{
		int ry60 = gy + graph_height - static_cast<int>((16.67 / max_ms) * graph_height);
		hb::shared::text::draw_text(GameFont::Default, gx + 2, ry60 - panel_line_height,
		                            "60fps", ref_style);
	}
	if (33.33 <= max_ms)
	{
		int ry30 = gy + graph_height - static_cast<int>((33.33 / max_ms) * graph_height);
		hb::shared::text::draw_text(GameFont::Default, gx + 2, ry30 - panel_line_height,
		                            "30fps", ref_style);
	}

	// Draw the line graph (oldest → newest, left → right)
	if (sample_count >= 2)
	{
		hb::shared::render::Color line_color{120, 200, 255};
		for (int i = 1; i < sample_count; i++)
		{
			// Ring buffer index: oldest sample first
			int idx0, idx1;
			if (m_history_filled)
			{
				idx0 = (m_history_write + i - 1) % graph_history_size;
				idx1 = (m_history_write + i) % graph_history_size;
			}
			else
			{
				idx0 = i - 1;
				idx1 = i;
			}

			double ms0 = m_frame_history[idx0];
			double ms1 = m_frame_history[idx1];

			// Map to pixel coordinates
			int x0 = gx + static_cast<int>(static_cast<double>(i - 1) / (sample_count - 1) * (graph_width - 1));
			int x1 = gx + static_cast<int>(static_cast<double>(i) / (sample_count - 1) * (graph_width - 1));
			int y0 = gy + graph_height - 1 - static_cast<int>((ms0 / max_ms) * (graph_height - 1));
			int y1 = gy + graph_height - 1 - static_cast<int>((ms1 / max_ms) * (graph_height - 1));
			y0 = std::clamp(y0, gy, gy + graph_height - 1);
			y1 = std::clamp(y1, gy, gy + graph_height - 1);

			renderer->draw_line(x0, y0, x1, y1, line_color);
		}
	}

	// Graph outline
	renderer->draw_rect_outline(gx, gy, graph_width, graph_height,
	                            hb::shared::render::Color(255, 255, 255, 30));

	// ====================================================================
	// Per-stage text breakdown
	// ====================================================================

	auto text_style = hb::shared::text::TextStyle::from_color({220, 220, 230})
	                  .with_font_size(font_size)
	                  .with_shadow_style(hb::shared::text::ShadowStyle::DropShadow);

	int tx = px + panel_padding;
	int ty = gy + graph_height + 8;

	// Fixed column for values (right-aligned)
	int value_col = tx + 100;

	for (int i = 0; i < breakdown_count; i++)
	{
		ProfileStage stage = static_cast<ProfileStage>(i);
		double avg_ms = FrameTiming::get_profile_avg_time_ms(stage);

		// Color swatch
		renderer->draw_rect_filled(tx, ty + 2, 8, 8, stage_colors[i]);

		// Stage name
		auto name_style = hb::shared::text::TextStyle::from_color(stage_colors[i])
		                  .with_font_size(font_size)
		                  .with_shadow_style(hb::shared::text::ShadowStyle::DropShadow);
		hb::shared::text::draw_text(GameFont::Default, tx + 12, ty,
		                            FrameTiming::get_stage_name(stage), name_style);

		// Time value
		int whole = static_cast<int>(avg_ms);
		int frac = static_cast<int>((avg_ms - whole) * 100);
		std::string value = std::format("{}.{:02}ms", whole, frac);

		hb::shared::text::draw_text(GameFont::Default, value_col, ty,
		                            value.c_str(), text_style);

		ty += panel_line_height;
	}

	// ====================================================================
	// Total separator + total line
	// ====================================================================

	renderer->draw_rect_filled(tx, ty, graph_width, 1,
	                           hb::shared::render::Color(255, 255, 255, 30));
	ty += 4;

	double total = FrameTiming::get_profile_avg_time_ms(ProfileStage::FrameTotal);
	int tw = static_cast<int>(total);
	int tf = static_cast<int>((total - tw) * 100);
	std::string total_text = std::format("{}.{:02}ms", tw, tf);

	auto total_style = hb::shared::text::TextStyle::from_color({255, 255, 200})
	                   .with_font_size(font_size)
	                   .with_shadow_style(hb::shared::text::ShadowStyle::DropShadow);
	hb::shared::text::draw_text(GameFont::Default, tx + 12, ty,
	                            "Total", total_style);
	hb::shared::text::draw_text(GameFont::Default, value_col, ty,
	                            total_text.c_str(), total_style);
}
