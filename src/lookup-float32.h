/*
 * SPDX-License-Identifier: MIT
 */

#ifndef LIBSIXEL_LOOKUP_FLOAT32_H
#define LIBSIXEL_LOOKUP_FLOAT32_H

#include "allocator.h"

#include <sixel.h>

#include <stddef.h>

enum { SIXEL_LOOKUP_FLOAT_COMPONENTS = 3 };

typedef struct sixel_lookup_float32_node sixel_lookup_float32_node_t;

typedef struct sixel_lookup_float32 {
    int policy;
    int depth;
    int ncolors;
    int complexion;
    int weights[SIXEL_LOOKUP_FLOAT_COMPONENTS];
    float *palette;
    sixel_lookup_float32_node_t *kdnodes;
    int kdtree_root;
    int kdnodes_count;
    sixel_allocator_t *allocator;
} sixel_lookup_float32_t;

void
sixel_lookup_float32_init(sixel_lookup_float32_t *lut,
                          sixel_allocator_t *allocator);

void
sixel_lookup_float32_clear(sixel_lookup_float32_t *lut);

void
sixel_lookup_float32_finalize(sixel_lookup_float32_t *lut);

SIXELSTATUS
sixel_lookup_float32_configure(sixel_lookup_float32_t *lut,
                               unsigned char const *palette,
                               int depth,
                               int ncolors,
                               int complexion,
                               int wcomp1,
                               int wcomp2,
                               int wcomp3,
                               int policy,
                               int pixelformat);

int
sixel_lookup_float32_map_pixel(sixel_lookup_float32_t *lut,
                               unsigned char const *pixel);

#endif /* LIBSIXEL_LOOKUP_FLOAT32_H */

