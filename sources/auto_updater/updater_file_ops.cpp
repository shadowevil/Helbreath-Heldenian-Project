#include "updater_file_ops.h"
#include "updater_constants.h"
#include "updater_platform.h"
#include <chrono>
#include <filesystem>
#include <cstdlib>
#include <thread>

namespace fs = std::filesystem;

namespace hb::updater
{
	std::string get_exe_path()
	{
#ifdef _WIN32
		char buf[MAX_PATH] = {};
		GetModuleFileNameA(nullptr, buf, MAX_PATH);
		return std::string(buf);
#else
		char buf[PATH_MAX] = {};
		ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
		if (len > 0)
		{
			buf[len] = '\0';
			return std::string(buf);
		}
		return {};
#endif
	}

	std::string get_exe_directory()
	{
		auto exe = fs::path(get_exe_path());
		return exe.parent_path().string();
	}

	bool ensure_staging_dir(const std::string& base_dir)
	{
		auto staging = fs::path(base_dir) / staging_dir;
		std::error_code ec;
		fs::create_directories(staging, ec);
		return !ec;
	}

	bool disperse_file(const std::string& staged_path, const std::string& final_path,
		bool is_executable)
	{
		std::error_code ec;

		if (is_executable)
		{
			// For executables, place as .update — swap happens on next launch
			std::string update_path = final_path + exe_update_suffix;
			fs::copy_file(staged_path, update_path, fs::copy_options::overwrite_existing, ec);
		}
		else
		{
			// For data files, overwrite directly
			auto parent = fs::path(final_path).parent_path();
			if (!parent.empty())
				fs::create_directories(parent, ec);

			fs::copy_file(staged_path, final_path, fs::copy_options::overwrite_existing, ec);
		}

		return !ec;
	}

	bool apply_pending_exe_swap(const std::string& exe_path)
	{
		std::string update_path = exe_path + exe_update_suffix;
		std::string old_path = exe_path + exe_old_suffix;

		if (!fs::exists(update_path))
			return false;

		std::error_code ec;

		// Rename running exe → .old
		fs::rename(exe_path, old_path, ec);
		if (ec)
			return false;

		// Rename .update → current exe
		fs::rename(update_path, exe_path, ec);
		if (ec)
		{
			// Rollback: restore old exe
			fs::rename(old_path, exe_path, ec);
			return false;
		}

		return true;
	}

	void cleanup_old_exe(const std::string& exe_path)
	{
		std::string old_path = exe_path + exe_old_suffix;
		if (!fs::exists(old_path))
			return;

		// Retry a few times — on Windows, _execl spawns a child then exits
		// the parent, so the .old file may still be locked briefly.
		for (int i = 0; i < 10; ++i)
		{
			std::error_code ec;
			fs::remove(old_path, ec);
			if (!ec || !fs::exists(old_path))
				return;

			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	}

	void cleanup_staging(const std::string& base_dir)
	{
		auto staging = fs::path(base_dir) / staging_dir;
		std::error_code ec;
		fs::remove_all(staging, ec);
	}

	[[noreturn]] void relaunch()
	{
		std::string exe = get_exe_path();

#ifdef _WIN32
		// Use _execl to replace current process
		_execl(exe.c_str(), exe.c_str(), nullptr);
#else
		execl(exe.c_str(), exe.c_str(), nullptr);
#endif
		// If exec fails, exit
		std::exit(1);
	}
}
