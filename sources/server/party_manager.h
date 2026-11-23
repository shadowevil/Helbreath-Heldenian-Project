// party_manager.h: interface for the PartyManager class.
//
//////////////////////////////////////////////////////////////////////

#pragma once


#include "common_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include "game.h"

namespace hb::server::party { constexpr int MaxParty = 5000; constexpr int MaxPartyMember = 100; }


class PartyManager
{
public:
	void check_member_activity();
	void set_server_change_status(char* name, int party_id);
	void game_server_down();
	bool get_party_info(int gsch, char* name, int party_id);
	bool check_party_member(int gsch, int party_id, char* name);
	bool remove_member(int party_id, char* member_name);
	bool add_member(int party_id, char* member_name);
	bool delete_party(int party_id);
	int create_new_party_id(char* master_name);
	PartyManager(class CGame* game);
	virtual ~PartyManager();

	int m_member_num_list[hb::server::party::MaxParty];

	struct {
		int  m_party_id, m_index;
		char m_name[hb::shared::limits::CharNameLen];
		uint32_t server_change_time;
	} m_member_name_list[hb::server::party::MaxParty];

	class CGame* m_game;
	uint32_t m_check_member_act_time;
};
