// quest_manager.h: Manages quest assignment, progress tracking, and completion.
// Extracted from CGame (Phase B3).

#pragma once

#include "common_types.h"
#include "quest.h"
#include "game.h"  // For hb::server::config constants

class QuestManager
{
public:
	QuestManager();
	~QuestManager();

	void set_game(CGame* game) { m_game = game; }

	// Lifecycle
	void init_arrays();
	void cleanup_arrays();

	// Quest handlers (moved from CGame)
	void npc_talk_handler(int client_h, int who);
	void quest_accepted_handler(int client_h);
	void cancel_quest_handler(int client_h);
	void send_quest_contents(int client_h);
	void check_quest_environment(int client_h);
	bool check_is_quest_completed(int client_h);
	void clear_quest_status(int client_h);

	// Config array (public for database loading)
	CQuest* m_quest_config_list[hb::server::config::MaxQuestType]{};

private:
	CGame* m_game = nullptr;

	// Private quest search helpers
	int talk_to_npc_result_cityhall(int client_h, int* quest_type, int* mode, int* reward_type, int* reward_amount, int* contribution, char* target_name, int* target_type, int* target_count, int* pX, int* pY, int* range);
	int talk_to_npc_result_guard(int client_h, int* quest_type, int* mode, int* reward_type, int* reward_amount, int* contribution, char* target_name, int* target_type, int* target_count, int* pX, int* pY, int* range);
	int search_for_quest(int client_h, int who, int* quest_type, int* mode, int* reward_type, int* reward_amount, int* contribution, char* target_name, int* target_type, int* target_count, int* pX, int* pY, int* range);
};
