#pragma once

// Owner class constants - classifies tile/object owners as Player, NPC,
// or PlayerIndirect. Used in CTile::m_owner_class, CDynamicObject::m_owner_type,
// and function parameters (owner_type, attacker_type, target_type).

namespace hb::shared::owner_class {
	constexpr char Player = 1;
	constexpr char Npc = 2;
	constexpr char PlayerIndirect = 3;
}
