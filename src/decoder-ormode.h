/* SPDX-License-Identifier: MIT */
#ifndef SIXEL_DECODER_ORMODE_H
#define SIXEL_DECODER_ORMODE_H

#include <stdint.h>

/* Spread eight register bits into the low bit of eight byte lanes. A sixel
 * has only six bits, so multiplying this word by its pattern cannot carry
 * between lanes, even for composite registers such as #3. */
static inline uint64_t
sixel_ormode_spread(unsigned int index)
{
    uint64_t word;

    word = index & 255u;
    word = (word | (word << 28)) & UINT64_C(0x0000000f0000000f);
    word = (word | (word << 14)) & UINT64_C(0x0003000300030003);
    return (word | (word << 7)) & UINT64_C(0x0101010101010101);
}

/* Extract row r from all eight planes. The multiply packs the isolated
 * bits into one byte; the unsigned overflow is intentional. */
static inline unsigned int
sixel_ormode_gather(uint64_t word, int row)
{
    return (unsigned int)((((word >> row) &
        UINT64_C(0x0101010101010101)) * UINT64_C(0x0102040810204080)) >> 56);
}

static inline void
sixel_ormode_store(uint64_t *dst, uint64_t spread, int bits, int repeat)
{
    uint64_t pattern;
    int x;

    pattern = (uint64_t)bits * spread;
    if (pattern == 0) {
        /* Empty sixels and unused planes only advance the cursor. */
        return;
    }
    for (x = 0; x < repeat; ++x) {
        dst[x] |= pattern;
    }
}

#endif /* SIXEL_DECODER_ORMODE_H */
