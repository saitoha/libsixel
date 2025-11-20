/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
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
    if (lut->lookup_8bit->cert == NULL) {
        /*
         * CERT LUT requires its own workspace.  If allocation failed,
         * bail out early to avoid later null dereferences during
         * configuration.
         */
        sixel_lookup_8bit_finalize(lut->lookup_8bit);
        free(lut->lookup_8bit);
        free(lut->lookup_float32);
        free(lut);
        return SIXEL_BAD_ALLOCATION;
    }
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
                    int wcomp1,
                    int wcomp2,
                    int wcomp3,
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
                                              wcomp1,
                                              wcomp2,
                                              wcomp3,
                                              policy,
                                              pixelformat);
    }

    return sixel_lookup_8bit_configure(lut->lookup_8bit,
                                       palette,
                                       depth,
                                       ncolors,
                                       complexion,
                                       wcomp1,
                                       wcomp2,
                                       wcomp3,
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

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
