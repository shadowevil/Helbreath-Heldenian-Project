#pragma once

class CGame;

class inventory_manager
{
public:
	static inventory_manager& get();

	void set_game(CGame* game);

	// Item ordering
	void set_item_order(int where, int item_id);

	// Weight/count queries
	int calc_total_weight();
	int get_total_item_count();
	int get_bank_item_count();

	// Item operations
	void erase_item(int item_id);
	bool check_item_operation_enabled(int item_id);

	// Item lock (delegates to CItem::m_locked)
	void lock_item(int slot);
	bool try_lock_item(int slot);
	void unlock_item(int slot);
	bool is_locked(int slot) const;
	bool warn_if_locked(int slot);
	void unlock_all();

	// Equipment
	void unequip_slot(int equip_pos);
	void equip_item(int item_id);

private:
	inventory_manager() = default;
	CGame* m_game = nullptr;
};
