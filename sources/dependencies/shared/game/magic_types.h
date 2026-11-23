#pragma once

// magic_types.h - Shared Magic Type Constants
//
// Shared between client and server. Each value identifies a spell category
// that determines how the magic system processes the spell (targeting, damage, effects).

namespace hb::shared::magic {
	constexpr int DamageSpot                = 1;
	constexpr int HpUpSpot                  = 2;
	constexpr int DamageArea                = 3;
	constexpr int SpDownSpot                = 4;
	constexpr int SpDownArea                = 5;
	constexpr int SpUpSpot                  = 6;
	constexpr int SpUpArea                  = 7;
	constexpr int Teleport                  = 8;
	constexpr int Summon                    = 9;
	constexpr int Create                    = 10;
	constexpr int Protect                   = 11;
	constexpr int HoldObject                = 12;
	constexpr int Invisibility              = 13;
	constexpr int CreateDynamic             = 14;
	constexpr int Possession                = 15;
	constexpr int Confuse                   = 16;
	constexpr int Poison                    = 17;
	constexpr int Berserk                   = 18;
	constexpr int DamageLinear              = 19;
	constexpr int Polymorph                 = 20;
	constexpr int DamageAreaNoSpot          = 21;
	constexpr int Tremor                    = 22;
	constexpr int Ice                       = 23;
	// 24 unused
	constexpr int DamageAreaNoSpotSpDown    = 25;
	constexpr int IceLinear                 = 26;
	// 27 unused
	constexpr int DamageAreaArmorBreak      = 28;
	constexpr int Cancellation              = 29;
	constexpr int DamageLinearSpDown        = 30;
	constexpr int Inhibition                = 31;
	constexpr int Resurrection              = 32;
	constexpr int Scan                      = 33;

	constexpr int Haste                     = 45;
} // namespace hb::shared::magic
