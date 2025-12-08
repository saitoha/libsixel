/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Voronoi + 3D EDT lookup implementation for float32 pixel buffers.
 */
#ifndef LIBSIXEL_LOOKUP_VPTE_FLOAT32_H
#define LIBSIXEL_LOOKUP_VPTE_FLOAT32_H

#include "allocator.h"

#include <sixel.h>

#include <stddef.h>
#include <stdint.h>

#ifndef SIXEL_LOOKUP_VPTE_SHARED_T
#define SIXEL_LOOKUP_VPTE_SHARED_T
typedef struct sixel_lookup_vpte_shared sixel_lookup_vpte_shared_t;
#endif

typedef struct sixel_lookup_vpte_float32 {
    sixel_allocator_t *allocator;
    sixel_lookup_vpte_shared_t *shared;
    int use_cache;
} sixel_lookup_vpte_float32_t;

SIXELSTATUS
sixel_lookup_vpte_float32_create(sixel_allocator_t *allocator,
                                 sixel_lookup_vpte_float32_t **vpte_out);

void
sixel_lookup_vpte_float32_unref(sixel_lookup_vpte_float32_t *vpte);

SIXELSTATUS
sixel_lookup_vpte_float32_configure(sixel_lookup_vpte_float32_t *vpte,
                                 float const *palette,
                                 int ncolors,
                                 int resolution,
                                 int refine,
                                 int use_dist2,
                                 int use_cache,
                                 int shared,
                                 int wcomp1,
                                 int wcomp2,
                                 int wcomp3,
                                    int pixelformat);

int
sixel_lookup_vpte_float32_map(sixel_lookup_vpte_float32_t *vpte,
                              float const *pixel);

uint32_t
sixel_lookup_vpte_float32_signature(float const *palette,
                                    int ncolors,
                                    int resolution,
                                    int refine,
                                    int wcomp1,
                                    int wcomp2,
                                    int wcomp3,
                                    int depth);

uint32_t
sixel_lookup_vpte_float32_shared_signature(
    sixel_lookup_vpte_shared_t const *shared);

void
sixel_lookup_vpte_float32_shared_set_signature(
    sixel_lookup_vpte_shared_t *shared,
    uint32_t signature);

#endif /* LIBSIXEL_LOOKUP_VPTE_FLOAT32_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
