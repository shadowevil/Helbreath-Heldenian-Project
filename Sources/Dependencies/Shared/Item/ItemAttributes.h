// ItemAttributes.h: Item attribute type definitions
//
// Defines the enum types for item prefix and secondary effect attributes.
// Values are stored as individual fields on CItem — no bitmask packing.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>

namespace hb::shared::item {

//------------------------------------------------------------------------
// Primary Attribute Types
// These determine the name prefix added to items
//------------------------------------------------------------------------
enum class AttributePrefixType : uint8_t
{
	None           = 0,
	Critical       = 1,   // Critical hit damage bonus
	Poisoning      = 2,   // Poison damage bonus
	Righteous      = 3,   // Holy damage
	Reserved4      = 4,   // Unused
	Agile          = 5,   // Attack speed bonus
	Light          = 6,   // Weight reduction
	Sharp          = 7,   // Extra damage
	Strong         = 8,   // Endurance bonus
	Ancient        = 9,   // Extra damage
	Special        = 10,  // Magic casting bonus
	ManaConverting = 11,  // Damage to mana conversion
	CritChance     = 12   // Critical hit chance bonus
};

//------------------------------------------------------------------------
// Secondary Effect Types
// These determine additional bonuses displayed on item
//------------------------------------------------------------------------
enum class SecondaryEffectType : uint8_t
{
	None              = 0,
	PoisonResistance  = 1,   // Poison Resistance +X%
	HittingProb       = 2,   // Hitting Probability +X
	DefenseRatio      = 3,   // Defense Ratio +X
	HPRecovery        = 4,   // HP recovery X%
	SPRecovery        = 5,   // SP recovery X%
	MPRecovery        = 6,   // MP recovery X%
	MagicResistance   = 7,   // Magic Resistance +X%
	PhysicalAbsorb    = 8,   // Physical Absorption +X%
	MagicAbsorb       = 9,   // Magic Absorption +X%
	ConsecutiveAttack = 10,  // Consecutive Attack Damage +X
	ExperienceBonus   = 11,  // Experience +X%
	GoldBonus         = 12   // Gold +X%
};

} // namespace hb::shared::item
