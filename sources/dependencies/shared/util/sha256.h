#pragma once

#include <string>
#include <cstddef>

namespace hb::shared::util {

// Compute SHA256 hash of a byte buffer. Returns lowercase hex string (64 chars).
std::string sha256(const void* data, std::size_t length);

// Compute SHA256 hash of a file. Returns lowercase hex string. Empty on failure.
std::string sha256_file(const char* file_path);

} // namespace hb::shared::util
