#pragma once

#include <array>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string_view>

namespace hb::logger {

class log_backend
{
public:
	static constexpr int max_channels = 32;

	struct config
	{
		std::string_view directory;
		const std::string_view* filenames = nullptr;
		int channel_count = 0;
		std::string_view (*channel_namer)(int) = nullptr;
	};

	void init(const config& cfg);
	void write(int channel, int level, std::string_view message);
	void close();

protected:
	virtual void write_console(int channel, std::string_view colored_line);

private:
	std::array<std::ofstream, max_channels> m_files;
	std::mutex m_mutex;
	std::filesystem::path m_directory;
	const std::string_view* m_filenames = nullptr;
	std::string_view (*m_channel_namer)(int) = nullptr;
	int m_channel_count = 0;
	bool m_initialized = false;
};

} // namespace hb::logger
