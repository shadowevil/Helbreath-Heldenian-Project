#pragma once

#include <cstdint>

class CGame;
class CDynamicObject;

class DynamicObjectManager
{
public:
	DynamicObjectManager() = default;
	~DynamicObjectManager() = default;

	void set_game(CGame* game) { m_game = game; }

	void init_arrays();
	void cleanup_arrays();

	int  add_dynamic_object_list(short owner, char owner_type, short type, char map_index, short sX, short sY, uint32_t last_time, int v1 = 0);
	void check_dynamic_object_list();
	void dynamic_object_effect_processor();

	CDynamicObject* m_dynamic_object_list[60000];

private:
	CGame* m_game = nullptr;
};
