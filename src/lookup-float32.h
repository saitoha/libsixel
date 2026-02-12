/*
 * SPDX-License-Identifier: MIT
 */

#ifndef LIBSIXEL_LOOKUP_FLOAT32_H
#define LIBSIXEL_LOOKUP_FLOAT32_H

#include "allocator.h"
#include "lookup-vpte-float32.h"
#include "lookup-vptree-float32.h"

#include <sixel.h>

#include <stddef.h>

enum { SIXEL_LOOKUP_FLOAT_COMPONENTS = 3 };

typedef struct sixel_lookup_float32_node sixel_lookup_float32_node_t;

typedef struct sixel_lookup_float32_1d_eytzinger {
    int count;
    float weights[SIXEL_LOOKUP_FLOAT_COMPONENTS];
    int window;
    float *keys;
    int *palette_index;
    int *rank;
    int *sorted_palette_index;
    float *sorted_keys;
    int ready;
} sixel_lookup_float32_1d_eytzinger_t;

typedef struct sixel_lookup_float32 {
    int policy;
    int depth;
    int ncolors;
    int complexion;
    float weights[SIXEL_LOOKUP_FLOAT_COMPONENTS];
    float *palette;
    sixel_lookup_float32_node_t *kdnodes;
    int kdtree_root;
    int kdnodes_count;
    sixel_allocator_t *allocator;
    sixel_lookup_vpte_float32_t *vpte;
    int vpte_ready;
    sixel_lookup_vptree_float32_t *vptree;
    int vptree_ready;
    sixel_lookup_float32_1d_eytzinger_t eytz;
    struct {
        int pivot_count;
        int *pivots;
        float *radius;
        int *member_offset;
        int *member_index;
        float *mean;
        float *inv_cov;
        int ready;
    } rbc;
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
                               float const *palette_float,
                               int depth,
                               int float_depth,
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
