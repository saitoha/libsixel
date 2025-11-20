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

#include "config.h"

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
#include "palette.h"
#include "compat_stub.h"
#include "lut.h"
#include "assessment.h"
#include "dither-common-pipeline.h"
#include "dither-positional-8bit.h"
#include "dither-positional-float32.h"
#include "dither-fixed-8bit.h"
#include "dither-fixed-float32.h"
#include "dither-varcoeff-8bit.h"
#include "dither-varcoeff-float32.h"
#include "dither-internal.h"
#include "parallel-log.h"
#include "precision.h"
#include "pixelformat.h"
#if SIXEL_ENABLE_THREADS
# include "threadpool.h"
#endif
#include <sixel.h>

static int g_sixel_precision_float32_env_cached = (-1);

static unsigned char
sixel_precision_ascii_tolower(unsigned char ch)
{
#if HAVE_CTYPE_H
    return (unsigned char)tolower((int)ch);
#else
    if (ch >= 'A' && ch <= 'Z') {
        ch = (unsigned char)(ch - 'A' + 'a');
    }
    return ch;
#endif
}

static int
sixel_precision_is_space(unsigned char ch)
{
#if HAVE_CTYPE_H
    return isspace((int)ch);
#else
    switch (ch) {
    case ' ':  /* fallthrough */
    case '\t': /* fallthrough */
    case '\r': /* fallthrough */
    case '\n': /* fallthrough */
    case '\f':
        return 1;
    default:
        return 0;
    }
#endif
}

static int
sixel_precision_strcaseeq(const char *lhs, const char *rhs)
{
    unsigned char lc;
    unsigned char rc;

    if (lhs == NULL || rhs == NULL) {
        return 0;
    }

    while (*lhs != '\0' && *rhs != '\0') {
        lc = sixel_precision_ascii_tolower((unsigned char)*lhs);
        rc = sixel_precision_ascii_tolower((unsigned char)*rhs);
        if (lc != rc) {
            return 0;
        }
        ++lhs;
        ++rhs;
    }

    return *lhs == '\0' && *rhs == '\0';
}

static int
sixel_precision_parse_float32_flag(const char *value, int fallback)
{
    const char *cursor;

    if (value == NULL) {
        return fallback;
    }

    cursor = value;
    while (*cursor != '\0'
           && sixel_precision_is_space((unsigned char)*cursor)) {
        ++cursor;
    }
    if (*cursor == '\0') {
        return 0;
    }

    if (cursor[0] == '0' && cursor[1] == '\0') {
        return 0;
    }
    if (cursor[0] == '1' && cursor[1] == '\0') {
        return 1;
    }
    if (sixel_precision_strcaseeq(cursor, "false") ||
            sixel_precision_strcaseeq(cursor, "off") ||
            sixel_precision_strcaseeq(cursor, "no")) {
        return 0;
    }
    if (sixel_precision_strcaseeq(cursor, "true") ||
            sixel_precision_strcaseeq(cursor, "on") ||
            sixel_precision_strcaseeq(cursor, "yes") ||
            sixel_precision_strcaseeq(cursor, "auto") ||
            sixel_precision_strcaseeq(cursor, "kmeans")) {
        return 1;
    }

    return fallback;
}

int
sixel_precision_env_wants_float32(void)
{
    const char *value;

    if (g_sixel_precision_float32_env_cached != (-1)) {
        return g_sixel_precision_float32_env_cached;
    }

    value = sixel_compat_getenv(SIXEL_FLOAT32_ENVVAR);
    if (value == NULL) {
        g_sixel_precision_float32_env_cached = 0;
        return g_sixel_precision_float32_env_cached;
    }

    g_sixel_precision_float32_env_cached =
        sixel_precision_parse_float32_flag(value, 1);
    return g_sixel_precision_float32_env_cached;
}

void
sixel_precision_reset_float32_cache(void)
{
    g_sixel_precision_float32_env_cached = (-1);
}

/* Cache the parsed environment decision to avoid repeated getenv() calls. */
static int
sixel_dither_env_wants_float32(void)
{
    return sixel_precision_env_wants_float32();
}

#if HAVE_TESTS
void
sixel_dither_tests_reset_float32_flag_cache(void)
{
    sixel_precision_reset_float32_cache();
}
#endif

/*
 * Promote an RGB888 buffer to RGBFLOAT32 by normalising each channel to the
 * 0.0-1.0 range.  The helper is used when the environment requests float32
 * precision but the incoming frame still relies on 8bit pixels.
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

/*
 * Determine whether the selected diffusion method can reuse the float32
 * pipeline.  Positional and variable-coefficient dithers have dedicated
 * float backends, while Floyd-Steinberg must disable the fast path when the
 * user explicitly enables carry buffers.
 */
static int
sixel_dither_method_supports_float_pipeline(sixel_dither_t const *dither)
{
    int method;

    if (dither == NULL) {
        return 0;
    }
    if (dither->prefer_rgbfloat32 == 0) {
        return 0;
    }
    if (dither->method_for_carry == SIXEL_CARRY_ENABLE) {
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
    case SIXEL_DIFFUSE_LSO2:
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


static sixel_lut_t *dither_lut_context = NULL;

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
 * certification tree stay encapsulated inside src/lut.c.
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
    int               /* in */  methodForCarry,
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
#if _MSC_VER
    enum { max_depth = 4 };
#else
    const size_t max_depth = 4;
#endif
    unsigned char copy[max_depth];
    float new_palette_float[SIXEL_PALETTE_MAX * max_depth];
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
    int carry_mode;
    sixel_lut_t *active_lut;
    int manage_lut;
    int policy;
    int wR;
    int wG;
    int wB;

    active_lut = NULL;
    manage_lut = 0;

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
                    && (size_t)float_components <= max_depth) {
                context.palette_float = palette_object->entries_float32;
                context.float_depth = float_components;
                context.new_palette_float = new_palette_float;
            }
        }
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
                      || methodForDiffuse == SIXEL_DIFFUSE_X_DITHER);
    carry_mode = (methodForCarry == SIXEL_CARRY_ENABLE)
               ? SIXEL_CARRY_ENABLE
               : SIXEL_CARRY_DISABLE;
    context.method_for_diffuse = methodForDiffuse;
    context.method_for_scan = methodForScan;
    context.method_for_carry = carry_mode;

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
    if (lut_policy == SIXEL_LUT_POLICY_NONE) {
        f_lookup = lookup_normal;
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
            && policy != SIXEL_LUT_POLICY_6BIT) {
            policy = SIXEL_LUT_POLICY_6BIT;
        }
        if (lut == NULL) {
            status = sixel_lut_new(&active_lut, policy, allocator);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
            manage_lut = 1;
        } else {
            active_lut = lut;
            manage_lut = 0;
        }
        if (policy == SIXEL_LUT_POLICY_CERTLUT) {
            if (method_for_largest == SIXEL_LARGE_LUM) {
                wR = complexion * 299;
                wG = 587;
                wB = 114;
            } else {
                wR = complexion;
                wG = 1;
                wB = 1;
            }
        } else {
            wR = complexion;
            wG = 1;
            wB = 1;
        }
        status = sixel_lut_configure(active_lut,
                                     palette,
                                     depth,
                                     reqcolor,
                                     complexion,
                                     wR,
                                     wG,
                                     wB,
                                     policy,
                                     pixelformat);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
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
            && dither->prefer_rgbfloat32 != 0) {
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
            && dither->prefer_rgbfloat32 != 0) {
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
            && methodForCarry != SIXEL_CARRY_ENABLE
            && depth == 3
            && dither != NULL
            && dither->prefer_rgbfloat32 != 0) {
            /*
             * Float inputs can reuse the float32 renderer for every
             * fixed-weight kernel (FS, Sierra, Stucki, etc.) as long as
             * carry buffers are disabled.  The legacy 8bit path remains as
             * a fallback for unsupported argument combinations.
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
    sixel_allocator_t *allocator;
    sixel_dither_t *dither;
    size_t row_bytes;
    int width;
    int height;
    int band_height;
    int overlap;
    int method_for_diffuse;
    int method_for_scan;
    int method_for_carry;
    int optimize_palette;
    int optimize_palette_entries;
    int complexion;
    int lut_policy;
    int method_for_largest;
    int reqcolor;
    int pixelformat;
    sixel_parallel_logger_t *logger;
} sixel_parallel_dither_plan_t;

static int
sixel_dither_parallel_worker(tp_job_t job,
                             void *userdata,
                             void *workspace)
{
    sixel_parallel_dither_plan_t *plan;
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
    SIXELSTATUS status;

    (void)workspace;

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
    copy = (unsigned char *)malloc(required);
    if (copy == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    offset = (size_t)in0 * plan->row_bytes;
    memcpy(copy, plan->pixels + offset, required);

    if (plan->logger != NULL) {
        sixel_parallel_logger_logf(plan->logger,
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
    }

    local_ncolors = plan->reqcolor;
    /*
     * Map directly into the shared destination but suppress writes
     * before output_start.  The overlap rows are computed only to warm
     * up the error diffusion and are discarded by the output_start
     * check in the renderer, so neighboring bands never clobber each
     * other's body.
     */
    status = sixel_dither_map_pixels(plan->dest + (size_t)in0 * plan->width,
                                     copy,
                                     plan->width,
                                     rows,
                                     in0,
                                     y0,
                                     3,
                                     plan->palette->entries,
                                     plan->reqcolor,
                                     plan->method_for_diffuse,
                                     plan->method_for_scan,
                                     plan->method_for_carry,
                                     plan->optimize_palette,
                                     plan->optimize_palette_entries,
                                     plan->complexion,
                                     plan->lut_policy,
                                     plan->method_for_largest,
                                     NULL,
                                     &local_ncolors,
                                     plan->allocator,
                                     plan->dither,
                                     plan->pixelformat);
    if (plan->logger != NULL) {
        sixel_parallel_logger_logf(plan->logger,
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
    }
    free(copy);
    return status;
}

static SIXELSTATUS
sixel_dither_apply_palette_parallel(sixel_parallel_dither_plan_t *plan,
                                    int threads)
{
    SIXELSTATUS status;
    threadpool_t *pool;
    size_t depth_bytes;
    int nbands;
    int queue_depth;
    int band_index;
    int stride;
    int offset;

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

    pool = threadpool_create(threads,
                             queue_depth,
                             0,
                             sixel_dither_parallel_worker,
                             plan);
    if (pool == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

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
sixel_dither_resolve_indexes(sixel_index_t *result,
                             unsigned char *data,
                             int width,
                             int height,
                             int depth,
                             sixel_palette_t *palette,
                             int reqcolor,
                             int method_for_diffuse,
                             int method_for_scan,
                             int method_for_carry,
                             int foptimize,
                             int foptimize_palette,
                             int complexion,
                              int lut_policy,
                              int method_for_largest,
                              int *ncolors,
                              sixel_allocator_t *allocator,
                              sixel_dither_t *dither,
                              int pixelformat)
{
    SIXELSTATUS status = SIXEL_FALSE;

    if (palette == NULL || palette->entries == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

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
                                     method_for_carry,
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
    (*ppdither)->optimized = 0;
    (*ppdither)->optimize_palette = 0;
    (*ppdither)->complexion = 1;
    (*ppdither)->bodyonly = 0;
    (*ppdither)->method_for_largest = SIXEL_LARGE_NORM;
    (*ppdither)->method_for_rep = SIXEL_REP_CENTER_BOX;
    (*ppdither)->method_for_diffuse = SIXEL_DIFFUSE_FS;
    (*ppdither)->method_for_scan = SIXEL_SCAN_AUTO;
    (*ppdither)->method_for_carry = SIXEL_CARRY_AUTO;
    (*ppdither)->quality_mode = quality_mode;
    (*ppdither)->requested_quality_mode = quality_mode;
    (*ppdither)->pixelformat = SIXEL_PIXELFORMAT_RGB888;
    (*ppdither)->prefer_rgbfloat32 = sixel_dither_env_wants_float32();
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
    (*ppdither)->pipeline_image_height = 0;
    (*ppdither)->pipeline_logger = NULL;

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
        sixel_allocator_free(allocator, dither);
        sixel_allocator_unref(allocator);
    }
}


SIXELAPI void
sixel_dither_ref(
    sixel_dither_t  /* in */ *dither)
{
    /* TODO: be thread safe */
    ++dither->ref;
}


SIXELAPI void
sixel_dither_unref(
    sixel_dither_t  /* in */ *dither)
{
    /* TODO: be thread safe */
    if (dither != NULL && --dither->ref == 0) {
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
    float *float_pixels = NULL;
    unsigned char *input_pixels;
    SIXELSTATUS status = SIXEL_FALSE;
    size_t total_pixels;
    unsigned int payload_length;
    int palette_pixelformat;
    int prefer_rgbfloat32;

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
    prefer_rgbfloat32 = dither->prefer_rgbfloat32;

    switch (pixelformat) {
    case SIXEL_PIXELFORMAT_RGB888:
        input_pixels = data;
        break;
    case SIXEL_PIXELFORMAT_RGBFLOAT32:
    case SIXEL_PIXELFORMAT_LINEARRGBFLOAT32:
    case SIXEL_PIXELFORMAT_OKLABFLOAT32:
        if (prefer_rgbfloat32) {
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

    if (prefer_rgbfloat32
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
            prefer_rgbfloat32 = 0;
            status = SIXEL_OK;
        }
    }

    dither->prefer_rgbfloat32 = prefer_rgbfloat32;

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
                                        dither->prefer_rgbfloat32,
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
        || lut_policy == SIXEL_LUT_POLICY_NONE) {
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


/* set carry buffer mode for diffusion */
SIXELAPI void
sixel_dither_set_diffusion_carry(
    sixel_dither_t  /* in */ *dither,
    int             /* in */ method_for_carry)
{
    if (method_for_carry != SIXEL_CARRY_AUTO &&
            method_for_carry != SIXEL_CARRY_DISABLE &&
            method_for_carry != SIXEL_CARRY_ENABLE) {
        method_for_carry = SIXEL_CARRY_AUTO;
    }
    dither->method_for_carry = method_for_carry;
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


/* set whether omitting palette difinition */
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
    dither->pixelformat = pixelformat;
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
        || final_merge == SIXEL_FINAL_MERGE_WARD
        || final_merge == SIXEL_FINAL_MERGE_HKMEANS) {
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
    dither->keycolor = transparent;
}


/* set transparent */
sixel_index_t *
sixel_dither_apply_palette(
    sixel_dither_t  /* in */ *dither,
    unsigned char   /* in */ *pixels,
    int             /* in */ width,
    int             /* in */ height)
{
    SIXELSTATUS status = SIXEL_FALSE;
    size_t bufsize;
    size_t normalized_size;
    size_t total_pixels;
    sixel_index_t *dest = NULL;
    int ncolors;
    int method_for_scan;
    int method_for_carry;
    unsigned char *normalized_pixels = NULL;
    unsigned char *input_pixels;
    float *float_pipeline_pixels = NULL;
    int owns_float_pipeline;
    int pipeline_pixelformat;
    int prefer_float_pipeline;
    int palette_probe_active;
    double palette_started_at;
    double palette_finished_at;
    double palette_duration;
    sixel_palette_t *palette;
    int dest_owned;
    int parallel_active;
    int parallel_band_height;
    int parallel_overlap;
    int parallel_threads;
    sixel_parallel_logger_t *parallel_logger;

    /* ensure dither object is not null */
    if (dither == NULL) {
        sixel_helper_set_additional_message(
            "sixel_dither_apply_palette: dither is null.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    sixel_dither_ref(dither);

    palette = dither->palette;
    if (palette == NULL) {
        sixel_helper_set_additional_message(
            "sixel_dither_apply_palette: palette is null.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    parallel_active = dither->pipeline_parallel_active;
    parallel_band_height = dither->pipeline_band_height;
    parallel_overlap = dither->pipeline_band_overlap;
    parallel_threads = dither->pipeline_dither_threads;
    parallel_logger = dither->pipeline_logger;

    if (parallel_active && dither->optimize_palette != 0) {
        /*
         * Palette minimization rewrites the palette entries in place.
         * Parallel bands would race on the shared table, so fall back to
         * the serial path when the feature is active.
         */
        parallel_active = 0;
    }

    bufsize = (size_t)(width * height) * sizeof(sixel_index_t);
    total_pixels = (size_t)width * (size_t)height;
    owns_float_pipeline = 0;
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
            int wR;
            int wG;
            int wB;

            policy = dither->lut_policy;
            if (policy != SIXEL_LUT_POLICY_CERTLUT
                && policy != SIXEL_LUT_POLICY_5BIT
                && policy != SIXEL_LUT_POLICY_6BIT) {
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
                    wR = dither->complexion * 299;
                    wG = 587;
                    wB = 114;
                } else {
                    wR = dither->complexion;
                    wG = 1;
                    wB = 1;
                }
            } else {
                wR = dither->complexion;
                wG = 1;
                wB = 1;
            }
            status = sixel_lut_configure(palette->lut,
                                         palette->entries,
                                         palette->depth,
                                         (int)palette->entry_count,
                                         dither->complexion,
                                         wR,
                                         wG,
                                         wB,
                                         policy,
                                         dither->pixelformat);
            if (SIXEL_FAILED(status)) {
                sixel_helper_set_additional_message(
                    "sixel_dither_apply_palette: lut configuration failed.");
                goto end;
            }
        }
    }

    owns_float_pipeline = 0;
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
        status = sixel_helper_normalize_pixelformat(normalized_pixels,
                                                    &dither->pixelformat,
                                                    pixels, dither->pixelformat,
                                                    width, height);
        if (SIXEL_FAILED(status)) {
            goto end;
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
        method_for_scan = SIXEL_SCAN_RASTER;
    }

    method_for_carry = dither->method_for_carry;
    if (method_for_carry == SIXEL_CARRY_AUTO) {
        method_for_carry = SIXEL_CARRY_DISABLE;
    }

    palette_probe_active = sixel_assessment_palette_probe_enabled();
    palette_started_at = 0.0;
    palette_finished_at = 0.0;
    palette_duration = 0.0;
    if (palette_probe_active) {
        /*
         * Palette spans execute inside the encode stage.  We sample the
         * duration here so the assessment can reassign the work to the
         * PaletteApply bucket.
         */
        palette_started_at = sixel_assessment_timer_now();
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
        plan.method_for_carry = method_for_carry;
        plan.optimize_palette = dither->optimized;
        plan.optimize_palette_entries = dither->optimize_palette;
        plan.complexion = dither->complexion;
        plan.lut_policy = dither->lut_policy;
        plan.method_for_largest = dither->method_for_largest;
        plan.reqcolor = dither->ncolors;
        plan.pixelformat = pipeline_pixelformat;
        plan.logger = parallel_logger;

        status = sixel_dither_apply_palette_parallel(&plan,
                                                     parallel_threads);
        ncolors = dither->ncolors;
    } else
#endif
    {
        status = sixel_dither_resolve_indexes(dest,
                                              input_pixels,
                                              width,
                                              height,
                                              3,
                                              palette,
                                              dither->ncolors,
                                              dither->method_for_diffuse,
                                              method_for_scan,
                                              method_for_carry,
                                              dither->optimized,
                                              dither->optimize_palette,
                                              dither->complexion,
                                              dither->lut_policy,
                                              dither->method_for_largest,
                                              &ncolors,
                                              dither->allocator,
                                              dither,
                                              pipeline_pixelformat);
    }
    if (palette_probe_active) {
        palette_finished_at = sixel_assessment_timer_now();
        palette_duration = palette_finished_at - palette_started_at;
        if (palette_duration < 0.0) {
            palette_duration = 0.0;
        }
        sixel_assessment_record_palette_apply_span(palette_duration);
    }
    if (SIXEL_FAILED(status)) {
        if (dest != NULL && dest_owned) {
            sixel_allocator_free(dither->allocator, dest);
        }
        dest = NULL;
        goto end;
    }

    dither->ncolors = ncolors;
    palette->entry_count = (unsigned int)ncolors;

end:
    if (normalized_pixels != NULL) {
        sixel_allocator_free(dither->allocator, normalized_pixels);
    }
    if (float_pipeline_pixels != NULL && owns_float_pipeline) {
        sixel_allocator_free(dither->allocator, float_pipeline_pixels);
    }
    sixel_dither_unref(dither);
    dither->pipeline_index_buffer = NULL;
    dither->pipeline_index_owned = 0;
    dither->pipeline_index_size = 0;
    dither->pipeline_parallel_active = 0;
    dither->pipeline_band_height = 0;
    dither->pipeline_band_overlap = 0;
    dither->pipeline_dither_threads = 0;
    dither->pipeline_image_height = 0;
    dither->pipeline_logger = NULL;
    return dest;
}


#if HAVE_TESTS
static int
test1(void)
{
    sixel_dither_t *dither = NULL;
    int nret = EXIT_FAILURE;
    SIXELSTATUS status;

    status = sixel_dither_new(&dither, 2, NULL);
    if (dither == NULL || status != SIXEL_OK) {
        goto error;
    }
    sixel_dither_ref(dither);
    sixel_dither_unref(dither);
    sixel_dither_unref(dither);
    nret = EXIT_SUCCESS;

error:
    sixel_dither_unref(dither);
    return nret;
}

static int
test2(void)
{
    sixel_dither_t *dither = NULL;
    int nret = EXIT_FAILURE;
    SIXELSTATUS status;

    status = sixel_dither_new(&dither, INT_MAX, NULL);
    if (status != SIXEL_BAD_INPUT || dither != NULL) {
        goto error;
    }

    nret = EXIT_SUCCESS;

error:
    return nret;
}

/* ensure the float32 flag honours the environment defaults */
static int
test_float_env_disabled(void)
{
    sixel_dither_t *dither = NULL;
    int nret = EXIT_FAILURE;
    SIXELSTATUS status;

    sixel_dither_tests_reset_float32_flag_cache();
    sixel_palette_tests_reset_last_engine();
    sixel_compat_setenv(SIXEL_FLOAT32_ENVVAR, "0");

    status = sixel_dither_new(&dither, 4, NULL);
    if (SIXEL_FAILED(status) || dither == NULL) {
        goto error;
    }
    if (dither->prefer_rgbfloat32 != 0) {
        goto error;
    }

    nret = EXIT_SUCCESS;

error:
    sixel_dither_unref(dither);
    return nret;
}

/* ensure empty environment assignments keep the legacy pipeline */
static int
test_float_env_empty_string_disables(void)
{
    sixel_dither_t *dither;
    SIXELSTATUS status;
    int nret;

    dither = NULL;
    status = SIXEL_FALSE;
    nret = EXIT_FAILURE;
    sixel_dither_tests_reset_float32_flag_cache();
    sixel_palette_tests_reset_last_engine();
    sixel_compat_setenv(SIXEL_FLOAT32_ENVVAR, "");

    status = sixel_dither_new(&dither, 4, NULL);
    if (SIXEL_FAILED(status) || dither == NULL) {
        goto error;
    }
    if (dither->prefer_rgbfloat32 != 0) {
        goto error;
    }

    nret = EXIT_SUCCESS;

error:
    sixel_dither_unref(dither);
    sixel_palette_tests_reset_last_engine();
    sixel_compat_setenv(SIXEL_FLOAT32_ENVVAR, "0");
    sixel_dither_tests_reset_float32_flag_cache();
    return nret;
}

/* ensure the float32 route dispatches the RGBFLOAT32 quantizer */
static int
test_float_env_enables_quantizer(void)
{
    static unsigned char const pixels[] = {
        0x00, 0x00, 0x00,
        0xff, 0xff, 0xff,
    };
    sixel_dither_t *dither = NULL;
    SIXELSTATUS status;
    int nret;

    nret = EXIT_FAILURE;
    sixel_dither_tests_reset_float32_flag_cache();
    sixel_palette_tests_reset_last_engine();
    sixel_compat_setenv(SIXEL_FLOAT32_ENVVAR, "1");

    status = sixel_dither_new(&dither, 2, NULL);
    if (SIXEL_FAILED(status) || dither == NULL) {
        goto error;
    }
    if (dither->prefer_rgbfloat32 == 0) {
        goto error;
    }

    status = sixel_dither_initialize(dither,
                                     (unsigned char *)pixels,
                                     2,
                                     1,
                                     SIXEL_PIXELFORMAT_RGB888,
                                     SIXEL_LARGE_AUTO,
                                     SIXEL_REP_AUTO,
                                     SIXEL_QUALITY_AUTO);
    if (SIXEL_FAILED(status)) {
        goto error;
    }
    if (!sixel_palette_tests_last_engine_requires_float32()) {
        goto error;
    }
    if (sixel_palette_tests_last_engine_model()
            != SIXEL_QUANTIZE_MODEL_MEDIANCUT) {
        goto error;
    }

    nret = EXIT_SUCCESS;

error:
    sixel_dither_unref(dither);
    sixel_palette_tests_reset_last_engine();
    sixel_compat_setenv(SIXEL_FLOAT32_ENVVAR, "0");
    sixel_dither_tests_reset_float32_flag_cache();
    return nret;
}

/* ensure explicit Heckbert requests also take the float32 route */
static int
test_float_env_explicit_mediancut_dispatch(void)
{
    static unsigned char const pixels[] = {
        0x80, 0x20, 0x20,
        0x20, 0x80, 0x20,
    };
    sixel_dither_t *dither = NULL;
    SIXELSTATUS status;
    int nret;

    nret = EXIT_FAILURE;
    sixel_dither_tests_reset_float32_flag_cache();
    sixel_palette_tests_reset_last_engine();
    sixel_compat_setenv(SIXEL_FLOAT32_ENVVAR, "1");

    status = sixel_dither_new(&dither, 2, NULL);
    if (SIXEL_FAILED(status) || dither == NULL) {
        goto error;
    }
    if (dither->prefer_rgbfloat32 == 0) {
        goto error;
    }
    dither->quantize_model = SIXEL_QUANTIZE_MODEL_MEDIANCUT;

    status = sixel_dither_initialize(dither,
                                     (unsigned char *)pixels,
                                     2,
                                     1,
                                     SIXEL_PIXELFORMAT_RGB888,
                                     SIXEL_LARGE_AUTO,
                                     SIXEL_REP_AUTO,
                                     SIXEL_QUALITY_AUTO);
    if (SIXEL_FAILED(status)) {
        goto error;
    }
    if (!sixel_palette_tests_last_engine_requires_float32()) {
        goto error;
    }
    if (sixel_palette_tests_last_engine_model()
            != SIXEL_QUANTIZE_MODEL_MEDIANCUT) {
        goto error;
    }

    nret = EXIT_SUCCESS;

error:
    sixel_dither_unref(dither);
    sixel_palette_tests_reset_last_engine();
    sixel_compat_setenv(SIXEL_FLOAT32_ENVVAR, "0");
    sixel_dither_tests_reset_float32_flag_cache();
    return nret;
}

/* ensure the float32 diffusion route is exercised for FS dithering */
static int
test_float_fs_diffusion_dispatch(void)
{
    static float pixels[] = {
        0.10f, 0.20f, 0.30f,
        0.85f, 0.60f, 0.40f,
    };
    sixel_dither_t *dither;
    sixel_index_t *indexes;
    SIXELSTATUS status;
    int nret;

    dither = NULL;
    indexes = NULL;
    nret = EXIT_FAILURE;
    sixel_dither_tests_reset_float32_flag_cache();
    sixel_palette_tests_reset_last_engine();
    sixel_dither_diffusion_tests_reset_float32_hits();
    sixel_compat_setenv(SIXEL_FLOAT32_ENVVAR, "1");

    status = sixel_dither_new(&dither, 2, NULL);
    if (SIXEL_FAILED(status) || dither == NULL) {
        goto error;
    }
    sixel_dither_set_pixelformat(dither, SIXEL_PIXELFORMAT_RGBFLOAT32);
    sixel_dither_set_diffusion_type(dither, SIXEL_DIFFUSE_FS);
    sixel_dither_set_diffusion_carry(dither, SIXEL_CARRY_DISABLE);

    status = sixel_dither_initialize(dither,
                                     (unsigned char *)pixels,
                                     2,
                                     1,
                                     SIXEL_PIXELFORMAT_RGBFLOAT32,
                                     SIXEL_LARGE_AUTO,
                                     SIXEL_REP_AUTO,
                                     SIXEL_QUALITY_AUTO);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    indexes = sixel_dither_apply_palette(dither,
                                         (unsigned char *)pixels,
                                         2,
                                         1);
    if (indexes == NULL) {
        goto error;
    }
    if (sixel_dither_diffusion_tests_float32_hits() <= 0) {
        goto error;
    }

    nret = EXIT_SUCCESS;

error:
    if (indexes != NULL && dither != NULL) {
        sixel_allocator_free(dither->allocator, indexes);
    }
    sixel_dither_unref(dither);
    sixel_palette_tests_reset_last_engine();
    sixel_compat_setenv(SIXEL_FLOAT32_ENVVAR, "0");
    sixel_dither_tests_reset_float32_flag_cache();
    sixel_dither_diffusion_tests_reset_float32_hits();
    return nret;
}

/* helpers shared by the float32 palette validation tests */
static unsigned char
test_float_component_to_u8(float value)
{
    double channel;

    channel = (double)value;
    if (channel < 0.0) {
        channel = 0.0;
    }
    if (channel > 1.0) {
        channel = 1.0;
    }
    channel = channel * 255.0 + 0.5;
    if (channel < 0.0) {
        channel = 0.0;
    }
    if (channel > 255.0) {
        channel = 255.0;
    }

    return (unsigned char)channel;
}

static int
test_palette_float_matches(float const *float_entries,
                           size_t entry_count,
                           unsigned char const *u8_entries,
                           size_t u8_count)
{
    size_t index;
    size_t base;
    unsigned char red;
    unsigned char green;
    unsigned char blue;

    if (float_entries == NULL || u8_entries == NULL) {
        return 0;
    }
    if (entry_count != u8_count) {
        return 0;
    }
    if (entry_count == 0U) {
        return 1;
    }

    for (index = 0U; index < entry_count; ++index) {
        base = index * 3U;
        red = test_float_component_to_u8(float_entries[base + 0U]);
        green = test_float_component_to_u8(float_entries[base + 1U]);
        blue = test_float_component_to_u8(float_entries[base + 2U]);
        if (red != u8_entries[base + 0U]
                || green != u8_entries[base + 1U]
                || blue != u8_entries[base + 2U]) {
            return 0;
        }
    }

    return 1;
}

/* ensure palette optimization copies float buffers in lockstep */
static int
test_float_optimize_palette_sync(void)
{
    static float pixels[] = {
        1.0f, 0.9f, 0.0f,
        1.0f, 0.9f, 0.0f,
        0.1f, 0.2f, 0.9f,
        0.1f, 0.2f, 0.9f,
    };
    sixel_dither_t *dither;
    sixel_palette_t *palette_obj;
    SIXELSTATUS status;
    sixel_index_t *indexes;
    float *palette_float_copy;
    unsigned char *palette_u8_copy;
    size_t palette_float_count;
    size_t palette_u8_count;
    int nret;

    dither = NULL;
    palette_obj = NULL;
    status = SIXEL_FALSE;
    indexes = NULL;
    palette_float_copy = NULL;
    palette_u8_copy = NULL;
    palette_float_count = 0U;
    palette_u8_count = 0U;
    nret = EXIT_FAILURE;

    sixel_dither_tests_reset_float32_flag_cache();
    sixel_palette_tests_reset_last_engine();
    sixel_compat_setenv(SIXEL_FLOAT32_ENVVAR, "1");

    status = sixel_dither_new(&dither, 4, NULL);
    if (SIXEL_FAILED(status) || dither == NULL) {
        goto error;
    }
    sixel_dither_set_pixelformat(dither, SIXEL_PIXELFORMAT_RGBFLOAT32);
    sixel_dither_set_diffusion_type(dither, SIXEL_DIFFUSE_FS);
    sixel_dither_set_diffusion_carry(dither, SIXEL_CARRY_DISABLE);
    sixel_dither_set_optimize_palette(dither, 1);

    status = sixel_dither_initialize(dither,
                                     (unsigned char *)pixels,
                                     2,
                                     2,
                                     SIXEL_PIXELFORMAT_RGBFLOAT32,
                                     SIXEL_LARGE_AUTO,
                                     SIXEL_REP_AUTO,
                                     SIXEL_QUALITY_AUTO);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    indexes = sixel_dither_apply_palette(dither,
                                         (unsigned char *)pixels,
                                         2,
                                         2);
    if (indexes == NULL) {
        goto error;
    }

    status = sixel_dither_get_quantized_palette(dither, &palette_obj);
    if (SIXEL_FAILED(status) || palette_obj == NULL) {
        goto error;
    }

    status = sixel_palette_copy_entries_float32(
        palette_obj,
        &palette_float_copy,
        &palette_float_count,
        SIXEL_PIXELFORMAT_RGBFLOAT32,
        dither->allocator);
    if (SIXEL_FAILED(status) || palette_float_copy == NULL) {
        goto error;
    }

    status = sixel_palette_copy_entries_8bit(
        palette_obj,
        &palette_u8_copy,
        &palette_u8_count,
        SIXEL_PIXELFORMAT_RGB888,
        dither->allocator);
    if (SIXEL_FAILED(status) || palette_u8_copy == NULL) {
        goto error;
    }

    if (palette_float_count == 0U || palette_u8_count == 0U) {
        goto error;
    }
    if (!test_palette_float_matches(palette_float_copy,
                                    palette_float_count,
                                    palette_u8_copy,
                                    palette_u8_count)) {
        goto error;
    }

    nret = EXIT_SUCCESS;

error:
    if (indexes != NULL && dither != NULL) {
        sixel_allocator_free(dither->allocator, indexes);
    }
    if (palette_obj != NULL) {
        sixel_palette_unref(palette_obj);
    }
    if (palette_float_copy != NULL && dither != NULL) {
        sixel_allocator_free(dither->allocator, palette_float_copy);
    }
    if (palette_u8_copy != NULL && dither != NULL) {
        sixel_allocator_free(dither->allocator, palette_u8_copy);
    }
    sixel_dither_unref(dither);
    sixel_palette_tests_reset_last_engine();
    sixel_compat_setenv(SIXEL_FLOAT32_ENVVAR, "0");
    sixel_dither_tests_reset_float32_flag_cache();
    return nret;
}

/* ensure float32 palettes remain accessible alongside legacy buffers */
static int
test_float_palette_accessor_returns_data(void)
{
    static float pixels[] = {
        1.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
    };
    sixel_dither_t *dither;
    SIXELSTATUS status;
    int nret;
    int colors;
    int red_float;
    int green_float;
    int red_u8;
    int green_u8;
    int index;
    float *entry_float;
    unsigned char *entry_u8;
    sixel_palette_t *palette_obj;
    float *palette_float_copy;
    unsigned char *palette_u8_copy;
    size_t palette_float_count;
    size_t palette_u8_count;

    dither = NULL;
    status = SIXEL_FALSE;
    palette_float_copy = NULL;
    palette_u8_copy = NULL;
    palette_obj = NULL;
    palette_float_count = 0U;
    palette_u8_count = 0U;
    nret = EXIT_FAILURE;
    colors = 0;
    red_float = 0;
    green_float = 0;
    red_u8 = 0;
    green_u8 = 0;
    index = 0;
    entry_float = NULL;
    entry_u8 = NULL;

    sixel_dither_tests_reset_float32_flag_cache();
    sixel_palette_tests_reset_last_engine();
    sixel_compat_setenv(SIXEL_FLOAT32_ENVVAR, "1");

    status = sixel_dither_new(&dither, 2, NULL);
    if (SIXEL_FAILED(status) || dither == NULL) {
        goto error;
    }
    sixel_dither_set_pixelformat(dither, SIXEL_PIXELFORMAT_RGBFLOAT32);
    status = sixel_dither_initialize(dither,
                                     (unsigned char *)pixels,
                                     2,
                                     1,
                                     SIXEL_PIXELFORMAT_RGBFLOAT32,
                                     SIXEL_LARGE_AUTO,
                                     SIXEL_REP_AUTO,
                                     SIXEL_QUALITY_AUTO);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    status = sixel_dither_get_quantized_palette(dither, &palette_obj);
    if (SIXEL_FAILED(status)) {
        goto error;
    }
    status = sixel_palette_copy_entries_float32(
        palette_obj,
        &palette_float_copy,
        &palette_float_count,
        SIXEL_PIXELFORMAT_RGBFLOAT32,
        dither->allocator);
    if (SIXEL_FAILED(status) || palette_float_copy == NULL) {
        goto error;
    }
    status = sixel_palette_copy_entries_8bit(
        palette_obj,
        &palette_u8_copy,
        &palette_u8_count,
        SIXEL_PIXELFORMAT_RGB888,
        dither->allocator);
    if (SIXEL_FAILED(status) || palette_u8_copy == NULL) {
        goto error;
    }
    sixel_palette_unref(palette_obj);
    palette_obj = NULL;
    colors = sixel_dither_get_num_of_palette_colors(dither);
    if (colors != 2) {
        goto error;
    }

    for (index = 0; index < colors; ++index) {
        entry_float = palette_float_copy + (size_t)index * 3U;
        if (entry_float[0] > 0.9f && entry_float[1] < 0.1f
                && entry_float[2] < 0.1f) {
            red_float = 1;
        }
        if (entry_float[1] > 0.9f && entry_float[0] < 0.1f
                && entry_float[2] < 0.1f) {
            green_float = 1;
        }

        entry_u8 = palette_u8_copy + (size_t)index * 3U;
        if (entry_u8[0] > 200U && entry_u8[1] < 30U && entry_u8[2] < 30U) {
            red_u8 = 1;
        }
        if (entry_u8[1] > 200U && entry_u8[0] < 30U && entry_u8[2] < 30U) {
            green_u8 = 1;
        }
    }
    if (!red_float || !green_float || !red_u8 || !green_u8) {
        goto error;
    }

    nret = EXIT_SUCCESS;

error:
    sixel_dither_unref(dither);
    sixel_palette_tests_reset_last_engine();
    sixel_compat_setenv(SIXEL_FLOAT32_ENVVAR, "0");
    sixel_dither_tests_reset_float32_flag_cache();
    return nret;
}

/* ensure the float accessor reports NULL for legacy 8bit runs */
static int
test_float_palette_accessor_legacy_null(void)
{
    static unsigned char const pixels[] = {
        0xff, 0x00, 0x00,
        0x00, 0xff, 0x00,
    };
    sixel_dither_t *dither;
    SIXELSTATUS status;
    sixel_palette_t *palette_obj;
    float *palette_float_copy;
    unsigned char *palette_u8_copy;
    size_t palette_float_count;
    size_t palette_u8_count;
    int nret;

    dither = NULL;
    status = SIXEL_FALSE;
    palette_obj = NULL;
    palette_float_copy = NULL;
    palette_u8_copy = NULL;
    palette_float_count = 0U;
    palette_u8_count = 0U;
    nret = EXIT_FAILURE;
    sixel_dither_tests_reset_float32_flag_cache();
    sixel_palette_tests_reset_last_engine();
    sixel_compat_setenv(SIXEL_FLOAT32_ENVVAR, "0");

    status = sixel_dither_new(&dither, 2, NULL);
    if (SIXEL_FAILED(status) || dither == NULL) {
        goto error;
    }
    sixel_dither_set_pixelformat(dither, SIXEL_PIXELFORMAT_RGB888);
    status = sixel_dither_initialize(dither,
                                     (unsigned char *)pixels,
                                     2,
                                     1,
                                     SIXEL_PIXELFORMAT_RGB888,
                                     SIXEL_LARGE_AUTO,
                                     SIXEL_REP_AUTO,
                                     SIXEL_QUALITY_AUTO);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    status = sixel_dither_get_quantized_palette(dither, &palette_obj);
    if (SIXEL_FAILED(status)) {
        goto error;
    }
    status = sixel_palette_copy_entries_float32(
        palette_obj,
        &palette_float_copy,
        &palette_float_count,
        SIXEL_PIXELFORMAT_RGBFLOAT32,
        dither->allocator);
    if (SIXEL_FAILED(status)) {
        goto error;
    }
    if (palette_float_copy != NULL) {
        goto error;
    }
    status = sixel_palette_copy_entries_8bit(
        palette_obj,
        &palette_u8_copy,
        &palette_u8_count,
        SIXEL_PIXELFORMAT_RGB888,
        dither->allocator);
    if (SIXEL_FAILED(status) || palette_u8_copy == NULL) {
        goto error;
    }
    sixel_palette_unref(palette_obj);
    palette_obj = NULL;

    nret = EXIT_SUCCESS;

error:
    if (palette_obj != NULL) {
        sixel_palette_unref(palette_obj);
    }
    if (palette_float_copy != NULL && dither != NULL) {
        sixel_allocator_free(dither->allocator, palette_float_copy);
    }
    if (palette_u8_copy != NULL && dither != NULL) {
        sixel_allocator_free(dither->allocator, palette_u8_copy);
    }
    sixel_dither_unref(dither);
    sixel_palette_tests_reset_last_engine();
    sixel_dither_tests_reset_float32_flag_cache();
    return nret;
}


SIXELAPI int
sixel_dither_tests_main(void)
{
    int nret = EXIT_FAILURE;
    size_t i;
    typedef int (* testcase)(void);

    static testcase const testcases[] = {
        test1,
        test2,
        test_float_env_disabled,
        test_float_env_empty_string_disables,
        test_float_env_enables_quantizer,
        test_float_env_explicit_mediancut_dispatch,
        test_float_fs_diffusion_dispatch,
        test_float_optimize_palette_sync,
        test_float_palette_accessor_returns_data,
        test_float_palette_accessor_legacy_null,
    };

    for (i = 0; i < sizeof(testcases) / sizeof(testcase); ++i) {
        nret = testcases[i]();
        if (nret != EXIT_SUCCESS) {
            goto error;
        }
    }

    nret = EXIT_SUCCESS;

error:
    return nret;
}
#endif  /* HAVE_TESTS */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
