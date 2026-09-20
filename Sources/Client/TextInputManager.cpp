#include "TextInputManager.h"
#include "textbox.h"
#include "InputStateHelper.h"
#include "TextFieldRenderer.h"
#include "GameFonts.h"
#include "TextLibExt.h"
#include "CommonTypes.h"
#include "IInput.h"

static constexpr int TEXTBOX_ID = 1;
static constexpr int TEXTBOX_WIDTH = 800;

text_input_manager& text_input_manager::get()
{
	static text_input_manager instance;
	return instance;
}

text_input_manager::text_input_manager()
{
	m_controls.set_clipboard_provider(
		[] { return hb::shared::input::get_clipboard_text(); },
		[](const std::string& s) { hb::shared::input::set_clipboard_text(s); }
	);
}

void text_input_manager::start_input(int x, int y, unsigned char max_len, std::string& buffer,
                                     bool hidden, std::string_view char_filter)
{
	// Flush stale characters that accumulated while no input was active
	hb::shared::input::take_typed_chars();

	m_controls.clear();

	int line_height = hb::shared::text::get_line_height(GameFont::Default);
	auto* tb = m_controls.add<cc::textbox>(TEXTBOX_ID, cc::rect{x, y, TEXTBOX_WIDTH, line_height}, max_len);

	if (hidden)
		tb->set_hidden(true);
	if (!char_filter.empty())
		tb->set_character_filter(char_filter);

	tb->set_render_handler([](const cc::control&) {
		// Rendering handled in render() via draw_text_field
	});

	// Copy caller's buffer into textbox
	tb->text() = buffer;

	// Set focus and rebuild focus order
	m_controls.set_focus_order({TEXTBOX_ID});
	m_controls.set_focus(TEXTBOX_ID);

	m_active_textbox = tb;
	m_buffer = &buffer;
	m_is_active = true;
	m_show_chat_bg = false;
}

void text_input_manager::end_input()
{
	if (m_is_active && m_active_textbox && m_buffer)
		*m_buffer = m_active_textbox->text();

	m_controls.clear();
	m_active_textbox = nullptr;
	m_buffer = nullptr;
	m_is_active = false;
}

void text_input_manager::clear_input()
{
	if (m_active_textbox)
	{
		m_active_textbox->text().clear();
		m_active_textbox->move_to_start(false);
	}
	if (m_buffer)
		m_buffer->clear();
}

void text_input_manager::update(uint32_t time_ms)
{
	if (!m_is_active)
		return;

	cc::input_state input{};
	hb::client::fill_input_state(input);
	m_controls.update(input, time_ms);

	// Sync textbox text back to caller's buffer each frame
	if (m_active_textbox && m_buffer)
		*m_buffer = m_active_textbox->text();
}

void text_input_manager::render()
{
	if (!m_is_active || !m_active_textbox)
		return;

	hb::client::draw_text_field(*m_active_textbox, 0,
		hb::shared::text::TextStyle::with_shadow(GameColors::InputNormal),
		hb::shared::text::TextStyle::with_shadow(GameColors::InputNormal));
}

void text_input_manager::show_input()
{
	render();
}

std::string text_input_manager::get_input_string() const
{
	if (m_active_textbox)
		return m_active_textbox->text();
	if (m_buffer)
		return *m_buffer;
	return {};
}
