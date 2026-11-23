#pragma once

// Effect type IDs for CEffect::m_type
// Each value corresponds to a distinct visual/audio effect in the game
enum class EffectType : short
{
	INVALID = 0,

	// Basic Combat Effects (1-4)
	NORMAL_HIT = 1,              // Coup normal / normal hit burst
	ARROW_FLYING = 2,            // Projectile: Arrow in flight
	// 3 = [UNUSED]
	GOLD_DROP = 4,               // Item drop: Gold coins animation

	// Elemental Magic Explosions (5-7)
	FIREBALL_EXPLOSION = 5,      // Fire explosion (fireball impact)
	ENERGY_BOLT_EXPLOSION = 6,   // Energy bolt impact
	MAGIC_MISSILE_EXPLOSION = 7, // Magic missile impact

	// Particle Bursts (8-12)
	BURST_SMALL = 8,             // Small particle burst (detail-filtered)
	BURST_MEDIUM = 9,            // Medium burst with gravity
	LIGHTNING_ARROW_EXPLOSION = 10, // Lightning arrow impact
	BURST_SMALL_GRENADE = 11,    // Grenade burst particles (detail-filtered)
	BURST_LARGE = 12,            // Large particle burst with gravity

	// Status Effects & Environment (13-15)
	BUBBLES_DRUNK = 13,          // Drunkenness bubbles
	FOOTPRINT = 14,              // Footprint / tremor effect (detail-filtered)
	RED_CLOUD_PARTICLES = 15,    // Red cloud explosion particles (detail-filtered)

	// Advanced Projectiles (16-18)
	PROJECTILE_GENERIC = 16,     // Generic projectile with impact (creates type 18)
	ICE_STORM = 17,              // Ice storm seeking target
	IMPACT_BURST = 18,           // Generic impact/burst effect

	// Critical Strike Effects (20-27)
	CRITICAL_STRIKE_1 = 20,      // Critical strike variant 1
	CRITICAL_STRIKE_2 = 21,
	CRITICAL_STRIKE_3 = 22,
	CRITICAL_STRIKE_4 = 23,
	CRITICAL_STRIKE_5 = 24,
	CRITICAL_STRIKE_6 = 25,
	CRITICAL_STRIKE_7 = 26,
	CRITICAL_STRIKE_8 = 27,

	// Mass Fire Strike (30-33)
	MASS_FIRE_STRIKE_CALLER1 = 30, // Mass fire strike explosion (caller 1)
	MASS_FIRE_STRIKE_CALLER3 = 31, // Mass fire strike explosion (caller 3)
	FOOTPRINT_RAIN = 32,            // Footprint variant for rain weather
	IMPACT_EFFECT = 33,             // Generic impact visual
	BLOODY_SHOCK_STRIKE = 34,       // Bloody shock wave strike (spawned by type 81)

	// Mass Magic Missile (35-36)
	MASS_MAGIC_MISSILE_AURA1 = 35, // Mass MM aura effect 1
	MASS_MAGIC_MISSILE_AURA2 = 36, // Mass MM aura effect 2

	// Chill Wind & Ice Strike (40-51)
	CHILL_WIND_IMPACT = 40,      // Chill wind impact (creates type 51)
	ICE_STRIKE_VARIANT_1 = 41,   // Ice strike falling crystal 1
	ICE_STRIKE_VARIANT_2 = 42,
	ICE_STRIKE_VARIANT_3 = 43,
	ICE_STRIKE_VARIANT_4 = 44,
	ICE_STRIKE_VARIANT_5 = 45,
	ICE_STRIKE_VARIANT_6 = 46,
	BLIZZARD_VARIANT_1 = 47,     // Blizzard falling ice 1
	BLIZZARD_VARIANT_2 = 48,
	BLIZZARD_VARIANT_3 = 49,
	SMOKE_DUST = 50,             // Final smoke/dust burst
	SPARKLE_SMALL = 51,          // Small sparkle particle

	// Buff Auras (52-54)
	PROTECTION_RING = 52,        // Protection/defense buff aura
	HOLD_TWIST = 53,             // Hold/paralyze effect aura
	STAR_TWINKLE = 54,           // Weapon shine/twinkle
	UNUSED_55 = 55,              // [UNUSED] Falls through to STAR_TWINKLE behavior

	// Mass Chill Wind (56-57)
	MASS_CHILL_WIND = 56,        // Mass chill wind area effect
	BUFF_EFFECT_LIGHT = 57,      // Generic buff light effect

	// Meteor Strike (60-67)
	METEOR_FLYING = 60,          // Meteor strike projectile
	FIRE_AURA_GROUND = 61,       // Fire aura on ground (meteor impact)
	METEOR_IMPACT = 62,          // Meteor ground impact
	FIRE_EXPLOSION_CRUSADE = 63, // Crusade meteor explosion
	WHITE_HALO = 64,             // White halo aura
	MS_CRUSADE_CASTING = 65,     // Meteor strike crusade casting
	MS_CRUSADE_EXPLOSION = 66,   // Meteor strike crusade explosion
	MS_FIRE_SMOKE = 67,          // Meteor strike fire with smoke

	// Creature & Light Effects (68-74)
	WORM_BITE = 68,              // Worm bite creature attack
	LIGHT_EFFECT_1 = 69,         // Generic light effect 1
	LIGHT_EFFECT_2 = 70,         // Generic light effect 2 (twin of 69)
	BLIZZARD_PROJECTILE = 71,    // Blizzard flying projectile
	BLIZZARD_IMPACT = 72,        // Blizzard impact
	AURA_EFFECT_1 = 73,          // Aura effect 1
	AURA_EFFECT_2 = 74,          // Aura effect 2

	// Ice Golem (75-77)
	ICE_GOLEM_EFFECT_1 = 75,     // Ice golem directional effect 1
	ICE_GOLEM_EFFECT_2 = 76,
	ICE_GOLEM_EFFECT_3 = 77,

	// Advanced Crusade Effects (80-82)
	EARTH_SHOCK_WAVE_PARTICLE = 80, // Earthquake particle
	STORM_BLADE = 81,               // Crusade storm blade
	GATE_APOCALYPSE = 82,           // Apocalypse gate

	// Flying Magic Projectiles (100-196)
	MAGIC_MISSILE_FLYING = 100,     // Magic Missile (Circle 1)
	HEAL = 101,
	CREATE_FOOD = 102,

	ENERGY_BOLT_FLYING = 110,       // Energy Bolt (Circle 2)
	STAMINA_DRAIN = 111,
	RECALL = 112,
	DEFENSE_SHIELD = 113,
	CELEBRATING_LIGHT = 114,        // Celebrating light effect

	FIRE_BALL_FLYING = 120,         // Fire Ball (Circle 3)
	GREAT_HEAL = 121,
	UNUSED_122 = 122,               // [UNUSED - ABSENT in v220/351] Falls through to buff spell behavior
	STAMINA_RECOVERY = 123,
	PROTECT_FROM_NM = 124,
	HOLD_PERSON = 125,
	POSSESSION = 126,
	POISON = 127,
	GREAT_STAMINA_RECOVERY = 128,

	FIRE_STRIKE_FLYING = 130,       // Fire Strike (Circle 4)
	SUMMON_CREATURE = 131,
	INVISIBILITY = 132,
	PROTECT_FROM_MAGIC = 133,
	DETECT_INVISIBILITY = 134,
	PARALYZE = 135,
	CURE = 136,
	LIGHTNING_ARROW_FLYING = 137,
	TREMOR = 138,

	CONFUSE_LANGUAGE = 142,         // (Circle 5)
	LIGHTNING = 143,
	GREAT_DEFENSE_SHIELD = 144,
	CHILL_WIND = 145,

	TRIPLE_ENERGY_BOLT = 147,

	BERSERK = 150,                  // (Circle 6)
	LIGHTNING_BOLT = 151,
	POLYMORPH = 152,
	MASS_POISON = 153,

	MASS_LIGHTNING_ARROW = 156,
	ICE_STRIKE = 157,

	ENERGY_STRIKE = 160,            // (Circle 7)
	MASS_FIRE_STRIKE_FLYING = 161,
	CONFUSION = 162,
	MASS_CHILL_WIND_SPELL = 163,
	WORM_BITE_MASS = 164,
	ABSOLUTE_MAGIC_PROTECTION = 165,
	ARMOR_BREAK = 166,

	BLOODY_SHOCK_WAVE = 170,        // (Circle 8)
	MASS_CONFUSION = 171,
	MASS_ICE_STRIKE = 172,

	LIGHTNING_STRIKE = 174,

	CANCELLATION = 176,             // (Circle 9)
	ILLUSION_MOVEMENT = 177,
	HASTE = 178,

	ILLUSION = 180,
	METEOR_STRIKE_DESCENDING = 181,
	MASS_MAGIC_MISSILE_FLYING = 182,
	INHIBITION_CASTING = 183,
	MAGIC_DRAIN = 184,              // EP's Magic Drain effect

	MASS_ILLUSION = 190,            // (Circle 10)
	BLIZZARD = 191,
	ICE_RAIN_VARIANT_1 = 192,       // Ice rain variant 1
	ICE_RAIN_VARIANT_2 = 193,       // Ice rain variant 2
	RESURRECTION = 194,
	MASS_ILLUSION_MOVEMENT = 195,
	EARTH_SHOCK_WAVE = 196,

	// Apocalypse Effects (200-206)
	SHOTSTAR_FALL_1 = 200,
	SHOTSTAR_FALL_2 = 201,
	SHOTSTAR_FALL_3 = 202,
	EXPLOSION_FIRE_APOCALYPSE = 203,
	CRACK_OBLIQUE = 204,
	CRACK_HORIZONTAL = 205,
	STEAMS_SMOKE = 206,

	// Hero Set Effects (242-244)
	MAGE_HERO_SET = 242,
	WAR_HERO_SET = 243,
	MASS_MM_AURA_CASTER = 244,

	// Gate & Special (250-252)
	GATE_ROUND = 250,
	SALMON_BURST = 251,
	SALMON_BURST_IMPACT = 252
};
