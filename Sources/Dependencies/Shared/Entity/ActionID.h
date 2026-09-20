#pragma once
#include <cstdint>

namespace hb::shared::action
{

namespace Type
{
	enum : int
	{
		stop       = 0,
		Move       = 1,
		Run        = 2,
		Attack     = 3,
		Magic      = 4,
		GetItem    = 5,
		Damage     = 6,
		DamageMove = 7,
		AttackMove = 8,
		Dying      = 10,
		NullAction = 100,
	};
}

namespace Sentinel
{
	enum : int32_t
	{
		DamageImmune = -32768,
		MagicFailed  = -32767,
	};
}

namespace Confirm
{
	enum : int
	{
		MoveConfirm         = 1001,
		MoveReject          = 1010,
		MotionConfirm       = 1020,
		MotionAttackConfirm = 1030,
		MotionReject        = 1040,
	};
}

} // namespace hb::shared::action
