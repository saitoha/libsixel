/* SPDX-License-Identifier: MIT */
#ifndef SIXEL_DECODER_COLUMNS_H
#define SIXEL_DECODER_COLUMNS_H

#include <stdint.h>

/* Six byte lanes hold overwrite-mode registers. Byte six is independent
 * paint coverage, so register zero remains distinct from an untouched cell.
 * A constant table avoids shared lazy initialization in concurrent decodes. */
static uint64_t const sixel_column_row_masks[64] = {
    UINT64_C(0x000000000000), UINT64_C(0x0000000000ff),
    UINT64_C(0x00000000ff00), UINT64_C(0x00000000ffff),
    UINT64_C(0x000000ff0000), UINT64_C(0x000000ff00ff),
    UINT64_C(0x000000ffff00), UINT64_C(0x000000ffffff),
    UINT64_C(0x0000ff000000), UINT64_C(0x0000ff0000ff),
    UINT64_C(0x0000ff00ff00), UINT64_C(0x0000ff00ffff),
    UINT64_C(0x0000ffff0000), UINT64_C(0x0000ffff00ff),
    UINT64_C(0x0000ffffff00), UINT64_C(0x0000ffffffff),
    UINT64_C(0x00ff00000000), UINT64_C(0x00ff000000ff),
    UINT64_C(0x00ff0000ff00), UINT64_C(0x00ff0000ffff),
    UINT64_C(0x00ff00ff0000), UINT64_C(0x00ff00ff00ff),
    UINT64_C(0x00ff00ffff00), UINT64_C(0x00ff00ffffff),
    UINT64_C(0x00ffff000000), UINT64_C(0x00ffff0000ff),
    UINT64_C(0x00ffff00ff00), UINT64_C(0x00ffff00ffff),
    UINT64_C(0x00ffffff0000), UINT64_C(0x00ffffff00ff),
    UINT64_C(0x00ffffffff00), UINT64_C(0x00ffffffffff),
    UINT64_C(0xff0000000000), UINT64_C(0xff00000000ff),
    UINT64_C(0xff000000ff00), UINT64_C(0xff000000ffff),
    UINT64_C(0xff0000ff0000), UINT64_C(0xff0000ff00ff),
    UINT64_C(0xff0000ffff00), UINT64_C(0xff0000ffffff),
    UINT64_C(0xff00ff000000), UINT64_C(0xff00ff0000ff),
    UINT64_C(0xff00ff00ff00), UINT64_C(0xff00ff00ffff),
    UINT64_C(0xff00ffff0000), UINT64_C(0xff00ffff00ff),
    UINT64_C(0xff00ffffff00), UINT64_C(0xff00ffffffff),
    UINT64_C(0xffff00000000), UINT64_C(0xffff000000ff),
    UINT64_C(0xffff0000ff00), UINT64_C(0xffff0000ffff),
    UINT64_C(0xffff00ff0000), UINT64_C(0xffff00ff00ff),
    UINT64_C(0xffff00ffff00), UINT64_C(0xffff00ffffff),
    UINT64_C(0xffffff000000), UINT64_C(0xffffff0000ff),
    UINT64_C(0xffffff00ff00), UINT64_C(0xffffff00ffff),
    UINT64_C(0xffffffff0000), UINT64_C(0xffffffff00ff),
    UINT64_C(0xffffffffff00), UINT64_C(0xffffffffffff)
};

static inline uint64_t
sixel_column_color(unsigned int index)
{
    return (uint64_t)index * UINT64_C(0x010101010101);
}

static inline void
sixel_column_store(uint64_t *dst, uint64_t color, int bits, int repeat)
{
    uint64_t mask;
    uint64_t paint;
    int x;

    if (bits == 0) {
        /* An empty sixel only advances the cursor. Repeated empties are
         * the bulk of size-policy cursor skips; touching them is wasted. */
        return;
    }
    mask = sixel_column_row_masks[bits];
    paint = (color & mask) | ((uint64_t)bits << 48);
    if (bits == 63) {
        /* All six rows are replaced and fully covered, and byte seven is
         * always zero, so the result does not depend on the old word. This
         * is the common size-policy underpaint, often hundreds of columns. */
        for (x = 0; x < repeat; ++x) {
            dst[x] = paint;
        }
        return;
    }
    for (x = 0; x < repeat; ++x) {
        dst[x] = (dst[x] & ~mask) | paint;
    }
}

#endif /* SIXEL_DECODER_COLUMNS_H */
