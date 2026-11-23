#pragma once

#include "text_input.h"
#include "text_lib.h"
#include <cstdint>
#include <string_view>

namespace hb::client {

// Character filter presets.
// Pass to cc::text_input::set_character_filter().
constexpr std::string_view username_allowed_chars =
	R"(abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%^&*_-.)";
constexpr std::string_view password_allowed_chars =
	R"(abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%^&*_-.)";
constexpr std::string_view email_allowed_chars =
	R"(abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789@.)";
constexpr std::string_view character_name_allowed_chars =
	R"(abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-)";
constexpr std::string_view guild_name_allowed_chars =
	R"(abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 _-!@#$%^&*)";
constexpr std::string_view digits_only = "0123456789";

// Draws a text_input control: text (masked if hidden), cursor, selection highlight.
// Uses GameFont::Default and standard input colors.
void draw_text_field(const cc::text_input& field, uint32_t time_ms,
                     const hb::shared::text::TextStyle& valid_style,
                     const hb::shared::text::TextStyle& invalid_style);

} // namespace hb::client
