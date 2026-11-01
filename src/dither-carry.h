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
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef LIBSIXEL_DITHER_CARRY_H
#define LIBSIXEL_DITHER_CARRY_H

#include <stdint.h>
#include "dither-policy-private.h"

typedef struct sixel_dither_carry_tap {
    int dx;
    int dy;
    int weight;
} sixel_dither_carry_tap_t;

typedef struct sixel_dither_carry_kernel {
    int denominator;
    int rows;
    int count;
    sixel_dither_carry_tap_t taps[12];
} sixel_dither_carry_kernel_t;

SIXEL_INTERNAL_API sixel_dither_carry_kernel_t const *
sixel_dither_carry_kernel(int method);

/* Produce an unclipped rounded candidate and its byte lookup projection. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_dither_carry_candidate(unsigned char const *source,
                            int32_t const *carry, int depth, int denominator,
                            int policy, double threshold, double deadzone,
                            int32_t *candidate, unsigned char *lookup);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_dither_carry_apply(sixel_dither_policy_effective_t const *request,
                        int method);

#endif /* LIBSIXEL_DITHER_CARRY_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
