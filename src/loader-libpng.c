/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2021-2025 libsixel developers. See `AUTHORS`.
 * Copyright (c) 2014-2019 Hayaki Saito
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
 *
 * libpng-backed loader helpers extracted from loader.c. Concentrating PNG
 * handling here keeps the main registry lean and isolates libpng headers from
 * other backends while preserving the existing control flow and diagnostics.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#if HAVE_LIBPNG

#if !defined(_POSIX_C_SOURCE)
# define _POSIX_C_SOURCE 200809L
#endif

#include <stdio.h>
#include <setjmp.h>

#if HAVE_STRING_H
# include <string.h>
#endif
#if HAVE_ERRNO_H
# include <errno.h>
#endif
#if HAVE_LIMITS_H
# include <limits.h>
#endif
#if HAVE_STDLIB_H
# include <stdlib.h>
#endif
#if HAVE_STDARG_H
# include <stdarg.h>
#endif
#if HAVE_STDINT_H
# include <stdint.h>
#endif
#if HAVE_MATH_H
# include <math.h>
#endif

#include <png.h>

#include <sixel.h>

#include "allocator.h"
#include "cms.h"
#include "chunk-view.h"
#include "compat_stub.h"
#include "loader-common.h"
#include "frame-private.h"
#include "frame-factory.h"
#include "loader.h"
#include "loader-libpng.h"
#include "timeline-logger.h"
#include "options.h"

typedef struct sixel_loader_libpng_component {
    sixel_loader_component_t base;
    sixel_allocator_t *allocator;
    unsigned int ref;
    int fstatic;
    int fuse_palette;
    int reqcolors;
    int loop_control;
    int has_bgcolor;
    unsigned char bgcolor[3];
    int has_start_frame_no;
    int start_frame_no;
    int enable_cms;
    int enable_orientation;
} sixel_loader_libpng_component_t;

static int
loader_can_try_libpng(sixel_chunk_t const *chunk);

/*
 * Topic-scoped APNG decoder diagnostics.
 *
 * Enabled when SIXEL_TRACE_TOPIC includes "apng_decode".
 * Supported separators follow options.c behavior:
 * comma, colon, semicolon, and ASCII whitespace.
 */
static int
apng_decode_trace_is_enabled(void)
{
    return sixel_trace_topic_is_enabled("apng_decode");
}

static void
apng_decode_trace_message(char const *format, ...)
{
    va_list args;

    if (!apng_decode_trace_is_enabled()) {
        return;
    }

    fprintf(stderr, "libsixel[apng_decode]: ");
    va_start(args, format);
    sixel_compat_vfprintf(stderr, format, args);
    va_end(args);
    fputc('\n', stderr);
}

#if HAVE_DEBUG
static int
libpng_debug_trace_is_enabled(void)
{
    if (loader_trace_is_enabled()) {
        return 1;
    }
    if (sixel_trace_topic_is_enabled("loader")) {
        return 1;
    }

    return 0;
}

# define LIBPNG_DEBUG_LOG(...)                                          \
    do {                                                                 \
        if (libpng_debug_trace_is_enabled()) {                           \
            fprintf(stderr, __VA_ARGS__);                                \
        }                                                                \
    } while (0)
#else
# define LIBPNG_DEBUG_LOG(...) do { } while (0)
#endif

static double
png_decode_srgb_unit(double value);

static double
png_encode_srgb_unit(double value);

static double
png_decode_source_unit(double value,
                       int transfer_mode,
                       double file_gamma);

typedef struct sixel_png_read_chunk {
    unsigned char const *buffer;
    size_t size;
    size_t offset;
} sixel_png_read_chunk_t;

static void
read_png(png_structp png_ptr,
         png_bytep data,
         png_size_t length)
{
    sixel_png_read_chunk_t *reader;
    size_t available;

    reader = (sixel_png_read_chunk_t *)png_get_io_ptr(png_ptr);
    available = 0u;
    if (reader == NULL || reader->buffer == NULL ||
        reader->offset > reader->size) {
        png_error(png_ptr, "Invalid PNG read context");
    }

    available = reader->size - reader->offset;
    if (length > available) {
        png_error(png_ptr, "Truncated PNG read");
    }
    if (length > 0) {
        memcpy(data, reader->buffer + reader->offset, length);
        reader->offset += length;
    }
}

static void
read_palette(png_structp png_ptr,
             png_infop info_ptr,
             unsigned char *palette,
             int ncolors,
             png_color *png_palette,
             int *transparent,
             unsigned char *zero_alpha_map,
             int *zero_alpha_count)
{
    png_bytep trans;
    int num_trans;
    int alpha;
    int key_index;
    int zero_count;
    int i;

    trans = NULL;
    num_trans = 0;
    alpha = 0xff;
    key_index = -1;
    zero_count = 0;
    i = 0;

    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
        png_get_tRNS(png_ptr, info_ptr, &trans, &num_trans, NULL);
    }
    if (num_trans > ncolors) {
        num_trans = ncolors;
    }
    if (zero_alpha_map != NULL && ncolors > 0) {
        memset(zero_alpha_map, 0, (size_t)ncolors);
    }
    for (i = 0; i < ncolors; ++i) {
        alpha = (i < num_trans) ? trans[i] : 0xff;
        if (alpha == 0) {
            if (key_index < 0) {
                key_index = i;
            }
            ++zero_count;
            if (zero_alpha_map != NULL) {
                zero_alpha_map[i] = 1;
            }
        }
        /* This retained-palette path has no background composition. */
        palette[i * 3 + 0] = png_palette[i].red;
        palette[i * 3 + 1] = png_palette[i].green;
        palette[i * 3 + 2] = png_palette[i].blue;
    }
    if (transparent != NULL) {
        *transparent = key_index;
    }
    if (zero_alpha_count != NULL) {
        *zero_alpha_count = zero_count;
    }
}

enum {
    SIXEL_PNG_TRANSFER_SRGB = 0,
    SIXEL_PNG_TRANSFER_GAMA = 1
};

static double
png_clamp_unit(double value)
{
    if (value < 0.0) {
        return 0.0;
    }
    if (value > 1.0) {
        return 1.0;
    }
    return value;
}

static double
png_decode_srgb_unit(double value)
{
    value = png_clamp_unit(value);
    if (value <= 0.04045) {
        return value / 12.92;
    }
    return pow((value + 0.055) / 1.055, 2.4);
}

static double
png_encode_srgb_unit(double value)
{
    value = png_clamp_unit(value);
    if (value <= 0.0031308) {
        return value * 12.92;
    }
    return 1.055 * pow(value, 1.0 / 2.4) - 0.055;
}

static double
png_decode_source_unit(double value, int transfer_mode, double file_gamma)
{
    value = png_clamp_unit(value);
    if (transfer_mode == SIXEL_PNG_TRANSFER_GAMA && file_gamma > 0.0) {
        return pow(value, 1.0 / file_gamma);
    }
    return png_decode_srgb_unit(value);
}

/*
 * Keep stack-matrix parameters non-const in these helpers.  Pre-C23 C treats
 * pointer-to-array qualifiers narrowly enough that passing ordinary stack
 * matrices to const-qualified parameters can trip -Wpedantic.
 */
static int
png_invert_3x3(double in[3][3], double out[3][3])
{
    double det;
    double inv_det;

    det = in[0][0] * (in[1][1] * in[2][2] - in[1][2] * in[2][1])
        - in[0][1] * (in[1][0] * in[2][2] - in[1][2] * in[2][0])
        + in[0][2] * (in[1][0] * in[2][1] - in[1][1] * in[2][0]);
    if (fabs(det) < 1.0e-12) {
        return 0;
    }
    inv_det = 1.0 / det;

    out[0][0] =  (in[1][1] * in[2][2] - in[1][2] * in[2][1]) * inv_det;
    out[0][1] = -(in[0][1] * in[2][2] - in[0][2] * in[2][1]) * inv_det;
    out[0][2] =  (in[0][1] * in[1][2] - in[0][2] * in[1][1]) * inv_det;
    out[1][0] = -(in[1][0] * in[2][2] - in[1][2] * in[2][0]) * inv_det;
    out[1][1] =  (in[0][0] * in[2][2] - in[0][2] * in[2][0]) * inv_det;
    out[1][2] = -(in[0][0] * in[1][2] - in[0][2] * in[1][0]) * inv_det;
    out[2][0] =  (in[1][0] * in[2][1] - in[1][1] * in[2][0]) * inv_det;
    out[2][1] = -(in[0][0] * in[2][1] - in[0][1] * in[2][0]) * inv_det;
    out[2][2] =  (in[0][0] * in[1][1] - in[0][1] * in[1][0]) * inv_det;
    return 1;
}

static int
png_build_chrm_to_srgb_matrix(double white_x,
                              double white_y,
                              double red_x,
                              double red_y,
                              double green_x,
                              double green_y,
                              double blue_x,
                              double blue_y,
                              double source_to_srgb[3][3])
{
    static double const xyz_to_srgb[3][3] = {
        { 3.240969941904521, -1.537383177570093, -0.498610760293003 },
        { -0.969243636280880, 1.875967501507721, 0.041555057407176 },
        { 0.055630079696993, -0.203976958888977, 1.056971514242878 }
    };
    double primaries[3][3];
    double primaries_inv[3][3];
    double source_to_xyz[3][3];
    double white_xyz[3];
    double scale[3];
    int row;
    int col;

    if (source_to_srgb == NULL) {
        return 0;
    }
    if (white_y <= 0.0 || red_y <= 0.0 || green_y <= 0.0 || blue_y <= 0.0) {
        return 0;
    }
    if (white_x < 0.0 || white_x + white_y >= 1.0 ||
        red_x < 0.0 || red_x + red_y >= 1.0 ||
        green_x < 0.0 || green_x + green_y >= 1.0 ||
        blue_x < 0.0 || blue_x + blue_y >= 1.0) {
        return 0;
    }

    primaries[0][0] = red_x / red_y;
    primaries[1][0] = 1.0;
    primaries[2][0] = (1.0 - red_x - red_y) / red_y;
    primaries[0][1] = green_x / green_y;
    primaries[1][1] = 1.0;
    primaries[2][1] = (1.0 - green_x - green_y) / green_y;
    primaries[0][2] = blue_x / blue_y;
    primaries[1][2] = 1.0;
    primaries[2][2] = (1.0 - blue_x - blue_y) / blue_y;

    if (!png_invert_3x3(primaries, primaries_inv)) {
        return 0;
    }

    white_xyz[0] = white_x / white_y;
    white_xyz[1] = 1.0;
    white_xyz[2] = (1.0 - white_x - white_y) / white_y;
    scale[0] = primaries_inv[0][0] * white_xyz[0]
             + primaries_inv[0][1] * white_xyz[1]
             + primaries_inv[0][2] * white_xyz[2];
    scale[1] = primaries_inv[1][0] * white_xyz[0]
             + primaries_inv[1][1] * white_xyz[1]
             + primaries_inv[1][2] * white_xyz[2];
    scale[2] = primaries_inv[2][0] * white_xyz[0]
             + primaries_inv[2][1] * white_xyz[1]
             + primaries_inv[2][2] * white_xyz[2];

    for (row = 0; row < 3; ++row) {
        source_to_xyz[row][0] = primaries[row][0] * scale[0];
        source_to_xyz[row][1] = primaries[row][1] * scale[1];
        source_to_xyz[row][2] = primaries[row][2] * scale[2];
    }

    for (row = 0; row < 3; ++row) {
        for (col = 0; col < 3; ++col) {
            source_to_srgb[row][col] =
                xyz_to_srgb[row][0] * source_to_xyz[0][col]
                + xyz_to_srgb[row][1] * source_to_xyz[1][col]
                + xyz_to_srgb[row][2] * source_to_xyz[2][col];
        }
    }
    return 1;
}

static void
png_apply_linear_matrix_triplet(double rgb[3],
                                double source_to_srgb[3][3])
{
    double in_r;
    double in_g;
    double in_b;
    double out_r;
    double out_g;
    double out_b;

    if (rgb == NULL || source_to_srgb == NULL) {
        return;
    }

    in_r = rgb[0];
    in_g = rgb[1];
    in_b = rgb[2];
    out_r = source_to_srgb[0][0] * in_r
          + source_to_srgb[0][1] * in_g
          + source_to_srgb[0][2] * in_b;
    out_g = source_to_srgb[1][0] * in_r
          + source_to_srgb[1][1] * in_g
          + source_to_srgb[1][2] * in_b;
    out_b = source_to_srgb[2][0] * in_r
          + source_to_srgb[2][1] * in_g
          + source_to_srgb[2][2] * in_b;
    rgb[0] = png_clamp_unit(out_r);
    rgb[1] = png_clamp_unit(out_g);
    rgb[2] = png_clamp_unit(out_b);
}

static void
png_apply_linear_matrix_float32(float *pixels,
                                size_t pixel_count,
                                double source_to_srgb[3][3])
{
    size_t index;
    size_t offset;
    double rgb[3];

    if (pixels == NULL || source_to_srgb == NULL) {
        return;
    }

    for (index = 0u; index < pixel_count; ++index) {
        offset = index * 3u;
        rgb[0] = (double)pixels[offset + 0u];
        rgb[1] = (double)pixels[offset + 1u];
        rgb[2] = (double)pixels[offset + 2u];
        png_apply_linear_matrix_triplet(rgb, source_to_srgb);
        pixels[offset + 0u] = (float)rgb[0];
        pixels[offset + 1u] = (float)rgb[1];
        pixels[offset + 2u] = (float)rgb[2];
    }
}

static void
png_expand_background_sample_to_unit(png_uint_16 sample,
                                     png_uint_32 bitdepth,
                                     double *out)
{
    double max_value;

    if (out == NULL) {
        return;
    }
    if (bitdepth == 16u) {
        *out = (double)sample / 65535.0;
        return;
    }
    if (bitdepth == 0u || bitdepth > 16u) {
        *out = 0.0;
        return;
    }
    max_value = (double)((1u << bitdepth) - 1u);
    if (max_value <= 0.0) {
        *out = 0.0;
        return;
    }
    *out = (double)sample / max_value;
}

static void
png_resolve_background_unit(png_structp png_ptr,
                            png_infop info_ptr,
                            png_uint_32 color_type,
                            png_uint_32 bitdepth,
                            unsigned char const *bgcolor,
                            double bg_unit[3],
                            int *background_from_file)
{
    png_color_16p png_background;
    png_colorp palette;
    int ncolors;
    unsigned int index;
    double gray;

    png_background = NULL;
    palette = NULL;
    ncolors = 0;
    index = 0u;
    gray = 0.0;

    if (bg_unit == NULL || background_from_file == NULL) {
        return;
    }

    *background_from_file = 0;
    bg_unit[0] = 0.0;
    bg_unit[1] = 0.0;
    bg_unit[2] = 0.0;

    if (png_get_bKGD(png_ptr, info_ptr, &png_background) != PNG_INFO_bKGD ||
        png_background == NULL ||
        (bgcolor != NULL && loader_background_policy() ==
         SIXEL_BACKGROUND_POLICY_EXPLICIT_FIRST)) {
        if (bgcolor != NULL) {
            bg_unit[0] = (double)bgcolor[0] / 255.0;
            bg_unit[1] = (double)bgcolor[1] / 255.0;
            bg_unit[2] = (double)bgcolor[2] / 255.0;
        }
        return;
    }

    *background_from_file = 1;
    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        if (png_get_PLTE(png_ptr, info_ptr, &palette, &ncolors) !=
            PNG_INFO_PLTE ||
            palette == NULL || ncolors <= 0) {
            *background_from_file = 0;
            return;
        }
        index = (unsigned int)png_background->index;
        if ((int)index >= ncolors) {
            index = 0u;
        }
        bg_unit[0] = (double)palette[index].red / 255.0;
        bg_unit[1] = (double)palette[index].green / 255.0;
        bg_unit[2] = (double)palette[index].blue / 255.0;
        return;
    }
    if (color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_expand_background_sample_to_unit(png_background->gray,
                                             bitdepth,
                                             &gray);
        bg_unit[0] = gray;
        bg_unit[1] = gray;
        bg_unit[2] = gray;
        return;
    }

    png_expand_background_sample_to_unit(png_background->red,
                                         bitdepth, &bg_unit[0]);
    png_expand_background_sample_to_unit(png_background->green,
                                         bitdepth, &bg_unit[1]);
    png_expand_background_sample_to_unit(png_background->blue,
                                         bitdepth, &bg_unit[2]);
}

static int
png_colorspace_from_pixelformat(int pixelformat)
{
    switch (pixelformat) {
    case SIXEL_PIXELFORMAT_LINEARRGBFLOAT32:
        return SIXEL_COLORSPACE_LINEAR;
    case SIXEL_PIXELFORMAT_OKLABFLOAT32:
        return SIXEL_COLORSPACE_OKLAB;
    case SIXEL_PIXELFORMAT_CIELABFLOAT32:
        return SIXEL_COLORSPACE_CIELAB;
    case SIXEL_PIXELFORMAT_DIN99DFLOAT32:
        return SIXEL_COLORSPACE_DIN99D;
    default:
        return SIXEL_COLORSPACE_GAMMA;
    }
}

/* libpng error handler */
static void
png_error_callback(png_structp png_ptr, png_const_charp error_message)
{
    sixel_helper_set_additional_message(error_message);
    png_longjmp(png_ptr, 1);
}

/*
 * libpng can hide conflicting color declarations in its info structure.
 * Keep their original pre-IDAT presence for the ColorSync-compatible priority
 * shared with builtin PNG. These flags never authorize rejected ICC bytes.
 */
static unsigned int
png_source_color_chunks(unsigned char const *buffer, size_t size)
{
    size_t offset;
    size_t length;
    unsigned int flags;
    unsigned char const *type;

    flags = 0u;
    for (offset = 8u; offset <= size && size - offset >= 12u;
         offset += length + 12u) {
        length = ((size_t)buffer[offset] << 24) |
                 ((size_t)buffer[offset + 1u] << 16) |
                 ((size_t)buffer[offset + 2u] << 8) |
                 (size_t)buffer[offset + 3u];
        if (length > size - offset - 12u) {
            break;
        }
        type = buffer + offset + 4u;
        if (memcmp(type, "IDAT", 4u) == 0 ||
            memcmp(type, "IEND", 4u) == 0) {
            break;
        }
        if (memcmp(type, "sRGB", 4u) == 0 && length == 1u &&
            buffer[offset + 8u] <= 3u) {
            flags |= PNG_INFO_sRGB;
        } else if (memcmp(type, "cHRM", 4u) == 0 && length == 32u) {
            flags |= PNG_INFO_cHRM;
        }
    }
    return flags;
}

/*
 * Convert source metadata to linear sRGB without touching alpha. Preserve the
 * shared ColorSync compatibility rule: iCCP+sRGB+cHRM uses sRGB; iCCP+sRGB
 * without cHRM still tries the validated ICC profile. See the PNG precedence
 * section in docs/loader/color-management.md before changing this decision.
 * An unavailable transform falls back to supported PNG metadata.
 */
static int
png_source_to_linear(png_structp png_ptr, png_infop info_ptr,
                      float *pixels, int width, int height, int enable_cms,
                      unsigned int color_chunks)
{
    png_charp name;
    png_bytep profile;
    png_uint_32 profile_size;
    int compression;
    int converted;
    int has_gamma;
    int has_matrix;
    double gamma;
    double wx;
    double wy;
    double rx;
    double ry;
    double gx;
    double gy;
    double bx;
    double by;
    double matrix[3][3];
    size_t count;
    size_t i;

    count = (size_t)width * (size_t)height;
    converted = 0;
    has_gamma = 0;
    has_matrix = 0;
    gamma = 0.0;
    if (enable_cms &&
        (color_chunks & (PNG_INFO_sRGB | PNG_INFO_cHRM)) !=
            (PNG_INFO_sRGB | PNG_INFO_cHRM) &&
        png_get_iCCP(png_ptr, info_ptr, &name, &compression,
                                  &profile, &profile_size) == PNG_INFO_iCCP) {
        converted = sixel_cms_convert_to_srgb_with_profile_bytes(
            (unsigned char *)pixels, width, height,
            SIXEL_PIXELFORMAT_RGBFLOAT32, profile, profile_size);
        if (!converted) {
            loader_trace_message("libpng: ICC transform unavailable; "
                                 "using supported PNG color metadata");
        }
    }
    if (enable_cms && !converted && !(color_chunks & PNG_INFO_sRGB)) {
        has_gamma = png_get_gAMA(png_ptr, info_ptr, &gamma) == PNG_INFO_gAMA
                    && gamma > 0.0;
        if (png_get_cHRM(png_ptr, info_ptr, &wx, &wy, &rx, &ry,
                        &gx, &gy, &bx, &by) == PNG_INFO_cHRM) {
            has_matrix = png_build_chrm_to_srgb_matrix(
                wx, wy, rx, ry, gx, gy, bx, by, matrix);
        }
    }
    for (i = 0u; i < count * 3u; ++i) {
        pixels[i] = (float)png_decode_source_unit(pixels[i],
            has_gamma ? SIXEL_PNG_TRANSFER_GAMA : SIXEL_PNG_TRANSFER_SRGB,
            gamma);
    }
    if (has_matrix) {
        png_apply_linear_matrix_float32(pixels, count, matrix);
    }
    return converted || has_gamma || has_matrix;
}

static SIXELSTATUS
png_convert_rgb16_rows_to_rgbfloat32(unsigned char      /* out */ **result,
                                     unsigned char const /* in */  *rows16,
                                     png_size_t          /* in */  rowbytes,
                                     int                 /* in */  width,
                                     int                 /* in */  height,
                                     sixel_allocator_t   /* in */  *allocator)
{
    SIXELSTATUS status;
    float *dst;
    size_t pixel_count;
    size_t total_bytes;
    size_t y;
    size_t x;
    unsigned char const *src_row;
    size_t src_index;
    size_t dst_index;
    unsigned int value;

    status = SIXEL_FALSE;
    dst = NULL;
    pixel_count = 0u;
    total_bytes = 0u;
    y = 0u;
    x = 0u;
    src_row = NULL;
    src_index = 0u;
    dst_index = 0u;
    value = 0u;

    if (result == NULL || rows16 == NULL || allocator == NULL ||
        width <= 0 || height <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    if ((size_t)rowbytes < (size_t)width * 6u) {
        sixel_helper_set_additional_message(
            "load_png: invalid 16-bit RGB row stride.");
        return SIXEL_BAD_INPUT;
    }
    if ((size_t)width > SIZE_MAX / (size_t)height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    pixel_count = (size_t)width * (size_t)height;
    if (pixel_count > SIZE_MAX / (3u * sizeof(float))) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    total_bytes = pixel_count * 3u * sizeof(float);
    dst = (float *)sixel_allocator_malloc(allocator, total_bytes);
    if (dst == NULL) {
        sixel_helper_set_additional_message(
            "load_png: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    for (y = 0u; y < (size_t)height; ++y) {
        src_row = rows16 + y * (size_t)rowbytes;
        for (x = 0u; x < (size_t)width; ++x) {
            src_index = x * 6u;
            dst_index = (y * (size_t)width + x) * 3u;

            value = ((unsigned int)src_row[src_index + 0u] << 8u)
                | (unsigned int)src_row[src_index + 1u];
            dst[dst_index + 0u] = (float)value / 65535.0f;

            value = ((unsigned int)src_row[src_index + 2u] << 8u)
                | (unsigned int)src_row[src_index + 3u];
            dst[dst_index + 1u] = (float)value / 65535.0f;

            value = ((unsigned int)src_row[src_index + 4u] << 8u)
                | (unsigned int)src_row[src_index + 5u];
            dst[dst_index + 2u] = (float)value / 65535.0f;
        }
    }

    *result = (unsigned char *)dst;
    status = SIXEL_OK;

    return status;
}

/* Alpha is never a CMS channel. APNG keeps it until canvas composition. */
typedef struct sixel_png_alpha {
    float *values;
    double background[3];
    int has_background;
    int defer_composition;
} sixel_png_alpha_t;

static SIXELSTATUS
load_png(unsigned char      /* out */ **result,
         unsigned char const /* in */ *buffer,
         size_t             /* in */  size,
         int                /* out */ *psx,
         int                /* out */ *psy,
         unsigned char      /* out */ **ppalette,
         int                /* out */ *pncolors,
         int                /* in */  reqcolors,
         int                /* out */ *pixelformat,
         unsigned char      /* out */ *bgcolor,
         int                /* out */ *transparent,
         int                /* out */ *alpha_zero_is_transparent,
         int                /* out */ *cms_applied,
         int                /* in */  enable_cms,
         sixel_png_alpha_t          *alpha_state,
         sixel_allocator_t  /* in */  *allocator)
{
    SIXELSTATUS status;
    sixel_png_read_chunk_t read_chunk;
    png_structp png_ptr;
    png_infop volatile info_ptr;
    unsigned char **volatile rows;
    unsigned char *volatile raw16_pixels;
    float *volatile cms_pixels;
    png_color *png_palette;
    png_uint_32 width;
    png_uint_32 height;
    png_uint_32 bitdepth;
    png_uint_32 color_type;
    png_uint_32 png_status;
    png_uint_32 read_bitdepth;
    png_uint_32 read_channels;
    png_size_t rowbytes;
    size_t raw16_size;
    int promote_to_float32;
    int i;
    int depth;
    int cms_converted;
    int has_tRNS_chunk;
    int has_alpha_chunk;
    int has_transparency;
    int indexed_trns_palette_path;
    int trns_keycolor_mode;
    int use_trns_keycolor;
    int background_colorspace;
    int background_from_file;
    double bg_unit[3];
    int palette_force_pal8;
    int palette_keycolor_mode;
    int palette_keycolor_index;
    int palette_zero_alpha_count;
    int palette_remap_zero_alpha_indexes;
    unsigned char palette_zero_alpha_map[SIXEL_PALETTE_MAX];
    size_t pixel_count;
    size_t pixel_index;
    size_t y;
    size_t src_index;
    size_t dst_index;
    unsigned int palette_index;
    unsigned int sample;
    unsigned int color_chunks;
    size_t sample_bytes;
    size_t color_count;
    int palette_colors;
    float *float_pixels;
    float bg_pixel[3];
    int channel;
    double value;

    status = SIXEL_FALSE;
    *result = NULL;
    *transparent = -1;
    *alpha_zero_is_transparent = 0;
    *cms_applied = 0;
    png_ptr = NULL;
    info_ptr = NULL;
    rows = NULL;
    raw16_pixels = NULL;
    cms_pixels = NULL;
    png_palette = NULL;
    promote_to_float32 = 0;
    palette_remap_zero_alpha_indexes = 0;
    palette_keycolor_index = -1;
    png_ptr = png_create_read_struct(
        PNG_LIBPNG_VER_STRING, NULL, &png_error_callback, NULL);
    if (!png_ptr) {
        sixel_helper_set_additional_message(
            "png_create_read_struct() failed.");
        status = SIXEL_PNG_ERROR;
        goto cleanup;
    }

    /*
     * The minimum valid PNG is 67 bytes.
     * https://garethrees.org/2007/11/14/pngcrush/
     */
    if (size < 67) {
        sixel_helper_set_additional_message("PNG data too small to be valid!");
        status = SIXEL_PNG_ERROR;
        goto cleanup;
    }

#if HAVE_SETJMP
    if (setjmp(png_jmpbuf(png_ptr)) != 0) {
        sixel_allocator_free(allocator, *result);
        *result = NULL;
        status = SIXEL_PNG_ERROR;
        goto cleanup;
    }
#endif  /* HAVE_SETJMP */

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        sixel_helper_set_additional_message(
            "png_create_info_struct() failed.");
        status = SIXEL_PNG_ERROR;
        png_destroy_read_struct(&png_ptr, (png_infopp)0, (png_infopp)0);
        goto cleanup;
    }
    read_chunk.buffer = buffer;
    read_chunk.size = size;
    read_chunk.offset = 0u;

    png_set_read_fn(png_ptr,(png_voidp)&read_chunk, read_png);
#if defined(PNG_SET_OPTION_SUPPORTED) && defined(PNG_SKIP_sRGB_CHECK_PROFILE)
    png_set_option(png_ptr, PNG_SKIP_sRGB_CHECK_PROFILE, PNG_OPTION_ON);
#endif
    png_read_info(png_ptr, info_ptr);
    color_chunks = png_source_color_chunks(buffer, size);
    width = png_get_image_width(png_ptr, info_ptr);
    height = png_get_image_height(png_ptr, info_ptr);

    if (width == 0u || height == 0u ||
        width > INT_MAX || height > INT_MAX ||
        (size_t)width > SIZE_MAX / (size_t)height / (4u * sizeof(float))) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto cleanup;
    }

    *psx = (int)width;
    *psy = (int)height;

    color_type = png_get_color_type(png_ptr, info_ptr);
    bitdepth = png_get_bit_depth(png_ptr, info_ptr);
    if (bitdepth == 16) {
#  if HAVE_DEBUG
        LIBPNG_DEBUG_LOG("bitdepth: %u\n", (unsigned int)bitdepth);
        LIBPNG_DEBUG_LOG("preserving 16bit for float32 conversion...\n");
#  endif
        promote_to_float32 = 1;
    }

    has_tRNS_chunk = png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS) != 0;
    has_alpha_chunk = (color_type & PNG_COLOR_MASK_ALPHA) != 0;
    indexed_trns_palette_path = has_tRNS_chunk &&
                                !has_alpha_chunk &&
                                color_type == PNG_COLOR_TYPE_PALETTE;
    trns_keycolor_mode = loader_png_trns_keycolor_mode();
    background_colorspace = loader_background_colorspace();
    png_resolve_background_unit(png_ptr,
                                info_ptr,
                                color_type,
                                bitdepth,
                                bgcolor,
                                bg_unit,
                                &background_from_file);
    alpha_state->has_background = background_from_file || bgcolor != NULL;
    has_transparency = has_tRNS_chunk || has_alpha_chunk ||
                       alpha_state->defer_composition;
    if (indexed_trns_palette_path && !enable_cms &&
        !alpha_state->has_background && !alpha_state->defer_composition &&
        png_get_PLTE(png_ptr, info_ptr, &png_palette, pncolors) &&
        *pncolors <= reqcolors) {
        has_transparency = 0;
    }
    use_trns_keycolor = trns_keycolor_mode != 0 && bitdepth <= 8u &&
                       !enable_cms && !alpha_state->has_background &&
                       !alpha_state->defer_composition && has_transparency &&
                       !indexed_trns_palette_path;
    palette_keycolor_mode = indexed_trns_palette_path && !has_transparency;

    png_set_interlace_handling(png_ptr);
    if (use_trns_keycolor) {
        if (color_type == PNG_COLOR_TYPE_GRAY && bitdepth < 8u) {
#if HAVE_DECL_PNG_SET_EXPAND_GRAY_1_2_4_TO_8
            png_set_expand_gray_1_2_4_to_8(png_ptr);
#elif HAVE_DECL_PNG_SET_GRAY_1_2_4_TO_8
            png_set_gray_1_2_4_to_8(png_ptr);
#endif
        }
        if (color_type == PNG_COLOR_TYPE_GRAY ||
            color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
            png_set_gray_to_rgb(png_ptr);
        }
        if (has_tRNS_chunk) {
            png_set_tRNS_to_alpha(png_ptr);
        }

        png_read_update_info(png_ptr, info_ptr);
        read_bitdepth = png_get_bit_depth(png_ptr, info_ptr);
        read_channels = png_get_channels(png_ptr, info_ptr);
        rowbytes = png_get_rowbytes(png_ptr, info_ptr);
        if (read_bitdepth != 8u || read_channels != 4u) {
            sixel_helper_set_additional_message(
                "load_png: unsupported tRNS keycolor layout.");
            status = SIXEL_BAD_INPUT;
            goto cleanup;
        }
        if ((size_t)*psy > 0u && (size_t)rowbytes > SIZE_MAX / (size_t)*psy) {
            status = SIXEL_BAD_INTEGER_OVERFLOW;
            goto cleanup;
        }
        if ((size_t)rowbytes != (size_t)*psx * 4u) {
            sixel_helper_set_additional_message(
                "load_png: unexpected RGBA row stride.");
            status = SIXEL_BAD_INPUT;
            goto cleanup;
        }

        *result = (unsigned char *)sixel_allocator_malloc(
            allocator,
            (size_t)*psy * (size_t)rowbytes);
        if (*result == NULL) {
            sixel_helper_set_additional_message(
                "load_png: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto cleanup;
        }
        rows = (unsigned char **)sixel_allocator_malloc(
            allocator,
            (size_t)*psy * sizeof(unsigned char *));
        if (rows == NULL) {
            sixel_helper_set_additional_message(
                "load_png: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto cleanup;
        }
        for (i = 0; i < *psy; ++i) {
            rows[i] = *result + (size_t)i * (size_t)rowbytes;
        }
        png_read_image(png_ptr, rows);
        png_read_end(png_ptr, info_ptr);

        *pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
        if (alpha_zero_is_transparent != NULL) {
            *alpha_zero_is_transparent = 1;
        }
        status = SIXEL_OK;
        goto cleanup;
    }

    if (has_transparency) {
        /* Read RGBA at source depth, including packed samples and Adam7. */
        if (color_type == PNG_COLOR_TYPE_PALETTE) {
            png_set_palette_to_rgb(png_ptr);
        }
        if (color_type == PNG_COLOR_TYPE_GRAY && bitdepth < 8u) {
#if HAVE_DECL_PNG_SET_EXPAND_GRAY_1_2_4_TO_8
            png_set_expand_gray_1_2_4_to_8(png_ptr);
#elif HAVE_DECL_PNG_SET_GRAY_1_2_4_TO_8
            png_set_gray_1_2_4_to_8(png_ptr);
#endif
        }
        if (color_type == PNG_COLOR_TYPE_GRAY ||
            color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
            png_set_gray_to_rgb(png_ptr);
        }
        if (has_tRNS_chunk) {
            png_set_tRNS_to_alpha(png_ptr);
        }
        if (!has_alpha_chunk && !has_tRNS_chunk) {
            png_set_add_alpha(png_ptr, bitdepth == 16u ? 65535u : 255u,
                              PNG_FILLER_AFTER);
        }
        png_read_update_info(png_ptr, info_ptr);
        read_bitdepth = png_get_bit_depth(png_ptr, info_ptr);
        rowbytes = png_get_rowbytes(png_ptr, info_ptr);
        pixel_count = (size_t)width * (size_t)height;
        sample_bytes = read_bitdepth == 16u ? 2u : 1u;
        if (png_get_channels(png_ptr, info_ptr) != 4 ||
            rowbytes != (size_t)width * 4u * sample_bytes ||
            pixel_count > SIZE_MAX / (4u * sizeof(float))) {
            status = SIXEL_BAD_INPUT;
            goto cleanup;
        }
        raw16_pixels = (unsigned char *)sixel_allocator_malloc(
            allocator, (size_t)rowbytes * height);
        rows = (unsigned char **)sixel_allocator_malloc(
            allocator, (size_t)height * sizeof(*rows));
        *result = (unsigned char *)sixel_allocator_malloc(
            allocator, pixel_count * 3u * sizeof(float));
        alpha_state->values = (float *)sixel_allocator_malloc(
            allocator, pixel_count * sizeof(float));
        if (raw16_pixels == NULL || rows == NULL || *result == NULL ||
            alpha_state->values == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto cleanup;
        }
        for (y = 0u; y < height; ++y) {
            rows[y] = raw16_pixels + y * rowbytes;
        }
        png_read_image(png_ptr, rows);
        png_read_end(png_ptr, info_ptr);
        float_pixels = (float *)*result;
        for (pixel_index = 0u; pixel_index < pixel_count; ++pixel_index) {
            for (channel = 0; channel < 4; ++channel) {
                src_index = (pixel_index * 4u + (size_t)channel) * sample_bytes;
                sample = raw16_pixels[src_index];
                if (sample_bytes == 2u) {
                    sample = (sample << 8) | raw16_pixels[src_index + 1u];
                }
                value = sample / (sample_bytes == 2u ? 65535.0 : 255.0);
                if (channel == 3) {
                    alpha_state->values[pixel_index] = (float)value;
                } else {
                    float_pixels[pixel_index * 3u + (size_t)channel] =
                        (float)value;
                }
            }
        }
        cms_converted = png_source_to_linear(png_ptr, info_ptr, float_pixels,
                                             *psx, *psy, enable_cms,
                                             color_chunks);
        for (channel = 0; channel < 3; ++channel) {
            bg_pixel[channel] = (float)bg_unit[channel];
        }
        if (background_from_file) {
            (void)png_source_to_linear(png_ptr, info_ptr, bg_pixel,
                                       1, 1, enable_cms, color_chunks);
        } else if (background_colorspace != SIXEL_COLORSPACE_LINEAR) {
            for (channel = 0; channel < 3; ++channel) {
                bg_pixel[channel] = (float)png_decode_srgb_unit(
                    bg_pixel[channel]);
            }
        }
        for (channel = 0; channel < 3; ++channel) {
            alpha_state->background[channel] = bg_pixel[channel];
        }
        for (pixel_index = 0u; pixel_index < pixel_count; ++pixel_index) {
            value = alpha_state->values[pixel_index];
            if (alpha_state->defer_composition ||
                !alpha_state->has_background ||
                (value == 0.0 && loader_transparent_policy() !=
                 SIXEL_ALPHA_POLICY_COMPOSITE)) {
                continue;
            }
            for (channel = 0; channel < 3; ++channel) {
                dst_index = pixel_index * 3u + (size_t)channel;
                float_pixels[dst_index] = (float)(
                    float_pixels[dst_index] * value +
                    bg_pixel[channel] * (1.0 - value));
            }
        }
        *pixelformat = SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
        *cms_applied = cms_converted;
        status = SIXEL_OK;
        goto cleanup;
    }

    switch (color_type) {
    case PNG_COLOR_TYPE_PALETTE:
#  if HAVE_DEBUG
        LIBPNG_DEBUG_LOG("paletted PNG(PNG_COLOR_TYPE_PALETTE)\n");
#  endif
        png_status = png_get_PLTE(png_ptr, info_ptr,
                                  &png_palette, pncolors);
        if (png_status != PNG_INFO_PLTE || png_palette == NULL) {
            sixel_helper_set_additional_message(
                "PLTE chunk not found");
            status = SIXEL_PNG_ERROR;
            goto cleanup;
        }
#  if HAVE_DEBUG
        LIBPNG_DEBUG_LOG("palette colors: %d\n", *pncolors);
        LIBPNG_DEBUG_LOG("bitdepth: %u\n", (unsigned int)bitdepth);
#  endif
        if (ppalette == NULL || *pncolors > reqcolors) {
#  if HAVE_DEBUG
            LIBPNG_DEBUG_LOG("detected more colors than required(>%d).\n",
                             reqcolors);
            LIBPNG_DEBUG_LOG("expand to RGB format...\n");
#  endif
            png_set_palette_to_rgb(png_ptr);
            png_set_strip_alpha(png_ptr);
            *pixelformat = SIXEL_PIXELFORMAT_RGB888;
        } else {
            *ppalette = (unsigned char *)
                sixel_allocator_malloc(allocator,
                                       (size_t)*pncolors * 3);
            if (*ppalette == NULL) {
                sixel_helper_set_additional_message(
                    "load_png: sixel_allocator_malloc() failed.");
                status = SIXEL_BAD_ALLOCATION;
                goto cleanup;
            }

            palette_force_pal8 = 0;
            palette_keycolor_index = -1;
            palette_zero_alpha_count = 0;
            palette_remap_zero_alpha_indexes = 0;
            memset(palette_zero_alpha_map, 0, sizeof(palette_zero_alpha_map));

            read_palette(png_ptr, info_ptr, *ppalette,
                         *pncolors,
                         png_palette,
                         palette_keycolor_mode ? transparent : NULL,
                         palette_keycolor_mode ? palette_zero_alpha_map : NULL,
                         palette_keycolor_mode
                         ? &palette_zero_alpha_count : NULL);

            if (palette_keycolor_mode && palette_zero_alpha_count > 0) {
                palette_force_pal8 = 1;
                if (transparent != NULL) {
                    palette_keycolor_index = *transparent;
                }
                if (palette_keycolor_index >= 0 &&
                    palette_zero_alpha_count > 1) {
                    palette_remap_zero_alpha_indexes = 1;
                }
            }

            if (palette_force_pal8 && bitdepth < 8u) {
                png_set_packing(png_ptr);
            }

            if (palette_force_pal8) {
                *pixelformat = SIXEL_PIXELFORMAT_PAL8;
            } else {
                switch (bitdepth) {
                case 1:
                    *pixelformat = SIXEL_PIXELFORMAT_PAL1;
                    break;
                case 2:
                    *pixelformat = SIXEL_PIXELFORMAT_PAL2;
                    break;
                case 4:
                    *pixelformat = SIXEL_PIXELFORMAT_PAL4;
                    break;
                case 8:
                    *pixelformat = SIXEL_PIXELFORMAT_PAL8;
                    break;
                default:
                    png_set_palette_to_rgb(png_ptr);
                    *pixelformat = SIXEL_PIXELFORMAT_RGB888;
                    break;
                }
            }
        }
        break;
    case PNG_COLOR_TYPE_GRAY:
#  if HAVE_DEBUG
        LIBPNG_DEBUG_LOG("grayscale PNG(PNG_COLOR_TYPE_GRAY)\n");
        LIBPNG_DEBUG_LOG("bitdepth: %u\n", (unsigned int)bitdepth);
#  endif
        if (1 << bitdepth > reqcolors) {
#  if HAVE_DEBUG
            LIBPNG_DEBUG_LOG("detected more colors than required(>%d).\n",
                             reqcolors);
            LIBPNG_DEBUG_LOG("expand into RGB format...\n");
#  endif
            png_set_gray_to_rgb(png_ptr);
            *pixelformat = SIXEL_PIXELFORMAT_RGB888;
        } else {
            switch (bitdepth) {
            case 1:
            case 2:
            case 4:
                if (ppalette) {
#  if HAVE_DECL_PNG_SET_EXPAND_GRAY_1_2_4_TO_8
#   if HAVE_DEBUG
                    LIBPNG_DEBUG_LOG("expand %u bpp to 8bpp format...\n",
                                     (unsigned int)bitdepth);
#   endif
                    png_set_expand_gray_1_2_4_to_8(png_ptr);
                    *pixelformat = SIXEL_PIXELFORMAT_G8;
#  elif HAVE_DECL_PNG_SET_GRAY_1_2_4_TO_8
#   if HAVE_DEBUG
                    LIBPNG_DEBUG_LOG("expand %u bpp to 8bpp format...\n",
                                     (unsigned int)bitdepth);
#   endif
                    png_set_gray_1_2_4_to_8(png_ptr);
                    *pixelformat = SIXEL_PIXELFORMAT_G8;
#  else
#   if HAVE_DEBUG
                    LIBPNG_DEBUG_LOG("expand into RGB format...\n");
#   endif
                    png_set_gray_to_rgb(png_ptr);
                    *pixelformat = SIXEL_PIXELFORMAT_RGB888;
#  endif
                } else {
                    png_set_gray_to_rgb(png_ptr);
                    *pixelformat = SIXEL_PIXELFORMAT_RGB888;
                }
                break;
            case 8:
                if (ppalette) {
                    *pixelformat = SIXEL_PIXELFORMAT_G8;
                } else {
#  if HAVE_DEBUG
                    LIBPNG_DEBUG_LOG("expand into RGB format...\n");
#  endif
                    png_set_gray_to_rgb(png_ptr);
                    *pixelformat = SIXEL_PIXELFORMAT_RGB888;
                }
                break;
            default:
#  if HAVE_DEBUG
                LIBPNG_DEBUG_LOG("expand into RGB format...\n");
#  endif
                png_set_gray_to_rgb(png_ptr);
                *pixelformat = SIXEL_PIXELFORMAT_RGB888;
                break;
            }
        }
        break;
    case PNG_COLOR_TYPE_GRAY_ALPHA:
#  if HAVE_DEBUG
        LIBPNG_DEBUG_LOG("grayscale-alpha PNG(PNG_COLOR_TYPE_GRAY_ALPHA)\n");
        LIBPNG_DEBUG_LOG("bitdepth: %u\n", (unsigned int)bitdepth);
        LIBPNG_DEBUG_LOG("expand to RGB format...\n");
#  endif
        png_set_gray_to_rgb(png_ptr);
        *pixelformat = SIXEL_PIXELFORMAT_RGB888;
        break;
    case PNG_COLOR_TYPE_RGB_ALPHA:
#  if HAVE_DEBUG
        LIBPNG_DEBUG_LOG("RGBA PNG(PNG_COLOR_TYPE_RGB_ALPHA)\n");
        LIBPNG_DEBUG_LOG("bitdepth: %u\n", (unsigned int)bitdepth);
        LIBPNG_DEBUG_LOG("expand to RGB format...\n");
#  endif
        *pixelformat = SIXEL_PIXELFORMAT_RGB888;
        break;
    case PNG_COLOR_TYPE_RGB:
#  if HAVE_DEBUG
        LIBPNG_DEBUG_LOG("RGB PNG(PNG_COLOR_TYPE_RGB)\n");
        LIBPNG_DEBUG_LOG("bitdepth: %u\n", (unsigned int)bitdepth);
#  endif
        *pixelformat = SIXEL_PIXELFORMAT_RGB888;
        break;
    default:
        /* unknown format */
        goto cleanup;
    }
    if (promote_to_float32 && *pixelformat == SIXEL_PIXELFORMAT_RGB888) {
        png_read_update_info(png_ptr, info_ptr);
        read_bitdepth = png_get_bit_depth(png_ptr, info_ptr);
        read_channels = png_get_channels(png_ptr, info_ptr);
        rowbytes = png_get_rowbytes(png_ptr, info_ptr);

        if (read_bitdepth != 16u || read_channels != 3u) {
            sixel_helper_set_additional_message(
                "load_png: unsupported 16-bit PNG channel layout.");
            status = SIXEL_BAD_INPUT;
            goto cleanup;
        }
        if ((size_t)*psy > 0u && (size_t)rowbytes > SIZE_MAX / (size_t)*psy) {
            status = SIXEL_BAD_INTEGER_OVERFLOW;
            goto cleanup;
        }
        raw16_size = (size_t)rowbytes * (size_t)*psy;
        raw16_pixels = (unsigned char *)sixel_allocator_malloc(allocator,
                                                               raw16_size);
        if (raw16_pixels == NULL) {
            sixel_helper_set_additional_message(
                "load_png: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto cleanup;
        }

        rows = (unsigned char **)sixel_allocator_malloc(
            allocator,
            (size_t)*psy * sizeof(unsigned char *));
        if (rows == NULL) {
            sixel_helper_set_additional_message(
                "load_png: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto cleanup;
        }
        for (i = 0; i < *psy; ++i) {
            rows[i] = raw16_pixels + (size_t)i * (size_t)rowbytes;
        }

        png_read_image(png_ptr, rows);
        png_read_end(png_ptr, info_ptr);

        status = png_convert_rgb16_rows_to_rgbfloat32(result,
                                                      raw16_pixels,
                                                      rowbytes,
                                                      *psx,
                                                      *psy,
                                                      allocator);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
        *pixelformat = SIXEL_PIXELFORMAT_RGBFLOAT32;
    } else {
        depth = sixel_helper_compute_depth(*pixelformat);
        *result = (unsigned char *)
            sixel_allocator_malloc(allocator,
                                   (size_t)*psx * (size_t)*psy * (size_t)depth);
        if (*result == NULL) {
            sixel_helper_set_additional_message(
                "load_png: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto cleanup;
        }
        rows = (unsigned char **)sixel_allocator_malloc(
            allocator,
            (size_t)*psy * sizeof(unsigned char *));
        if (rows == NULL) {
            sixel_helper_set_additional_message(
                "load_png: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto cleanup;
        }
        switch (*pixelformat) {
        case SIXEL_PIXELFORMAT_PAL1:
        case SIXEL_PIXELFORMAT_PAL2:
        case SIXEL_PIXELFORMAT_PAL4:
            for (i = 0; i < *psy; ++i) {
                rows[i] = *result + (size_t)i *
                    (((size_t)depth * (size_t)*psx * bitdepth + 7u) / 8u);
            }
            break;
        default:
            for (i = 0; i < *psy; ++i) {
                rows[i] = *result + (size_t)depth * (size_t)*psx * (size_t)i;
            }
            break;
        }

        png_read_image(png_ptr, rows);
        png_read_end(png_ptr, info_ptr);

        if (palette_remap_zero_alpha_indexes &&
            *pixelformat == SIXEL_PIXELFORMAT_PAL8 &&
            palette_keycolor_index >= 0 &&
            palette_keycolor_index < SIXEL_PALETTE_MAX &&
            *result != NULL &&
            *psx > 0 &&
            *psy > 0 &&
            (size_t)*psx <= SIZE_MAX / (size_t)*psy) {
            pixel_count = (size_t)*psx * (size_t)*psy;
            for (pixel_index = 0u; pixel_index < pixel_count; ++pixel_index) {
                palette_index = (*result)[pixel_index];
                if ((int)palette_index != palette_keycolor_index &&
                    palette_index < SIXEL_PALETTE_MAX &&
                    palette_zero_alpha_map[palette_index] != 0u) {
                    (*result)[pixel_index] =
                        (unsigned char)palette_keycolor_index;
                }
            }
        }
    }

    if (enable_cms) {
        color_count = (size_t)*psx * (size_t)*psy;
        palette_colors = (*pixelformat & SIXEL_FORMATTYPE_PALETTE) != 0;
        if (palette_colors) {
            color_count = (size_t)*pncolors;
        }
        if (color_count > SIZE_MAX / (3u * sizeof(float))) {
            status = SIXEL_BAD_INTEGER_OVERFLOW;
            goto cleanup;
        }
        cms_pixels = (float *)sixel_allocator_malloc(
            allocator, color_count * 3u * sizeof(float));
        if (cms_pixels == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto cleanup;
        }
        for (pixel_index = 0u; pixel_index < color_count; ++pixel_index) {
            for (channel = 0; channel < 3; ++channel) {
                dst_index = pixel_index * 3u + (size_t)channel;
                if (*pixelformat == SIXEL_PIXELFORMAT_RGBFLOAT32) {
                    value = ((float *)*result)[dst_index];
                } else if (palette_colors) {
                    value = (*ppalette)[dst_index] / 255.0;
                } else if (*pixelformat == SIXEL_PIXELFORMAT_G8) {
                    value = (*result)[pixel_index] / 255.0;
                } else {
                    value = (*result)[dst_index] / 255.0;
                }
                cms_pixels[dst_index] = (float)value;
            }
        }
        cms_converted = png_source_to_linear(png_ptr, info_ptr, cms_pixels,
            palette_colors ? *pncolors : *psx, palette_colors ? 1 : *psy, 1,
            color_chunks);
        if (*pixelformat == SIXEL_PIXELFORMAT_G8 && cms_converted) {
            sixel_allocator_free(allocator, *result);
            *result = (unsigned char *)sixel_allocator_malloc(
                allocator, color_count * 3u);
            if (*result == NULL) {
                status = SIXEL_BAD_ALLOCATION;
                goto cleanup;
            }
            *pixelformat = SIXEL_PIXELFORMAT_RGB888;
        }
        for (dst_index = 0u; dst_index < color_count * 3u; ++dst_index) {
            value = png_encode_srgb_unit(cms_pixels[dst_index]);
            if (*pixelformat == SIXEL_PIXELFORMAT_RGBFLOAT32) {
                ((float *)*result)[dst_index] = (float)value;
            } else if (palette_colors) {
                (*ppalette)[dst_index] = (unsigned char)(value * 255.0 + 0.5);
            } else if (*pixelformat != SIXEL_PIXELFORMAT_G8) {
                (*result)[dst_index] = (unsigned char)(value * 255.0 + 0.5);
            }
        }
        *cms_applied = cms_converted;
    }

    status = SIXEL_OK;

cleanup:
    if (png_ptr != NULL) {
        png_destroy_read_struct(&png_ptr, (png_infopp)&info_ptr, (png_infopp)0);
    }

    if (rows != NULL) {
        sixel_allocator_free(allocator, rows);
    }
    if (raw16_pixels != NULL) {
        sixel_allocator_free(allocator, raw16_pixels);
    }
    sixel_allocator_free(allocator, cms_pixels);
    if (SIXEL_FAILED(status)) {
        sixel_allocator_free(allocator, *result);
        *result = NULL;
    }

    return status;
}

typedef struct sixel_apng_frame_control {
    png_uint_32 width;
    png_uint_32 height;
    png_uint_32 x_offset;
    png_uint_32 y_offset;
    unsigned int delay_cs;
    unsigned int dispose_op;
    unsigned int blend_op;
} sixel_apng_frame_control_t;

#define APNG_FRAME_CACHE_MAX_BYTES_DEFAULT \
    ((size_t)(64u * 1024u * 1024u))

typedef struct sixel_apng_canvas {
    float *pixels;
    float *backup;
    int width;
    int height;
} sixel_apng_canvas_t;

typedef struct sixel_apng_replay_cache {
    sixel_frame_t **frames;
    size_t frame_count;
    size_t frame_capacity;
    size_t cached_bytes;
    int enabled;
} sixel_apng_replay_cache_t;

typedef struct sixel_apng_state {
    unsigned char const *ihdr;
    size_t ihdr_size;
    unsigned char *shared_chunks;
    size_t shared_chunks_size;
    size_t shared_chunks_capacity;
    unsigned char *chunk_base;
    size_t chunk_size;
    size_t chunk_capacity;
    png_uint_32 expected_sequence;
} sixel_apng_state_t;

static png_uint_32
read_be32(unsigned char const *p)
{
    png_uint_32 value;

    value = ((png_uint_32)p[0] << 24)
          | ((png_uint_32)p[1] << 16)
          | ((png_uint_32)p[2] << 8)
          |  (png_uint_32)p[3];
    return value;
}

/*
 * Extract EXIF orientation from PNG/APNG eXIf chunk.
 *
 * PNG stores eXIf as a standalone chunk payload, so the TIFF/EXIF parser can
 * consume it directly.
 */
static int
libpng_parse_exif_orientation(unsigned char const *buffer,
                              size_t size,
                              int *orientation)
{
    static unsigned char const png_signature[8] = {
        0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au
    };
    size_t offset;
    png_uint_32 chunk_length;
    size_t chunk_total;
    unsigned char const *chunk_type;

    offset = 0u;
    chunk_length = 0u;
    chunk_total = 0u;
    chunk_type = NULL;
    if (buffer == NULL || orientation == NULL || size < 8u) {
        return 0;
    }
    if (memcmp(buffer, png_signature, sizeof(png_signature)) != 0) {
        return 0;
    }

    offset = sizeof(png_signature);
    while (offset + 12u <= size) {
        chunk_length = read_be32(buffer + offset);
        chunk_total = 12u + (size_t)chunk_length;
        if (chunk_total > size - offset) {
            return 0;
        }

        chunk_type = buffer + offset + 4u;
        if (memcmp(chunk_type, "eXIf", 4u) == 0 &&
            loader_exif_parse_orientation(buffer + offset + 8u,
                                          (size_t)chunk_length,
                                          orientation)) {
            return 1;
        }
        if (memcmp(chunk_type, "IEND", 4u) == 0) {
            break;
        }
        offset += chunk_total;
    }

    return 0;
}

static void
write_be32(unsigned char *p, png_uint_32 value)
{
    p[0] = (unsigned char)(value >> 24);
    p[1] = (unsigned char)(value >> 16);
    p[2] = (unsigned char)(value >> 8);
    p[3] = (unsigned char)value;
}

static png_uint_32
crc32_update(unsigned char const *data, size_t length, png_uint_32 seed)
{
    png_uint_32 crc;
    size_t i;
    int bit;

    crc = ~seed;
    for (i = 0; i < length; ++i) {
        crc ^= data[i];
        for (bit = 0; bit < 8; ++bit) {
            if ((crc & 1U) != 0U) {
                crc = (crc >> 1) ^ 0xedb88320U;
            } else {
                crc >>= 1;
            }
        }
    }
    return ~crc;
}

static SIXELSTATUS
libpng_parse_animation_start_frame_no(int *start_frame_no)
{
    SIXELSTATUS status;
    sixel_suboption_value_t value;
    char diagnostic[128];
    int env_result;

    status = SIXEL_OK;
    memset(&value, 0, sizeof(value));
    diagnostic[0] = '\0';
    env_result = SIXEL_OPTION_ENVIRONMENT_UNSET;

    *start_frame_no = INT_MIN;
    env_result = sixel_option_resolve_scalar_environment(
        SIXEL_OPTION_SCHEMA_START_FRAME,
        &value,
        diagnostic,
        sizeof(diagnostic));
    if (env_result == SIXEL_OPTION_ENVIRONMENT_UNSET) {
        goto end;
    }
    if (env_result == SIXEL_OPTION_ENVIRONMENT_INVALID) {
        sixel_helper_set_additional_message(diagnostic);
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    *start_frame_no = value.int_value;

end:
    return status;
}

static SIXELSTATUS
libpng_resolve_animation_start_frame_no(int start_frame_no,
                                        int frame_count,
                                        int *resolved)
{
    SIXELSTATUS status;
    int index;

    status = SIXEL_OK;
    index = 0;

    if (frame_count <= 0) {
        sixel_helper_set_additional_message(
            "Animation frame count must be positive.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    if (start_frame_no >= 0) {
        index = start_frame_no;
    } else {
        index = frame_count + start_frame_no;
    }

    if (index < 0 || index >= frame_count) {
        sixel_helper_set_additional_message(
            "SIXEL_LOADER_ANIMATION_START_FRAME_NO is outside"
            " the animation frame range.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    *resolved = index;

end:
    return status;
}

static int
ensure_shared_capacity(
    sixel_apng_state_t       *state,
    size_t                    append_size,
    sixel_allocator_t        *allocator)
{
    unsigned char *next;
    size_t needed;
    size_t next_capacity;

    if (append_size > SIZE_MAX - state->shared_chunks_size) {
        return 0;
    }
    needed = state->shared_chunks_size + append_size;
    if (needed <= state->shared_chunks_capacity) {
        return 1;
    }

    next_capacity = state->shared_chunks_capacity;
    if (next_capacity == 0) {
        next_capacity = 1024;
    }
    while (next_capacity < needed) {
        if (next_capacity > SIZE_MAX / 2) {
            return 0;
        }
        next_capacity *= 2;
    }
    next = (unsigned char *)sixel_allocator_malloc(allocator, next_capacity);
    if (next == NULL) {
        return 0;
    }
    if (state->shared_chunks_size > 0 && state->shared_chunks != NULL) {
        memcpy(next, state->shared_chunks, state->shared_chunks_size);
    }
    sixel_allocator_free(allocator, state->shared_chunks);
    state->shared_chunks = next;
    state->shared_chunks_capacity = next_capacity;
    return 1;
}

static int
append_shared_chunk(
    sixel_apng_state_t       *state,
    unsigned char const      *chunk,
    size_t                    chunk_size,
    sixel_allocator_t        *allocator)
{
    if (!ensure_shared_capacity(state, chunk_size, allocator)) {
        return 0;
    }
    memcpy(state->shared_chunks + state->shared_chunks_size,
           chunk,
           chunk_size);
    state->shared_chunks_size += chunk_size;
    return 1;
}

static int
ensure_chunk_capacity(
    sixel_apng_state_t       *state,
    size_t                    append_size,
    sixel_allocator_t        *allocator)
{
    unsigned char *next;
    size_t needed;
    size_t next_capacity;

    if (append_size > SIZE_MAX - state->chunk_size) {
        return 0;
    }
    needed = state->chunk_size + append_size;
    if (needed <= state->chunk_capacity) {
        return 1;
    }

    next_capacity = state->chunk_capacity;
    if (next_capacity == 0) {
        next_capacity = 4096;
    }
    while (next_capacity < needed) {
        if (next_capacity > SIZE_MAX / 2) {
            return 0;
        }
        next_capacity *= 2;
    }

    next = (unsigned char *)sixel_allocator_malloc(allocator, next_capacity);
    if (next == NULL) {
        return 0;
    }
    if (state->chunk_size > 0 && state->chunk_base != NULL) {
        memcpy(next, state->chunk_base, state->chunk_size);
    }
    sixel_allocator_free(allocator, (void *)state->chunk_base);
    state->chunk_base = next;
    state->chunk_capacity = next_capacity;
    return 1;
}

static int
append_chunk(
    sixel_apng_state_t       *state,
    char const               *type,
    unsigned char const      *data,
    png_uint_32               length,
    sixel_allocator_t        *allocator)
{
    unsigned char *dst;
    png_uint_32 crc;
    size_t chunk_bytes;

    chunk_bytes = (size_t)length + 12;
    if (!ensure_chunk_capacity(state, chunk_bytes, allocator)) {
        return 0;
    }

    dst = (unsigned char *)state->chunk_base + state->chunk_size;
    write_be32(dst, length);
    memcpy(dst + 4, type, 4);
    if (length > 0 && data != NULL) {
        memcpy(dst + 8, data, length);
    }

    crc = crc32_update((unsigned char const *)(dst + 4), 4, 0);
    if (length > 0 && data != NULL) {
        crc = crc32_update(data, length, crc);
    }
    write_be32(dst + 8 + length, (png_uint_32)crc);
    state->chunk_size += chunk_bytes;
    return 1;
}

static int
parse_fctl(
    unsigned char const         *data,
    png_uint_32                  length,
    png_uint_32                 *sequence_no,
    sixel_apng_frame_control_t  *control)
{
    png_uint_32 delay_num;
    png_uint_32 delay_den;
    png_uint_32 raw_delay_den;

    if (length != 26 || sequence_no == NULL || control == NULL) {
        sixel_helper_set_additional_message(
            "APNG parse error: invalid fcTL chunk length");
        return 0;
    }

    *sequence_no = read_be32(data + 0);
    control->width = read_be32(data + 4);
    control->height = read_be32(data + 8);
    control->x_offset = read_be32(data + 12);
    control->y_offset = read_be32(data + 16);
    delay_num = (png_uint_32)(((unsigned int)data[20] << 8) | data[21]);
    delay_den = (png_uint_32)(((unsigned int)data[22] << 8) | data[23]);
    raw_delay_den = delay_den;
    control->dispose_op = data[24];
    control->blend_op = data[25];

    if (control->dispose_op > 2 || control->blend_op > 1) {
        sixel_helper_set_additional_message(
            "APNG parse error: invalid fcTL dispose/blend value");
        return 0;
    }

    if (delay_den == 0) {
        apng_decode_trace_message(
            "fcTL seq=%u delay_den=0 detected, fallback=100 delay_num=%u",
            (unsigned int)*sequence_no,
            (unsigned int)delay_num);
        delay_den = 100;
    }
    /*
     * sixel_frame_set_delay() expects centiseconds like the GIF loader.
     * APNG stores delay as delay_num / delay_den seconds.
     */
    control->delay_cs = (unsigned int)((delay_num * 100U) / delay_den);
    if (control->delay_cs == 0 && delay_num > 0) {
        control->delay_cs = 1;
    }

    apng_decode_trace_message(
        "fcTL seq=%u rect=%ux%u+%u+%u delay_num=%u delay_den=%u "
        "delay_cs=%u dispose=%u blend=%u",
        (unsigned int)*sequence_no,
        (unsigned int)control->width,
        (unsigned int)control->height,
        (unsigned int)control->x_offset,
        (unsigned int)control->y_offset,
        (unsigned int)delay_num,
        (unsigned int)raw_delay_den,
        control->delay_cs,
        (unsigned int)control->dispose_op,
        (unsigned int)control->blend_op);

    return 1;
}

/* Both static and animated rectangles use the same source interpretation. */
static SIXELSTATUS
decode_png_rgba(unsigned char **result, int *width, int *height,
                unsigned char *buffer, size_t size, unsigned char *bgcolor,
                int enable_cms, sixel_png_alpha_t *alpha_state,
                sixel_allocator_t *allocator)
{
    int format;
    int ncolors;
    int transparent;
    int alpha_zero;
    int cms_applied;

    format = 0;
    ncolors = 0;
    alpha_state->defer_composition = 1;
    return load_png(result, buffer, size, width, height, NULL, &ncolors, 0,
                    &format, bgcolor, &transparent, &alpha_zero, &cms_applied,
                    enable_cms, alpha_state, allocator);
}

static void
apng_clear_rect(sixel_apng_canvas_t const *canvas,
                sixel_apng_frame_control_t *control)
{
    size_t y;
    size_t offset;

    for (y = 0u; y < control->height; ++y) {
        offset = ((y + control->y_offset) * (size_t)canvas->width +
                  control->x_offset) * 4u;
        memset(canvas->pixels + offset, 0,
               (size_t)control->width * 4u * sizeof(float));
    }
}

/* Straight alpha, linear sRGB; retain fractional coverage between frames. */
static void
apng_blend_rect(sixel_apng_canvas_t const *canvas,
                sixel_apng_frame_control_t *control, float const *rgb,
                float const *alpha)
{
    size_t x;
    size_t y;
    size_t index;
    float *destination;
    double sa;
    double da;
    double oa;
    int channel;

    for (y = 0u; y < control->height; ++y) {
        for (x = 0u; x < control->width; ++x) {
            index = y * control->width + x;
            destination = canvas->pixels +
                (((y + control->y_offset) * (size_t)canvas->width +
                  x + control->x_offset) * 4u);
            sa = alpha[index];
            if (control->blend_op == 0) {
                memcpy(destination, rgb + index * 3u, 3u * sizeof(float));
                destination[3] = (float)sa;
                continue;
            }
            da = destination[3];
            oa = sa + da * (1.0 - sa);
            for (channel = 0; channel < 3; ++channel) {
                destination[channel] = oa == 0.0 ? 0.0f : (float)(
                    (rgb[index * 3u + (size_t)channel] * sa +
                     destination[channel] * da * (1.0 - sa)) / oa);
            }
            destination[3] = (float)oa;
        }
    }
}

static void
apng_replay_cache_reset(sixel_apng_replay_cache_t *cache,
                        sixel_allocator_t *allocator)
{
    size_t index;

    index = 0u;
    if (cache == NULL) {
        return;
    }

    if (cache->frames != NULL) {
        for (index = 0u; index < cache->frame_count; ++index) {
            sixel_frame_unref(cache->frames[index]);
        }
        if (allocator != NULL) {
            sixel_allocator_free(allocator, cache->frames);
        }
    }
    cache->frames = NULL;
    cache->frame_count = 0u;
    cache->frame_capacity = 0u;
    cache->cached_bytes = 0u;
    cache->enabled = 0;
}

static int
apng_replay_cache_prepare(sixel_apng_replay_cache_t *cache,
                          sixel_allocator_t *allocator,
                          int frame_capacity_hint)
{
    size_t frame_capacity;

    frame_capacity = 0u;
    if (cache == NULL || allocator == NULL || frame_capacity_hint <= 1) {
        return 0;
    }
    if ((size_t)frame_capacity_hint > SIZE_MAX / sizeof(*cache->frames)) {
        return 0;
    }

    frame_capacity = (size_t)frame_capacity_hint;
    cache->frames = (sixel_frame_t **)sixel_allocator_calloc(
        allocator,
        frame_capacity,
        sizeof(*cache->frames));
    if (cache->frames == NULL) {
        return 0;
    }
    cache->frame_count = 0u;
    cache->frame_capacity = frame_capacity;
    cache->cached_bytes = 0u;
    cache->enabled = 1;
    return 1;
}

static int
apng_replay_cache_store_frame(sixel_apng_replay_cache_t *cache,
                              sixel_frame_t *frame,
                              sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    sixel_frame_interface_t *frame_if;
    sixel_frame_timeline_t timeline;
    size_t frame_bytes;

    status = SIXEL_FALSE;
    frame_if = NULL;
    memset(&timeline, 0, sizeof(timeline));
    frame_bytes = 0u;
    if (cache == NULL || frame == NULL || cache->enabled == 0) {
        return 0;
    }
    frame_if = sixel_frame_as_interface(frame);
    if (frame_if == NULL || frame_if->vtbl == NULL ||
        frame_if->vtbl->measure_storage == NULL ||
        frame_if->vtbl->get_timeline == NULL ||
        frame_if->vtbl->set_timeline == NULL ||
        frame_if->vtbl->ref == NULL) {
        apng_replay_cache_reset(cache, allocator);
        return 0;
    }
    if (cache->frames == NULL ||
        cache->frame_capacity == 0u ||
        cache->frame_count >= cache->frame_capacity) {
        apng_replay_cache_reset(cache, allocator);
        return 0;
    }
    if (SIXEL_FAILED(frame_if->vtbl->measure_storage(frame_if,
                                                     &frame_bytes))) {
        apng_replay_cache_reset(cache, allocator);
        return 0;
    }
    if (frame_bytes > APNG_FRAME_CACHE_MAX_BYTES_DEFAULT ||
        cache->cached_bytes >
        APNG_FRAME_CACHE_MAX_BYTES_DEFAULT - frame_bytes) {
        apng_replay_cache_reset(cache, allocator);
        return 0;
    }

    status = frame_if->vtbl->get_timeline(frame_if, &timeline);
    if (SIXEL_FAILED(status)) {
        apng_replay_cache_reset(cache, allocator);
        return 0;
    }
    timeline.handoff_shareable = 1;
    status = frame_if->vtbl->set_timeline(frame_if, &timeline);
    if (SIXEL_FAILED(status)) {
        apng_replay_cache_reset(cache, allocator);
        return 0;
    }
    frame_if->vtbl->ref(frame_if);
    cache->frames[cache->frame_count] = frame;
    cache->frame_count += 1u;
    cache->cached_bytes += frame_bytes;
    return 1;
}

static SIXELSTATUS
emit_apng_frame(
    sixel_apng_state_t const      *state,
    sixel_apng_frame_control_t    *control,
    int                            frame_no,
    int                            loop_no,
    int                            multiframe,
    int                            emit_callback,
    unsigned char                 *bgcolor,
    int                            enable_cms,
    int                            exif_orientation,
    int                            reqcolors,
    int                            fuse_palette,
    sixel_apng_canvas_t           *canvas,
    sixel_apng_replay_cache_t     *replay_cache,
    sixel_load_image_function      fn_load,
    void                          *callback_context,
    sixel_allocator_t             *allocator)
{
    SIXELSTATUS status;
    sixel_frame_t *frame;
    int width;
    int height;
    size_t png_size;
    unsigned char *png_data;
    unsigned char *subframe;
    float *emitted;
    unsigned char *mask;
    sixel_png_alpha_t alpha_state;
    size_t pixel_count;
    size_t pixel_index;
    int channel;
    int hidden;
    int has_hidden;
    double alpha;
    size_t canvas_bytes;
    int cache_frame;
    unsigned char ihdr_copy[13];

    status = SIXEL_FALSE;
    frame = NULL;
    width = 0;
    height = 0;
    png_data = NULL;
    subframe = NULL;
    emitted = NULL;
    mask = NULL;
    memset(&alpha_state, 0, sizeof(alpha_state));
    canvas_bytes = 0u;
    cache_frame = 0;
    (void)reqcolors;
    (void)fuse_palette;

    if (replay_cache != NULL &&
        replay_cache->enabled != 0 &&
        loop_no == 0) {
        cache_frame = 1;
    }

    if (state->ihdr == NULL || state->ihdr_size != 13) {
        return SIXEL_BAD_INPUT;
    }
    if (state->shared_chunks_size > SIZE_MAX - 45u ||
        state->chunk_size > SIZE_MAX - 45u - state->shared_chunks_size) {
        return SIXEL_BAD_ALLOCATION;
    }

    png_size = 8 + 25 + state->shared_chunks_size + state->chunk_size + 12;
    png_data = (unsigned char *)sixel_allocator_malloc(allocator, png_size);
    if (png_data == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    memcpy(png_data, "\x89PNG\r\n\x1a\n", 8);
    memcpy(ihdr_copy, state->ihdr, sizeof(ihdr_copy));
    write_be32(ihdr_copy + 0, control->width);
    write_be32(ihdr_copy + 4, control->height);

    memcpy(png_data + 8, "\x00\x00\x00\x0dIHDR", 8);
    memcpy(png_data + 16, ihdr_copy, sizeof(ihdr_copy));
    write_be32(png_data + 29,
               crc32_update((unsigned char const *)(png_data + 12), 17, 0));

    if (state->shared_chunks_size > 0) {
        memcpy(png_data + 33,
               state->shared_chunks,
               state->shared_chunks_size);
    }
    memcpy(png_data + 33 + state->shared_chunks_size,
           state->chunk_base,
           state->chunk_size);
    memcpy(png_data + 33 + state->shared_chunks_size + state->chunk_size,
           "\x00\x00\x00\x00"
           "IEND"
           "\xae\x42\x60\x82",
           12);

    status = decode_png_rgba(&subframe,
                             &width,
                             &height,
                             png_data,
                             png_size,
                             bgcolor,
                             enable_cms,
                             &alpha_state,
                             allocator);

    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (width != (int)control->width || height != (int)control->height) {
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    canvas_bytes = (size_t)canvas->width * (size_t)canvas->height
                   * 4u * sizeof(float);
    if (control->dispose_op == 2) {
        memcpy(canvas->backup, canvas->pixels, canvas_bytes);
    }
    apng_blend_rect(canvas, control, (float const *)subframe,
                    alpha_state.values);

    if (!emit_callback && !cache_frame) {
        status = SIXEL_OK;
        goto dispose;
    }

    pixel_count = (size_t)canvas->width * (size_t)canvas->height;
    emitted = (float *)sixel_allocator_malloc(
        allocator, pixel_count * 3u * sizeof(float));
    mask = (unsigned char *)sixel_allocator_malloc(allocator, pixel_count);
    if (emitted == NULL || mask == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    has_hidden = 0;
    for (pixel_index = 0u; pixel_index < pixel_count; ++pixel_index) {
        alpha = canvas->pixels[pixel_index * 4u + 3u];
        hidden = alpha == 0.0 && (!alpha_state.has_background ||
                 loader_transparent_policy() != SIXEL_ALPHA_POLICY_COMPOSITE);
        mask[pixel_index] = (unsigned char)hidden;
        has_hidden |= hidden;
        if (hidden || !alpha_state.has_background) {
            alpha = 1.0;
        }
        for (channel = 0; channel < 3; ++channel) {
            emitted[pixel_index * 3u + (size_t)channel] = (float)(
                canvas->pixels[pixel_index * 4u + (size_t)channel] * alpha +
                alpha_state.background[channel] * (1.0 - alpha));
        }
    }

    status = sixel_frame_create_from_factory(&frame, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    frame->transparent = (-1);
    sixel_frame_set_delay(frame, (int)control->delay_cs);
    sixel_frame_set_frame_no(frame, frame_no);
    sixel_frame_set_loop_count(frame, loop_no);
    sixel_frame_set_multiframe(frame, multiframe);
    status = sixel_frame_as_interface(frame)->vtbl->init_pixels(
        sixel_frame_as_interface(frame),
        &(sixel_frame_pixels_request_t){
            (unsigned char *)emitted,
            NULL,
            canvas->width,
            canvas->height,
            SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
            SIXEL_COLORSPACE_LINEAR,
            0,
            SIXEL_FRAME_PIXELS_FLOAT32
        });
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    frame->alpha_zero_is_transparent = has_hidden;
    if (has_hidden) {
        frame->transparent_mask = mask;
        frame->transparent_mask_size = pixel_count;
        mask = NULL;
    }
    emitted = NULL;

    if (exif_orientation >= 2 && exif_orientation <= 8) {
        status = loader_frame_apply_orientation(frame, exif_orientation);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    if (cache_frame) {
        (void)apng_replay_cache_store_frame(replay_cache, frame, allocator);
    }

    if (emit_callback) {
        status = fn_load(frame, callback_context);
    } else {
        status = SIXEL_OK;
    }

dispose:

    if (control->dispose_op == 1) {
        apng_clear_rect(canvas, control);
    } else if (control->dispose_op == 2) {
        memcpy(canvas->pixels, canvas->backup, canvas_bytes);
    }

end:
    sixel_allocator_free(allocator, png_data);
    sixel_allocator_free(allocator, subframe);
    sixel_allocator_free(allocator, emitted);
    sixel_allocator_free(allocator, mask);
    sixel_allocator_free(allocator, alpha_state.values);
    sixel_frame_unref(frame);

    return status;
}

static SIXELSTATUS
load_apng_frames(
    sixel_chunk_t const       *pchunk,
    sixel_allocator_t         *allocator,
    int                        fstatic,
    int                        fuse_palette,
    int                        reqcolors,
    unsigned char             *bgcolor,
    int                        exif_orientation,
    int                        enable_cms,
    int                        loop_control,
    int                        start_frame_no_set,
    int                        start_frame_no_override,
    sixel_load_image_function  fn_load,
    void                      *context)
{
    static unsigned char const png_signature[8] = {
        0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au
    };
    SIXELSTATUS status;
    sixel_apng_state_t state;
    sixel_apng_frame_control_t control;
    unsigned char const *p;
    size_t remain;
    int seen_actl;
    int has_frame;
    int emit_frame_no;
    int source_frame_no;
    int frames_in_loop;
    int num_frames;
    int num_plays;
    int loop_no;
    int stop_loop;
    int saw_animation;
    int seen_fctl;
    int seen_idat;
    int emit_callback;
    sixel_apng_canvas_t canvas;
    size_t canvas_bytes;
    png_uint_32 sequence_no;
    png_uint_32 fd_sequence;
    png_uint_32 length;
    sixel_apng_replay_cache_t replay_cache;
    sixel_frame_t *replay_frame;
    size_t replay_index;
    int replay_from_cache;
    int start_frame_no;
    int start_frame_no_ready;
    int trace_start_frame_no;

    status = SIXEL_FALSE;
    memset(&state, 0, sizeof(state));
    memset(&control, 0, sizeof(control));
    p = NULL;
    remain = 0;
    seen_actl = 0;
    has_frame = 0;
    emit_frame_no = 0;
    source_frame_no = 0;
    frames_in_loop = 0;
    num_frames = 0;
    num_plays = 0;
    loop_no = 0;
    stop_loop = 0;
    saw_animation = 0;
    seen_fctl = 0;
    seen_idat = 0;
    emit_callback = 1;
    memset(&canvas, 0, sizeof(canvas));
    canvas_bytes = 0;
    sequence_no = 0;
    fd_sequence = 0;
    memset(&replay_cache, 0, sizeof(replay_cache));
    replay_frame = NULL;
    replay_index = 0u;
    replay_from_cache = 0;
    start_frame_no = INT_MIN;
    start_frame_no_ready = 0;
    trace_start_frame_no = INT_MIN;
    if (start_frame_no_set) {
        trace_start_frame_no = start_frame_no_override;
    }

    /*
     * APNG parsing starts after the PNG signature. Guard against short
     * buffers so size_t subtraction cannot underflow.
     */
    if (pchunk == NULL || allocator == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    if (sixel_chunk_get_buffer(pchunk) == NULL ||
        sixel_chunk_get_size(pchunk) < sizeof(png_signature)) {
        status = SIXEL_FALSE;
        goto end;
    }
    if (memcmp(sixel_chunk_get_buffer(pchunk), png_signature,
               sizeof(png_signature)) != 0) {
        status = SIXEL_FALSE;
        goto end;
    }

    apng_decode_trace_message(
        "load_apng_frames: input_size=%lu static=%d loop_control=%d "
        "start_frame_no=%d",
        (unsigned long)sixel_chunk_get_size(pchunk),
        fstatic,
        loop_control,
        trace_start_frame_no);

    for (;;) {
        if (sixel_loader_callback_is_canceled(context)) {
            status = SIXEL_INTERRUPTED;
            goto end;
        }
        replay_from_cache = 0;
        if (loop_no > 0 &&
            replay_cache.enabled != 0 &&
            replay_cache.frame_count > 0u &&
            replay_cache.frame_count == replay_cache.frame_capacity) {
            replay_from_cache = 1;
        }
        if (replay_from_cache != 0) {
            frames_in_loop = (int)replay_cache.frame_count;
            emit_frame_no = 0;
            for (replay_index = 0u;
                 replay_index < replay_cache.frame_count;
                 ++replay_index) {
                if (sixel_loader_callback_is_canceled(context)) {
                    status = SIXEL_INTERRUPTED;
                    goto end;
                }
                replay_frame = replay_cache.frames[replay_index];
                if (replay_frame == NULL) {
                    status = SIXEL_RUNTIME_ERROR;
                    goto end;
                }
                sixel_frame_set_frame_no(replay_frame, emit_frame_no);
                sixel_frame_set_loop_count(replay_frame, loop_no);
                sixel_frame_set_multiframe(
                    replay_frame,
                    (!fstatic && frames_in_loop > 1));
                status = fn_load(replay_frame, context);
                if (SIXEL_FAILED(status)) {
                    goto end;
                }
                ++emit_frame_no;
            }
            source_frame_no = frames_in_loop;
            ++loop_no;

            stop_loop = 0;
            if (loop_control == SIXEL_LOOP_DISABLE || frames_in_loop == 1) {
                stop_loop = 1;
            } else if (loop_control == SIXEL_LOOP_AUTO) {
                if (num_plays > 0 && loop_no >= num_plays) {
                    stop_loop = 1;
                }
            }
            if (stop_loop) {
                apng_decode_trace_message(
                    "load_apng_frames: stop loop_no=%d frames_in_loop=%d "
                    "num_plays=%d (cache)",
                    loop_no,
                    frames_in_loop,
                    num_plays);
                status = SIXEL_OK;
                goto end;
            }
            continue;
        }

        memset(&state, 0, sizeof(state));
        memset(&control, 0, sizeof(control));
        p = sixel_chunk_get_buffer(pchunk) + 8;
        remain = sixel_chunk_get_size(pchunk) - 8;
        seen_actl = 0;
        has_frame = 0;
        source_frame_no = 0;
        frames_in_loop = 0;
        seen_fctl = 0;
        seen_idat = 0;

        if (loop_no > 0 && canvas_bytes > 0) {
            memset(canvas.pixels, 0, canvas_bytes);
            memset(canvas.backup, 0, canvas_bytes);
        }

    while (remain >= 12) {
        if (sixel_loader_callback_is_canceled(context)) {
            status = SIXEL_INTERRUPTED;
            goto end;
        }

        length = read_be32(p);
        if ((size_t)length > remain - 12) {
            sixel_helper_set_additional_message(
                "APNG parse error: chunk length exceeds input size");
            status = SIXEL_BAD_INPUT;
            goto end;
        }

        apng_decode_trace_message(
            "chunk loop=%d remain=%lu type=%.4s length=%lu expected_seq=%lu",
            loop_no,
            (unsigned long)remain,
            (char const *)(p + 4),
            (unsigned long)length,
            (unsigned long)state.expected_sequence);

        if (memcmp(p + 4, "IHDR", 4) == 0) {
            if (length != 13) {
                sixel_helper_set_additional_message(
                    "APNG parse error: invalid IHDR chunk length");
                status = SIXEL_BAD_INPUT;
                goto end;
            }
            state.ihdr = p + 8;
            state.ihdr_size = length;
            if (canvas_bytes == 0) {
                canvas.width = (int)read_be32(p + 8);
                canvas.height = (int)read_be32(p + 12);
            }
            if (canvas.width <= 0 || canvas.height <= 0) {
                sixel_helper_set_additional_message(
                    "APNG parse error: invalid canvas size");
                status = SIXEL_BAD_INPUT;
                goto end;
            }
            if (canvas_bytes == 0) {
                if ((size_t)canvas.width >
                    SIZE_MAX / (size_t)canvas.height / (4u * sizeof(float))) {
                    status = SIXEL_BAD_INTEGER_OVERFLOW;
                    goto end;
                }
                canvas_bytes = (size_t)canvas.width *
                               (size_t)canvas.height * 4u * sizeof(float);
                canvas.pixels = (float *)sixel_allocator_malloc(
                    allocator,
                    canvas_bytes);
                canvas.backup = (float *)sixel_allocator_malloc(
                    allocator,
                    canvas_bytes);
                if (canvas.pixels == NULL || canvas.backup == NULL) {
                    status = SIXEL_BAD_ALLOCATION;
                    goto end;
                }
                memset(canvas.pixels, 0, canvas_bytes);
                memset(canvas.backup, 0, canvas_bytes);
            }
        } else if (memcmp(p + 4, "acTL", 4) == 0) {
            if (length != 8) {
                sixel_helper_set_additional_message(
                    "APNG parse error: invalid acTL chunk length");
                status = SIXEL_BAD_INPUT;
                goto end;
            }
            seen_actl = 1;
            saw_animation = 1;
            num_frames = (int)read_be32(p + 8);
            num_plays = (int)read_be32(p + 12);
            apng_decode_trace_message(
                "acTL parsed: num_frames=%d num_plays=%d loop_no=%d",
                num_frames,
                num_plays,
                loop_no);
            state.expected_sequence = 0;
            if (num_frames <= 0) {
                sixel_helper_set_additional_message(
                    "APNG parse error: acTL num_frames must be > 0");
                status = SIXEL_BAD_INPUT;
                goto end;
            }
            if (loop_no == 0 &&
                fstatic == 0 &&
                loop_control != SIXEL_LOOP_DISABLE &&
                replay_cache.frames == NULL) {
                (void)apng_replay_cache_prepare(&replay_cache,
                                                allocator,
                                                num_frames);
            }
            if (loop_no == 0 && !start_frame_no_ready) {
                /*
                 * Parse start-frame lazily after acTL confirmation so static
                 * PNG input ignores invalid animation start-frame settings.
                 */
                if (start_frame_no_set) {
                    start_frame_no = start_frame_no_override;
                } else {
                    status = libpng_parse_animation_start_frame_no(
                        &start_frame_no);
                    if (SIXEL_FAILED(status)) {
                        goto end;
                    }
                }
                start_frame_no_ready = 1;
            }
            if (loop_no == 0 && start_frame_no != INT_MIN) {
                status = libpng_resolve_animation_start_frame_no(
                    start_frame_no,
                    num_frames,
                    &start_frame_no);
                if (SIXEL_FAILED(status)) {
                    goto end;
                }
            }
        } else if (memcmp(p + 4, "fcTL", 4) == 0 && seen_actl) {
            if (has_frame && state.chunk_size > 0) {
                emit_callback = 1;
                if (loop_no == 0 && start_frame_no != INT_MIN &&
                    frames_in_loop < start_frame_no) {
                    emit_callback = 0;
                }
                if (loop_no == 0 && start_frame_no != INT_MIN) {
                    /*
                     * frame_no is consumed by the encoder to determine
                     * whether DECSC (first emitted frame) or DECRC
                     * (subsequent frame) should be written in tty scroll.
                     * Keep it as an emitted-frame index for the first loop
                     * when start-frame skips leading source frames.
                     */
                    emit_frame_no = source_frame_no - start_frame_no;
                } else {
                    emit_frame_no = source_frame_no;
                }
                status = emit_apng_frame(&state,
                                         &control,
                                         emit_frame_no,
                                         loop_no,
                                         (!fstatic && num_frames > 1),
                                         emit_callback,
                                         bgcolor,
                                         enable_cms,
                                         exif_orientation,
                                         reqcolors,
                                         fuse_palette,
                                         &canvas,
                                         &replay_cache,
                                         fn_load,
                                         context,
                                         allocator);
                if (SIXEL_FAILED(status)) {
                    goto end;
                }

                if (sixel_loader_callback_is_canceled(context)) {
                    status = SIXEL_INTERRUPTED;
                    goto end;
                }

                ++source_frame_no;
                ++frames_in_loop;
                if (fstatic && emit_callback) {
                    status = SIXEL_OK;
                    goto end;
                }
                state.chunk_size = 0;
            }
            if (!parse_fctl(p + 8, length, &sequence_no, &control)) {
                status = SIXEL_BAD_INPUT;
                goto end;
            }
            if (sequence_no != state.expected_sequence) {
                sixel_helper_set_additional_message(
                    "APNG parse error: fcTL sequence number mismatch");
                status = SIXEL_BAD_INPUT;
                goto end;
            }
            ++state.expected_sequence;
            apng_decode_trace_message(
                "fcTL accepted: seq=%lu next_expected=%lu seen_idat=%d",
                (unsigned long)sequence_no,
                (unsigned long)state.expected_sequence,
                seen_idat);
            if (control.width == 0 || control.height == 0 ||
                control.x_offset > (png_uint_32)canvas.width ||
                control.y_offset > (png_uint_32)canvas.height ||
                control.width > (png_uint_32)canvas.width - control.x_offset ||
                control.height >
                (png_uint_32)canvas.height - control.y_offset) {
                sixel_helper_set_additional_message(
                    "APNG parse error: fcTL rectangle is outside canvas");
                status = SIXEL_BAD_INPUT;
                goto end;
            }
            seen_fctl = 1;
            has_frame = 1;
        } else if (memcmp(p + 4, "fdAT", 4) == 0 && seen_actl) {
            if (!has_frame || !seen_fctl || length < 4) {
                sixel_helper_set_additional_message(
                    "APNG parse error: fdAT encountered before valid fcTL");
                status = SIXEL_BAD_INPUT;
                goto end;
            }
            fd_sequence = read_be32(p + 8);
            if (fd_sequence != state.expected_sequence) {
                sixel_helper_set_additional_message(
                    "APNG parse error: fdAT sequence number mismatch");
                status = SIXEL_BAD_INPUT;
                goto end;
            }
            ++state.expected_sequence;
            if (!append_chunk(&state,
                              "IDAT",
                              p + 12,
                              length - 4,
                              allocator)) {
                status = SIXEL_BAD_ALLOCATION;
                goto end;
            }
        } else if (memcmp(p + 4, "IDAT", 4) == 0) {
            /* The classifier already checked contiguous IDAT ordering.
             * Every chunk of an excluded default image is skipped. */
            if (seen_fctl && !append_chunk(&state,
                                           "IDAT", p + 8, length,
                                           allocator)) {
                status = SIXEL_BAD_ALLOCATION;
                goto end;
            }
            seen_idat = 1;
        } else if (memcmp(p + 4, "IEND", 4) == 0) {
            break;
        } else if (memcmp(p + 4, "acTL", 4) != 0 &&
                   memcmp(p + 4, "fcTL", 4) != 0 &&
                   memcmp(p + 4, "fdAT", 4) != 0 &&
                   memcmp(p + 4, "IHDR", 4) != 0 &&
                   memcmp(p + 4, "IEND", 4) != 0 &&
                   !seen_idat) {
            /* Preserve original pre-IDAT metadata, including tRNS. Moving
             * later color chunks here would make invalid ordering usable. */
            if (!append_shared_chunk(&state,
                                     p,
                                     (size_t)length + 12,
                                     allocator)) {
                status = SIXEL_BAD_ALLOCATION;
                goto end;
            }
        }

        p += (size_t)length + 12;
        remain -= (size_t)length + 12;
    }

    if (!seen_actl || !has_frame) {
        status = SIXEL_FALSE;
        goto end;
    }

    if (state.chunk_size > 0) {
        emit_callback = 1;
        if (loop_no == 0 && start_frame_no != INT_MIN &&
            frames_in_loop < start_frame_no) {
            emit_callback = 0;
        }
        if (loop_no == 0 && start_frame_no != INT_MIN) {
            /*
             * frame_no is used by the encoder/tty path to select DECSC
             * for the first emitted frame and DECRC for subsequent frames.
             * Keep frame_no aligned to emitted order when the first loop
             * skips leading source frames.
             */
            emit_frame_no = source_frame_no - start_frame_no;
        } else {
            emit_frame_no = source_frame_no;
        }
        status = emit_apng_frame(&state,
                                 &control,
                                 emit_frame_no,
                                 loop_no,
                                 (!fstatic && num_frames > 1),
                                  emit_callback,
                                  bgcolor,
                                  enable_cms,
                                  exif_orientation,
                                  reqcolors,
                                  fuse_palette,
                                  &canvas,
                                  &replay_cache,
                                 fn_load,
                                 context,
                                 allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        if (sixel_loader_callback_is_canceled(context)) {
            status = SIXEL_INTERRUPTED;
            goto end;
        }

        ++source_frame_no;
        ++frames_in_loop;
        if (fstatic && emit_callback) {
            status = SIXEL_OK;
            goto end;
        }
    }

    if (frames_in_loop == 0) {
        sixel_helper_set_additional_message(
            "APNG parse error: no decodable frame in animation");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    if (num_frames > 0 && frames_in_loop != num_frames) {
        sixel_helper_set_additional_message(
            "APNG parse error: decoded frame count mismatch");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    ++loop_no;

    if (loop_control == SIXEL_LOOP_DISABLE || frames_in_loop == 1) {
        stop_loop = 1;
    } else if (loop_control == SIXEL_LOOP_AUTO) {
        if (num_plays > 0 && loop_no >= num_plays) {
            stop_loop = 1;
        }
    }

    sixel_allocator_free(allocator, state.shared_chunks);
    sixel_allocator_free(allocator, (void *)state.chunk_base);
    state.shared_chunks = NULL;
    state.chunk_base = NULL;

    if (stop_loop) {
        apng_decode_trace_message(
            "load_apng_frames: stop loop_no=%d frames_in_loop=%d "
            "num_plays=%d",
            loop_no,
            frames_in_loop,
            num_plays);
        status = SIXEL_OK;
        goto end;
    }
    }

end:
    apng_decode_trace_message(
        "load_apng_frames: status=%d emit_frame_no=%d source_frame_no=%d "
        "loop_no=%d saw_animation=%d",
        status,
        emit_frame_no,
        source_frame_no,
        loop_no,
        saw_animation);
    sixel_allocator_free(allocator, canvas.pixels);
    sixel_allocator_free(allocator, canvas.backup);
    sixel_allocator_free(allocator, state.shared_chunks);
    sixel_allocator_free(allocator, (void *)state.chunk_base);
    apng_replay_cache_reset(&replay_cache, allocator);
    if (!saw_animation && status == SIXEL_FALSE) {
        return SIXEL_FALSE;
    }
    return status;
}

/*
 * Dedicated libpng loader for precise PNG decoding.
 *
 *    +-----------+     +------------------+     +--------------------+
 *    | PNG chunk | --> | libpng decode    | --> | sixel frame emit   |
 *    +-----------+     +------------------+     +--------------------+
 */
/*
 * Classify the original chunk stream without allocating image storage. APNG
 * reconstruction must never replace a corrupt original CRC with a valid one.
 * Check its animation grammar before the first callback, including the default
 * image that can be excluded from the animation. libpng validates static CRCs
 * and recoverable ancillary chunks while reading the original PNG.
 */
static SIXELSTATUS
libpng_classify_stream(sixel_chunk_t const *chunk, int *animated)
{
    unsigned char const *buffer;
    unsigned char const *p;
    size_t size;
    size_t offset;
    size_t length;
    png_uint_32 frames;
    png_uint_32 controls;
    png_uint_32 sequence;
    sixel_apng_frame_control_t control;
    png_uint_32 frame_sequence;
    png_uint_32 width;
    png_uint_32 height;
    int seen_idat;
    int idat_closed;
    int frame_data;
    int included_default;
    int critical;
    int animation_chunk;

    buffer = sixel_chunk_get_buffer(chunk);
    size = sixel_chunk_get_size(chunk);
    *animated = 0;
    frames = 0u;
    controls = 0u;
    sequence = 0u;
    width = 0u;
    height = 0u;
    seen_idat = 0;
    idat_closed = 0;
    frame_data = 0;
    included_default = 0;
    if (buffer == NULL || size < 33u ||
        memcmp(buffer, "\211PNG\r\n\032\n", 8) != 0) {
        return SIXEL_BAD_INPUT;
    }
    for (offset = 8u; offset <= size && size - offset >= 12u;
         offset += length + 12u) {
        p = buffer + offset;
        length = read_be32(p);
        if (length > size - offset - 12u) {
            goto invalid;
        }
        if (offset == 8u) {
            if (length != 13u || memcmp(p + 4, "IHDR", 4) != 0) {
                goto invalid;
            }
            width = read_be32(p + 8);
            height = read_be32(p + 12);
        } else if (memcmp(p + 4, "IHDR", 4) == 0) {
            goto invalid;
        }
        if (memcmp(p + 4, "acTL", 4) == 0) {
            if (*animated || seen_idat || length != 8u) {
                goto invalid;
            }
            frames = read_be32(p + 8);
            if (frames == 0u || frames > INT_MAX ||
                read_be32(p + 12) > INT_MAX) {
                goto invalid;
            }
            *animated = 1;
        } else if (memcmp(p + 4, "fcTL", 4) == 0) {
            if (!*animated || (controls != 0u && !frame_data) ||
                !parse_fctl(p + 8, length, &frame_sequence, &control) ||
                frame_sequence != sequence || sequence == UINT32_MAX) {
                goto invalid;
            }
            ++sequence;
            ++controls;
            frame_data = 0;
            if (!seen_idat) {
                included_default = 1;
                if (control.width != width || control.height != height ||
                    control.x_offset != 0u || control.y_offset != 0u) {
                    goto invalid;
                }
            }
            if (control.width == 0u || control.height == 0u ||
                control.x_offset > width || control.y_offset > height ||
                control.width > width - control.x_offset ||
                control.height > height - control.y_offset) {
                goto invalid;
            }
        } else if (memcmp(p + 4, "fdAT", 4) == 0) {
            if (!*animated || !seen_idat || controls == 0u ||
                (included_default && controls == 1u) || length < 4u ||
                read_be32(p + 8) != sequence || sequence == UINT32_MAX) {
                goto invalid;
            }
            ++sequence;
            frame_data |= length > 4u;
        } else if (memcmp(p + 4, "IDAT", 4) == 0) {
            if (idat_closed) {
                goto invalid;
            }
            seen_idat = 1;
            if (included_default) {
                frame_data |= length > 0u;
            }
        } else if (memcmp(p + 4, "IEND", 4) == 0) {
            if (length != 0u ||
                read_be32(p + 8) != UINT32_C(0xae426082) || !seen_idat ||
                (*animated && (controls != frames || !frame_data))) {
                goto invalid;
            }
            break;
        }
        if (seen_idat && memcmp(p + 4, "IDAT", 4) != 0) {
            idat_closed = 1;
        }
        if (seen_idat && (memcmp(p + 4, "PLTE", 4) == 0 ||
                         memcmp(p + 4, "tRNS", 4) == 0)) {
            goto invalid;
        }
    }
    if (offset > size || size - offset < 12u) {
        goto invalid;
    }
    if (!*animated) {
        return SIXEL_OK;
    }
    for (offset = 8u; size - offset >= 12u; offset += length + 12u) {
        p = buffer + offset;
        length = read_be32(p);
        critical = (p[4] & 0x20u) == 0u;
        animation_chunk = memcmp(p + 4, "acTL", 4) == 0 ||
                          memcmp(p + 4, "fcTL", 4) == 0 ||
                          memcmp(p + 4, "fdAT", 4) == 0;
        if (critical && memcmp(p + 4, "IHDR", 4) != 0 &&
            memcmp(p + 4, "PLTE", 4) != 0 &&
            memcmp(p + 4, "IDAT", 4) != 0 &&
            memcmp(p + 4, "IEND", 4) != 0) {
            goto invalid;
        }
        if ((critical || animation_chunk) &&
            crc32_update(p + 4, length + 4u, 0) !=
            read_be32(p + 8 + length)) {
            sixel_helper_set_additional_message("APNG: original CRC error");
            return SIXEL_PNG_ERROR;
        }
        if (memcmp(p + 4, "IEND", 4) == 0) {
            break;
        }
    }
    return SIXEL_OK;

invalid:
    sixel_helper_set_additional_message("PNG: invalid chunk structure");
    return SIXEL_BAD_INPUT;
}

static SIXELSTATUS
load_with_libpng(
    sixel_chunk_t const       /* in */     *pchunk,
    sixel_allocator_t         /* in */     *allocator,
    int                       /* in */     fstatic,
    int                       /* in */     fuse_palette,
    int                       /* in */     reqcolors,
    unsigned char             /* in */     *bgcolor,
    int                       /* in */     loop_control,
    int                       /* in */     start_frame_no_set,
    int                       /* in */     start_frame_no_override,
    int                       /* in */     enable_cms_override,
    int                       /* in */     enable_orientation_override,
    sixel_load_image_function /* in */     fn_load,
    void                      /* in/out */ *context)
{
    SIXELSTATUS status;
    sixel_frame_t *frame;
    unsigned char *pixels;
    int enable_cms;
    int cms_applied;
    int alpha_zero_is_transparent;
    int cms_target_pixelformat;
    int enable_orientation;
    int exif_orientation;
    int animated;
    sixel_png_alpha_t alpha_state;
    size_t pixel_count;
    size_t pixel_index;

    status = SIXEL_FALSE;
    frame = NULL;
    pixels = NULL;
    memset(&alpha_state, 0, sizeof(alpha_state));
    enable_cms = enable_cms_override != 0 ? 1 : 0;
    cms_applied = 0;
    alpha_zero_is_transparent = 0;
    cms_target_pixelformat = SIXEL_PIXELFORMAT_RGB888;
    enable_orientation = enable_orientation_override != 0 ? 1 : 0;
    exif_orientation = 1;

    if (pchunk == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = libpng_classify_stream(pchunk, &animated);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    if (enable_orientation) {
        (void)libpng_parse_exif_orientation(sixel_chunk_get_buffer(pchunk),
                                            sixel_chunk_get_size(pchunk),
                                            &exif_orientation);
    }

    if (animated) {
        status = load_apng_frames(pchunk,
                                  allocator,
                                  fstatic,
                                  fuse_palette,
                                  reqcolors,
                                  bgcolor,
                                  exif_orientation,
                                  enable_cms,
                                  loop_control,
                                  start_frame_no_set,
                                  start_frame_no_override,
                                  fn_load,
                                  context);
        /* Classification is final, including a callback's SIXEL_FALSE. */
        goto end;
    }

    status = sixel_frame_create_from_factory(&frame, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = load_png(&pixels,
                      sixel_chunk_get_buffer(pchunk),
                      sixel_chunk_get_size(pchunk),
                      &frame->width,
                      &frame->height,
                      &frame->palette,
                      &frame->ncolors,
                      fuse_palette ? reqcolors : 0,
                      &frame->pixelformat,
                      bgcolor,
                      &frame->transparent,
                      &alpha_zero_is_transparent,
                      &cms_applied,
                      enable_cms,
                      &alpha_state,
                      allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_frame_as_interface(frame)->vtbl->init_pixels(
        sixel_frame_as_interface(frame),
        &(sixel_frame_pixels_request_t){
            pixels,
            frame->palette,
            frame->width,
            frame->height,
            frame->pixelformat,
            png_colorspace_from_pixelformat(frame->pixelformat),
            frame->ncolors,
            SIXEL_PIXELFORMAT_IS_FLOAT32(frame->pixelformat)
            ? SIXEL_FRAME_PIXELS_FLOAT32
            : SIXEL_FRAME_PIXELS_U8
        });
    if (SIXEL_FAILED(status)) {
        sixel_allocator_free(allocator, pixels);
        goto end;
    }
    frame->alpha_zero_is_transparent = alpha_zero_is_transparent != 0 ? 1 : 0;
    pixels = NULL;
    if (alpha_state.values != NULL &&
        (!alpha_state.has_background || loader_transparent_policy() !=
         SIXEL_ALPHA_POLICY_COMPOSITE)) {
        pixel_count = (size_t)frame->width * (size_t)frame->height;
        frame->transparent_mask = (unsigned char *)sixel_allocator_malloc(
            allocator, pixel_count);
        if (frame->transparent_mask == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        frame->transparent_mask_size = pixel_count;
        for (pixel_index = 0u; pixel_index < pixel_count; ++pixel_index) {
            frame->transparent_mask[pixel_index] =
                alpha_state.values[pixel_index] == 0.0f;
            frame->alpha_zero_is_transparent |=
                frame->transparent_mask[pixel_index];
        }
    }
    if (cms_applied
            && ((frame->pixelformat & SIXEL_FORMATTYPE_PALETTE) == 0)
            && frame->pixelformat != SIXEL_PIXELFORMAT_LINEARRGBFLOAT32) {
        cms_target_pixelformat = loader_cms_target_pixelformat();
        status = sixel_frame_set_pixelformat(frame, cms_target_pixelformat);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    if (enable_orientation && exif_orientation >= 2 && exif_orientation <= 8) {
        if (frame->pixelformat == SIXEL_PIXELFORMAT_PAL1 ||
            frame->pixelformat == SIXEL_PIXELFORMAT_PAL2 ||
            frame->pixelformat == SIXEL_PIXELFORMAT_PAL4) {
            status = sixel_frame_set_pixelformat(frame, SIXEL_PIXELFORMAT_PAL8);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
        } else if (frame->pixelformat == SIXEL_PIXELFORMAT_G1 ||
                   frame->pixelformat == SIXEL_PIXELFORMAT_G2 ||
                   frame->pixelformat == SIXEL_PIXELFORMAT_G4) {
            status = sixel_frame_set_pixelformat(frame, SIXEL_PIXELFORMAT_G8);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
        }
        status = loader_frame_apply_orientation(frame, exif_orientation);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    if (!frame->alpha_zero_is_transparent) {
        status = sixel_frame_strip_alpha(frame, bgcolor);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    status = fn_load(frame, context);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = SIXEL_OK;

end:
    sixel_allocator_free(allocator, alpha_state.values);
    sixel_frame_unref(frame);

    return status;
}


static void
sixel_loader_libpng_ref(sixel_loader_component_t *component)
{
    sixel_loader_libpng_component_t *self;

    self = NULL;
    if (component == NULL) {
        return;
    }

    self = (sixel_loader_libpng_component_t *)component;
    ++self->ref;
}

static void
sixel_loader_libpng_unref(sixel_loader_component_t *component)
{
    sixel_loader_libpng_component_t *self;
    sixel_allocator_t *allocator;

    self = NULL;
    allocator = NULL;
    if (component == NULL) {
        return;
    }

    self = (sixel_loader_libpng_component_t *)component;
    if (self->ref == 0u) {
        return;
    }

    --self->ref;
    if (self->ref > 0u) {
        return;
    }

    allocator = self->allocator;
    sixel_allocator_free(allocator, self);
    sixel_allocator_unref(allocator);
}

static SIXELSTATUS
sixel_loader_libpng_setopt(sixel_loader_component_t *component,
                           int option,
                           void const *value)
{
    sixel_loader_libpng_component_t *self;
    int const *flag;
    unsigned char const *color;

    self = NULL;
    flag = NULL;
    color = NULL;
    if (component == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    self = (sixel_loader_libpng_component_t *)component;
    switch (option) {
    case SIXEL_LOADER_OPTION_REQUIRE_STATIC:
        flag = (int const *)value;
        self->fstatic = flag != NULL ? *flag : 0;
        return SIXEL_OK;
    case SIXEL_LOADER_OPTION_USE_PALETTE:
        flag = (int const *)value;
        self->fuse_palette = flag != NULL ? *flag : 0;
        return SIXEL_OK;
    case SIXEL_LOADER_OPTION_REQCOLORS:
        flag = (int const *)value;
        if (flag != NULL) {
            self->reqcolors = *flag;
        }
        return SIXEL_OK;
    case SIXEL_LOADER_OPTION_BGCOLOR:
        if (value == NULL) {
            self->has_bgcolor = 0;
            return SIXEL_OK;
        }
        color = (unsigned char const *)value;
        self->bgcolor[0] = color[0];
        self->bgcolor[1] = color[1];
        self->bgcolor[2] = color[2];
        self->has_bgcolor = 1;
        return SIXEL_OK;
    case SIXEL_LOADER_OPTION_LOOP_CONTROL:
        flag = (int const *)value;
        if (flag != NULL) {
            self->loop_control = *flag;
        }
        return SIXEL_OK;
    case SIXEL_LOADER_OPTION_START_FRAME_NO:
        if (value == NULL) {
            self->has_start_frame_no = 0;
            self->start_frame_no = INT_MIN;
            return SIXEL_OK;
        }
        flag = (int const *)value;
        self->start_frame_no = *flag;
        self->has_start_frame_no = 1;
        return SIXEL_OK;
    case SIXEL_LOADER_COMPONENT_OPTION_LIBPNG_ENABLE_CMS:
        flag = (int const *)value;
        self->enable_cms = (flag != NULL && *flag != 0) ? 1 : 0;
        return SIXEL_OK;
    case SIXEL_LOADER_COMPONENT_OPTION_LIBPNG_ENABLE_ORIENTATION:
        flag = (int const *)value;
        self->enable_orientation = (flag == NULL || *flag != 0) ? 1 : 0;
        return SIXEL_OK;
    case SIXEL_LOADER_COMPONENT_OPTION_CMS_ENGINE:
        flag = (int const *)value;
        if (flag != NULL && *flag >= 0) {
            self->enable_cms = (*flag == SIXEL_CMS_ENGINE_NONE) ? 0 : 1;
        }
        sixel_helper_set_loader_cms_engine(flag != NULL ? *flag : -1);
        return SIXEL_OK;
    default:
        return SIXEL_OK;
    }
}

static SIXELSTATUS
sixel_loader_libpng_load(sixel_loader_component_t *component,
                         sixel_chunk_t const *chunk,
                         sixel_load_image_function fn_load,
                         void *context)
{
    sixel_loader_libpng_component_t *self;
    unsigned char *bgcolor;
    SIXELSTATUS status;
    int header_job_id;
    int decode_job_id;
    sixel_loader_timeline_callback_state_t timeline_state;

    self = NULL;
    bgcolor = NULL;
    status = SIXEL_FALSE;
    header_job_id = -1;
    decode_job_id = -1;
    if (component == NULL || chunk == NULL || fn_load == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    self = (sixel_loader_libpng_component_t *)component;
    if (self->has_bgcolor) {
        bgcolor = self->bgcolor;
    }

    header_job_id = loader_timeline_phase_start("header/read");
    decode_job_id = loader_timeline_phase_start("decode/pixels");
    loader_timeline_callback_state_init(&timeline_state,
                                        fn_load,
                                        context,
                                        header_job_id,
                                        decode_job_id);

    status = load_with_libpng(chunk,
                              self->allocator,
                              self->fstatic,
                              self->fuse_palette,
                              self->reqcolors,
                              bgcolor,
                              self->loop_control,
                              self->has_start_frame_no,
                              self->start_frame_no,
                              self->enable_cms,
                              self->enable_orientation,
                              loader_timeline_emit_frame_callback,
                              &timeline_state);

    loader_timeline_callback_close_header(&timeline_state, status);
    loader_timeline_callback_close_decode(&timeline_state, status);
    loader_timeline_optional_skip_if_unmarked("post/colorspace");
    loader_timeline_optional_skip_if_unmarked("post/background");
    loader_timeline_optional_skip_if_unmarked("post/icc");

    return status;
}

static char const *
sixel_loader_libpng_name(sixel_loader_component_t const *component)
{
    (void)component;
    return "libpng";
}

static int
sixel_loader_libpng_predicate(sixel_loader_component_t *component,
                              sixel_chunk_t const *chunk)
{
    (void)component;
    return loader_can_try_libpng(chunk);
}

static sixel_loader_component_vtbl_t const g_sixel_loader_libpng_vtbl = {
    sixel_loader_libpng_ref,
    sixel_loader_libpng_unref,
    sixel_loader_libpng_setopt,
    sixel_loader_libpng_load,
    sixel_loader_libpng_name,
    sixel_loader_libpng_predicate
};

SIXELSTATUS
sixel_loader_libpng_new(sixel_allocator_t *allocator,
                        void **ppcomponent)
{
    sixel_loader_libpng_component_t *self;

    self = NULL;
    if (allocator == NULL || ppcomponent == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *ppcomponent = NULL;
    self = (sixel_loader_libpng_component_t *)
        sixel_allocator_malloc(allocator, sizeof(*self));
    if (self == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    memset(self, 0, sizeof(*self));
    self->base.vtbl = &g_sixel_loader_libpng_vtbl;
    self->allocator = allocator;
    self->ref = 1u;
    self->reqcolors = 256;
    self->start_frame_no = INT_MIN;
    self->enable_cms = 0;
    self->enable_orientation = 1;
    sixel_allocator_ref(allocator);
    *ppcomponent = &self->base;
    return SIXEL_OK;
}

static int
loader_can_try_libpng(sixel_chunk_t const *chunk)
{
    if (chunk == NULL) {
        return 0;
    }

    return chunk_is_png(chunk);
}

#else  /* !HAVE_LIBPNG */

/*
 * Provide a dummy symbol so that pedantic compilers do not flag the unit as
 * empty when libpng support is disabled at configure time.
 */
enum { sixel_loader_libpng_placeholder = 0 };

#if defined(__GNUC__) || defined(__clang__)
# define SIXEL_LIBPNG_PLACEHOLDER_UNUSED __attribute__((unused))
#else
# define SIXEL_LIBPNG_PLACEHOLDER_UNUSED
#endif

static void
sixel_loader_libpng_placeholder_function(void)
    SIXEL_LIBPNG_PLACEHOLDER_UNUSED;

static void
sixel_loader_libpng_placeholder_function(void)
{
    /*
     * Tie the placeholder enum to a symbol so MSVC does not warn about an
     * empty translation unit when libpng is disabled.
     */
    (void)sixel_loader_libpng_placeholder;
}

#undef SIXEL_LIBPNG_PLACEHOLDER_UNUSED

#endif  /* HAVE_LIBPNG */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
