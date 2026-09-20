#pragma once

#include <cstdint>
#include <map>

#include "DirectionHelpers.h"
using hb::shared::direction::direction;

struct DropTable;
class CGame;
class CItem;
struct ShopData;

class ItemManager
{
public:
	ItemManager() = default;
	~ItemManager() = default;

	void set_game(CGame* game) { m_game = game; }

	// Item config / init
	bool send_client_item_configs(int client_h);
	const DropTable* get_drop_table(int id) const;
	void clear_item_config_list();
	bool init_item_attr(CItem* item, const char* item_name);
	bool init_item_attr(CItem* item, int item_id);
	void reload_item_configs();

	// Item attribute generation
	void adjust_rare_item_value(CItem* item);
	bool generate_item_attributes(CItem* item);
	int roll_attribute_value(int min_val, int max_val);

	// Inventory management
	bool add_item(int client_h, CItem* item, char mode);
	bool add_client_item_list(int client_h, CItem* item, int* del_req);
	int add_client_bulk_item_list(int client_h, const char* item_name, int amount);
	void release_item_handler(int client_h, short item_index, bool notice);
	int set_item_count(int client_h, int item_index, uint64_t count);
	int set_item_count_by_id(int client_h, short item_id, uint64_t count);
	uint64_t get_item_count_by_id(int client_h, short item_id);
	int get_item_space_left(int client_h);
	void set_item_pos(int client_h, char* data);
	int get_item_weight(CItem* item, int count);
	bool copy_item_contents(CItem* original, CItem* copy);
	bool check_item_receive_condition(int client_h, CItem* item);
	int send_item_notify_msg(int client_h, uint16_t msg_type, CItem* item, int v1);

	// Item use / effects
	void use_item_handler(int client_h, short item_index, short dX, short dY, short dest_item_id);
	void item_deplete_handler(int client_h, short item_index, bool is_use_item_result);
	bool deplete_dest_type_item_use_effect(int client_h, int dX, int dY, short item_index, short dest_item_id);
	int calculate_use_skill_item_effect(int owner_h, char owner_type, char owner_skill, int skill_num, char map_index, int dX, int dY);
	bool plant_seed_bag(int map_index, int dX, int dY, int item_effect_value1, int item_effect_value2, int client_h);

	// Equipment
	bool equip_item_handler(int client_h, short item_index, bool notify = true);
	void validate_equipped_items(int client_h);
	void calc_total_item_effect(int client_h, int equip_item_id, bool notify = true);
	void check_unique_item_equipment(int client_h);
	bool check_and_convert_plus_weapon_item(int client_h, int item_index);
	char check_hero_item_equipped(int client_h);
	int get_arrow_item_index(int client_h);
	void calculate_ssn_item_index(int client_h, short weapon_index, int value);

	// Drop / pickup
	void drop_item_handler(int client_h, short item_index, int amount, const char* item_name, bool by_player = true);
	int client_motion_get_item_handler(int client_h, short sX, short sY, direction dir);

	// Give / exchange
	void give_item_handler(int client_h, short item_index, int amount, short dX, short dY, uint16_t object_id, const char* item_name);
	void exchange_item_handler(int client_h, short item_index, int amount, short dX, short dY, uint16_t object_id, const char* item_name);
	void set_exchange_item(int client_h, int item_index, int amount);
	void confirm_exchange_item(int client_h);
	void cancel_exchange_item(int client_h);
	void clear_exchange_status(int to_h);

	// Bank
	bool set_item_to_bank_item(int client_h, CItem* item);
	bool set_item_to_bank_item(int client_h, short item_index);
	void request_retrieve_item_handler(int client_h, char* data);

	// Shop / purchase / sell
	void request_purchase_item_handler(int client_h, const char* item_name, int num, int item_id = 0);
	void request_sell_item_list_handler(int client_h, char* data);
	void req_sell_item_handler(int client_h, char item_id, char sell_to_whom, int num, const char* item_name);
	void req_sell_item_confirm_handler(int client_h, char item_id, int num, const char* string);

	// Repair
	void req_repair_item_handler(int client_h, char item_id, char repair_whom, const char* string);
	void req_repair_item_cofirm_handler(int client_h, char item_id, const char* string);
	void request_repair_all_items_handler(int client_h);
	void request_repair_all_items_delete_handler(int client_h, int index);
	void request_repair_all_items_confirm_handler(int client_h);

	// Crafting
	void build_item_handler(int client_h, char* data);

	// Upgrade
	bool check_is_item_upgrade_success(int client_h, int item_index, int som_h, bool bonus = false);
	void request_item_upgrade_handler(int client_h, int item_index);

	// Hero / special
	void get_hero_mantle_handler(int client_h, int item_id, const char* string);

	// Slate
	void req_create_slate_handler(int client_h, char* data);
	void set_slate_flag(int client_h, short type, bool flag);

	// Logging
	bool item_log(int action, int client_h, char* name, CItem* item);
	bool item_log(int action, int give_h, int recv_h, CItem* item, bool force_item_log = false);
	bool check_good_item(CItem* item);

private:
	CGame* m_game = nullptr;
};
