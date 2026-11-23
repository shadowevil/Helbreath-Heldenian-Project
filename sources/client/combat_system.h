#pragma once

#include <cstdint>

class CPlayer;
class CGame;

class combat_system
{
public:
	static combat_system& get();

	void set_player(CPlayer& player);
	void set_game(CGame& game);

	int get_attack_type() const;
	int get_weapon_skill_type() const;
	bool can_super_attack() const;

private:
	combat_system() = default;
	int8_t get_weapon_class() const;
	CPlayer* m_player = nullptr;
	CGame* m_game = nullptr;
};
