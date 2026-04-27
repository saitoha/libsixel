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
#include "components.h"
#include "factory.h"
#include "lookup-policy.h"
#include "dither-policy.h"
#include "timer.h"
#include "dither-common-pipeline.h"
#include "logger.h"
#include "pixelformat.h"
#include "sixel_atomic.h"
#if SIXEL_ENABLE_THREADS
# include "threadpool.h"
#endif
#include <sixel.h>

/*
 * IDL usage in this unit
 *
 * IComponents.getservice("services/factory", &factory)
 * IFactory.create("lookup/...", allocator, &lookup)
 * IFactory.create("dither/...", allocator, &dither_policy)
 * ILookupPolicy.prepare(request{shared_instance_enabled,...})
 * IDitherPolicy.prepare(request)
 * IDitherPolicy.apply(request)
 * IDitherPolicy.supports_parallel_bands()
 * ILookupPolicySharedConfig.{certlut,5bit,6bit}_shared_instance_enabled()
 */

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
sixel_dither_interframe_state_reset_with_reason(sixel_dither_t *dither,
                                                int reason);

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

static int
sixel_dither_palette_has_float_entries(
    sixel_palette_t const *palette,
    int channels)
{
    size_t required_size;
    int required_depth;

    required_size = 0U;
    required_depth = 0;
    if (palette == NULL) {
        return 0;
    }
    if (channels <= 0) {
        return 0;
    }
    if (palette->entries_float32 == NULL || palette->entry_count == 0U) {
        return 0;
    }
    required_depth = channels * (int)sizeof(float);
    if (palette->float_depth < required_depth) {
        return 0;
    }
    required_size = (size_t)palette->entry_count
                  * (size_t)palette->float_depth;
    if (palette->entries_float32_size < required_size) {
        return 0;
    }

    return 1;
}

/*
 * Convert the current byte palette into float entries so float32 dither
 * kernels and float lookup policies can share the same value domain.
 */
static SIXELSTATUS
sixel_dither_promote_palette_rgb888_to_float32(
    sixel_palette_t *palette,
    int pixelformat,
    sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    float *converted;
    size_t payload_size;
    size_t row_stride;
    size_t row_offset;
    unsigned int color;
    int channel;
    int channels;

    status = SIXEL_FALSE;
    converted = NULL;
    payload_size = 0U;
    row_stride = 0U;
    row_offset = 0U;
    color = 0U;
    channel = 0;
    channels = 0;

    if (palette == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (palette->entries == NULL || palette->entry_count == 0U) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (!SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat)) {
        return SIXEL_BAD_ARGUMENT;
    }

    channels = sixel_helper_compute_depth(pixelformat);
    if (channels <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    channels /= (int)sizeof(float);
    if (channels <= 0 || channels > SIXEL_MAX_CHANNELS) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (palette->depth < channels) {
        return SIXEL_BAD_ARGUMENT;
    }

    row_stride = (size_t)channels;
    if ((size_t)palette->entry_count > SIZE_MAX / row_stride) {
        return SIXEL_BAD_INPUT;
    }
    payload_size = (size_t)palette->entry_count * row_stride;
    if (payload_size > SIZE_MAX / sizeof(float)) {
        return SIXEL_BAD_INPUT;
    }

    converted = (float *)sixel_allocator_malloc(
        allocator,
        payload_size * sizeof(float));
    if (converted == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    for (color = 0U; color < palette->entry_count; ++color) {
        row_offset = (size_t)color * row_stride;
        for (channel = 0; channel < channels; ++channel) {
            converted[row_offset + (size_t)channel]
                = sixel_pixelformat_byte_to_float(
                    pixelformat,
                    channel,
                    palette->entries[(size_t)color * (size_t)palette->depth
                                     + (size_t)channel]);
        }
    }

    status = sixel_palette_set_entries_float32(
        palette,
        converted,
        palette->entry_count,
        channels * (int)sizeof(float),
        allocator);
    sixel_allocator_free(allocator, converted);
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


static int
sixel_dither_lut_policy_supports_shared_instance(int lut_policy)
{
    if (lut_policy == SIXEL_LUT_POLICY_CERTLUT
            || lut_policy == SIXEL_LUT_POLICY_5BIT
            || lut_policy == SIXEL_LUT_POLICY_6BIT) {
        return 1;
    }

    return 0;
}

static int
sixel_dither_lookup_shared_instance_enabled(
    sixel_dither_t const *dither,
    int lut_policy)
{
    if (!sixel_dither_lut_policy_supports_shared_instance(lut_policy)) {
        return 1;
    }

    if (dither != NULL && dither->lut_policy_shared_instance_override != 0) {
        return dither->lut_policy_shared_instance != 0;
    }

    if (lut_policy == SIXEL_LUT_POLICY_CERTLUT) {
        return sixel_lookup_policy_certlut_shared_instance_enabled();
    }
    if (lut_policy == SIXEL_LUT_POLICY_5BIT) {
        return sixel_lookup_policy_5bit_shared_instance_enabled();
    }
    if (lut_policy == SIXEL_LUT_POLICY_6BIT) {
        return sixel_lookup_policy_6bit_shared_instance_enabled();
    }

    return 1;
}

static SIXELSTATUS
sixel_dither_prepare_lookup_policy(
    sixel_lookup_policy_interface_t **lookup_policy,
    unsigned char const *palette,
    float const *palette_float,
    int depth,
    int float_depth,
    int reqcolor,
    int foptimize,
    int lut_policy,
    int shared_instance_enabled,
    int pixelformat,
    int parallel_dither_active,
    sixel_lookup_policy_interface_t *reuse_policy,
    sixel_lookup_policy_interface_t **reuse_policy_slot,
    sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    sixel_lookup_policy_select_request_t select_request;
    sixel_lookup_policy_prepare_request_t request;
    sixel_lookup_policy_interface_t *prepared_policy;
    sixel_factory_t *factory;
    void *service;
    char const *policy_name;

    status = SIXEL_FALSE;
    memset(&select_request, 0, sizeof(select_request));
    memset(&request, 0, sizeof(request));
    prepared_policy = NULL;
    factory = NULL;
    service = NULL;
    policy_name = NULL;

    if (lookup_policy == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    request.palette = palette;
    request.palette_float = palette_float;
    request.depth = depth;
    request.float_depth = float_depth;
    request.reqcolor = reqcolor;
    request.pixelformat = pixelformat;
    request.parallel_dither_active = parallel_dither_active;
    request.shared_instance_enabled = (shared_instance_enabled != 0) ? 1 : 0;
    request.reuse_policy = reuse_policy;
    request.reuse_policy_slot = reuse_policy_slot;
    request.allocator = allocator;

    select_request.palette = palette;
    select_request.depth = depth;
    select_request.reqcolor = reqcolor;
    select_request.optimize_lookup = foptimize;
    select_request.lut_policy = lut_policy;
    select_request.pixelformat = pixelformat;

    policy_name = sixel_lookup_policy_select_name(&select_request);
    if (policy_name == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = sixel_components_getservice("services/factory", &service);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    factory = (sixel_factory_t *)service;

    status = factory->vtbl->create(factory,
                                   policy_name,
                                   allocator,
                                   (void **)&prepared_policy);
    factory->vtbl->unref(factory);
    factory = NULL;
    if (SIXEL_FAILED(status)) {
        return status;
    }

    status = prepared_policy->vtbl->prepare(prepared_policy, &request);
    if (SIXEL_FAILED(status)) {
        prepared_policy->vtbl->unref(prepared_policy);
        return status;
    }

    if (*lookup_policy != NULL) {
        (*lookup_policy)->vtbl->unref(*lookup_policy);
    }
    *lookup_policy = prepared_policy;

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_dither_prepare_dither_policy(
    sixel_dither_t *dither,
    int depth,
    int method_for_scan,
    int pixelformat)
{
    SIXELSTATUS status;
    sixel_factory_t *factory;
    void *service;
    char const *policy_name;
    sixel_dither_policy_select_request_t select_request;
    sixel_dither_policy_prepare_request_t request;
    sixel_dither_policy_interface_t *prepared_policy;

    status = SIXEL_FALSE;
    factory = NULL;
    service = NULL;
    policy_name = NULL;
    prepared_policy = NULL;
    memset(&select_request, 0, sizeof(select_request));
    memset(&request, 0, sizeof(request));

    if (dither == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    select_request.method_for_diffuse = dither->method_for_diffuse;
    select_request.ncolors = dither->ncolors;
    select_request.pixelformat = pixelformat;
    policy_name = sixel_dither_policy_select_name(&select_request);
    if (policy_name == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    request.dither = dither;
    request.depth = depth;
    request.method_for_scan = method_for_scan;
    request.pixelformat = pixelformat;

    if (dither->dither_policy != NULL
            && dither->dither_policy_class_name != NULL
            && strcmp(dither->dither_policy_class_name, policy_name) == 0) {
        status = dither->dither_policy->vtbl->prepare(
            dither->dither_policy,
            &request);
        return status;
    }

    status = sixel_components_getservice("services/factory", &service);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    factory = (sixel_factory_t *)service;

    status = factory->vtbl->create(factory,
                                   policy_name,
                                   dither->allocator,
                                   (void **)&prepared_policy);
    factory->vtbl->unref(factory);
    factory = NULL;
    if (SIXEL_FAILED(status)) {
        return status;
    }

    status = prepared_policy->vtbl->prepare(prepared_policy, &request);
    if (SIXEL_FAILED(status)) {
        prepared_policy->vtbl->unref(prepared_policy);
        return status;
    }

    if (dither->dither_policy != NULL) {
        dither->dither_policy->vtbl->unref(dither->dither_policy);
        dither->dither_policy = NULL;
    }
    dither->dither_policy = prepared_policy;
    dither->dither_policy_class_name = policy_name;
    return SIXEL_OK;
}

#if SIXEL_ENABLE_THREADS
typedef struct sixel_parallel_dither_plan {
    sixel_index_t *dest;
    unsigned char *pixels;
    sixel_palette_t *palette;
    sixel_dither_policy_interface_t *dither_policy;
    sixel_lookup_policy_interface_t *lookup_policy;
    sixel_allocator_t *allocator;
    sixel_dither_t *dither;
    size_t row_bytes;
    int width;
    int height;
    int band_height;
    int overlap;
    int method_for_scan;
    int lut_policy;
    int shared_lut;
    int lookup_shared_instance_enabled;
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
    sixel_lookup_policy_interface_t *lookup_policy;
} sixel_parallel_dither_state_t;

static void
sixel_parallel_dither_cleanup(void *workspace)
{
    sixel_parallel_dither_state_t *state;

    state = (sixel_parallel_dither_state_t *)workspace;
    if (state == NULL) {
        return;
    }
    if (state->lookup_policy != NULL) {
        /*
         * Each worker owns its private lookup policy instance when shared
         * caches are disabled. Release it here so threadpool teardown can
         * free the workspace without leaking per-thread caches.
         */
        state->lookup_policy->vtbl->unref(state->lookup_policy);
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
    SIXELSTATUS status;
    sixel_parallel_dither_state_t *state;
    sixel_lookup_policy_interface_t *reuse_policy;
    int restore_context;
    sixel_lookup_policy_interface_t *lookup_policy;
    sixel_dither_policy_apply_request_t apply_request;

    plan = (sixel_parallel_dither_plan_t *)userdata;
    if (plan == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (plan->dither_policy == NULL || plan->dither_policy->vtbl == NULL) {
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

    state = (sixel_parallel_dither_state_t *)workspace;
    reuse_policy = plan->lookup_policy;
    restore_context = 0;
    if (reuse_policy != NULL) {
        /* shared prepared policy */
    } else if (state != NULL) {
        reuse_policy = state->lookup_policy;
    }
    lookup_policy = NULL;
    status = sixel_dither_prepare_lookup_policy(
        &lookup_policy,
        plan->palette->entries,
        plan->palette->entries_float32,
        3,
        plan->palette->float_depth,
        plan->reqcolor,
        (plan->dither != NULL) ? plan->dither->optimized : 0,
        plan->lut_policy,
        plan->lookup_shared_instance_enabled,
        plan->pixelformat,
        1,
        reuse_policy,
        NULL,
        plan->allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    if (state != NULL && plan->lookup_policy == NULL) {
        if (state->lookup_policy != NULL
                && state->lookup_policy != lookup_policy) {
            state->lookup_policy->vtbl->unref(state->lookup_policy);
        }
        state->lookup_policy = lookup_policy;
        lookup_policy->vtbl->ref(lookup_policy);
    }

    /*
     * Map directly into the shared destination but suppress writes
     * before output_start.  The overlap rows are computed only to warm
     * up the error diffusion and are discarded by the output_start
     * check in the renderer, so neighboring bands never clobber each
     * other's body.
     */
    memset(&apply_request, 0, sizeof(apply_request));
    apply_request.result = plan->dest + (size_t)in0 * plan->width;
    apply_request.data = (unsigned char *)source;
    apply_request.width = plan->width;
    apply_request.height = rows;
    apply_request.band_origin = in0;
    apply_request.output_start = y0;
    apply_request.depth = 3;
    apply_request.palette = plan->palette->entries;
    apply_request.method_for_scan = plan->method_for_scan;
    apply_request.lookup_policy = lookup_policy;
    apply_request.dither = plan->dither;
    apply_request.pixelformat = plan->pixelformat;
    status = plan->dither_policy->vtbl->apply(
        plan->dither_policy,
        &apply_request);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
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

cleanup:
    if (lookup_policy != NULL) {
        lookup_policy->vtbl->unref(lookup_policy);
        lookup_policy = NULL;
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
    if (plan->lookup_policy == NULL
            && plan->lut_policy != SIXEL_LUT_POLICY_NONE) {
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


/* Bundle serial index-resolution inputs to avoid pcc ICE on long signatures. */
typedef struct sixel_dither_resolve_indexes_request {
    sixel_index_t *result;
    unsigned char *data;
    int width;
    int height;
    int depth;
    sixel_palette_t *palette;
    int reqcolor;
    int method_for_scan;
    int foptimize;
    int lut_policy;
    int *ncolors;
    sixel_allocator_t *allocator;
    sixel_dither_t *dither;
    sixel_dither_policy_interface_t *dither_policy;
    int pixelformat;
} sixel_dither_resolve_indexes_request_t;

/*
 * Route palette application through the local dithering helper.  The
 * function keeps all state in the palette object so we can share cache
 * buffers between invocations and later stages.  The flow is:
 *   1. Synchronize the quantizer configuration with the dither object so the
 *      LUT builder honors the requested policy.
 *   2. Invoke IDitherPolicy.apply() to populate the index buffer.
 *   3. Return the status to the caller so palette application errors can
 *      be reported at a single site.
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
    int method_for_scan;
    int foptimize;
    int lut_policy;
    int *ncolors;
    sixel_allocator_t *allocator;
    sixel_dither_t *dither;
    sixel_dither_policy_interface_t *dither_policy;
    int shared_lut;
    int pixelformat;
    sixel_dither_policy_apply_request_t apply_request;

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
    method_for_scan = request->method_for_scan;
    foptimize = request->foptimize;
    lut_policy = request->lut_policy;
    ncolors = request->ncolors;
    allocator = request->allocator;
    dither = request->dither;
    dither_policy = request->dither_policy;
    shared_lut = sixel_dither_lookup_shared_instance_enabled(dither,
                                                             lut_policy);
    pixelformat = request->pixelformat;

    sixel_palette_set_lut_policy(lut_policy);

    status = sixel_dither_prepare_lookup_policy(
        &palette->lookup_policy,
        palette->entries,
        palette->entries_float32,
        depth,
        palette->float_depth,
        reqcolor,
        foptimize,
        lut_policy,
        shared_lut,
        pixelformat,
        0,
        palette->lookup_policy,
        NULL,
        allocator);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    if (dither_policy == NULL || dither_policy->vtbl == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (ncolors != NULL && dither != NULL) {
        *ncolors = dither->ncolors;
    }

    memset(&apply_request, 0, sizeof(apply_request));
    apply_request.result = result;
    apply_request.data = data;
    apply_request.width = width;
    apply_request.height = height;
    apply_request.band_origin = 0;
    apply_request.output_start = 0;
    apply_request.depth = depth;
    apply_request.palette = palette->entries;
    apply_request.method_for_scan = method_for_scan;
    apply_request.lookup_policy = palette->lookup_policy;
    apply_request.dither = dither;
    apply_request.pixelformat = pixelformat;
    status = dither_policy->vtbl->apply(dither_policy, &apply_request);

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
    (*ppdither)->lut_policy_shared_instance_override = 0;
    (*ppdither)->lut_policy_shared_instance = 0;
    (*ppdither)->dither_policy = NULL;
    (*ppdither)->dither_policy_class_name = NULL;
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
        if (dither->dither_policy != NULL) {
            dither->dither_policy->vtbl->unref(dither->dither_policy);
            dither->dither_policy = NULL;
            dither->dither_policy_class_name = NULL;
        }
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
    int method_for_largest_for_palette;
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
    /*
     * Largest-axis heuristics belong to Heckbert median-cut only. Other
     * quantizers keep a neutral value so -f does not affect their output.
     */
    method_for_largest_for_palette = SIXEL_LARGE_NORM;
    if (dither->quantize_model == SIXEL_QUANTIZE_MODEL_MEDIANCUT) {
        method_for_largest_for_palette = dither->method_for_largest;
    }

    status = sixel_palette_make_palette(&buf,
                                        input_pixels,
                                        payload_length,
                                        palette_pixelformat,
                                        (unsigned int)dither->reqcolors,
                                        (unsigned int *)&dither->ncolors,
                                        (unsigned int *)&dither->origcolors,
                                        method_for_largest_for_palette,
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
     * Policy transitions invalidate prepared lookup policy caches so the
     * next apply call rebuilds under the new class.
     */
    dither->lut_policy = normalized;
    if (dither->palette != NULL) {
        dither->palette->lut_policy = normalized;
        if (dither->palette->lookup_policy != NULL) {
            dither->palette->lookup_policy->vtbl->unref(
                dither->palette->lookup_policy);
            dither->palette->lookup_policy = NULL;
        }
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
    sixel_dither_policy_interface_t *policy;

    policy = NULL;
    if (dither == NULL) {
        return;
    }

    if (method_for_diffuse == SIXEL_DIFFUSE_AUTO) {
        if (dither->ncolors > 16) {
            method_for_diffuse = SIXEL_DIFFUSE_FS;
        } else {
            method_for_diffuse = SIXEL_DIFFUSE_ATKINSON;
        }
    }

    if (dither->method_for_diffuse == method_for_diffuse) {
        return;
    }

    dither->method_for_diffuse = method_for_diffuse;
    policy = dither->dither_policy;
    if (policy != NULL) {
        policy->vtbl->unref(policy);
        dither->dither_policy = NULL;
        dither->dither_policy_class_name = NULL;
    }
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


/* set whether omitting palette definition */
SIXELAPI void
sixel_dither_set_body_only(
    sixel_dither_t /* in */ *dither,     /* dither context object */
    int            /* in */ bodyonly)    /* 0: output palette section
                                            1: do not output palette section  */
{
    dither->bodyonly = bodyonly;
}


/*
 * Keep this API for source compatibility.
 * Palette-entry minimization is retired and always disabled internally.
 */
SIXELAPI void
sixel_dither_set_optimize_palette(
    sixel_dither_t /* in */ *dither,   /* dither context object */
    int            /* in */ do_opt)    /* 0: don't optimize palette size
                                          1: optimize palette size */
{
    (void)dither;
    (void)do_opt;
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
    dither->interframe_state.reset_count = 0UL;
    dither->interframe_state.reset_frame_boundary_count = 0UL;
    dither->interframe_state.reset_size_change_count = 0UL;
    dither->interframe_state.reset_clear_count = 0UL;
    dither->interframe_state.last_apply_status = SIXEL_FALSE;
    dither->interframe_state.last_apply_consumed = 0;
}

SIXEL_INTERNAL_API void
sixel_dither_note_interframe_reset_reason(sixel_dither_t *dither,
                                          int reason)
{
    if (dither == NULL) {
        return;
    }

    dither->interframe_state.reset_count += 1UL;
    if (reason == SIXEL_DITHER_INTERFRAME_RESET_REASON_FRAME_BOUNDARY) {
        dither->interframe_state.reset_frame_boundary_count += 1UL;
    } else if (reason == SIXEL_DITHER_INTERFRAME_RESET_REASON_SIZE_CHANGE) {
        dither->interframe_state.reset_size_change_count += 1UL;
    } else if (reason == SIXEL_DITHER_INTERFRAME_RESET_REASON_CLEAR) {
        dither->interframe_state.reset_clear_count += 1UL;
    }
}

static void
sixel_dither_interframe_state_reset_with_reason(sixel_dither_t *dither,
                                                int reason)
{
    if (dither == NULL) {
        return;
    }

    sixel_dither_note_interframe_reset_reason(dither, reason);
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

    sixel_dither_interframe_state_reset_with_reason(
        dither,
        SIXEL_DITHER_INTERFRAME_RESET_REASON_NONE);
    dither->frame_context.frame_no = 0;
    dither->frame_context.loop_no = 0;
    dither->frame_context.multiframe = 0;
    dither->frame_context.valid = 0;
    dither->interframe_state.apply_count = 0UL;
    dither->interframe_state.consume_count = 0UL;
    dither->interframe_state.reset_count = 0UL;
    dither->interframe_state.reset_frame_boundary_count = 0UL;
    dither->interframe_state.reset_size_change_count = 0UL;
    dither->interframe_state.reset_clear_count = 0UL;
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
    sixel_dither_interframe_state_reset_with_reason(
        dither,
        SIXEL_DITHER_INTERFRAME_RESET_REASON_CLEAR);
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
        sixel_dither_interframe_state_reset_with_reason(
            dither,
            SIXEL_DITHER_INTERFRAME_RESET_REASON_FRAME_BOUNDARY);
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
    int float_backend_available;
    int source_has_float;
    int palette_has_float;
    int float_channels;
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
    int resolved_apply_mode = SIXEL_DITHER_APPLY_CONSUME_INTERFRAME_STATE;
    int needs_size_reset = 0;
    int pipeline_depth = 0;
    int supports_parallel_bands;
    sixel_dither_policy_interface_t *dither_policy;
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
    if (resolved_apply_mode == SIXEL_DITHER_APPLY_CONSUME_INTERFRAME_STATE) {
        /*
         * Keep size-change resets observable in diagnostics even when
         * timeline-boundary resets have already been applied.
         */
        if (dither->interframe_state.width > 0
                && dither->interframe_state.height > 0
                && (dither->interframe_state.width != width
                    || dither->interframe_state.height != height)) {
            needs_size_reset = 1;
        }
        if (needs_size_reset != 0) {
            sixel_dither_interframe_state_reset_with_reason(
                dither,
                SIXEL_DITHER_INTERFRAME_RESET_REASON_SIZE_CHANGE);
        }
    }
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
    dither_policy = NULL;
    supports_parallel_bands = 0;
    float_backend_available = 0;
    source_has_float = 0;
    palette_has_float = 0;
    float_channels = 0;
    preset_transparent_mask = NULL;
    preset_transparent_mask_size = 0u;
    preset_transparent_keycolor = (-1);
    parallel_active = dither->pipeline_parallel_active;
#if defined(__PCC__) || defined(__TINYC__)
    /*
     * pcc and TinyCC builds do not provide the thread-safety guarantees that
     * the parallel palette-application path expects, so keep this path
     * serial.
     */
    parallel_active = 0;
#endif
#if SIXEL_ENABLE_THREADS
    parallel_band_height = dither->pipeline_band_height;
    parallel_overlap = dither->pipeline_band_overlap;
    parallel_threads = dither->pipeline_dither_threads;
#endif  /* SIXEL_ENABLE_THREADS */
    logger = dither->pipeline_logger;

    /*
     * Resolve shared-instance flags on the caller thread before workers start
     * so policy constructors can reuse parsed environment values when CLI
     * overrides are not active.
     */
    if (dither->lut_policy_shared_instance_override == 0) {
        (void)sixel_lookup_policy_certlut_shared_instance_enabled();
        (void)sixel_lookup_policy_5bit_shared_instance_enabled();
        (void)sixel_lookup_policy_6bit_shared_instance_enabled();
    }

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

    pipeline_pixelformat = dither->pixelformat;
    source_has_float = SIXEL_PIXELFORMAT_IS_FLOAT32(pipeline_pixelformat);
    float_backend_available =
        sixel_dither_method_supports_float_pipeline(dither);
    float_channels = sixel_helper_compute_depth(pipeline_pixelformat);
    if (source_has_float != 0 && float_channels > 0) {
        float_channels /= (int)sizeof(float);
    }
    palette_has_float = sixel_dither_palette_has_float_entries(
        palette,
        (float_channels > 0) ? float_channels : 3);
    prefer_float_pipeline = 0;
    if (float_backend_available != 0
            && (dither->prefer_float32 != 0
                || source_has_float != 0
                || palette_has_float != 0)) {
        prefer_float_pipeline = 1;
    }
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

    /*
     * Keep source and palette precision aligned.  Once float32 dither is
     * selected, ensure palette entries are available in float32 too.
     */
    if (prefer_float_pipeline
            && SIXEL_PIXELFORMAT_IS_FLOAT32(pipeline_pixelformat)) {
        float_channels = sixel_helper_compute_depth(pipeline_pixelformat);
        if (float_channels <= 0) {
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        float_channels /= (int)sizeof(float);
        if (float_channels <= 0 || float_channels > SIXEL_MAX_CHANNELS) {
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        if (!sixel_dither_palette_has_float_entries(palette,
                                                    float_channels)) {
            status = sixel_dither_promote_palette_rgb888_to_float32(
                palette,
                pipeline_pixelformat,
                dither->allocator);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
        }
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

    status = sixel_dither_prepare_dither_policy(dither,
                                                3,
                                                method_for_scan,
                                                pipeline_pixelformat);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    dither_policy = dither->dither_policy;
    if (dither_policy == NULL || dither_policy->vtbl == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    ncolors = dither->ncolors;
    supports_parallel_bands =
        dither_policy->vtbl->supports_parallel_bands(dither_policy);
    if (parallel_active && supports_parallel_bands == 0) {
        /*
         * The selected dither class requires frame-global state and
         * cannot process overlapped bands in parallel.
         */
        parallel_active = 0;
    }
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

    palette->lut_policy = dither->lut_policy;
#if SIXEL_ENABLE_THREADS
    shared_lut = sixel_dither_lookup_shared_instance_enabled(
        dither,
        dither->lut_policy);
    if (parallel_active && parallel_threads > 1
            && parallel_band_height > 0) {
        sixel_parallel_dither_plan_t plan;
        int adjusted_overlap;
        int adjusted_height;

        status = sixel_dither_prepare_lookup_policy(
            &palette->lookup_policy,
            palette->entries,
            palette->entries_float32,
            palette->depth,
            palette->float_depth,
            dither->ncolors,
            dither->optimized,
            dither->lut_policy,
            shared_lut,
            pipeline_pixelformat,
            parallel_active,
            palette->lookup_policy,
            NULL,
            dither->allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }

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
        plan.method_for_scan = method_for_scan;
        plan.lut_policy = dither->lut_policy;
        plan.lookup_shared_instance_enabled = shared_lut;
        plan.reqcolor = dither->ncolors;
        plan.pixelformat = pipeline_pixelformat;
        plan.dither_policy = dither_policy;
        /* Carry the pipeline pinning preference as a strict 0/1 flag. */
        plan.pin_threads = dither->pipeline_pin_threads != 0 ? 1 : 0;
        plan.logger = logger;
        if (shared_lut != 0) {
            plan.lookup_policy = palette->lookup_policy;
        } else {
            plan.lookup_policy = NULL;
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
        resolve_request.method_for_scan = method_for_scan;
        resolve_request.foptimize = dither->optimized;
        resolve_request.lut_policy = dither->lut_policy;
        resolve_request.ncolors = &ncolors;
        resolve_request.allocator = dither->allocator;
        resolve_request.dither = dither;
        resolve_request.dither_policy = dither_policy;
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
    if (resolved_apply_mode == SIXEL_DITHER_APPLY_CONSUME_INTERFRAME_STATE) {
        pipeline_depth = sixel_helper_compute_depth(pipeline_pixelformat);
        dither->interframe_state.width = width;
        dither->interframe_state.height = height;
        dither->interframe_state.depth = pipeline_depth;
    }

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
