/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <string.h>
#include "cpu.h"
#include "retained-plane-rows.h"

/* A shared configuration may target another architecture in a universal
 * build. Both the compiler target and available intrinsics must agree. */
#if HAVE_NEON && HAVE_ARM_NEON_H && defined(__aarch64__)
#define SIXEL_RETAINED_NEON 1
#include <arm_neon.h>
#else
#define SIXEL_RETAINED_NEON 0
#endif

SIXEL_INTERNAL_API void
sixel_retained_rows_prepare(sixel_retained_rows_t *rows,
    sixel_wire_palette_view_t const *palette)
{
#if SIXEL_RETAINED_NEON
    size_t i;
#endif

    rows->palette = *palette;
    rows->neon = 0;
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
