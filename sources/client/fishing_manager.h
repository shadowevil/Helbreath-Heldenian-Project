// fishing_manager.h: Handles client-side fishing network messages.
// Extracted from network_messages_fish.cpp (Phase B1).

#pragma once

class CGame;

class fishing_manager
{
public:
	fishing_manager() = default;
	~fishing_manager() = default;

	void set_game(CGame* game) { m_game = game; }

	// Network message handlers (moved from NetworkMessageHandlers namespace)
	void handle_fish_chance(char* data);
	void handle_event_fish_mode(char* data);
	void handle_fish_canceled(char* data);
	void handle_fish_success(char* data);
	void handle_fish_fail(char* data);

private:
	CGame* m_game = nullptr;
};
