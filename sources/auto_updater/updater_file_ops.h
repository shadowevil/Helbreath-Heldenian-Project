#pragma once
#include <string>

namespace hb::updater
{
	// Get the directory containing the running executable.
	std::string get_exe_directory();

	// Get the path to the running executable.
	std::string get_exe_path();

	// Create staging directory if it doesn't exist.
	bool ensure_staging_dir(const std::string& base_dir);

	// Move a staged file to its final location.
	// For executables: copies to <final_path>.update (swap happens on next launch).
	// For data files: overwrites directly, creating parent directories as needed.
	bool disperse_file(const std::string& staged_path, const std::string& final_path,
		bool is_executable);

	// Check for pending exe swap from previous run. Returns true if swap was performed.
	bool apply_pending_exe_swap(const std::string& exe_path);

	// Clean up .old exe files from previous swap.
	void cleanup_old_exe(const std::string& exe_path);

	// Remove the staging directory and all contents.
	void cleanup_staging(const std::string& base_dir);

	// Re-execute the current process (used after exe swap).
	[[noreturn]] void relaunch();
}
