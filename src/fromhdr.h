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
 * FROM, OUT OF, OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef LIBSIXEL_FROMHDR_H
#define LIBSIXEL_FROMHDR_H

#include <stddef.h>

#include <sixel.h>

#include "chunk.h"

typedef struct sixel_builtin_hdr_profile_hint {
    int has_format;
    int format_kind;
    int format_malformed;
    int has_gamma;
    double gamma;
    int gamma_malformed;
    int has_primaries;
    double white_x;
    double white_y;
    double red_x;
    double red_y;
    double green_x;
    double green_y;
    double blue_x;
    double blue_y;
    int primaries_malformed;
    int has_exposure;
    double exposure_scale;
    int exposure_malformed;
    int has_colorcorr;
    double colorcorr_r;
    double colorcorr_g;
    double colorcorr_b;
    int colorcorr_malformed;
    int has_pixaspect;
    double pixaspect;
    int pixaspect_malformed;
    int has_view;
    int view_malformed;
    int has_resolution;
    int orientation_axis1;
    int orientation_axis1_sign;
    int orientation_axis1_length;
    int orientation_axis2;
    int orientation_axis2_sign;
    int orientation_axis2_length;
    size_t pixel_data_offset;
    int width;
    int height;
    int resolution_malformed;
    int malformed;
} sixel_builtin_hdr_profile_hint_t;

#define SIXEL_BUILTIN_HDR_FORMAT_UNKNOWN 0
#define SIXEL_BUILTIN_HDR_FORMAT_RGBE    1
#define SIXEL_BUILTIN_HDR_FORMAT_XYZE    2

#define SIXEL_BUILTIN_HDR_AXIS_X         0
#define SIXEL_BUILTIN_HDR_AXIS_Y         1

SIXELSTATUS
sixel_builtin_decode_hdr_float32(
    sixel_chunk_t const *chunk,
    unsigned char **ppixels,
    int *pwidth,
    int *pheight,
    int *ppixelformat,
    int *pcolorspace);

SIXELSTATUS
sixel_builtin_parse_hdr_profile_hint(
    sixel_chunk_t const *chunk,
    sixel_builtin_hdr_profile_hint_t *out_hint);

SIXEL_INTERNAL_API void
sixel_builtin_hdr_apply_postprocess(
    unsigned char *pixels,
    int width,
    int height,
    int pixelformat,
    sixel_builtin_hdr_profile_hint_t const *hint,
    SIXELSTATUS hint_status,
    int enable_cms);

/* Decode HDR into frame storage and apply builtin HDR postprocess controls. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_builtin_load_hdr_frame(
    sixel_chunk_t const *chunk,
    sixel_frame_t *frame,
    int enable_cms);

#endif /* LIBSIXEL_FROMHDR_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 : */
/* EOF */
