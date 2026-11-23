#pragma once
#include "dialog_box.h"

class DialogBox_NpcActionQuery : public IDialogBox
{
public:
	DialogBox_NpcActionQuery(CGame* game);
	~DialogBox_NpcActionQuery() override = default;

	void on_draw() override;
	bool on_click() override;

	bool on_enable(int type, int64_t v1, int v2, const char* string) override;
	bool on_disable() override;

	void enable_with_target(int mode, int64_t item_id, int owner_type,
		int action_type, int object_id,
		int target_x, int target_y,
		const char* npc_name);
private:
	void DrawMode0_NpcMenu(short sX, short sY);
	void DrawMode1_GiveToPlayer(short sX, short sY);
	void DrawMode2_SellToShop(short sX, short sY);
	void DrawMode3_DepositToWarehouse(short sX, short sY);
	void DrawMode4_TalkToNpcOrUnicorn(short sX, short sY);
	void DrawMode5_ShopWithSell(short sX, short sY);
	void DrawMode6_Gail(short sX, short sY);

	void draw_highlighted_text(short sX, short sY, const char* text, short mouse_x, short mouse_y, short hitX1, short hitX2, short hitY1, short hitY2);

public:
	enum class mode : uint8_t
	{
		npc_menu = 0,
		give_to_player = 1,
		sell_to_shop = 2,
		deposit_to_warehouse = 3,
		talk_to_npc = 4,
		shop_with_sell = 5,
		gail = 6,
	};
	mode m_mode{mode::npc_menu};
	int m_item_index{-1};
	int m_owner_type{};
	int m_action_type{};
	int m_object_id{};
	int m_target_x{};
	int m_target_y{};
	char m_npc_name[32]{};
};
