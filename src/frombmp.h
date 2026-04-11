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

#ifndef LIBSIXEL_FROMBMP_H
#define LIBSIXEL_FROMBMP_H

#include <stddef.h>
#include <sixel.h>

#include "chunk.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SIXEL_FROMBMP_COMPRESSION_RGB        0u
#define SIXEL_FROMBMP_COMPRESSION_RLE8       1u
#define SIXEL_FROMBMP_COMPRESSION_RLE4       2u
#define SIXEL_FROMBMP_COMPRESSION_BITFIELDS  3u
#define SIXEL_FROMBMP_COMPRESSION_JPEG       4u
#define SIXEL_FROMBMP_COMPRESSION_PNG        5u
#define SIXEL_FROMBMP_COMPRESSION_ALPHABITFIELDS 6u
#define SIXEL_FROMBMP_COMPRESSION_CMYK       11u
#define SIXEL_FROMBMP_COMPRESSION_CMYKRLE8   12u
#define SIXEL_FROMBMP_COMPRESSION_CMYKRLE4   13u
#define SIXEL_FROMBMP_COMPRESSION_OS2_HUFFMAN1D 14u
#define SIXEL_FROMBMP_COMPRESSION_OS2_RLE24  15u

#define SIXEL_FROMBMP_DIB_FAMILY_WINDOWS 1
#define SIXEL_FROMBMP_DIB_FAMILY_OS2     2

typedef struct sixel_frombmp_probe {
    int width;
    int height;
    int bpp;
    int is_cmyk;
    int dib_family;
    unsigned int compression;
    unsigned char const *payload;
    size_t payload_size;
    unsigned char const *icc_profile;
    size_t icc_profile_length;
    int has_calibrated_rgb;
    double calibrated_gamma;
    double white_x;
    double white_y;
    double red_x;
    double red_y;
    double green_x;
    double green_y;
    double blue_x;
    double blue_y;
} sixel_frombmp_probe_t;

/*
 * Decode BMP from an in-memory chunk.
 *
 * The decoder normalizes output to RGB888 or RGBA8888 and leaves alpha policy
 * (background composition vs transparent mask) to the caller.
 */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_frombmp_load(
    sixel_chunk_t const *chunk,
    unsigned char **ppixels,
    int *pwidth,
    int *pheight,
    int *pcomp,
    int *pis_cmyk,
    unsigned char const **picc_profile,
    size_t *picc_profile_length);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_frombmp_probe(
    sixel_chunk_t const *chunk,
    sixel_frombmp_probe_t *probe);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_FROMBMP_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
