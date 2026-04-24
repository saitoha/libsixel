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

#ifndef LIBSIXEL_LOOKUP_FLOAT32_H
#define LIBSIXEL_LOOKUP_FLOAT32_H

#include "allocator.h"
#include "lookup-fhedt-float32.h"
#include "lookup-vptree-float32.h"

#include <sixel.h>

#include <stddef.h>

enum { SIXEL_LOOKUP_FLOAT_COMPONENTS = 3 };

typedef struct sixel_lookup_float32_node sixel_lookup_float32_node_t;

typedef struct sixel_lookup_float32_1d_eytzinger {
    int count;
    float weights[SIXEL_LOOKUP_FLOAT_COMPONENTS];
    int window;
    float *keys;
    int *palette_index;
    int *rank;
    int *sorted_palette_index;
    float *sorted_keys;
    int ready;
} sixel_lookup_float32_1d_eytzinger_t;

typedef struct sixel_lookup_float32 {
    int policy;
    int depth;
    int ncolors;
    int complexion;
    float weights[SIXEL_LOOKUP_FLOAT_COMPONENTS];
    float *palette;
    sixel_lookup_float32_node_t *kdnodes;
    int kdtree_root;
    int kdnodes_count;
    sixel_allocator_t *allocator;
    sixel_lookup_fhedt_float32_t *fhedt;
    int fhedt_ready;
    sixel_lookup_vptree_float32_t *vptree;
    int vptree_ready;
    sixel_lookup_float32_1d_eytzinger_t eytz;
    struct {
        int pivot_count;
        int *pivots;
        float *radius;
        int *member_offset;
        int *member_index;
        float *mean;
        float *inv_cov;
        int ready;
    } rbc;
} sixel_lookup_float32_t;

void
sixel_lookup_float32_init(sixel_lookup_float32_t *lut,
                          sixel_allocator_t *allocator);

void
sixel_lookup_float32_clear(sixel_lookup_float32_t *lut);

void
sixel_lookup_float32_finalize(sixel_lookup_float32_t *lut);

SIXELSTATUS
sixel_lookup_float32_configure(sixel_lookup_float32_t *lut,
                               unsigned char const *palette,
                               float const *palette_float,
                               int depth,
                               int float_depth,
                               int ncolors,
                               int complexion,
                               int wcomp1,
                               int wcomp2,
                               int wcomp3,
                               int policy,
                               int pixelformat);

SIXELSTATUS
sixel_lookup_float32_configure_fhedt(sixel_lookup_float32_t *lut,
                                     unsigned char const *palette,
                                     float const *palette_float,
                                     int depth,
                                     int float_depth,
                                     int ncolors,
                                     int complexion,
                                     int wcomp1,
                                     int wcomp2,
                                     int wcomp3,
                                     int pixelformat);

SIXELSTATUS
sixel_lookup_float32_configure_vptree(sixel_lookup_float32_t *lut,
                                      unsigned char const *palette,
                                      float const *palette_float,
                                      int depth,
                                      int float_depth,
                                      int ncolors,
                                      int complexion,
                                      int wcomp1,
                                      int wcomp2,
                                      int wcomp3,
                                      int pixelformat);

SIXELSTATUS
sixel_lookup_float32_configure_rbc(sixel_lookup_float32_t *lut,
                                   unsigned char const *palette,
                                   float const *palette_float,
                                   int depth,
                                   int float_depth,
                                   int ncolors,
                                   int complexion,
                                   int wcomp1,
                                   int wcomp2,
                                   int wcomp3,
                                   int pixelformat);

SIXELSTATUS
sixel_lookup_float32_configure_mahalanobis(sixel_lookup_float32_t *lut,
                                           unsigned char const *palette,
                                           float const *palette_float,
                                           int depth,
                                           int float_depth,
                                           int ncolors,
                                           int complexion,
                                           int wcomp1,
                                           int wcomp2,
                                           int wcomp3,
                                           int pixelformat);

int
sixel_lookup_float32_map_pixel(sixel_lookup_float32_t *lut,
                               unsigned char const *pixel);

int
sixel_lookup_float32_map_pixel_fhedt(sixel_lookup_float32_t *lut,
                                     unsigned char const *pixel);

int
sixel_lookup_float32_map_pixel_vptree(sixel_lookup_float32_t *lut,
                                      unsigned char const *pixel);

int
sixel_lookup_float32_map_pixel_rbc(sixel_lookup_float32_t *lut,
                                   unsigned char const *pixel);

int
sixel_lookup_float32_map_pixel_mahalanobis(sixel_lookup_float32_t *lut,
                                           unsigned char const *pixel);

#endif /* LIBSIXEL_LOOKUP_FLOAT32_H */


/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
