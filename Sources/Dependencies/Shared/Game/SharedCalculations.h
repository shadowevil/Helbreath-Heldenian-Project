#pragma once

#include "BalanceConstants.h"
#include "FormulaEngine.h"
#include <algorithm>
#include <cstdint>
#include <utility>

namespace hb::shared::calc
{

// ============================================================================
// Typed stat args — each stat is a distinct type with its stat_map key
// ============================================================================

struct vit              { double value; static constexpr const char* key() { return "vit"; } };
struct str              { double value; static constexpr const char* key() { return "str"; } };
struct dex              { double value; static constexpr const char* key() { return "dex"; } };
struct intel            { double value; static constexpr const char* key() { return "int"; } };
struct mag              { double value; static constexpr const char* key() { return "mag"; } };
struct chr              { double value; static constexpr const char* key() { return "chr"; } };
struct level            { double value; static constexpr const char* key() { return "level"; } };
struct angelic_str      { double value; static constexpr const char* key() { return "angelic_str"; } };
struct angelic_mag      { double value; static constexpr const char* key() { return "angelic_mag"; } };
struct angelic_int      { double value; static constexpr const char* key() { return "angelic_int"; } };
struct angelic_dex      { double value; static constexpr const char* key() { return "angelic_dex"; } };

// Fold variadic args into stat_map
template <typename Arg>
void collect_arg(stat_map& m, const Arg& arg) { m[Arg::key()] = arg.value; }

template <typename... Args>
stat_map build_stat_map(Args&&... args)
{
	stat_map m;
	(collect_arg(m, std::forward<Args>(args)), ...);
	return m;
}

// ============================================================================
// Max Resource Calculations (DB formulas)
// ============================================================================

template <typename... Args>
int max_hp(const formula_engine& fe, Args&&... args)
{
	return fe.evaluate("max_hp", build_stat_map(std::forward<Args>(args)...));
}

template <typename... Args>
int max_mp(const formula_engine& fe, Args&&... args)
{
	return fe.evaluate("max_mp", build_stat_map(std::forward<Args>(args)...));
}

template <typename... Args>
int max_sp(const formula_engine& fe, Args&&... args)
{
	return fe.evaluate("max_sp", build_stat_map(std::forward<Args>(args)...));
}

// ============================================================================
// Carry Weight (DB formula)
// ============================================================================

template <typename... Args>
int max_load(const formula_engine& fe, Args&&... args)
{
	return fe.evaluate("max_load", build_stat_map(std::forward<Args>(args)...));
}

// ============================================================================
// Experience (DB formula)
// ============================================================================

template <typename... Args>
uint32_t level_exp(const formula_engine& fe, Args&&... args)
{
	return static_cast<uint32_t>(fe.evaluate("level_exp", build_stat_map(std::forward<Args>(args)...)));
}

// ============================================================================
// HP Regen (DB formulas)
// ============================================================================

template <typename... Args>
int hp_regen_max_roll(const formula_engine& fe, Args&&... args)
{
	return fe.evaluate("hp_regen_max_roll", build_stat_map(std::forward<Args>(args)...));
}

template <typename... Args>
int hp_regen_min_roll(const formula_engine& fe, Args&&... args)
{
	return fe.evaluate("hp_regen_min_roll", build_stat_map(std::forward<Args>(args)...));
}

template <typename... Args>
int hp_regen_roll_variance(const formula_engine& fe, Args&&... args)
{
	return fe.evaluate("hp_regen_roll_variance", build_stat_map(std::forward<Args>(args)...));
}

// ============================================================================
// MP Regen (DB formulas)
// ============================================================================

template <typename... Args>
int mp_regen_max_roll(const formula_engine& fe, Args&&... args)
{
	return fe.evaluate("mp_regen_max_roll", build_stat_map(std::forward<Args>(args)...));
}

template <typename... Args>
int mp_regen_min_roll(const formula_engine& fe, Args&&... args)
{
	return fe.evaluate("mp_regen_min_roll", build_stat_map(std::forward<Args>(args)...));
}

template <typename... Args>
int mp_regen_roll_variance(const formula_engine& fe, Args&&... args)
{
	return fe.evaluate("mp_regen_roll_variance", build_stat_map(std::forward<Args>(args)...));
}

// ============================================================================
// SP Regen (DB formulas)
// ============================================================================

template <typename... Args>
int sp_regen_max_roll(const formula_engine& fe, Args&&... args)
{
	return fe.evaluate("sp_regen_max_roll", build_stat_map(std::forward<Args>(args)...));
}

template <typename... Args>
int sp_regen_min_roll(const formula_engine& fe, Args&&... args)
{
	return fe.evaluate("sp_regen_min_roll", build_stat_map(std::forward<Args>(args)...));
}

template <typename... Args>
int sp_regen_roll_variance(const formula_engine& fe, Args&&... args)
{
	return fe.evaluate("sp_regen_roll_variance", build_stat_map(std::forward<Args>(args)...));
}

// ============================================================================
// Weapon Swing Formulas (computed directly from balance constants)
// ============================================================================

inline int attack_delay(int weapon_spd, int player_str, int player_angelic_str)
{
	return std::max(weapon_spd - (player_str + player_angelic_str) / balance::swing_str_divisor, 0);
}

inline int swing_time(int atk_delay_value)
{
	return balance::swing_frames * (balance::base_frame_time + atk_delay_value * balance::delay_per_frame);
}

} // namespace hb::shared::calc
