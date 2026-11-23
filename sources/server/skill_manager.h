#pragma once

#include <cstddef>
#include <cstdint>

class CGame;

class SkillManager
{
public:
	SkillManager() = default;
	~SkillManager() = default;

	void set_game(CGame* game) { m_game = game; }

	// Skill config
	bool send_client_skill_configs(int client_h);
	void reload_skill_configs();

	// Skill use
	void use_skill_handler(int client_h, int v1, int v2, int v3);
	void clear_skill_using_status(int client_h);
	void taming_handler(int client_h, int skill_num, char map_index, int dX, int dY);

	// Skill mastery / SSN
	void calculate_ssn_skill_index(int client_h, short skill_index, int value);
	bool check_total_skill_mastery_points(int client_h, int skill);
	int calc_skill_ssn_point(int level);
	void train_skill_response(bool success, int client_h, int skill_num, int skill_level);

	// Skill management
	void set_down_skill_index_handler(int client_h, int skill_index);
	void skill_check(int target_h);
	void set_skill_all(int client_h, char* data, size_t msg_size);

private:
	CGame* m_game = nullptr;
};
