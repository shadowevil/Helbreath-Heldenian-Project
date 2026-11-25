#include "dialog_box_guild_operation.h"
#include "game.h"

using namespace hb::client::sprite_id;

DialogBox_GuildOperation::DialogBox_GuildOperation(CGame* game)
	: IDialogBox(DialogBoxId::GuildOperation, game)
{
	set_default_rect(497, 57, 295, 346);
}

void DialogBox_GuildOperation::on_draw()
{
	short sX = m_x;
	short sY = m_y;

	draw_tall_frame(tall_frame::stone, sX, sY, 19);   // "Guild Menu"
}

bool DialogBox_GuildOperation::on_click()
{
	return false;
}

void DialogBox_GuildOperation::put(const char* /*name*/, char /*op_mode*/)
{
}

void DialogBox_GuildOperation::shift()
{
}

void DialogBox_GuildOperation::reset()
{
}
