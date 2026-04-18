/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2021-2025 libsixel developers. See `AUTHORS`.
 * Copyright (c) 2014-2018 Hayaki Saito
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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>

#if HAVE_MATH_H
# include <math.h>
#endif  /* HAVE_MATH_H */
#if HAVE_STRING_H
# include <string.h>
#endif  /* HAVE_STRING_H */
#if HAVE_CTYPE_H
# include <ctype.h>
#endif  /* HAVE_CTYPE_H */
#if HAVE_LIMITS_H
# include <limits.h>
#endif  /* HAVE_LIMITS_H */
#if HAVE_INTTYPES_H
# include <inttypes.h>
#endif  /* HAVE_INTTYPES_H */

#include "dither.h"
#include "dither-interframe-method.h"
#include "palette.h"
#include "compat_stub.h"
#include "lookup-common.h"
#include "timer.h"
#include "dither-common-pipeline.h"
#include "dither-positional-8bit.h"
#include "dither-positional-float32.h"
#include "dither-fixed-8bit.h"
#include "dither-fixed-float32.h"
#include "dither-varcoeff-8bit.h"
#include "dither-varcoeff-float32.h"
#include "dither-internal.h"
#include "filter-lookup.h"
#include "logger.h"
#include "pixelformat.h"
#include "sixel_atomic.h"
#if SIXEL_ENABLE_THREADS
# include "threadpool.h"
#endif
#include <sixel.h>

static SIXELSTATUS
sixel_dither_preblend_alpha_inplace(unsigned char *pixels,
                                    size_t total_pixels,
                                    int pixelformat,
                                    unsigned char const *background);

static SIXELSTATUS
sixel_dither_composite_alpha_to_rgb(unsigned char *dst,
                                    unsigned char const *src,
                                    size_t total_pixels,
                                    int pixelformat,
                                    unsigned char const *background);

static void
sixel_dither_interframe_state_init(sixel_dither_t *dither);

static void
sixel_dither_interframe_state_reset(sixel_dither_t *dither);

static void
sixel_dither_interframe_state_dispose(sixel_dither_t *dither);

static int
sixel_dither_interframe_resolve_apply_mode(int apply_mode);

static void
sixel_dither_interframe_begin_apply(sixel_dither_t *dither,
                                  int apply_mode);

static void
sixel_dither_interframe_finish_apply(sixel_dither_t *dither,
                                   int apply_mode,
                                   SIXELSTATUS status);

static void
sixel_dither_cleanup_apply_hints(sixel_dither_t *dither);

#if SIXEL_ENABLE_THREADS
static int
sixel_dither_logger_set_frame_context(sixel_logger_t *logger,
                                      sixel_dither_t const *dither);
#endif  /* SIXEL_ENABLE_THREADS */


/*
 * Promote an RGB888 buffer to RGBFLOAT32 by normalising each channel to the
 * 0.0-1.0 range.  The helper is used when a float32 pipeline is requested via
 * the pixelformat but the incoming frame still relies on 8bit pixels.
 */
static SIXELSTATUS
sixel_dither_promote_rgb888_to_float32(float **out_pixels,
                                       unsigned char const *rgb888,
                                       size_t pixel_total,
                                       sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    float *buffer;
    size_t bytes;
    size_t index;
    size_t base;

    status = SIXEL_BAD_ARGUMENT;
    buffer = NULL;
    bytes = 0U;
    index = 0U;
    base = 0U;

    if (out_pixels == NULL || rgb888 == NULL || allocator == NULL) {
        return status;
    }

    *out_pixels = NULL;
    status = SIXEL_OK;
    if (pixel_total == 0U) {
        return status;
    }

    if (pixel_total > SIZE_MAX / (3U * sizeof(float))) {
        return SIXEL_BAD_INPUT;
    }
    bytes = pixel_total * 3U * sizeof(float);

    buffer = (float *)sixel_allocator_malloc(allocator, bytes);
    if (buffer == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    for (index = 0U; index < pixel_total; ++index) {
        base = index * 3U;
        buffer[base + 0U] = (float)rgb888[base + 0U] / 255.0f;
        buffer[base + 1U] = (float)rgb888[base + 1U] / 255.0f;
        buffer[base + 2U] = (float)rgb888[base + 2U] / 255.0f;
    }

    *out_pixels = buffer;
    return status;
}

#if SIXEL_ENABLE_THREADS
static int
sixel_dither_logger_set_frame_context(sixel_logger_t *logger,
                                      sixel_dither_t const *dither)
{
    if (logger == NULL || dither == NULL) {
        return 0;
    }
    if (dither->frame_context.valid == 0) {
        return 0;
    }
    sixel_logger_set_frame_context(logger,
                                   dither->frame_context.frame_no,
                                   dither->frame_context.loop_no,
                                   dither->frame_context.multiframe);
    return 1;
}
#endif  /* SIXEL_ENABLE_THREADS */

/*
 * Determine whether the selected diffusion method can reuse the float32
 * pipeline. Positional, variable-coefficient, fixed-kernel, and interframe
 * methods all have float-capable backends, so only the diffusion token
 * controls eligibility.
 */
static int
sixel_dither_method_supports_float_pipeline(sixel_dither_t const *dither)
{
    int method;

    if (dither == NULL) {
        return 0;
    }
    if (dither->prefer_float32 == 0) {
        return 0;
    }
    method = dither->method_for_diffuse;
    switch (method) {
    case SIXEL_DIFFUSE_NONE:
    case SIXEL_DIFFUSE_ATKINSON:
    case SIXEL_DIFFUSE_FS:
    case SIXEL_DIFFUSE_JAJUNI:
    case SIXEL_DIFFUSE_STUCKI:
    case SIXEL_DIFFUSE_BURKES:
    case SIXEL_DIFFUSE_SIERRA1:
    case SIXEL_DIFFUSE_SIERRA2:
    case SIXEL_DIFFUSE_SIERRA3:
        return 1;
    case SIXEL_DIFFUSE_A_DITHER:
    case SIXEL_DIFFUSE_X_DITHER:
    case SIXEL_DIFFUSE_BLUENOISE_DITHER:
    case SIXEL_DIFFUSE_LSO2:
        return 1;
    case SIXEL_DIFFUSE_INTERFRAME:
        return 1;
    default:
        return 0;
    }
}


static const unsigned char pal_mono_dark[] = {
    0x00, 0x00, 0x00, 0xff, 0xff, 0xff
};


static const unsigned char pal_mono_light[] = {
    0xff, 0xff, 0xff, 0x00, 0x00, 0x00
};

static const unsigned char pal_gray_1bit[] = {
    0x00, 0x00, 0x00, 0xff, 0xff, 0xff
};


static const unsigned char pal_gray_2bit[] = {
    0x00, 0x00, 0x00, 0x55, 0x55, 0x55, 0xaa, 0xaa, 0xaa, 0xff, 0xff, 0xff
};


static const unsigned char pal_gray_4bit[] = {
    0x00, 0x00, 0x00, 0x11, 0x11, 0x11, 0x22, 0x22, 0x22, 0x33, 0x33, 0x33,
    0x44, 0x44, 0x44, 0x55, 0x55, 0x55, 0x66, 0x66, 0x66, 0x77, 0x77, 0x77,
    0x88, 0x88, 0x88, 0x99, 0x99, 0x99, 0xaa, 0xaa, 0xaa, 0xbb, 0xbb, 0xbb,
    0xcc, 0xcc, 0xcc, 0xdd, 0xdd, 0xdd, 0xee, 0xee, 0xee, 0xff, 0xff, 0xff
};


static const unsigned char pal_gray_8bit[] = {
    0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x02, 0x02, 0x02, 0x03, 0x03, 0x03,
    0x04, 0x04, 0x04, 0x05, 0x05, 0x05, 0x06, 0x06, 0x06, 0x07, 0x07, 0x07,
    0x08, 0x08, 0x08, 0x09, 0x09, 0x09, 0x0a, 0x0a, 0x0a, 0x0b, 0x0b, 0x0b,
    0x0c, 0x0c, 0x0c, 0x0d, 0x0d, 0x0d, 0x0e, 0x0e, 0x0e, 0x0f, 0x0f, 0x0f,
    0x10, 0x10, 0x10, 0x11, 0x11, 0x11, 0x12, 0x12, 0x12, 0x13, 0x13, 0x13,
    0x14, 0x14, 0x14, 0x15, 0x15, 0x15, 0x16, 0x16, 0x16, 0x17, 0x17, 0x17,
    0x18, 0x18, 0x18, 0x19, 0x19, 0x19, 0x1a, 0x1a, 0x1a, 0x1b, 0x1b, 0x1b,
    0x1c, 0x1c, 0x1c, 0x1d, 0x1d, 0x1d, 0x1e, 0x1e, 0x1e, 0x1f, 0x1f, 0x1f,
    0x20, 0x20, 0x20, 0x21, 0x21, 0x21, 0x22, 0x22, 0x22, 0x23, 0x23, 0x23,
    0x24, 0x24, 0x24, 0x25, 0x25, 0x25, 0x26, 0x26, 0x26, 0x27, 0x27, 0x27,
    0x28, 0x28, 0x28, 0x29, 0x29, 0x29, 0x2a, 0x2a, 0x2a, 0x2b, 0x2b, 0x2b,
    0x2c, 0x2c, 0x2c, 0x2d, 0x2d, 0x2d, 0x2e, 0x2e, 0x2e, 0x2f, 0x2f, 0x2f,
    0x30, 0x30, 0x30, 0x31, 0x31, 0x31, 0x32, 0x32, 0x32, 0x33, 0x33, 0x33,
    0x34, 0x34, 0x34, 0x35, 0x35, 0x35, 0x36, 0x36, 0x36, 0x37, 0x37, 0x37,
    0x38, 0x38, 0x38, 0x39, 0x39, 0x39, 0x3a, 0x3a, 0x3a, 0x3b, 0x3b, 0x3b,
    0x3c, 0x3c, 0x3c, 0x3d, 0x3d, 0x3d, 0x3e, 0x3e, 0x3e, 0x3f, 0x3f, 0x3f,
    0x40, 0x40, 0x40, 0x41, 0x41, 0x41, 0x42, 0x42, 0x42, 0x43, 0x43, 0x43,
    0x44, 0x44, 0x44, 0x45, 0x45, 0x45, 0x46, 0x46, 0x46, 0x47, 0x47, 0x47,
    0x48, 0x48, 0x48, 0x49, 0x49, 0x49, 0x4a, 0x4a, 0x4a, 0x4b, 0x4b, 0x4b,
    0x4c, 0x4c, 0x4c, 0x4d, 0x4d, 0x4d, 0x4e, 0x4e, 0x4e, 0x4f, 0x4f, 0x4f,
    0x50, 0x50, 0x50, 0x51, 0x51, 0x51, 0x52, 0x52, 0x52, 0x53, 0x53, 0x53,
    0x54, 0x54, 0x54, 0x55, 0x55, 0x55, 0x56, 0x56, 0x56, 0x57, 0x57, 0x57,
    0x58, 0x58, 0x58, 0x59, 0x59, 0x59, 0x5a, 0x5a, 0x5a, 0x5b, 0x5b, 0x5b,
    0x5c, 0x5c, 0x5c, 0x5d, 0x5d, 0x5d, 0x5e, 0x5e, 0x5e, 0x5f, 0x5f, 0x5f,
    0x60, 0x60, 0x60, 0x61, 0x61, 0x61, 0x62, 0x62, 0x62, 0x63, 0x63, 0x63,
    0x64, 0x64, 0x64, 0x65, 0x65, 0x65, 0x66, 0x66, 0x66, 0x67, 0x67, 0x67,
    0x68, 0x68, 0x68, 0x69, 0x69, 0x69, 0x6a, 0x6a, 0x6a, 0x6b, 0x6b, 0x6b,
    0x6c, 0x6c, 0x6c, 0x6d, 0x6d, 0x6d, 0x6e, 0x6e, 0x6e, 0x6f, 0x6f, 0x6f,
    0x70, 0x70, 0x70, 0x71, 0x71, 0x71, 0x72, 0x72, 0x72, 0x73, 0x73, 0x73,
    0x74, 0x74, 0x74, 0x75, 0x75, 0x75, 0x76, 0x76, 0x76, 0x77, 0x77, 0x77,
    0x78, 0x78, 0x78, 0x79, 0x79, 0x79, 0x7a, 0x7a, 0x7a, 0x7b, 0x7b, 0x7b,
    0x7c, 0x7c, 0x7c, 0x7d, 0x7d, 0x7d, 0x7e, 0x7e, 0x7e, 0x7f, 0x7f, 0x7f,
    0x80, 0x80, 0x80, 0x81, 0x81, 0x81, 0x82, 0x82, 0x82, 0x83, 0x83, 0x83,
    0x84, 0x84, 0x84, 0x85, 0x85, 0x85, 0x86, 0x86, 0x86, 0x87, 0x87, 0x87,
    0x88, 0x88, 0x88, 0x89, 0x89, 0x89, 0x8a, 0x8a, 0x8a, 0x8b, 0x8b, 0x8b,
    0x8c, 0x8c, 0x8c, 0x8d, 0x8d, 0x8d, 0x8e, 0x8e, 0x8e, 0x8f, 0x8f, 0x8f,
    0x90, 0x90, 0x90, 0x91, 0x91, 0x91, 0x92, 0x92, 0x92, 0x93, 0x93, 0x93,
    0x94, 0x94, 0x94, 0x95, 0x95, 0x95, 0x96, 0x96, 0x96, 0x97, 0x97, 0x97,
    0x98, 0x98, 0x98, 0x99, 0x99, 0x99, 0x9a, 0x9a, 0x9a, 0x9b, 0x9b, 0x9b,
    0x9c, 0x9c, 0x9c, 0x9d, 0x9d, 0x9d, 0x9e, 0x9e, 0x9e, 0x9f, 0x9f, 0x9f,
    0xa0, 0xa0, 0xa0, 0xa1, 0xa1, 0xa1, 0xa2, 0xa2, 0xa2, 0xa3, 0xa3, 0xa3,
    0xa4, 0xa4, 0xa4, 0xa5, 0xa5, 0xa5, 0xa6, 0xa6, 0xa6, 0xa7, 0xa7, 0xa7,
    0xa8, 0xa8, 0xa8, 0xa9, 0xa9, 0xa9, 0xaa, 0xaa, 0xaa, 0xab, 0xab, 0xab,
    0xac, 0xac, 0xac, 0xad, 0xad, 0xad, 0xae, 0xae, 0xae, 0xaf, 0xaf, 0xaf,
    0xb0, 0xb0, 0xb0, 0xb1, 0xb1, 0xb1, 0xb2, 0xb2, 0xb2, 0xb3, 0xb3, 0xb3,
    0xb4, 0xb4, 0xb4, 0xb5, 0xb5, 0xb5, 0xb6, 0xb6, 0xb6, 0xb7, 0xb7, 0xb7,
    0xb8, 0xb8, 0xb8, 0xb9, 0xb9, 0xb9, 0xba, 0xba, 0xba, 0xbb, 0xbb, 0xbb,
    0xbc, 0xbc, 0xbc, 0xbd, 0xbd, 0xbd, 0xbe, 0xbe, 0xbe, 0xbf, 0xbf, 0xbf,
    0xc0, 0xc0, 0xc0, 0xc1, 0xc1, 0xc1, 0xc2, 0xc2, 0xc2, 0xc3, 0xc3, 0xc3,
    0xc4, 0xc4, 0xc4, 0xc5, 0xc5, 0xc5, 0xc6, 0xc6, 0xc6, 0xc7, 0xc7, 0xc7,
    0xc8, 0xc8, 0xc8, 0xc9, 0xc9, 0xc9, 0xca, 0xca, 0xca, 0xcb, 0xcb, 0xcb,
    0xcc, 0xcc, 0xcc, 0xcd, 0xcd, 0xcd, 0xce, 0xce, 0xce, 0xcf, 0xcf, 0xcf,
    0xd0, 0xd0, 0xd0, 0xd1, 0xd1, 0xd1, 0xd2, 0xd2, 0xd2, 0xd3, 0xd3, 0xd3,
    0xd4, 0xd4, 0xd4, 0xd5, 0xd5, 0xd5, 0xd6, 0xd6, 0xd6, 0xd7, 0xd7, 0xd7,
    0xd8, 0xd8, 0xd8, 0xd9, 0xd9, 0xd9, 0xda, 0xda, 0xda, 0xdb, 0xdb, 0xdb,
    0xdc, 0xdc, 0xdc, 0xdd, 0xdd, 0xdd, 0xde, 0xde, 0xde, 0xdf, 0xdf, 0xdf,
    0xe0, 0xe0, 0xe0, 0xe1, 0xe1, 0xe1, 0xe2, 0xe2, 0xe2, 0xe3, 0xe3, 0xe3,
    0xe4, 0xe4, 0xe4, 0xe5, 0xe5, 0xe5, 0xe6, 0xe6, 0xe6, 0xe7, 0xe7, 0xe7,
    0xe8, 0xe8, 0xe8, 0xe9, 0xe9, 0xe9, 0xea, 0xea, 0xea, 0xeb, 0xeb, 0xeb,
    0xec, 0xec, 0xec, 0xed, 0xed, 0xed, 0xee, 0xee, 0xee, 0xef, 0xef, 0xef,
    0xf0, 0xf0, 0xf0, 0xf1, 0xf1, 0xf1, 0xf2, 0xf2, 0xf2, 0xf3, 0xf3, 0xf3,
    0xf4, 0xf4, 0xf4, 0xf5, 0xf5, 0xf5, 0xf6, 0xf6, 0xf6, 0xf7, 0xf7, 0xf7,
    0xf8, 0xf8, 0xf8, 0xf9, 0xf9, 0xf9, 0xfa, 0xfa, 0xfa, 0xfb, 0xfb, 0xfb,
    0xfc, 0xfc, 0xfc, 0xfd, 0xfd, 0xfd, 0xfe, 0xfe, 0xfe, 0xff, 0xff, 0xff
};


static const unsigned char pal_xterm256[] = {
    0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 0x80, 0x80, 0x00,
    0x00, 0x00, 0x80, 0x80, 0x00, 0x80, 0x00, 0x80, 0x80, 0xc0, 0xc0, 0xc0,
    0x80, 0x80, 0x80, 0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0xff, 0xff, 0x00,
    0x00, 0x00, 0xff, 0xff, 0x00, 0xff, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x5f, 0x00, 0x00, 0x87, 0x00, 0x00, 0xaf,
    0x00, 0x00, 0xd7, 0x00, 0x00, 0xff, 0x00, 0x5f, 0x00, 0x00, 0x5f, 0x5f,
    0x00, 0x5f, 0x87, 0x00, 0x5f, 0xaf, 0x00, 0x5f, 0xd7, 0x00, 0x5f, 0xff,
    0x00, 0x87, 0x00, 0x00, 0x87, 0x5f, 0x00, 0x87, 0x87, 0x00, 0x87, 0xaf,
    0x00, 0x87, 0xd7, 0x00, 0x87, 0xff, 0x00, 0xaf, 0x00, 0x00, 0xaf, 0x5f,
    0x00, 0xaf, 0x87, 0x00, 0xaf, 0xaf, 0x00, 0xaf, 0xd7, 0x00, 0xaf, 0xff,
    0x00, 0xd7, 0x00, 0x00, 0xd7, 0x5f, 0x00, 0xd7, 0x87, 0x00, 0xd7, 0xaf,
    0x00, 0xd7, 0xd7, 0x00, 0xd7, 0xff, 0x00, 0xff, 0x00, 0x00, 0xff, 0x5f,
    0x00, 0xff, 0x87, 0x00, 0xff, 0xaf, 0x00, 0xff, 0xd7, 0x00, 0xff, 0xff,
    0x5f, 0x00, 0x00, 0x5f, 0x00, 0x5f, 0x5f, 0x00, 0x87, 0x5f, 0x00, 0xaf,
    0x5f, 0x00, 0xd7, 0x5f, 0x00, 0xff, 0x5f, 0x5f, 0x00, 0x5f, 0x5f, 0x5f,
    0x5f, 0x5f, 0x87, 0x5f, 0x5f, 0xaf, 0x5f, 0x5f, 0xd7, 0x5f, 0x5f, 0xff,
    0x5f, 0x87, 0x00, 0x5f, 0x87, 0x5f, 0x5f, 0x87, 0x87, 0x5f, 0x87, 0xaf,
    0x5f, 0x87, 0xd7, 0x5f, 0x87, 0xff, 0x5f, 0xaf, 0x00, 0x5f, 0xaf, 0x5f,
    0x5f, 0xaf, 0x87, 0x5f, 0xaf, 0xaf, 0x5f, 0xaf, 0xd7, 0x5f, 0xaf, 0xff,
    0x5f, 0xd7, 0x00, 0x5f, 0xd7, 0x5f, 0x5f, 0xd7, 0x87, 0x5f, 0xd7, 0xaf,
    0x5f, 0xd7, 0xd7, 0x5f, 0xd7, 0xff, 0x5f, 0xff, 0x00, 0x5f, 0xff, 0x5f,
    0x5f, 0xff, 0x87, 0x5f, 0xff, 0xaf, 0x5f, 0xff, 0xd7, 0x5f, 0xff, 0xff,
    0x87, 0x00, 0x00, 0x87, 0x00, 0x5f, 0x87, 0x00, 0x87, 0x87, 0x00, 0xaf,
    0x87, 0x00, 0xd7, 0x87, 0x00, 0xff, 0x87, 0x5f, 0x00, 0x87, 0x5f, 0x5f,
    0x87, 0x5f, 0x87, 0x87, 0x5f, 0xaf, 0x87, 0x5f, 0xd7, 0x87, 0x5f, 0xff,
    0x87, 0x87, 0x00, 0x87, 0x87, 0x5f, 0x87, 0x87, 0x87, 0x87, 0x87, 0xaf,
    0x87, 0x87, 0xd7, 0x87, 0x87, 0xff, 0x87, 0xaf, 0x00, 0x87, 0xaf, 0x5f,
    0x87, 0xaf, 0x87, 0x87, 0xaf, 0xaf, 0x87, 0xaf, 0xd7, 0x87, 0xaf, 0xff,
    0x87, 0xd7, 0x00, 0x87, 0xd7, 0x5f, 0x87, 0xd7, 0x87, 0x87, 0xd7, 0xaf,
    0x87, 0xd7, 0xd7, 0x87, 0xd7, 0xff, 0x87, 0xff, 0x00, 0x87, 0xff, 0x5f,
    0x87, 0xff, 0x87, 0x87, 0xff, 0xaf, 0x87, 0xff, 0xd7, 0x87, 0xff, 0xff,
    0xaf, 0x00, 0x00, 0xaf, 0x00, 0x5f, 0xaf, 0x00, 0x87, 0xaf, 0x00, 0xaf,
    0xaf, 0x00, 0xd7, 0xaf, 0x00, 0xff, 0xaf, 0x5f, 0x00, 0xaf, 0x5f, 0x5f,
    0xaf, 0x5f, 0x87, 0xaf, 0x5f, 0xaf, 0xaf, 0x5f, 0xd7, 0xaf, 0x5f, 0xff,
    0xaf, 0x87, 0x00, 0xaf, 0x87, 0x5f, 0xaf, 0x87, 0x87, 0xaf, 0x87, 0xaf,
    0xaf, 0x87, 0xd7, 0xaf, 0x87, 0xff, 0xaf, 0xaf, 0x00, 0xaf, 0xaf, 0x5f,
    0xaf, 0xaf, 0x87, 0xaf, 0xaf, 0xaf, 0xaf, 0xaf, 0xd7, 0xaf, 0xaf, 0xff,
    0xaf, 0xd7, 0x00, 0xaf, 0xd7, 0x5f, 0xaf, 0xd7, 0x87, 0xaf, 0xd7, 0xaf,
    0xaf, 0xd7, 0xd7, 0xaf, 0xd7, 0xff, 0xaf, 0xff, 0x00, 0xaf, 0xff, 0x5f,
    0xaf, 0xff, 0x87, 0xaf, 0xff, 0xaf, 0xaf, 0xff, 0xd7, 0xaf, 0xff, 0xff,
    0xd7, 0x00, 0x00, 0xd7, 0x00, 0x5f, 0xd7, 0x00, 0x87, 0xd7, 0x00, 0xaf,
    0xd7, 0x00, 0xd7, 0xd7, 0x00, 0xff, 0xd7, 0x5f, 0x00, 0xd7, 0x5f, 0x5f,
    0xd7, 0x5f, 0x87, 0xd7, 0x5f, 0xaf, 0xd7, 0x5f, 0xd7, 0xd7, 0x5f, 0xff,
    0xd7, 0x87, 0x00, 0xd7, 0x87, 0x5f, 0xd7, 0x87, 0x87, 0xd7, 0x87, 0xaf,
    0xd7, 0x87, 0xd7, 0xd7, 0x87, 0xff, 0xd7, 0xaf, 0x00, 0xd7, 0xaf, 0x5f,
    0xd7, 0xaf, 0x87, 0xd7, 0xaf, 0xaf, 0xd7, 0xaf, 0xd7, 0xd7, 0xaf, 0xff,
    0xd7, 0xd7, 0x00, 0xd7, 0xd7, 0x5f, 0xd7, 0xd7, 0x87, 0xd7, 0xd7, 0xaf,
    0xd7, 0xd7, 0xd7, 0xd7, 0xd7, 0xff, 0xd7, 0xff, 0x00, 0xd7, 0xff, 0x5f,
    0xd7, 0xff, 0x87, 0xd7, 0xff, 0xaf, 0xd7, 0xff, 0xd7, 0xd7, 0xff, 0xff,
    0xff, 0x00, 0x00, 0xff, 0x00, 0x5f, 0xff, 0x00, 0x87, 0xff, 0x00, 0xaf,
    0xff, 0x00, 0xd7, 0xff, 0x00, 0xff, 0xff, 0x5f, 0x00, 0xff, 0x5f, 0x5f,
    0xff, 0x5f, 0x87, 0xff, 0x5f, 0xaf, 0xff, 0x5f, 0xd7, 0xff, 0x5f, 0xff,
    0xff, 0x87, 0x00, 0xff, 0x87, 0x5f, 0xff, 0x87, 0x87, 0xff, 0x87, 0xaf,
    0xff, 0x87, 0xd7, 0xff, 0x87, 0xff, 0xff, 0xaf, 0x00, 0xff, 0xaf, 0x5f,
    0xff, 0xaf, 0x87, 0xff, 0xaf, 0xaf, 0xff, 0xaf, 0xd7, 0xff, 0xaf, 0xff,
    0xff, 0xd7, 0x00, 0xff, 0xd7, 0x5f, 0xff, 0xd7, 0x87, 0xff, 0xd7, 0xaf,
    0xff, 0xd7, 0xd7, 0xff, 0xd7, 0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0x5f,
    0xff, 0xff, 0x87, 0xff, 0xff, 0xaf, 0xff, 0xff, 0xd7, 0xff, 0xff, 0xff,
    0x08, 0x08, 0x08, 0x12, 0x12, 0x12, 0x1c, 0x1c, 0x1c, 0x26, 0x26, 0x26,
    0x30, 0x30, 0x30, 0x3a, 0x3a, 0x3a, 0x44, 0x44, 0x44, 0x4e, 0x4e, 0x4e,
    0x58, 0x58, 0x58, 0x62, 0x62, 0x62, 0x6c, 0x6c, 0x6c, 0x76, 0x76, 0x76,
    0x80, 0x80, 0x80, 0x8a, 0x8a, 0x8a, 0x94, 0x94, 0x94, 0x9e, 0x9e, 0x9e,
    0xa8, 0xa8, 0xa8, 0xb2, 0xb2, 0xb2, 0xbc, 0xbc, 0xbc, 0xc6, 0xc6, 0xc6,
    0xd0, 0xd0, 0xd0, 0xda, 0xda, 0xda, 0xe4, 0xe4, 0xe4, 0xee, 0xee, 0xee,
};


#if defined(_MSC_VER)
# define SIXEL_TLS __declspec(thread)
# define SIXEL_TLS_AVAILABLE 1
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L \
    && !defined(__STDC_NO_THREADS__) \
    && !defined(__PCC__)
# define SIXEL_TLS _Thread_local
# define SIXEL_TLS_AVAILABLE 1
#elif defined(__GNUC__) && !defined(__PCC__)
# define SIXEL_TLS __thread
# define SIXEL_TLS_AVAILABLE 1
#else
# define SIXEL_TLS
# define SIXEL_TLS_AVAILABLE 0
#endif

/*
 * Fast LUT lookups rely on per-thread scratch state.  A TLS indirection keeps
 * parallel dithering workers from stomping on each other's lookup context when
 * several bands run concurrently.
 */
static SIXEL_TLS sixel_lut_t *dither_lut_context = NULL;

#undef SIXEL_TLS

/* lookup closest color from palette with "normal" strategy */
static int
lookup_normal(unsigned char const * const pixel,
              int const depth,
              unsigned char const * const palette,
              int const reqcolor,
              unsigned short * const cachetable,
              int const complexion)
{
    int result;
    int diff;
    int r;
    int i;
    int n;
    int distant;

    result = (-1);
    diff = INT_MAX;

    /* don't use cachetable in 'normal' strategy */
    (void) cachetable;

    for (i = 0; i < reqcolor; i++) {
        distant = 0;
        r = pixel[0] - palette[i * depth + 0];
        distant += r * r * complexion;
        for (n = 1; n < depth; ++n) {
            r = pixel[n] - palette[i * depth + n];
            distant += r * r;
        }
        if (distant < diff) {
            diff = distant;
            result = i;
        }
    }

    return result;
}


/*
 * Shared fast lookup flow handled by the lut module.  The palette lookup now
 * delegates to sixel_lut_map_pixel() so policy-specific caches and the
 * certification tree stay encapsulated inside src/lookup-common.c.
 */

static int
lookup_fast_lut(unsigned char const * const pixel,
                int const depth,
                unsigned char const * const palette,
                int const reqcolor,
                unsigned short * const cachetable,
                int const complexion)
{
    (void)depth;
    (void)palette;
    (void)reqcolor;
    (void)cachetable;
    (void)complexion;

    if (dither_lut_context == NULL) {
        return 0;
    }

    return sixel_lut_map_pixel(dither_lut_context, pixel);
}


static int
lookup_mono_darkbg(unsigned char const * const pixel,
                   int const depth,
                   unsigned char const * const palette,
                   int const reqcolor,
                   unsigned short * const cachetable,
                   int const complexion)
{
    int n;
    int distant;

    /* unused */ (void) palette;
    /* unused */ (void) cachetable;
    /* unused */ (void) complexion;

    distant = 0;
    for (n = 0; n < depth; ++n) {
        distant += pixel[n];
    }
    return distant >= 128 * reqcolor ? 1: 0;
}


static int
lookup_mono_lightbg(unsigned char const * const pixel,
                    int const depth,
                    unsigned char const * const palette,
                    int const reqcolor,
                    unsigned short * const cachetable,
                    int const complexion)
{
    int n;
    int distant;

    /* unused */ (void) palette;
    /* unused */ (void) cachetable;
    /* unused */ (void) complexion;

    distant = 0;
    for (n = 0; n < depth; ++n) {
        distant += pixel[n];
    }
    return distant < 128 * reqcolor ? 1: 0;
}

static SIXELSTATUS
sixel_dither_validate_complexion_limit(int depth, int complexion)
{
    enum { max_channel_diff_sq = 255 * 255 };
    int non_weighted_components;
    long long weighted_budget;
    long long max_complexion;

    if (complexion <= 1) {
        return SIXEL_OK;
    }

    non_weighted_components = (depth > 1) ? (depth - 1) : 0;
    weighted_budget = (long long)INT_MAX
        - (long long)max_channel_diff_sq * (long long)non_weighted_components;
    if (weighted_budget <= 0) {
        max_complexion = 0;
    } else {
        max_complexion = weighted_budget / (long long)max_channel_diff_sq;
    }

    if ((long long)complexion > max_complexion) {
        sixel_helper_set_additional_message(
            "sixel_dither_map_pixels: complexion parameter is too large.");
        return SIXEL_BAD_ARGUMENT;
    }

    return SIXEL_OK;
}

/*
 * Apply the palette into the supplied pixel buffer while coordinating the
 * dithering strategy.  The routine performs the following steps:
 *   - Select an index lookup helper, enabling the fast LUT path when the
 *     caller requested palette optimization and the input pixels are RGB.
 *   - Ensure the LUT object is prepared for the active policy, including
 *     weight selection for CERTLUT so complexion aware lookups remain
 *     accurate.
 *   - Dispatch to the positional, variable, or fixed error diffusion
 *     routines, each of which expects the LUT context to be initialized
 *     beforehand.
 *   - Release any temporary LUT references that were acquired for the fast
 *     lookup path.
 */
static SIXELSTATUS
sixel_dither_map_pixels(
    sixel_index_t     /* out */ *result,
    unsigned char     /* in */  *data,
    int               /* in */  width,
    int               /* in */  height,
    int               /* in */  band_origin,
    int               /* in */  output_start,
    int               /* in */  depth,
    unsigned char     /* in */  *palette,
    int               /* in */  reqcolor,
    int               /* in */  methodForDiffuse,
    int               /* in */  methodForScan,
    int               /* in */  foptimize,
    int               /* in */  foptimize_palette,
    int               /* in */  complexion,
    int               /* in */  lut_policy,
    int               /* in */  method_for_largest,
    sixel_lut_t       /* in */  *lut,
    int               /* in */  *ncolors,
    sixel_allocator_t /* in */  *allocator,
    sixel_dither_t    /* in */  *dither,
    int               /* in */  pixelformat)
{
    unsigned char copy[SIXEL_MAX_CHANNELS];
    float new_palette_float[SIXEL_PALETTE_MAX * SIXEL_MAX_CHANNELS];
    SIXELSTATUS status = SIXEL_FALSE;
    int sum1;
    int sum2;
    int n;
    unsigned char new_palette[SIXEL_PALETTE_MAX * 4];
    unsigned short migration_map[SIXEL_PALETTE_MAX];
    sixel_dither_context_t context;
    int (*f_lookup)(unsigned char const * const pixel,
                    int const depth,
                    unsigned char const * const palette,
                    int const reqcolor,
                    unsigned short * const cachetable,
                    int const complexion) = lookup_normal;
    int use_varerr;
    int use_positional;
    sixel_lut_t *active_lut;
    int manage_lut;
    int policy;
    int shared_lut;
    float const *palette_float;
    int palette_float_depth;
    sixel_filter_lookup_config_t lookup_config;
    sixel_filter_lookup_result_t lookup_result;

    /*
     * Per-component weights used by the lookup backends.  These remain generic
     * to support RGB as well as alternate color spaces when evaluating palette
     * distance.
     */

    active_lut = NULL;
    manage_lut = 0;
    palette_float = NULL;
    palette_float_depth = 0;
    memset(&lookup_config, 0, sizeof(lookup_config));
    memset(&lookup_result, 0, sizeof(lookup_result));

    memset(&context, 0, sizeof(context));
    context.result = result;
    context.width = width;
    context.height = height;
    context.band_origin = band_origin;
    context.output_start = output_start;
    context.depth = depth;
    context.palette = palette;
    context.reqcolor = reqcolor;
    context.new_palette = new_palette;
    context.migration_map = migration_map;
    context.ncolors = ncolors;
    context.scratch = copy;
    context.indextable = NULL;
    context.pixels = data;
    context.pixels_float = NULL;
    context.pixelformat = pixelformat;
    context.palette_float = NULL;
    context.new_palette_float = NULL;
    context.float_depth = 0;
    context.lookup_source_is_float = 0;
    context.prefer_palette_float_lookup = 0;
    context.transparent_mask = NULL;
    context.transparent_mask_size = 0;
    context.transparent_keycolor = (-1);
    context.bluenoise_gradient_map = NULL;
    context.bluenoise_gradient_map_size = 0U;
    context.bluenoise_gradient_width = 0;
    context.bluenoise_gradient_height = 0;
    context.lut = NULL;
    if (SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat)) {
        context.pixels_float = (float *)(void *)data;
    }

    if (dither != NULL && dither->palette != NULL) {
        sixel_palette_t *palette_object;
        int float_components;

        palette_object = dither->palette;
        if (palette_object->entries_float32 != NULL
                && palette_object->float_depth > 0) {
            float_components = palette_object->float_depth
                / (int)sizeof(float);
            if (float_components > 0
                    && (size_t)float_components <= SIXEL_MAX_CHANNELS) {
                context.palette_float = palette_object->entries_float32;
                context.float_depth = float_components;
                context.new_palette_float = new_palette_float;
                palette_float = palette_object->entries_float32;
                palette_float_depth = palette_object->float_depth;
            }
        }
    }
    if (dither != NULL
            && dither->pipeline_transparent_mask != NULL
            && dither->pipeline_transparent_keycolor >= 0
            && dither->pipeline_transparent_keycolor < SIXEL_PALETTE_MAX) {
        context.transparent_mask = dither->pipeline_transparent_mask;
        context.transparent_mask_size = dither->pipeline_transparent_mask_size;
        context.transparent_keycolor = dither->pipeline_transparent_keycolor;
    }
    if (dither != NULL && dither->bluenoise_gradient_map != NULL) {
        context.bluenoise_gradient_map = dither->bluenoise_gradient_map;
        context.bluenoise_gradient_map_size =
            dither->bluenoise_gradient_map_size;
        context.bluenoise_gradient_width = dither->bluenoise_gradient_width;
        context.bluenoise_gradient_height = dither->bluenoise_gradient_height;
    }

    if (reqcolor < 1) {
        status = SIXEL_BAD_ARGUMENT;
        sixel_helper_set_additional_message(
            "sixel_dither_map_pixels: "
            "a bad argument is detected, reqcolor < 0.");
        goto end;
    }

    use_varerr = (depth == 3
                  && methodForDiffuse == SIXEL_DIFFUSE_LSO2);
    use_positional = (methodForDiffuse == SIXEL_DIFFUSE_A_DITHER
                      || methodForDiffuse == SIXEL_DIFFUSE_X_DITHER
                      || methodForDiffuse == SIXEL_DIFFUSE_BLUENOISE_DITHER);
    context.method_for_diffuse = methodForDiffuse;
    context.method_for_scan = methodForScan;

    if (reqcolor == 2) {
        sum1 = 0;
        sum2 = 0;
        for (n = 0; n < depth; ++n) {
            sum1 += palette[n];
        }
        for (n = depth; n < depth + depth; ++n) {
            sum2 += palette[n];
        }
        if (sum1 == 0 && sum2 == 255 * 3) {
            f_lookup = lookup_mono_darkbg;
        } else if (sum1 == 255 * 3 && sum2 == 0) {
            f_lookup = lookup_mono_lightbg;
        }
    }
    if (foptimize && depth == 3 && f_lookup == lookup_normal) {
        f_lookup = lookup_fast_lut;
    }
#if SIXEL_ENABLE_THREADS && !SIXEL_TLS_AVAILABLE
    if (f_lookup == lookup_fast_lut) {
        /*
         * Fast lookup stores the active LUT in a TLS pointer for the hot path.
         * Compilers without TLS support collapse that pointer into a process
         * global, which is unsafe when multiple worker threads map pixels at
         * the same time.
         */
        f_lookup = lookup_normal;
    }
#endif
    if (lut_policy == SIXEL_LUT_POLICY_NONE) {
        f_lookup = lookup_normal;
    }

    if ((f_lookup == lookup_normal || f_lookup == lookup_fast_lut)
            && !SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat)) {
        status = sixel_dither_validate_complexion_limit(depth, complexion);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    if (f_lookup == lookup_fast_lut) {
        if (depth != 3) {
            status = SIXEL_BAD_ARGUMENT;
            sixel_helper_set_additional_message(
                "sixel_dither_map_pixels: fast lookup requires RGB pixels.");
            goto end;
        }
        policy = lut_policy;
        if (policy != SIXEL_LUT_POLICY_CERTLUT
            && policy != SIXEL_LUT_POLICY_5BIT
            && policy != SIXEL_LUT_POLICY_6BIT
            && policy != SIXEL_LUT_POLICY_EYTZINGER
            && policy != SIXEL_LUT_POLICY_FHEDT
            && policy != SIXEL_LUT_POLICY_VPTREE
            && policy != SIXEL_LUT_POLICY_RBC
            && policy != SIXEL_LUT_POLICY_MAHALANOBIS) {
            policy = SIXEL_LUT_POLICY_6BIT;
        }
        shared_lut = 1;
        if (policy == SIXEL_LUT_POLICY_CERTLUT) {
            shared_lut = sixel_lookup_env_shared_certlut();
        } else if (policy == SIXEL_LUT_POLICY_5BIT) {
            shared_lut = sixel_lookup_env_shared_5bit();
        } else if (policy == SIXEL_LUT_POLICY_6BIT) {
            shared_lut = sixel_lookup_env_shared_6bit();
        }
        if (lut != NULL && sixel_lookup_parallel_dither_active() != 0
                && shared_lut == 0) {
            /*
             * Caller requested thread-local CERTLUT/DENCELUT caches.
             * Drop the shared handle so each worker builds an isolated
             * instance instead of serializing on a mutex.
             */
            lut = NULL;
        }
        if (lut != NULL && sixel_lookup_parallel_dither_active() != 0
                && shared_lut != 0) {
            /*
             * Parallel palette application reuses the preconfigured LUT to
             * avoid rebuilding FHEDT inside each worker.  The shared LUT is
             * immutable after setup, so workers only need a read-only handle
             * here.
             */
            active_lut = lut;
            manage_lut = 0;
        } else {
            lookup_config.palette = palette;
            lookup_config.palette_float = palette_float;
            lookup_config.depth = depth;
            lookup_config.float_depth = palette_float_depth;
            lookup_config.ncolors = reqcolor;
            lookup_config.complexion = complexion;
            lookup_config.method_for_largest = method_for_largest;
            lookup_config.lut_policy = policy;
            lookup_config.pixelformat = pixelformat;
            lookup_config.reuse_lut = lut;

            status = sixel_filter_lookup_build(&lookup_config,
                                               allocator,
                                               NULL,
                                               &lookup_result);
            if (SIXEL_FAILED(status)) {
                goto end;
            }

            active_lut = lookup_result.lut;
            manage_lut = lookup_result.owned;
        }
        context.lut = active_lut;
        dither_lut_context = active_lut;
    }

    context.lookup = f_lookup;
    if (f_lookup == lookup_fast_lut
        && SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat)) {
        context.lookup_source_is_float = 1;
    }
    if (SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat)
        && context.palette_float != NULL
        && context.float_depth >= context.depth
        && f_lookup == lookup_normal) {
        context.prefer_palette_float_lookup = 1;
    }
    context.optimize_palette = foptimize_palette;
    context.complexion = complexion;

    if (use_positional) {
        if (context.pixels_float != NULL
            && dither != NULL
            && dither->prefer_float32 != 0) {
            status = sixel_dither_apply_positional_float32(dither, &context);
            if (status == SIXEL_BAD_ARGUMENT) {
                status = sixel_dither_apply_positional_8bit(dither, &context);
            }
        } else {
            status = sixel_dither_apply_positional_8bit(dither, &context);
        }
    } else if (use_varerr) {
        if (context.pixels_float != NULL
            && dither != NULL
            && dither->prefer_float32 != 0) {
            status = sixel_dither_apply_varcoeff_float32(dither, &context);
            if (status == SIXEL_BAD_ARGUMENT) {
                status = sixel_dither_apply_varcoeff_8bit(dither, &context);
            }
        } else {
            status = sixel_dither_apply_varcoeff_8bit(dither, &context);
        }
    } else {
        if (SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat)
            && context.pixels_float != NULL
            && depth == 3
            && dither != NULL
            && dither->prefer_float32 != 0) {
            /*
             * Float inputs can reuse the float32 renderer for every
             * fixed-weight kernel (FS, Sierra, Stucki, etc.), including
             * interframe diffusion strategies.
             */
            status = sixel_dither_apply_fixed_float32(dither, &context);
            if (status == SIXEL_BAD_ARGUMENT) {
                status = sixel_dither_apply_fixed_8bit(dither, &context);
            }
        } else {
            status = sixel_dither_apply_fixed_8bit(dither, &context);
        }
    }
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = SIXEL_OK;

end:
    if (dither_lut_context != NULL && f_lookup == lookup_fast_lut) {
        dither_lut_context = NULL;
    }
    if (manage_lut && active_lut != NULL) {
        sixel_lut_unref(active_lut);
    }
    return status;
}

#if SIXEL_ENABLE_THREADS
typedef struct sixel_parallel_dither_plan {
    sixel_index_t *dest;
    unsigned char *pixels;
    sixel_palette_t *palette;
    sixel_lut_t *lut;
    sixel_allocator_t *allocator;
    sixel_dither_t *dither;
    size_t row_bytes;
    int width;
    int height;
    int band_height;
    int overlap;
    int method_for_diffuse;
    int method_for_scan;
    int optimize_palette;
    int optimize_palette_entries;
    int complexion;
    int lut_policy;
    int method_for_largest;
    int reqcolor;
    int pixelformat;
    int pin_threads;
    sixel_logger_t *logger;
} sixel_parallel_dither_plan_t;

/*
 * Dedicated worker state for dithering threads so it does not collide with
 * encoder worker bookkeeping when compiled as a single translation unit.
 */
typedef struct sixel_parallel_dither_state {
    sixel_lut_t *lut;
    int lut_initialized;
} sixel_parallel_dither_state_t;

static void
sixel_parallel_dither_cleanup(void *workspace)
{
    sixel_parallel_dither_state_t *state;

    state = (sixel_parallel_dither_state_t *)workspace;
    if (state == NULL) {
        return;
    }
    if (state->lut_initialized != 0 && state->lut != NULL) {
        /*
         * Each worker owns its private LUT instance when shared caches are
         * disabled.  Release it here so threadpool teardown can free the
         * workspace without leaking per-thread caches.  The mutex inside the
         * LUT was already removed during configuration for the private mode,
         * so this final drop is the only step needed to return allocator
         * ownership.
         */
        sixel_lut_unref(state->lut);
    }
}

static int
sixel_dither_parallel_worker(tp_job_t job,
                             void *userdata,
                             void *workspace)
{
    sixel_parallel_dither_plan_t *plan;
    unsigned char const *source;
    unsigned char *copy;
    size_t required;
    size_t offset;
    int band_index;
    int y0;
    int y1;
    int in0;
    int in1;
    int rows;
    int local_ncolors;
    int wcomp1;
    int wcomp2;
    int wcomp3;
    SIXELSTATUS status;
    sixel_parallel_dither_state_t *state;
    sixel_lut_t *local_lut;
    int restore_context;

    plan = (sixel_parallel_dither_plan_t *)userdata;
    if (plan == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    band_index = job.band_index;
    if (band_index < 0) {
        return SIXEL_BAD_ARGUMENT;
    }

    y0 = band_index * plan->band_height;
    if (y0 >= plan->height) {
        return SIXEL_OK;
    }

    y1 = y0 + plan->band_height;
    if (y1 > plan->height) {
        y1 = plan->height;
    }

    in0 = y0 - plan->overlap;
    if (in0 < 0) {
        in0 = 0;
    }
    in1 = y1;
    rows = in1 - in0;
    if (rows <= 0) {
        return SIXEL_OK;
    }

    required = (size_t)rows * plan->row_bytes;
    offset = (size_t)in0 * plan->row_bytes;
    copy = NULL;
    source = plan->pixels + offset;
    if (plan->overlap > 0) {
        copy = (unsigned char *)malloc(required);
        if (copy == NULL) {
            return SIXEL_BAD_ALLOCATION;
        }
        memcpy(copy, source, required);
        source = copy;
    }

    if (plan->logger != NULL) {
        restore_context = sixel_dither_logger_set_frame_context(
            plan->logger,
            plan->dither);
        sixel_logger_logf(plan->logger,
                          "worker",
                          "dither",
                          "start",
                          band_index,
                          in0,
                          y0,
                          y1,
                          in0,
                          in1,
                          "prepare rows=%d",
                          rows);
        if (restore_context != 0) {
            sixel_logger_clear_frame_context(plan->logger);
        }
    }

    local_ncolors = plan->reqcolor;
    state = (sixel_parallel_dither_state_t *)workspace;
    local_lut = plan->lut;
    restore_context = 0;
    if (local_lut == NULL && state != NULL) {
        if (state->lut_initialized == 0) {
            status = sixel_lut_new(&state->lut,
                                   plan->lut_policy,
                                   plan->allocator);
            if (SIXEL_FAILED(status)) {
                if (copy != NULL) {
                    free(copy);
                }
                return status;
            }
            if (plan->lut_policy == SIXEL_LUT_POLICY_CERTLUT) {
                if (plan->method_for_largest == SIXEL_LARGE_LUM) {
                    wcomp1 = plan->complexion * 299;
                    wcomp2 = 587;
                    wcomp3 = 114;
                } else {
                    wcomp1 = plan->complexion;
                    wcomp2 = 1;
                    wcomp3 = 1;
                }
            } else {
                wcomp1 = plan->complexion;
                wcomp2 = 1;
                wcomp3 = 1;
            }
            status = sixel_lut_configure(state->lut,
                                         plan->palette->entries,
                                         plan->palette->entries_float32,
                                         plan->palette->depth,
                                         plan->palette->float_depth,
                                         (int)plan->palette->entry_count,
                                         plan->complexion,
                                         wcomp1,
                                         wcomp2,
                                         wcomp3,
                                         plan->lut_policy,
                                         plan->pixelformat);
            if (SIXEL_FAILED(status)) {
                sixel_lut_unref(state->lut);
                state->lut = NULL;
                if (copy != NULL) {
                    free(copy);
                }
                return status;
            }
            state->lut_initialized = 1;
        }
        local_lut = state->lut;
    }
    /*
     * Map directly into the shared destination but suppress writes
     * before output_start.  The overlap rows are computed only to warm
     * up the error diffusion and are discarded by the output_start
     * check in the renderer, so neighboring bands never clobber each
     * other's body.
     */
    status = sixel_dither_map_pixels(plan->dest + (size_t)in0 * plan->width,
                                     (unsigned char *)source,
                                     plan->width,
                                     rows,
                                     in0,
                                     y0,
                                     3,
                                     plan->palette->entries,
                                     plan->reqcolor,
                                     plan->method_for_diffuse,
                                     plan->method_for_scan,
                                     plan->optimize_palette,
                                     plan->optimize_palette_entries,
                                     plan->complexion,
                                     plan->lut_policy,
                                     plan->method_for_largest,
                                     local_lut,
                                     &local_ncolors,
                                     plan->allocator,
                                     plan->dither,
                                     plan->pixelformat);
    if (plan->logger != NULL) {
        restore_context = sixel_dither_logger_set_frame_context(
            plan->logger,
            plan->dither);
        sixel_logger_logf(plan->logger,
                          "worker",
                          "dither",
                          "finish",
                          band_index,
                          in1 - 1,
                          y0,
                          y1,
                          in0,
                          in1,
                          "status=%d rows=%d",
                          status,
                          rows);
        if (restore_context != 0) {
            sixel_logger_clear_frame_context(plan->logger);
        }
    }
    if (copy != NULL) {
        free(copy);
    }
    return status;
}

static SIXELSTATUS
sixel_dither_apply_palette_parallel(sixel_parallel_dither_plan_t *plan,
                                    int threads)
{
    SIXELSTATUS status;
    threadpool_t *pool;
    size_t depth_bytes;
    size_t workspace_size;
    int nbands;
    int queue_depth;
    int band_index;
    int stride;
    int offset;
    tp_workspace_cleanup_fn cleanup;

    if (plan == NULL || plan->palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    status = sixel_dither_validate_complexion_limit(3, plan->complexion);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    depth_bytes = (size_t)sixel_helper_compute_depth(plan->pixelformat);
    if (depth_bytes == 0U) {
        return SIXEL_BAD_ARGUMENT;
    }
    plan->row_bytes = (size_t)plan->width * depth_bytes;

    nbands = (plan->height + plan->band_height - 1) / plan->band_height;
    if (nbands < 1) {
        return SIXEL_OK;
    }

    if (threads > nbands) {
        threads = nbands;
    }
    if (threads < 1) {
        threads = 1;
    }

    queue_depth = threads * 3;
    if (queue_depth > nbands) {
        queue_depth = nbands;
    }
    if (queue_depth < 1) {
        queue_depth = 1;
    }

    workspace_size = 0U;
    cleanup = NULL;
    if (plan->lut == NULL && plan->lut_policy != SIXEL_LUT_POLICY_NONE) {
        workspace_size = sizeof(sixel_parallel_dither_state_t);
        cleanup = sixel_parallel_dither_cleanup;
        /*
         * Worker-local caches are constructed only when the shared LUT is
         * disabled.  Allocating the workspace up front lets each thread keep
         * the configured LUT for all assigned bands instead of rebuilding it
         * every time the worker callback is invoked.
         */
    }
    pool = threadpool_create(threads,
                             queue_depth,
                             workspace_size,
                             sixel_dither_parallel_worker,
                             plan,
                             cleanup);
    if (pool == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    threadpool_set_affinity(pool, plan->pin_threads);

    /*
     * Distribute the initial jobs so each worker starts far apart, then feed
     * follow-up work that walks downward from those seeds.  This staggered
     * order reduces contention around the top of the image and keeps later
     * assignments close to the last band a worker processed, improving cache
     * locality.
     */
    stride = (nbands + threads - 1) / threads;
    for (offset = 0; offset < stride; ++offset) {
        for (band_index = 0; band_index < threads; ++band_index) {
            tp_job_t job;
            int seeded;

            seeded = band_index * stride + offset;
            if (seeded >= nbands) {
                continue;
            }
            job.band_index = seeded;
            threadpool_push(pool, job);
        }
    }

    threadpool_finish(pool);
    status = threadpool_get_error(pool);
    threadpool_destroy(pool);

    return status;
}
#endif


/*
 * Helper that detects whether the palette currently matches either of the
 * builtin monochrome definitions.  These tables skip cache initialization
 * during fast-path dithering because they already match the terminal
 * defaults.
 */
static int
sixel_palette_is_builtin_mono(sixel_palette_t const *palette)
{
    if (palette == NULL) {
        return 0;
    }
    if (palette->entries == NULL) {
        return 0;
    }
    if (palette->entry_count < 2U) {
        return 0;
    }
    if (palette->depth != 3) {
        return 0;
    }
    if (memcmp(palette->entries, pal_mono_dark,
               sizeof(pal_mono_dark)) == 0) {
        return 1;
    }
    if (memcmp(palette->entries, pal_mono_light,
               sizeof(pal_mono_light)) == 0) {
        return 1;
    }
    return 0;
}

/* Bundle serial index-resolution inputs to avoid pcc ICE on long signatures. */
typedef struct sixel_dither_resolve_indexes_request {
    sixel_index_t *result;
    unsigned char *data;
    int width;
    int height;
    int depth;
    sixel_palette_t *palette;
    int reqcolor;
    int method_for_diffuse;
    int method_for_scan;
    int foptimize;
    int foptimize_palette;
    int complexion;
    int lut_policy;
    int method_for_largest;
    int *ncolors;
    sixel_allocator_t *allocator;
    sixel_dither_t *dither;
    int pixelformat;
} sixel_dither_resolve_indexes_request_t;

/*
 * Route palette application through the local dithering helper.  The
 * function keeps all state in the palette object so we can share cache
 * buffers between invocations and later stages.  The flow is:
 *   1. Synchronize the quantizer configuration with the dither object so the
 *      LUT builder honors the requested policy.
 *   2. Invoke sixel_dither_map_pixels() to populate the index buffer and
 *      record the resulting palette size.
 *   3. Return the status to the caller so palette application errors can be
 *      reported at a single site.
 */
static SIXELSTATUS
sixel_dither_resolve_indexes(
    sixel_dither_resolve_indexes_request_t const *request)
{
    SIXELSTATUS status;
    sixel_index_t *result;
    unsigned char *data;
    int width;
    int height;
    int depth;
    sixel_palette_t *palette;
    int reqcolor;
    int method_for_diffuse;
    int method_for_scan;
    int foptimize;
    int foptimize_palette;
    int complexion;
    int lut_policy;
    int method_for_largest;
    int *ncolors;
    sixel_allocator_t *allocator;
    sixel_dither_t *dither;
    int pixelformat;

    status = SIXEL_FALSE;
    if (request == NULL || request->palette == NULL
            || request->palette->entries == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    result = request->result;
    data = request->data;
    width = request->width;
    height = request->height;
    depth = request->depth;
    palette = request->palette;
    reqcolor = request->reqcolor;
    method_for_diffuse = request->method_for_diffuse;
    method_for_scan = request->method_for_scan;
    foptimize = request->foptimize;
    foptimize_palette = request->foptimize_palette;
    complexion = request->complexion;
    lut_policy = request->lut_policy;
    method_for_largest = request->method_for_largest;
    ncolors = request->ncolors;
    allocator = request->allocator;
    dither = request->dither;
    pixelformat = request->pixelformat;

    sixel_palette_set_lut_policy(lut_policy);
    sixel_palette_set_method_for_largest(method_for_largest);

    status = sixel_dither_map_pixels(result,
                                     data,
                                     width,
                                     height,
                                     0,
                                     0,
                                     depth,
                                     palette->entries,
                                     reqcolor,
                                     method_for_diffuse,
                                     method_for_scan,
                                     foptimize,
                                     foptimize_palette,
                                     complexion,
                                     lut_policy,
                                     method_for_largest,
                                     palette->lut,
                                     ncolors,
                                     allocator,
                                     dither,
                                     pixelformat);

    return status;
}


/*
 * VT340 undocumented behavior regarding the color palette reported
 * by Vertis Sidus(@vrtsds):
 *     it loads the first fifteen colors as 1 through 15, and loads the
 *     sixteenth color as 0.
 */
static const unsigned char pal_vt340_mono[] = {
    /* 1   Gray-2   */  13 * 255 / 100, 13 * 255 / 100, 13 * 255 / 100,
    /* 2   Gray-4   */  26 * 255 / 100, 26 * 255 / 100, 26 * 255 / 100,
    /* 3   Gray-6   */  40 * 255 / 100, 40 * 255 / 100, 40 * 255 / 100,
    /* 4   Gray-1   */   6 * 255 / 100,  6 * 255 / 100,  6 * 255 / 100,
    /* 5   Gray-3   */  20 * 255 / 100, 20 * 255 / 100, 20 * 255 / 100,
    /* 6   Gray-5   */  33 * 255 / 100, 33 * 255 / 100, 33 * 255 / 100,
    /* 7   White 7  */  46 * 255 / 100, 46 * 255 / 100, 46 * 255 / 100,
    /* 8   Black 0  */   0 * 255 / 100,  0 * 255 / 100,  0 * 255 / 100,
    /* 9   Gray-2   */  13 * 255 / 100, 13 * 255 / 100, 13 * 255 / 100,
    /* 10  Gray-4   */  26 * 255 / 100, 26 * 255 / 100, 26 * 255 / 100,
    /* 11  Gray-6   */  40 * 255 / 100, 40 * 255 / 100, 40 * 255 / 100,
    /* 12  Gray-1   */   6 * 255 / 100,  6 * 255 / 100,  6 * 255 / 100,
    /* 13  Gray-3   */  20 * 255 / 100, 20 * 255 / 100, 20 * 255 / 100,
    /* 14  Gray-5   */  33 * 255 / 100, 33 * 255 / 100, 33 * 255 / 100,
    /* 15  White 7  */  46 * 255 / 100, 46 * 255 / 100, 46 * 255 / 100,
    /* 0   Black    */   0 * 255 / 100,  0 * 255 / 100,  0 * 255 / 100,
};


static const unsigned char pal_vt340_color[] = {
    /* 1   Blue     */  20 * 255 / 100, 20 * 255 / 100, 80 * 255 / 100,
    /* 2   Red      */  80 * 255 / 100, 13 * 255 / 100, 13 * 255 / 100,
    /* 3   Green    */  20 * 255 / 100, 80 * 255 / 100, 20 * 255 / 100,
    /* 4   Magenta  */  80 * 255 / 100, 20 * 255 / 100, 80 * 255 / 100,
    /* 5   Cyan     */  20 * 255 / 100, 80 * 255 / 100, 80 * 255 / 100,
    /* 6   Yellow   */  80 * 255 / 100, 80 * 255 / 100, 20 * 255 / 100,
    /* 7   Gray 50% */  53 * 255 / 100, 53 * 255 / 100, 53 * 255 / 100,
    /* 8   Gray 25% */  26 * 255 / 100, 26 * 255 / 100, 26 * 255 / 100,
    /* 9   Blue*    */  33 * 255 / 100, 33 * 255 / 100, 60 * 255 / 100,
    /* 10  Red*     */  60 * 255 / 100, 26 * 255 / 100, 26 * 255 / 100,
    /* 11  Green*   */  33 * 255 / 100, 60 * 255 / 100, 33 * 255 / 100,
    /* 12  Magenta* */  60 * 255 / 100, 33 * 255 / 100, 60 * 255 / 100,
    /* 13  Cyan*    */  33 * 255 / 100, 60 * 255 / 100, 60 * 255 / 100,
    /* 14  Yellow*  */  60 * 255 / 100, 60 * 255 / 100, 33 * 255 / 100,
    /* 15  Gray 75% */  80 * 255 / 100, 80 * 255 / 100, 80 * 255 / 100,
    /* 0   Black    */   0 * 255 / 100,  0 * 255 / 100,  0 * 255 / 100,
};


/* create dither context object */
SIXELAPI SIXELSTATUS
sixel_dither_new(
    sixel_dither_t    /* out */ **ppdither, /* dither object to be created */
    int               /* in */  ncolors,    /* required colors */
    sixel_allocator_t /* in */  *allocator) /* allocator, null if you use
                                               default allocator */
{
    SIXELSTATUS status = SIXEL_FALSE;
    size_t headsize;
    size_t palette_bytes;
    int quality_mode;
    sixel_palette_t *palette;

    /* ensure given pointer is not null */
    if (ppdither == NULL) {
        sixel_helper_set_additional_message(
            "sixel_dither_new: ppdither is null.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    *ppdither = NULL;

    if (allocator == NULL) {
        status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
        if (SIXEL_FAILED(status)) {
            *ppdither = NULL;
            goto end;
        }
    } else {
        sixel_allocator_ref(allocator);
    }

    if (ncolors < 0) {
        ncolors = SIXEL_PALETTE_MAX;
        quality_mode = SIXEL_QUALITY_HIGHCOLOR;
    } else {
        if (ncolors > SIXEL_PALETTE_MAX) {
            status = SIXEL_BAD_INPUT;
            goto end;
        } else if (ncolors < 1) {
            status = SIXEL_BAD_INPUT;
            sixel_helper_set_additional_message(
                "sixel_dither_new: palette colors must be more than 0");
            goto end;
        }
        quality_mode = SIXEL_QUALITY_LOW;
    }
    headsize = sizeof(sixel_dither_t);
    palette_bytes = 0U;

    *ppdither = (sixel_dither_t *)sixel_allocator_malloc(allocator, headsize);
    if (*ppdither == NULL) {
        sixel_allocator_unref(allocator);
        sixel_helper_set_additional_message(
            "sixel_dither_new: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    (*ppdither)->ref = 1U;
    (*ppdither)->palette = NULL;
    (*ppdither)->reqcolors = ncolors;
    (*ppdither)->force_palette = 0;
    (*ppdither)->ncolors = ncolors;
    (*ppdither)->origcolors = (-1);
    (*ppdither)->keycolor = (-1);
    sixel_dither_clear_transparent_bgcolor_hint(*ppdither);
    (*ppdither)->optimized = 0;
    (*ppdither)->optimize_palette = 0;
    (*ppdither)->complexion = 1;
    (*ppdither)->bodyonly = 0;
    (*ppdither)->method_for_largest = SIXEL_LARGE_NORM;
    (*ppdither)->method_for_rep = SIXEL_REP_CENTER_BOX;
    (*ppdither)->method_for_diffuse = SIXEL_DIFFUSE_FS;
    (*ppdither)->method_for_scan = SIXEL_SCAN_AUTO;
    (*ppdither)->interframe_strategy_override = 0;
    (*ppdither)->interframe_strategy_token
        = SIXEL_INTERFRAME_STRATEGY_TOKEN_NONE;
    (*ppdither)->interframe_spatial_diffuse_override = 0;
    (*ppdither)->interframe_spatial_diffuse = SIXEL_DIFFUSE_FS;
    (*ppdither)->interframe_noise_strength_override = 0;
    (*ppdither)->interframe_noise_strength_u8 = 0;
    (*ppdither)->stbn_motion_adapt_override = 0;
    (*ppdither)->stbn_motion_adapt_enabled = 0;
    (*ppdither)->stbn_scene_cut_reset_override = 0;
    (*ppdither)->stbn_scene_cut_reset_enabled = 0;
    (*ppdither)->stbn_scene_detect_override = 0;
    (*ppdither)->stbn_scene_detect_enabled = 0;
    (*ppdither)->stbn_alpha_guard_override = 0;
    (*ppdither)->stbn_alpha_guard_enabled = 0;
    (*ppdither)->stbn_perceptual_weight_override = 0;
    (*ppdither)->stbn_perceptual_weight_enabled = 0;
    (*ppdither)->stbn_fastpath_override = 0;
    (*ppdither)->stbn_fastpath_enabled = 0;
    (*ppdither)->bluenoise_strength_override = 0;
    (*ppdither)->bluenoise_strength = 0.055f;
    (*ppdither)->bluenoise_phase_override = 0;
    (*ppdither)->bluenoise_phase_x = 0;
    (*ppdither)->bluenoise_phase_y = 0;
    (*ppdither)->bluenoise_seed_override = 0;
    (*ppdither)->bluenoise_seed = 0;
    (*ppdither)->bluenoise_channel_override = 0;
    (*ppdither)->bluenoise_channel_rgb = 0;
    (*ppdither)->bluenoise_size_override = 0;
    (*ppdither)->bluenoise_size = 64;
    (*ppdither)->bluenoise_gradient_factor_override = 0;
    (*ppdither)->bluenoise_gradient_factor = 0.0f;
    (*ppdither)->quality_mode = quality_mode;
    (*ppdither)->requested_quality_mode = quality_mode;
    (*ppdither)->pixelformat = SIXEL_PIXELFORMAT_RGB888;
    (*ppdither)->prefer_float32 = 0;
    (*ppdither)->allocator = allocator;
    (*ppdither)->lut_policy = SIXEL_LUT_POLICY_AUTO;
    (*ppdither)->sixel_reversible = 0;
    (*ppdither)->quantize_model = SIXEL_QUANTIZE_MODEL_AUTO;
    (*ppdither)->final_merge_mode = SIXEL_FINAL_MERGE_AUTO;
    (*ppdither)->pipeline_row_callback = NULL;
    (*ppdither)->pipeline_row_priv = NULL;
    (*ppdither)->pipeline_index_buffer = NULL;
    (*ppdither)->pipeline_index_size = 0;
    (*ppdither)->pipeline_index_owned = 0;
    (*ppdither)->pipeline_parallel_active = 0;
    (*ppdither)->pipeline_band_height = 0;
    (*ppdither)->pipeline_band_overlap = 0;
    (*ppdither)->pipeline_dither_threads = 0;
    (*ppdither)->pipeline_pin_threads = 1;
    (*ppdither)->pipeline_image_width = 0;
    (*ppdither)->pipeline_image_height = 0;
    sixel_dither_clear_pipeline_transparent_mask_hint(*ppdither);
    (*ppdither)->bluenoise_gradient_map = NULL;
    (*ppdither)->bluenoise_gradient_map_size = 0U;
    (*ppdither)->bluenoise_gradient_width = 0;
    (*ppdither)->bluenoise_gradient_height = 0;
    (*ppdither)->pipeline_logger = NULL;
    sixel_dither_interframe_state_init(*ppdither);

    status = sixel_palette_new(&(*ppdither)->palette, allocator);
    if (SIXEL_FAILED(status)) {
        sixel_allocator_free(allocator, *ppdither);
        *ppdither = NULL;
        goto end;
    }

    palette = (*ppdither)->palette;
    palette->requested_colors = (unsigned int)ncolors;
    palette->quality_mode = quality_mode;
    palette->force_palette = 0;
    palette->lut_policy = SIXEL_LUT_POLICY_AUTO;

    status = sixel_palette_resize(palette,
                                  (unsigned int)ncolors,
                                  3,
                                  allocator);
    if (SIXEL_FAILED(status)) {
        sixel_palette_unref(palette);
        (*ppdither)->palette = NULL;
        sixel_allocator_free(allocator, *ppdither);
        *ppdither = NULL;
        goto end;
    }
    /*
     * Ensure palette entries are fully initialized before any lookup
     * path reads them under MSan instrumentation.
     */
    palette_bytes = palette->entries_size;
    if (palette->entries != NULL && palette_bytes > 0U) {
        memset(palette->entries, 0, palette_bytes);
    }

    status = SIXEL_OK;

end:
    if (SIXEL_FAILED(status)) {
        sixel_allocator_unref(allocator);
    }
    return status;
}


/* create dither context object (deprecated) */
SIXELAPI sixel_dither_t *
sixel_dither_create(
    int     /* in */ ncolors)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_dither_t *dither = NULL;

    status = sixel_dither_new(&dither, ncolors, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

end:
    return dither;
}


SIXELAPI void
sixel_dither_destroy(
    sixel_dither_t  /* in */ *dither)
{
    sixel_allocator_t *allocator;

    if (dither) {
        allocator = dither->allocator;
        if (dither->palette != NULL) {
            sixel_palette_unref(dither->palette);
            dither->palette = NULL;
        }
        sixel_dither_clear_bluenoise_gradient_map_hint(dither);
        sixel_dither_interframe_state_dispose(dither);
        sixel_allocator_free(allocator, dither);
        sixel_allocator_unref(allocator);
    }
}


SIXELAPI void
sixel_dither_ref(
    sixel_dither_t  /* in */ *dither)
{
    if (dither == NULL) {
        return;
    }

    (void)sixel_atomic_fetch_add_u32(&dither->ref, 1U);
}


SIXELAPI void
sixel_dither_unref(
    sixel_dither_t  /* in */ *dither)
{
    unsigned int previous;

    if (dither == NULL) {
        return;
    }

    previous = sixel_atomic_fetch_sub_u32(&dither->ref, 1U);
    if (previous == 1U) {
        sixel_dither_destroy(dither);
    }
}


SIXELAPI sixel_dither_t *
sixel_dither_get(
    int     /* in */ builtin_dither)
{
    SIXELSTATUS status = SIXEL_FALSE;
    unsigned char *palette;
    int ncolors;
    int keycolor;
    sixel_dither_t *dither = NULL;

    switch (builtin_dither) {
    case SIXEL_BUILTIN_MONO_DARK:
        ncolors = 2;
        palette = (unsigned char *)pal_mono_dark;
        keycolor = 0;
        break;
    case SIXEL_BUILTIN_MONO_LIGHT:
        ncolors = 2;
        palette = (unsigned char *)pal_mono_light;
        keycolor = 0;
        break;
    case SIXEL_BUILTIN_XTERM16:
        ncolors = 16;
        palette = (unsigned char *)pal_xterm256;
        keycolor = (-1);
        break;
    case SIXEL_BUILTIN_XTERM256:
        ncolors = 256;
        palette = (unsigned char *)pal_xterm256;
        keycolor = (-1);
        break;
    case SIXEL_BUILTIN_VT340_MONO:
        ncolors = 16;
        palette = (unsigned char *)pal_vt340_mono;
        keycolor = (-1);
        break;
    case SIXEL_BUILTIN_VT340_COLOR:
        ncolors = 16;
        palette = (unsigned char *)pal_vt340_color;
        keycolor = (-1);
        break;
    case SIXEL_BUILTIN_G1:
        ncolors = 2;
        palette = (unsigned char *)pal_gray_1bit;
        keycolor = (-1);
        break;
    case SIXEL_BUILTIN_G2:
        ncolors = 4;
        palette = (unsigned char *)pal_gray_2bit;
        keycolor = (-1);
        break;
    case SIXEL_BUILTIN_G4:
        ncolors = 16;
        palette = (unsigned char *)pal_gray_4bit;
        keycolor = (-1);
        break;
    case SIXEL_BUILTIN_G8:
        ncolors = 256;
        palette = (unsigned char *)pal_gray_8bit;
        keycolor = (-1);
        break;
    default:
        goto end;
    }

    status = sixel_dither_new(&dither, ncolors, NULL);
    if (SIXEL_FAILED(status)) {
        dither = NULL;
        goto end;
    }
    if (dither == NULL) {
        goto end;
    }

    status = sixel_palette_set_entries(dither->palette,
                                       palette,
                                       (unsigned int)ncolors,
                                       3,
                                       dither->allocator);
    if (SIXEL_FAILED(status)) {
        sixel_dither_unref(dither);
        dither = NULL;
        goto end;
    }
    dither->palette->requested_colors = (unsigned int)ncolors;
    dither->palette->entry_count = (unsigned int)ncolors;
    dither->palette->depth = 3;
    dither->keycolor = keycolor;
    dither->optimized = 1;
    dither->optimize_palette = 0;

end:
    return dither;
}


static void
sixel_dither_set_method_for_largest(
    sixel_dither_t  /* in */ *dither,
    int             /* in */ method_for_largest)
{
    if (method_for_largest == SIXEL_LARGE_AUTO) {
        method_for_largest = SIXEL_LARGE_NORM;
    }
    dither->method_for_largest = method_for_largest;
}


static void
sixel_dither_set_method_for_rep(
    sixel_dither_t  /* in */ *dither,
    int             /* in */ method_for_rep)
{
    if (method_for_rep == SIXEL_REP_AUTO) {
        method_for_rep = SIXEL_REP_CENTER_BOX;
    }
    dither->method_for_rep = method_for_rep;
}


static void
sixel_dither_set_quality_mode(
    sixel_dither_t  /* in */  *dither,
    int             /* in */  quality_mode)
{
    dither->requested_quality_mode = quality_mode;

    if (quality_mode == SIXEL_QUALITY_AUTO) {
        if (dither->ncolors <= 8) {
            quality_mode = SIXEL_QUALITY_HIGH;
        } else {
            quality_mode = SIXEL_QUALITY_LOW;
        }
    }
    dither->quality_mode = quality_mode;
}


SIXELAPI SIXELSTATUS
sixel_dither_initialize(
    sixel_dither_t  /* in */ *dither,
    unsigned char   /* in */ *data,
    int             /* in */ width,
    int             /* in */ height,
    int             /* in */ pixelformat,
    int             /* in */ method_for_largest,
    int             /* in */ method_for_rep,
    int             /* in */ quality_mode)
{
    unsigned char *buf = NULL;
    unsigned char *normalized_pixels = NULL;
    unsigned char *alpha_pixels = NULL;
    float *float_pixels = NULL;
    unsigned char *input_pixels;
    SIXELSTATUS status = SIXEL_FALSE;
    size_t total_pixels;
    unsigned int payload_length;
    int palette_pixelformat;
    int prefer_float32;

    /* ensure dither object is not null */
    if (dither == NULL) {
        sixel_helper_set_additional_message(
            "sixel_dither_new: dither is null.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    /* increment ref count */
    sixel_dither_ref(dither);

    sixel_dither_set_pixelformat(dither, pixelformat);

    /* keep quantizer policy in sync with the dither object */
    sixel_palette_set_lut_policy(dither->lut_policy);

    input_pixels = NULL;
    total_pixels = (size_t)width * (size_t)height;
    payload_length = 0U;
    palette_pixelformat = SIXEL_PIXELFORMAT_RGB888;
    prefer_float32 = dither->prefer_float32;

    /* Float32 input requires the pipeline to honour higher precision. */
    if (SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat)) {
        prefer_float32 = 1;
    }

    switch (pixelformat) {
    case SIXEL_PIXELFORMAT_RGB888:
        input_pixels = data;
        break;
    case SIXEL_PIXELFORMAT_RGBA8888:
    case SIXEL_PIXELFORMAT_ARGB8888:
    case SIXEL_PIXELFORMAT_BGRA8888:
    case SIXEL_PIXELFORMAT_ABGR8888:
    case SIXEL_PIXELFORMAT_GA88:
    case SIXEL_PIXELFORMAT_AG88:
        if (dither->keycolor >= 0) {
            int source_depth;

            source_depth = sixel_helper_compute_depth(pixelformat);
            if (source_depth <= 0) {
                status = SIXEL_BAD_ARGUMENT;
                goto end;
            }
            input_pixels = data;
            palette_pixelformat = pixelformat;
            payload_length = (unsigned int)(total_pixels * (size_t)source_depth);
            if (dither->transparent_bgcolor_valid != 0 && payload_length > 0U) {
                alpha_pixels = (unsigned char *)sixel_allocator_malloc(
                    dither->allocator,
                    payload_length);
                if (alpha_pixels == NULL) {
                    sixel_helper_set_additional_message(
                        "sixel_dither_initialize: alpha blend buffer alloc failed.");
                    status = SIXEL_BAD_ALLOCATION;
                    goto end;
                }
                memcpy(alpha_pixels, data, payload_length);
                status = sixel_dither_preblend_alpha_inplace(
                    alpha_pixels,
                    total_pixels,
                    pixelformat,
                    dither->transparent_bgcolor);
                if (SIXEL_FAILED(status)) {
                    goto end;
                }
                input_pixels = alpha_pixels;
            }
            break;
        }
        /* fallthrough */
    case SIXEL_PIXELFORMAT_RGBFLOAT32:
    case SIXEL_PIXELFORMAT_LINEARRGBFLOAT32:
    case SIXEL_PIXELFORMAT_OKLABFLOAT32:
    case SIXEL_PIXELFORMAT_CIELABFLOAT32:
    case SIXEL_PIXELFORMAT_DIN99DFLOAT32:
        if (prefer_float32) {
            input_pixels = data;
            palette_pixelformat = pixelformat;
            payload_length = (unsigned int)(total_pixels * 3U
                                            * sizeof(float));
            break;
        }
        /* fallthrough */
    default:
        /* normalize pixelformat */
        normalized_pixels
            = (unsigned char *)sixel_allocator_malloc(
                dither->allocator, (size_t)(width * height * 3));
        if (normalized_pixels == NULL) {
            sixel_helper_set_additional_message(
                "sixel_dither_initialize: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }

        status = sixel_helper_normalize_pixelformat(
            normalized_pixels,
            &pixelformat,
            data,
            pixelformat,
            width,
            height);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        input_pixels = normalized_pixels;
        break;
    }

    if (payload_length == 0U) {
        payload_length = (unsigned int)(total_pixels * 3U);
    }

    if (prefer_float32
        && !SIXEL_PIXELFORMAT_IS_FLOAT32(palette_pixelformat)
        && total_pixels > 0U) {
        status = sixel_dither_promote_rgb888_to_float32(
            &float_pixels,
            input_pixels,
            total_pixels,
            dither->allocator);
        if (SIXEL_SUCCEEDED(status) && float_pixels != NULL) {
            payload_length
                = (unsigned int)(total_pixels * 3U * sizeof(float));
            palette_pixelformat = SIXEL_PIXELFORMAT_RGBFLOAT32;
            input_pixels = (unsigned char *)float_pixels;
        } else {
            prefer_float32 = 0;
            status = SIXEL_OK;
        }
    }

    dither->prefer_float32 = prefer_float32;

    sixel_dither_set_method_for_largest(dither, method_for_largest);
    sixel_dither_set_method_for_rep(dither, method_for_rep);
    sixel_dither_set_quality_mode(dither, quality_mode);

    status = sixel_palette_make_palette(&buf,
                                        input_pixels,
                                        payload_length,
                                        palette_pixelformat,
                                        (unsigned int)dither->reqcolors,
                                        (unsigned int *)&dither->ncolors,
                                        (unsigned int *)&dither->origcolors,
                                        dither->method_for_largest,
                                        dither->method_for_rep,
                                        dither->quality_mode,
                                        dither->force_palette,
                                        dither->sixel_reversible,
                                        dither->quantize_model,
                                        dither->final_merge_mode,
                                        dither->prefer_float32,
                                        dither->allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_palette_set_entries(dither->palette,
                                       buf,
                                       (unsigned int)dither->ncolors,
                                       3,
                                       dither->allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    dither->palette->entry_count = (unsigned int)dither->ncolors;
    dither->palette->requested_colors = (unsigned int)dither->reqcolors;
    dither->palette->original_colors = (unsigned int)dither->origcolors;
    dither->palette->depth = 3;

    dither->optimized = 1;
    if (dither->origcolors <= dither->ncolors) {
        dither->method_for_diffuse = SIXEL_DIFFUSE_NONE;
    }

    sixel_palette_free_palette(buf, dither->allocator);
    status = SIXEL_OK;

end:
    if (normalized_pixels != NULL) {
        sixel_allocator_free(dither->allocator, normalized_pixels);
    }
    if (alpha_pixels != NULL) {
        sixel_allocator_free(dither->allocator, alpha_pixels);
    }
    if (float_pixels != NULL) {
        sixel_allocator_free(dither->allocator, float_pixels);
    }

    /* decrement ref count */
    sixel_dither_unref(dither);

    return status;
}


/* set lookup table policy */
SIXELAPI void
sixel_dither_set_lut_policy(
    sixel_dither_t  /* in */ *dither,
    int             /* in */ lut_policy)
{
    int normalized;
    int previous_policy;

    if (dither == NULL) {
        return;
    }

    normalized = SIXEL_LUT_POLICY_AUTO;
    if (lut_policy == SIXEL_LUT_POLICY_5BIT
        || lut_policy == SIXEL_LUT_POLICY_6BIT
        || lut_policy == SIXEL_LUT_POLICY_CERTLUT
        || lut_policy == SIXEL_LUT_POLICY_EYTZINGER
        || lut_policy == SIXEL_LUT_POLICY_NONE
        || lut_policy == SIXEL_LUT_POLICY_FHEDT
        || lut_policy == SIXEL_LUT_POLICY_VPTREE
        || lut_policy == SIXEL_LUT_POLICY_RBC
        || lut_policy == SIXEL_LUT_POLICY_MAHALANOBIS) {
        normalized = lut_policy;
    }
    previous_policy = dither->lut_policy;
    if (previous_policy == normalized) {
        return;
    }

    /*
     * Policy transitions for the shared LUT mirror the previous cache flow:
     *
     *   [lut] --policy change--> (drop) --rebuild--> [lut]
     */
    dither->lut_policy = normalized;
    if (dither->palette != NULL) {
        dither->palette->lut_policy = normalized;
    }
    if (dither->palette != NULL && dither->palette->lut != NULL) {
        sixel_lut_unref(dither->palette->lut);
        dither->palette->lut = NULL;
    }
}


/* get lookup table policy */
SIXELAPI int
sixel_dither_get_lut_policy(
    sixel_dither_t  /* in */ *dither)
{
    int policy;

    policy = SIXEL_LUT_POLICY_AUTO;
    if (dither != NULL) {
        policy = dither->lut_policy;
    }

    return policy;
}


/* set diffusion type, choose from enum methodForDiffuse */
SIXELAPI void
sixel_dither_set_diffusion_type(
    sixel_dither_t  /* in */ *dither,
    int             /* in */ method_for_diffuse)
{
    if (method_for_diffuse == SIXEL_DIFFUSE_AUTO) {
        if (dither->ncolors > 16) {
            method_for_diffuse = SIXEL_DIFFUSE_FS;
        } else {
            method_for_diffuse = SIXEL_DIFFUSE_ATKINSON;
        }
    }
    dither->method_for_diffuse = method_for_diffuse;
}


/* set scan order for diffusion */
SIXELAPI void
sixel_dither_set_diffusion_scan(
    sixel_dither_t  /* in */ *dither,
    int             /* in */ method_for_scan)
{
    if (method_for_scan != SIXEL_SCAN_AUTO &&
            method_for_scan != SIXEL_SCAN_RASTER &&
            method_for_scan != SIXEL_SCAN_SERPENTINE) {
        method_for_scan = SIXEL_SCAN_RASTER;
    }
    dither->method_for_scan = method_for_scan;
}


/* get number of palette colors */
SIXELAPI int
sixel_dither_get_num_of_palette_colors(
    sixel_dither_t  /* in */ *dither)
{
    return dither->ncolors;
}


/* get number of histogram colors */
SIXELAPI int
sixel_dither_get_num_of_histogram_colors(
    sixel_dither_t /* in */ *dither)  /* dither context object */
{
    return dither->origcolors;
}


/* typoed: remained for keeping compatibility */
SIXELAPI int
sixel_dither_get_num_of_histgram_colors(
    sixel_dither_t /* in */ *dither)  /* dither context object */
{
    return sixel_dither_get_num_of_histogram_colors(dither);
}


/* get palette */
SIXELAPI unsigned char *
sixel_dither_get_palette(
    sixel_dither_t /* in */ *dither)  /* dither context object */
{
    if (dither == NULL || dither->palette == NULL) {
        return NULL;
    }

    return dither->palette->entries;
}

SIXELAPI SIXELSTATUS
sixel_dither_get_quantized_palette(sixel_dither_t *dither,
                                   sixel_palette_t **pppalette)
{
    if (pppalette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *pppalette = NULL;

    if (dither == NULL || dither->palette == NULL) {
        return SIXEL_RUNTIME_ERROR;
    }

    sixel_palette_ref(dither->palette);
    *pppalette = dither->palette;

    return SIXEL_OK;
}


/* set palette */
SIXELAPI void
sixel_dither_set_palette(
    sixel_dither_t /* in */ *dither,   /* dither context object */
    unsigned char  /* in */ *palette)
{
    if (dither == NULL || dither->palette == NULL) {
        return;
    }

    (void)sixel_palette_set_entries(dither->palette,
                                    palette,
                                    (unsigned int)dither->ncolors,
                                    3,
                                    dither->allocator);
}


/* set the factor of complexion color correcting */
SIXELAPI void
sixel_dither_set_complexion_score(
    sixel_dither_t /* in */ *dither,  /* dither context object */
    int            /* in */ score)    /* complexion score (>= 1) */
{
    dither->complexion = score;
    if (dither->palette != NULL) {
        dither->palette->complexion = score;
    }
}


/* set whether omitting palette definition */
SIXELAPI void
sixel_dither_set_body_only(
    sixel_dither_t /* in */ *dither,     /* dither context object */
    int            /* in */ bodyonly)    /* 0: output palette section
                                            1: do not output palette section  */
{
    dither->bodyonly = bodyonly;
}


/* set whether optimize palette size */
SIXELAPI void
sixel_dither_set_optimize_palette(
    sixel_dither_t /* in */ *dither,   /* dither context object */
    int            /* in */ do_opt)    /* 0: optimize palette size
                                          1: don't optimize palette size */
{
    dither->optimize_palette = do_opt;
}


/* set pixelformat */
SIXELAPI void
sixel_dither_set_pixelformat(
    sixel_dither_t /* in */ *dither,     /* dither context object */
    int            /* in */ pixelformat) /* one of enum pixelFormat */
{
    /* Keep the float32 preference aligned with the requested pixelformat. */
    dither->pixelformat = pixelformat;
    dither->prefer_float32 =
        SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat) ? 1 : 0;
}


/* toggle SIXEL reversible palette mode */
SIXELAPI void
sixel_dither_set_sixel_reversible(
    sixel_dither_t /* in */ *dither,
    int            /* in */ enable)
{
    /*
     * The diagram below shows how the flag routes palette generation:
     *
     *   pixels --> [histogram]
     *                  |
     *                  v
     *           (optional reversible snap)
     *                  |
     *                  v
     *               palette
     */
    if (dither == NULL) {
        return;
    }
    dither->sixel_reversible = enable ? 1 : 0;
    if (dither->palette != NULL) {
        dither->palette->sixel_reversible = dither->sixel_reversible;
    }
}

/* select final merge policy */
SIXELAPI void
sixel_dither_set_final_merge(
    sixel_dither_t /* in */ *dither,
    int            /* in */ final_merge)
{
    int mode;

    if (dither == NULL) {
        return;
    }
    mode = SIXEL_FINAL_MERGE_AUTO;
    if (final_merge == SIXEL_FINAL_MERGE_NONE
        || final_merge == SIXEL_FINAL_MERGE_WARD) {
        mode = final_merge;
    } else if (final_merge == SIXEL_FINAL_MERGE_AUTO) {
        mode = SIXEL_FINAL_MERGE_AUTO;
    }
    dither->final_merge_mode = mode;
    if (dither->palette != NULL) {
        dither->palette->final_merge = mode;
    }
}

/* set transparent */
SIXELAPI void
sixel_dither_set_transparent(
    sixel_dither_t /* in */ *dither,      /* dither context object */
    int            /* in */ transparent)  /* transparent color index */
{
    if (dither == NULL) {
        return;
    }
    dither->keycolor = transparent;
}

void
sixel_dither_clear_transparent_bgcolor_hint(
    sixel_dither_t *dither)
{
    if (dither == NULL) {
        return;
    }

    dither->transparent_bgcolor[0] = 0U;
    dither->transparent_bgcolor[1] = 0U;
    dither->transparent_bgcolor[2] = 0U;
    dither->transparent_bgcolor_valid = 0;
}

void
sixel_dither_set_transparent_bgcolor_hint(
    sixel_dither_t *dither,
    unsigned char const *bgcolor)
{
    if (dither == NULL) {
        return;
    }

    if (bgcolor != NULL) {
        dither->transparent_bgcolor[0] = bgcolor[0];
        dither->transparent_bgcolor[1] = bgcolor[1];
        dither->transparent_bgcolor[2] = bgcolor[2];
    } else {
        dither->transparent_bgcolor[0] = 0U;
        dither->transparent_bgcolor[1] = 0U;
        dither->transparent_bgcolor[2] = 0U;
    }
    dither->transparent_bgcolor_valid = 1;
}

void
sixel_dither_clear_pipeline_transparent_mask_hint(
    sixel_dither_t *dither)
{
    if (dither == NULL) {
        return;
    }

    dither->pipeline_transparent_mask = NULL;
    dither->pipeline_transparent_mask_size = 0u;
    dither->pipeline_transparent_keycolor = (-1);
}

SIXEL_INTERNAL_API void
sixel_dither_clear_bluenoise_gradient_map_hint(
    sixel_dither_t *dither)
{
    if (dither == NULL) {
        return;
    }

    if (dither->bluenoise_gradient_map != NULL && dither->allocator != NULL) {
        sixel_allocator_free(dither->allocator,
                             dither->bluenoise_gradient_map);
    }
    dither->bluenoise_gradient_map = NULL;
    dither->bluenoise_gradient_map_size = 0U;
    dither->bluenoise_gradient_width = 0;
    dither->bluenoise_gradient_height = 0;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_dither_set_bluenoise_gradient_map_hint(
    sixel_dither_t *dither,
    unsigned char *gradient_map,
    size_t gradient_map_size,
    int width,
    int height)
{
    size_t expected_size;

    expected_size = 0U;

    if (dither == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    sixel_dither_clear_bluenoise_gradient_map_hint(dither);
    if (gradient_map == NULL
            || gradient_map_size == 0U
            || width <= 0
            || height <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    if ((size_t)width > SIZE_MAX / (size_t)height) {
        return SIXEL_BAD_INPUT;
    }
    expected_size = (size_t)width * (size_t)height;
    if (gradient_map_size < expected_size) {
        return SIXEL_BAD_INPUT;
    }

    dither->bluenoise_gradient_map = gradient_map;
    dither->bluenoise_gradient_map_size = gradient_map_size;
    dither->bluenoise_gradient_width = width;
    dither->bluenoise_gradient_height = height;

    return SIXEL_OK;
}

void
sixel_dither_set_pipeline_transparent_mask_hint(
    sixel_dither_t *dither,
    unsigned char const *transparent_mask,
    size_t transparent_mask_size,
    int keycolor)
{
    if (dither == NULL) {
        return;
    }

    sixel_dither_clear_pipeline_transparent_mask_hint(dither);
    if (transparent_mask == NULL || transparent_mask_size == 0u) {
        return;
    }
    if (keycolor < 0 || keycolor >= SIXEL_PALETTE_MAX) {
        return;
    }

    dither->pipeline_transparent_mask = transparent_mask;
    dither->pipeline_transparent_mask_size = transparent_mask_size;
    dither->pipeline_transparent_keycolor = keycolor;
}

static void
sixel_dither_interframe_state_init(sixel_dither_t *dither)
{
    if (dither == NULL) {
        return;
    }

    dither->frame_context.frame_no = 0;
    dither->frame_context.loop_no = 0;
    dither->frame_context.multiframe = 0;
    dither->frame_context.valid = 0;

    dither->interframe_state.error_frame = NULL;
    dither->interframe_state.error_frame_size = 0U;
    dither->interframe_state.width = 0;
    dither->interframe_state.height = 0;
    dither->interframe_state.depth = 0;
    dither->interframe_state.method_id = SIXEL_INTERFRAME_METHOD_NONE;
    dither->interframe_state.method_private = NULL;
    dither->interframe_state.method_private_size = 0U;
    dither->interframe_state.apply_count = 0UL;
    dither->interframe_state.consume_count = 0UL;
    dither->interframe_state.last_apply_status = SIXEL_FALSE;
    dither->interframe_state.last_apply_consumed = 0;
}

static void
sixel_dither_interframe_state_reset(sixel_dither_t *dither)
{
    if (dither == NULL) {
        return;
    }

    sixel_interframe_release_shared_frame(dither);
    dither->interframe_state.last_apply_status = SIXEL_FALSE;
    dither->interframe_state.last_apply_consumed = 0;
}

static void
sixel_dither_interframe_state_dispose(sixel_dither_t *dither)
{
    if (dither == NULL) {
        return;
    }

    sixel_dither_interframe_state_reset(dither);
    dither->frame_context.frame_no = 0;
    dither->frame_context.loop_no = 0;
    dither->frame_context.multiframe = 0;
    dither->frame_context.valid = 0;
    dither->interframe_state.apply_count = 0UL;
    dither->interframe_state.consume_count = 0UL;
}

SIXEL_INTERNAL_API void
sixel_dither_clear_frame_context(sixel_dither_t *dither)
{
    if (dither == NULL) {
        return;
    }

    dither->frame_context.frame_no = 0;
    dither->frame_context.loop_no = 0;
    dither->frame_context.multiframe = 0;
    dither->frame_context.valid = 0;
    sixel_dither_interframe_state_reset(dither);
}

SIXEL_INTERNAL_API void
sixel_dither_set_frame_context(sixel_dither_t *dither,
                               int frame_no,
                               int loop_no,
                               int multiframe)
{
    int needs_reset;

    if (dither == NULL) {
        return;
    }

    needs_reset = 0;
    if (dither->frame_context.valid == 0) {
        needs_reset = 1;
    } else if (dither->frame_context.multiframe == 0 || multiframe == 0) {
        needs_reset = 1;
    } else if (dither->frame_context.loop_no != loop_no) {
        needs_reset = 1;
    } else if (frame_no == 0 && dither->frame_context.frame_no != 0) {
        needs_reset = 1;
    }

    dither->frame_context.frame_no = frame_no;
    dither->frame_context.loop_no = loop_no;
    dither->frame_context.multiframe = multiframe;
    dither->frame_context.valid = 1;

    if (needs_reset) {
        /*
         * Interframe buffers are reserved for future diffusion modes. Reset
         * state at timeline boundaries so capture and encode paths stay in
         * sync even before interframe diffusion is enabled.
         */
        sixel_dither_interframe_state_reset(dither);
    }
}

static int
sixel_dither_interframe_resolve_apply_mode(int apply_mode)
{
    if (apply_mode == SIXEL_DITHER_APPLY_PRESERVE_INTERFRAME_STATE) {
        return SIXEL_DITHER_APPLY_PRESERVE_INTERFRAME_STATE;
    }
    return SIXEL_DITHER_APPLY_CONSUME_INTERFRAME_STATE;
}

static void
sixel_dither_interframe_begin_apply(sixel_dither_t *dither,
                                  int apply_mode)
{
    if (dither == NULL) {
        return;
    }

    if (apply_mode == SIXEL_DITHER_APPLY_CONSUME_INTERFRAME_STATE) {
        dither->interframe_state.apply_count += 1UL;
    }
    dither->interframe_state.last_apply_consumed =
        (apply_mode == SIXEL_DITHER_APPLY_CONSUME_INTERFRAME_STATE) ? 1 : 0;
}

static void
sixel_dither_interframe_finish_apply(sixel_dither_t *dither,
                                   int apply_mode,
                                   SIXELSTATUS status)
{
    if (dither == NULL) {
        return;
    }

    dither->interframe_state.last_apply_status = status;
    if (status == SIXEL_OK
            && apply_mode == SIXEL_DITHER_APPLY_CONSUME_INTERFRAME_STATE) {
        dither->interframe_state.consume_count += 1UL;
    }
}

static void
sixel_dither_cleanup_apply_hints(sixel_dither_t *dither)
{
    if (dither == NULL) {
        return;
    }

    dither->pipeline_index_buffer = NULL;
    dither->pipeline_index_owned = 0;
    dither->pipeline_index_size = 0;
    dither->pipeline_parallel_active = 0;
    dither->pipeline_band_height = 0;
    dither->pipeline_band_overlap = 0;
    dither->pipeline_dither_threads = 0;
    dither->pipeline_image_width = 0;
    dither->pipeline_image_height = 0;
    sixel_dither_clear_pipeline_transparent_mask_hint(dither);
    sixel_dither_clear_bluenoise_gradient_map_hint(dither);
    dither->pipeline_logger = NULL;
}

static int
sixel_dither_pixelformat_has_alpha(int pixelformat)
{
    switch (pixelformat) {
    case SIXEL_PIXELFORMAT_RGBA8888:
    case SIXEL_PIXELFORMAT_ARGB8888:
    case SIXEL_PIXELFORMAT_BGRA8888:
    case SIXEL_PIXELFORMAT_ABGR8888:
    case SIXEL_PIXELFORMAT_GA88:
    case SIXEL_PIXELFORMAT_AG88:
        return 1;
    default:
        return 0;
    }
}

static unsigned char
sixel_dither_extract_alpha_u8(unsigned char const *pixel, int pixelformat)
{
    switch (pixelformat) {
    case SIXEL_PIXELFORMAT_RGBA8888:
    case SIXEL_PIXELFORMAT_BGRA8888:
        return pixel[3];
    case SIXEL_PIXELFORMAT_GA88:
        return pixel[1];
    case SIXEL_PIXELFORMAT_ARGB8888:
    case SIXEL_PIXELFORMAT_ABGR8888:
        return pixel[0];
    case SIXEL_PIXELFORMAT_AG88:
        return pixel[0];
    default:
        return 0xffU;
    }
}

static unsigned char
sixel_dither_blend_channel_u8(unsigned char source,
                              unsigned char background,
                              unsigned char alpha)
{
    unsigned int blended;

    blended = (unsigned int)source * (unsigned int)alpha
            + (unsigned int)background * (255U - (unsigned int)alpha)
            + 127U;

    return (unsigned char)(blended / 255U);
}

static unsigned char
sixel_dither_background_gray_u8(unsigned char const *background)
{
    unsigned int luma;

    luma = 299U * (unsigned int)background[0]
         + 587U * (unsigned int)background[1]
         + 114U * (unsigned int)background[2]
         + 500U;

    return (unsigned char)(luma / 1000U);
}

static SIXELSTATUS
sixel_dither_preblend_alpha_inplace(unsigned char *pixels,
                                    size_t total_pixels,
                                    int pixelformat,
                                    unsigned char const *background)
{
    size_t index;
    unsigned char alpha;
    unsigned char bg_gray;
    unsigned char *pixel;

    if (pixels == NULL || background == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    index = 0U;
    bg_gray = sixel_dither_background_gray_u8(background);
    pixel = pixels;
    switch (pixelformat) {
    case SIXEL_PIXELFORMAT_RGBA8888:
        for (index = 0U; index < total_pixels; ++index) {
            alpha = pixel[3];
            if (alpha != 0xffU) {
                pixel[0] = sixel_dither_blend_channel_u8(pixel[0],
                                                         background[0],
                                                         alpha);
                pixel[1] = sixel_dither_blend_channel_u8(pixel[1],
                                                         background[1],
                                                         alpha);
                pixel[2] = sixel_dither_blend_channel_u8(pixel[2],
                                                         background[2],
                                                         alpha);
            }
            pixel += 4;
        }
        return SIXEL_OK;
    case SIXEL_PIXELFORMAT_ARGB8888:
        for (index = 0U; index < total_pixels; ++index) {
            alpha = pixel[0];
            if (alpha != 0xffU) {
                pixel[1] = sixel_dither_blend_channel_u8(pixel[1],
                                                         background[0],
                                                         alpha);
                pixel[2] = sixel_dither_blend_channel_u8(pixel[2],
                                                         background[1],
                                                         alpha);
                pixel[3] = sixel_dither_blend_channel_u8(pixel[3],
                                                         background[2],
                                                         alpha);
            }
            pixel += 4;
        }
        return SIXEL_OK;
    case SIXEL_PIXELFORMAT_BGRA8888:
        for (index = 0U; index < total_pixels; ++index) {
            alpha = pixel[3];
            if (alpha != 0xffU) {
                pixel[0] = sixel_dither_blend_channel_u8(pixel[0],
                                                         background[2],
                                                         alpha);
                pixel[1] = sixel_dither_blend_channel_u8(pixel[1],
                                                         background[1],
                                                         alpha);
                pixel[2] = sixel_dither_blend_channel_u8(pixel[2],
                                                         background[0],
                                                         alpha);
            }
            pixel += 4;
        }
        return SIXEL_OK;
    case SIXEL_PIXELFORMAT_ABGR8888:
        for (index = 0U; index < total_pixels; ++index) {
            alpha = pixel[0];
            if (alpha != 0xffU) {
                pixel[1] = sixel_dither_blend_channel_u8(pixel[1],
                                                         background[2],
                                                         alpha);
                pixel[2] = sixel_dither_blend_channel_u8(pixel[2],
                                                         background[1],
                                                         alpha);
                pixel[3] = sixel_dither_blend_channel_u8(pixel[3],
                                                         background[0],
                                                         alpha);
            }
            pixel += 4;
        }
        return SIXEL_OK;
    case SIXEL_PIXELFORMAT_GA88:
        for (index = 0U; index < total_pixels; ++index) {
            alpha = pixel[1];
            if (alpha != 0xffU) {
                pixel[0] = sixel_dither_blend_channel_u8(pixel[0],
                                                         bg_gray,
                                                         alpha);
            }
            pixel += 2;
        }
        return SIXEL_OK;
    case SIXEL_PIXELFORMAT_AG88:
        for (index = 0U; index < total_pixels; ++index) {
            alpha = pixel[0];
            if (alpha != 0xffU) {
                pixel[1] = sixel_dither_blend_channel_u8(pixel[1],
                                                         bg_gray,
                                                         alpha);
            }
            pixel += 2;
        }
        return SIXEL_OK;
    default:
        break;
    }

    return SIXEL_BAD_ARGUMENT;
}

static SIXELSTATUS
sixel_dither_composite_alpha_to_rgb(unsigned char *dst,
                                    unsigned char const *src,
                                    size_t total_pixels,
                                    int pixelformat,
                                    unsigned char const *background)
{
    size_t index;
    unsigned char alpha;
    unsigned char gray;
    unsigned char bg_gray;
    unsigned char const *pixel;
    unsigned char *output;

    if (dst == NULL || src == NULL || background == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    index = 0U;
    bg_gray = sixel_dither_background_gray_u8(background);
    pixel = src;
    output = dst;
    switch (pixelformat) {
    case SIXEL_PIXELFORMAT_RGBA8888:
        for (index = 0U; index < total_pixels; ++index) {
            alpha = pixel[3];
            output[0] = sixel_dither_blend_channel_u8(pixel[0],
                                                      background[0],
                                                      alpha);
            output[1] = sixel_dither_blend_channel_u8(pixel[1],
                                                      background[1],
                                                      alpha);
            output[2] = sixel_dither_blend_channel_u8(pixel[2],
                                                      background[2],
                                                      alpha);
            pixel += 4;
            output += 3;
        }
        return SIXEL_OK;
    case SIXEL_PIXELFORMAT_ARGB8888:
        for (index = 0U; index < total_pixels; ++index) {
            alpha = pixel[0];
            output[0] = sixel_dither_blend_channel_u8(pixel[1],
                                                      background[0],
                                                      alpha);
            output[1] = sixel_dither_blend_channel_u8(pixel[2],
                                                      background[1],
                                                      alpha);
            output[2] = sixel_dither_blend_channel_u8(pixel[3],
                                                      background[2],
                                                      alpha);
            pixel += 4;
            output += 3;
        }
        return SIXEL_OK;
    case SIXEL_PIXELFORMAT_BGRA8888:
        for (index = 0U; index < total_pixels; ++index) {
            alpha = pixel[3];
            output[0] = sixel_dither_blend_channel_u8(pixel[2],
                                                      background[0],
                                                      alpha);
            output[1] = sixel_dither_blend_channel_u8(pixel[1],
                                                      background[1],
                                                      alpha);
            output[2] = sixel_dither_blend_channel_u8(pixel[0],
                                                      background[2],
                                                      alpha);
            pixel += 4;
            output += 3;
        }
        return SIXEL_OK;
    case SIXEL_PIXELFORMAT_ABGR8888:
        for (index = 0U; index < total_pixels; ++index) {
            alpha = pixel[0];
            output[0] = sixel_dither_blend_channel_u8(pixel[3],
                                                      background[0],
                                                      alpha);
            output[1] = sixel_dither_blend_channel_u8(pixel[2],
                                                      background[1],
                                                      alpha);
            output[2] = sixel_dither_blend_channel_u8(pixel[1],
                                                      background[2],
                                                      alpha);
            pixel += 4;
            output += 3;
        }
        return SIXEL_OK;
    case SIXEL_PIXELFORMAT_GA88:
        for (index = 0U; index < total_pixels; ++index) {
            alpha = pixel[1];
            gray = sixel_dither_blend_channel_u8(pixel[0], bg_gray, alpha);
            output[0] = gray;
            output[1] = gray;
            output[2] = gray;
            pixel += 2;
            output += 3;
        }
        return SIXEL_OK;
    case SIXEL_PIXELFORMAT_AG88:
        for (index = 0U; index < total_pixels; ++index) {
            alpha = pixel[0];
            gray = sixel_dither_blend_channel_u8(pixel[1], bg_gray, alpha);
            output[0] = gray;
            output[1] = gray;
            output[2] = gray;
            pixel += 2;
            output += 3;
        }
        return SIXEL_OK;
    default:
        break;
    }

    return SIXEL_BAD_ARGUMENT;
}


/* apply palette with optional interframe-state consumption */
SIXEL_INTERNAL_API sixel_index_t *
sixel_dither_apply_palette_with_mode(
    sixel_dither_t  /* in */ *dither,
    unsigned char   /* in */ *pixels,
    int             /* in */ width,
    int             /* in */ height,
    int             /* in */ apply_mode)
{
    SIXELSTATUS status = SIXEL_FALSE;
    size_t bufsize;
    size_t normalized_size;
    size_t total_pixels;
    sixel_index_t *dest = NULL;
    int ncolors;
    int method_for_scan;
    unsigned char *normalized_pixels = NULL;
    unsigned char *input_pixels;
    float *float_pipeline_pixels = NULL;
    int owns_float_pipeline = 0;
    int pipeline_pixelformat;
    int source_pixelformat;
    int prefer_float_pipeline;
    sixel_palette_t *palette;
    int dest_owned;
    int palette_entry_limit;
    unsigned char *transparent_mask = NULL;
    unsigned char const *preset_transparent_mask;
    size_t preset_transparent_mask_size;
    int preset_transparent_keycolor;
    int using_preset_transparent_mask = 0;
    int apply_transparent_mask = 0;
    int keycolor_for_mask = (-1);
    size_t source_depth;
    size_t index;
    int parallel_active = 0;
    sixel_dither_resolve_indexes_request_t resolve_request;
#if SIXEL_ENABLE_THREADS
    int parallel_band_height = 0;
    int parallel_overlap = 0;
    int parallel_threads = 1;
#endif  /* SIXEL_ENABLE_THREADS */
    sixel_logger_t *logger = NULL;
    int wcomp1;
    int wcomp2;
    int wcomp3;
    int resolved_apply_mode = SIXEL_DITHER_APPLY_CONSUME_INTERFRAME_STATE;
#if SIXEL_ENABLE_THREADS
    int shared_lut;
#endif  /* SIXEL_ENABLE_THREADS */

    /* ensure dither object is not null */
    if (dither == NULL) {
        sixel_helper_set_additional_message(
            "sixel_dither_apply_palette: dither is null.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    resolved_apply_mode =
        sixel_dither_interframe_resolve_apply_mode(apply_mode);
    sixel_dither_ref(dither);
    sixel_dither_interframe_begin_apply(dither, resolved_apply_mode);

    palette = dither->palette;
    if (palette == NULL) {
        sixel_helper_set_additional_message(
            "sixel_dither_apply_palette: palette is null.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    source_pixelformat = dither->pixelformat;
    palette_entry_limit = (int)palette->entry_count;
    source_depth = 0U;
    index = 0U;
    preset_transparent_mask = NULL;
    preset_transparent_mask_size = 0u;
    preset_transparent_keycolor = (-1);
    status = sixel_dither_validate_complexion_limit(3, dither->complexion);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    parallel_active = dither->pipeline_parallel_active;
#if defined(__PCC__)
    /*
     * pcc builds avoid compiler TLS codegen.  The resulting shared LUT
     * context is not thread-safe, so keep palette application serial.
     */
    parallel_active = 0;
#endif
#if SIXEL_ENABLE_THREADS
    parallel_band_height = dither->pipeline_band_height;
    parallel_overlap = dither->pipeline_band_overlap;
    parallel_threads = dither->pipeline_dither_threads;
#endif  /* SIXEL_ENABLE_THREADS */
    logger = dither->pipeline_logger;

    if (!parallel_active && logger != NULL) {
        sixel_logger_logf(logger,
                          "worker",
                          "dither",
                          "start",
                          0,
                          0,
                          0,
                          height,
                          0,
                          height,
                          "serial dither begin height=%d",
                          height);
    }

    if (parallel_active && dither->optimize_palette != 0) {
        /*
         * Palette minimization rewrites the palette entries in place.
         * Parallel bands would race on the shared table, so fall back to
         * the serial path when the feature is active.
         */
        parallel_active = 0;
    }
    if (parallel_active
            && dither->method_for_diffuse == SIXEL_DIFFUSE_INTERFRAME) {
        /*
         * Interframe diffusion keeps frame-wide state. Disable band-parallel
         * dithering until interframe state synchronization is introduced.
         */
        parallel_active = 0;
    }

    /*
     * Force lookup shared flags to initialize before worker threads start.
     * The env helpers use lazy initialization, so touching them here avoids
     * thread sanitizer reports when parallel dither starts.
     */
    (void)sixel_lookup_env_shared_certlut();
    (void)sixel_lookup_env_shared_5bit();
    (void)sixel_lookup_env_shared_6bit();

    /*
     * Inform lookup helpers whether concurrent palette application will run.
     * FHEDT caches rely on this hint when TLS is unavailable so they can
     * disable shared caches during parallel dithering while remaining enabled
     * for serial passes.
     */
    sixel_lookup_set_parallel_dither_active(parallel_active);

    bufsize = (size_t)(width * height) * sizeof(sixel_index_t);
    total_pixels = (size_t)width * (size_t)height;
    preset_transparent_mask = dither->pipeline_transparent_mask;
    preset_transparent_mask_size = dither->pipeline_transparent_mask_size;
    preset_transparent_keycolor = dither->pipeline_transparent_keycolor;
    sixel_dither_clear_pipeline_transparent_mask_hint(dither);

    /*
     * Prefer caller-provided transparency masks when available.
     * This path avoids alpha extraction and keeps precision for
     * non-8bit sources.
     */
    if (preset_transparent_mask != NULL
            && preset_transparent_mask_size >= total_pixels
            && preset_transparent_keycolor >= 0
            && preset_transparent_keycolor < SIXEL_PALETTE_MAX) {
        transparent_mask = (unsigned char *)preset_transparent_mask;
        keycolor_for_mask = preset_transparent_keycolor;
        apply_transparent_mask = 1;
        using_preset_transparent_mask = 1;
    } else if (dither->keycolor >= 0
            && sixel_dither_pixelformat_has_alpha(source_pixelformat)
            && total_pixels > 0U) {
        source_depth = (size_t)sixel_helper_compute_depth(source_pixelformat);
        if (source_depth == 0U) {
            sixel_helper_set_additional_message(
                "sixel_dither_apply_palette: invalid source depth.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        transparent_mask = (unsigned char *)sixel_allocator_malloc(
            dither->allocator,
            total_pixels);
        if (transparent_mask == NULL) {
            sixel_helper_set_additional_message(
                "sixel_dither_apply_palette: transparency mask allocation failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        memset(transparent_mask, 0, total_pixels);
        keycolor_for_mask = dither->keycolor;
        for (index = 0U; index < total_pixels; ++index) {
            if (sixel_dither_extract_alpha_u8(
                    pixels + index * source_depth,
                    source_pixelformat) == 0U) {
                transparent_mask[index] = 1U;
                apply_transparent_mask = 1;
            }
        }
    }
    if (apply_transparent_mask && transparent_mask != NULL &&
        keycolor_for_mask >= 0 && keycolor_for_mask < SIXEL_PALETTE_MAX) {
        sixel_dither_set_pipeline_transparent_mask_hint(
            dither,
            transparent_mask,
            total_pixels,
            keycolor_for_mask);
    }

    pipeline_pixelformat = dither->pixelformat;
    /*
     * Reuse the externally allocated index buffer when the pipeline has
     * already provisioned storage for the producer/worker hand-off.
     */
    if (dither->pipeline_index_buffer != NULL &&
            dither->pipeline_index_size >= bufsize) {
        dest = dither->pipeline_index_buffer;
        dest_owned = 0;
    } else {
        dest = (sixel_index_t *)sixel_allocator_malloc(dither->allocator,
                                                       bufsize);
        if (dest == NULL) {
            sixel_helper_set_additional_message(
                "sixel_dither_new: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        dest_owned = 1;
    }
    dither->pipeline_index_owned = dest_owned;

    /*
     * Disable palette caching when the caller selected the NONE policy so
     * every pixel lookup performs a direct palette scan.  Other quality
     * modes continue to honor the requested LUT policy, including "full".
     */
    if (dither->lut_policy == SIXEL_LUT_POLICY_NONE) {
        dither->optimized = 0;
    }

    if (dither->optimized) {
        if (!sixel_palette_is_builtin_mono(palette)) {
            int policy;
            policy = dither->lut_policy;
            if (policy != SIXEL_LUT_POLICY_CERTLUT
                && policy != SIXEL_LUT_POLICY_5BIT
                && policy != SIXEL_LUT_POLICY_6BIT
                && policy != SIXEL_LUT_POLICY_EYTZINGER
                && policy != SIXEL_LUT_POLICY_FHEDT
                && policy != SIXEL_LUT_POLICY_VPTREE
                && policy != SIXEL_LUT_POLICY_RBC
                && policy != SIXEL_LUT_POLICY_MAHALANOBIS) {
                policy = SIXEL_LUT_POLICY_6BIT;
            }
            if (palette->lut == NULL) {
                status = sixel_lut_new(&palette->lut,
                                       policy,
                                       palette->allocator);
                if (SIXEL_FAILED(status)) {
                    sixel_helper_set_additional_message(
                        "sixel_dither_apply_palette: lut allocation failed.");
                    goto end;
                }
            }
            if (policy == SIXEL_LUT_POLICY_CERTLUT) {
                if (dither->method_for_largest == SIXEL_LARGE_LUM) {
                    wcomp1 = dither->complexion * 299;
                    wcomp2 = 587;
                    wcomp3 = 114;
                } else {
                    wcomp1 = dither->complexion;
                    wcomp2 = 1;
                    wcomp3 = 1;
                }
            } else {
                wcomp1 = dither->complexion;
                wcomp2 = 1;
                wcomp3 = 1;
            }
            status = sixel_lut_configure(palette->lut,
                                         palette->entries,
                                         palette->entries_float32,
                                         palette->depth,
                                         palette->float_depth,
                                         (int)palette->entry_count,
                                         dither->complexion,
                                         wcomp1,
                                         wcomp2,
                                         wcomp3,
                                         policy,
                                         dither->pixelformat);
            if (SIXEL_FAILED(status)) {
                sixel_helper_set_additional_message(
                    "sixel_dither_apply_palette: lut configuration failed.");
                goto end;
            }
        }
    }

    pipeline_pixelformat = dither->pixelformat;
    prefer_float_pipeline =
        sixel_dither_method_supports_float_pipeline(dither);
    if (pipeline_pixelformat == SIXEL_PIXELFORMAT_RGB888) {
        input_pixels = pixels;
    } else if (SIXEL_PIXELFORMAT_IS_FLOAT32(pipeline_pixelformat)
               && prefer_float_pipeline) {
        input_pixels = pixels;
    } else {
        normalized_size = (size_t)width * (size_t)height * 3U;
        normalized_pixels
            = (unsigned char *)sixel_allocator_malloc(dither->allocator,
                                                      normalized_size);
        if (normalized_pixels == NULL) {
            sixel_helper_set_additional_message(
                "sixel_dither_new: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        if (dither->transparent_bgcolor_valid != 0
                && sixel_dither_pixelformat_has_alpha(source_pixelformat)
                && dither->keycolor >= 0) {
            status = sixel_dither_composite_alpha_to_rgb(
                normalized_pixels,
                pixels,
                total_pixels,
                source_pixelformat,
                dither->transparent_bgcolor);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
            dither->pixelformat = SIXEL_PIXELFORMAT_RGB888;
        } else {
            status = sixel_helper_normalize_pixelformat(
                normalized_pixels,
                &dither->pixelformat,
                pixels,
                dither->pixelformat,
                width,
                height);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
        }
        input_pixels = normalized_pixels;
        pipeline_pixelformat = SIXEL_PIXELFORMAT_RGB888;
    }
    if (prefer_float_pipeline
        && pipeline_pixelformat == SIXEL_PIXELFORMAT_RGB888
        && total_pixels > 0U) {
        status = sixel_dither_promote_rgb888_to_float32(
            &float_pipeline_pixels,
            input_pixels,
            total_pixels,
            dither->allocator);
        if (SIXEL_SUCCEEDED(status) && float_pipeline_pixels != NULL) {
            input_pixels = (unsigned char *)float_pipeline_pixels;
            pipeline_pixelformat = SIXEL_PIXELFORMAT_RGBFLOAT32;
            owns_float_pipeline = 1;
        } else {
            prefer_float_pipeline = 0;
            status = SIXEL_OK;
        }
    } else if (prefer_float_pipeline
               && !SIXEL_PIXELFORMAT_IS_FLOAT32(pipeline_pixelformat)) {
        prefer_float_pipeline = 0;
    }

    method_for_scan = dither->method_for_scan;
    if (method_for_scan == SIXEL_SCAN_AUTO) {
        /*
         * Keep scan defaults deterministic by diffusion family:
         * - LSO2 prefers serpentine for its variable-coefficient kernel.
         * - All other diffusion families default to raster.
         */
        if (dither->method_for_diffuse == SIXEL_DIFFUSE_LSO2) {
            method_for_scan = SIXEL_SCAN_SERPENTINE;
        } else {
            method_for_scan = SIXEL_SCAN_RASTER;
        }
    }

    palette->lut_policy = dither->lut_policy;
    palette->method_for_largest = dither->method_for_largest;
#if SIXEL_ENABLE_THREADS
    if (parallel_active && parallel_threads > 1
            && parallel_band_height > 0) {
        sixel_parallel_dither_plan_t plan;
        int adjusted_overlap;
        int adjusted_height;

        adjusted_overlap = parallel_overlap;
        if (adjusted_overlap < 0) {
            adjusted_overlap = 0;
        }
        adjusted_height = parallel_band_height;
        if (adjusted_height < 6) {
            adjusted_height = 6;
        }
        if ((adjusted_height % 6) != 0) {
            adjusted_height = ((adjusted_height + 5) / 6) * 6;
        }
        if (adjusted_overlap > adjusted_height / 2) {
            adjusted_overlap = adjusted_height / 2;
        }

        memset(&plan, 0, sizeof(plan));
        plan.dest = dest;
        plan.pixels = input_pixels;
        plan.palette = palette;
        plan.allocator = dither->allocator;
        plan.dither = dither;
        plan.width = width;
        plan.height = height;
        plan.band_height = adjusted_height;
        plan.overlap = adjusted_overlap;
        plan.method_for_diffuse = dither->method_for_diffuse;
        plan.method_for_scan = method_for_scan;
        plan.optimize_palette = dither->optimized;
        plan.optimize_palette_entries = dither->optimize_palette;
        plan.complexion = dither->complexion;
        plan.lut_policy = dither->lut_policy;
        plan.method_for_largest = dither->method_for_largest;
        plan.reqcolor = dither->ncolors;
        plan.pixelformat = pipeline_pixelformat;
        /* Carry the pipeline pinning preference as a strict 0/1 flag. */
        plan.pin_threads = dither->pipeline_pin_threads != 0 ? 1 : 0;
        plan.logger = logger;

#if SIXEL_ENABLE_THREADS
        shared_lut = 1;
        if (plan.lut_policy == SIXEL_LUT_POLICY_CERTLUT) {
            shared_lut = sixel_lookup_env_shared_certlut();
        } else if (plan.lut_policy == SIXEL_LUT_POLICY_5BIT) {
            shared_lut = sixel_lookup_env_shared_5bit();
        } else if (plan.lut_policy == SIXEL_LUT_POLICY_6BIT) {
            shared_lut = sixel_lookup_env_shared_6bit();
        }
        if (shared_lut != 0) {
            plan.lut = palette->lut;
        } else {
            plan.lut = NULL;
        }
#else
        plan.lut = palette->lut;
#endif  /* SIXEL_ENABLE_THREADS */

        if (plan.lut != NULL && dither->optimized != 0
                && plan.lut_policy != SIXEL_LUT_POLICY_NONE) {
            if (plan.lut_policy == SIXEL_LUT_POLICY_CERTLUT) {
                if (plan.method_for_largest == SIXEL_LARGE_LUM) {
                    wcomp1 = dither->complexion * 299;
                    wcomp2 = 587;
                    wcomp3 = 114;
                } else {
                    wcomp1 = dither->complexion;
                    wcomp2 = 1;
                    wcomp3 = 1;
                }
            } else {
                wcomp1 = dither->complexion;
                wcomp2 = 1;
                wcomp3 = 1;
            }
            status = sixel_lut_configure(plan.lut,
                                         plan.palette->entries,
                                         plan.palette->entries_float32,
                                         plan.palette->depth,
                                         plan.palette->float_depth,
                                         (int)plan.palette->entry_count,
                                         dither->complexion,
                                         wcomp1,
                                         wcomp2,
                                         wcomp3,
                                         plan.lut_policy,
                                         plan.pixelformat);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
        }

        status = sixel_dither_apply_palette_parallel(&plan,
                                                     parallel_threads);
        ncolors = dither->ncolors;
    } else
#endif
    {
        resolve_request.result = dest;
        resolve_request.data = input_pixels;
        resolve_request.width = width;
        resolve_request.height = height;
        resolve_request.depth = 3;
        resolve_request.palette = palette;
        resolve_request.reqcolor = dither->ncolors;
        resolve_request.method_for_diffuse = dither->method_for_diffuse;
        resolve_request.method_for_scan = method_for_scan;
        resolve_request.foptimize = dither->optimized;
        resolve_request.foptimize_palette = dither->optimize_palette;
        resolve_request.complexion = dither->complexion;
        resolve_request.lut_policy = dither->lut_policy;
        resolve_request.method_for_largest = dither->method_for_largest;
        resolve_request.ncolors = &ncolors;
        resolve_request.allocator = dither->allocator;
        resolve_request.dither = dither;
        resolve_request.pixelformat = pipeline_pixelformat;
        status = sixel_dither_resolve_indexes(&resolve_request);
    }
    if (SIXEL_FAILED(status)) {
        if (dest != NULL && dest_owned) {
            sixel_allocator_free(dither->allocator, dest);
        }
        dest = NULL;
        goto end;
    }

    /*
     * Parallel encode workers may already classify rows from `dest`.  Avoid
     * a final whole-buffer rewrite in that mode; transparent keycolor mapping
     * is applied during index resolution via pipeline_transparent_mask.
     */
    if (!parallel_active &&
        apply_transparent_mask && transparent_mask != NULL &&
        keycolor_for_mask >= 0 &&
        keycolor_for_mask < SIXEL_PALETTE_MAX) {
        for (index = 0U; index < total_pixels; ++index) {
            if (transparent_mask[index] != 0U) {
                dest[index] = (sixel_index_t)keycolor_for_mask;
            }
        }
    }
    if (keycolor_for_mask >= 0 &&
        keycolor_for_mask < palette_entry_limit &&
        ncolors <= keycolor_for_mask) {
        ncolors = keycolor_for_mask + 1;
    }
    if (dither->force_palette != 0 && palette_entry_limit > 0) {
        /*
         * Keep output palette slots stable for forced-palette mode.
         * Index resolution may touch fewer colors per frame, but callers
         * relying on fixed slots (for example animation palette locking)
         * need the original entry count preserved.
         */
        ncolors = palette_entry_limit;
    }

    dither->ncolors = ncolors;
    palette->entry_count = (unsigned int)ncolors;

end:
    if (!parallel_active && logger != NULL) {
        int last_row;

        last_row = height > 0 ? height - 1 : 0;
        sixel_logger_logf(logger,
                          "worker",
                          "dither",
                          "finish",
                          0,
                          last_row,
                          0,
                          height,
                          0,
                          height,
                          "serial status=%d",
                          status);
    }
    sixel_lookup_set_parallel_dither_active(0);
    if (dither != NULL) {
        sixel_dither_interframe_finish_apply(dither,
                                           resolved_apply_mode,
                                           status);
        if (normalized_pixels != NULL) {
            sixel_allocator_free(dither->allocator, normalized_pixels);
        }
        if (float_pipeline_pixels != NULL && owns_float_pipeline) {
            sixel_allocator_free(dither->allocator, float_pipeline_pixels);
        }
        /*
         * Frame-owned preset masks are borrowed pointers and must not be
         * released here.
         */
        if (transparent_mask != NULL && !using_preset_transparent_mask) {
            sixel_allocator_free(dither->allocator, transparent_mask);
        }
        sixel_dither_cleanup_apply_hints(dither);
        sixel_dither_unref(dither);
    }
    return dest;
}

SIXEL_INTERNAL_API sixel_index_t *
sixel_dither_apply_palette(
    sixel_dither_t  /* in */ *dither,
    unsigned char   /* in */ *pixels,
    int             /* in */ width,
    int             /* in */ height)
{
    return sixel_dither_apply_palette_with_mode(
        dither,
        pixels,
        width,
        height,
        SIXEL_DITHER_APPLY_CONSUME_INTERFRAME_STATE);
}


/*
 * Verify reference counting works when new dithers are created and unrefed
 * multiple times.
 */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
