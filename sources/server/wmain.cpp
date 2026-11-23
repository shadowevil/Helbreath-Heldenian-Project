#include "common_types.h"
#include <cstdio>
#include <cstring>
#include <thread>
#include <chrono>
#include <ctime>
#include <csignal>
#ifdef _WIN32
#include <windows.h>
#endif

#include "game.h"
#include "login_server.h"
#include "server_console.h"
#include "server_command.h"
#include "game_chat_command.h"
#include "io_service_pool.h"
#include "concurrent_msg_queue.h"
#include "log.h"
#include "server_log_channels.h"
#include "version.h"

#include <string>

using namespace hb::server::config;
using hb::log_channel;

// --- Globals ---

char            G_cTxt[512];
char            G_cData50000[50000];

class hb::shared::net::ASIOSocket* G_pListenSock = nullptr;
class hb::shared::net::ASIOSocket* G_pLogSock = nullptr;
class CGame* G_pGame = nullptr;
class hb::shared::net::ASIOSocket* G_pLoginSock = nullptr;
class LoginServer* g_login = nullptr;

hb::shared::net::IOServicePool* G_pIOPool = nullptr;

hb::shared::net::ConcurrentQueue<asio::ip::tcp::socket> G_gameAcceptQueue;
hb::shared::net::ConcurrentQueue<asio::ip::tcp::socket> G_loginAcceptQueue;
hb::shared::net::ConcurrentQueue<hb::shared::net::SocketErrorEvent> G_errorQueue;

bool G_bRunning = true;

// --- Signal handlers for graceful shutdown ---

static void signal_handler(int sig)
{
	G_bRunning = false;
}

#ifdef _WIN32
static BOOL WINAPI console_ctrl_handler(DWORD ctrl_type)
{
	if (ctrl_type == CTRL_C_EVENT || ctrl_type == CTRL_CLOSE_EVENT || ctrl_type == CTRL_BREAK_EVENT)
	{
		G_bRunning = false;
		return TRUE;
	}
	return FALSE;
}
#endif

// --- Async I/O helpers (called from main loop) ---

void DrainAcceptQueues()
{
	if (G_pGame == nullptr) return;

	asio::ip::tcp::socket peer(G_pIOPool->get_context());
	while (G_gameAcceptQueue.pop(peer)) {
		G_pGame->accept_from_async(std::move(peer));
	}

	asio::ip::tcp::socket loginPeer(G_pIOPool->get_context());
	while (G_loginAcceptQueue.pop(loginPeer)) {
		G_pGame->accept_login_from_async(std::move(loginPeer));
	}
}

void DrainErrorQueue()
{
	if (G_pGame == nullptr) return;

	hb::shared::net::SocketErrorEvent evt;
	while (G_errorQueue.pop(evt)) {
		if (evt.socket_index > 0 && evt.socket_index < MaxClients) {
			if (G_pGame->m_client_list[evt.socket_index] != nullptr) {
				hb::logger::log("Client {}: disconnected, async error={} ({}) char={}", evt.socket_index, evt.error_code, G_pGame->m_client_list[evt.socket_index]->m_ip_address, G_pGame->m_client_list[evt.socket_index]->m_char_name);
				G_pGame->delete_client(evt.socket_index, true, true);
			}
		}
		else if (evt.socket_index < 0) {
			int loginH = -(evt.socket_index + 1);
			if (loginH >= 0 && loginH < MaxClientLoginSock) {
				G_pGame->delete_login_client(loginH);
			}
		}
	}
}

void PollLoginClients()
{
	if (G_pGame == nullptr) return;

	for (int i = 0; i < MaxClientLoginSock; i++) {
		if (G_pGame->_lclients[i] != nullptr && G_pGame->_lclients[i]->sock != nullptr) {
			G_pGame->on_login_client_socket_event(i);
		}
	}
}

// Called from EntityManager during NPC processing
void PollAllSockets()
{
	DrainAcceptQueues();
	DrainErrorQueue();
	PollLoginClients();
}

// --- Entry point ---

int main()
{
	// Register signal handlers for graceful shutdown
	std::signal(SIGINT, signal_handler);
	std::signal(SIGTERM, signal_handler);
#ifdef _WIN32
	SetConsoleCtrlHandler(console_ctrl_handler, TRUE);
#endif

	// Portable startup timestamp
	auto now = std::chrono::system_clock::now();
	std::time_t t = std::chrono::system_clock::to_time_t(now);
	std::tm tm_buf;
#ifdef _WIN32
	localtime_s(&tm_buf, &t);
#else
	localtime_r(&t, &tm_buf);
#endif

	{
		const char* banner[] = {
			"  _  _ ___ _    ___ ___ ___   _ _____ _  _",
			" | || | __| |  | _ ) _ \\ __| /_\\_   _| || |",
			" | __ | _|| |__| _ \\   / _| / _ \\| | | __ |",
			" |_||_|___|____|___/_|_\\___/_/ \\_\\_| |_||_|",
		};
		const char* subtitle = "The Heldenian Project";
		constexpr int console_width = 80;

		int max_width = 0;
		for (const auto& line : banner)
		{
			int len = static_cast<int>(strlen(line));
			if (len > max_width) max_width = len;
		}

		int left_pad = (console_width - max_width) / 2;
		if (left_pad < 0) left_pad = 0;

		printf("\n\033[1;36m");
		for (const auto& line : banner)
			printf("%*s%s\n", left_pad, "", line);

		int subtitle_len = static_cast<int>(strlen(subtitle));
		int subtitle_pad = left_pad + max_width - subtitle_len;
		printf("\033[1;37m%*s%s\033[0m\n", subtitle_pad, "", subtitle);

		namespace version = hb::shared::version;
		const std::string server_version(version::version_string(version::component::server));
		const std::string build_id(version::build_id());
		const std::string commit(version::commit_sha().substr(0, 8));
		const std::string sql_release(version::release_version(version::component::sql));
		printf("\033[0;90m  Version: %s\n", server_version.c_str());
		printf("  Build:   %s (%s, %s)\n", build_id.c_str(), version::source_clean() ? "clean" : "dirty", commit.c_str());
		printf("  SQL:     %s\n", sql_release.c_str());
		printf("  Started: %d/%d/%d %02d:%02d\033[0m\n\n",
			tm_buf.tm_mon + 1, tm_buf.tm_mday, tm_buf.tm_year + 1900,
			tm_buf.tm_hour, tm_buf.tm_min);
	}

	// initialize console first so log messages get ANSI color
	GetServerConsole().init();

	// initialize logger
	hb::logger::initialize("gamelogs/");
	hb::logger::log("Server {} (sql {}, {} tree, commit {})",
		hb::shared::version::version_string(hb::shared::version::component::server),
		hb::shared::version::release_version(hb::shared::version::component::sql),
		hb::shared::version::source_clean() ? "clean" : "dirty", hb::shared::version::commit_sha().substr(0, 8));

	// Create I/O service pool
	G_pIOPool = new hb::shared::net::IOServicePool(4);

	// Create and initialize game
	G_pGame = new CGame();
	if (!G_pGame->init()) {
		hb::logger::error("Server initialization failed");
		hb::logger::shutdown();
		delete G_pGame;
		delete G_pIOPool;
		return 1;
	}

	ServerCommandManager::get().initialize(G_pGame);
	GameChatCommandManager::get().initialize(G_pGame);

	// start listen sockets
	G_pListenSock = new hb::shared::net::ASIOSocket(G_pIOPool->get_context(), ServerSocketBlockLimit);
	G_pListenSock->listen(G_pGame->m_game_listen_ip, G_pGame->m_game_listen_port);

	G_pLoginSock = new hb::shared::net::ASIOSocket(G_pIOPool->get_context(), ServerSocketBlockLimit);
	G_pLoginSock->listen(G_pGame->m_login_listen_ip, G_pGame->m_login_listen_port);

	// start async accept
	G_pListenSock->start_async_accept([](asio::ip::tcp::socket peer) {
		G_gameAcceptQueue.push(std::move(peer));
	});
	G_pLoginSock->start_async_accept([](asio::ip::tcp::socket peer) {
		G_loginAcceptQueue.push(std::move(peer));
	});

	// start I/O thread pool
	G_pIOPool->start();

	// --- Main loop ---
	constexpr uint32_t tick_interval = 300 / GameTickMultiplier;
	uint32_t last_tick = GameClock::GetTimeMS();

	while (G_bRunning) {
		uint32_t now_ms = GameClock::GetTimeMS();

		if (now_ms - last_tick >= tick_interval) {
			G_pGame->on_timer(0);
			G_pGame->game_process();
			last_tick = now_ms;
		}

		DrainAcceptQueues();
		DrainErrorQueue();
		PollLoginClients();

		char cmd[256];
		if (GetServerConsole().poll_input(cmd, sizeof(cmd))) {
			ServerCommandManager::get().process_command(cmd);
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	// --- shutdown ---
	hb::logger::log("Saving all player data before shutdown...");
	int saved = G_pGame->save_all_players();
	hb::logger::log("Saved {} player(s)", saved);

	G_pIOPool->stop();

	delete G_pListenSock;
	G_pListenSock = nullptr;

	delete G_pLogSock;
	G_pLogSock = nullptr;

	delete G_pLoginSock;
	G_pLoginSock = nullptr;

	G_pGame->quit();
	delete G_pGame;
	G_pGame = nullptr;

	delete g_login;
	g_login = nullptr;

	delete G_pIOPool;
	G_pIOPool = nullptr;

	hb::logger::shutdown();

	return 0;
}
