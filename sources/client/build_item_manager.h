#pragma once

#include <array>
#include <memory>
#include <string>
#include "net_constants.h"

class build_item;
class CGame;

class build_item_manager
{
public:
	static build_item_manager& get();
	void set_game(CGame* game);

	bool load_recipes();
	bool update_available_recipes();
	bool validate_current_recipe();

	auto& get_display_list() { return m_display_recipes; }

private:
	build_item_manager();
	~build_item_manager();

	bool parse_recipe_file(const std::string& buffer);

	CGame* m_game = nullptr;
	std::array<std::unique_ptr<build_item>, hb::shared::limits::MaxBuildItems> m_recipes;
	std::array<std::unique_ptr<build_item>, hb::shared::limits::MaxBuildItems> m_display_recipes;
};
