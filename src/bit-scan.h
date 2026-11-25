/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_BIT_SCAN_H
#define LIBSIXEL_BIT_SCAN_H

#include <stdint.h>

#if HAVE_BITSCAN_FORWARD64 || HAVE_BITSCAN_REVERSE64
# include <intrin.h>
#endif

/* Callers supply a nonzero word. Return the zero-based bit position. */
static inline int
sixel_bit_scan_forward64(uint64_t bits)
{
#if HAVE_BUILTIN_CTZLL
    return __builtin_ctzll((unsigned long long)bits);
#elif HAVE_BITSCAN_FORWARD64
    unsigned long index;

    index = 0;
    (void)_BitScanForward64(&index, bits);
    return (int)index;
#else
    int shift;

    shift = 0;
    if ((bits & UINT64_C(0xffffffff)) == 0) {
        bits >>= 32;
        shift += 32;
    }
    if ((bits & UINT64_C(0xffff)) == 0) {
        bits >>= 16;
        shift += 16;
    }
    if ((bits & UINT64_C(0xff)) == 0) {
        bits >>= 8;
        shift += 8;
    }
    if ((bits & UINT64_C(0xf)) == 0) {
        bits >>= 4;
        shift += 4;
    }
    if ((bits & UINT64_C(3)) == 0) {
        bits >>= 2;
        shift += 2;
    }
    return shift + ((bits & UINT64_C(1)) == 0);
#endif
}

/* Same precondition as the forward scan, counting from the same end. */
static inline unsigned int
sixel_bit_scan_reverse64(uint64_t bits)
{
#if HAVE_BUILTIN_CLZLL
    return 63u - (unsigned int)__builtin_clzll((unsigned long long)bits);
#elif HAVE_BITSCAN_REVERSE64
    unsigned long index;

    index = 0;
    (void)_BitScanReverse64(&index, bits);
    return (unsigned int)index;
#else
    unsigned int shift;

    shift = 0;
    if (bits >> 32) {
        bits >>= 32;
        shift += 32;
    }
    if (bits >> 16) {
        bits >>= 16;
        shift += 16;
    }
    if (bits >> 8) {
        bits >>= 8;
        shift += 8;
    }
    if (bits >> 4) {
        bits >>= 4;
        shift += 4;
    }
    if (bits >> 2) {
        bits >>= 2;
        shift += 2;
    }
    return shift + (unsigned int)(bits >> 1);
#endif
}

#endif /* LIBSIXEL_BIT_SCAN_H */
