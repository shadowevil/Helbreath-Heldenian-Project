// ItemEnums.h: Unified type-safe enums for the Item system
//
// Replaces divergent DEF_ macros from Client/Item.h and Server/Item.h
// Naming decisions:
//   - Slot 5: Boots (Shoes, Long Boots)
//   - Slot 13: FullBody (describes slot purpose, not behavior)
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>

namespace hb::shared::item {

//------------------------------------------------------------------------
// Equipment Position
//------------------------------------------------------------------------
enum class EquipPos : int8_t
{
    None      = 0,
    Head      = 1,
    Body      = 2,
    Arms      = 3,
    Leggings  = 4,   // Lower body (Trousers, Skirts, Chain Hose, Plate Leggings)
    Boots     = 5,   // Footwear (Shoes, Long Boots)
    Neck      = 6,
    LeftHand  = 7,
    RightHand = 8,
    TwoHand   = 9,
    RightFinger = 10,
    LeftFinger  = 11,
    Back      = 12,
    FullBody  = 13,  // Full-body armor (robes) - releases other armor slots when equipped

    Max       = 15
};

constexpr int DEF_MAXITEMEQUIPPOS = 15;

//------------------------------------------------------------------------
// Item Type — what the item IS
//------------------------------------------------------------------------
namespace item_type {
enum item_type : int8_t
{
	none       = 0,
	consumable = 1,   // Used and destroyed (potions, scrolls, arrows, dyes)
	equipment  = 2,   // Equippable gear (weapons, armor, accessories)
	material   = 3,   // Stackable resources (gold, ores, drops, bars)
	quest      = 4,   // Quest items (reserved)
	tool       = 5,   // Usable but not destroyed (fishing rod, map, crafting tools)
	misc       = 6    // Decorative / uncategorized
};
}

//------------------------------------------------------------------------
// Item Sub-Type — categorization within type
// Flat namespace with non-overlapping ranges per type
//------------------------------------------------------------------------
namespace item_sub_type {
enum item_sub_type : int8_t
{
	none         = 0,   // Generic (potions, food, scrolls, tickets, etc.)
	ammo         = 1,   // Arrows, auto-consumed in combat
	target       = 2,   // Requires pointing at a target (dyes, flags, seeds)
	weapon       = 3,   // Weapons
	armor        = 4,   // Armor, shields, helms, boots, gloves
	accessory    = 5,   // Rings, necklaces, capes
	component    = 6,   // Crafting inputs — ores, gems, stones
	monster_drop = 7,   // Monster body parts
	crafted      = 8,   // Smelted bars, wares (output of crafting)
	currency     = 9,   // Gold, gold sacks
	fishing      = 10,  // Fishing Rod
	crafting     = 11,  // Alchemy Bowl, Smith's Anvil, Crafting Vessel
	map          = 12,  // Map
	pendant      = 13,  // Pendants (generic)
	angelic      = 14   // Angelic Pendants
};
}

//------------------------------------------------------------------------
// Weapon Class — replaces weapon portion of appr_value
//------------------------------------------------------------------------
namespace weapon_class {
enum weapon_class : int8_t
{
	none        = 0,
	dagger      = 1,   // Skill: Short Sword (7)
	short_sword = 2,   // Skill: Short Sword (7)
	long_sword  = 3,   // Skill: Long Sword (8)
	fencing     = 4,   // Skill: Fencing (9)
	axe         = 5,   // Skill: Axe (10)
	hammer      = 6,   // Skill: Hammer (14)
	wand        = 7,   // Skill: Wand (21)
	bow         = 8    // Skill: Bow (6)
};
}

//------------------------------------------------------------------------
// Armor Class — distinguishes clothing from armor for dye targeting
//------------------------------------------------------------------------
namespace armor_class {
enum armor_class : int8_t
{
	none     = 0,
	clothing = 1,   // Capes, shirts, boots — regular dye target
	armor    = 2    // Plate, chain, shields — armor dye target
};
}

//------------------------------------------------------------------------
// Item Effect Type
//------------------------------------------------------------------------
enum class ItemEffectType : int16_t
{
    None               = 0,
    Attack             = 1,   // Attack value: value1 D value2 + value3
    Defense            = 2,   // Defense capability
    AttackArrow        = 3,   // Arrow attack - adds to base weapon damage
    HP                 = 4,   // HP restoration effect
    MP                 = 5,   // MP restoration effect
    SP                 = 6,   // SP restoration effect
    HPStock            = 7,   // HP recovery over time (no immediate visual)
    get                = 8,   // Acquire something (tools, containers)
    StudySkill         = 9,   // Skill learning item
    ShowLocation       = 10,  // Shows location on map
    Magic              = 11,  // Item with magic effect when used
    ChangeAttr         = 12,  // Changes player attributes (hair, skin, etc.)
    AttackManaSave     = 13,  // Attack with mana saving effect
    add_effect          = 14,  // Additional effect
    MagicDamageSave    = 15,  // Magic damage absorption
    OccupyFlag         = 16,  // Capture flag
    Dye                = 17,  // Dye item
    StudyMagic         = 18,  // Magic learning item
    AttackMaxHPDown    = 19,  // Attack that reduces max HP and HP recovery
    AttackDefense      = 20,  // Attack with defense reduction effect
    MaterialAttr       = 21,  // Material attribute for crafting
    FirmStamina        = 22,  // Stamina enhancement
    Lottery            = 23,  // Lottery ticket
    AttackSpecAbility  = 24,  // Attack with special ability
    DefenseSpecAbility = 25,  // Defense with special ability
    AlterItemDrop      = 26,  // Affects item drop of other items
    ConstructionKit    = 27,  // Construction kit
    Warm               = 28,  // Unfreeze potion effect
    // 29 is unused
    Farming            = 30,  // Farming item
    Slates             = 31,  // Ancient Tablets
    ArmorDye           = 32,  // Armor dye
    CritKomm           = 33,  // Crit Candy
    WeaponDye          = 34   // Weapon dye
};

//------------------------------------------------------------------------
// add_effect Sub-Types (used with ItemEffectType::add_effect)
// m_item_effect_value1 contains the sub-type, m_item_effect_value2 contains the value
//------------------------------------------------------------------------
enum class AddEffectType : int16_t
{
    MagicResist     = 1,   // Additional magic resistance
    ManaSave        = 2,   // Mana save percentage
    PoisonResist    = 3,   // Poison resistance
    CriticalHit     = 4,   // Critical hit chance
    Poisoning       = 5,   // Poison attack
    RepairItem      = 6,   // Repair item
    FireAbsorb      = 7,   // Fire damage absorption
    IceAbsorb       = 8,   // Ice damage absorption
    LightAbsorb     = 9,   // Light damage absorption
    ExpBonus        = 10,  // Experience bonus
    GoldBonus       = 11,  // Gold drop bonus
    // 12 unused
    Summon          = 13,  // Summon creature
    CancelBuff      = 14,  // Cancel buffs
    // 15-17 unused
    SpecialAbility  = 18,  // Special ability
    ParalysisImmune = 19   // Paralysis immunity
};

constexpr int16_t to_int(AddEffectType type) { return static_cast<int16_t>(type); }

//------------------------------------------------------------------------
// Touch Effect Type (item-specific effects on first touch/creation)
//------------------------------------------------------------------------
enum class TouchEffectType : int16_t
{
    None        = 0,
    UniqueOwner = 1,  // Item bound to owner
    ID          = 2,  // Item has unique ID
    Date        = 3   // Item has expiration date
};

//------------------------------------------------------------------------
// Helper Functions
//------------------------------------------------------------------------

// Equipment slot display name
constexpr const char* equip_pos_name(EquipPos pos)
{
    switch (pos)
    {
    case EquipPos::Head:        return "Head";
    case EquipPos::Body:        return "Body";
    case EquipPos::Arms:        return "Arms";
    case EquipPos::Leggings:    return "Leggings";
    case EquipPos::Boots:       return "Boots";
    case EquipPos::Neck:        return "Neck";
    case EquipPos::LeftHand:    return "Left Hand";
    case EquipPos::RightHand:   return "Right Hand";
    case EquipPos::TwoHand:     return "Two Hand";
    case EquipPos::RightFinger: return "Ring";
    case EquipPos::LeftFinger:  return "Ring";
    case EquipPos::Back:        return "Back";
    case EquipPos::FullBody:    return "Full Body";
    default:                    return "";
    }
}

// Check if equipment position is a weapon slot
constexpr bool is_weapon_slot(EquipPos pos)
{
    return pos == EquipPos::LeftHand ||
           pos == EquipPos::RightHand ||
           pos == EquipPos::TwoHand;
}

// Check if equipment position is an armor slot
constexpr bool is_armor_slot(EquipPos pos)
{
    return pos == EquipPos::Head ||
           pos == EquipPos::Body ||
           pos == EquipPos::Arms ||
           pos == EquipPos::Leggings ||
           pos == EquipPos::Boots ||
           pos == EquipPos::Back ||
           pos == EquipPos::FullBody;
}

// Check if equipment position is an accessory slot
constexpr bool is_accessory_slot(EquipPos pos)
{
    return pos == EquipPos::Neck ||
           pos == EquipPos::RightFinger ||
           pos == EquipPos::LeftFinger;
}

// Check if item effect type is an attack type
constexpr bool is_attack_effect_type(ItemEffectType type)
{
    return type == ItemEffectType::Attack ||
           type == ItemEffectType::AttackArrow ||
           type == ItemEffectType::AttackManaSave ||
           type == ItemEffectType::AttackMaxHPDown ||
           type == ItemEffectType::AttackDefense ||
           type == ItemEffectType::AttackSpecAbility;
}

// Check if item effect type is consumable (potion-like)
constexpr bool is_consumable_effect_type(ItemEffectType type)
{
    return type == ItemEffectType::HP ||
           type == ItemEffectType::MP ||
           type == ItemEffectType::SP ||
           type == ItemEffectType::HPStock;
}

//------------------------------------------------------------------------
// Enum conversion helpers (for serialization/deserialization)
//------------------------------------------------------------------------

constexpr int8_t to_int(EquipPos pos) { return static_cast<int8_t>(pos); }
constexpr int8_t to_int(item_type::item_type type) { return static_cast<int8_t>(type); }
constexpr int8_t to_int(item_sub_type::item_sub_type type) { return static_cast<int8_t>(type); }
constexpr int8_t to_int(weapon_class::weapon_class type) { return static_cast<int8_t>(type); }
constexpr int8_t to_int(armor_class::armor_class type) { return static_cast<int8_t>(type); }
constexpr int16_t to_int(ItemEffectType type) { return static_cast<int16_t>(type); }
constexpr int16_t to_int(TouchEffectType type) { return static_cast<int16_t>(type); }

constexpr EquipPos to_equip_pos(int8_t val) { return static_cast<EquipPos>(val); }
constexpr item_type::item_type to_item_type(int8_t val) { return static_cast<item_type::item_type>(val); }
constexpr item_sub_type::item_sub_type to_item_sub_type(int8_t val) { return static_cast<item_sub_type::item_sub_type>(val); }
constexpr weapon_class::weapon_class to_weapon_class(int8_t val) { return static_cast<weapon_class::weapon_class>(val); }
constexpr armor_class::armor_class to_armor_class(int8_t val) { return static_cast<armor_class::armor_class>(val); }
constexpr ItemEffectType to_item_effect_type(int16_t val) { return static_cast<ItemEffectType>(val); }
constexpr TouchEffectType to_touch_effect_type(int16_t val) { return static_cast<TouchEffectType>(val); }

//------------------------------------------------------------------------
// Common Item IDs
// These are well-known item IDs that are frequently referenced in code
//------------------------------------------------------------------------
namespace ItemId
{
    constexpr short Excaliber = 20;
    constexpr short Arrow = 77;
    constexpr short PoisonArrow = 78;
    constexpr short GuildAdmissionTicket = 88;
    constexpr short GuildSecessionTicket = 89;
    constexpr short Gold = 90;
    constexpr short MagicWandMShield = 259;
    constexpr short MagicWandMS30LLF = 291;
    constexpr short MagicNecklaceRM10 = 300;
    constexpr short MagicNecklaceDMp1 = 305;
    constexpr short MagicNecklaceMS10 = 308;
    constexpr short MagicNecklaceDFp10 = 311;
    constexpr short EmeraldRing = 335;
    constexpr short SapphireRing = 336;
    constexpr short RubyRing = 337;
    constexpr short AresdenHeroCape = 400;
    constexpr short ElvineHeroCape = 401;
    constexpr short AresdenHeroHelmM = 403;
    constexpr short ElvineHeroLeggingsW = 426;
    constexpr short AresdenHeroCapePlus1 = 427;
    constexpr short ElvineHeroCapePlus1 = 428;
    constexpr short BloodSword = 490;
    constexpr short BloodAxe = 491;
    constexpr short BloodRapier = 492;
    constexpr short XelimaBlade = 610;
    constexpr short XelimaAxe = 611;
    constexpr short XelimaRapier = 612;
    constexpr short DemonSlayer = 616;
    constexpr short DarkElfBow = 618;
    constexpr short MerienShield = 620;
    constexpr short MerienPlateMailM = 621;
    constexpr short MerienPlateMailW = 622;
    constexpr short RingoftheXelima = 630;
    constexpr short RingoftheAbaddon = 631;
    constexpr short RingofOgrepower = 632;
    constexpr short RingofDemonpower = 633;
    constexpr short RingofWizard = 634;
    constexpr short RingofMage = 635;
    constexpr short RingofGrandMage = 636;
    constexpr short NecklaceOfBeholder = 646;
    constexpr short NecklaceOfStoneGol = 647;
    constexpr short NecklaceOfLiche = 648;
    constexpr short StoneOfXelima = 656;
    constexpr short StoneOfMerien = 657;
    constexpr short SwordofMedusa = 724;
    constexpr short SwordofIceElemental = 725;
    constexpr short RingofArcmage = 734;
    constexpr short RingofDragonpower = 735;
    constexpr short ZemstoneofSacrifice = 753;
    constexpr short StormBringer = 845;
    constexpr short KlonessBlade = 849;
    constexpr short KlonessAxe = 850;
    constexpr short KlonessEsterk = 851;
    constexpr short NecklaceOfMerien = 858;
    constexpr short NecklaceOfKloness = 859;
    constexpr short NecklaceOfXelima = 860;
    constexpr short BerserkWandMS20 = 861;
    constexpr short BerserkWandMS10 = 862;
    constexpr short KlonessWandMS20 = 863;
    constexpr short KlonessWandMS10 = 864;
    constexpr short ResurWandMS20 = 865;
    constexpr short ResurWandMS10 = 866;
    constexpr short AcientTablet = 867;
    constexpr short AcientTabletLU = 868;
    constexpr short AcientTabletLD = 869;
    constexpr short AcientTabletRU = 870;
    constexpr short AcientTabletRD = 871;
    constexpr short DarkExecutor = 879;
    constexpr short TheDevastator = 880;
    constexpr short LightingBlade = 881;
    constexpr short MagicNecklaceDFp15 = 1086;
    constexpr short MagicNecklaceRM30 = 1101;
    constexpr short AngelicPendantSTR = 1108;
    constexpr short AngelicPendantDEX = 1109;
    constexpr short AngelicPendantINT = 1110;
    constexpr short AngelicPendantMAG = 1111;

    // DK Weapon IDs — used for glare visual effect
    constexpr short DarkKnightSword = 745;   // Dark Knight Templar (appr_val was 14, glare 3)
    constexpr short DarkMageStaff = 746;     // Dark Mage Templar (appr_val was 37, glare 2)
}

inline bool is_special_item(short i_dnum)
{
    switch (i_dnum) {
    case ItemId::Excaliber:
    case ItemId::MagicWandMShield:
    case ItemId::MagicWandMS30LLF:
    case ItemId::MagicNecklaceRM10:
    case ItemId::MagicNecklaceDMp1:
    case ItemId::MagicNecklaceMS10:
    case ItemId::MagicNecklaceDFp10:
    case ItemId::EmeraldRing:
    case ItemId::SapphireRing:
    case ItemId::RubyRing:
    case ItemId::AresdenHeroCape:
    case ItemId::ElvineHeroCape:
    case ItemId::AresdenHeroCapePlus1:
    case ItemId::ElvineHeroCapePlus1:
    case ItemId::BloodSword:
    case ItemId::BloodAxe:
    case ItemId::BloodRapier:
    case ItemId::XelimaBlade:
    case ItemId::XelimaAxe:
    case ItemId::XelimaRapier:
    case ItemId::DemonSlayer:
    case ItemId::DarkElfBow:
    case ItemId::MerienShield:
    case ItemId::MerienPlateMailM:
    case ItemId::MerienPlateMailW:
    case ItemId::RingoftheXelima:
    case ItemId::RingoftheAbaddon:
    case ItemId::RingofOgrepower:
    case ItemId::RingofDemonpower:
    case ItemId::RingofWizard:
    case ItemId::RingofMage:
    case ItemId::RingofGrandMage:
    case ItemId::NecklaceOfBeholder:
    case ItemId::NecklaceOfStoneGol:
    case ItemId::NecklaceOfLiche:
    case ItemId::StoneOfXelima:
    case ItemId::StoneOfMerien:
    case ItemId::SwordofMedusa:
    case ItemId::SwordofIceElemental:
    case ItemId::RingofArcmage:
    case ItemId::RingofDragonpower:
    case ItemId::ZemstoneofSacrifice:
    case ItemId::StormBringer:
    case ItemId::KlonessBlade:
    case ItemId::KlonessAxe:
    case ItemId::KlonessEsterk:
    case ItemId::NecklaceOfMerien:
    case ItemId::NecklaceOfKloness:
    case ItemId::NecklaceOfXelima:
    case ItemId::BerserkWandMS20:
    case ItemId::BerserkWandMS10:
    case ItemId::KlonessWandMS20:
    case ItemId::KlonessWandMS10:
    case ItemId::ResurWandMS20:
    case ItemId::ResurWandMS10:
    case ItemId::AcientTablet:
    case ItemId::AcientTabletLU:
    case ItemId::AcientTabletLD:
    case ItemId::AcientTabletRU:
    case ItemId::AcientTabletRD:
    case ItemId::DarkExecutor:
    case ItemId::TheDevastator:
    case ItemId::LightingBlade:
    case ItemId::MagicNecklaceDFp15:
    case ItemId::MagicNecklaceRM30:
    case ItemId::AngelicPendantSTR:
    case ItemId::AngelicPendantDEX:
    case ItemId::AngelicPendantINT:
    case ItemId::AngelicPendantMAG:
        return true;
    default:
        // Also check ranges for items between known IDs
        if (i_dnum >= ItemId::AresdenHeroHelmM && i_dnum <= ItemId::ElvineHeroLeggingsW) return true;  // Hero items 403-426
        if (i_dnum >= ItemId::MagicNecklaceDFp15 && i_dnum <= ItemId::MagicNecklaceRM30) return true;  // Magic necklaces 1086-1101
        return false;
    }
}

} // namespace hb::shared::item
