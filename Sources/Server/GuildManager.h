#pragma once

class CGame;

class GuildManager
{
public:
	GuildManager() = default;
	~GuildManager() = default;

	void set_game(CGame* game) { m_game = game; }

private:
	CGame* m_game = nullptr;
};
