/*
 * SPDX-License-Identifier: MIT
 */

#ifndef LIBSIXEL_LOOKUP_8BIT_H
#define LIBSIXEL_LOOKUP_8BIT_H

#include "allocator.h"

#include <sixel.h>

#include <stddef.h>
#include <stdint.h>

typedef struct sixel_certlut sixel_certlut_t;

typedef struct sixel_lookup_8bit_quantization {
    unsigned int channel_shift;
    unsigned int channel_bits;
    unsigned int channel_mask;
} sixel_lookup_8bit_quantization_t;

typedef struct sixel_lookup_8bit {
    int policy;
    int depth;
    int ncolors;
    int complexion;
    unsigned char const *palette;
    sixel_allocator_t *allocator;
    sixel_lookup_8bit_quantization_t quant;
    int32_t *dense;
    size_t dense_size;
    int dense_ready;
    sixel_certlut_t *cert;
    int cert_ready;
} sixel_lookup_8bit_t;

void
sixel_lookup_8bit_init(sixel_lookup_8bit_t *lut,
                       sixel_allocator_t *allocator);

void
sixel_lookup_8bit_clear(sixel_lookup_8bit_t *lut);

void
sixel_lookup_8bit_finalize(sixel_lookup_8bit_t *lut);

SIXELSTATUS
sixel_lookup_8bit_configure(sixel_lookup_8bit_t *lut,
                            unsigned char const *palette,
                            int depth,
                            int ncolors,
                            int complexion,
                            int wR,
                            int wG,
                            int wB,
                            int policy,
                            int pixelformat);

int
sixel_lookup_8bit_map_pixel(sixel_lookup_8bit_t *lut,
                            unsigned char const *pixel);

#endif /* LIBSIXEL_LOOKUP_8BIT_H */

