/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#include "simd-config.h"
#endif

#include <string.h>
#include "cpu.h"
#include "retained-plane-rows.h"

/* A shared configuration may target another architecture in a universal
 * build. Both the compiler target and available intrinsics must agree. */
#if SIXEL_SIMD_NEON_A64
#define SIXEL_RETAINED_NEON 1
#include <arm_neon.h>
#else
#define SIXEL_RETAINED_NEON 0
#endif

#if SIXEL_SIMD_AVX2
#define SIXEL_RETAINED_AVX2 1
#include <immintrin.h>
#else
#define SIXEL_RETAINED_AVX2 0
#endif

SIXEL_INTERNAL_API void
sixel_retained_rows_prepare(sixel_retained_rows_t *rows,
    sixel_wire_palette_view_t const *palette)
{
#if SIXEL_RETAINED_NEON || SIXEL_RETAINED_AVX2
    size_t i;
#endif

    rows->palette = *palette;
    rows->neon = 0;
    rows->avx2 = 0;
#if SIXEL_RETAINED_AVX2
    if (rows->width >= 16 &&
            sixel_cpu_simd_level() == SIXEL_SIMD_LEVEL_AVX2) {
        /* A kept sentinel may address any table slot. Initialize all slots
         * even though kept colors are discarded by the write mask. */
        memset(rows->packed_rgb, 0, sizeof(rows->packed_rgb));
        for (i = 0; i < palette->count; ++i) {
            rows->packed_rgb[i] = (int)palette->colors[i].emitted_rgb[0] |
                ((int)palette->colors[i].emitted_rgb[1] << 8) |
                ((int)palette->colors[i].emitted_rgb[2] << 16);
        }
        rows->avx2 = 1;
    }
#endif
#if SIXEL_RETAINED_NEON
    if (rows->width < 16 ||
            sixel_cpu_simd_level() != SIXEL_SIMD_LEVEL_NEON) {
        return;
    }
    /* Kept lanes may carry any byte, including an out-of-palette sentinel.
     * Initialize those LUT entries too; their values are never published. */
    memset(rows->lut, 0, sizeof(rows->lut));
    for (i = 0; i < palette->count; ++i) {
        rows->lut[0][i] = palette->colors[i].emitted_rgb[0];
        rows->lut[1][i] = palette->colors[i].emitted_rgb[1];
        rows->lut[2][i] = palette->colors[i].emitted_rgb[2];
    }
    rows->neon = 1;
#endif
}

#if SIXEL_RETAINED_NEON
/* Accel: RETAINED-ROWS neon */
static uint8x16_t
sixel_retained_rows_gather(unsigned char const *lut, uint8x16_t indexes)
{
    uint8x16x4_t table;
    uint8x16_t result;
    int i;

    result = vdupq_n_u8(0);
    /* Each selector belongs to exactly one disjoint 64-byte table. TBL's
     * out-of-range zero lets OR combine them without rounding or widening. */
    for (i = 0; i < 4; ++i) {
        table.val[0] = vld1q_u8(lut + i * 64);
        table.val[1] = vld1q_u8(lut + i * 64 + 16);
        table.val[2] = vld1q_u8(lut + i * 64 + 32);
        table.val[3] = vld1q_u8(lut + i * 64 + 48);
        result = vorrq_u8(result, vqtbl4q_u8(table,
            vsubq_u8(indexes, vdupq_n_u8((unsigned char)(i * 64)))));
    }
    return result;
}
#endif

#if SIXEL_RETAINED_AVX2
/* Gather eight packed RGB entries, then remove each fourth padding byte.
 * The two 128-bit lanes each hold twelve useful bytes. Stores cover exactly
 * 24 bytes, so the last row never reads or writes beyond its RGB extent. */
/* Accel: RETAINED-ROWS avx2 */
static SIXEL_TARGET_AVX2 int
sixel_retained_rows_avx2(sixel_retained_rows_t const *rows,
    size_t index, size_t target, sixel_retained_update_stats_t *stats)
{
    __m256i colors;
    __m256i selectors;
    __m256i packed;
    __m256i shuffle;
    __m128i skips;
    __m128i mask;
    __m128i lo;
    __m128i hi;
    __m128i rgb0;
    __m128i rgb1;
    __m128i mask0;
    __m128i mask1;
    __m128i previous;
    __m128i ones;
    unsigned char *destination;
    unsigned int painted;
    int x;

    shuffle = _mm256_setr_epi8(0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14,
        -1, -1, -1, -1, 0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14,
        -1, -1, -1, -1);
    ones = _mm_set1_epi8(1);
    for (x = 0; x <= rows->width - 8;
            x += 8, index += 8, target += 8) {
        skips = rows->skip != NULL ?
            _mm_loadl_epi64((__m128i const *)(rows->skip + index)) :
            _mm_setzero_si128();
        stats->refinement_needed |= (_mm_movemask_epi8(_mm_cmpeq_epi8(
            skips, _mm_set1_epi8(SIXEL_INDEXED_KEEP_APPROXIMATE))) & 255)
            != 0;
        mask = _mm_cmpeq_epi8(skips, _mm_setzero_si128());
        painted = (unsigned int)_mm_movemask_epi8(mask) & 255U;
        if (painted == 0U) continue;
        selectors = _mm256_cvtepu8_epi32(_mm_loadl_epi64(
            (__m128i const *)(rows->indexes + index)));
        colors = _mm256_i32gather_epi32(rows->packed_rgb, selectors, 4);
        packed = _mm256_shuffle_epi8(colors, shuffle);
        lo = _mm256_castsi256_si128(packed);
        hi = _mm256_extracti128_si256(packed, 1);
        rgb0 = _mm_or_si128(lo, _mm_slli_si128(hi, 12));
        rgb1 = _mm_srli_si128(hi, 4);
        destination = rows->rgb + target * 3;
        if (painted != 255U) {
            /* Expand the keep decision through the same RGB byte layout. */
            packed = _mm256_shuffle_epi8(_mm256_cvtepi8_epi32(mask),
                shuffle);
            lo = _mm256_castsi256_si128(packed);
            hi = _mm256_extracti128_si256(packed, 1);
            mask0 = _mm_or_si128(lo, _mm_slli_si128(hi, 12));
            mask1 = _mm_srli_si128(hi, 4);
            previous = _mm_loadu_si128((__m128i const *)destination);
            rgb0 = _mm_blendv_epi8(previous, rgb0, mask0);
            previous = _mm_loadl_epi64((__m128i const *)(destination + 16));
            rgb1 = _mm_blendv_epi8(previous, rgb1, mask1);
        }
        _mm_storeu_si128((__m128i *)destination, rgb0);
        _mm_storel_epi64((__m128i *)(destination + 16), rgb1);
        previous = _mm_loadl_epi64((__m128i const *)(rows->valid + target));
        _mm_storel_epi64((__m128i *)(rows->valid + target),
            _mm_blendv_epi8(previous, ones, mask));
        if (rows->paint != NULL) {
            previous = _mm_loadl_epi64(
                (__m128i const *)(rows->paint + index));
            _mm_storel_epi64((__m128i *)(rows->paint + index),
                _mm_blendv_epi8(previous, ones, mask));
        }
        /* SAD counts the low eight painted lanes without requiring POPCNT. */
        stats->painted_pixels += (unsigned int)_mm_cvtsi128_si32(
            _mm_sad_epu8(_mm_and_si128(mask, ones), _mm_setzero_si128()));
    }
    return x;
}
#endif

/* Accel: RETAINED-ROWS neon */
SIXEL_INTERNAL_API void
sixel_retained_rows_run(sixel_retained_rows_t const *rows,
    int first, int last, sixel_retained_update_stats_t *stats)
{
    size_t index;
    size_t target;
    unsigned int skip;
    int x;
    int y;
#if SIXEL_RETAINED_NEON
    unsigned int count;
    uint8x16_t skips;
    uint8x16_t mask;
    uint8x16_t indexes;
    uint8x16x3_t rgb;
    uint8x16x3_t previous;
#endif

    index = (size_t)first * (size_t)rows->width;
    for (y = first; y < last; ++y) {
        target = (size_t)y * rows->stride;
        x = 0;
#if SIXEL_RETAINED_AVX2
        if (rows->avx2) {
            x = sixel_retained_rows_avx2(rows, index, target, stats);
            index += (size_t)x;
            target += (size_t)x;
        }
#endif
#if SIXEL_RETAINED_NEON
        if (rows->neon) {
            for (; x <= rows->width - 16;
                    x += 16, index += 16, target += 16) {
                skips = rows->skip != NULL ? vld1q_u8(rows->skip + index) :
                    vdupq_n_u8(0);
                /* Approximate keeps still request refinement when a block
                 * has no painted lane. Reduce before the all-keep shortcut. */
                stats->refinement_needed |= vmaxvq_u8(vceqq_u8(skips,
                    vdupq_n_u8(SIXEL_INDEXED_KEEP_APPROXIMATE))) != 0;
                mask = vceqq_u8(skips, vdupq_n_u8(0));
                count = vaddvq_u8(vandq_u8(mask, vdupq_n_u8(1)));
                if (count == 0) continue;
                indexes = vld1q_u8(rows->indexes + index);
                rgb.val[0] = sixel_retained_rows_gather(rows->lut[0], indexes);
                rgb.val[1] = sixel_retained_rows_gather(rows->lut[1], indexes);
                rgb.val[2] = sixel_retained_rows_gather(rows->lut[2], indexes);
                if (count != 16) {
                    previous = vld3q_u8(rows->rgb + target * 3);
                    rgb.val[0] = vbslq_u8(mask, rgb.val[0], previous.val[0]);
                    rgb.val[1] = vbslq_u8(mask, rgb.val[1], previous.val[1]);
                    rgb.val[2] = vbslq_u8(mask, rgb.val[2], previous.val[2]);
                }
                vst3q_u8(rows->rgb + target * 3, rgb);
                vst1q_u8(rows->valid + target, vbslq_u8(mask,
                    vdupq_n_u8(1), vld1q_u8(rows->valid + target)));
                if (rows->paint != NULL) {
                    vst1q_u8(rows->paint + index, vbslq_u8(mask,
                        vdupq_n_u8(1), vld1q_u8(rows->paint + index)));
                }
                stats->painted_pixels += count;
            }
        }
#endif
        /* Scalar targets and short row tails preserve the same emitted RGB
         * interpretation. No read through an invalid kept index is allowed. */
        for (; x < rows->width; ++x, ++index, ++target) {
            skip = rows->skip != NULL ? rows->skip[index] : 0;
            if (skip != 0) {
                stats->refinement_needed |=
                    skip == SIXEL_INDEXED_KEEP_APPROXIMATE;
                continue;
            }
            memcpy(rows->rgb + target * 3,
                rows->palette.colors[rows->indexes[index]].emitted_rgb, 3);
            rows->valid[target] = 1;
            if (rows->paint != NULL) rows->paint[index] = 1;
            ++stats->painted_pixels;
        }
    }
}
