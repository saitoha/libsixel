/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See AUTHORS.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
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

#ifndef LIBSIXEL_FROMWEBP_VP8_PRIVATE_H
#define LIBSIXEL_FROMWEBP_VP8_PRIVATE_H

/* STDC_HEADERS */
#include <stddef.h>

#include "compat_stub.h"
#include "fromwebp-internal.h"

typedef struct sixel_webp_vp8_frame_header {
    int width;
    int height;
    int version;
    int show_frame;
    size_t first_partition_size;
} sixel_webp_vp8_frame_header_t;

SIXEL_INTERNAL_API SIXELSTATUS
sixel_webp_vp8_validate_dimensions(int width,
                                   int height);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_webp_vp8_parse_header(unsigned char const *payload,
                            size_t payload_size,
                            sixel_webp_vp8_frame_header_t *header);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_webp_vp8_decode_native_payload(unsigned char const *payload,
                                     size_t payload_size,
                                     sixel_webp_vp8_frame_header_t const
                                         *header,
                                     unsigned char *out_rgba,
                                     size_t out_rgba_size,
                                     unsigned char **prgba,
                                     int *pwidth,
                                     int *pheight,
                                     sixel_webp_vp8_workspace_t *workspace,
                                     sixel_allocator_t *allocator);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_webp_vp8_decode_native_payload_strided(
    unsigned char const *payload,
    size_t payload_size,
    sixel_webp_vp8_frame_header_t const *header,
    unsigned char *out_rgba,
    size_t out_rgba_size,
    size_t out_rgba_stride,
    int out_rgba_width,
    int out_rgba_height,
    unsigned char **prgba,
    int *pwidth,
    int *pheight,
    sixel_webp_vp8_workspace_t *workspace,
    sixel_allocator_t *allocator);

#endif  /* LIBSIXEL_FROMWEBP_VP8_PRIVATE_H */


/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
