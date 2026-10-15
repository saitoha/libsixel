/* SPDX-License-Identifier: MIT */
#ifndef SIXEL_DECODER_SIMD_H
#define SIXEL_DECODER_SIMD_H

/*
 * Vector helpers for the band-word decoders. Each helper reproduces the
 * scalar word update of decoder-columns.h or decoder-ormode.h exactly; the
 * scalar loops remain the reference and the fallback. The NEON variants
 * need AArch64 table lookups and horizontal reductions, and assume the
 * little-endian byte lanes that the column words are defined with.
 */

#include <stdint.h>

#if defined(HAVE_NEON) && HAVE_NEON && \
    defined(HAVE_ARM_NEON_H) && HAVE_ARM_NEON_H && \
    (defined(__aarch64__) || defined(_M_ARM64)) && \
    !defined(__ARM_BIG_ENDIAN)
# include <arm_neon.h>
# define SIXEL_DECODER_NEON 1
#else
# define SIXEL_DECODER_NEON 0
#endif

#if SIXEL_DECODER_NEON

/* Lane i of pair k selects sixel 2k (lanes 0-7) or 2k+1 (lanes 8-15). */
static uint8_t const sixel_simd_pair_index[8][16] = {
    { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1 },
    { 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3 },
    { 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5 },
    { 6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7 },
    { 8, 8, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9, 9, 9 },
    { 10, 10, 10, 10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 11, 11 },
    { 12, 12, 12, 12, 12, 12, 12, 12, 13, 13, 13, 13, 13, 13, 13, 13 },
    { 14, 14, 14, 14, 14, 14, 14, 14, 15, 15, 15, 15, 15, 15, 15, 15 }
};


/*
 * Validate sixteen bytes as sixel data and return their six-bit patterns.
 * Bytes below '?' wrap above 63 after the subtraction, so one maximum test
 * rejects both sides of the range.
 */
static inline int
sixel_simd_load_sixels(unsigned char const *p, uint8x16_t *patterns)
{
    uint8x16_t bits;

    bits = vsubq_u8(vld1q_u8(p), vdupq_n_u8('?'));
    if (vmaxvq_u8(bits) > 63) {
        return 0;
    }
    *patterns = bits;
    return 1;
}

/* Fold the sixteen patterns into the union of painted rows and the offset
 * of the last nonzero pattern, matching the scalar run bookkeeping. */
static inline void
sixel_simd_run_extent(uint8x16_t bits, int *rows, int *last)
{
    uint8x8_t folded;
    uint64_t word;
    uint64_t nonzero;

    folded = vorr_u8(vget_low_u8(bits), vget_high_u8(bits));
    word = vget_lane_u64(vreinterpret_u64_u8(folded), 0);
    word |= word >> 32;
    word |= word >> 16;
    word |= word >> 8;
    *rows |= (int)(word & 63u);
    nonzero = vget_lane_u64(vreinterpret_u64_u8(vshrn_n_u16(
        vreinterpretq_u16_u8(vtstq_u8(bits, bits)), 4)), 0);
    *last = nonzero == 0 ? (-1) : 15 - (__builtin_clzll(nonzero) >> 2);
}

/*
 * OR mode: OR sixteen sixels into the byte lanes selected by the register
 * spread, i.e. word |= bits * spread per column. The spread holds 0 or 1
 * per lane, so a lane mask of spread * 0xff gives the same product.
 * Returns 0 without writing when any byte is not sixel data. OR runs are
 * long enough for this whole-block form; ordinary runs are not, and their
 * scalar column update is already about 1.4 cycles per column.
 */
static inline int
sixel_simd_planes_or16(uint64_t *dst, uint64_t spread,
                       unsigned char const *p, int *rows, int *last)
{
    uint8x16_t bits;
    uint8x16_t lanes;
    uint8x16_t select;
    uint8_t *words;
    int k;

    if (!sixel_simd_load_sixels(p, &bits)) {
        return 0;
    }
    select = vreinterpretq_u8_u64(vdupq_n_u64(spread * UINT64_C(0xff)));
    words = (uint8_t *)dst;
    for (k = 0; k < 8; ++k) {
        lanes = vqtbl1q_u8(bits, vld1q_u8(sixel_simd_pair_index[k]));
        vst1q_u8(words + k * 16,
                 vorrq_u8(vld1q_u8(words + k * 16),
                          vandq_u8(lanes, select)));
    }
    sixel_simd_run_extent(bits, rows, last);
    return 1;
}

/* Lane p of each word is shifted left by p so a pairwise add packs it. */
static int8_t const sixel_simd_plane_shift[16] = {
    0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7
};

/*
 * OR mode: gather row r of sixteen band words into sixteen palette indices,
 * i.e. sixel_ormode_gather() per word. Each byte lane holds one plane; the
 * row bit is isolated, moved to its plane position, and three pairwise adds
 * fold the eight lanes of every word into one byte, keeping word order.
 */
static inline uint8x16_t
sixel_simd_gather16(uint64_t const *words, int row)
{
    uint8x16_t v[8];
    uint8x16_t one;
    int8x16_t down;
    int8x16_t up;
    uint8_t const *bytes;
    int k;

    one = vdupq_n_u8(1);
    down = vdupq_n_s8((int8_t)-row);
    up = vld1q_s8(sixel_simd_plane_shift);
    bytes = (uint8_t const *)words;
    for (k = 0; k < 8; ++k) {
        v[k] = vshlq_u8(vandq_u8(vshlq_u8(vld1q_u8(bytes + k * 16), down),
                                 one), up);
    }
    v[0] = vpaddq_u8(v[0], v[1]);
    v[1] = vpaddq_u8(v[2], v[3]);
    v[2] = vpaddq_u8(v[4], v[5]);
    v[3] = vpaddq_u8(v[6], v[7]);
    v[0] = vpaddq_u8(v[0], v[1]);
    v[1] = vpaddq_u8(v[2], v[3]);
    return vpaddq_u8(v[0], v[1]);
}

#endif /* SIXEL_DECODER_NEON */

#endif /* SIXEL_DECODER_SIMD_H */
