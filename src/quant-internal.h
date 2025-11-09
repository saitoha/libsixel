/*
 * SPDX-License-Identifier: MIT
 *
 * Shared quantization helpers between palette and quant modules.
 */

#ifndef LIBSIXEL_QUANT_INTERNAL_H
#define LIBSIXEL_QUANT_INTERNAL_H

#include "config.h"

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

#include <sixel.h>

#if HAVE_DEBUG
#define quant_trace fprintf
#else
static inline void quant_trace(FILE *f, ...)
{
    (void)f;
}
#endif

struct histogram_control {
    unsigned int channel_shift;
    unsigned int channel_bits;
    unsigned int channel_mask;
    int reversible_rounding;
};

typedef unsigned long sample;
typedef sample *tuple;

struct tupleint {
    unsigned int value;
    sample tuple[1];
};

typedef struct tupleint **tupletable;

typedef struct {
    unsigned int size;
    tupletable table;
} tupletable2;

struct histogram_control
histogram_control_make(unsigned int depth);

struct histogram_control
histogram_control_make_for_policy(unsigned int depth, int lut_policy);

size_t
histogram_dense_size(unsigned int depth,
                     struct histogram_control const *control);

unsigned int
histogram_reconstruct(unsigned int quantized,
                      struct histogram_control const *control);

unsigned int
histogram_pack_color(unsigned char const *data,
                     unsigned int depth,
                     struct histogram_control const *control);

uint32_t
histogram_hash_mix(uint32_t key);

void
sixel_quant_reversible_palette(unsigned char *palette,
                               unsigned int colors,
                               unsigned int depth);

SIXELSTATUS
build_palette_kmeans(unsigned char **result,
                     unsigned char const *data,
                     unsigned int length,
                     unsigned int depth,
                     unsigned int reqcolors,
                     unsigned int *ncolors,
                     unsigned int *origcolors,
                     int qualityMode,
                     int force_palette,
                     int final_merge_mode,
                     sixel_allocator_t *allocator);

int
computeColorMapFromInput(unsigned char const *data,
                         unsigned int length,
                         unsigned int depth,
                         unsigned int reqColors,
                         int methodForLargest,
                         int methodForRep,
                         int qualityMode,
                         int force_palette,
                         int use_reversible,
                         int final_merge_mode,
                         tupletable2 *colormapP,
                         unsigned int *origcolors,
                         sixel_allocator_t *allocator);

#endif /* LIBSIXEL_QUANT_INTERNAL_H */
