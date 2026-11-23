#pragma once

class CGame;

class guild_manager
{
public:
	guild_manager() = default;
	~guild_manager() = default;

	void set_game(CGame* game) { m_game = game; }

private:
	CGame* m_game = nullptr;
};
