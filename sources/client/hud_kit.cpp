// hud_kit.cpp: The HUD action bar drawn from the staged HUD kit (presentation only)
//
//////////////////////////////////////////////////////////////////////

#include "hud_kit.h"
#include "renderer.h"
#include "texture.h"
#include "json.hpp"
#include "log.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <stdexcept>
#include <string>

using json = nlohmann::json;

namespace hb::client
{
	namespace
	{
		struct point
		{
			int x = 0;
			int y = 0;
		};

		point point_of(const json& value)
		{
			return {value.at(0).get<int>(), value.at(1).get<int>()};
		}

		// The fill's share of length for value / max, clamped to 0..length (hard cut)
		int clipped(int length, uint64_t value, uint64_t max)
		{
			if (max == 0)
				return 0;
			return static_cast<int>(std::min<uint64_t>(value, max) * static_cast<uint64_t>(length) / max);
		}

		constexpr int menu_count = 6;
	}

	struct hud_kit::kit_data
	{
		// An atlas rect (HD pixels) and the game-pixel size it is drawn at; every region and offset is in game
		// pixels and maps onto the atlas rect by kx, ky (about 2.4)
		struct part
		{
			float x = 0.0f, y = 0.0f;
			float kx = 1.0f, ky = 1.0f;   // atlas pixels per game pixel
			int w = 0, h = 0;             // game pixels
		};

		std::unique_ptr<hb::shared::render::ITexture> sheet;
		hb::shared::render::IRenderer* renderer = nullptr;   // not owned

		part bar, exp_fill, msg_frame, tube_trough, tube_fill_hp, tube_fill_mp, sp_trough, sp_fill, hunger_trough,
			hunger_fill, socket, ring_hover, orb_peace, orb_peace_hover, orb_attack, orb_safe, orb_super, mode_eye,
			mode_eye_lit, mode_cross, mode_cross_lit;
		std::array<part, menu_count> menu_normal{}, menu_hover{};

		point at_msg_frame, at_tube_hp, at_tube_mp, at_sp, at_hunger, at_socket, at_crusade, at_menu;
		int menu_step = 0;
		point tube_fill, sp_fill_at, hunger_fill_at;
		int tube_cap_left = 0, tube_cap_right = 0, hunger_cap = 0;

		// Part of a part (sx, sy, w, h in game pixels) drawn w x h game pixels at (x, y)
		void region(const part& p, int sx, int sy, int w, int h, int x, int y, float alpha = 1.0f,
			const hb::shared::render::draw_tint& tint = {}) const
		{
			if (w <= 0 || h <= 0)
				return;
			renderer->draw_texture_region(sheet.get(), p.x + static_cast<float>(sx) * p.kx, p.y + static_cast<float>(sy) * p.ky,
				static_cast<float>(w) * p.kx, static_cast<float>(h) * p.ky, static_cast<float>(x), static_cast<float>(y),
				static_cast<float>(w), static_cast<float>(h), alpha, tint);
		}

		void draw(const part& p, int x, int y) const
		{
			region(p, 0, 0, p.w, p.h, x, y);
		}

		// A part centred on the socket
		void centred(const part& p, int top, float alpha = 1.0f, bool additive = false) const
		{
			const int x = at_socket.x + (socket.w - p.w) / 2;
			const int y = top + at_socket.y + (socket.h - p.h) / 2;
			namespace render = hb::shared::render;
			region(p, 0, 0, p.w, p.h, x, y, alpha,
				render::draw_tint{render::Color::White(), additive ? render::BlendMode::Additive : render::BlendMode::Alpha});
		}
	};

	hud_kit::hud_kit() = default;
	hud_kit::~hud_kit() = default;

	bool hud_kit::load(hb::shared::render::IRenderer& renderer, const std::string& sprite_dir)
	{
		m_data.reset();
		const std::string dir = sprite_dir + "/hudkit";
		const std::string json_path = dir + "/hud_kit.json";
		std::ifstream file(json_path);
		if (!file.is_open())
		{
			hb::logger::warn("hud_kit: {} not found", json_path);
			return false;
		}

		auto data = std::make_unique<kit_data>();
		try
		{
			const json doc = json::parse(file);
			const json& frames = doc.at("frames");
			std::string sheet_name;
			const auto part_of = [&](const std::string& name)
			{
				const json& f = frames.at(name);
				sheet_name = f.at("image").get<std::string>();
				const point size = point_of(f.at("size"));
				if (size.x <= 0 || size.y <= 0)
					throw std::runtime_error("frame " + name + " has an empty size");
				return kit_data::part{f.at("x").get<float>(), f.at("y").get<float>(),
					f.at("w").get<float>() / static_cast<float>(size.x), f.at("h").get<float>() / static_cast<float>(size.y),
					size.x, size.y};
			};
			data->bar = part_of("bar");
			data->exp_fill = part_of("exp_fill");
			data->msg_frame = part_of("msg_frame");
			data->tube_trough = part_of("tube_trough");
			data->tube_fill_hp = part_of("tube_fill_hp");
			data->tube_fill_mp = part_of("tube_fill_mp");
			data->sp_trough = part_of("sp_trough");
			data->sp_fill = part_of("sp_fill");
			data->hunger_trough = part_of("hunger_trough");
			data->hunger_fill = part_of("hunger_fill");
			data->socket = part_of("socket");
			data->ring_hover = part_of("ring_hover");
			data->orb_peace = part_of("orb_peace");
			data->orb_peace_hover = part_of("orb_peace_hover");
			data->orb_attack = part_of("orb_attack");
			data->orb_safe = part_of("orb_safe");
			data->orb_super = part_of("orb_super");
			data->mode_eye = part_of("mode_eye");
			data->mode_eye_lit = part_of("mode_eye_lit");
			data->mode_cross = part_of("mode_cross");
			data->mode_cross_lit = part_of("mode_cross_lit");

			const json& layout = doc.at("layout");
			const json& order = layout.at("menu_order");
			if (order.size() != menu_count)
				throw std::runtime_error("menu_order needs 6 buttons");
			for (int i = 0; i < menu_count; i++)
			{
				const std::string name = order.at(i).get<std::string>();
				data->menu_normal[i] = part_of("menu_" + name + "_normal");
				data->menu_hover[i] = part_of("menu_" + name + "_hover");
			}
			data->at_msg_frame = point_of(layout.at("msg_frame"));
			data->at_tube_hp = point_of(layout.at("tube_hp"));
			data->at_tube_mp = point_of(layout.at("tube_mp"));
			data->at_sp = point_of(layout.at("sp_trough"));
			data->at_hunger = point_of(layout.at("hunger_trough"));
			data->at_socket = point_of(layout.at("socket"));
			data->at_crusade = point_of(layout.at("crusade"));
			data->at_menu = point_of(layout.at("menu"));
			data->menu_step = layout.at("menu_step").get<int>();

			const json& gauges = doc.at("gauges");
			data->tube_fill = point_of(gauges.at("tube").at("fill_offset"));
			data->tube_cap_left = gauges.at("tube").at("cap_left").get<int>();
			data->tube_cap_right = gauges.at("tube").at("cap_right").get<int>();
			data->sp_fill_at = point_of(gauges.at("sp").at("fill_offset"));
			data->hunger_fill_at = point_of(gauges.at("hunger").at("fill_offset"));
			data->hunger_cap = gauges.at("hunger").at("cap").get<int>();

			const std::string sheet_path = dir + "/" + sheet_name;
			// The HD atlas is always drawn scaled (down at 1x, about 2.4 atlas px per game px), so smooth with
			// mipmaps; its parts are padded and edge-extruded for that
			data->sheet.reset(renderer.create_texture_from_file(sheet_path.c_str(), hb::shared::render::texture_filter::smooth));
			if (!data->sheet || data->sheet->get_width() == 0)
				throw std::runtime_error("cannot load " + sheet_path);
		}
		catch (const std::exception& e)
		{
			hb::logger::warn("hud_kit: invalid kit in {} ({})", dir, e.what());
			return false;
		}

		data->renderer = &renderer;
		m_data = std::move(data);
		return true;
	}

	void hud_kit::bar(int top) const
	{
		if (m_data)
			m_data->draw(m_data->bar, 0, top);
	}

	void hud_kit::message_frame(int top) const
	{
		if (m_data)
			m_data->draw(m_data->msg_frame, m_data->at_msg_frame.x, top + m_data->at_msg_frame.y);
	}

	void hud_kit::exp(int top, uint64_t value, uint64_t max) const
	{
		if (!m_data)
			return;
		const auto& f = m_data->exp_fill;
		m_data->region(f, 0, 0, clipped(f.w, value, max), f.h, 0, top);   // the bar's top rows are its trough
	}

	void hud_kit::tube(hud_tube::type which, int top, int value, int max) const
	{
		if (!m_data)
			return;
		const kit_data& k = *m_data;
		const point at = which == hud_tube::hp ? k.at_tube_hp : k.at_tube_mp;
		const auto& fill = which == hud_tube::hp ? k.tube_fill_hp : k.tube_fill_mp;
		const auto& trough = k.tube_trough;
		const int x = at.x;
		const int y = top + at.y;
		k.draw(trough, x, y);
		k.region(fill, 0, 0, clipped(fill.w, static_cast<uint64_t>(std::max(value, 0)), static_cast<uint64_t>(std::max(max, 0))),
			fill.h, x + k.tube_fill.x, y + k.tube_fill.y);
		k.region(trough, 0, 0, k.tube_cap_left, trough.h, x, y);
		k.region(trough, trough.w - k.tube_cap_right, 0, k.tube_cap_right, trough.h, x + trough.w - k.tube_cap_right, y);
	}

	void hud_kit::stamina(int top, int value, int max) const
	{
		if (!m_data)
			return;
		const kit_data& k = *m_data;
		const int x = k.at_sp.x;
		const int y = top + k.at_sp.y;
		k.draw(k.sp_trough, x, y);
		k.region(k.sp_fill, 0, 0,
			clipped(k.sp_fill.w, static_cast<uint64_t>(std::max(value, 0)), static_cast<uint64_t>(std::max(max, 0))),
			k.sp_fill.h, x + k.sp_fill_at.x, y + k.sp_fill_at.y);
	}

	void hud_kit::hunger(int top, int value, int max) const
	{
		if (!m_data)
			return;
		const kit_data& k = *m_data;
		const auto& trough = k.hunger_trough;
		const auto& fill = k.hunger_fill;
		const int x = k.at_hunger.x;
		const int y = top + k.at_hunger.y;
		// Anchored at the bottom: the fill's lowest h rows, at the bottom of the fill rect
		const int h = clipped(fill.h, static_cast<uint64_t>(std::max(value, 0)), static_cast<uint64_t>(std::max(max, 0)));
		k.draw(trough, x, y);
		k.region(fill, 0, fill.h - h, fill.w, h, x + k.hunger_fill_at.x, y + k.hunger_fill_at.y + fill.h - h);
		k.region(trough, 0, 0, trough.w, k.hunger_cap, x, y);
		k.region(trough, 0, trough.h - k.hunger_cap, trough.w, k.hunger_cap, x, y + trough.h - k.hunger_cap);
	}

	void hud_kit::combat(int top, hud_combat::type mode, bool hover) const
	{
		if (!m_data)
			return;
		const kit_data& k = *m_data;
		k.draw(k.socket, k.at_socket.x, top + k.at_socket.y);
		switch (mode)
		{
		case hud_combat::attack:      k.centred(k.orb_attack, top); break;
		case hud_combat::safe_attack: k.centred(k.orb_safe, top); break;
		default:                      k.centred(hover ? k.orb_peace_hover : k.orb_peace, top); break;
		}
		if (hover)
			k.centred(k.ring_hover, top);
	}

	void hud_kit::super_orb(int top) const
	{
		if (m_data)
			m_data->centred(m_data->orb_super, top, 0.7f, true);
	}

	void hud_kit::crusade(int top, bool aresden, bool hover) const
	{
		if (!m_data)
			return;
		const kit_data& k = *m_data;
		const auto& icon = aresden ? (hover ? k.mode_cross_lit : k.mode_cross) : (hover ? k.mode_eye_lit : k.mode_eye);
		k.draw(icon, k.at_crusade.x, top + k.at_crusade.y);
	}

	void hud_kit::menu(int top, int hovered) const
	{
		if (!m_data)
			return;
		const kit_data& k = *m_data;
		for (int i = 0; i < menu_count; i++)
			k.draw(i == hovered ? k.menu_hover[i] : k.menu_normal[i], k.at_menu.x + i * k.menu_step, top + k.at_menu.y);
	}
}
