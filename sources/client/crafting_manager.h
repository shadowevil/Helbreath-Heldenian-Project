// crafting_manager.h: Handles client-side crafting/portion network messages.
// Extracted from network_messages_crafting.cpp (Phase B2).

#pragma once

class CGame;

class crafting_manager
{
public:
	crafting_manager() = default;
	~crafting_manager() = default;

	void set_game(CGame* game) { m_game = game; }

	// Network message handlers
	void handle_crafting_success(char* data);
	void handle_crafting_fail(char* data);
	void handle_build_item_success(char* data);
	void handle_build_item_fail(char* data);
	void handle_portion_success(char* data);
	void handle_portion_fail(char* data);
	void handle_low_portion_skill(char* data);
	void handle_no_matching_portion(char* data);

private:
	CGame* m_game = nullptr;
};
