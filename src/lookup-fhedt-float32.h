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
#ifndef LIBSIXEL_LOOKUP_FHEDT_FLOAT32_H
#define LIBSIXEL_LOOKUP_FHEDT_FLOAT32_H

#include "allocator.h"

#include <sixel.h>

#include <stddef.h>
#include <stdint.h>

#ifndef SIXEL_FHEDT_TLS
/* Select a thread-local qualifier only when the active language standard */
/* promises support; this avoids pedantic warnings under strict C99. */
# if defined(SIXEL_ENABLE_THREADS)
#  if defined(_MSC_VER)
#   define SIXEL_FHEDT_TLS __declspec(thread)
#   define SIXEL_FHEDT_TLS_AVAILABLE 1
#  elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#   if !defined(__STDC_NO_THREADS__) && !defined(__PCC__)
#    define SIXEL_FHEDT_TLS _Thread_local
#    define SIXEL_FHEDT_TLS_AVAILABLE 1
#   else
#    define SIXEL_FHEDT_TLS
#    define SIXEL_FHEDT_TLS_AVAILABLE 0
#   endif
#  elif defined(__GNUC__) && !defined(__PCC__)
#   define SIXEL_FHEDT_TLS __thread
#   define SIXEL_FHEDT_TLS_AVAILABLE 1
#  else
#   define SIXEL_FHEDT_TLS
#   define SIXEL_FHEDT_TLS_AVAILABLE 0
#  endif
# else
#  define SIXEL_FHEDT_TLS
#  define SIXEL_FHEDT_TLS_AVAILABLE 0
# endif
#endif

#ifndef SIXEL_LOOKUP_FHEDT_SHARED_FLOAT32_T
#define SIXEL_LOOKUP_FHEDT_SHARED_FLOAT32_T
typedef struct sixel_lookup_fhedt_shared_float32
        sixel_lookup_fhedt_shared_float32_t;
#endif

typedef struct sixel_lookup_fhedt_float32 {
    sixel_allocator_t *allocator;
    sixel_lookup_fhedt_shared_float32_t *shared;
    int shared_published;
    int use_cache;
    int parallel_dither_active;
} sixel_lookup_fhedt_float32_t;

SIXELSTATUS
sixel_lookup_fhedt_float32_create(sixel_allocator_t *allocator,
                                 sixel_lookup_fhedt_float32_t **fhedt_out);

void
sixel_lookup_fhedt_float32_unref(sixel_lookup_fhedt_float32_t *fhedt);

SIXELSTATUS
sixel_lookup_fhedt_float32_configure(sixel_lookup_fhedt_float32_t *fhedt,
                                 float const *palette,
                                 int ncolors,
                                 int resolution,
                                 int refine,
                                 int use_dist2,
                                 int use_cache,
                                 int shared,
                                 float wcomp1,
                                 float wcomp2,
                                 float wcomp3,
                                 int pixelformat,
                                 int parallel_dither_active);

int
sixel_lookup_fhedt_float32_map(sixel_lookup_fhedt_float32_t *fhedt,
                              float const *pixel);

uint32_t
sixel_lookup_fhedt_float32_signature(float const *palette,
                                    int ncolors,
                                    int resolution,
                                    int refine,
                                    float wcomp1,
                                    float wcomp2,
                                    float wcomp3,
                                    int depth,
                                    int pixelformat);

uint32_t
sixel_lookup_fhedt_float32_shared_signature(
    sixel_lookup_fhedt_shared_float32_t const *shared);

void
sixel_lookup_fhedt_float32_shared_set_signature(
    sixel_lookup_fhedt_shared_float32_t *shared,
    uint32_t signature);

#endif /* LIBSIXEL_LOOKUP_FHEDT_FLOAT32_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
