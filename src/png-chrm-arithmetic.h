/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
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
#ifndef LIBSIXEL_PNG_CHRM_ARITHMETIC_H
#define LIBSIXEL_PNG_CHRM_ARITHMETIC_H

#include <sixel.h>
#include <stddef.h>
#include <stdint.h>

/* Internal PNG-only arithmetic. This is not a general multiprecision API. */
#define SIXEL_PNG_CHRM_AUTO 0
#define SIXEL_PNG_CHRM_DOUBLE 1
#define SIXEL_PNG_CHRM_512BIT 2

#define SIXEL_PNG_CHRM_OK 1
#define SIXEL_PNG_CHRM_METADATA_SKIP 0
#define SIXEL_PNG_CHRM_UNSUPPORTED (-1)
#define SIXEL_PNG_CHRM_INVALID_PIXELS (-2)
#define SIXEL_PNG_CHRM_INVALID_ARGUMENT (-3)

#define SIXEL_PNG_CHRM_FORCED 1u
#define SIXEL_PNG_CHRM_SMALL_AREA 2u
#define SIXEL_PNG_CHRM_SMALL_CONE 4u
#define SIXEL_PNG_CHRM_ORDINARY 8u

typedef struct {
    uint32_t w[16];
    int sign;
} sixel_png_chrm_integer_t;

typedef struct {
    int64_t grid[8];
    int cat;
    int requested;
    int selected;
    unsigned reason;
    int exact_ready;
    int identity;
    int gray;
    sixel_png_chrm_integer_t n[9];
    sixel_png_chrm_integer_t d;
    double m[9];
} sixel_png_chrm_matrix_t;

typedef struct {
    size_t identity_pixels;
    size_t double_pixels;
    size_t integer_pixels;
} sixel_png_chrm_stats_t;

/* Build transactionally from validated integer-grid metadata. `adapt` chooses
 * Bradford to D65; source-only use remains available for paired controls.
 * Undefined geometry returns METADATA_SKIP without changing `out`.
 */
SIXEL_INTERNAL_API int sixel_png_chrm_build(sixel_png_chrm_matrix_t *out,
                                            int64_t const grid[8],
                                            int requested, int adapt);

/* Input is the existing TRC output, binary32 RGB in [0,1]. Exact aliasing is
 * supported. Invalid input is rejected before any output or stats is changed.
 * Only 512bit evaluation guarantees direct rational ties-even rounding.
 * That guarantee starts at these input float bits: it does not extend to
 * the preceding nonlinear TRC, later composition or other CMS engines.
 * Ordinary double assumes the normal nearest-even floating environment;
 * auto only considers matrix denominators, not pixel cancellation.
 */
SIXEL_INTERNAL_API int sixel_png_chrm_apply(sixel_png_chrm_matrix_t const *m,
                                            float const *input, float *output,
                                            size_t count,
                                            sixel_png_chrm_stats_t *stats);

#endif

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
