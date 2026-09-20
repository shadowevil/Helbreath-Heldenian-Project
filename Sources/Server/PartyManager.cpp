// PartyManager.cpp: implementation of the PartyManager class.
//
//////////////////////////////////////////////////////////////////////

#include "CommonTypes.h"
#include "PartyManager.h"
#include "Packet/SharedPackets.h"
#include "Log.h"
#include "StringCompat.h"

extern char G_cTxt[120];

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

PartyManager::PartyManager(class CGame* game)
{
	

	for(int i = 0; i < hb::server::party::MaxParty; i++) {
		m_member_num_list[i] = 0;
		m_member_name_list[i].m_party_id = 0;
		m_member_name_list[i].m_index = 0;
		m_member_name_list[i].server_change_time = 0;
		std::memset(m_member_name_list[i].m_name, 0, sizeof(m_member_name_list[i].m_name));
	}

	m_game = game;
	m_check_member_act_time = GameClock::GetTimeMS();
}

PartyManager::~PartyManager()
{

}

int PartyManager::create_new_party_id(char* master_name)
{
	int party_id;

	// ??? PartyMaster? ????? ?? 
	for(int i = 1; i < hb::server::party::MaxParty; i++)
		if ((m_member_name_list[i].m_party_id != 0) && (hb_stricmp(m_member_name_list[i].m_name, master_name) == 0)) return 0;

	party_id = 0;
	for(int i = 1; i < hb::server::party::MaxParty; i++)
		if (m_member_num_list[i] == 0) {
			// Party ID? i, ??? ??
			party_id = i;
			m_member_num_list[party_id]++;
			break;
		}

	if (party_id == 0) return 0;
	// ?? ??? ????.
	for(int i = 1; i < hb::server::party::MaxParty; i++)
		if (m_member_name_list[i].m_party_id == 0) {
			m_member_name_list[i].m_party_id = party_id;
			std::memset(m_member_name_list[i].m_name, 0, sizeof(m_member_name_list[i].m_name));
			strcpy(m_member_name_list[i].m_name, master_name);
			m_member_name_list[i].m_index = 1;

			//testcode
			hb::logger::log("New party(ID:{} Master:{})", party_id, master_name);

			return party_id;
		}

	return 0;
}

bool PartyManager::delete_party(int party_id)
{
	bool flag;
	char data[120];

	flag = false;
	m_member_num_list[party_id] = 0;

	for(int i = 1; i < hb::server::party::MaxParty; i++)
		if (m_member_name_list[i].m_party_id == party_id) {
			m_member_name_list[i].m_party_id = 0;
			m_member_name_list[i].m_index = 0;
			m_member_name_list[i].server_change_time = 0;
			std::memset(m_member_name_list[i].m_name, 0, sizeof(m_member_name_list[i].m_name));
			flag = true;
		}

	// Notify game server about party deletion
	std::memset(data, 0, sizeof(data));
	hb::net::PartyOpResultDelete delResult{};
	delResult.op_type = 2;
	delResult.party_id = static_cast<uint16_t>(party_id);
	std::memcpy(data, &delResult, sizeof(delResult));
	m_game->party_operation_result_handler(data);

	//testcode
	hb::logger::log("Delete party(ID:{})", party_id);

	return flag;
}

bool PartyManager::add_member(int party_id, char* member_name)
{
	

	if (m_member_num_list[party_id] >= hb::server::party::MaxPartyMember) return false;

	// ?? ??? ?? ???? ??
	for(int i = 1; i < hb::server::party::MaxParty; i++)
		if ((m_member_name_list[i].m_party_id != 0) && (hb_stricmp(m_member_name_list[i].m_name, member_name) == 0))
		{
			m_member_name_list[i].m_party_id = 0;
			m_member_name_list[i].m_index = 0;
			m_member_name_list[i].server_change_time = 0;
			std::memset(m_member_name_list[i].m_name, 0, sizeof(m_member_name_list[i].m_name));

			m_member_num_list[party_id]--;
			if (m_member_num_list[party_id] <= 1) delete_party(party_id); // ???? 1?? ??? ?? ??
		}
	//		return false;

	for(int i = 1; i < hb::server::party::MaxParty; i++)
		if (m_member_name_list[i].m_party_id == 0) {
			m_member_name_list[i].m_party_id = party_id;
			m_member_name_list[i].m_index = 1;
			m_member_name_list[i].server_change_time = 0;
			std::memset(m_member_name_list[i].m_name, 0, sizeof(m_member_name_list[i].m_name));
			strcpy(m_member_name_list[i].m_name, member_name);
			m_member_num_list[party_id]++;

			//testcode
			hb::logger::log("Add Member: PartyID:{} Name:{}", party_id, member_name);
			return true;
		}

	return false;
}

bool PartyManager::remove_member(int party_id, char* member_name)
{
	

	for(int i = 1; i < hb::server::party::MaxParty; i++)
		if ((m_member_name_list[i].m_party_id == party_id) && (hb_stricmp(m_member_name_list[i].m_name, member_name) == 0)) {

			m_member_name_list[i].m_party_id = 0;
			m_member_name_list[i].m_index = 0;
			m_member_name_list[i].server_change_time = 0;
			std::memset(m_member_name_list[i].m_name, 0, sizeof(m_member_name_list[i].m_name));

			m_member_num_list[party_id]--;
			if (m_member_num_list[party_id] <= 1) delete_party(party_id); // ???? 1?? ??? ?? ?? 

			//testcode
			hb::logger::log("Remove Member: PartyID:{} Name:{}", party_id, member_name);
			return true;
		}

	return false;
}

bool PartyManager::check_party_member(int gsch, int party_id, char* name)
{
	char data[120]{};

	for (int i = 1; i < hb::server::party::MaxParty; i++)
		if ((m_member_name_list[i].m_party_id == party_id) && (hb_stricmp(m_member_name_list[i].m_name, name) == 0)) {
			m_member_name_list[i].server_change_time = 0;
			return true;
		}

	// Member not found — notify game server to remove
	hb::net::PartyOpCreateRequest req{};
	req.op_type = 3;
	req.client_h = static_cast<uint16_t>(gsch);
	std::memcpy(req.name, name, sizeof(req.name));
	std::memcpy(data, &req, sizeof(req));
	m_game->party_operation_result_handler(data);

	return false;
}

bool PartyManager::get_party_info(int gsch, char* name, int party_id)
{
	char data[1024]{};

	auto& hdr = *reinterpret_cast<hb::net::PartyOpResultInfoHeader*>(data);
	hdr.op_type = 5;
	hdr.client_h = static_cast<uint16_t>(gsch);
	std::memcpy(hdr.name, name, sizeof(hdr.name));

	char* cp = data + sizeof(hb::net::PartyOpResultInfoHeader);
	int total = 0;
	for (int i = 1; i < hb::server::party::MaxParty; i++)
		if ((m_member_name_list[i].m_party_id == party_id) && (m_member_name_list[i].m_party_id != 0)) {
			std::memcpy(cp, m_member_name_list[i].m_name, hb::shared::limits::CharNameLen - 1);
			cp += 11;
			total++;
		}

	hdr.total = static_cast<uint16_t>(total);
	m_game->party_operation_result_handler(data);

	return true;
}

void PartyManager::game_server_down()
{
	

	for(int i = 0; i < hb::server::party::MaxParty; i++)
		if (m_member_name_list[i].m_index == 1) {
			//testcode
			hb::logger::log("Removing party member '{}' (server shutdown)", m_member_name_list[i].m_name);

			m_member_num_list[m_member_name_list[i].m_party_id]--;
			m_member_name_list[i].m_party_id = 0;
			m_member_name_list[i].m_index = 0;
			m_member_name_list[i].server_change_time = 0;
			std::memset(m_member_name_list[i].m_name, 0, sizeof(m_member_name_list[i].m_name));
		}
}

void PartyManager::set_server_change_status(char* name, int party_id)
{
	

	for(int i = 1; i < hb::server::party::MaxParty; i++)
		if ((m_member_name_list[i].m_party_id == party_id) && (hb_stricmp(m_member_name_list[i].m_name, name) == 0)) {
			m_member_name_list[i].server_change_time = GameClock::GetTimeMS();
			return;
		}
}

void PartyManager::check_member_activity()
{
	uint32_t time = GameClock::GetTimeMS();
	char data[120];

	if ((time - m_check_member_act_time) > 1000 * 2) {
		m_check_member_act_time += 1000 * 2;
		if (time - m_check_member_act_time > 1000 * 2) m_check_member_act_time = time;
	}
	else return;

	for (int i = 1; i < hb::server::party::MaxParty; i++)
		if ((m_member_name_list[i].server_change_time != 0) && ((time - m_member_name_list[i].server_change_time) > 1000 * 20)) {
			std::memset(data, 0, sizeof(data));
			hb::net::PartyOpResultWithStatus dismissOp{};
			dismissOp.op_type = 6;
			dismissOp.result = 1;
			dismissOp.client_h = 0;
			std::memcpy(dismissOp.name, m_member_name_list[i].m_name, sizeof(dismissOp.name));
			dismissOp.party_id = static_cast<uint16_t>(m_member_name_list[i].m_party_id);
			std::memcpy(data, &dismissOp, sizeof(dismissOp));
			m_game->party_operation_result_handler(data);

			remove_member(m_member_name_list[i].m_party_id, m_member_name_list[i].m_name);
		}
}
