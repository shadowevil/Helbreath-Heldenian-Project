#include "dialog_box_sys_menu.h"
#include "game.h"
#include "chat_manager.h"
#include "dialog_box_manager.h"
#include "global_def.h"
#include "lan_eng.h"
#include "audio_manager.h"
#include "config_manager.h"
#include "input.h"
#include "renderer_factory.h"
#include "text_renderer.h"
#include "panel_kit.h"
#include "ccontrols.h"
#include "screen_on_game.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <format>
#include <string>

namespace render = hb::shared::render;
using hb::client::kit_rect;
using hb::shared::render::Color;

namespace
{
	// ---- Layout (mockup b2, dialog-relative)
	constexpr int dlg_w = 420;
	constexpr int dlg_h = 303;
	constexpr kit_rect tab_column{14, 42, 96, 247};
	constexpr kit_rect pane{116, 42, 290, 220};
	constexpr int tab_x = 20;
	constexpr int tab_y = 50;
	constexpr int tab_w = 84;
	constexpr int tab_h = 22;
	constexpr int tab_step = 26;
	constexpr int logout_y = 259;
	constexpr int restart_y = logout_y - tab_step;
	constexpr int head_x = pane.x + 10;
	constexpr int head_w = pane.w - 30;
	constexpr int label_x = pane.x + 14;
	constexpr int ctl_x = 248;
	constexpr int ctl_w = 140;
	constexpr int row_h = 19;
	constexpr int head_h = 16;
	constexpr int first_row_y = pane.y + 12;   // a row's y is its text's cap middle; controls sit 8 px above it
	constexpr int clip_top = pane.y + 4;
	constexpr int clip_bottom = pane.y + pane.h - 4;
	constexpr kit_rect scroll_rect{pane.x + pane.w - 17, pane.y + 4, 15, pane.h - 8};
	constexpr int slider_x = ctl_x + 20;   // after the row's on/off box
	constexpr int slider_w = 116;          // 100 px of travel; the knob centre is 8 px in at volume 0
	constexpr int slider_knob_x = 8;
	constexpr int footer_y = 276;
	constexpr int footer_button_y = 266;
	constexpr int title_y = 22;   // the sprite title's cap top (measured, HH-0065)
	constexpr int list_rows = 8;

	// ---- The keep-or-revert popup (mockup b2 panel 3), centred on the menu
	constexpr int popup_w = 315;
	constexpr int popup_h = 120;
	constexpr uint32_t confirm_ms = 15000;

	// ---- Text colours (mockup b2)
	constexpr Color label_color{28, 22, 16};
	constexpr Color label_disabled{78, 66, 52};
	constexpr Color heading_color{60, 40, 24};
	constexpr Color value_color{60, 40, 24};
	constexpr Color pending_color{120, 40, 24};

	namespace control_id
	{
		enum type
		{
			frame = 1,
			main,
			popup,
			keep,
			revert_popup,
			pane_area,
			tab_area,
			tab_first,                          // one per tab
			logout = tab_first + 8,
			restart,
			scrollbar,
			apply,
			revert,
			page_first,                         // one per tab
		};
	}

	// The display model in effect (the window's, which config_manager mirrors)
	const render::display_settings& current_display()
	{
		return render::Window::get()->get_display_settings();
	}

	// Apply a display change through the graphics protocol (nothing saved); false when the window had to fall back
	bool apply_to_window(const render::display_settings& settings)
	{
		render::IWindow* window = render::Window::get();
		if (!window)
			return false;
		const bool exact = window->apply_display_settings(settings);
		if (hb::shared::input::get())
			hb::shared::input::get()->set_window_active(true);
		return exact;
	}

	// Save what took effect. The saved size stays unless the player picked another resolution: the window's size
	// may be the start-up snap to a smaller desktop, which is for this session only. After a resolution change
	// the size in effect is saved (a mode the monitor answered with a nearby size)
	void persist_display(const render::display_settings& requested, const render::display_settings& before)
	{
		render::display_settings saved = current_display();
		const bool resized = requested.width != before.width || requested.height != before.height;
		if (!resized)
		{
			const render::display_settings& config = config_manager::get().get_display_settings();
			saved.width = config.width;
			saved.height = config.height;
		}
		config_manager::get().set_display_settings(saved);
	}

	// A change that applies at once (VSync, window border): applied and saved
	void apply_display_now(const render::display_settings& request)
	{
		const render::display_settings before = current_display();   // a copy: the apply replaces it
		apply_to_window(request);
		persist_display(request, before);
	}

	const char* window_mode_text(render::window_mode::type mode)
	{
		switch (mode)
		{
		case render::window_mode::borderless_fullscreen: return "Borderless Fullscreen";
		case render::window_mode::exclusive_fullscreen: return "Exclusive Fullscreen";
		default: return "Windowed";
		}
	}

	std::string date_time_text()
	{
		const std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		std::tm local_time{};
#ifdef _WIN32
		localtime_s(&local_time, &t);
#else
		localtime_r(&t, &local_time);
#endif
		const int hour12 = local_time.tm_hour % 12 == 0 ? 12 : local_time.tm_hour % 12;
		return std::format("{:02}/{:02}/{:04} {}:{:02} {}", local_time.tm_mon + 1, local_time.tm_mday,
			local_time.tm_year + 1900, hour12, local_time.tm_min, local_time.tm_hour < 12 ? "AM" : "PM");
	}

	// Hide or grey a control without leaving it a stale click. CControls stops updating a hidden or disabled
	// control (and its children), but the collection still fires any button whose last update saw a click, so
	// that click would fire again on the next mouse release anywhere (Apply re-applying when Keep was clicked).
	// One idle update first clears it.
	void quiesce(cc::control& c)
	{
		cc::input_state idle;
		idle.mouse_x = -10000;
		idle.mouse_y = -10000;
		c.update(idle, idle);
	}

	void show(cc::control& c, bool visible)
	{
		if (!visible && c.is_visible())
			quiesce(c);
		c.set_visible(visible);
	}

	void enable(cc::control& c, bool enabled)
	{
		if (!enabled && c.is_enabled())
			quiesce(c);
		c.set_enabled(enabled);
	}

	void play_click()
	{
		audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
	}

	// A kit small button drawn at the control, lit while highlighted or when lit() says so
	cc::render_handler kit_button_handler(const hb::client::panel_kit& kit, std::function<std::string()> text,
		std::function<bool()> lit, const float* alpha)
	{
		return [&kit, text = std::move(text), lit = std::move(lit), alpha](const cc::control& c)
		{
			const cc::rect sb = c.screen_bounds();
			kit.button(kit.style_of(hb::client::button_layout::small), text(), {sb.x, sb.y, sb.w, sb.h},
				c.is_highlighted() || (lit && lit()), alpha ? *alpha : 1.0f);
		};
	}
}

DialogBox_SysMenu::DialogBox_SysMenu(CGame* game)
	: IDialogBox(DialogBoxId::SystemMenu, game)
{
	set_default_rect(192, 67, dlg_w, dlg_h);
	build_controls();
}

// =============================================================================
// BUILDING
// =============================================================================
void DialogBox_SysMenu::build_controls()
{
	const hb::client::panel_kit& kit = m_game->get_panel_kit();
	m_controls.set_click_sound(play_click);
	m_frame = m_controls.add<cc::panel>(control_id::frame, cc::rect{0, 0, dlg_w, dlg_h});
	m_main = m_frame->add<cc::panel>(control_id::main, cc::rect{0, 0, dlg_w, dlg_h});
	m_pane_area = m_main->add<cc::panel>(control_id::pane_area, cc::rect{pane.x, pane.y, pane.w, pane.h});
	m_tab_area = m_main->add<cc::panel>(control_id::tab_area, cc::rect{tab_column.x, tab_column.y, tab_column.w, tab_column.h});

	// Tab column: the active tab stays lit; Restart (when dead) above Log Out / Continue at its foot
	static constexpr const char* tab_names[tab::count] = {"General", "Graphics", "Audio", "Gameplay"};
	for (int i = 0; i < tab::count; i++)
	{
		m_tabs[i] = m_main->add<cc::button>(control_id::tab_first + i, cc::rect{tab_x, tab_y + i * tab_step, tab_w, tab_h});
		m_tabs[i]->set_render_handler(kit_button_handler(kit, [i] { return std::string(tab_names[i]); },
			[this, i] { return m_active_tab == i; }, &m_alpha));
		m_tabs[i]->set_on_click([this, i](int) { m_active_tab = i; });
	}

	m_logout = m_main->add<cc::button>(control_id::logout, cc::rect{tab_x, logout_y, tab_w, tab_h});
	m_logout->set_render_handler(kit_button_handler(kit,
		[this] { return std::string(m_game->on_game()->m_logout_count == -1 ? "Log Out" : "Continue"); }, nullptr, &m_alpha));
	m_logout->set_on_click([this](int)
	{
		if (m_game->m_force_disconn)
			return;
		if (m_game->on_game()->m_logout_count == -1)
		{
			m_game->on_game()->m_logout_count = 11;
			m_game->on_game()->m_logout_count_time = GameClock::get_time_ms();
		}
		else
		{
			m_game->on_game()->m_logout_count = -1;
			add_event_list(DLGBOX_CLICK_SYSMENU2, 10);
			disable_this_dialog();
		}
	});

	m_restart = m_main->add<cc::button>(control_id::restart, cc::rect{tab_x, restart_y, tab_w, tab_h});
	m_restart->set_render_handler(kit_button_handler(kit, [] { return std::string("Restart"); }, nullptr, &m_alpha));
	m_restart->set_on_click([this](int)
	{
		m_game->m_restart_count = 5;
		m_game->m_restart_count_time = GameClock::get_time_ms();
		disable_this_dialog();
		add_event_list(std::format(DLGBOX_CLICK_SYSMENU1, m_game->m_restart_count).c_str(), 10);
	});

	// The pages, then the scrollbar over them
	for (int i = 0; i < tab::count; i++)
	{
		m_pages[i].panel = m_main->add<cc::panel>(control_id::page_first + i, cc::rect{0, 0, dlg_w, dlg_h});
		m_row_y = first_row_y;
		switch (i)
		{
		case tab::general: build_general(m_pages[i].panel); break;
		case tab::graphics: build_graphics(m_pages[i].panel); break;
		case tab::audio: build_audio(m_pages[i].panel); break;
		default: build_gameplay(m_pages[i].panel); break;
		}
		m_pages[i].content_bottom = m_row_y - row_h + 8;
	}

	m_scrollbar = m_main->add<cc::button>(control_id::scrollbar, cc::rect{scroll_rect.x, scroll_rect.y, scroll_rect.w, scroll_rect.h});
	m_scrollbar->set_click_sound(nullptr);
	m_scrollbar->set_render_handler([this, &kit](const cc::control& c)
	{
		const pane_page& p = m_pages[m_active_tab];
		const int max_scroll = max_scroll_of(p);
		const cc::rect sb = c.screen_bounds();
		kit.scrollbar({sb.x, sb.y, sb.w, sb.h}, max_scroll > 0 ? static_cast<float>(p.scroll) / max_scroll : 0.0f, m_alpha);
	});

	// Footer: Revert and Apply while a display change is staged
	m_revert = m_main->add<cc::button>(control_id::revert, cc::rect{dlg_w - 14 - 74 - 80, footer_button_y, 74, 20});
	m_revert->set_render_handler(kit_button_handler(kit, [] { return std::string("Revert"); }, nullptr, &m_alpha));
	m_revert->set_on_click([this](int) { m_staged = current_display(); });
	m_apply = m_main->add<cc::button>(control_id::apply, cc::rect{dlg_w - 14 - 74, footer_button_y, 74, 20});
	m_apply->set_render_handler(kit_button_handler(kit, [] { return std::string("Apply"); }, [] { return true; }, &m_alpha));
	m_apply->set_on_click([this](int) { apply_pending(); });

	build_popup();
}

void DialogBox_SysMenu::build_popup()
{
	const hb::client::panel_kit& kit = m_game->get_panel_kit();
	m_popup = m_frame->add<cc::panel>(control_id::popup, cc::rect{(dlg_w - popup_w) / 2, (dlg_h - popup_h) / 2, popup_w, popup_h});
	m_popup->set_visible(false);
	// A rivet-cornered notice with the question and the countdown; opaque whatever the transparency setting
	m_popup->set_render_handler([this, &kit](const cc::control& c)
	{
		const cc::rect sb = c.screen_bounds();
		const hb::client::kit_dialog popup(kit, hb::client::kit_point{popup_w, popup_h}, hb::client::kit_point{sb.x, sb.y}, 1.0f,
			hb::client::panel_style{"stone_cloth", "wood_light", "rivet"});
		popup.inset({14, 14, 287, 58});
		const int cx = sb.x + popup_w / 2;
		kit.text_recolored("field_label", "Keep these display settings?", cx, sb.y + 32, hb::client::text_anchor::cap, heading_color);
		const uint32_t now = GameClock::get_time_ms();
		const uint32_t left_ms = m_confirm.deadline_ms > now ? m_confirm.deadline_ms - now : 0;
		kit.text_recolored("field_label", std::format("Reverting in {} seconds", (left_ms + 999) / 1000), cx, sb.y + 54,
			hb::client::text_anchor::cap, label_color);
	});
	auto* keep = m_popup->add<cc::button>(control_id::keep, cc::rect{popup_w / 2 - 80, 84, 74, 20});
	keep->set_render_handler(kit_button_handler(kit, [] { return std::string("Keep"); }, [] { return true; }, nullptr));
	keep->set_on_click([this](int) { keep_display(); });
	auto* revert = m_popup->add<cc::button>(control_id::revert_popup, cc::rect{popup_w / 2 + 6, 84, 74, 20});
	revert->set_render_handler(kit_button_handler(kit, [] { return std::string("Revert"); }, nullptr, nullptr));
	revert->set_on_click([this](int) { revert_display(); });
}

void DialogBox_SysMenu::add_heading(cc::panel* page, const char* text)
{
	const hb::client::panel_kit& kit = m_game->get_panel_kit();
	auto* head = page->add<cc::label>(m_next_id++, cc::rect{head_x, m_row_y - 8, head_w, 16}, text);
	// Small heading at the left, a groove rule from its ink to the row's end
	head->set_render_handler([this, &kit](const cc::control& c)
	{
		const auto& l = static_cast<const cc::label&>(c);
		const cc::rect sb = c.screen_bounds();
		const int cy = sb.y + 8;
		kit.text_recolored("field_label", l.text(), sb.x, cy, hb::client::text_anchor::lcap, heading_color);
		const int rule_x = sb.x + kit.text_width("field_label", l.text()) + 6;
		kit.rule(rule_x, cy, sb.x + sb.w - rule_x, m_alpha);
	});
	m_row_y += head_h;
}

cc::label* DialogBox_SysMenu::add_label(cc::panel* page, const char* text)
{
	const hb::client::panel_kit& kit = m_game->get_panel_kit();
	auto* label = page->add<cc::label>(m_next_id++, cc::rect{label_x, m_row_y - 8, ctl_x - label_x - 4, 16}, text);
	label->set_render_handler([this, &kit](const cc::control& c)
	{
		const cc::rect sb = c.screen_bounds();
		kit.text_recolored("field_label", static_cast<const cc::label&>(c).text(), sb.x, sb.y + 8, hb::client::text_anchor::lcap,
			c.is_effectively_enabled() ? label_color : label_disabled);
	});
	return label;
}

cc::check_box* DialogBox_SysMenu::add_check(cc::panel* page, const char* text, std::function<void(bool)> on_change)
{
	cc::label* label = add_label(page, text);
	auto* box = page->add<cc::check_box>(m_next_id++, cc::rect{ctl_x, m_row_y - 7, 13, 13});
	box->set_on_change([on_change = std::move(on_change)](int, bool checked) { on_change(checked); });
	hb::client::skin_check_box(*box, m_game->get_panel_kit());
	m_label_links.emplace_back(label, box);
	m_row_y += row_h;
	return box;
}

cc::dropdown* DialogBox_SysMenu::add_dropdown(cc::panel* page, const char* text, std::function<void(int)> on_change)
{
	cc::label* label = add_label(page, text);
	auto* list = page->add<cc::dropdown>(m_next_id++, cc::rect{ctl_x, m_row_y - 8, ctl_w, 16});
	list->set_max_visible_rows(list_rows);
	list->set_on_change([on_change = std::move(on_change)](int, int index) { on_change(index); });
	hb::client::skin_dropdown(*list, m_game->get_panel_kit());
	m_label_links.emplace_back(label, list);
	m_row_y += row_h;
	return list;
}

void DialogBox_SysMenu::add_info(cc::panel* page, const char* text, std::function<std::string()> value)
{
	const hb::client::panel_kit& kit = m_game->get_panel_kit();
	add_label(page, text);
	auto* field = page->add<cc::label>(m_next_id++, cc::rect{ctl_x + 2, m_row_y - 8, ctl_w - 2, 16});
	field->set_render_handler([this, &kit, value = std::move(value)](const cc::control& c)
	{
		const cc::rect sb = c.screen_bounds();
		kit.text_recolored("field_label", value(), sb.x, sb.y + 8, hb::client::text_anchor::lcap, value_color);
	});
	m_row_y += row_h;
}

void DialogBox_SysMenu::add_volume(cc::panel* page, const char* text, volume_row row)
{
	const hb::client::panel_kit& kit = m_game->get_panel_kit();
	const int index = static_cast<int>(m_volumes.size());
	cc::label* label = add_label(page, text);
	row.enabled = page->add<cc::check_box>(m_next_id++, cc::rect{ctl_x, m_row_y - 7, 13, 13});
	hb::client::skin_check_box(*row.enabled, kit);
	m_label_links.emplace_back(label, row.enabled);

	// The slider is a button: a press on it drags the knob (handle_drags) until the mouse button is up
	row.slider = page->add<cc::button>(m_next_id++, cc::rect{slider_x, m_row_y - 8, slider_w, 16});
	row.slider->set_click_sound(nullptr);
	row.slider->set_render_handler([this, &kit, index](const cc::control& c)
	{
		const cc::rect sb = c.screen_bounds();
		// A groove for the track and the kit's framed box (btn_box_b, user 2026-11-25) for the knob
		kit.groove({sb.x + 2, sb.y + 6, sb.w - 4, 5}, m_alpha);
		kit.piece("btn_box_b", sb.x + slider_knob_x + m_volumes[index].get() - 7, sb.y, m_alpha);
	});
	m_volumes.push_back(std::move(row));
	m_row_y += row_h;
}

void DialogBox_SysMenu::build_general(cc::panel* page)
{
	// Information only (user, 2026-11-25): room for more later
	add_heading(page, "Information");
	add_info(page, "Server", [] { return std::string(UPDATE_SCREEN_ON_SELECT_CHARACTER36); });
	add_info(page, "Date / Time", [] { return date_time_text(); });
	add_info(page, "Ping", [this] { return m_game->m_latency_ms > 0 ? std::format("{} ms", m_game->m_latency_ms) : std::string("-"); });
}

void DialogBox_SysMenu::build_graphics(cc::panel* page)
{
	auto check = [this, page](const char* text, std::function<bool()> get, std::function<void(bool)> set)
	{
		m_checks.emplace_back(add_check(page, text, std::move(set)), std::move(get));
	};

	// Display: staged, except VSync and the window border, which apply at once
	add_heading(page, "Display");
	m_window_mode = add_dropdown(page, "Window Mode", [this](int i) { m_staged.mode = m_modes[i]; });
	m_resolution = add_dropdown(page, "Resolution", [this](int i)
	{
		m_staged.width = m_resolutions[i].width;
		m_staged.height = m_resolutions[i].height;
	});
	m_refresh = add_dropdown(page, "Refresh Rate", [this](int i) { m_staged.refresh_rate = m_refresh_rates[i].rate; });
	m_vsync = add_check(page, "VSync", [](bool on)
	{
		render::display_settings request = current_display();
		request.vsync = on;
		apply_display_now(request);
	});
	m_aspect = add_dropdown(page, "Aspect", [this](int i)
	{
		m_staged.aspect = i == 1 ? render::aspect_mode::stretch : render::aspect_mode::letterbox;
	});
	m_aspect->set_options({"Letterbox", "Stretch"});
	m_window_border = add_check(page, "Borderless Window", [](bool on)
	{
		render::display_settings request = current_display();
		request.windowed_borderless = on;
		apply_display_now(request);
	});

	add_heading(page, "Quality");
	m_detail = add_dropdown(page, "Detail Level", [this](int i)
	{
		config_manager::get().set_detail_level(i);
		static constexpr const char* messages[] = {NOTIFY_MSG_DETAIL_LEVEL_LOW, NOTIFY_MSG_DETAIL_LEVEL_MEDIUM, NOTIFY_MSG_DETAIL_LEVEL_HIGH};
		add_event_list(messages[std::clamp(i, 0, 2)], 10);
	});
	m_detail->set_options({"Low", "Normal", "High"});
	check("Transparency", [] { return config_manager::get().is_dialog_transparency_enabled(); },
		[](bool on) { config_manager::get().set_dialog_transparency_enabled(on); });
	check("Antialias Font", [] { return config_manager::get().is_font_smoothing_enabled(); }, [](bool on)
	{
		config_manager::get().set_font_smoothing_enabled(on);
		if (hb::shared::text::GetTextRenderer())
			hb::shared::text::GetTextRenderer()->set_font_smoothing(on);
	});
	check("Antialias Buffer", [] { return config_manager::get().is_upscale_smoothing_enabled(); }, [](bool on)
	{
		config_manager::get().set_upscale_smoothing_enabled(on);
		if (render::Renderer::get())
			render::Renderer::get()->set_upscale_smoothing(on);
	});
	// On = a zoomed tier (mid or in), off = the whole map
	check("Map Zoom", [] { return config_manager::get().get_minimap_zoom_level() > 0; },
		[](bool on) { config_manager::get().set_minimap_zoom_level(on ? 1 : 0); });

	add_heading(page, "Interface");
	check("Show FPS", [] { return config_manager::get().is_show_fps_enabled(); },
		[](bool on) { config_manager::get().set_show_fps_enabled(on); });
	check("Show Latency", [] { return config_manager::get().is_show_latency_enabled(); },
		[](bool on) { config_manager::get().set_show_latency_enabled(on); });
	check("Power Saving", [] { return config_manager::get().is_background_fps_throttle_enabled(); }, [](bool on)
	{
		config_manager::get().set_background_fps_throttle_enabled(on);
		render::Window::get()->set_background_fps_limit(on ? 5 : 0);
	});
#ifdef _DEBUG
	check("Tile Grid", [] { return config_manager::get().is_tile_grid_enabled(); },
		[](bool on) { config_manager::get().set_tile_grid_enabled(on); });
	check("Patching Grid", [] { return config_manager::get().is_patching_grid_enabled(); },
		[](bool on) { config_manager::get().set_patching_grid_enabled(on); });
#endif
}

void DialogBox_SysMenu::build_audio(cc::panel* page)
{
	// A row: its on/off box (is_on / set_on, saved at once) and its slider (get / set live, save on release)
	auto volume = [this, page](const char* text, std::function<bool()> is_on, std::function<void(bool)> set_on,
		std::function<int()> get, std::function<void(int)> set, std::function<void(int)> save)
	{
		volume_row row;
		row.get = get;
		row.set = std::move(set);
		row.save = [get, save = std::move(save)] { save(get()); };
		add_volume(page, text, std::move(row));
		cc::check_box* box = m_volumes.back().enabled;
		box->set_on_change([set_on = std::move(set_on)](int, bool on)
		{
			set_on(on);
			config_manager::get().save();
		});
		m_checks.emplace_back(box, std::move(is_on));
	};
	audio_manager& audio = audio_manager::get();
	config_manager& config = config_manager::get();
	add_heading(page, "Volume");
	volume("Master", [&audio] { return audio.is_master_enabled(); },
		[&audio, &config](bool on) { audio.set_master_enabled(on); config.set_master_enabled(on); },
		[&audio] { return audio.get_master_volume(); }, [&audio](int v) { audio.set_master_volume(v); },
		[&config](int v) { config.set_master_volume(v); });
	volume("Effects", [&audio] { return audio.is_sound_enabled(); },
		[&audio, &config](bool on) { audio.set_sound_enabled(on); config.set_sound_enabled(on); },
		[&audio] { return audio.get_sound_volume(); }, [&audio](int v) { audio.set_sound_volume(v); },
		[&config](int v) { config.set_sound_volume(v); });
	volume("Ambient", [&audio] { return audio.is_ambient_enabled(); },
		[&audio, &config](bool on) { audio.set_ambient_enabled(on); config.set_ambient_enabled(on); },
		[&audio] { return audio.get_ambient_volume(); }, [&audio](int v) { audio.set_ambient_volume(v); },
		[&config](int v) { config.set_ambient_volume(v); });
	volume("UI", [&audio] { return audio.is_ui_enabled(); },
		[&audio, &config](bool on) { audio.set_ui_enabled(on); config.set_ui_enabled(on); },
		[&audio] { return audio.get_ui_volume(); }, [&audio](int v) { audio.set_ui_volume(v); },
		[&config](int v) { config.set_ui_volume(v); });
	volume("Music", [&audio] { return audio.is_music_enabled(); },
		[this, &audio, &config](bool on)
		{
			audio.set_music_enabled(on);
			config.set_music_enabled(on);
			if (on)
				m_game->start_bgm();
			else
				audio.stop_music();
		},
		[&audio] { return audio.get_music_volume(); }, [&audio](int v) { audio.set_music_volume(v); },
		[&config](int v) { config.set_music_volume(v); });
}

void DialogBox_SysMenu::build_gameplay(cc::panel* page)
{
	auto check = [this, page](const char* text, std::function<bool()> get, std::function<void(bool)> set)
	{
		m_checks.emplace_back(add_check(page, text, std::move(set)), std::move(get));
	};
	add_heading(page, "Gameplay");
	check("Running Mode", [] { return config_manager::get().is_running_mode_enabled(); },
		[](bool on) { config_manager::get().set_running_mode_enabled(on); });
	check("Capture Mouse", [] { return config_manager::get().is_mouse_capture_enabled(); }, [](bool on)
	{
		config_manager::get().set_mouse_capture_enabled(on);
		render::Window::get()->set_mouse_capture_enabled(on);
	});
	check("Reduced Motion", [] { return config_manager::get().is_reduced_motion_enabled(); },
		[](bool on) { config_manager::get().set_reduced_motion_enabled(on); });
	check("Toggle to Chat", [] { return config_manager::get().is_toggle_to_chat_enabled(); },
		[](bool on) { config_manager::get().set_toggle_to_chat_enabled(on); });

	add_heading(page, "Chat and Map");
	check("Whisper", [] { return ChatManager::get().is_whisper_enabled(); }, [this](bool on)
	{
		ChatManager::get().set_whisper_enabled(on);
		add_event_list(on ? BCHECK_LOCAL_CHAT_COMMAND6 : BCHECK_LOCAL_CHAT_COMMAND7, 10);
	});
	check("Shout", [] { return ChatManager::get().is_shout_enabled(); }, [this](bool on)
	{
		ChatManager::get().set_shout_enabled(on);
		add_event_list(on ? BCHECK_LOCAL_CHAT_COMMAND8 : BCHECK_LOCAL_CHAT_COMMAND9, 10);
	});
	check("Guide Map", [this] { return m_game->get_dialog_box_manager().is_enabled(DialogBoxId::GuideMap); }, [this](bool on)
	{
		if (on)
			enable_dialog_box(DialogBoxId::GuideMap, 0, 0, 0);
		else
			disable_dialog_box(DialogBoxId::GuideMap);
	});
}

// =============================================================================
// PER FRAME
// =============================================================================
int DialogBox_SysMenu::max_scroll_of(const pane_page& p)
{
	const int over = p.content_bottom - clip_bottom;
	return over > 0 ? (over + row_h - 1) / row_h * row_h : 0;
}

bool DialogBox_SysMenu::any_list_open() const
{
	for (const cc::dropdown* list : {m_window_mode, m_resolution, m_refresh, m_aspect, m_detail})
	{
		if (list->is_open())
			return true;
	}
	return false;
}

cc::input_state DialogBox_SysMenu::controls_input()
{
	namespace input = hb::shared::input;
	cc::input_state state;
	// The wheel as a level: the deltas this dialog has seen (update_all runs once per presented frame)
	m_wheel_delta = input::get_mouse_wheel_delta();
	m_wheel_total += m_wheel_delta;
	state.mouse_wheel = m_wheel_total;

	const bool left = input::is_mouse_button_down(input::MouseButton::Left);
	// An open list sees every press, so a click anywhere (off the menu too) closes it
	const bool ours = !input::is_suppressed() && (any_list_open() || manager().dialog_at_mouse() == DialogBoxId::SystemMenu);
	if (left && !m_prev_left)
		m_press_ours = ours;
	m_prev_left = left;
	if (left ? m_press_ours : ours)
	{
		state.mouse_x = input::get_mouse_x();
		state.mouse_y = input::get_mouse_y();
		state.mouse_left_down = left;
	}
	else
	{
		state.mouse_x = -10000;
		state.mouse_y = -10000;
	}
	return state;
}

void DialogBox_SysMenu::sync_controls()
{
	const bool confirming = m_confirm.active;
	show(*m_popup, confirming);
	enable(*m_main, !confirming);
	show(*m_restart, player().m_hp <= 0 && m_game->m_restart_count == -1);
	const bool pending = is_display_pending();
	show(*m_apply, pending);
	show(*m_revert, pending);

	for (auto& [box, get] : m_checks)
		box->set_checked(get());
	const bool sound = audio_manager::get().is_sound_available();
	for (volume_row& row : m_volumes)
		enable(*row.enabled, sound);

	// Display rows: the staged picks, greyed by the staged mode (Resolution under borderless shows the desktop,
	// Aspect is for the fullscreen modes); Refresh Rate is the frame cap, so VSync greys it outside exclusive
	const render::display_settings& current = current_display();
	const auto mode_row = std::find(m_modes.begin(), m_modes.end(), m_staged.mode);
	m_window_mode->set_selected(mode_row != m_modes.end() ? static_cast<int>(mode_row - m_modes.begin()) : 0);

	const bool borderless = m_staged.mode == render::window_mode::borderless_fullscreen;
	int resolution_row = 0;
	if (borderless)
	{
		const auto desktop = std::find_if(m_resolutions.begin(), m_resolutions.end(),
			[](const render::resolution_option& r) { return r.desktop; });
		resolution_row = desktop != m_resolutions.end() ? static_cast<int>(desktop - m_resolutions.begin()) : 0;
	}
	else if (const render::resolution_option* nearest = render::nearest_resolution(m_resolutions, m_staged.width, m_staged.height))
		resolution_row = static_cast<int>(nearest - m_resolutions.data());
	m_resolution->set_selected(resolution_row);
	enable(*m_resolution, !borderless);

	int refresh_row = 0;
	for (size_t i = 0; i < m_refresh_rates.size(); i++)
	{
		if (std::abs(m_refresh_rates[i].rate - m_staged.refresh_rate) < std::abs(m_refresh_rates[refresh_row].rate - m_staged.refresh_rate))
			refresh_row = static_cast<int>(i);
	}
	m_refresh->set_selected(refresh_row);
	enable(*m_refresh, !current.vsync || m_staged.mode == render::window_mode::exclusive_fullscreen);

	m_vsync->set_checked(current.vsync);
	m_aspect->set_selected(m_staged.aspect == render::aspect_mode::stretch ? 1 : 0);
	enable(*m_aspect, m_staged.mode != render::window_mode::windowed);
	m_window_border->set_checked(current.windowed_borderless);
	enable(*m_window_border, current.mode == render::window_mode::windowed && m_staged.mode == render::window_mode::windowed);
	m_detail->set_selected(config_manager::get().get_detail_level());

	for (auto& [label, control] : m_label_links)
		label->set_enabled(control->is_enabled());
	// The kit skins draw at the control's opacity: the dialog transparency setting
	m_main->walk([this](cc::control& c) { c.set_opacity(m_alpha); });
}

void DialogBox_SysMenu::layout_page()
{
	for (int i = 0; i < tab::count; i++)
		show(*m_pages[i].panel, i == m_active_tab);

	// Rows move by the scroll; a row not wholly inside the pane is hidden (CControls does not clip)
	pane_page& p = m_pages[m_active_tab];
	p.scroll = std::clamp(p.scroll, 0, max_scroll_of(p));
	p.panel->set_local_bounds({0, -p.scroll, dlg_w, dlg_h});
	for (const auto& child : p.panel->children())
	{
		const int top = child->local_bounds().y - p.scroll;
		show(*child, top >= clip_top && top + child->local_bounds().h <= clip_bottom);
	}
	show(*m_scrollbar, max_scroll_of(p) > 0);
}

void DialogBox_SysMenu::handle_wheel(int notches)
{
	// Over the pane the wheel scrolls a page that does not fit; over the tab column it steps the tabs
	pane_page& p = m_pages[m_active_tab];
	if (m_pane_area->is_hovered() && max_scroll_of(p) > 0)
		p.scroll = std::clamp(p.scroll - notches * row_h, 0, max_scroll_of(p));
	else if (m_tab_area->is_hovered())
	{
		m_active_tab = (m_active_tab - notches + tab::count * 4) % tab::count;
		play_click();
	}
}

void DialogBox_SysMenu::handle_drags()
{
	// A held control keeps its hold while the mouse button is down, wherever the mouse goes
	if (m_scrollbar->is_held())
	{
		pane_page& p = m_pages[m_active_tab];
		const cc::rect sb = m_scrollbar->screen_bounds();
		const float pos = m_game->get_panel_kit().scrollbar_pos_at({sb.x, sb.y, sb.w, sb.h}, m_input.mouse_y);
		p.scroll = static_cast<int>(std::lround(pos * (max_scroll_of(p) / row_h))) * row_h;
	}
	for (int i = 0; i < static_cast<int>(m_volumes.size()); i++)
	{
		volume_row& row = m_volumes[i];
		if (row.slider->is_held())
		{
			const int knob_x = row.slider->screen_bounds().x + slider_knob_x;
			row.set(std::clamp(m_input.mouse_x - knob_x, 0, 100));
			m_dragged_volume = i;
		}
		else if (m_dragged_volume == i)
		{
			row.save();   // saved once, when the drag ends
			m_dragged_volume = -1;
		}
	}
}

void DialogBox_SysMenu::update_controls()
{
	m_frame->set_local_bounds({m_x, m_y, dlg_w, dlg_h});
	// A list opens upward rather than past the dialog's bottom, so it never hangs over the world
	m_controls.set_screen_size(m_x + dlg_w, m_y + dlg_h);
	m_alpha = hb::client::dialog_alpha(config_manager::get().is_dialog_transparency_enabled());

	// Nothing staged: the rows follow what is in effect (a change made elsewhere, an exclusive mode's answer)
	if (!m_confirm.active && !is_display_pending())
		m_staged = current_display();
	sync_controls();
	layout_page();

	m_input = controls_input();
	if (!m_input.mouse_left_down)
		m_is_scroll_selected = false;
	// The wheel: an open list scrolls itself (the collection hands it the wheel); otherwise this dialog's own
	const bool list_open = any_list_open();
	const int wheel = m_wheel_delta;
	m_controls.update(m_input, GameClock::get_time_ms());
	handle_drags();
	if (wheel != 0 && !list_open && !any_list_open() && !m_confirm.active)
		handle_wheel(wheel > 0 ? 1 : -1);
}

// =============================================================================
// DISPLAY STAGING
// =============================================================================
void DialogBox_SysMenu::rebuild_display_options()
{
	const render::IWindow* window = render::Window::get();
	if (!window)
		return;
	m_modes.clear();
	std::vector<std::string> labels;
	for (const render::window_mode::type mode : {render::window_mode::windowed, render::window_mode::borderless_fullscreen,
		render::window_mode::exclusive_fullscreen})
	{
		if (window->is_window_mode_supported(mode))
		{
			m_modes.push_back(mode);
			labels.emplace_back(window_mode_text(mode));
		}
	}
	m_window_mode->set_options(std::move(labels));

	m_resolutions = window->get_resolution_options();
	labels.clear();
	for (const render::resolution_option& option : m_resolutions)
		labels.push_back(option.label);
	m_resolution->set_options(std::move(labels));

	m_refresh_rates = window->get_refresh_options();
	labels.clear();
	for (const render::refresh_option& option : m_refresh_rates)
		labels.push_back(option.label);
	m_refresh->set_options(std::move(labels));
}

bool DialogBox_SysMenu::is_display_pending() const
{
	if (m_confirm.active)
		return false;
	const render::display_settings& current = current_display();
	// Borderless always uses the desktop, so its size is not a pick
	const bool resized = m_staged.mode != render::window_mode::borderless_fullscreen &&
		(m_staged.width != current.width || m_staged.height != current.height);
	return m_staged.mode != current.mode || m_staged.refresh_rate != current.refresh_rate ||
		m_staged.aspect != current.aspect || resized;
}

void DialogBox_SysMenu::apply_pending()
{
	render::display_settings request = current_display();
	request.mode = m_staged.mode;
	request.refresh_rate = m_staged.refresh_rate;
	request.aspect = m_staged.aspect;
	if (m_staged.mode != render::window_mode::borderless_fullscreen)
	{
		request.width = m_staged.width;
		request.height = m_staged.height;
	}
	apply_staged(request);
	m_staged = current_display();   // what took effect (an exclusive mode can answer with a nearby size)
}

void DialogBox_SysMenu::apply_staged(const render::display_settings& request)
{
	m_confirm.before = current_display();   // a copy: the apply replaces it
	m_confirm.requested = request;
	if (!apply_to_window(request))
		add_event_list("The display could not be set exactly; the closest setting is shown.", 10);
	m_confirm.deadline_ms = GameClock::get_time_ms() + confirm_ms;
	m_confirm.active = true;
}

void DialogBox_SysMenu::keep_display()
{
	if (!m_confirm.active)
		return;
	m_confirm.active = false;
	persist_display(m_confirm.requested, m_confirm.before);
}

void DialogBox_SysMenu::revert_display()
{
	if (!m_confirm.active)
		return;
	m_confirm.active = false;
	// What was in effect before Apply: the saved settings never changed, so there is nothing to save. The staged
	// rows go back with it, or they would show the reverted change as pending
	if (!apply_to_window(m_confirm.before))
		add_event_list("The display could not be restored exactly; the closest setting is shown.", 10);
	// Teardown can reach here (on_uninitialize) after the window is gone
	if (render::Window::get())
		m_staged = current_display();
}

// =============================================================================
// DIALOG HOOKS
// =============================================================================
bool DialogBox_SysMenu::on_enable(int, int64_t, int, const char*)
{
	// Every way out without Keep reverts, so a confirm still marked active on a reopen is stale: drop it, never
	// revert. (enable_dialog_box also calls this on an open menu; its popup stays.)
	if (!is_enabled())
		m_confirm = {};
	rebuild_display_options();
	m_staged = current_display();
	// The click that opened the menu is not a click on its controls
	m_controls.discard_pending_input(controls_input());
	return true;
}

bool DialogBox_SysMenu::on_disable()
{
	// Closing the menu while the popup asks is not a Keep
	revert_display();
	for (cc::dropdown* list : {m_window_mode, m_resolution, m_refresh, m_aspect, m_detail})
		list->close();
	return true;
}

void DialogBox_SysMenu::on_update()
{
	if (m_confirm.active && static_cast<int32_t>(GameClock::get_time_ms() - m_confirm.deadline_ms) >= 0)
		revert_display();
	update_controls();
}

void DialogBox_SysMenu::on_draw()
{
	const hb::client::panel_kit& kit = m_game->get_panel_kit();
	const hb::client::kit_dialog frame(kit, hb::client::kit_point{dlg_w, dlg_h}, hb::client::kit_point{m_x, m_y}, m_alpha,
		hb::client::panel_style{"stone_cloth", "wood_light", "gold"});
	constexpr std::string_view title = "System Menu";
	frame.title(title, title_y, kit_layout::title_fill);
	// The waves 22 px from the title's ink each side, smaller than elsewhere (0.56)
	const int wave_y = frame.title_cap_middle(title, title_y);
	const int reach = kit.text_width("title_red", title) / 2 + 22;
	frame.flourish("divider_wave", dlg_w / 2 - reach, wave_y, false, 0.56f);
	frame.flourish("divider_wave", dlg_w / 2 + reach, wave_y, true, 0.56f);
	frame.text_area(tab_column, 0.8f);
	frame.inset(pane);

	// Footer: a staged display change, else (outside the information page) that changes apply at once. Text stays
	// opaque under dialog transparency, as the sprite menu's did
	if (is_display_pending())
		kit.text_recolored("field_label", "Display change pending", m_x + label_x, m_y + footer_y, hb::client::text_anchor::lcap, pending_color);
	else if (m_active_tab != tab::general && !m_confirm.active)
		kit.text_recolored("field_label", "Changes apply at once", m_x + label_x, m_y + footer_y, hb::client::text_anchor::lcap, label_disabled);

	m_controls.render();
}

bool DialogBox_SysMenu::on_click()
{
	// The controls act on the release themselves (update_controls); the click stays with the menu
	return true;
}

PressResult DialogBox_SysMenu::on_press()
{
	// A press on a control, an open list or the popup is not the start of a dialog drag
	if (m_confirm.active || any_list_open())
		return PressResult::ScrollClaimed;
	const int mx = hb::shared::input::get_mouse_x();
	const int my = hb::shared::input::get_mouse_y();
	bool on_control = false;
	m_main->walk([&](const cc::control& c)
	{
		on_control = on_control || (c.is_focusable() && c.is_effectively_visible() && c.is_effectively_enabled() &&
			cc::rect_contains(c.screen_bounds(), mx, my));
	});
	return on_control ? PressResult::ScrollClaimed : PressResult::Normal;
}
