// Item.h: Unified CItem class for Helbreath Client and Server
//
// This class combines the item definitions from both Client/Item.h and Server/Item.h
// with the addition of m_cDisplayName for localized display names.
//
// Note: Display names are now stored directly in the database (name column).
// The server sends display names directly, so no client-side lookup is needed.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "ItemEnums.h"
#include "ItemAttributes.h"
#include "ItemInstanceData.h"
#include "NetConstants.h"
#include "Game/BalanceConstants.h"
#include <cstring>
#include <cstdint>
#include <algorithm>

struct dice_range
{
	int min;
	int max;
};

constexpr dice_range parse_dice(int num_rolls, int num_sides, int bonus = 0)
{
	return { num_rolls + bonus, num_rolls * num_sides + bonus };
}

class CItem
{
public:
    inline CItem()
    {
        std::memset(m_name, 0, sizeof(m_name));

        m_id_num = 0;
        m_item_type = 0;
        m_item_sub_type = 0;
        m_equip_pos = 0;
        m_item_effect_type = 0;

        m_item_effect_value1 = 0;
        m_item_effect_value2 = 0;
        m_item_effect_value3 = 0;
        m_item_effect_value4 = 0;
        m_item_effect_value5 = 0;
        m_item_effect_value6 = 0;

        m_durability = 0;
        m_special_effect = 0;
        m_special_effect_value1 = 0;
        m_special_effect_value2 = 0;

        m_x = 0;
        m_y = 0;

        m_display_id = -1;

        m_weapon_class = 0;
        m_swing_speed = 0;

        m_sell_price = 0;
        m_weight = 0;
        m_level_requirement = 0;
        m_gender_requirement = 0;

        m_related_skill = 0;

        m_hide_armor = 0;
        m_is_skirt = 0;
        m_stackable = 0;
        m_is_dyeable = 0;
        m_armor_class = 0;
        m_set_id = 0;

        // m_instance is zero-initialized by its default member initializers
    }

    inline virtual ~CItem()
    {
    }

    //------------------------------------------------------------------------
    // Core Item Data
    //------------------------------------------------------------------------

    char  m_name[hb::shared::limits::ItemNameLen];    // Internal item name (from database)

    short m_id_num;                 // Item ID number (unique identifier / lookup key — NOT instance data)
    char  m_item_type;              // Item type (see item_type enum)
    char  m_item_sub_type;          // Item sub-type (see item_sub_type enum)
    char  m_equip_pos;              // Equipment position (see EquipPos enum)

    //------------------------------------------------------------------------
    // Item Effects
    //------------------------------------------------------------------------

    short m_item_effect_type;        // Primary effect type (see ItemEffectType enum)
    short m_item_effect_value1;      // Effect value 1
    short m_item_effect_value2;      // Effect value 2
    short m_item_effect_value3;      // Effect value 3
    short m_item_effect_value4;      // Effect value 4 (for armor: stat bonus type)
    short m_item_effect_value5;      // Effect value 5 (for armor: stat bonus amount)
    short m_item_effect_value6;      // Effect value 6

    //------------------------------------------------------------------------
    // Special Effects
    //------------------------------------------------------------------------

    short m_special_effect;         // Special effect type
    short m_special_effect_value1;   // Special effect value 1
    short m_special_effect_value2;   // Special effect value 2

    //------------------------------------------------------------------------
    // Visual Properties
    //------------------------------------------------------------------------

    short m_display_id = -1;       // Atlas display ID (maps to ItemSpriteMetadata, -1 = unmapped)

    //------------------------------------------------------------------------
    // Position (client-side for inventory/ground display)
    //------------------------------------------------------------------------

    short m_x;                     // X position
    short m_y;                     // Y position

    //------------------------------------------------------------------------
    // Weapon Properties
    //------------------------------------------------------------------------

    char  m_weapon_class;          // Weapon class (see weapon_class enum)
    char  m_swing_speed;           // Weapon attack speed

    //------------------------------------------------------------------------
    // Stats and Limits
    //------------------------------------------------------------------------

    uint32_t m_sell_price;         // Sell price in gold (0 = cannot sell)
    uint16_t m_weight;             // Weight (affects encumbrance)
    short m_level_requirement;     // Minimum level to use
    char  m_gender_requirement;    // Gender restriction (0=none, 1=male, 2=female)
    short m_related_skill;         // Related skill for proficiency

    //------------------------------------------------------------------------
    // Durability (config — maximum)
    //------------------------------------------------------------------------

    uint16_t m_durability;         // Maximum durability

    //------------------------------------------------------------------------
    // Behavioral Flags
    //------------------------------------------------------------------------

    char  m_hide_armor;            // Body armor hides sprite when equipped
    char  m_is_skirt;              // Pants render as skirt for female characters
    char  m_stackable;             // Item merges into a single slot with count
    char  m_is_dyeable;            // Item can be a dye target
    char  m_armor_class;           // Armor class (0=none, 1=clothing, 2=armor)
    int16_t m_set_id;              // Equipment set ID (0 = no set)

    //------------------------------------------------------------------------
    // Client-side lock (prevents interaction while item is in a pending operation)
    //------------------------------------------------------------------------

    bool m_locked = false;

    bool is_locked() const { return m_locked; }
    bool try_lock() { if (m_locked) return false; m_locked = true; return true; }
    void unlock() { m_locked = false; }

    //------------------------------------------------------------------------
    // Instance Data (per-instance mutable fields)
    //------------------------------------------------------------------------

    hb::shared::item::item_instance_data m_instance;

    //------------------------------------------------------------------------
    // Display Name Helpers
    //------------------------------------------------------------------------

    // Returns display name if set, otherwise falls back to m_name
    // Note: Since DB migration, m_name now contains display names directly
    const char* get_display_name() const
    {
        return m_name;
    }

    //------------------------------------------------------------------------
    // Type-Safe Enum Accessors
    //------------------------------------------------------------------------

    hb::shared::item::EquipPos get_equip_pos() const
    {
        return hb::shared::item::to_equip_pos(m_equip_pos);
    }

    void set_equip_pos(hb::shared::item::EquipPos pos)
    {
        m_equip_pos = hb::shared::item::to_int(pos);
    }

    hb::shared::item::item_type::item_type get_item_type() const
    {
        return hb::shared::item::to_item_type(m_item_type);
    }

    void set_item_type(hb::shared::item::item_type::item_type type)
    {
        m_item_type = hb::shared::item::to_int(type);
    }

    hb::shared::item::item_sub_type::item_sub_type get_item_sub_type() const
    {
        return hb::shared::item::to_item_sub_type(m_item_sub_type);
    }

    void set_item_sub_type(hb::shared::item::item_sub_type::item_sub_type type)
    {
        m_item_sub_type = hb::shared::item::to_int(type);
    }

    hb::shared::item::weapon_class::weapon_class get_weapon_class() const
    {
        return hb::shared::item::to_weapon_class(m_weapon_class);
    }

    void set_weapon_class(hb::shared::item::weapon_class::weapon_class wc)
    {
        m_weapon_class = hb::shared::item::to_int(wc);
    }

    hb::shared::item::ItemEffectType get_item_effect_type() const
    {
        return hb::shared::item::to_item_effect_type(m_item_effect_type);
    }

    void set_item_effect_type(hb::shared::item::ItemEffectType type)
    {
        m_item_effect_type = hb::shared::item::to_int(type);
    }

    bool sprite_is_female() const
    {
        return m_gender_requirement == 2;
    }

    hb::shared::item::TouchEffectType get_touch_effect_type() const
    {
        return hb::shared::item::to_touch_effect_type(m_instance.touch_effect_type);
    }

    void set_touch_effect_type(hb::shared::item::TouchEffectType type)
    {
        m_instance.touch_effect_type = hb::shared::item::to_int(type);
    }

    //------------------------------------------------------------------------
    // Attribute Helpers
    //------------------------------------------------------------------------

    hb::shared::item::AttributePrefixType get_prefix_type() const
    {
        return static_cast<hb::shared::item::AttributePrefixType>(m_instance.prefix_type);
    }

    hb::shared::item::SecondaryEffectType get_secondary_type() const
    {
        return static_cast<hb::shared::item::SecondaryEffectType>(m_instance.secondary_type);
    }

    bool is_custom_made() const
    {
        return m_instance.custom_made != 0;
    }

    void set_custom_made(bool custom)
    {
        m_instance.custom_made = custom ? 1 : 0;
    }

    bool has_special_attributes() const
    {
        return m_instance.prefix_type != 0 ||
               m_instance.secondary_type != 0 ||
               m_instance.enchant_bonus > 0 ||
               m_instance.custom_made != 0;
    }

    // Copy attribute fields to a packet or DB struct that has matching field names
    template <typename T>
    void copy_attributes_to(T& target) const
    {
        target.custom_made = m_instance.custom_made;
        target.prefix_type = m_instance.prefix_type;
        target.prefix_value = m_instance.prefix_value;
        target.secondary_type = m_instance.secondary_type;
        target.secondary_value = m_instance.secondary_value;
        target.enchant_bonus = m_instance.enchant_bonus;
    }

    // Load attribute fields from a packet or DB struct that has matching field names
    template <typename T>
    void load_attributes_from(const T& source)
    {
        m_instance.custom_made = source.custom_made;
        m_instance.prefix_type = source.prefix_type;
        m_instance.prefix_value = source.prefix_value;
        m_instance.secondary_type = source.secondary_type;
        m_instance.secondary_value = source.secondary_value;
        m_instance.enchant_bonus = source.enchant_bonus;
    }

    // Copy all attribute fields from another CItem
    void copy_attributes_from(const CItem* other)
    {
        m_instance.custom_made = other->m_instance.custom_made;
        m_instance.prefix_type = other->m_instance.prefix_type;
        m_instance.prefix_value = other->m_instance.prefix_value;
        m_instance.secondary_type = other->m_instance.secondary_type;
        m_instance.secondary_value = other->m_instance.secondary_value;
        m_instance.enchant_bonus = other->m_instance.enchant_bonus;
    }

    // Return a const reference to the instance data
    const hb::shared::item::item_instance_data& to_instance_data() const { return m_instance; }

    // Check if item is stackable (reads the config flag)
    bool is_stackable() const
    {
        return m_stackable != 0;
    }

    // Base damage range from dice values (throw D range + bonus)
    dice_range get_damage_range() const
    {
        return parse_dice(m_item_effect_value1, m_item_effect_value2, m_item_effect_value3);
    }

    // Light attribute percentage — prefix_value is already the actual percentage
    int get_light_percent() const
    {
        if (m_instance.prefix_type == static_cast<uint8_t>(hb::shared::item::AttributePrefixType::Light))
            return m_instance.prefix_value;
        return 0;
    }

    // Weight adjusted for light attribute
    int get_effective_weight() const
    {
        int light = get_light_percent();
        if (light > 0)
            return static_cast<int>(m_weight) * (100 - light) / 100;
        return m_weight;
    }

    // Total weight for a stack of items (effective_weight * count), 0 for zero-weight items
    static inline constexpr int calc_item_stack_weight(int effective_weight, int count)
    {
        if (effective_weight <= 0) return 0;
        return effective_weight * count;
    }

    // Clamp item count to int16_t range for wire protocol (v2 field)
    static inline constexpr short count_to_v2(uint64_t count)
    {
        return static_cast<short>(count > 32767 ? 32767 : count);
    }

    // Convert raw weight units to stones (float)
    static inline constexpr float weight_to_stones(int raw_weight)
    {
        return static_cast<float>(raw_weight) / hb::shared::balance::weight_units_per_stone;
    }

    // Check if item is a weapon
    bool is_weapon() const
    {
        return hb::shared::item::is_weapon_slot(get_equip_pos());
    }

    // Check if item is armor
    bool is_armor() const
    {
        return hb::shared::item::is_armor_slot(get_equip_pos());
    }

    // Check if item is an accessory
    bool is_accessory() const
    {
        return hb::shared::item::is_accessory_slot(get_equip_pos());
    }
};
