#include "FloatingTextManager.h"
#include "MapData.h"
#include "IRenderer.h"
#include "GameFonts.h"
#include "TextLibExt.h"
#include "ConfigManager.h"
#include "ActionID.h"
#include "AudioManager.h"
#include "GameGeometry.h"
#include <cstdio>
#include <format>
#include <string>


using namespace hb::shared::action;

// Multibyte character kind detection (replicates CGame::get_char_kind)
static int get_char_kind(const char* str, int index)
{
	int kind = 1;
	do
	{
		if (kind == 2) kind = 3;
		else
		{
			if ((unsigned char)*str < 128) kind = 1;
			else kind = 2;
		}
		str++;
		index--;
	} while (index >= 0);
	return kind;
}
static constexpr int CharKind_HAN1 = 2; // CharKind_HAN1 equivalent

// ---------------------------------------------------------------
// Slot management
// ---------------------------------------------------------------

int floating_text_manager::find_free_slot() const
{
	for (int i = 1; i < max_messages; i++)
		if (!m_messages[i])
			return i;
	return 0;
}

int floating_text_manager::bind_to_tile(int index, int object_id, CMapData* map_data, short sX, short sY)
{
	m_messages[index]->m_object_id = object_id;
	if (!map_data->set_chat_msg_owner(static_cast<uint16_t>(object_id), sX, sY, index))
	{
		m_messages[index].reset();
		return 0;
	}
	return index;
}

// ---------------------------------------------------------------
// Add methods
// ---------------------------------------------------------------

int floating_text_manager::add_chat_text(std::string_view msg, uint32_t time, int object_id,
                                      CMapData* map_data, short sX, short sY)
{
	int i = find_free_slot();
	if (i == 0) return 0;
	m_messages[i] = std::make_unique<floating_text>(chat_text_type::player_chat, msg, time);
	return bind_to_tile(i, object_id, map_data, sX, sY);
}

int floating_text_manager::add_damage_text(damage_text_type eType, std::string_view msg, uint32_t time,
                                        int object_id, CMapData* map_data)
{
	int i = find_free_slot();
	if (i == 0) return 0;
	m_messages[i] = std::make_unique<floating_text>(eType, msg, time);
	return bind_to_tile(i, object_id, map_data, -10, -10);
}

int floating_text_manager::add_notify_text(notify_text_type eType, std::string_view msg, uint32_t time,
                                        int object_id, CMapData* map_data)
{
	int i = find_free_slot();
	if (i == 0) return 0;
	m_messages[i] = std::make_unique<floating_text>(eType, msg, time);
	return bind_to_tile(i, object_id, map_data, -10, -10);
}

int floating_text_manager::add_damage_from_value(int damage, bool last_hit, uint32_t time,
                                             int object_id, CMapData* map_data)
{
	std::string txt;

	if (damage == Sentinel::DamageImmune)
	{
		txt = "* Immune *";
		audio_manager::get().play_game_sound(sound_type::character, 17, 0, 0);
		return add_damage_text(damage_text_type::Medium, txt, time, object_id, map_data);
	}
	if (damage == Sentinel::MagicFailed)
	{
		txt = "* Failed! *";
		audio_manager::get().play_game_sound(sound_type::character, 17, 0, 0);
		return add_damage_text(damage_text_type::Medium, txt, time, object_id, map_data);
	}
	if (damage > 128)
	{
		txt = "Critical!";
		return add_damage_text(damage_text_type::Large, txt, time, object_id, map_data);
	}
	if (damage > 0)
	{
		if (last_hit && damage >= 12)
			txt = std::format("-{}Pts!", damage);
		else
			txt = std::format("-{}Pts", damage);

		damage_text_type eType;
		if (damage < 12)       eType = damage_text_type::Small;
		else if (damage < 40)  eType = damage_text_type::Medium;
		else                    eType = damage_text_type::Large;
		return add_damage_text(eType, txt, time, object_id, map_data);
	}
	return 0;
}

// ---------------------------------------------------------------
// Removal / cleanup
// ---------------------------------------------------------------

void floating_text_manager::remove_by_object_id(int object_id)
{
	for (int i = 1; i < max_messages; i++)
		if (m_messages[i] && m_messages[i]->m_object_id == object_id)
			m_messages[i].reset();
}

void floating_text_manager::release_expired(uint32_t time)
{
	for (int i = 1; i < max_messages; i++)
	{
		if (!m_messages[i]) continue;
		const auto& params = m_messages[i]->get_params();
		uint32_t total = params.m_lifetime_ms + params.m_show_delay_ms;
		if ((time - m_messages[i]->m_time) > total)
			m_messages[i].reset();
	}
}

void floating_text_manager::clear(int index)
{
	if (index >= 0 && index < max_messages)
		m_messages[index].reset();
}

void floating_text_manager::clear_all()
{
	for (int i = 0; i < max_messages; i++)
		m_messages[i].reset();
}

// ---------------------------------------------------------------
// Access / position updates
// ---------------------------------------------------------------

floating_text* floating_text_manager::get(int index)
{
	if (index >= 0 && index < max_messages)
		return m_messages[index].get();
	return nullptr;
}

void floating_text_manager::update_position(int index, short sX, short sY)
{
	if (index >= 0 && index < max_messages && m_messages[index])
	{
		m_messages[index]->m_x = sX;
		m_messages[index]->m_y = sY;
	}
}

bool floating_text_manager::is_valid(int index, int object_id) const
{
	if (index < 0 || index >= max_messages) return false;
	return m_messages[index] && m_messages[index]->m_object_id == object_id;
}

bool floating_text_manager::is_occupied(int index) const
{
	if (index < 0 || index >= max_messages) return false;
	return m_messages[index] != nullptr;
}

// ---------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------

void floating_text_manager::draw_all(short min_x, short min_y, short max_x, short max_y,
                                   uint32_t cur_time, hb::shared::render::IRenderer* renderer)
{
	for (int i = 0; i < max_messages; i++)
	{
		if (!m_messages[i]) continue;
		if (m_messages[i]->m_x < min_x || m_messages[i]->m_x > max_x ||
		    m_messages[i]->m_y < min_y || m_messages[i]->m_y > max_y)
			continue;
		draw_message(*m_messages[i], m_messages[i]->m_x, m_messages[i]->m_y, cur_time, renderer);
	}
}

void floating_text_manager::draw_single(int index, short sX, short sY,
                                      uint32_t cur_time, hb::shared::render::IRenderer* renderer)
{
	if (index < 0 || index >= max_messages || !m_messages[index]) return;
	draw_message(*m_messages[index], sX, sY, cur_time, renderer);
}

void floating_text_manager::draw_message(const floating_text& msg, short sX, short sY,
                                       uint32_t cur_time, hb::shared::render::IRenderer* renderer)
{
	const auto& params = msg.get_params();

	// Compute elapsed time
	uint32_t elapsed = cur_time - msg.m_time;
	if (elapsed < params.m_show_delay_ms) return;
	uint32_t visible = elapsed - params.m_show_delay_ms;

	// Compute rise offset via linear interpolation
	int rise;
	if (params.m_rise_duration_ms <= 0 || static_cast<int>(visible) >= params.m_rise_duration_ms)
		rise = params.m_rise_pixels;
	else
		rise = static_cast<int>((visible * params.m_rise_pixels) / params.m_rise_duration_ms);

	// Detail-level transparency
	bool is_trans = (config_manager::get().get_detail_level() != 0);

	if (params.m_use_sprite_font)
	{
		// Sprite font path: split text into 20-char lines for multi-line display
		// Note: Text encoding is EUC-KR (legacy Korean). get_char_kind detects multibyte boundaries.
		const std::string& text = msg.m_text;
		std::string msg_a, msg_b, msg_c;
		int lines = 0;
		size_t pos = 0;

		if (pos < text.size()) {
			size_t count = std::min<size_t>(20, text.size() - pos);
			if (count == 20 && pos + 20 < text.size()) {
				int ret = get_char_kind(text.c_str() + pos, 19);
				if (ret == CharKind_HAN1) count = 21;
			}
			msg_a = text.substr(pos, count);
			pos += count;
			lines = 1;
		}

		if (pos < text.size()) {
			size_t count = std::min<size_t>(20, text.size() - pos);
			if (count == 20 && pos + 20 < text.size()) {
				int ret = get_char_kind(text.c_str() + pos, 19);
				if (ret == CharKind_HAN1) count = 21;
			}
			msg_b = text.substr(pos, count);
			pos += count;
			lines = 2;
		}

		if (pos < text.size()) {
			size_t count = std::min<size_t>(20, text.size() - pos);
			if (count == 20 && pos + 20 < text.size()) {
				int ret = get_char_kind(text.c_str() + pos, 19);
				if (ret == CharKind_HAN1) count = 21;
			}
			msg_c = text.substr(pos, count);
			pos += count;
			lines = 3;
		}

		// Compute text pixel width for centering
		int size = 0;
		for (size_t i = 0; i < msg_a.size(); i++)
		{
			if (static_cast<unsigned char>(msg_a[i]) >= 128) { size += 5; i++; }
			else size += 4;
		}

		int font_id = GameFont::SprFont3_0 + params.m_font_offset;
		int base_y = sY - params.m_start_offset_y - rise;

		// magic_cast_name uses full-string width and red shadow style
		if (msg.m_category == floating_text::Category::Notify &&
		    msg.m_notify_type == notify_text_type::magic_cast_name)
		{
			int size2 = 0;
			for (size_t i = 0; i < msg.m_text.size(); i++)
				if (msg.m_text[i] != 0)
				{
					if (static_cast<unsigned char>(msg.m_text[i]) >= 128) { size2 += 5; i++; }
					else size2 += 4;
				}
			hb::shared::text::draw_text(font_id, sX - size2, base_y, msg.m_text.c_str(),
			                  hb::shared::text::TextStyle::with_two_point_shadow(GameColors::Red4x).with_additive());
		}
		else
		{
			// Damage/LevelUp/enemy_kill: yellow sprite font with multi-line support
			auto style = is_trans
				? hb::shared::text::TextStyle::from_color(GameColors::Yellow2x).with_alpha(0.7f).with_additive()
				: hb::shared::text::TextStyle::with_two_point_shadow(GameColors::Yellow2x).with_additive();

			switch (lines) {
			case 1:
				hb::shared::text::draw_text(font_id, sX - size, base_y, msg_a.c_str(), style);
				break;
			case 2:
				hb::shared::text::draw_text(font_id, sX - size, base_y - 16, msg_a.c_str(), style);
				hb::shared::text::draw_text(font_id, sX - size, base_y, msg_b.c_str(), style);
				break;
			case 3:
				hb::shared::text::draw_text(font_id, sX - size, base_y - 32, msg_a.c_str(), style);
				hb::shared::text::draw_text(font_id, sX - size, base_y - 16, msg_b.c_str(), style);
				hb::shared::text::draw_text(font_id, sX - size, base_y, msg_c.c_str(), style);
				break;
			}
		}
	}
	else
	{
		// hb::shared::render::Renderer text path (chat, skill change) — word-wrap handled by TextLib
		const char* text = msg.m_text.c_str();
		hb::shared::render::Color rgb = params.m_color;

		int text_height = hb::shared::text::measure_wrapped_text_height(GameFont::Default, text, 160);
		int box_height = params.m_start_offset_y + text_height;
		int box_y = sY - box_height - rise;

		// Shadow (+1,0) and (0,+1) offsets in black, then foreground
		auto black = hb::shared::text::TextStyle::from_color(hb::shared::render::Color::Black());
		hb::shared::text::draw_text_wrapped(GameFont::Default, sX - 80 + 1, box_y, 160, box_height, text, black, hb::shared::text::Align::TopCenter);
		hb::shared::text::draw_text_wrapped(GameFont::Default, sX - 80, box_y + 1, 160, box_height, text, black, hb::shared::text::Align::TopCenter);
		hb::shared::text::draw_text_wrapped(GameFont::Default, sX - 80, box_y, 160, box_height, text, hb::shared::text::TextStyle::from_color(rgb), hb::shared::text::Align::TopCenter);
	}
}
