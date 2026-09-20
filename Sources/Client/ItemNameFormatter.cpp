#include "ItemNameFormatter.h"
#include "Item/Item.h"
#include "lan_eng.h"
#include "GameConstants.h"
#include "OwnerType.h"

#include <format>
#include <string>

using hb::shared::item::EquipPos;

item_name_formatter& item_name_formatter::get()
{
	static item_name_formatter instance;
	return instance;
}

void item_name_formatter::set_item_configs(const std::array<std::unique_ptr<CItem>, 5000>& configs)
{
	m_item_configs = &configs;
}

void item_name_formatter::set_multipliers(const uint8_t* prefix, const uint8_t* secondary)
{
	m_prefix_multiplier = prefix;
	m_secondary_multiplier = secondary;
}

CItem* item_name_formatter::get_config(int item_id) const
{
	if (!m_item_configs || item_id <= 0 || item_id >= 5000) return nullptr;
	return (*m_item_configs)[item_id].get();
}

ItemNameInfo item_name_formatter::format(CItem* item)
{
	auto result = format(item->m_id_num, item->to_instance_data());

	// Mana save effect comes from item config, not instance data
	CItem* cfg = get_config(item->m_id_num);
	if (cfg)
	{
		auto effectType = cfg->get_item_effect_type();
		int mana_save_value = 0;
		if (effectType == hb::shared::item::ItemEffectType::AttackManaSave)
		{
			mana_save_value = cfg->m_item_effect_value4;
		}
		else if (effectType == hb::shared::item::ItemEffectType::add_effect &&
		         cfg->m_item_effect_value1 == hb::shared::item::to_int(hb::shared::item::AddEffectType::ManaSave))
		{
			mana_save_value = cfg->m_item_effect_value2;
		}

		if (mana_save_value > 0)
		{
			result.is_special = true;
			result.effects.push_back({"Mana save ", std::format("+{}%", mana_save_value)});
		}
	}

	return result;
}

ItemNameInfo item_name_formatter::format(short item_id)
{
	ItemNameInfo result;
	CItem* cfg = get_config(item_id);
	if (!cfg || cfg->m_name[0] == '\0')
	{
		result.name = "Unknown Item";
		return result;
	}
	result.name = cfg->m_name;
	if (hb::shared::item::is_special_item(item_id)) result.is_special = true;
	return result;
}

ItemNameInfo item_name_formatter::format(short item_id, const hb::shared::item::item_instance_data& data)
{
	ItemNameInfo result;
	std::string txt;
	uint32_t type1, type2, value1, value2, value3;

	CItem* cfg = get_config(item_id);
	if (!cfg || cfg->m_name[0] == '\0')
	{
		result.name = "Unknown Item";
		return result;
	}
	const char* name = cfg->m_name;

	if (hb::shared::item::is_special_item(item_id)) result.is_special = true;

	if (data.custom_made)
	{
		result.is_special = true;
		result.name = name;
		if (cfg->get_item_type() == hb::shared::item::item_type::material)
			result.effects.push_back({"Purity: ", std::format("{}%", data.special_effect_value2)});
		else
		{
			if (cfg->get_equip_pos() == EquipPos::LeftFinger)
				result.effects.push_back({"Completion: ", std::format("{}%", data.special_effect_value2)});
			else
				result.effects.push_back({"Completion: ", std::format("{}%", data.special_effect_value2 + 100)});
		}
	}
	else
	{
		if (data.count <= 1)
			result.name = name;
		else
			result.name = std::format(DRAW_DIALOGBOX_SELLOR_REPAIR_ITEM1, data.count, name);
	}

	type1 = data.prefix_type;
	value1 = data.prefix_value;
	type2 = data.secondary_type;
	value2 = data.secondary_value;

	if (type1 != 0 || type2 != 0)
	{
		result.is_special = true;
		if (type1 != 0)
		{
			switch (type1) {
			case 1: txt = GET_ITEM_NAME3; break;
			case 2: txt = GET_ITEM_NAME4; break;
			case 3: txt = GET_ITEM_NAME5; break;
			case 4: break;
			case 5: txt = GET_ITEM_NAME6; break;
			case 6: txt = GET_ITEM_NAME7; break;
			case 7: txt = GET_ITEM_NAME8; break;
			case 8: txt = GET_ITEM_NAME9; break;
			case 9: txt = GET_ITEM_NAME10; break;
			case hb::shared::owner::Slime: txt = GET_ITEM_NAME11; break;
			case hb::shared::owner::Skeleton: txt = GET_ITEM_NAME12; break;
			case hb::shared::owner::StoneGolem: txt = GET_ITEM_NAME13; break;
			}
			txt += result.name;
			result.name = txt;

			int pm = (m_prefix_multiplier && type1 < 16) ? m_prefix_multiplier[type1] : 1;

			switch (type1) {
			case 1: result.effects.push_back({"Critical Hit Damage", std::format("+{}", value1 * pm)}); break;
			case 2: result.effects.push_back({"Poison Damage", std::format("+{}", value1 * pm)}); break;
			case 3: break;
			case 4: break;
			case 5: result.effects.push_back({"Attack Speed -1", ""}); break;
			case 6: result.effects.push_back({std::format("-{}%", value1 * pm), "", effect_category::inline_weight}); break;
			case 7: result.effects.push_back({std::format("+{}", value1 * pm), "", effect_category::inline_damage}); break;
			case 8: result.effects.push_back({"Durability", std::format("+{}%", value1 * pm), effect_category::inline_durability}); break;
			case 9: result.effects.push_back({std::format("+{}", value1 * pm), "", effect_category::inline_damage}); break;
			case hb::shared::owner::Slime: result.effects.push_back({"Magic Casting Probability ", std::format("+{}%", value1 * pm)}); break;
			case hb::shared::owner::Skeleton: result.effects.push_back({std::format("Replace {}% damage to mana", value1 * pm), ""}); break;
			case hb::shared::owner::StoneGolem: result.effects.push_back({"Crit Increase Chance ", std::format("{}%", value1 * pm)}); break;
			}
		}

		if (type2 != 0)
		{
			int s = (m_secondary_multiplier && type2 < 16) ? m_secondary_multiplier[type2] : 1;
			switch (type2) {
			case 1:  result.effects.push_back({"Poison Resistance", std::format("+{}%", value2 * s)}); break;
			case 2:  result.effects.push_back({"Hitting Probability", std::format("+{}", value2 * s)}); break;
			case 3:  result.effects.push_back({"Defense Ratio", std::format("+{}", value2 * s)}); break;
			case 4:  result.effects.push_back({"HP recovery ", std::format("{}%", value2 * s)}); break;
			case 5:  result.effects.push_back({"SP recovery ", std::format("{}%", value2 * s)}); break;
			case 6:  result.effects.push_back({"MP recovery ", std::format("{}%", value2 * s)}); break;
			case 7:  result.effects.push_back({"Magic Resistance", std::format("+{}%", value2 * s)}); break;
			case 8:  result.effects.push_back({"Physical Absorption", std::format("+{}%", value2 * s)}); break;
			case 9:  result.effects.push_back({"Magic Absorption", std::format("+{}%", value2 * s)}); break;
			case hb::shared::owner::Slime: result.effects.push_back({"Consecutive Attack Damage", std::format("+{}", value2 * s)}); break;
			case hb::shared::owner::Skeleton: result.effects.push_back({"Experience", std::format("+{}%", value2 * s)}); break;
			case hb::shared::owner::StoneGolem: result.effects.push_back({"Gold", std::format("+{}%", value2 * s)}); break;
			}
		}
	}

	value3 = data.enchant_bonus;
	if (value3 > 0)
	{
		auto plusPos = result.name.rfind('+');
		if (plusPos != std::string::npos && plusPos + 1 < result.name.size())
		{
			try {
				int existingPlus = std::stoi(result.name.substr(plusPos + 1));
				value3 += existingPlus;
				result.name = std::format("{}+{}", result.name.substr(0, plusPos), value3);
			} catch (...) {
				result.name += std::format("+{}", value3);
			}
		}
		else
		{
			result.name += std::format("+{}", value3);
		}
	}

	return result;
}
