#pragma once
#include <cstdint>

class CGame;

class StatusEffectManager
{
public:
	StatusEffectManager() = default;
	~StatusEffectManager() = default;
	void set_game(CGame* game) { m_game = game; }

	// Status effect flags
	void set_hero_flag(short owner_h, char owner_type, bool status);
	void set_berserk_flag(short owner_h, char owner_type, bool status);
	void set_haste_flag(short owner_h, char owner_type, bool status);
	void set_poison_flag(short owner_h, char owner_type, bool status);
	void set_defense_shield_flag(short owner_h, char owner_type, bool status);
	void set_magic_protection_flag(short owner_h, char owner_type, bool status);
	void set_protection_from_arrow_flag(short owner_h, char owner_type, bool status);
	void set_illusion_movement_flag(short owner_h, char owner_type, bool status);
	void set_illusion_flag(short owner_h, char owner_type, bool status);
	void set_ice_flag(short owner_h, char owner_type, bool status);
	void set_invisibility_flag(short owner_h, char owner_type, bool status);
	void set_inhibition_casting_flag(short owner_h, char owner_type, bool status);
	void set_angel_flag(short owner_h, char owner_type, int status, int temp);

	// Farming exploit detection
	void check_farming_action(short attacker_h, short target_h, bool type);

private:
	CGame* m_game = nullptr;
};
