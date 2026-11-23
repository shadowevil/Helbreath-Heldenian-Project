#pragma once

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>

#ifdef _WIN32
	#include <windows.h>
	#include <bcrypt.h>
#else
	#include <fstream>
#endif

#include "vendor/picosha2.h"

namespace PasswordHash
{

constexpr int SaltHexLen = 33;  // 16 bytes = 32 hex chars + null
constexpr int HashHexLen = 65;  // 32 bytes = 64 hex chars + null

// =========================================================================
// Portable SHA-256 (FIPS 180-4)
// =========================================================================

namespace detail {

inline void sha256(const unsigned char* data, size_t len, unsigned char out[32])
{
	std::vector<unsigned char> hash(picosha2::k_digest_size);
	picosha2::hash256(data, data + len, hash.begin(), hash.end());
	std::memcpy(out, hash.data(), 32);
}

} // namespace detail

// =========================================================================
// Public API (same interface as before)
// =========================================================================

inline void BytesToHex(const unsigned char* bytes, size_t len, char* outHex, size_t outSize)
{
	for (size_t i = 0; i < len && (i * 2 + 2) < outSize; i++) {
		std::snprintf(outHex + i * 2, 3, "%02x", bytes[i]);
	}
}

inline bool HexToBytes(const char* hex, unsigned char* outBytes, size_t outLen)
{
	for (size_t i = 0; i < outLen; i++) {
		unsigned int val = 0;
		if (std::sscanf(hex + i * 2, "%02x", &val) != 1) {
			return false;
		}
		outBytes[i] = static_cast<unsigned char>(val);
	}
	return true;
}

inline bool GenerateSalt(char* outSaltHex, size_t outSize)
{
	if (outSize < SaltHexLen) return false;

	unsigned char saltBytes[16] = {};

#ifdef _WIN32
	NTSTATUS status = BCryptGenRandom(nullptr, saltBytes, sizeof(saltBytes), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
	if (status != 0) return false;
#else
	std::ifstream urandom("/dev/urandom", std::ios::binary);
	if (!urandom.read(reinterpret_cast<char*>(saltBytes), sizeof(saltBytes)))
		return false;
#endif

	std::memset(outSaltHex, 0, outSize);
	BytesToHex(saltBytes, sizeof(saltBytes), outSaltHex, outSize);
	return true;
}

inline bool HashPassword(const char* password, const char* saltHex, char* outHashHex, size_t outSize)
{
	if (outSize < HashHexLen) return false;

	// Construct input: saltHex + password
	char input[256] = {};
	std::snprintf(input, sizeof(input), "%s%s", saltHex, password);
	size_t inputLen = std::strlen(input);

	unsigned char hashResult[32] = {};
	detail::sha256(reinterpret_cast<const unsigned char*>(input), inputLen, hashResult);

	std::memset(outHashHex, 0, outSize);
	BytesToHex(hashResult, sizeof(hashResult), outHashHex, outSize);

	// Clear sensitive data
	volatile char* p = input;
	for (size_t i = 0; i < sizeof(input); i++) p[i] = 0;

	return true;
}

inline bool VerifyPassword(const char* password, const char* saltHex, const char* storedHashHex)
{
	char computedHash[HashHexLen] = {};
	if (!HashPassword(password, saltHex, computedHash, sizeof(computedHash))) {
		return false;
	}
	return std::strcmp(computedHash, storedHashHex) == 0;
}

} // namespace PasswordHash
