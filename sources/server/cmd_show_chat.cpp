#include "cmd_show_chat.h"
#include "server_console.h"
#include <cstdio>
#include <filesystem>

#ifdef _WIN32

void CmdShowChat::execute(CGame* game, const char* args)
{
	if (m_hProcess != nullptr)
	{
		DWORD exit_code = 0;
		if (GetExitCodeProcess(m_hProcess, &exit_code) && exit_code == STILL_ACTIVE)
		{
			hb::console::warn("Chat viewer is already open.");
			return;
		}
		CloseHandle(m_hProcess);
		m_hProcess = nullptr;
	}

	std::string logPath = std::filesystem::absolute("gamelogs/chat.log").string();

	char cmd_line[1024];
	std::snprintf(cmd_line, sizeof(cmd_line),
		"powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \""
		"$Host.UI.RawUI.WindowTitle = 'HB Chat'; "
		"Get-Content '%s' -Wait -Tail 0 | Select-String -NotMatch -SimpleMatch '[DEBUG]'\"",
		logPath.c_str());

	STARTUPINFOA si = {};
	si.cb = sizeof(si);
	PROCESS_INFORMATION pi = {};

	BOOL result = CreateProcessA(
		nullptr,
		cmd_line,
		nullptr,
		nullptr,
		FALSE,
		CREATE_NEW_CONSOLE,
		nullptr,
		nullptr,
		&si,
		&pi
	);

	if (result)
	{
		CloseHandle(pi.hThread);
		m_hProcess = pi.hProcess;
		hb::console::success("Chat viewer opened in new terminal.");
	}
	else
	{
		hb::console::error("Failed to open chat viewer (error {}).", GetLastError());
	}
}

#else // POSIX

void CmdShowChat::execute(CGame* game, const char* args)
{
	std::string logPath = std::filesystem::absolute("gamelogs/chat.log").string();
	std::string cmd = "tail -f \"" + logPath + "\" | grep -v '\\[DEBUG\\]' &";
	int ret = std::system(cmd.c_str());
	if (ret == 0)
		hb::console::success("Chat viewer started (tail -f).");
	else
		hb::console::error("Failed to open chat viewer (exit code {}).", ret);
}

#endif
