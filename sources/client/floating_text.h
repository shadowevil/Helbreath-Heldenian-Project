#pragma once

#include <string>
#include <string_view>
#include <cstdint>
#include "floating_text_types.h"

class floating_text {
public:
	enum class Category : uint8_t { Chat, Damage, Notify };

	// Chat constructor
	floating_text(chat_text_type eType, std::string_view msg, uint32_t time)
		: m_category(Category::Chat)
		, m_chat_type(eType)
		, m_damage_type{}
		, m_notify_type{}
		, m_text(msg)
		, m_x(0), m_y(0)
		, m_time(time)
		, m_object_id(-1)
	{
	}

	// Damage constructor
	floating_text(damage_text_type eType, std::string_view msg, uint32_t time)
		: m_category(Category::Damage)
		, m_chat_type{}
		, m_damage_type(eType)
		, m_notify_type{}
		, m_text(msg)
		, m_x(0), m_y(0)
		, m_time(time)
		, m_object_id(-1)
	{
	}

	// Notify constructor
	floating_text(notify_text_type eType, std::string_view msg, uint32_t time)
		: m_category(Category::Notify)
		, m_chat_type{}
		, m_damage_type{}
		, m_notify_type(eType)
		, m_text(msg)
		, m_x(0), m_y(0)
		, m_time(time)
		, m_object_id(-1)
	{
	}

	const AnimParams& get_params() const
	{
		switch (m_category) {
		case Category::Chat:   return FloatingTextParams::Chat[static_cast<int>(m_chat_type)];
		case Category::Damage: return FloatingTextParams::Damage[static_cast<int>(m_damage_type)];
		case Category::Notify: return FloatingTextParams::Notify[static_cast<int>(m_notify_type)];
		}
		return FloatingTextParams::Chat[0]; // fallback
	}

	bool uses_sprite_font() const { return get_params().m_use_sprite_font; }

	Category       m_category;
	chat_text_type   m_chat_type;
	damage_text_type m_damage_type;
	notify_text_type m_notify_type;

	std::string    m_text;
	short          m_x, m_y;
	uint32_t       m_time;
	int            m_object_id;
};
