// quest_manager.h: Handles client-side quest network messages.
// Extracted from network_messages_quest.cpp (Phase B3).

#pragma once

class CGame;

class quest_manager
{
public:
	quest_manager() = default;
	~quest_manager() = default;

	void set_game(CGame* game) { m_game = game; }

	// Network message handlers
	void handle_quest_counter(char* data);
	void handle_quest_contents(char* data);
	void handle_quest_reward(char* data);
	void handle_quest_completed(char* data);
	void handle_quest_aborted(char* data);

private:
	CGame* m_game = nullptr;
};
