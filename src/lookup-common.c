/*
 * SPDX-License-Identifier: MIT
 */

/*
 * Lookup dispatcher that selects either the 8bit or float32 backend.
 * Backend implementations live in lookup-8bit.c and lookup-float32.c so
 * additional search structures can be added without touching callers.
 */

#include "config.h"

#include <stdlib.h>

#include <sixel.h>

#include "allocator.h"
#include "lookup-8bit.h"
#include "lookup-float32.h"
#include "lookup-common.h"

struct sixel_lut {
    int input_is_float;
    sixel_allocator_t *allocator;
    sixel_lookup_8bit_t *lookup_8bit;
    sixel_lookup_float32_t *lookup_float32;
};

SIXELSTATUS
sixel_lut_new(sixel_lut_t **out,
              int policy,
              sixel_allocator_t *allocator)
{
    sixel_lut_t *lut;

    if (out == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    lut = (sixel_lut_t *)malloc(sizeof(sixel_lut_t));
    if (lut == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    lut->input_is_float = 0;
    lut->allocator = allocator;
    lut->lookup_8bit = (sixel_lookup_8bit_t *)
        malloc(sizeof(sixel_lookup_8bit_t));
    lut->lookup_float32 = (sixel_lookup_float32_t *)
        malloc(sizeof(sixel_lookup_float32_t));
    if (lut->lookup_8bit == NULL || lut->lookup_float32 == NULL) {
        free(lut->lookup_8bit);
        free(lut->lookup_float32);
        free(lut);
        return SIXEL_BAD_ALLOCATION;
    }
    sixel_lookup_8bit_init(lut->lookup_8bit, allocator);
    sixel_lookup_float32_init(lut->lookup_float32, allocator);

    *out = lut;

    /* policy is normalized inside backend configure functions */
    (void)policy;

    return SIXEL_OK;
}

SIXELSTATUS
sixel_lut_configure(sixel_lut_t *lut,
                    unsigned char const *palette,
                    int depth,
                    int ncolors,
                    int complexion,
                    int wR,
                    int wG,
                    int wB,
                    int policy,
                    int pixelformat)
{
    if (lut == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    lut->input_is_float = SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat);
    if (lut->input_is_float) {
        return sixel_lookup_float32_configure(lut->lookup_float32,
                                              palette,
                                              depth,
                                              ncolors,
                                              complexion,
                                              wR,
                                              wG,
                                              wB,
                                              policy,
                                              pixelformat);
    }

    return sixel_lookup_8bit_configure(lut->lookup_8bit,
                                       palette,
                                       depth,
                                       ncolors,
                                       complexion,
                                       wR,
                                       wG,
                                       wB,
                                       policy,
                                       pixelformat);
}

int
sixel_lut_map_pixel(sixel_lut_t *lut, unsigned char const *pixel)
{
    if (lut == NULL) {
        return 0;
    }

    if (lut->input_is_float) {
        return sixel_lookup_float32_map_pixel(lut->lookup_float32, pixel);
    }

    return sixel_lookup_8bit_map_pixel(lut->lookup_8bit, pixel);
}

void
sixel_lut_clear(sixel_lut_t *lut)
{
    if (lut == NULL) {
        return;
    }

    sixel_lookup_8bit_clear(lut->lookup_8bit);
    sixel_lookup_float32_clear(lut->lookup_float32);
    lut->input_is_float = 0;
}

void
sixel_lut_unref(sixel_lut_t *lut)
{
    if (lut == NULL) {
        return;
    }

    sixel_lookup_8bit_finalize(lut->lookup_8bit);
    sixel_lookup_float32_finalize(lut->lookup_float32);
    free(lut->lookup_8bit);
    free(lut->lookup_float32);
    free(lut);
}

