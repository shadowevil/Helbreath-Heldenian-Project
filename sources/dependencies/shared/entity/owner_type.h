#pragma once

// Entity owner type constants — replaces magic numbers for NPC/player type IDs.
// Values match the server-assigned owner_type and the _iAttackerHeight[] array index.

namespace hb::shared::owner {

	constexpr short empty = 0;

	// Player range: 1-3 male body variants, 4-6 female body variants.
	// Helbreath is classless — the value only affects the body sprite frame.
	constexpr short PlayerFirst = 1;
	constexpr short PlayerLast = 6;
	constexpr short MaleFirst = 1;
	constexpr short MaleLast = 3;
	constexpr short FemaleFirst = 4;
	constexpr short FemaleLast = 6;

	// NPC / Monster types (10+)
	constexpr short NpcFirst = 10;

	constexpr short Slime = 10;
	constexpr short Skeleton = 11;
	constexpr short StoneGolem = 12;
	constexpr short Cyclops = 13;
	constexpr short OrcMage = 14;
	constexpr short ShopKeeper = 15;
	constexpr short GiantAnt = 16;
	constexpr short Scorpion = 17;
	constexpr short Zombie = 18;
	constexpr short Gandalf = 19;       // Magician NPC
	constexpr short Howard = 20;        // Warehouse Keeper
	constexpr short Guard = 21;
	constexpr short Amphis = 22;        // Snake
	constexpr short ClayGolem = 23;
	constexpr short Tom = 24;           // Blacksmith Keeper
	constexpr short William = 25;       // CityHall Officer
	constexpr short Kennedy = 26;       // GuildHall Officer
	constexpr short Hellhound = 27;
	constexpr short Troll = 28;
	constexpr short Ogre = 29;
	constexpr short Liche = 30;
	constexpr short Demon = 31;
	constexpr short Unicorn = 32;
	constexpr short WereWolf = 33;
	constexpr short Dummy = 34;
	constexpr short EnergySphere = 35;
	constexpr short ArrowGuardTower = 36;
	constexpr short CannonGuardTower = 37;
	constexpr short ManaCollector = 38;
	constexpr short Detector = 39;
	constexpr short EnergyShield = 40;
	constexpr short GrandMagicGenerator = 41;
	constexpr short ManaStone = 42;
	constexpr short LightWarBeetle = 43;
	constexpr short GodsHandKnight = 44;
	constexpr short GodsHandKnightCK = 45;
	constexpr short TempleKnight = 46;
	constexpr short BattleGolem = 47;
	constexpr short Stalker = 48;
	constexpr short HellClaw = 49;
	constexpr short TigerWorm = 50;
	constexpr short Catapult = 51;
	constexpr short Gargoyle = 52;
	constexpr short Beholder = 53;
	constexpr short DarkElf = 54;
	constexpr short Bunny = 55;
	constexpr short Cat = 56;
	constexpr short GiantFrog = 57;
	constexpr short MountainGiant = 58;
	constexpr short Ettin = 59;
	constexpr short CannibalPlant = 60;
	constexpr short Rudolph = 61;
	constexpr short DireBoar = 62;
	constexpr short Frost = 63;
	constexpr short Crops = 64;
	constexpr short IceGolem = 65;
	constexpr short Wyvern = 66;
	constexpr short McGaffin = 67;
	constexpr short Perry = 68;
	constexpr short Devlin = 69;
	constexpr short Dragon = 70;
	constexpr short Centaur = 71;
	constexpr short ClawTurtle = 72;
	constexpr short FireWyvern = 73;
	constexpr short GiantCrayfish = 74;
	constexpr short GiLizard = 75;
	constexpr short GiTree = 76;
	constexpr short MasterOrc = 77;
	constexpr short Minaus = 78;
	constexpr short Nizie = 79;
	constexpr short Tentocle = 80;
	constexpr short Abaddon = 81;
	constexpr short Sorceress = 82;
	constexpr short ATK = 83;
	constexpr short MasterElf = 84;
	constexpr short DSK = 85;
	constexpr short HBT = 86;
	constexpr short CT = 87;
	constexpr short Barbarian = 88;
	constexpr short AGC = 89;
	constexpr short Gail = 90;
	constexpr short Gate = 91;

	constexpr short AirElemental = 110;

	// --- Helpers ---

	inline bool is_player(short t) { return t >= PlayerFirst && t <= PlayerLast; }
	inline bool is_npc(short t) { return t >= NpcFirst; }
	inline bool is_male(short t) { return t >= MaleFirst && t <= MaleLast; }
	inline bool is_female(short t) { return t >= FemaleFirst && t <= FemaleLast; }

	// Energy Sphere and Abaddon are always rendered invisible (alpha).
	inline bool is_always_invisible(short t) { return t == EnergySphere || t == Abaddon; }

	// Returns true for entity types that can receive items from players.
	// Players (give/exchange), ShopKeeper/Tom (sell/repair), Howard (bank deposit), Kennedy (guild tickets).
	inline bool can_receive_items(short t)
	{
		return is_player(t) || t == ShopKeeper || t == Tom || t == Howard || t == Kennedy;
	}

} // namespace hb::shared::owner
