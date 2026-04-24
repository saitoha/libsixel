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

#ifndef LIBSIXEL_LOOKUP_8BIT_H
#define LIBSIXEL_LOOKUP_8BIT_H

#include "allocator.h"
#include "lookup-fhedt-8bit.h"
#include "lookup-vptree-8bit.h"

#include <sixel.h>

#include <stddef.h>
#include <stdint.h>

typedef struct sixel_certlut sixel_certlut_t;

typedef struct sixel_lookup_8bit_quantization {
    unsigned int channel_shift;
    unsigned int channel_bits;
    unsigned int channel_mask;
} sixel_lookup_8bit_quantization_t;

typedef struct sixel_lookup_8bit_1d_eytzinger {
    int count;
    int weights[3];
    int window;
    float *keys;
    int *palette_index;
    int *rank;
    int *sorted_palette_index;
    float *sorted_keys;
    int ready;
} sixel_lookup_8bit_1d_eytzinger_t;

typedef struct sixel_lookup_8bit {
    int policy;
    int depth;
    int ncolors;
    int complexion;
    int packing;
    unsigned char const *palette;
    sixel_allocator_t *allocator;
    sixel_lookup_8bit_quantization_t quant;
    int32_t *dense;
    size_t dense_size;
    int dense_ready;
    sixel_certlut_t *cert;
    int cert_ready;
    sixel_lookup_fhedt_8bit_t *fhedt;
    int fhedt_ready;
    sixel_lookup_vptree_8bit_t *vptree;
    int vptree_ready;
    sixel_lookup_8bit_1d_eytzinger_t eytz;
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
                            int wcomp1,
                            int wcomp2,
                            int wcomp3,
                            int policy,
                            int pixelformat);

SIXELSTATUS
sixel_lookup_8bit_configure_5bit(sixel_lookup_8bit_t *lut,
                                 unsigned char const *palette,
                                 int depth,
                                 int ncolors,
                                 int complexion,
                                 int wcomp1,
                                 int wcomp2,
                                 int wcomp3,
                                 int pixelformat);

SIXELSTATUS
sixel_lookup_8bit_configure_6bit(sixel_lookup_8bit_t *lut,
                                 unsigned char const *palette,
                                 int depth,
                                 int ncolors,
                                 int complexion,
                                 int wcomp1,
                                 int wcomp2,
                                 int wcomp3,
                                 int pixelformat);

SIXELSTATUS
sixel_lookup_8bit_configure_certlut(sixel_lookup_8bit_t *lut,
                                    unsigned char const *palette,
                                    int depth,
                                    int ncolors,
                                    int complexion,
                                    int wcomp1,
                                    int wcomp2,
                                    int wcomp3,
                                    int pixelformat);

SIXELSTATUS
sixel_lookup_8bit_configure_eytzinger(sixel_lookup_8bit_t *lut,
                                      unsigned char const *palette,
                                      int depth,
                                      int ncolors,
                                      int complexion,
                                      int wcomp1,
                                      int wcomp2,
                                      int wcomp3,
                                      int pixelformat);

SIXELSTATUS
sixel_lookup_8bit_configure_fhedt(sixel_lookup_8bit_t *lut,
                                  unsigned char const *palette,
                                  int depth,
                                  int ncolors,
                                  int complexion,
                                  int wcomp1,
                                  int wcomp2,
                                  int wcomp3,
                                  int pixelformat);

SIXELSTATUS
sixel_lookup_8bit_configure_vptree(sixel_lookup_8bit_t *lut,
                                   unsigned char const *palette,
                                   int depth,
                                   int ncolors,
                                   int complexion,
                                   int wcomp1,
                                   int wcomp2,
                                   int wcomp3,
                                   int pixelformat);

SIXELSTATUS
sixel_lookup_8bit_configure_rbc(sixel_lookup_8bit_t *lut,
                                unsigned char const *palette,
                                int depth,
                                int ncolors,
                                int complexion,
                                int wcomp1,
                                int wcomp2,
                                int wcomp3,
                                int pixelformat);

SIXELSTATUS
sixel_lookup_8bit_configure_mahalanobis(sixel_lookup_8bit_t *lut,
                                        unsigned char const *palette,
                                        int depth,
                                        int ncolors,
                                        int complexion,
                                        int wcomp1,
                                        int wcomp2,
                                        int wcomp3,
                                        int pixelformat);

int
sixel_lookup_8bit_map_pixel(sixel_lookup_8bit_t *lut,
                            unsigned char const *pixel);

int
sixel_lookup_8bit_map_pixel_5bit(sixel_lookup_8bit_t *lut,
                                 unsigned char const *pixel);

int
sixel_lookup_8bit_map_pixel_6bit(sixel_lookup_8bit_t *lut,
                                 unsigned char const *pixel);

int
sixel_lookup_8bit_map_pixel_certlut(sixel_lookup_8bit_t *lut,
                                    unsigned char const *pixel);

int
sixel_lookup_8bit_map_pixel_eytzinger(sixel_lookup_8bit_t *lut,
                                      unsigned char const *pixel);

int
sixel_lookup_8bit_map_pixel_fhedt(sixel_lookup_8bit_t *lut,
                                  unsigned char const *pixel);

int
sixel_lookup_8bit_map_pixel_vptree(sixel_lookup_8bit_t *lut,
                                   unsigned char const *pixel);

int
sixel_lookup_8bit_map_pixel_rbc(sixel_lookup_8bit_t *lut,
                                unsigned char const *pixel);

int
sixel_lookup_8bit_map_pixel_mahalanobis(sixel_lookup_8bit_t *lut,
                                        unsigned char const *pixel);

#endif /* LIBSIXEL_LOOKUP_8BIT_H */


/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
