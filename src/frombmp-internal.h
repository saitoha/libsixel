/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef LIBSIXEL_FROMBMP_INTERNAL_H
#define LIBSIXEL_FROMBMP_INTERNAL_H

#include <stddef.h>
#include <sixel.h>

#include "chunk.h"

#define SIXEL_BMP_MAX_PALETTE 256u

typedef struct sixel_bmp_decode_info {
    /*
     * Borrowed byte view validated by sixel_bmp_parse_header().
     * Decode helpers use this stable view instead of re-reading the chunk
     * vtbl, so analyzer paths match the parser's input validation.
     */
    unsigned char const *source_bytes;
    size_t source_size;
    int width;
    int height;
    int top_down;
    int bpp;
    int is_cmyk;
    int dib_family;
    unsigned int compression;
    unsigned int red_mask;
    unsigned int green_mask;
    unsigned int blue_mask;
    unsigned int alpha_mask;
    int has_alpha_mask;
    int has_explicit_alpha;
    int palette_count;
    unsigned char palette[SIXEL_BMP_MAX_PALETTE][4];
    size_t pixel_offset;
    size_t payload_size;
    size_t row_stride;
    unsigned char const *payload;
    unsigned char const *icc_profile;
    size_t icc_profile_length;
    int has_calibrated_rgb;
    double calibrated_gamma;
    double calibrated_gamma_r;
    double calibrated_gamma_g;
    double calibrated_gamma_b;
    double white_x;
    double white_y;
    double red_x;
    double red_y;
    double green_x;
    double green_y;
    double blue_x;
    double blue_y;
} sixel_bmp_decode_info_t;

SIXEL_INTERNAL_API SIXELSTATUS
sixel_bmp_parse_header(sixel_chunk_t const *chunk,
                       sixel_bmp_decode_info_t *info,
                       int info40_mode);

#endif /* LIBSIXEL_FROMBMP_INTERNAL_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
