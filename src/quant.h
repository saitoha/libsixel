/*
 * Copyright (c) 2014-2016 Hayaki Saito
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef LIBSIXEL_QUANT_H
#define LIBSIXEL_QUANT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <sixel.h>

/* choose colors using median-cut method */
SIXELSTATUS
sixel_quant_make_palette(
    unsigned char           /* out */ **result,
    unsigned const char     /* in */  *data,             /* data for sampling */
    unsigned int            /* in */  length,            /* data size */
    int                     /* in */  pixelformat,
    unsigned int            /* in */  reqcolors,
    unsigned int            /* in */  *ncolors,
    unsigned int            /* in */  *origcolors,
    int                     /* in */  methodForLargest,
    int                     /* in */  methodForRep,
    int                     /* in */  qualityMode,
    sixel_allocator_t       /* in */  *allocator);


/* apply color palette into specified pixel buffers */
SIXELSTATUS
sixel_quant_apply_palette(
    sixel_index_t       /* out */ *result,
    unsigned char       /* in */  *data,
    int                 /* in */  width,
    int                 /* in */  height,
    int                 /* in */  pixelformat,
    unsigned char       /* in */  *palette,
    int                 /* in */  reqcolor,
    int const           /* in */  methodForDiffuse,
    int const           /* in */  methodForScan,
    int const           /* in */  methodForCarry,
    int                 /* in */  foptimize,
    int                 /* in */  foptimize_palette,
    int                 /* in */  complexion,
    unsigned short      /* in */  *cachetable,
    int                 /* in */  *ncolor,
    sixel_allocator_t   /* in */  *allocator);

void
sixel_quant_set_lut_policy(
    int                 /* in */  lut_policy);

size_t
sixel_quant_fast_cache_size(void);

SIXELSTATUS
sixel_quant_cache_prepare(
    unsigned short      /* in,out */ **cachetable,
    size_t              /* in,out */ *cachetable_size,
    int                 /* in */     lut_policy,
    int                 /* in */     reqcolor,
    sixel_allocator_t   /* in */     *allocator);

void
sixel_quant_cache_clear(
    unsigned short      /* in,out */ *cachetable,
    int                 /* in */     lut_policy);

void
sixel_quant_cache_release(
    unsigned short      /* in,out */ *cachetable,
    int                 /* in */     lut_policy,
    sixel_allocator_t   /* in */     *allocator);


/* deallocate specified palette */
void
sixel_quant_free_palette(
    unsigned char       /* in */ *data,
    sixel_allocator_t   /* in */ *allocator);

#if HAVE_TESTS
SIXELAPI int
sixel_quant_tests_main(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_QUANT_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
