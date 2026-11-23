#pragma once

#include "sprite_types.h"
#include <cstdint>

class CGame;
class Screen_OnGame;

class CNpcRenderer
{
public:
	explicit CNpcRenderer(CGame& game) : m_game(game) {}
	void set_screen(Screen_OnGame* screen) { m_screen = screen; }

	hb::shared::sprite::BoundRect draw_stop(int indexX, int indexY, int sX, int sY, bool trans, uint32_t time);
	hb::shared::sprite::BoundRect draw_move(int indexX, int indexY, int sX, int sY, bool trans, uint32_t time);
	hb::shared::sprite::BoundRect draw_run(int indexX, int indexY, int sX, int sY, bool trans, uint32_t time);
	hb::shared::sprite::BoundRect draw_attack(int indexX, int indexY, int sX, int sY, bool trans, uint32_t time);
	hb::shared::sprite::BoundRect draw_attack_move(int indexX, int indexY, int sX, int sY, bool trans, uint32_t time);
	hb::shared::sprite::BoundRect draw_magic(int indexX, int indexY, int sX, int sY, bool trans, uint32_t time);
	hb::shared::sprite::BoundRect draw_get_item(int indexX, int indexY, int sX, int sY, bool trans, uint32_t time);
	hb::shared::sprite::BoundRect draw_damage(int indexX, int indexY, int sX, int sY, bool trans, uint32_t time);
	hb::shared::sprite::BoundRect draw_damage_move(int indexX, int indexY, int sX, int sY, bool trans, uint32_t time);
	hb::shared::sprite::BoundRect draw_dying(int indexX, int indexY, int sX, int sY, bool trans, uint32_t time);
	hb::shared::sprite::BoundRect draw_dead(int indexX, int indexY, int sX, int sY, bool trans, uint32_t time);

private:
	CGame& m_game;
	Screen_OnGame* m_screen = nullptr;
};
