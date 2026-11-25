// screen_main_menu.cpp: Main Menu Screen implementation
//
//////////////////////////////////////////////////////////////////////

#include "screen_main_menu.h"
#include "game.h"
#include "game_mode_manager.h"
#include "input.h"
#include "global_def.h"
#include "renderer.h"
#include "audio_manager.h"
#include "sprite_factory.h"
#include "log.h"
#include <array>
#include <cmath>
#include <string>

namespace
{

// Art pixels of interface/front_end/main_menu.png (HH-0082): the user's sheet, backdrop at the top, the
// button plate (translucent white, about 20%) and two label rows below it (black, gold)
constexpr float art_width = 1920.0f;
constexpr float art_height = 1080.0f;
constexpr float plate_x = 16.0f;
constexpr float plate_y = 1100.0f;
constexpr float plate_w = 296.0f;
constexpr float plate_h = 39.0f;
constexpr float label_black_y = 1108.0f;
constexpr float label_gold_y = 1148.0f;
constexpr float label_h = 24.0f;

// Plates and labels draw at 1.3x their sheet size (user, 2026-11-25)
constexpr float item_scale = 1.3f;
constexpr float item_plate_w = plate_w * item_scale;
constexpr float item_plate_h = plate_h * item_scale;
constexpr float item_label_h = label_h * item_scale;

// The original 800x600 menu maps onto the art's centre 4:3 region (x 240-1680, 1.8 art px per UI unit)
constexpr float classic_art_x = 240.0f;
constexpr float classic_art_scale = 1.8f;

struct menu_item
{
	int id;           // its cc::button
	GameMode mode;    // where a click goes
	float src_x;      // label in the sheet
	float src_w;
	cc::rect classic; // the original 800x600 button, whose centre places the item (user, 2026-11-25)
};

// Log In, New Account, Exit; in focus order
constexpr std::array<menu_item, 3> items{{
	{Screen_MainMenu::BTN_LOGIN, GameMode::Login, 337.0f, 111.0f, {465, 238, 164, 22}},
	{Screen_MainMenu::BTN_NEW_ACCOUNT, GameMode::CreateNewAccount, 476.0f, 246.0f, {465, 276, 164, 22}},
	{Screen_MainMenu::BTN_QUIT, GameMode::Quit, 750.0f, 72.0f, {465, 315, 164, 22}},
}};

// An item's centre in art pixels: its original button's centre through the classic mapping
float item_centre_x(int index)
{
	const cc::rect& r = items[static_cast<size_t>(index)].classic;
	return classic_art_x + (static_cast<float>(r.x) + static_cast<float>(r.w) / 2.0f) * classic_art_scale;
}

float item_centre_y(int index)
{
	const cc::rect& r = items[static_cast<size_t>(index)].classic;
	return (static_cast<float>(r.y) + static_cast<float>(r.h) / 2.0f) * classic_art_scale;
}

} // namespace

Screen_MainMenu::Screen_MainMenu(CGame* game)
	: IGameScreen(game)
{
}

void Screen_MainMenu::on_initialize()
{
	m_game->m_arrow_pressed = 0;

	const std::string art_path = hb::shared::sprite::Sprites::get_sprite_path() + "/interface/front_end/main_menu.png";
	m_art.reset(m_game->m_Renderer->create_texture_from_file(art_path.c_str()));
	if (!m_art)
		hb::logger::error("Main menu art not loaded: {}", art_path);

	m_controls.set_screen_size(LOGICAL_WIDTH(), LOGICAL_HEIGHT());
	m_controls.set_hover_focus(false);

	for (int index = 0; index < static_cast<int>(items.size()); index++)
	{
		const menu_item& item = items[static_cast<size_t>(index)];
		auto* button = m_controls.add<cc::button>(item.id, cc::rect{0, 0, 1, 1});
		button->set_on_click([this, mode = item.mode](int) { m_game->change_game_mode(mode); });
		button->set_click_sound([] { audio_manager::get().play_game_sound(sound_type::effect, 14, 5); });
		button->set_render_handler([this, index](const cc::control& c) { draw_item(index, c.is_highlighted()); });
	}

	m_controls.set_focus_order({BTN_LOGIN, BTN_NEW_ACCOUNT, BTN_QUIT});
	m_controls.set_focus(BTN_LOGIN);
	update_layout();
}

void Screen_MainMenu::on_uninitialize()
{
	m_art.reset();
}

void Screen_MainMenu::update_layout()
{
	m_fit = hb::shared::render::cover_fit(m_game->m_Renderer->get_screen_extent(), art_width, art_height);

	// Each button's hit area is its plate, centred on its label
	for (int index = 0; index < static_cast<int>(items.size()); index++)
	{
		const float left = m_fit.x(item_centre_x(index) - item_plate_w / 2.0f);
		const float top = m_fit.y(item_centre_y(index) - item_plate_h / 2.0f);
		const float right = m_fit.x(item_centre_x(index) + item_plate_w / 2.0f);
		const float bottom = m_fit.y(item_centre_y(index) + item_plate_h / 2.0f);
		const int x = static_cast<int>(std::lround(left));
		const int y = static_cast<int>(std::lround(top));
		if (cc::control* button = m_controls.find(items[static_cast<size_t>(index)].id))
			button->set_local_bounds(cc::rect{x, y, static_cast<int>(std::lround(right)) - x, static_cast<int>(std::lround(bottom)) - y});
	}
}

void Screen_MainMenu::draw_item(int index, bool highlighted) const
{
	if (!m_art)
		return;

	hb::shared::render::IRenderer* renderer = m_game->m_Renderer;
	const menu_item& label = items[static_cast<size_t>(index)];
	const float scale = m_fit.scale;
	const float centre_x = item_centre_x(index);
	const float centre_y = item_centre_y(index);
	const float label_w = label.src_w * item_scale;

	// Like the original menu (user, 2026-11-25): every button always on its plate (added to the backdrop),
	// the label black, turning gold when hovered or focused
	renderer->draw_texture_region(m_art.get(), plate_x, plate_y, plate_w, plate_h,
		m_fit.x(centre_x - item_plate_w / 2.0f), m_fit.y(centre_y - item_plate_h / 2.0f),
		item_plate_w * scale, item_plate_h * scale, 1.0f,
		hb::shared::render::draw_tint{hb::shared::render::Color::White(), hb::shared::render::BlendMode::Additive});
	renderer->draw_texture_region(m_art.get(), label.src_x, highlighted ? label_gold_y : label_black_y, label.src_w, label_h,
		m_fit.x(centre_x - label_w / 2.0f), m_fit.y(centre_y - item_label_h / 2.0f),
		label_w * scale, item_label_h * scale);
}

void Screen_MainMenu::on_update()
{
	update_layout();
	update_controls(m_controls);
}

void Screen_MainMenu::on_render()
{
	// m_fit is from this frame's on_update (or on_initialize on the frame the screen is switched in)
	if (m_art)
	{
		m_game->m_Renderer->draw_texture_region(m_art.get(), 0.0f, 0.0f, art_width, art_height,
			m_fit.origin_x, m_fit.origin_y, art_width * m_fit.scale, art_height * m_fit.scale);
	}
	m_controls.render();
	draw_version();
}
