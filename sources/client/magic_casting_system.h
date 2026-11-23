#pragma once

class CGame;

class magic_casting_system
{
public:
	static magic_casting_system& get();

	void set_game(CGame* game);

	int get_mana_cost(int magic_no);
	void begin_cast(int magic_no);

private:
	magic_casting_system() = default;
	CGame* m_game = nullptr;
};
