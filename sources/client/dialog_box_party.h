#pragma once
#include "dialog_box.h"
#include "net/net_constants.h"
#include <string>

class DialogBox_Party : public IDialogBox
{
public:
	DialogBox_Party(CGame* game);
	~DialogBox_Party() override = default;

	void on_draw() override;
	bool on_click() override;
	bool on_enable(int type, int64_t v1, int v2, const char* string) override;

	enum class mode : uint8_t
	{
		main_menu = 0,
		invited = 1,
		pointing = 2,
		join_requested = 3,
		member_list = 4,
		leaving = 5,
		withdrawn = 6,
		party_full = 7,
		failed = 8,
		already_in_party = 9,
		disbanded = 10,
		confirm_leave = 11,
	};

	mode m_mode{mode::main_menu};
	char m_leader_name[32]{};

	void reset_members();
	bool add_member_name(const char* name);
	void set_name_list(int count, const char* data, int name_len);
	bool remove_member_name(const char* name);
	void clear_name_list();
	bool is_party_member(const std::string& name) const;

private:
	struct PartyMember { char status = 0; std::string name; };
	PartyMember m_members[hb::shared::limits::MaxPartyMembers]{};

	struct PartyName { std::string name; };
	PartyName m_name_list[hb::shared::limits::MaxPartyMembers + 1]{};

	static constexpr ui_rect btn_left{30, 292, 75, 21};
	static constexpr ui_rect btn_right{154, 292, 75, 21};
	static constexpr ui_rect link_create{81, 81, 114, 19};
	static constexpr ui_rect link_leave{81, 101, 114, 19};
	static constexpr ui_rect link_members{81, 121, 114, 19};
};
