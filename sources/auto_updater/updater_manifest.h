#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace hb::updater
{
	struct manifest_entry
	{
		std::string path;       // Relative path from game root (e.g. "sprites/H-Earth1.pak")
		std::string sha256;     // Expected SHA256 hex digest
		std::string platform;   // "windows", "linux", or "any"
		uint64_t size;          // File size in bytes
		bool is_executable;     // True for .exe / linux binary — uses swap-on-restart
	};

	struct update_manifest
	{
		int version_major = 0;
		int version_minor = 0;
		int version_patch = 0;
		std::vector<manifest_entry> files;
	};

	// Parse JSON manifest string. Returns false on parse error.
	bool parse_manifest(const std::string& json_text, update_manifest& out);
}
