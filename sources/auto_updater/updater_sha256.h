#pragma once
#include <string>
#include <cstddef>

namespace hb::updater
{
	// Compute SHA256 hash of a file. Returns lowercase hex string.
	// Returns empty string on file read failure.
	std::string sha256_file(const char* file_path);

	// Compute SHA256 hash of a byte buffer. Returns lowercase hex string.
	std::string sha256_bytes(const void* data, std::size_t length);
}
