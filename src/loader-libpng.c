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
#include "icc-apply.h"
#include "icc-parse.h"
#include "chunk.h"
#include "compat_stub.h"
#include "loader-common.h"
#include "frame.h"
#include "loader.h"
#include "loader-libpng.h"
#include "logger.h"

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
    char const *topics;
    char const *cursor;
    char const *token_end;
    size_t topic_length;
    size_t token_length;
    char const *topic;

    topics = NULL;
    cursor = NULL;
    token_end = NULL;
    topic_length = 0u;
    token_length = 0u;
    topic = "apng_decode";

    topic_length = strlen(topic);
    topics = sixel_compat_getenv("SIXEL_TRACE_TOPIC");
    if (topics == NULL || topics[0] == '\0') {
        return 0;
    }

    cursor = topics;
    while (*cursor != '\0') {
        while (*cursor != '\0' &&
               (*cursor == ' ' || *cursor == '\t' || *cursor == ',' ||
                *cursor == ':' || *cursor == ';')) {
            ++cursor;
        }
        if (*cursor == '\0') {
            break;
        }

        token_end = cursor;
        while (*token_end != '\0' &&
               *token_end != ' ' && *token_end != '\t' &&
               *token_end != ',' && *token_end != ':' &&
               *token_end != ';') {
            ++token_end;
        }

        token_length = (size_t)(token_end - cursor);
        if (token_length == topic_length &&
            strncmp(cursor, topic, token_length) == 0) {
            return 1;
        }

        cursor = token_end;
    }

    return 0;
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

static int
libpng_trns_keycolor_mode(void)
{
    char const *env;
    static int initialized = 0;
    static int mode = 2;
    /*
     * mode:
     *   0 -> disabled
     *   1 -> tRNS keycolor only
     *   2 -> tRNS keycolor + alpha-channel keycolor
     *        (default when env is unset)
     */

    if (initialized) {
        return mode;
    }
    initialized = 1;
    mode = 2;

    env = sixel_compat_getenv("SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR");
    if (env == NULL || env[0] == '\0') {
        return mode;
    }
    if (env[0] == '1' && env[1] == '\0') {
        mode = 2;
    } else if (env[0] == '0' && env[1] == '\0') {
        mode = 0;
    } else {
        mode = 0;
    }

    return mode;
}

static double
png_decode_srgb_unit(double value);

static double
png_encode_srgb_unit(double value);

static double
png_decode_source_unit(double value,
                       int transfer_mode,
                       double file_gamma);

static void
read_png(png_structp png_ptr,
         png_bytep data,
         png_size_t length)
{
    sixel_chunk_t *pchunk;

    pchunk = (sixel_chunk_t *)png_get_io_ptr(png_ptr);
    if (length > pchunk->size) {
        length = pchunk->size;
    }
    if (length > 0) {
        memcpy(data, pchunk->buffer, length);
        pchunk->buffer += length;
        pchunk->size -= length;
    }
}

static void
read_palette(png_structp png_ptr,
             png_infop info_ptr,
             unsigned char *palette,
             int ncolors,
             png_color *png_palette,
             png_color_16 *pbackground,
             int background_colorspace,
             double const bg_linear[3],
             int *transparent,
             unsigned char *zero_alpha_map,
             int *zero_alpha_count)
{
    png_bytep trans;
    int num_trans;
    int alpha;
    int key_index;
    int zero_count;
    double alpha_unit;
    double src_unit;
    double src_linear;
    double out_linear;
    int i;

    trans = NULL;
    num_trans = 0;
    alpha = 0xff;
    key_index = -1;
    zero_count = 0;
    alpha_unit = 0.0;
    src_unit = 0.0;
    src_linear = 0.0;
    out_linear = 0.0;
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
        if (pbackground && alpha < 0xff) {
            if (background_colorspace == SIXEL_COLORSPACE_LINEAR &&
                bg_linear != NULL) {
                alpha_unit = (double)alpha / 255.0;

                src_unit = (double)png_palette[i].red / 255.0;
                src_linear = png_decode_srgb_unit(src_unit);
                out_linear = src_linear * alpha_unit
                             + bg_linear[0] * (1.0 - alpha_unit);
                palette[i * 3 + 0] = (unsigned char)(
                    png_encode_srgb_unit(out_linear) * 255.0 + 0.5);

                src_unit = (double)png_palette[i].green / 255.0;
                src_linear = png_decode_srgb_unit(src_unit);
                out_linear = src_linear * alpha_unit
                             + bg_linear[1] * (1.0 - alpha_unit);
                palette[i * 3 + 1] = (unsigned char)(
                    png_encode_srgb_unit(out_linear) * 255.0 + 0.5);

                src_unit = (double)png_palette[i].blue / 255.0;
                src_linear = png_decode_srgb_unit(src_unit);
                out_linear = src_linear * alpha_unit
                             + bg_linear[2] * (1.0 - alpha_unit);
                palette[i * 3 + 2] = (unsigned char)(
                    png_encode_srgb_unit(out_linear) * 255.0 + 0.5);
            } else {
                palette[i * 3 + 0] = ((0xff - alpha) * pbackground->red
                                       + alpha * png_palette[i].red) >> 8;
                palette[i * 3 + 1] = ((0xff - alpha) * pbackground->green
                                       + alpha * png_palette[i].green) >> 8;
                palette[i * 3 + 2] = ((0xff - alpha) * pbackground->blue
                                       + alpha * png_palette[i].blue) >> 8;
            }
        } else {
            palette[i * 3 + 0] = png_palette[i].red;
            palette[i * 3 + 1] = png_palette[i].green;
            palette[i * 3 + 2] = png_palette[i].blue;
        }
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

#if !HAVE_LCMS2
static uint32_t
png_read_be32_nolcms(unsigned char const *p)
{
    return ((uint32_t)p[0] << 24u) |
           ((uint32_t)p[1] << 16u) |
           ((uint32_t)p[2] << 8u) |
           (uint32_t)p[3];
}

static int
png_detect_chunk_flags_raw_nolcms(unsigned char const *buffer,
                                  size_t size,
                                  int *has_iccp,
                                  int *has_srgb,
                                  int *has_chrm,
                                  int *has_gama)
{
    static unsigned char const signature[8] = {
        0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au
    };
    size_t offset;

    if (has_iccp == NULL || has_srgb == NULL ||
        has_chrm == NULL || has_gama == NULL) {
        return 0;
    }
    *has_iccp = 0;
    *has_srgb = 0;
    *has_chrm = 0;
    *has_gama = 0;

    if (buffer == NULL || size < sizeof(signature)) {
        return 0;
    }
    if (memcmp(buffer, signature, sizeof(signature)) != 0) {
        return 0;
    }

    offset = sizeof(signature);
    while (offset + 12u <= size) {
        uint32_t chunk_length;
        size_t chunk_total;
        unsigned char const *chunk_type;

        chunk_length = png_read_be32_nolcms(buffer + offset);
        chunk_total = 12u + (size_t)chunk_length;
        if (chunk_total > size - offset) {
            return 0;
        }

        chunk_type = buffer + offset + 4u;
        if (memcmp(chunk_type, "iCCP", 4u) == 0) {
            *has_iccp = 1;
        } else if (memcmp(chunk_type, "sRGB", 4u) == 0) {
            *has_srgb = 1;
        } else if (memcmp(chunk_type, "cHRM", 4u) == 0) {
            *has_chrm = 1;
        } else if (memcmp(chunk_type, "gAMA", 4u) == 0) {
            *has_gama = 1;
        } else if (memcmp(chunk_type, "IEND", 4u) == 0) {
            break;
        }

        offset += chunk_total;
    }
    return 1;
}

static int
png_invert_3x3(double const in[3][3], double out[3][3])
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
            source_to_srgb[row][col] = xyz_to_srgb[row][0] * source_to_xyz[0][col]
                                     + xyz_to_srgb[row][1] * source_to_xyz[1][col]
                                     + xyz_to_srgb[row][2] * source_to_xyz[2][col];
        }
    }
    return 1;
}

static void
png_apply_linear_matrix_triplet(double rgb[3],
                                double const source_to_srgb[3][3])
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
                                double const source_to_srgb[3][3])
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
png_apply_gama_to_srgb_u8(unsigned char *samples,
                          size_t sample_count,
                          double file_gamma)
{
    size_t index;
    double linear;
    double srgb;

    if (samples == NULL || file_gamma <= 0.0) {
        return;
    }
    for (index = 0u; index < sample_count; ++index) {
        linear = png_decode_source_unit((double)samples[index] / 255.0,
                                        SIXEL_PNG_TRANSFER_GAMA,
                                        file_gamma);
        srgb = png_encode_srgb_unit(linear);
        samples[index] = (unsigned char)(png_clamp_unit(srgb) * 255.0 + 0.5);
    }
}

static void
png_apply_gama_to_srgb_float32(float *samples,
                               size_t sample_count,
                               double file_gamma)
{
    size_t index;
    double linear;

    if (samples == NULL || file_gamma <= 0.0) {
        return;
    }
    for (index = 0u; index < sample_count; ++index) {
        linear = png_decode_source_unit((double)samples[index],
                                        SIXEL_PNG_TRANSFER_GAMA,
                                        file_gamma);
        samples[index] = (float)png_encode_srgb_unit(linear);
    }
}

static void
png_apply_gama_chrm_to_srgb_u8(unsigned char *samples,
                               size_t pixel_count,
                               double file_gamma,
                               double const source_to_srgb[3][3])
{
    size_t index;
    size_t offset;
    double linear[3];
    double srgb;
    int channel;

    if (samples == NULL || file_gamma <= 0.0 || source_to_srgb == NULL) {
        return;
    }
    for (index = 0u; index < pixel_count; ++index) {
        offset = index * 3u;
        for (channel = 0; channel < 3; ++channel) {
            linear[channel] = png_decode_source_unit(
                (double)samples[offset + (size_t)channel] / 255.0,
                SIXEL_PNG_TRANSFER_GAMA,
                file_gamma);
        }
        png_apply_linear_matrix_triplet(linear, source_to_srgb);
        for (channel = 0; channel < 3; ++channel) {
            srgb = png_encode_srgb_unit(linear[channel]);
            samples[offset + (size_t)channel] = (unsigned char)(
                png_clamp_unit(srgb) * 255.0 + 0.5);
        }
    }
}

static void
png_apply_gama_chrm_to_srgb_float32(float *samples,
                                    size_t pixel_count,
                                    double file_gamma,
                                    double const source_to_srgb[3][3])
{
    size_t index;
    size_t offset;
    double linear[3];
    double srgb;
    int channel;

    if (samples == NULL || file_gamma <= 0.0 || source_to_srgb == NULL) {
        return;
    }
    for (index = 0u; index < pixel_count; ++index) {
        offset = index * 3u;
        for (channel = 0; channel < 3; ++channel) {
            linear[channel] = png_decode_source_unit(
                (double)samples[offset + (size_t)channel],
                SIXEL_PNG_TRANSFER_GAMA,
                file_gamma);
        }
        png_apply_linear_matrix_triplet(linear, source_to_srgb);
        for (channel = 0; channel < 3; ++channel) {
            srgb = png_encode_srgb_unit(linear[channel]);
            samples[offset + (size_t)channel] = (float)srgb;
        }
    }
}
#endif  /* !HAVE_LCMS2 */

static SIXELSTATUS
png_roundtrip_target_to_linear(float *pixels,
                               size_t pixel_count,
                               int enable_cms)
{
    SIXELSTATUS status;
    size_t size_bytes;
    int target_colorspace;

    if (pixels == NULL || pixel_count == 0u || !enable_cms) {
        return SIXEL_OK;
    }

    target_colorspace = loader_cms_target_colorspace();
    if (target_colorspace == SIXEL_COLORSPACE_LINEAR) {
        return SIXEL_OK;
    }
    if (pixel_count > SIZE_MAX / (3u * sizeof(float))) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    size_bytes = pixel_count * 3u * sizeof(float);

    status = sixel_helper_convert_colorspace((unsigned char *)pixels,
                                             size_bytes,
                                             SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
                                             SIXEL_COLORSPACE_LINEAR,
                                             target_colorspace);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    return sixel_helper_convert_colorspace((unsigned char *)pixels,
                                           size_bytes,
                                           SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
                                           target_colorspace,
                                           SIXEL_COLORSPACE_LINEAR);
}

static SIXELSTATUS
png_roundtrip_background_to_linear(double bg_linear[3],
                                   int enable_cms)
{
    SIXELSTATUS status;
    float bg_pixel[3];
    int channel;

    if (bg_linear == NULL || !enable_cms) {
        return SIXEL_OK;
    }

    for (channel = 0; channel < 3; ++channel) {
        bg_pixel[channel] = (float)png_clamp_unit(bg_linear[channel]);
    }

    status = png_roundtrip_target_to_linear(bg_pixel, 1u, enable_cms);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    for (channel = 0; channel < 3; ++channel) {
        bg_linear[channel] = png_clamp_unit((double)bg_pixel[channel]);
    }

    return SIXEL_OK;
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

    if (bgcolor != NULL) {
        bg_unit[0] = (double)bgcolor[0] / 255.0;
        bg_unit[1] = (double)bgcolor[1] / 255.0;
        bg_unit[2] = (double)bgcolor[2] / 255.0;
        return;
    }

    if (png_get_bKGD(png_ptr, info_ptr, &png_background) != PNG_INFO_bKGD ||
        png_background == NULL) {
        return;
    }

    *background_from_file = 1;
    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        if (png_get_PLTE(png_ptr, info_ptr, &palette, &ncolors) != PNG_INFO_PLTE ||
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

    png_expand_background_sample_to_unit(png_background->red, bitdepth, &bg_unit[0]);
    png_expand_background_sample_to_unit(png_background->green, bitdepth, &bg_unit[1]);
    png_expand_background_sample_to_unit(png_background->blue, bitdepth, &bg_unit[2]);
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

#if HAVE_SETJMP && HAVE_LONGJMP
static jmp_buf jmpbuf;
#endif  /* HAVE_SETJMP && HAVE_LONGJMP */

/* libpng error handler */
static void
png_error_callback(png_structp png_ptr, png_const_charp error_message)
{
    (void) png_ptr;

    sixel_helper_set_additional_message(error_message);
#if HAVE_SETJMP && HAVE_LONGJMP
    longjmp(jmpbuf, 1);
#endif  /* HAVE_SETJMP && HAVE_LONGJMP */
}

#if HAVE_LCMS2
static uint32_t
png_read_be32_chunk(unsigned char const *p)
{
    return ((uint32_t)p[0] << 24u) |
           ((uint32_t)p[1] << 16u) |
           ((uint32_t)p[2] << 8u) |
           (uint32_t)p[3];
}

static int
png_detect_chunk_flags_raw(unsigned char const *buffer,
                           size_t size,
                           int *has_iccp,
                           int *has_srgb,
                           int *has_chrm,
                           int *has_gama)
{
    static unsigned char const signature[8] = {
        0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au
    };
    size_t offset;

    if (has_iccp == NULL || has_srgb == NULL ||
        has_chrm == NULL || has_gama == NULL) {
        return 0;
    }

    *has_iccp = 0;
    *has_srgb = 0;
    *has_chrm = 0;
    *has_gama = 0;

    if (buffer == NULL || size < 8u) {
        return 0;
    }
    if (memcmp(buffer, signature, sizeof(signature)) != 0) {
        return 0;
    }

    offset = 8u;
    while (offset + 12u <= size) {
        uint32_t chunk_length;
        size_t chunk_total;
        unsigned char const *chunk_type;

        chunk_length = png_read_be32_chunk(buffer + offset);
        chunk_total = 12u + (size_t)chunk_length;
        if (chunk_total > size - offset) {
            return 0;
        }

        chunk_type = buffer + offset + 4u;
        if (memcmp(chunk_type, "iCCP", 4u) == 0) {
            *has_iccp = 1;
        } else if (memcmp(chunk_type, "sRGB", 4u) == 0) {
            *has_srgb = 1;
        } else if (memcmp(chunk_type, "cHRM", 4u) == 0) {
            *has_chrm = 1;
        } else if (memcmp(chunk_type, "gAMA", 4u) == 0) {
            *has_gama = 1;
        } else if (memcmp(chunk_type, "IEND", 4u) == 0) {
            break;
        }

        offset += chunk_total;
    }

    return 1;
}

/*
 * Convert decoded PNG RGB pixels from an embedded ICC profile to sRGB.
 *
 * The iCCP chunk is optional. When it is absent or invalid, the loader keeps
 * the original decoded pixels so behavior remains backward compatible.
 */
static int
png_convert_profile_to_srgb(unsigned char *pixels,
                            int width,
                            int height,
                            int pixelformat,
                            sixel_cms_profile_t * src_profile)
{
    sixel_cms_profile_t * dst_profile;
    sixel_cms_transform_t * transform;
    sixel_cms_color_space_t src_colorspace;
    sixel_cms_pixel_format_t src_type;
    sixel_cms_pixel_format_t dst_type;
    size_t pixel_count;
    unsigned char *gray_in;
    unsigned char *rgb_out;
    unsigned char *rgb_in;
    int converted;
    size_t i;

    dst_profile = NULL;
    transform = NULL;
    src_colorspace = SIXEL_CMS_COLORSPACE_RGB;
    src_type = SIXEL_CMS_PIXELFORMAT_RGB_8;
    dst_type = SIXEL_CMS_PIXELFORMAT_RGB_8;
    pixel_count = 0;
    gray_in = NULL;
    rgb_out = NULL;
    rgb_in = NULL;
    converted = 0;
    i = 0u;

    if (pixels == NULL || width <= 0 || height <= 0 || src_profile == NULL) {
        return 0;
    }
    if (pixelformat != SIXEL_PIXELFORMAT_RGB888 &&
        pixelformat != SIXEL_PIXELFORMAT_G8 &&
        pixelformat != SIXEL_PIXELFORMAT_RGBFLOAT32) {
        return 0;
    }
    if (pixelformat == SIXEL_PIXELFORMAT_RGBFLOAT32) {
        src_type = SIXEL_CMS_PIXELFORMAT_RGB_F32;
        dst_type = SIXEL_CMS_PIXELFORMAT_RGB_F32;
    }
    src_colorspace = sixel_cms_get_color_space(src_profile);
    pixel_count = (size_t)width * (size_t)height;

    dst_profile = sixel_cms_create_srgb_profile();
    if (dst_profile == NULL) {
        return 0;
    }

    if (src_colorspace == SIXEL_CMS_COLORSPACE_GRAY && pixelformat == SIXEL_PIXELFORMAT_RGB888) {
        gray_in = (unsigned char *)malloc(pixel_count);
        rgb_out = (unsigned char *)malloc(pixel_count * 3u);
        if (gray_in == NULL || rgb_out == NULL) {
            goto cleanup;
        }
        for (i = 0u; i < pixel_count; ++i) {
            gray_in[i] = pixels[i * 3u];
        }
        transform = sixel_cms_create_transform(src_profile,
                                       SIXEL_CMS_PIXELFORMAT_GRAY_8,
                                       dst_profile,
                                       SIXEL_CMS_PIXELFORMAT_RGB_8,
                                       SIXEL_CMS_TRANSFORM_DEFAULT);
        if (transform == NULL) {
            goto cleanup;
        }
        sixel_cms_do_transform(transform, gray_in, rgb_out, pixel_count);
        memcpy(pixels, rgb_out, pixel_count * 3u);
        converted = 1;
        goto cleanup;
    }
    if (pixelformat == SIXEL_PIXELFORMAT_G8) {
        rgb_out = (unsigned char *)malloc(pixel_count * 3u);
        if (rgb_out == NULL) {
            goto cleanup;
        }
        if (src_colorspace == SIXEL_CMS_COLORSPACE_GRAY) {
            gray_in = (unsigned char *)malloc(pixel_count);
            if (gray_in == NULL) {
                goto cleanup;
            }
            memcpy(gray_in, pixels, pixel_count);
            transform = sixel_cms_create_transform(src_profile,
                                           SIXEL_CMS_PIXELFORMAT_GRAY_8,
                                           dst_profile,
                                           SIXEL_CMS_PIXELFORMAT_RGB_8,
                                           SIXEL_CMS_TRANSFORM_DEFAULT);
            if (transform == NULL) {
                goto cleanup;
            }
            sixel_cms_do_transform(transform, gray_in, rgb_out, pixel_count);
        } else {
            rgb_in = (unsigned char *)malloc(pixel_count * 3u);
            if (rgb_in == NULL) {
                goto cleanup;
            }
            for (i = 0u; i < pixel_count; ++i) {
                rgb_in[i * 3u + 0u] = pixels[i];
                rgb_in[i * 3u + 1u] = pixels[i];
                rgb_in[i * 3u + 2u] = pixels[i];
            }
            transform = sixel_cms_create_transform(src_profile,
                                           SIXEL_CMS_PIXELFORMAT_RGB_8,
                                           dst_profile,
                                           SIXEL_CMS_PIXELFORMAT_RGB_8,
                                           SIXEL_CMS_TRANSFORM_DEFAULT);
            if (transform == NULL) {
                goto cleanup;
            }
            sixel_cms_do_transform(transform, rgb_in, rgb_out, pixel_count);
        }
        for (i = 0u; i < pixel_count; ++i) {
            pixels[i] = rgb_out[i * 3u + 0u];
        }
        converted = 1;
        goto cleanup;
    }

    transform = sixel_cms_create_transform(src_profile,
                                   src_type,
                                   dst_profile,
                                   dst_type,
                                   SIXEL_CMS_TRANSFORM_DEFAULT);
    if (transform == NULL) {
        goto cleanup;
    }

    if (sixel_cms_do_transform(transform, pixels, pixels, pixel_count)) {
        converted = 1;
    }

cleanup:
    if (rgb_out != NULL) {
        free(rgb_out);
    }
    if (gray_in != NULL) {
        free(gray_in);
    }
    if (rgb_in != NULL) {
        free(rgb_in);
    }
    if (transform != NULL) {
        sixel_cms_delete_transform(transform);
    }
    if (dst_profile != NULL) {
        sixel_cms_close_profile(dst_profile);
    }
    return converted;
}

static int
png_build_rgb_profile_from_chunks(png_structp png_ptr,
                                  png_infop info_ptr,
                                  sixel_cms_profile_t **profile)
{
    sixel_cms_profile_t *built_profile;
    double file_gamma;
    double white_x;
    double white_y;
    double red_x;
    double red_y;
    double green_x;
    double green_y;
    double blue_x;
    double blue_y;
    int has_chrm;
    int has_gama;

    built_profile = NULL;
    file_gamma = 0.0;
    white_x = 0.0;
    white_y = 0.0;
    red_x = 0.0;
    red_y = 0.0;
    green_x = 0.0;
    green_y = 0.0;
    blue_x = 0.0;
    blue_y = 0.0;
    has_chrm = 0;
    has_gama = 0;

    if (profile == NULL) {
        return 0;
    }
    *profile = NULL;

    has_chrm = png_get_cHRM(png_ptr,
                            info_ptr,
                            &white_x,
                            &white_y,
                            &red_x,
                            &red_y,
                            &green_x,
                            &green_y,
                            &blue_x,
                            &blue_y) == PNG_INFO_cHRM;
    has_gama = png_get_gAMA(png_ptr, info_ptr, &file_gamma) == PNG_INFO_gAMA;

    if (!has_gama) {
        return 0;
    }
    if (!has_chrm) {
        white_x = 0.3127;
        white_y = 0.3290;
        red_x = 0.6400;
        red_y = 0.3300;
        green_x = 0.3000;
        green_y = 0.6000;
        blue_x = 0.1500;
        blue_y = 0.0600;
    }
    if (!has_gama) {
        file_gamma = 1.0 / 2.2;
    }
    if (file_gamma <= 0.0) {
        return 0;
    }

    built_profile = sixel_cms_create_rgb_profile_from_gamma_chrm(file_gamma,
                                                                 white_x,
                                                                 white_y,
                                                                 red_x,
                                                                 red_y,
                                                                 green_x,
                                                                 green_y,
                                                                 blue_x,
                                                                 blue_y);
    if (built_profile == NULL) {
        return 0;
    }

    *profile = built_profile;
    return 1;
}

static int
png_convert_embedded_icc_to_srgb(unsigned char *pixels,
                                 int width,
                                 int height,
                                 int pixelformat,
                                 png_bytep profile,
                                 png_uint_32 profile_length)
{
    sixel_cms_profile_t * src_profile;
    int converted;

    src_profile = NULL;
    converted = 0;
    if (profile == NULL || profile_length == 0u) {
        return 0;
    }

    src_profile = sixel_cms_open_profile_from_mem(profile, profile_length);
    if (src_profile == NULL) {
        return 0;
    }

    converted = png_convert_profile_to_srgb(pixels,
                                            width,
                                            height,
                                            pixelformat,
                                            src_profile);
    sixel_cms_close_profile(src_profile);
    return converted;
}
#endif

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

static SIXELSTATUS
load_png(unsigned char      /* out */ **result,
         unsigned char      /* in */  *buffer,
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
         sixel_allocator_t  /* in */  *allocator)
{
    SIXELSTATUS status;
    sixel_chunk_t read_chunk;
    png_uint_32 bitdepth;
    png_uint_32 png_status;
    png_structp png_ptr;
    png_infop info_ptr;
#ifdef HAVE_DIAGNOSTIC_CLOBBERED
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wclobbered"
#endif
    unsigned char **rows = NULL;
    png_color *png_palette = NULL;
    png_color_16 background;
#if HAVE_LCMS2
    png_charp icc_name;
    int icc_compression_type;
    png_bytep icc_profile;
    png_uint_32 icc_profile_length;
    sixel_cms_profile_t * chunk_profile;
    int has_embedded_icc;
    int has_embedded_icc_raw;
    int has_srgb_chunk;
    int has_chrm_chunk;
    int has_gama_chunk;
    int has_raw_chunk_flags;
    int has_srgb_chunk_raw;
    int has_chrm_chunk_raw;
    int has_gama_chunk_raw;
#endif
    int has_iccp_chunk_any;
    int has_chrm_chunk_any;
#if !HAVE_LCMS2
    int has_srgb_chunk_raw_nolcms;
    int has_chrm_chunk_raw_nolcms;
    int skip_iccp_conversion_nolcms;
    int has_embedded_icc_nolcms;
    int has_icc_profile_bytes_nolcms;
    png_charp icc_name_nolcms;
    int icc_compression_type_nolcms;
    png_bytep icc_profile_nolcms_bytes;
    png_uint_32 icc_profile_nolcms_bytes_length;
    int has_icc_profile_nolcms;
    sixel_icc_profile_t icc_profile_nolcms;
#endif
    double white_x;
    double white_y;
    double red_x;
    double red_y;
    double green_x;
    double green_y;
    double blue_x;
    double blue_y;
    png_uint_32 width;
    png_uint_32 height;
    png_uint_32 color_type;
    png_uint_32 read_bitdepth;
    png_uint_32 read_channels;
    png_size_t rowbytes;
    unsigned char *raw16_pixels = NULL;
    size_t raw16_size = 0u;
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
    int source_transfer_mode;
    int has_srgb_chunk_any;
    int has_gama_chunk_any;
    int srgb_intent;
    double gamma_chunk_value;
    double bg_unit[3];
    double bg_linear[3];
    double file_gamma_decode;
    int palette_force_pal8;
    int palette_keycolor_mode;
    int palette_keycolor_index;
    int palette_zero_alpha_count;
    int palette_remap_zero_alpha_indexes;
    unsigned char palette_zero_alpha_map[SIXEL_PALETTE_MAX];
    size_t pixel_count;
    size_t pixel_index;
    unsigned int palette_index;

#if HAVE_SETJMP && HAVE_LONGJMP
    if (setjmp(jmpbuf) != 0) {
        sixel_allocator_free(allocator, *result);
        *result = NULL;
        status = SIXEL_PNG_ERROR;
        goto cleanup;
    }
#endif  /* HAVE_SETJMP && HAVE_LONGJMP */

    status = SIXEL_FALSE;
    *result = NULL;
    png_ptr = NULL;
    info_ptr = NULL;
    read_bitdepth = 0u;
    read_channels = 0u;
    rowbytes = 0u;
    promote_to_float32 = 0;
    cms_converted = 0;
    color_type = 0u;
    has_tRNS_chunk = 0;
    has_alpha_chunk = 0;
    has_transparency = 0;
    indexed_trns_palette_path = 0;
    trns_keycolor_mode = 0;
    use_trns_keycolor = 0;
    background_colorspace = SIXEL_COLORSPACE_GAMMA;
    background_from_file = 0;
    source_transfer_mode = SIXEL_PNG_TRANSFER_SRGB;
    has_srgb_chunk_any = 0;
    has_gama_chunk_any = 0;
    has_iccp_chunk_any = 0;
    has_chrm_chunk_any = 0;
#if !HAVE_LCMS2
    has_srgb_chunk_raw_nolcms = 0;
    has_chrm_chunk_raw_nolcms = 0;
    skip_iccp_conversion_nolcms = 0;
    has_embedded_icc_nolcms = 0;
    has_icc_profile_bytes_nolcms = 0;
    icc_name_nolcms = NULL;
    icc_compression_type_nolcms = 0;
    icc_profile_nolcms_bytes = NULL;
    icc_profile_nolcms_bytes_length = 0u;
    has_icc_profile_nolcms = 0;
    memset(&icc_profile_nolcms, 0, sizeof(icc_profile_nolcms));
#endif
    srgb_intent = 0;
    gamma_chunk_value = 0.0;
    white_x = 0.0;
    white_y = 0.0;
    red_x = 0.0;
    red_y = 0.0;
    green_x = 0.0;
    green_y = 0.0;
    blue_x = 0.0;
    blue_y = 0.0;
    bg_unit[0] = 0.0;
    bg_unit[1] = 0.0;
    bg_unit[2] = 0.0;
    bg_linear[0] = 0.0;
    bg_linear[1] = 0.0;
    bg_linear[2] = 0.0;
    file_gamma_decode = 0.0;
    palette_force_pal8 = 0;
    palette_keycolor_mode = 0;
    palette_keycolor_index = -1;
    palette_zero_alpha_count = 0;
    palette_remap_zero_alpha_indexes = 0;
    memset(palette_zero_alpha_map, 0, sizeof(palette_zero_alpha_map));
    pixel_count = 0u;
    pixel_index = 0u;
    palette_index = 0u;
#if HAVE_LCMS2
    icc_name = NULL;
    icc_compression_type = 0;
    icc_profile = NULL;
    icc_profile_length = 0u;
    chunk_profile = NULL;
    has_embedded_icc = 0;
    has_embedded_icc_raw = 0;
    has_srgb_chunk = 0;
    has_chrm_chunk = 0;
    has_gama_chunk = 0;
    has_raw_chunk_flags = 0;
    has_srgb_chunk_raw = 0;
    has_chrm_chunk_raw = 0;
    has_gama_chunk_raw = 0;
#else
    (void)cms_applied;
#endif
    if (cms_applied != NULL) {
        *cms_applied = 0;
    }
    if (transparent != NULL) {
        *transparent = -1;
    }
    if (alpha_zero_is_transparent != NULL) {
        *alpha_zero_is_transparent = 0;
    }

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

    png_set_read_fn(png_ptr,(png_voidp)&read_chunk, read_png);
#if defined(PNG_SET_OPTION_SUPPORTED) && defined(PNG_SKIP_sRGB_CHECK_PROFILE)
    png_set_option(png_ptr, PNG_SKIP_sRGB_CHECK_PROFILE, PNG_OPTION_ON);
#endif
    png_read_info(png_ptr, info_ptr);
    has_srgb_chunk_any = png_get_sRGB(png_ptr, info_ptr, &srgb_intent) == PNG_INFO_sRGB;
    has_gama_chunk_any = png_get_gAMA(png_ptr, info_ptr, &gamma_chunk_value) == PNG_INFO_gAMA;
    has_chrm_chunk_any = png_get_cHRM(png_ptr,
                                      info_ptr,
                                      &white_x,
                                      &white_y,
                                      &red_x,
                                      &red_y,
                                      &green_x,
                                      &green_y,
                                      &blue_x,
                                      &blue_y) == PNG_INFO_cHRM;
    (void)srgb_intent;
#if HAVE_LCMS2
    if (png_get_iCCP(png_ptr,
                     info_ptr,
                     &icc_name,
                     &icc_compression_type,
                     &icc_profile,
                     &icc_profile_length) == PNG_INFO_iCCP) {
        (void)icc_name;
        (void)icc_compression_type;
        has_embedded_icc = 1;
    } else {
        icc_profile = NULL;
        icc_profile_length = 0u;
        has_embedded_icc = 0;
    }
    has_iccp_chunk_any = has_embedded_icc;
    has_srgb_chunk = has_srgb_chunk_any;
    has_chrm_chunk = has_chrm_chunk_any;
    has_gama_chunk = has_gama_chunk_any;
    has_raw_chunk_flags = png_detect_chunk_flags_raw(buffer,
                                                     size,
                                                     &has_embedded_icc_raw,
                                                     &has_srgb_chunk_raw,
                                                     &has_chrm_chunk_raw,
                                                     &has_gama_chunk_raw);
    if (!has_raw_chunk_flags) {
        has_srgb_chunk_raw = has_srgb_chunk;
        has_chrm_chunk_raw = has_chrm_chunk;
        has_gama_chunk_raw = has_gama_chunk;
    } else {
        has_iccp_chunk_any = has_embedded_icc_raw;
    }
    (void)has_embedded_icc_raw;
#else
    {
        int raw_srgb;
        int raw_chrm;
        int raw_gama;

        raw_srgb = 0;
        raw_chrm = 0;
        raw_gama = 0;
        if (png_get_iCCP(png_ptr,
                         info_ptr,
                         &icc_name_nolcms,
                         &icc_compression_type_nolcms,
                         &icc_profile_nolcms_bytes,
                         &icc_profile_nolcms_bytes_length) == PNG_INFO_iCCP) {
            has_embedded_icc_nolcms = 1;
            has_icc_profile_bytes_nolcms = 1;
        } else {
            has_embedded_icc_nolcms = 0;
            has_icc_profile_bytes_nolcms = 0;
            icc_profile_nolcms_bytes = NULL;
            icc_profile_nolcms_bytes_length = 0u;
        }
        has_srgb_chunk_raw_nolcms = has_srgb_chunk_any;
        has_chrm_chunk_raw_nolcms = has_chrm_chunk_any;
        if (png_detect_chunk_flags_raw_nolcms(buffer,
                                              size,
                                              &has_iccp_chunk_any,
                                              &raw_srgb,
                                              &raw_chrm,
                                              &raw_gama)) {
            has_srgb_chunk_raw_nolcms = raw_srgb;
            has_chrm_chunk_raw_nolcms = raw_chrm;
        } else {
            has_iccp_chunk_any = 0;
        }
        if (has_embedded_icc_nolcms) {
            has_iccp_chunk_any = 1;
        }
        /*
         * Keep parity with the lcms2 priority chain when iCCP conflicts
         * with explicit sRGB+cHRM chunks. In that case, prefer the PNG
         * chunk interpretation and skip ICC conversion in no-lcms mode.
         */
        skip_iccp_conversion_nolcms = has_srgb_chunk_raw_nolcms &&
                                      has_chrm_chunk_raw_nolcms;
        if (skip_iccp_conversion_nolcms) {
            has_icc_profile_bytes_nolcms = 0;
            icc_profile_nolcms_bytes = NULL;
            icc_profile_nolcms_bytes_length = 0u;
        }
        (void)icc_name_nolcms;
        (void)icc_compression_type_nolcms;
    }
#endif
#if !HAVE_LCMS2
    if (enable_cms &&
        has_iccp_chunk_any &&
        !(has_srgb_chunk_raw_nolcms && has_chrm_chunk_raw_nolcms)) {
        /*
         * Prefer libpng-provided profile bytes to share intent and profile
         * guard behavior with the common CMS path. Fall back to the raw iCCP
         * parser when libpng does not expose profile bytes.
         */
        if (!has_icc_profile_bytes_nolcms &&
            sixel_icc_parse_png_iccp(buffer, size, &icc_profile_nolcms)) {
            has_icc_profile_nolcms = 1;
        }
    }
#endif

    width = png_get_image_width(png_ptr, info_ptr);
    height = png_get_image_height(png_ptr, info_ptr);

    if (width > INT_MAX || height > INT_MAX) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto cleanup;
    }

    *psx = (int)width;
    *psy = (int)height;

    color_type = png_get_color_type(png_ptr, info_ptr);
    bitdepth = png_get_bit_depth(png_ptr, info_ptr);
    if (bitdepth == 16) {
#  if HAVE_DEBUG
        fprintf(stderr, "bitdepth: %u\n", (unsigned int)bitdepth);
        fprintf(stderr, "preserving 16bit for float32 conversion...\n");
#  endif
        promote_to_float32 = 1;
    }

    has_tRNS_chunk = png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS) != 0;
    has_alpha_chunk = (color_type & PNG_COLOR_MASK_ALPHA) != 0;
    indexed_trns_palette_path = has_tRNS_chunk &&
                                !has_alpha_chunk &&
                                color_type == PNG_COLOR_TYPE_PALETTE;
    has_transparency = (has_tRNS_chunk || has_alpha_chunk) &&
                       !indexed_trns_palette_path;
    trns_keycolor_mode = libpng_trns_keycolor_mode();
    use_trns_keycolor = trns_keycolor_mode != 0 &&
                        !enable_cms &&
                        bgcolor == NULL &&
                        ((has_tRNS_chunk &&
                          !has_alpha_chunk &&
                          (color_type == PNG_COLOR_TYPE_GRAY ||
                           color_type == PNG_COLOR_TYPE_RGB))
                         || (has_alpha_chunk && trns_keycolor_mode == 2));
    palette_keycolor_mode = trns_keycolor_mode != 0 &&
                            !enable_cms &&
                            bgcolor == NULL &&
                            indexed_trns_palette_path;
    background_colorspace = loader_background_colorspace();
    png_resolve_background_unit(png_ptr,
                                info_ptr,
                                color_type,
                                bitdepth,
                                bgcolor,
                                bg_unit,
                                &background_from_file);
    background.red = (png_uint_16)(bg_unit[0] * 255.0 + 0.5);
    background.green = (png_uint_16)(bg_unit[1] * 255.0 + 0.5);
    background.blue = (png_uint_16)(bg_unit[2] * 255.0 + 0.5);
    background.gray = (png_uint_16)((bg_unit[0] + bg_unit[1] + bg_unit[2])
                                    * 255.0 / 3.0 + 0.5);
    if (background_colorspace == SIXEL_COLORSPACE_LINEAR) {
        bg_linear[0] = bg_unit[0];
        bg_linear[1] = bg_unit[1];
        bg_linear[2] = bg_unit[2];
    } else {
        bg_linear[0] = png_decode_srgb_unit(bg_unit[0]);
        bg_linear[1] = png_decode_srgb_unit(bg_unit[1]);
        bg_linear[2] = png_decode_srgb_unit(bg_unit[2]);
    }

    if (use_trns_keycolor) {
        if (bitdepth == 16u) {
            png_set_strip_16(png_ptr);
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

        *pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
        if (alpha_zero_is_transparent != NULL) {
            *alpha_zero_is_transparent = 1;
        }
        status = SIXEL_OK;
        goto cleanup;
    }

    if (has_transparency) {
        unsigned char *rgb8_pixels;
        float *rgb16_pixels;
        float *dst_float_pixels;
        size_t pixel_count;
        size_t y;
        size_t x;
        size_t src_index;
        size_t dst_index;
        unsigned char const *src_row;
#if HAVE_LCMS2
        int profile_conversion_kind;
        sixel_cms_profile_t *active_chunk_profile;
#else
        int apply_source_chrm_matrix;
        double source_to_srgb_matrix[3][3];
        int background_profile_converted;
        double background_profile_unit[3];
#endif

        rgb8_pixels = NULL;
        rgb16_pixels = NULL;
        dst_float_pixels = NULL;
        pixel_count = 0u;
        y = 0u;
        x = 0u;
        src_index = 0u;
        dst_index = 0u;
        src_row = NULL;
#if HAVE_LCMS2
        profile_conversion_kind = 0;
        active_chunk_profile = NULL;
#else
        apply_source_chrm_matrix = 0;
        memset(source_to_srgb_matrix, 0, sizeof(source_to_srgb_matrix));
        background_profile_converted = 0;
        background_profile_unit[0] = 0.0;
        background_profile_unit[1] = 0.0;
        background_profile_unit[2] = 0.0;
#endif
        source_transfer_mode = SIXEL_PNG_TRANSFER_SRGB;
        file_gamma_decode = gamma_chunk_value;

        if ((size_t)width > SIZE_MAX / (size_t)height) {
            status = SIXEL_BAD_INTEGER_OVERFLOW;
            goto alpha_cleanup;
        }
        pixel_count = (size_t)width * (size_t)height;

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
            if (bitdepth == 16u) {
                png_set_add_alpha(png_ptr, 0xffffu, PNG_FILLER_AFTER);
            } else {
                png_set_add_alpha(png_ptr, 0xffu, PNG_FILLER_AFTER);
            }
        }

        png_read_update_info(png_ptr, info_ptr);
        read_bitdepth = png_get_bit_depth(png_ptr, info_ptr);
        read_channels = png_get_channels(png_ptr, info_ptr);
        rowbytes = png_get_rowbytes(png_ptr, info_ptr);
        if (read_channels != 4u) {
            sixel_helper_set_additional_message(
                "load_png: unsupported alpha PNG channel layout.");
            status = SIXEL_BAD_INPUT;
            goto alpha_cleanup;
        }
        if ((size_t)*psy > 0u && (size_t)rowbytes > SIZE_MAX / (size_t)*psy) {
            status = SIXEL_BAD_INTEGER_OVERFLOW;
            goto alpha_cleanup;
        }

        raw16_size = (size_t)rowbytes * (size_t)*psy;
        raw16_pixels = (unsigned char *)sixel_allocator_malloc(allocator,
                                                               raw16_size);
        if (raw16_pixels == NULL) {
            sixel_helper_set_additional_message(
                "load_png: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto alpha_cleanup;
        }

        rows = (unsigned char **)sixel_allocator_malloc(
            allocator,
            (size_t)*psy * sizeof(unsigned char *));
        if (rows == NULL) {
            sixel_helper_set_additional_message(
                "load_png: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto alpha_cleanup;
        }
        for (i = 0; i < *psy; ++i) {
            rows[i] = raw16_pixels + (size_t)i * (size_t)rowbytes;
        }
        png_read_image(png_ptr, rows);

#if HAVE_LCMS2
        if (enable_cms && has_embedded_icc && has_srgb_chunk_raw && has_chrm_chunk_raw) {
            /* keep original sRGB interpretation */
        } else if (enable_cms && has_embedded_icc) {
            profile_conversion_kind = 1;
        } else if (enable_cms && has_srgb_chunk_raw) {
            /* no profile conversion required */
        } else if (enable_cms && has_gama_chunk_raw &&
                   png_build_rgb_profile_from_chunks(png_ptr,
                                                     info_ptr,
                                                     &active_chunk_profile)) {
            profile_conversion_kind = 2;
        }
#endif

        if (read_bitdepth == 16u) {
            double alpha;
            double src_linear;
            double out_linear;
            unsigned int sample;
            int channel;

            if (pixel_count > SIZE_MAX / (3u * sizeof(float))) {
                status = SIXEL_BAD_INTEGER_OVERFLOW;
                goto alpha_cleanup;
            }
            rgb16_pixels = (float *)sixel_allocator_malloc(
                allocator,
                pixel_count * 3u * sizeof(float));
            if (rgb16_pixels == NULL) {
                sixel_helper_set_additional_message(
                    "load_png: sixel_allocator_malloc() failed.");
                status = SIXEL_BAD_ALLOCATION;
                goto alpha_cleanup;
            }

            for (y = 0u; y < (size_t)*psy; ++y) {
                src_row = raw16_pixels + y * (size_t)rowbytes;
                for (x = 0u; x < (size_t)*psx; ++x) {
                    src_index = x * 8u;
                    dst_index = (y * (size_t)*psx + x) * 3u;

                    sample = ((unsigned int)src_row[src_index + 0u] << 8u)
                           | (unsigned int)src_row[src_index + 1u];
                    rgb16_pixels[dst_index + 0u] = (float)sample / 65535.0f;
                    sample = ((unsigned int)src_row[src_index + 2u] << 8u)
                           | (unsigned int)src_row[src_index + 3u];
                    rgb16_pixels[dst_index + 1u] = (float)sample / 65535.0f;
                    sample = ((unsigned int)src_row[src_index + 4u] << 8u)
                           | (unsigned int)src_row[src_index + 5u];
                    rgb16_pixels[dst_index + 2u] = (float)sample / 65535.0f;
                }
            }

#if HAVE_LCMS2
            if (profile_conversion_kind == 1) {
                if (png_convert_embedded_icc_to_srgb((unsigned char *)rgb16_pixels,
                                                     *psx,
                                                     *psy,
                                                     SIXEL_PIXELFORMAT_RGBFLOAT32,
                                                     icc_profile,
                                                     icc_profile_length)) {
                    cms_converted = 1;
                }
            } else if (profile_conversion_kind == 2 && active_chunk_profile != NULL) {
                if (png_convert_profile_to_srgb((unsigned char *)rgb16_pixels,
                                                *psx,
                                                *psy,
                                                SIXEL_PIXELFORMAT_RGBFLOAT32,
                                                active_chunk_profile)) {
                    cms_converted = 1;
                }
            }
#endif
#if !HAVE_LCMS2
            if (enable_cms &&
                has_icc_profile_bytes_nolcms &&
                sixel_cms_convert_to_srgb_with_profile_bytes(
                    (unsigned char *)rgb16_pixels,
                    *psx,
                    *psy,
                    SIXEL_PIXELFORMAT_RGBFLOAT32,
                    icc_profile_nolcms_bytes,
                    (size_t)icc_profile_nolcms_bytes_length)) {
                cms_converted = 1;
            } else if (enable_cms &&
                       has_icc_profile_nolcms &&
                       sixel_icc_apply_rgb_float32(rgb16_pixels,
                                                   pixel_count,
                                                   &icc_profile_nolcms)) {
                cms_converted = 1;
            }
#endif

#if !HAVE_LCMS2
            apply_source_chrm_matrix = 0;
#endif
            if (enable_cms && (cms_converted || has_srgb_chunk_any)) {
                source_transfer_mode = SIXEL_PNG_TRANSFER_SRGB;
            } else if (enable_cms &&
                       !has_iccp_chunk_any &&
                       has_gama_chunk_any &&
                       file_gamma_decode > 0.0) {
                source_transfer_mode = SIXEL_PNG_TRANSFER_GAMA;
#if !HAVE_LCMS2
                if (has_chrm_chunk_any) {
                    apply_source_chrm_matrix =
                        png_build_chrm_to_srgb_matrix(white_x,
                                                      white_y,
                                                      red_x,
                                                      red_y,
                                                      green_x,
                                                      green_y,
                                                      blue_x,
                                                      blue_y,
                                                      source_to_srgb_matrix);
                }
#endif
            } else {
                source_transfer_mode = SIXEL_PNG_TRANSFER_SRGB;
            }

            for (dst_index = 0u; dst_index < pixel_count * 3u; ++dst_index) {
                rgb16_pixels[dst_index] = (float)png_decode_source_unit(
                    (double)rgb16_pixels[dst_index],
                    source_transfer_mode,
                    file_gamma_decode);
            }
#if !HAVE_LCMS2
            if (apply_source_chrm_matrix) {
                png_apply_linear_matrix_float32(rgb16_pixels,
                                                pixel_count,
                                                source_to_srgb_matrix);
            }
#endif
            status = png_roundtrip_target_to_linear(rgb16_pixels,
                                                    pixel_count,
                                                    enable_cms);
            if (SIXEL_FAILED(status)) {
                goto alpha_cleanup;
            }

#if !HAVE_LCMS2
            background_profile_converted = 0;
            if (background_from_file &&
                cms_converted &&
                background_colorspace != SIXEL_COLORSPACE_LINEAR) {
                if (has_icc_profile_bytes_nolcms) {
                    float bg_rgb_float[3];

                    bg_rgb_float[0] = (float)bg_unit[0];
                    bg_rgb_float[1] = (float)bg_unit[1];
                    bg_rgb_float[2] = (float)bg_unit[2];
                    if (sixel_cms_convert_to_srgb_with_profile_bytes(
                            (unsigned char *)bg_rgb_float,
                            1,
                            1,
                            SIXEL_PIXELFORMAT_RGBFLOAT32,
                            icc_profile_nolcms_bytes,
                            (size_t)icc_profile_nolcms_bytes_length)) {
                        background_profile_unit[0] = (double)bg_rgb_float[0];
                        background_profile_unit[1] = (double)bg_rgb_float[1];
                        background_profile_unit[2] = (double)bg_rgb_float[2];
                        background_profile_converted = 1;
                    }
                } else if (has_icc_profile_nolcms) {
                    background_profile_unit[0] = bg_unit[0];
                    background_profile_unit[1] = bg_unit[1];
                    background_profile_unit[2] = bg_unit[2];
                    if (sixel_icc_apply_rgb_triplet_unit(background_profile_unit,
                                                         &icc_profile_nolcms)) {
                        background_profile_converted = 1;
                    }
                }
            }
#endif
            for (channel = 0; channel < 3; ++channel) {
                if (background_colorspace == SIXEL_COLORSPACE_LINEAR) {
                    bg_linear[channel] = png_clamp_unit(bg_unit[channel]);
                } else if (background_from_file) {
#if HAVE_LCMS2
                    if (cms_converted && profile_conversion_kind != 0) {
                        float bg_rgb_float[3];
                        bg_rgb_float[0] = (float)bg_unit[0];
                        bg_rgb_float[1] = (float)bg_unit[1];
                        bg_rgb_float[2] = (float)bg_unit[2];
                        if (profile_conversion_kind == 1) {
                            (void)png_convert_embedded_icc_to_srgb(
                                (unsigned char *)bg_rgb_float,
                                1,
                                1,
                                SIXEL_PIXELFORMAT_RGBFLOAT32,
                                icc_profile,
                                icc_profile_length);
                        } else if (active_chunk_profile != NULL) {
                            (void)png_convert_profile_to_srgb(
                                (unsigned char *)bg_rgb_float,
                                1,
                                1,
                                SIXEL_PIXELFORMAT_RGBFLOAT32,
                                active_chunk_profile);
                        }
                        bg_linear[channel] = png_decode_srgb_unit(
                            (double)bg_rgb_float[channel]);
                        continue;
                    }
#endif
#if !HAVE_LCMS2
                    if (background_profile_converted) {
                        bg_linear[channel] = png_decode_srgb_unit(
                            background_profile_unit[channel]);
                        continue;
                    }
#endif
                    bg_linear[channel] = png_decode_source_unit(
                        bg_unit[channel],
                        source_transfer_mode,
                        file_gamma_decode);
                } else {
                    bg_linear[channel] = png_decode_srgb_unit(bg_unit[channel]);
                }
            }
 #if !HAVE_LCMS2
            if (background_colorspace != SIXEL_COLORSPACE_LINEAR &&
                background_from_file &&
                !background_profile_converted &&
                apply_source_chrm_matrix) {
                png_apply_linear_matrix_triplet(bg_linear, source_to_srgb_matrix);
            }
#endif
            status = png_roundtrip_background_to_linear(bg_linear, enable_cms);
            if (SIXEL_FAILED(status)) {
                goto alpha_cleanup;
            }

            dst_float_pixels = (float *)sixel_allocator_malloc(
                allocator,
                pixel_count * 3u * sizeof(float));
            if (dst_float_pixels == NULL) {
                sixel_helper_set_additional_message(
                    "load_png: sixel_allocator_malloc() failed.");
                status = SIXEL_BAD_ALLOCATION;
                goto alpha_cleanup;
            }

            for (y = 0u; y < (size_t)*psy; ++y) {
                src_row = raw16_pixels + y * (size_t)rowbytes;
                for (x = 0u; x < (size_t)*psx; ++x) {
                    src_index = x * 8u;
                    dst_index = (y * (size_t)*psx + x) * 3u;
                    sample = ((unsigned int)src_row[src_index + 6u] << 8u)
                           | (unsigned int)src_row[src_index + 7u];
                    alpha = (double)sample / 65535.0;

                    src_linear = (double)rgb16_pixels[dst_index + 0u];
                    out_linear = src_linear * alpha + bg_linear[0] * (1.0 - alpha);
                    dst_float_pixels[dst_index + 0u] = (float)out_linear;

                    src_linear = (double)rgb16_pixels[dst_index + 1u];
                    out_linear = src_linear * alpha + bg_linear[1] * (1.0 - alpha);
                    dst_float_pixels[dst_index + 1u] = (float)out_linear;

                    src_linear = (double)rgb16_pixels[dst_index + 2u];
                    out_linear = src_linear * alpha + bg_linear[2] * (1.0 - alpha);
                    dst_float_pixels[dst_index + 2u] = (float)out_linear;
                }
            }

            *result = (unsigned char *)dst_float_pixels;
            dst_float_pixels = NULL;
            *pixelformat = SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
            status = SIXEL_OK;
            goto alpha_cleanup;
        }

        if (read_bitdepth != 8u) {
            sixel_helper_set_additional_message(
                "load_png: unsupported alpha PNG bit depth.");
            status = SIXEL_BAD_INPUT;
            goto alpha_cleanup;
        }

        if (pixel_count > SIZE_MAX / 3u) {
            status = SIXEL_BAD_INTEGER_OVERFLOW;
            goto alpha_cleanup;
        }

        rgb8_pixels = (unsigned char *)sixel_allocator_malloc(allocator,
                                                              pixel_count * 3u);
        if (rgb8_pixels == NULL) {
            sixel_helper_set_additional_message(
                "load_png: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto alpha_cleanup;
        }

        for (y = 0u; y < (size_t)*psy; ++y) {
            src_row = raw16_pixels + y * (size_t)rowbytes;
            for (x = 0u; x < (size_t)*psx; ++x) {
                src_index = x * 4u;
                dst_index = (y * (size_t)*psx + x) * 3u;
                rgb8_pixels[dst_index + 0u] = src_row[src_index + 0u];
                rgb8_pixels[dst_index + 1u] = src_row[src_index + 1u];
                rgb8_pixels[dst_index + 2u] = src_row[src_index + 2u];
            }
        }

#if HAVE_LCMS2
        if (profile_conversion_kind == 1) {
            if (png_convert_embedded_icc_to_srgb(rgb8_pixels,
                                                 *psx,
                                                 *psy,
                                                 SIXEL_PIXELFORMAT_RGB888,
                                                 icc_profile,
                                                 icc_profile_length)) {
                cms_converted = 1;
            }
        } else if (profile_conversion_kind == 2 && active_chunk_profile != NULL) {
            if (png_convert_profile_to_srgb(rgb8_pixels,
                                            *psx,
                                            *psy,
                                            SIXEL_PIXELFORMAT_RGB888,
                                            active_chunk_profile)) {
                cms_converted = 1;
            }
        }
#endif
#if !HAVE_LCMS2
        if (enable_cms &&
            has_icc_profile_bytes_nolcms &&
            sixel_cms_convert_to_srgb_with_profile_bytes(
                rgb8_pixels,
                *psx,
                *psy,
                SIXEL_PIXELFORMAT_RGB888,
                icc_profile_nolcms_bytes,
                (size_t)icc_profile_nolcms_bytes_length)) {
            cms_converted = 1;
        } else if (enable_cms &&
                   has_icc_profile_nolcms &&
                   sixel_icc_apply_rgb_u8(rgb8_pixels,
                                          pixel_count,
                                          &icc_profile_nolcms)) {
            cms_converted = 1;
        }
#endif

#if !HAVE_LCMS2
        apply_source_chrm_matrix = 0;
#endif
        if (enable_cms && (cms_converted || has_srgb_chunk_any)) {
            source_transfer_mode = SIXEL_PNG_TRANSFER_SRGB;
        } else if (enable_cms &&
                   !has_iccp_chunk_any &&
                   has_gama_chunk_any &&
                   file_gamma_decode > 0.0) {
            source_transfer_mode = SIXEL_PNG_TRANSFER_GAMA;
#if !HAVE_LCMS2
            if (has_chrm_chunk_any) {
                apply_source_chrm_matrix =
                    png_build_chrm_to_srgb_matrix(white_x,
                                                  white_y,
                                                  red_x,
                                                  red_y,
                                                  green_x,
                                                  green_y,
                                                  blue_x,
                                                  blue_y,
                                                  source_to_srgb_matrix);
            }
#endif
        } else {
            source_transfer_mode = SIXEL_PNG_TRANSFER_SRGB;
        }

        if (pixel_count > SIZE_MAX / (3u * sizeof(float))) {
            status = SIXEL_BAD_INTEGER_OVERFLOW;
            goto alpha_cleanup;
        }
        rgb16_pixels = (float *)sixel_allocator_malloc(
            allocator,
            pixel_count * 3u * sizeof(float));
        if (rgb16_pixels == NULL) {
            sixel_helper_set_additional_message(
                "load_png: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto alpha_cleanup;
        }
        for (dst_index = 0u; dst_index < pixel_count * 3u; ++dst_index) {
            rgb16_pixels[dst_index] = (float)png_decode_source_unit(
                (double)rgb8_pixels[dst_index] / 255.0,
                source_transfer_mode,
                file_gamma_decode);
        }
#if !HAVE_LCMS2
        if (apply_source_chrm_matrix) {
            png_apply_linear_matrix_float32(rgb16_pixels,
                                            pixel_count,
                                            source_to_srgb_matrix);
        }
#endif
        status = png_roundtrip_target_to_linear(rgb16_pixels,
                                                pixel_count,
                                                enable_cms);
        if (SIXEL_FAILED(status)) {
            goto alpha_cleanup;
        }

#if !HAVE_LCMS2
        background_profile_converted = 0;
        if (background_from_file &&
            cms_converted &&
            background_colorspace != SIXEL_COLORSPACE_LINEAR) {
            if (has_icc_profile_bytes_nolcms) {
                float bg_rgb_float[3];

                bg_rgb_float[0] = (float)bg_unit[0];
                bg_rgb_float[1] = (float)bg_unit[1];
                bg_rgb_float[2] = (float)bg_unit[2];
                if (sixel_cms_convert_to_srgb_with_profile_bytes(
                        (unsigned char *)bg_rgb_float,
                        1,
                        1,
                        SIXEL_PIXELFORMAT_RGBFLOAT32,
                        icc_profile_nolcms_bytes,
                        (size_t)icc_profile_nolcms_bytes_length)) {
                    background_profile_unit[0] = (double)bg_rgb_float[0];
                    background_profile_unit[1] = (double)bg_rgb_float[1];
                    background_profile_unit[2] = (double)bg_rgb_float[2];
                    background_profile_converted = 1;
                }
            } else if (has_icc_profile_nolcms) {
                background_profile_unit[0] = bg_unit[0];
                background_profile_unit[1] = bg_unit[1];
                background_profile_unit[2] = bg_unit[2];
                if (sixel_icc_apply_rgb_triplet_unit(background_profile_unit,
                                                     &icc_profile_nolcms)) {
                    background_profile_converted = 1;
                }
            }
        }
#endif
        if (background_colorspace == SIXEL_COLORSPACE_LINEAR) {
            bg_linear[0] = png_clamp_unit(bg_unit[0]);
            bg_linear[1] = png_clamp_unit(bg_unit[1]);
            bg_linear[2] = png_clamp_unit(bg_unit[2]);
        } else if (background_from_file) {
#if HAVE_LCMS2
            if (cms_converted && profile_conversion_kind != 0) {
                unsigned char bg_rgb[3];
                bg_rgb[0] = (unsigned char)(bg_unit[0] * 255.0 + 0.5);
                bg_rgb[1] = (unsigned char)(bg_unit[1] * 255.0 + 0.5);
                bg_rgb[2] = (unsigned char)(bg_unit[2] * 255.0 + 0.5);
                if (profile_conversion_kind == 1) {
                    (void)png_convert_embedded_icc_to_srgb(bg_rgb,
                                                           1,
                                                           1,
                                                           SIXEL_PIXELFORMAT_RGB888,
                                                           icc_profile,
                                                           icc_profile_length);
                } else if (active_chunk_profile != NULL) {
                    (void)png_convert_profile_to_srgb(bg_rgb,
                                                      1,
                                                      1,
                                                      SIXEL_PIXELFORMAT_RGB888,
                                                      active_chunk_profile);
                }
                bg_linear[0] = png_decode_srgb_unit((double)bg_rgb[0] / 255.0);
                bg_linear[1] = png_decode_srgb_unit((double)bg_rgb[1] / 255.0);
                bg_linear[2] = png_decode_srgb_unit((double)bg_rgb[2] / 255.0);
            } else
#endif
            {
#if !HAVE_LCMS2
                if (background_profile_converted) {
                    bg_linear[0] = png_decode_srgb_unit(background_profile_unit[0]);
                    bg_linear[1] = png_decode_srgb_unit(background_profile_unit[1]);
                    bg_linear[2] = png_decode_srgb_unit(background_profile_unit[2]);
                } else
#endif
                {
                bg_linear[0] = png_decode_source_unit(bg_unit[0],
                                                      source_transfer_mode,
                                                      file_gamma_decode);
                bg_linear[1] = png_decode_source_unit(bg_unit[1],
                                                      source_transfer_mode,
                                                      file_gamma_decode);
                bg_linear[2] = png_decode_source_unit(bg_unit[2],
                                                      source_transfer_mode,
                                                      file_gamma_decode);
                }
            }
        } else {
            bg_linear[0] = png_decode_srgb_unit(bg_unit[0]);
            bg_linear[1] = png_decode_srgb_unit(bg_unit[1]);
            bg_linear[2] = png_decode_srgb_unit(bg_unit[2]);
        }
 #if !HAVE_LCMS2
        if (background_colorspace != SIXEL_COLORSPACE_LINEAR &&
            background_from_file &&
            !background_profile_converted &&
            apply_source_chrm_matrix) {
            png_apply_linear_matrix_triplet(bg_linear, source_to_srgb_matrix);
        }
#endif
        status = png_roundtrip_background_to_linear(bg_linear, enable_cms);
        if (SIXEL_FAILED(status)) {
            goto alpha_cleanup;
        }

        dst_float_pixels = (float *)sixel_allocator_malloc(
            allocator,
            pixel_count * 3u * sizeof(float));
        if (dst_float_pixels == NULL) {
            sixel_helper_set_additional_message(
                "load_png: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto alpha_cleanup;
        }

        for (y = 0u; y < (size_t)*psy; ++y) {
            src_row = raw16_pixels + y * (size_t)rowbytes;
            for (x = 0u; x < (size_t)*psx; ++x) {
                double alpha;
                double src_linear;
                double out_linear;

                src_index = x * 4u;
                dst_index = (y * (size_t)*psx + x) * 3u;

                alpha = (double)src_row[src_index + 3u] / 255.0;

                src_linear = (double)rgb16_pixels[dst_index + 0u];
                out_linear = src_linear * alpha + bg_linear[0] * (1.0 - alpha);
                dst_float_pixels[dst_index + 0u] = (float)out_linear;

                src_linear = (double)rgb16_pixels[dst_index + 1u];
                out_linear = src_linear * alpha + bg_linear[1] * (1.0 - alpha);
                dst_float_pixels[dst_index + 1u] = (float)out_linear;

                src_linear = (double)rgb16_pixels[dst_index + 2u];
                out_linear = src_linear * alpha + bg_linear[2] * (1.0 - alpha);
                dst_float_pixels[dst_index + 2u] = (float)out_linear;
            }
        }

        *result = (unsigned char *)dst_float_pixels;
        dst_float_pixels = NULL;
        *pixelformat = SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
        status = SIXEL_OK;

alpha_cleanup:
        if (rgb8_pixels != NULL) {
            sixel_allocator_free(allocator, rgb8_pixels);
        }
        if (rgb16_pixels != NULL) {
            sixel_allocator_free(allocator, rgb16_pixels);
        }
        if (dst_float_pixels != NULL) {
            sixel_allocator_free(allocator, dst_float_pixels);
        }
#if HAVE_LCMS2
        if (active_chunk_profile != NULL) {
            sixel_cms_close_profile(active_chunk_profile);
        }
#endif
        if (cms_applied != NULL) {
            *cms_applied = cms_converted;
        }
        goto cleanup;
    }

    switch (color_type) {
    case PNG_COLOR_TYPE_PALETTE:
#  if HAVE_DEBUG
        fprintf(stderr, "paletted PNG(PNG_COLOR_TYPE_PALETTE)\n");
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
        fprintf(stderr, "palette colors: %d\n", *pncolors);
        fprintf(stderr, "bitdepth: %u\n", (unsigned int)bitdepth);
#  endif
        if (ppalette == NULL || *pncolors > reqcolors) {
#  if HAVE_DEBUG
            fprintf(stderr, "detected more colors than required(>%d).\n",
                    reqcolors);
            fprintf(stderr, "expand to RGB format...\n");
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
                         &background,
                         background_colorspace,
                         bg_linear,
                         palette_keycolor_mode ? transparent : NULL,
                         palette_keycolor_mode ? palette_zero_alpha_map : NULL,
                         palette_keycolor_mode ? &palette_zero_alpha_count : NULL);

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
        fprintf(stderr, "grayscale PNG(PNG_COLOR_TYPE_GRAY)\n");
        fprintf(stderr, "bitdepth: %u\n", (unsigned int)bitdepth);
#  endif
        if (1 << bitdepth > reqcolors) {
#  if HAVE_DEBUG
            fprintf(stderr, "detected more colors than required(>%d).\n",
                    reqcolors);
            fprintf(stderr, "expand into RGB format...\n");
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
                    fprintf(stderr, "expand %u bpp to 8bpp format...\n",
                            (unsigned int)bitdepth);
#   endif
                    png_set_expand_gray_1_2_4_to_8(png_ptr);
                    *pixelformat = SIXEL_PIXELFORMAT_G8;
#  elif HAVE_DECL_PNG_SET_GRAY_1_2_4_TO_8
#   if HAVE_DEBUG
                    fprintf(stderr, "expand %u bpp to 8bpp format...\n",
                            (unsigned int)bitdepth);
#   endif
                    png_set_gray_1_2_4_to_8(png_ptr);
                    *pixelformat = SIXEL_PIXELFORMAT_G8;
#  else
#   if HAVE_DEBUG
                    fprintf(stderr, "expand into RGB format...\n");
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
                    fprintf(stderr, "expand into RGB format...\n");
#  endif
                    png_set_gray_to_rgb(png_ptr);
                    *pixelformat = SIXEL_PIXELFORMAT_RGB888;
                }
                break;
            default:
#  if HAVE_DEBUG
                fprintf(stderr, "expand into RGB format...\n");
#  endif
                png_set_gray_to_rgb(png_ptr);
                *pixelformat = SIXEL_PIXELFORMAT_RGB888;
                break;
            }
        }
        break;
    case PNG_COLOR_TYPE_GRAY_ALPHA:
#  if HAVE_DEBUG
        fprintf(stderr, "grayscale-alpha PNG(PNG_COLOR_TYPE_GRAY_ALPHA)\n");
        fprintf(stderr, "bitdepth: %u\n", (unsigned int)bitdepth);
        fprintf(stderr, "expand to RGB format...\n");
#  endif
        png_set_gray_to_rgb(png_ptr);
        *pixelformat = SIXEL_PIXELFORMAT_RGB888;
        break;
    case PNG_COLOR_TYPE_RGB_ALPHA:
#  if HAVE_DEBUG
        fprintf(stderr, "RGBA PNG(PNG_COLOR_TYPE_RGB_ALPHA)\n");
        fprintf(stderr, "bitdepth: %u\n", (unsigned int)bitdepth);
        fprintf(stderr, "expand to RGB format...\n");
#  endif
        *pixelformat = SIXEL_PIXELFORMAT_RGB888;
        break;
    case PNG_COLOR_TYPE_RGB:
#  if HAVE_DEBUG
        fprintf(stderr, "RGB PNG(PNG_COLOR_TYPE_RGB)\n");
        fprintf(stderr, "bitdepth: %u\n", (unsigned int)bitdepth);
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
                                   (size_t)(*psx * *psy * depth));
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
                rows[i] = *result + (depth * *psx * (int)bitdepth + 7) / 8 * i;
            }
            break;
        default:
            for (i = 0; i < *psy; ++i) {
                rows[i] = *result + depth * *psx * i;
            }
            break;
        }

        png_read_image(png_ptr, rows);

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
                    (*result)[pixel_index] = (unsigned char)palette_keycolor_index;
                }
            }
        }
    }

#if HAVE_LCMS2
    if (enable_cms && has_embedded_icc && has_srgb_chunk_raw && has_chrm_chunk_raw) {
        /* Priority 1: iCCP+sRGB+cHRM coexistence => no conversion. */
    } else if (enable_cms && has_embedded_icc) {
        /* Priority 2: iCCP only. */
        if (*pixelformat == SIXEL_PIXELFORMAT_PAL8 &&
            ppalette != NULL &&
            *ppalette != NULL &&
            pncolors != NULL &&
            *pncolors > 0) {
            sixel_cms_profile_t * embedded_profile;

            embedded_profile = sixel_cms_open_profile_from_mem(icc_profile, icc_profile_length);
            if (embedded_profile != NULL) {
                if (png_convert_profile_to_srgb(*ppalette,
                                                *pncolors,
                                                1,
                                                SIXEL_PIXELFORMAT_RGB888,
                                                embedded_profile)) {
                    cms_converted = 1;
                }
                sixel_cms_close_profile(embedded_profile);
            }
        } else {
            if (png_convert_embedded_icc_to_srgb(*result,
                                                 *psx,
                                                 *psy,
                                                 *pixelformat,
                                                 icc_profile,
                                                 icc_profile_length)) {
                cms_converted = 1;
            }
        }
    } else if (enable_cms && has_srgb_chunk_raw) {
        /* Priority 3: sRGB present => no conversion. */
    } else if (enable_cms &&
               has_gama_chunk_raw &&
               png_build_rgb_profile_from_chunks(png_ptr,
                                                 info_ptr,
                                                 &chunk_profile)) {
        /* Priority 4: gAMA(+/-cHRM). */
        if (*pixelformat == SIXEL_PIXELFORMAT_PAL8 &&
            ppalette != NULL &&
            *ppalette != NULL &&
            pncolors != NULL &&
            *pncolors > 0) {
            if (png_convert_profile_to_srgb(*ppalette,
                                            *pncolors,
                                            1,
                                            SIXEL_PIXELFORMAT_RGB888,
                                            chunk_profile)) {
                cms_converted = 1;
            }
        } else {
            if (png_convert_profile_to_srgb(*result,
                                            *psx,
                                            *psy,
                                            *pixelformat,
                                            chunk_profile)) {
                cms_converted = 1;
            }
        }
        sixel_cms_close_profile(chunk_profile);
    }
#endif
#if !HAVE_LCMS2
    if (enable_cms &&
        (has_icc_profile_bytes_nolcms || has_icc_profile_nolcms)) {
        switch (*pixelformat) {
        case SIXEL_PIXELFORMAT_PAL1:
        case SIXEL_PIXELFORMAT_PAL2:
        case SIXEL_PIXELFORMAT_PAL4:
        case SIXEL_PIXELFORMAT_PAL8:
            if (ppalette != NULL &&
                *ppalette != NULL &&
                pncolors != NULL &&
                *pncolors > 0 &&
                (size_t)*pncolors <= SIZE_MAX / 3u &&
                ((has_icc_profile_bytes_nolcms &&
                  sixel_cms_convert_to_srgb_with_profile_bytes(
                      *ppalette,
                      *pncolors,
                      1,
                      SIXEL_PIXELFORMAT_RGB888,
                      icc_profile_nolcms_bytes,
                      (size_t)icc_profile_nolcms_bytes_length)) ||
                 (has_icc_profile_nolcms &&
                  sixel_icc_apply_rgb_u8(*ppalette,
                                         (size_t)*pncolors,
                                         &icc_profile_nolcms)))) {
                cms_converted = 1;
            }
            break;
        case SIXEL_PIXELFORMAT_G8:
            if (*psx > 0 &&
                *psy > 0 &&
                (size_t)*psx <= SIZE_MAX / (size_t)*psy) {
                pixel_count = (size_t)*psx * (size_t)*psy;
                if ((has_icc_profile_bytes_nolcms &&
                     sixel_cms_convert_to_srgb_with_profile_bytes(
                         *result,
                         *psx,
                         *psy,
                         SIXEL_PIXELFORMAT_G8,
                         icc_profile_nolcms_bytes,
                         (size_t)icc_profile_nolcms_bytes_length)) ||
                    (has_icc_profile_nolcms &&
                     sixel_icc_apply_gray_u8(*result,
                                             pixel_count,
                                             &icc_profile_nolcms))) {
                    cms_converted = 1;
                }
            }
            break;
        case SIXEL_PIXELFORMAT_RGB888:
            if (*psx > 0 &&
                *psy > 0 &&
                (size_t)*psx <= SIZE_MAX / (size_t)*psy) {
                pixel_count = (size_t)*psx * (size_t)*psy;
                if (pixel_count <= SIZE_MAX / 3u) {
                    if ((has_icc_profile_bytes_nolcms &&
                         sixel_cms_convert_to_srgb_with_profile_bytes(
                             *result,
                             *psx,
                             *psy,
                             SIXEL_PIXELFORMAT_RGB888,
                             icc_profile_nolcms_bytes,
                             (size_t)icc_profile_nolcms_bytes_length)) ||
                        (has_icc_profile_nolcms &&
                         sixel_icc_apply_rgb_u8(*result,
                                                pixel_count,
                                                &icc_profile_nolcms))) {
                        cms_converted = 1;
                    }
                }
            }
            break;
        case SIXEL_PIXELFORMAT_RGBFLOAT32:
            if (*psx > 0 &&
                *psy > 0 &&
                (size_t)*psx <= SIZE_MAX / (size_t)*psy) {
                pixel_count = (size_t)*psx * (size_t)*psy;
                if (pixel_count <= SIZE_MAX / 3u) {
                    if ((has_icc_profile_bytes_nolcms &&
                         sixel_cms_convert_to_srgb_with_profile_bytes(
                             *result,
                             *psx,
                             *psy,
                             SIXEL_PIXELFORMAT_RGBFLOAT32,
                             icc_profile_nolcms_bytes,
                             (size_t)icc_profile_nolcms_bytes_length)) ||
                        (has_icc_profile_nolcms &&
                         sixel_icc_apply_rgb_float32((float *)*result,
                                                     pixel_count,
                                                     &icc_profile_nolcms))) {
                        cms_converted = 1;
                    }
                }
            }
            break;
        default:
            break;
        }
    }

    if (enable_cms &&
        !cms_converted &&
        !has_iccp_chunk_any &&
        has_gama_chunk_any &&
        !has_srgb_chunk_any &&
        gamma_chunk_value > 0.0) {
        int apply_chrm_matrix;
        double source_to_srgb_matrix[3][3];

        apply_chrm_matrix = 0;
        memset(source_to_srgb_matrix, 0, sizeof(source_to_srgb_matrix));
        if (has_chrm_chunk_any) {
            apply_chrm_matrix = png_build_chrm_to_srgb_matrix(white_x,
                                                              white_y,
                                                              red_x,
                                                              red_y,
                                                              green_x,
                                                              green_y,
                                                              blue_x,
                                                              blue_y,
                                                              source_to_srgb_matrix);
        }
        switch (*pixelformat) {
        case SIXEL_PIXELFORMAT_PAL1:
        case SIXEL_PIXELFORMAT_PAL2:
        case SIXEL_PIXELFORMAT_PAL4:
        case SIXEL_PIXELFORMAT_PAL8:
            if (ppalette != NULL &&
                *ppalette != NULL &&
                pncolors != NULL &&
                *pncolors > 0 &&
                (size_t)*pncolors <= SIZE_MAX / 3u) {
                if (apply_chrm_matrix) {
                    png_apply_gama_chrm_to_srgb_u8(*ppalette,
                                                   (size_t)*pncolors,
                                                   gamma_chunk_value,
                                                   source_to_srgb_matrix);
                } else {
                    png_apply_gama_to_srgb_u8(*ppalette,
                                              (size_t)*pncolors * 3u,
                                              gamma_chunk_value);
                }
            }
            break;
        case SIXEL_PIXELFORMAT_G8:
            if (*psx > 0 &&
                *psy > 0 &&
                (size_t)*psx <= SIZE_MAX / (size_t)*psy) {
                png_apply_gama_to_srgb_u8(*result,
                                          (size_t)*psx * (size_t)*psy,
                                          gamma_chunk_value);
            }
            break;
        case SIXEL_PIXELFORMAT_RGB888:
            if (*psx > 0 &&
                *psy > 0 &&
                (size_t)*psx <= SIZE_MAX / (size_t)*psy) {
                size_t pixel_count;

                pixel_count = (size_t)*psx * (size_t)*psy;
                if (pixel_count <= SIZE_MAX / 3u) {
                    if (apply_chrm_matrix) {
                        png_apply_gama_chrm_to_srgb_u8(*result,
                                                       pixel_count,
                                                       gamma_chunk_value,
                                                       source_to_srgb_matrix);
                    } else {
                        png_apply_gama_to_srgb_u8(*result,
                                                  pixel_count * 3u,
                                                  gamma_chunk_value);
                    }
                }
            }
            break;
        case SIXEL_PIXELFORMAT_RGBFLOAT32:
            if (*psx > 0 &&
                *psy > 0 &&
                (size_t)*psx <= SIZE_MAX / (size_t)*psy) {
                size_t pixel_count;

                pixel_count = (size_t)*psx * (size_t)*psy;
                if (pixel_count <= SIZE_MAX / 3u) {
                    if (apply_chrm_matrix) {
                        png_apply_gama_chrm_to_srgb_float32((float *)*result,
                                                            pixel_count,
                                                            gamma_chunk_value,
                                                            source_to_srgb_matrix);
                    } else {
                        png_apply_gama_to_srgb_float32((float *)*result,
                                                       pixel_count * 3u,
                                                       gamma_chunk_value);
                    }
                }
            }
            break;
        default:
            break;
        }
    }
#endif
    if (cms_applied != NULL) {
        *cms_applied = cms_converted;
    }

    status = SIXEL_OK;

cleanup:
    if (png_ptr != NULL) {
        png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)0);
    }

    if (rows != NULL) {
        sixel_allocator_free(allocator, rows);
    }
    if (raw16_pixels != NULL) {
        sixel_allocator_free(allocator, raw16_pixels);
    }
#if !HAVE_LCMS2
    if (has_icc_profile_nolcms) {
        sixel_icc_profile_destroy(&icc_profile_nolcms);
    }
#endif

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
    unsigned char *pixels;
    unsigned char *backup;
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
    char const *env_value;
    char *endptr;
    long parsed;

    status = SIXEL_OK;
    env_value = NULL;
    endptr = NULL;
    parsed = 0;

    *start_frame_no = INT_MIN;
    env_value = sixel_compat_getenv("SIXEL_LOADER_ANIMATION_START_FRAME_NO");
    if (env_value == NULL || env_value[0] == '\0') {
        goto end;
    }

    parsed = strtol(env_value, &endptr, 10);
    if (endptr == env_value || *endptr != '\0') {
        sixel_helper_set_additional_message(
            "SIXEL_LOADER_ANIMATION_START_FRAME_NO must be an integer.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    if (parsed < (long)INT_MIN || parsed > (long)INT_MAX) {
        sixel_helper_set_additional_message(
            "SIXEL_LOADER_ANIMATION_START_FRAME_NO is out of range.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    *start_frame_no = (int)parsed;

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

static SIXELSTATUS
decode_png_rgba(
    unsigned char      /* out */ **result,
    int                /* out */ *psx,
    int                /* out */ *psy,
    unsigned char      /* in */  *buffer,
    size_t             /* in */  size,
    sixel_allocator_t  /* in */  *allocator)
{
    SIXELSTATUS status;
    sixel_chunk_t read_chunk;
    png_structp png_ptr;
    png_infop info_ptr;
    png_uint_32 width;
    png_uint_32 height;
    png_uint_32 rowbytes;
    png_byte color_type;
    png_byte bitdepth;
    unsigned char **rows;
    int i;

    status = SIXEL_FALSE;
    png_ptr = NULL;
    info_ptr = NULL;
    rows = NULL;
    *result = NULL;
    *psx = 0;
    *psy = 0;

    png_ptr = png_create_read_struct(
        PNG_LIBPNG_VER_STRING, NULL, &png_error_callback, NULL);
    if (!png_ptr) {
        status = SIXEL_PNG_ERROR;
        goto end;
    }

#if HAVE_SETJMP
    if (setjmp(png_jmpbuf(png_ptr)) != 0) {
        status = SIXEL_PNG_ERROR;
        goto end;
    }
#endif

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        status = SIXEL_PNG_ERROR;
        goto end;
    }

    read_chunk.buffer = buffer;
    read_chunk.size = size;
    png_set_read_fn(png_ptr, (png_voidp)&read_chunk, read_png);
    png_read_info(png_ptr, info_ptr);

    width = png_get_image_width(png_ptr, info_ptr);
    height = png_get_image_height(png_ptr, info_ptr);
    if (width > INT_MAX || height > INT_MAX) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto end;
    }

    color_type = png_get_color_type(png_ptr, info_ptr);
    bitdepth = png_get_bit_depth(png_ptr, info_ptr);
    if (bitdepth == 16) {
        png_set_strip_16(png_ptr);
    }
    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png_ptr);
    }
    if (color_type == PNG_COLOR_TYPE_GRAY && bitdepth < 8) {
#if HAVE_DECL_PNG_SET_EXPAND_GRAY_1_2_4_TO_8
        png_set_expand_gray_1_2_4_to_8(png_ptr);
#elif HAVE_DECL_PNG_SET_GRAY_1_2_4_TO_8
        png_set_gray_1_2_4_to_8(png_ptr);
#endif
    }
    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
        png_set_tRNS_to_alpha(png_ptr);
    }
    if (color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(png_ptr);
    }
    if ((color_type & PNG_COLOR_MASK_ALPHA) == 0 &&
        !png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
        png_set_add_alpha(png_ptr, 0xff, PNG_FILLER_AFTER);
    }

    png_read_update_info(png_ptr, info_ptr);
    rowbytes = png_get_rowbytes(png_ptr, info_ptr);
    if (rowbytes != width * 4) {
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    *result = (unsigned char *)sixel_allocator_malloc(
        allocator,
        (size_t)height * (size_t)rowbytes);
    if (*result == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    rows = (unsigned char **)sixel_allocator_malloc(
        allocator,
        (size_t)height * sizeof(unsigned char *));
    if (rows == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    for (i = 0; i < (int)height; ++i) {
        rows[i] = *result + (size_t)i * rowbytes;
    }
    png_read_image(png_ptr, rows);

    *psx = (int)width;
    *psy = (int)height;
    status = SIXEL_OK;

end:
    if (SIXEL_FAILED(status)) {
        sixel_allocator_free(allocator, *result);
        *result = NULL;
    }
    sixel_allocator_free(allocator, rows);
    png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)0);
    return status;
}

static void
apng_clear_rect(
    sixel_apng_canvas_t const      *canvas,
    sixel_apng_frame_control_t     *control)
{
    int x;
    int y;
    int px;
    int py;
    unsigned char *dst;

    for (y = 0; y < (int)control->height; ++y) {
        py = (int)control->y_offset + y;
        if (py < 0 || py >= canvas->height) {
            continue;
        }
        for (x = 0; x < (int)control->width; ++x) {
            px = (int)control->x_offset + x;
            if (px < 0 || px >= canvas->width) {
                continue;
            }
            dst = canvas->pixels + ((py * canvas->width + px) * 4);
            dst[0] = 0;
            dst[1] = 0;
            dst[2] = 0;
            dst[3] = 0;
        }
    }
}

static void
apng_blend_rect(
    sixel_apng_canvas_t const      *canvas,
    sixel_apng_frame_control_t     *control,
    unsigned char const            *src)
{
    int x;
    int y;
    int px;
    int py;
    int idx;
    unsigned int sa;
    unsigned int da;
    unsigned int oa;
    unsigned char const *sp;
    unsigned char *dp;

    for (y = 0; y < (int)control->height; ++y) {
        py = (int)control->y_offset + y;
        if (py < 0 || py >= canvas->height) {
            continue;
        }
        for (x = 0; x < (int)control->width; ++x) {
            px = (int)control->x_offset + x;
            if (px < 0 || px >= canvas->width) {
                continue;
            }
            idx = y * (int)control->width + x;
            sp = src + idx * 4;
            dp = canvas->pixels + ((py * canvas->width + px) * 4);

            if (control->blend_op == 0) {
                dp[0] = sp[0];
                dp[1] = sp[1];
                dp[2] = sp[2];
                dp[3] = sp[3];
                continue;
            }

            sa = sp[3];
            da = dp[3];
            oa = sa + ((da * (255 - sa)) / 255);
            if (oa == 0) {
                dp[0] = 0;
                dp[1] = 0;
                dp[2] = 0;
                dp[3] = 0;
                continue;
            }
            dp[0] = (unsigned char)((sp[0] * sa + dp[0] * da * (255 - sa) / 255)
                                    / oa);
            dp[1] = (unsigned char)((sp[1] * sa + dp[1] * da * (255 - sa) / 255)
                                    / oa);
            dp[2] = (unsigned char)((sp[2] * sa + dp[2] * da * (255 - sa) / 255)
                                    / oa);
            dp[3] = (unsigned char)oa;
        }
    }
}

static int
apng_replay_cache_measure_frame_bytes(sixel_frame_t const *frame,
                                      size_t *frame_bytes)
{
    int depth_result;
    size_t depth;
    size_t pixel_total;
    size_t pixel_bytes;
    size_t palette_bytes;
    size_t mask_bytes;
    size_t total;

    depth_result = 0;
    depth = 0u;
    pixel_total = 0u;
    pixel_bytes = 0u;
    palette_bytes = 0u;
    mask_bytes = 0u;
    total = 0u;
    if (frame == NULL || frame_bytes == NULL) {
        return 0;
    }

    *frame_bytes = 0u;
    if (frame->width <= 0 || frame->height <= 0) {
        return 0;
    }
    if ((size_t)frame->width > SIZE_MAX / (size_t)frame->height) {
        return 0;
    }

    depth_result = sixel_helper_compute_depth(frame->pixelformat);
    if (depth_result <= 0) {
        return 0;
    }
    depth = (size_t)depth_result;
    pixel_total = (size_t)frame->width * (size_t)frame->height;
    if (pixel_total > SIZE_MAX / depth) {
        return 0;
    }
    pixel_bytes = pixel_total * depth;

    if (frame->palette != NULL && frame->ncolors > 0) {
        if (frame->ncolors > SIXEL_PALETTE_MAX ||
            (size_t)frame->ncolors > SIZE_MAX / 3u) {
            return 0;
        }
        palette_bytes = (size_t)frame->ncolors * 3u;
    }
    if (frame->transparent_mask != NULL && frame->transparent_mask_size > 0u) {
        mask_bytes = frame->transparent_mask_size;
    }

    total = pixel_bytes;
    if (total > SIZE_MAX - palette_bytes) {
        return 0;
    }
    total += palette_bytes;
    if (total > SIZE_MAX - mask_bytes) {
        return 0;
    }
    total += mask_bytes;
    *frame_bytes = total;
    return 1;
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
    size_t frame_bytes;

    frame_bytes = 0u;
    if (cache == NULL || frame == NULL || cache->enabled == 0) {
        return 0;
    }
    if (cache->frames == NULL ||
        cache->frame_capacity == 0u ||
        cache->frame_count >= cache->frame_capacity) {
        apng_replay_cache_reset(cache, allocator);
        return 0;
    }
    if (!apng_replay_cache_measure_frame_bytes(frame, &frame_bytes)) {
        apng_replay_cache_reset(cache, allocator);
        return 0;
    }
    if (frame_bytes > APNG_FRAME_CACHE_MAX_BYTES_DEFAULT ||
        cache->cached_bytes >
        APNG_FRAME_CACHE_MAX_BYTES_DEFAULT - frame_bytes) {
        apng_replay_cache_reset(cache, allocator);
        return 0;
    }

    frame->handoff_shareable = 1;
    sixel_frame_ref(frame);
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
    int                            alpha_zero_is_transparent,
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
    unsigned char *emitted;
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
    if (state->chunk_size > SIZE_MAX - 8 - 25) {
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
                             allocator);

    if (width != (int)control->width || height != (int)control->height) {
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    canvas_bytes = (size_t)canvas->width * (size_t)canvas->height * 4u;
    if (control->dispose_op == 2) {
        memcpy(canvas->backup, canvas->pixels, canvas_bytes);
    }
    apng_blend_rect(canvas, control, subframe);

    if (!emit_callback && !cache_frame) {
        status = SIXEL_OK;
        goto dispose;
    }

    emitted = (unsigned char *)sixel_allocator_malloc(allocator, canvas_bytes);
    if (emitted == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    memcpy(emitted, canvas->pixels, canvas_bytes);

    status = sixel_frame_new(&frame, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    frame->width = canvas->width;
    frame->height = canvas->height;
    frame->palette = NULL;
    frame->ncolors = 0;
    frame->pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
    frame->transparent = (-1);
    frame->alpha_zero_is_transparent =
        alpha_zero_is_transparent != 0 ? 1 : 0;
    sixel_frame_set_delay(frame, (int)control->delay_cs);
    sixel_frame_set_frame_no(frame, frame_no);
    sixel_frame_set_loop_count(frame, loop_no);
    sixel_frame_set_multiframe(frame, multiframe);
    sixel_frame_set_pixels(frame, emitted);
    emitted = NULL;

    if (exif_orientation >= 2 && exif_orientation <= 8) {
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
    sixel_frame_unref(frame);

    return status;
}

static SIXELSTATUS
load_apng_frames(
    sixel_chunk_t const       *pchunk,
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
    int alpha_zero_is_transparent;
    int color_type;
    int has_alpha_chunk;
    int has_trns_chunk;
    int trns_keycolor_mode;
    sixel_apng_canvas_t canvas;
    size_t canvas_bytes;
    png_uint_32 sequence_no;
    png_uint_32 fd_sequence;
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
    alpha_zero_is_transparent = 0;
    color_type = (-1);
    has_alpha_chunk = 0;
    has_trns_chunk = 0;
    trns_keycolor_mode = libpng_trns_keycolor_mode();
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
    if (pchunk == NULL || pchunk->buffer == NULL ||
        pchunk->size < sizeof(png_signature)) {
        status = SIXEL_FALSE;
        goto end;
    }
    if (memcmp(pchunk->buffer, png_signature, sizeof(png_signature)) != 0) {
        status = SIXEL_FALSE;
        goto end;
    }

    apng_decode_trace_message(
        "load_apng_frames: input_size=%lu static=%d loop_control=%d "
        "start_frame_no=%d",
        (unsigned long)pchunk->size,
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
        p = pchunk->buffer + 8;
        remain = pchunk->size - 8;
        seen_actl = 0;
        has_frame = 0;
        source_frame_no = 0;
        frames_in_loop = 0;
        seen_fctl = 0;
        seen_idat = 0;
        color_type = (-1);
        has_alpha_chunk = 0;
        has_trns_chunk = 0;
        alpha_zero_is_transparent = 0;

        if (loop_no > 0 && canvas_bytes > 0) {
            memset(canvas.pixels, 0, canvas_bytes);
            memset(canvas.backup, 0, canvas_bytes);
        }

    while (remain >= 12) {
        if (sixel_loader_callback_is_canceled(context)) {
            status = SIXEL_INTERRUPTED;
            goto end;
        }

        png_uint_32 length;

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
            color_type = (int)p[17];
            has_alpha_chunk = (color_type & PNG_COLOR_MASK_ALPHA) != 0 ? 1 : 0;
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
                canvas_bytes = (size_t)canvas.width * (size_t)canvas.height * 4;
                canvas.pixels = (unsigned char *)sixel_allocator_malloc(
                    pchunk->allocator,
                    canvas_bytes);
                canvas.backup = (unsigned char *)sixel_allocator_malloc(
                    pchunk->allocator,
                    canvas_bytes);
                if (canvas.pixels == NULL || canvas.backup == NULL) {
                    status = SIXEL_BAD_ALLOCATION;
                    goto end;
                }
                memset(canvas.pixels, 0, canvas_bytes);
                memset(canvas.backup, 0, canvas_bytes);
            }
            alpha_zero_is_transparent =
                trns_keycolor_mode != 0 &&
                bgcolor == NULL &&
                !enable_cms &&
                ((has_trns_chunk &&
                  !has_alpha_chunk)
                 || (has_alpha_chunk && trns_keycolor_mode == 2));
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
                                                pchunk->allocator,
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
                                         alpha_zero_is_transparent,
                                         exif_orientation,
                                         reqcolors,
                                         fuse_palette,
                                         &canvas,
                                         &replay_cache,
                                         fn_load,
                                         context,
                                         pchunk->allocator);
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
                              pchunk->allocator)) {
                status = SIXEL_BAD_ALLOCATION;
                goto end;
            }
        } else if (memcmp(p + 4, "IDAT", 4) == 0) {
            if (seen_actl && !has_frame && (seen_fctl || seen_idat)) {
                sixel_helper_set_additional_message(
                    "APNG parse error: unexpected IDAT ordering");
                status = SIXEL_BAD_INPUT;
                goto end;
            }
            if (!append_chunk(&state,
                              "IDAT",
                              p + 8,
                              length,
                              pchunk->allocator)) {
                status = SIXEL_BAD_ALLOCATION;
                goto end;
            }
            if (seen_actl && !seen_fctl && !seen_idat) {
                control.width = (png_uint_32)canvas.width;
                control.height = (png_uint_32)canvas.height;
                control.x_offset = 0;
                control.y_offset = 0;
                control.delay_cs = 0;
                control.dispose_op = 0;
                control.blend_op = 0;
            }
            seen_idat = 1;
            if (seen_actl != 0) {
                apng_decode_trace_message(
                    "IDAT accepted: has_frame=%d seen_fctl=%d seen_idat=%d",
                    has_frame,
                    seen_fctl,
                    seen_idat);
            }
            has_frame = 1;
        } else if (memcmp(p + 4, "IEND", 4) == 0) {
            break;
        } else if (memcmp(p + 4, "tRNS", 4) == 0) {
            has_trns_chunk = 1;
            alpha_zero_is_transparent =
                trns_keycolor_mode != 0 &&
                bgcolor == NULL &&
                !enable_cms &&
                ((has_trns_chunk &&
                  !has_alpha_chunk)
                 || (has_alpha_chunk && trns_keycolor_mode == 2));
        } else if (memcmp(p + 4, "acTL", 4) != 0 &&
                   memcmp(p + 4, "fcTL", 4) != 0 &&
                   memcmp(p + 4, "fdAT", 4) != 0 &&
                   memcmp(p + 4, "IHDR", 4) != 0 &&
                   memcmp(p + 4, "IEND", 4) != 0 &&
                   state.chunk_size == 0) {
            if (!append_shared_chunk(&state,
                                     p,
                                     (size_t)length + 12,
                                     pchunk->allocator)) {
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
                                  alpha_zero_is_transparent,
                                  exif_orientation,
                                  reqcolors,
                                  fuse_palette,
                                  &canvas,
                                  &replay_cache,
                                 fn_load,
                                 context,
                                 pchunk->allocator);
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

    sixel_allocator_free(pchunk->allocator, state.shared_chunks);
    sixel_allocator_free(pchunk->allocator, (void *)state.chunk_base);
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
        "loop_no=%d saw_animation=%d alpha_zero_is_transparent=%d",
        status,
        emit_frame_no,
        source_frame_no,
        loop_no,
        saw_animation,
        alpha_zero_is_transparent);
    sixel_allocator_free(pchunk->allocator, canvas.pixels);
    sixel_allocator_free(pchunk->allocator, canvas.backup);
    sixel_allocator_free(pchunk->allocator, state.shared_chunks);
    sixel_allocator_free(pchunk->allocator, (void *)state.chunk_base);
    apng_replay_cache_reset(&replay_cache, pchunk->allocator);
    if (!saw_animation && status == SIXEL_FALSE) {
        return SIXEL_FALSE;
    }
    return status;
}
#ifdef HAVE_DIAGNOSTIC_CLOBBERED
# pragma GCC diagnostic pop
#endif

/*
 * Dedicated libpng loader for precise PNG decoding.
 *
 *    +-----------+     +------------------+     +--------------------+
 *    | PNG chunk | --> | libpng decode    | --> | sixel frame emit   |
 *    +-----------+     +------------------+     +--------------------+
 */
static SIXELSTATUS
load_with_libpng(
    sixel_chunk_t const       /* in */     *pchunk,
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

    status = SIXEL_FALSE;
    frame = NULL;
    pixels = NULL;
    enable_cms = enable_cms_override != 0 ? 1 : 0;
    cms_applied = 0;
    alpha_zero_is_transparent = 0;
    cms_target_pixelformat = SIXEL_PIXELFORMAT_RGB888;
    enable_orientation = enable_orientation_override != 0 ? 1 : 0;
    exif_orientation = 1;

    (void)fstatic;
    (void)loop_control;

    if (enable_orientation) {
        (void)libpng_parse_exif_orientation(pchunk->buffer,
                                            pchunk->size,
                                            &exif_orientation);
    }

    status = load_apng_frames(pchunk,
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
    /*
     * Only fall back to single-frame PNG decoding when APNG chunks are
     * absent. If APNG parsing started and returned an error, preserve the
     * error instead of masking it with the non-APNG fallback path.
     */
    if (status != SIXEL_FALSE) {
        goto end;
    }

    status = sixel_frame_new(&frame, pchunk->allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = load_png(&pixels,
                      pchunk->buffer,
                      pchunk->size,
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
                      pchunk->allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    sixel_frame_set_colorspace(frame,
                               png_colorspace_from_pixelformat(
                                   frame->pixelformat));
    frame->alpha_zero_is_transparent = alpha_zero_is_transparent != 0 ? 1 : 0;
    sixel_frame_set_pixels(frame, pixels);
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

    self = NULL;
    bgcolor = NULL;
    if (component == NULL || chunk == NULL || fn_load == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    self = (sixel_loader_libpng_component_t *)component;
    if (self->has_bgcolor) {
        bgcolor = self->bgcolor;
    }

    return load_with_libpng(chunk,
                            self->fstatic,
                            self->fuse_palette,
                            self->reqcolors,
                            bgcolor,
                            self->loop_control,
                            self->has_start_frame_no,
                            self->start_frame_no,
                            self->enable_cms,
                            self->enable_orientation,
                            fn_load,
                            context);
}

static char const *
sixel_loader_libpng_name(sixel_loader_component_t const *component)
{
    (void)component;
    return "libpng";
}

static sixel_loader_component_vtbl_t const g_sixel_loader_libpng_vtbl = {
    sixel_loader_libpng_ref,
    sixel_loader_libpng_unref,
    sixel_loader_libpng_setopt,
    sixel_loader_libpng_load,
    sixel_loader_libpng_name
};

SIXELSTATUS
sixel_loader_libpng_new(sixel_allocator_t *allocator,
                        sixel_loader_component_t **ppcomponent)
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

int
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
