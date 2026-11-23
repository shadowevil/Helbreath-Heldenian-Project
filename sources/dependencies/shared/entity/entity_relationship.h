#pragma once

#include <cstdint>

// EntityRelationship: Server-computed per-viewer relationship.
// The server determines how each entity relates to each viewing player
// and sends this value in the status struct. The client reads it directly.
//
// Negative values = hostile, zero = neutral, positive = friendly.
enum class EntityRelationship : int8_t
{
	PK       = -2,  // Target is a player killer — always hostile
	Enemy    = -1,  // Opposing faction or crusade enemy
	Neutral  =  0,  // No faction affiliation or indeterminate
	Friendly =  1,  // Same faction as viewer
};

constexpr bool IsHostile(EntityRelationship r) { return r == EntityRelationship::PK || r == EntityRelationship::Enemy; }
constexpr bool IsFriendly(EntityRelationship r) { return r == EntityRelationship::Friendly; }
