#pragma once

#include <cstdint>

// Network packets are treated as little-endian, matching the existing client/server usage.
// No byte swapping is performed to preserve the current protocol behavior.

#if defined(_MSC_VER)
#define HB_PACK_BEGIN __pragma(pack(push, 1))
#define HB_PACK_END __pragma(pack(pop))
#else
#define HB_PACK_BEGIN
#define HB_PACK_END
#endif

#if defined(__GNUC__) || defined(__clang__)
#define HB_PACKED __attribute__((packed))
#else
#define HB_PACKED
#endif
