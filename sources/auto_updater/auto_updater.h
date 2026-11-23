#pragma once

namespace hb::updater
{
	enum class update_result
	{
		no_update,          // Already up to date
		updated,            // Files updated, no restart needed
		restart_required,   // Exe updated, restart needed
		server_unreachable, // Couldn't reach update server (continue with current version)
		error               // Update failed (continue with current version)
	};

	// Call from GameMain() before any SFML init.
	// Returns immediately if server is unreachable (non-blocking fail).
	update_result check_for_updates();

	// Re-execute the current process (used after exe swap).
	[[noreturn]] void relaunch();
}
