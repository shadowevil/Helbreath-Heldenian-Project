#pragma once

#include "CommonTypes.h"
#include <iostream>
#include <vector>
using namespace std;

#include "Game.h"
#include "AccountSqliteStore.h"

enum class LogIn
{
	Ok,
	NoAcc,
	NoPass,
};

class LoginServer
{
public:
	LoginServer();
	~LoginServer();

	void request_login(int h, char* data);
	void get_char_list(string acc, char*& cp2, const std::vector<AccountDbCharacterSummary>& chars);
	LogIn AccountLogIn(string name, string pass, std::vector<AccountDbCharacterSummary>& chars);
	void response_character(int h, char* data);
	void delete_character(int h, char* data);
	void change_password(int h, char* data);
	void request_enter_game(int h, char* data);
	void create_new_account(int h, char* data);
	void send_login_msg(uint32_t msgid, uint16_t msgtype, char* data, size_t sz, int h);
	void send_balance_config(int h);
	void send_server_config(int h);
	void send_color_palette(int h);
	void local_save_player_data(int h);
	void activated();
};

extern LoginServer* g_login;
