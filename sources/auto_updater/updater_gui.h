#pragma once

namespace hb::updater
{
	class updater_gui
	{
	public:
		updater_gui();
		~updater_gui();

		bool create();                          // Show centered window
		void destroy();                         // Close window
		void set_status(const char* text);      // "Downloading file 3 of 12..."
		void set_progress(float fraction);      // 0.0 to 1.0
		void pump_messages();                   // Process OS message queue
		bool is_cancelled() const;              // User closed window

	private:
		struct impl;
		impl* m_impl = nullptr;
	};

	// Standalone dialog (no window needed).
	// Returns true if the user wants to retry, false to skip updates.
	bool show_server_unreachable_dialog();
}
