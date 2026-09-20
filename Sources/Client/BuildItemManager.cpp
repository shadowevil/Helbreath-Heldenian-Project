#include "BuildItemManager.h"
#include "DialogBox_Manufacture.h"
#include "Game.h"
#include <fstream>
#include <string>

build_item_manager::build_item_manager() = default;
build_item_manager::~build_item_manager() = default;

build_item_manager& build_item_manager::get()
{
	static build_item_manager instance;
	return instance;
}

void build_item_manager::set_game(CGame* game)
{
	m_game = game;
}

bool build_item_manager::load_recipes()
{
	for (int i = 0; i < hb::shared::limits::MaxBuildItems; i++)
	{
		if (m_recipes[i] != 0)
			m_recipes[i].reset();
	}

	std::ifstream file("contents/bitemcfg.txt");
	if (!file) return false;

	std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

	return parse_recipe_file(content);
}

bool build_item_manager::update_available_recipes()
{
	int index, i, j, match, count;
	char temp_name[hb::shared::limits::ItemNameLen];
	int  item_count[hb::shared::limits::MaxItems];

	for (i = 0; i < hb::shared::limits::MaxBuildItems; i++)
		if (m_display_recipes[i] != 0)
		{
			m_display_recipes[i].reset();
		}
	index = 0;
	for (i = 0; i < hb::shared::limits::MaxBuildItems; i++)
		if (m_recipes[i] != 0)
		{	// Skill-Limit
			if (m_game->m_player->m_skill_mastery[13] >= m_recipes[i]->m_skill_limit)
			{
				match = 0;
				m_display_recipes[index] = std::make_unique<build_item>();
				m_display_recipes[index]->m_name = m_recipes[i]->m_name;

				m_display_recipes[index]->m_element_name_1 = m_recipes[i]->m_element_name_1;
				m_display_recipes[index]->m_element_name_2 = m_recipes[i]->m_element_name_2;
				m_display_recipes[index]->m_element_name_3 = m_recipes[i]->m_element_name_3;
				m_display_recipes[index]->m_element_name_4 = m_recipes[i]->m_element_name_4;
				m_display_recipes[index]->m_element_name_5 = m_recipes[i]->m_element_name_5;
				m_display_recipes[index]->m_element_name_6 = m_recipes[i]->m_element_name_6;

				m_display_recipes[index]->m_element_count[1] = m_recipes[i]->m_element_count[1];
				m_display_recipes[index]->m_element_count[2] = m_recipes[i]->m_element_count[2];
				m_display_recipes[index]->m_element_count[3] = m_recipes[i]->m_element_count[3];
				m_display_recipes[index]->m_element_count[4] = m_recipes[i]->m_element_count[4];
				m_display_recipes[index]->m_element_count[5] = m_recipes[i]->m_element_count[5];
				m_display_recipes[index]->m_element_count[6] = m_recipes[i]->m_element_count[6];

				m_display_recipes[index]->m_sprite_handle = m_recipes[i]->m_sprite_handle;
				m_display_recipes[index]->m_sprite_frame = m_recipes[i]->m_sprite_frame;
				m_display_recipes[index]->m_max_skill = m_recipes[i]->m_max_skill;
				m_display_recipes[index]->m_skill_limit = m_recipes[i]->m_skill_limit;

				// ItemCount
				for (j = 0; j < hb::shared::limits::MaxItems; j++)
					if (m_game->m_player->m_item_list[j] != 0)
						item_count[j] = static_cast<int>(m_game->m_player->m_item_list[j]->m_instance.count);
					else item_count[j] = 0;

				for (int elem = 1; elem <= 6; elem++)
				{
					std::snprintf(temp_name, sizeof(temp_name), "%s", m_recipes[i]->get_element_name(elem).c_str());
					count = m_recipes[i]->m_element_count[elem];
					if (count == 0)
					{
						match++;
						continue;
					}
					for (j = 0; j < hb::shared::limits::MaxItems; j++)
					{
						if (m_game->m_player->m_item_list[j] != 0)
						{
							CItem* cfg_j = m_game->get_item_config(m_game->m_player->m_item_list[j]->m_id_num);
							if (cfg_j && (memcmp(cfg_j->m_name, temp_name, hb::shared::limits::ItemNameLen - 1) == 0) &&
								(m_game->m_player->m_item_list[j]->m_instance.count >= static_cast<uint32_t>(count)) &&
								(item_count[j] > 0))
							{
								match++;
								m_display_recipes[index]->m_element_flag[elem] = true;
								item_count[j] -= count;
								break;
							}
						}
					}
				}

				if (match == 6) m_display_recipes[index]->m_build_enabled = true;
				index++;
			}
		}
	return true;
}

bool build_item_manager::parse_recipe_file(const std::string& buffer)
{
	constexpr std::string_view seps = "= ,\t\n";
	char read_mode_a = 0;
	char read_mode_b = 0;
	int index = 0;

	auto copyToCharArray = [](char* dest, size_t destSize, const std::string& src) {
		std::memset(dest, 0, destSize);
		size_t copyLen = std::min(src.length(), destSize - 1);
		std::memcpy(dest, src.c_str(), copyLen);
	};

	size_t start = 0;
	while (start < buffer.size())
	{
		size_t end = buffer.find_first_of(seps, start);
		if (end == std::string::npos) end = buffer.size();

		if (end > start)
		{
			std::string token = buffer.substr(start, end - start);

			if (read_mode_a != 0)
			{
				switch (read_mode_a) {
				case 1:
					switch (read_mode_b) {
					case 1:
						m_recipes[index]->m_name = token;
						read_mode_b = 2;
						break;
					case 2:
						m_recipes[index]->m_skill_limit = std::stoi(token);
						read_mode_b = 3;
						break;
					case 3:
						m_recipes[index]->m_element_name_1 = token;
						read_mode_b = 4;
						break;
					case 4:
						m_recipes[index]->m_element_count[1] = std::stoi(token);
						read_mode_b = 5;
						break;
					case 5:
						m_recipes[index]->m_element_name_2 = token;
						read_mode_b = 6;
						break;
					case 6:
						m_recipes[index]->m_element_count[2] = std::stoi(token);
						read_mode_b = 7;
						break;
					case 7:
						m_recipes[index]->m_element_name_3 = token;
						read_mode_b = 8;
						break;
					case 8:
						m_recipes[index]->m_element_count[3] = std::stoi(token);
						read_mode_b = 9;
						break;
					case 9:
						m_recipes[index]->m_element_name_4 = token;
						read_mode_b = 10;
						break;
					case 10:
						m_recipes[index]->m_element_count[4] = std::stoi(token);
						read_mode_b = 11;
						break;
					case 11:
						m_recipes[index]->m_element_name_5 = token;
						read_mode_b = 12;
						break;
					case 12:
						if (token == "xxx")
							m_recipes[index]->m_element_count[5] = 0;
						else
							m_recipes[index]->m_element_count[5] = std::stoi(token);
						read_mode_b = 13;
						break;
					case 13:
						m_recipes[index]->m_element_name_6 = token;
						read_mode_b = 14;
						break;
					case 14:
						if(token == "xxx")
							m_recipes[index]->m_element_count[6] = 0;
						else
							m_recipes[index]->m_element_count[6] = std::stoi(token);
						read_mode_b = 15;
						break;
					case 15:
						m_recipes[index]->m_sprite_handle = std::stoi(token);
						read_mode_b = 16;
						break;
					case 16:
						m_recipes[index]->m_sprite_frame = std::stoi(token);
						read_mode_b = 17;
						break;
					case 17:
						m_recipes[index]->m_max_skill = std::stoi(token);
						read_mode_a = 0;
						read_mode_b = 0;
						index++;
						break;
					}
					break;
				}
			}
			else
			{
				if (token.starts_with("BuildItem"))
				{
					if (index >= hb::shared::limits::MaxBuildItems) break;
					read_mode_a = 1;
					read_mode_b = 1;
					m_recipes[index] = std::make_unique<build_item>();
				}
			}
		}
		start = end + 1;
	}
	return (read_mode_a == 0) && (read_mode_b == 0);
}

bool build_item_manager::validate_current_recipe()
{
	int i, count2, match, index, item_index[7];
	int count;
	int item_count[7];
	char temp_name[hb::shared::limits::ItemNameLen];
	bool item_flag[7];

	auto* mfg = m_game->get_dialog_box_manager().get_dialog_as<DialogBox_Manufacture>(DialogBoxId::Manufacture);
	if (!mfg) return false;
	index = mfg->m_progress;
	if (index < 0 || index >= hb::shared::limits::MaxBuildItems) return false;

	if (m_display_recipes[index] == 0) return false;

	item_index[1] = mfg->m_slot_1;
	item_index[2] = mfg->m_slot_2;
	item_index[3] = mfg->m_slot_3;
	item_index[4] = mfg->m_slot_4;
	item_index[5] = mfg->m_slot_5;
	item_index[6] = mfg->m_slot_6;

	for (i = 1; i <= 6; i++)
		if (item_index[i] != -1)
			item_count[i] = static_cast<int>(m_game->m_player->m_item_list[item_index[i]]->m_instance.count);
		else item_count[i] = 0;
	match = 0;
	for (i = 1; i <= 6; i++) item_flag[i] = false;

	for (int elem = 1; elem <= 6; elem++)
	{
		std::snprintf(temp_name, sizeof(temp_name), "%s", m_display_recipes[index]->get_element_name(elem).c_str());
		count = m_display_recipes[index]->m_element_count[elem];
		if (count == 0)
		{
			match++;
			continue;
		}
		for (i = 1; i <= 6; i++)
		{
			if (item_index[i] == -1) continue;
			CItem* cfg_bi = m_game->get_item_config(m_game->m_player->m_item_list[item_index[i]]->m_id_num);
			if (cfg_bi && (memcmp(cfg_bi->m_name, temp_name, hb::shared::limits::ItemNameLen - 1) == 0) &&
				(m_game->m_player->m_item_list[item_index[i]]->m_instance.count >= static_cast<uint32_t>(count)) &&
				(item_count[i] > 0) && (item_flag[i] == false))
			{
				match++;
				item_count[i] -= count;
				item_flag[i] = true;
				break;
			}
		}
	}

	count = 0;
	for (i = 1; i <= 6; i++)
		if (m_display_recipes[index]->m_element_count[i] != 0) count++;
	count2 = 0;
	for (i = 1; i <= 6; i++)
		if (item_index[i] != -1) count2++;
	if ((match == 6) && (count == count2)) return true;
	return false;
}

