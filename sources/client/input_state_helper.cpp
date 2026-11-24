#include "input_state_helper.h"
#include "input.h"
#include "renderer_factory.h"
#include "render_constants.h"
#include <algorithm>

namespace MouseButton = hb::shared::input::MouseButton;

namespace hb::client {

hb::shared::geometry::GamePoint mouse_in_world_units()
{
	hb::shared::geometry::GamePoint mouse(hb::shared::input::get_mouse_x(), hb::shared::input::get_mouse_y());
	if (auto* renderer = hb::shared::render::Renderer::get())
		mouse = renderer->ui_to_world(mouse);
	return mouse;
}

hb::shared::geometry::GamePoint world_mouse()
{
	hb::shared::geometry::GamePoint mouse = mouse_in_world_units();
	mouse.x = std::clamp(mouse.x, 0, LOGICAL_MAX_X());
	mouse.y = std::clamp(mouse.y, 0, LOGICAL_MAX_Y());
	return mouse;
}

void fill_input_state(cc::input_state& input)
{
	// When input is suppressed (overlay active over base screen), hide mouse
	// and skip consuming typed chars / key state so the overlay receives them.
	bool suppressed = hb::shared::input::is_suppressed();

	if (suppressed)
	{
		input.mouse_x = -10000;
		input.mouse_y = -10000;
		input.mouse_left_down = false;
		input.typed_char_count = 0;
		return;
	}

	// Mouse
	input.mouse_x = hb::shared::input::get_mouse_x();
	input.mouse_y = hb::shared::input::get_mouse_y();
	input.mouse_left_down = hb::shared::input::is_mouse_button_down(MouseButton::Left);

	// Navigation keys
	input.key_tab = hb::shared::input::is_key_down(KeyCode::Tab);
	input.key_shift = hb::shared::input::is_shift_down();
	input.key_enter = hb::shared::input::is_key_down(KeyCode::Enter);
	input.key_escape = hb::shared::input::is_key_down(KeyCode::Escape);
	input.key_up = hb::shared::input::is_key_down(KeyCode::Up);
	input.key_down = hb::shared::input::is_key_down(KeyCode::Down);
	input.key_left = hb::shared::input::is_key_down(KeyCode::Left);
	input.key_right = hb::shared::input::is_key_down(KeyCode::Right);
	input.key_space = hb::shared::input::is_key_down(KeyCode::Space);

	// Text input keys
	input.key_ctrl = hb::shared::input::is_ctrl_down();
	input.key_home = hb::shared::input::is_key_down(KeyCode::Home);
	input.key_end = hb::shared::input::is_key_down(KeyCode::End);
	input.key_delete = hb::shared::input::is_key_down(KeyCode::Delete);
	input.key_backspace = hb::shared::input::is_key_down(KeyCode::Backspace);
	input.key_a = hb::shared::input::is_key_down(KeyCode::A);
	input.key_c = hb::shared::input::is_key_down(KeyCode::C);
	input.key_x = hb::shared::input::is_key_down(KeyCode::X);
	input.key_v = hb::shared::input::is_key_down(KeyCode::V);

	// Typed characters (from engine text input buffer)
	auto chars = hb::shared::input::take_typed_chars();
	int count = static_cast<int>(chars.size());
	if (count > cc::input_state::max_typed_chars)
		count = cc::input_state::max_typed_chars;
	for (int i = 0; i < count; i++)
		input.typed_chars[i] = chars[i];
	input.typed_char_count = count;
}

} // namespace hb::client
