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
 * Voronoi + 3D EDT lookup implementation for 8bit pixel buffers.
 */
#ifndef LIBSIXEL_LOOKUP_VPTE_8BIT_H
#define LIBSIXEL_LOOKUP_VPTE_8BIT_H

#include "allocator.h"

#include <sixel.h>

#include <stddef.h>
#include <stdint.h>

#ifndef SIXEL_VPTE_TLS
/* Select a thread-local qualifier only when the active language standard */
/* promises support; this avoids pedantic warnings under strict C99. */
# if defined(SIXEL_ENABLE_THREADS)
#  if defined(_MSC_VER)
#   define SIXEL_VPTE_TLS __declspec(thread)
#   define SIXEL_VPTE_TLS_AVAILABLE 1
#  elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#   if !defined(__STDC_NO_THREADS__)
#    define SIXEL_VPTE_TLS _Thread_local
#    define SIXEL_VPTE_TLS_AVAILABLE 1
#   else
#    define SIXEL_VPTE_TLS
#    define SIXEL_VPTE_TLS_AVAILABLE 0
#   endif
#  elif defined(__GNUC__)
#   define SIXEL_VPTE_TLS __thread
#   define SIXEL_VPTE_TLS_AVAILABLE 1
#  else
#   define SIXEL_VPTE_TLS
#   define SIXEL_VPTE_TLS_AVAILABLE 0
#  endif
# else
#  define SIXEL_VPTE_TLS
#  define SIXEL_VPTE_TLS_AVAILABLE 0
# endif
#endif

#ifndef SIXEL_LOOKUP_VPTE_SHARED_8BIT_T
#define SIXEL_LOOKUP_VPTE_SHARED_8BIT_T
typedef struct sixel_lookup_vpte_shared_8bit sixel_lookup_vpte_shared_8bit_t;
#endif

typedef struct sixel_lookup_vpte_8bit {
    sixel_allocator_t *allocator;
    sixel_lookup_vpte_shared_8bit_t *shared;
    int shared_published;
    int use_cache;
} sixel_lookup_vpte_8bit_t;

SIXELSTATUS
sixel_lookup_vpte_8bit_create(sixel_allocator_t *allocator,
                              sixel_lookup_vpte_8bit_t **vpte_out);

void
sixel_lookup_vpte_8bit_unref(sixel_lookup_vpte_8bit_t *vpte);

SIXELSTATUS
sixel_lookup_vpte_8bit_configure(sixel_lookup_vpte_8bit_t *vpte,
                                 unsigned char const *palette,
                                 int ncolors,
                                 int resolution,
                                 int refine,
                                 int use_dist2,
                                 int use_cache,
                                 int shared,
                                 int wcomp1,
                                 int wcomp2,
                                 int wcomp3,
                                 int pixelformat,
                                 int depth);

int
sixel_lookup_vpte_8bit_map(sixel_lookup_vpte_8bit_t *vpte,
                           unsigned char const *pixel);

uint32_t
sixel_lookup_vpte_8bit_signature(unsigned char const *palette,
                                 int ncolors,
                                 int resolution,
                                 int refine,
                                 int wcomp1,
                                 int wcomp2,
                                 int wcomp3,
                                 int depth);

uint32_t
sixel_lookup_vpte_8bit_shared_signature(
    sixel_lookup_vpte_shared_8bit_t const *shared);

void
sixel_lookup_vpte_8bit_shared_set_signature(
    sixel_lookup_vpte_shared_8bit_t *shared,
    uint32_t signature);

#endif /* LIBSIXEL_LOOKUP_VPTE_8BIT_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
