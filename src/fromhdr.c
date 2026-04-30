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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <limits.h>
#include <stdlib.h>

#if HAVE_CTYPE_H
# include <ctype.h>
#endif
#if HAVE_FLOAT_H
# include <float.h>
#endif
#if HAVE_MATH_H
# include <math.h>
#endif
#if HAVE_STDIO_H
# include <stdio.h>
#endif
#if HAVE_STDINT_H
# include <stdint.h>
#endif
#if HAVE_STRING_H
# include <string.h>
#endif

#include <sixel.h>
#include <6cells.h>

#include "cms.h"
#include "compat_stub.h"
#include "fromhdr.h"
#include "loader-common.h"

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

static SIXELSTATUS
sixel_builtin_decode_hdr_float32_with_hint(
    sixel_chunk_t const *chunk,
    unsigned char **ppixels,
    int *pwidth,
    int *pheight,
    int *ppixelformat,
    int *pcolorspace,
    sixel_builtin_hdr_profile_hint_t *out_hint,
    SIXELSTATUS *out_hint_status);

static SIXELSTATUS
sixel_builtin_parse_hdr_profile_hint(
    sixel_chunk_t const *chunk,
    sixel_builtin_hdr_profile_hint_t *out_hint);

static void
sixel_builtin_hdr_apply_postprocess(
    unsigned char *pixels,
    int width,
    int height,
    int pixelformat,
    sixel_builtin_hdr_profile_hint_t const *hint,
    SIXELSTATUS hint_status,
    int enable_cms);

static SIXELSTATUS
sixel_builtin_hdr_assign_decoded_frame(
    sixel_chunk_t const *chunk,
    sixel_frame_t *frame,
    unsigned char *pixels,
    int width,
    int height,
    int hdr_pixelformat,
    int hdr_colorspace);

static int
sixel_builtin_hdr_hint_has_decodable_stream(
    SIXELSTATUS hint_status,
    sixel_builtin_hdr_profile_hint_t const *hint);

static int
sixel_builtin_hdr_parse_primaries_line(char const *line,
                                       sixel_builtin_hdr_profile_hint_t *hint);

static int
sixel_builtin_hdr_parse_format_line(char const *line, int *format_kind);

static int
sixel_builtin_hdr_ascii_case_equal(char const *left, char const *right)
{
    size_t index;
    unsigned char left_ch;
    unsigned char right_ch;

    index = 0u;
    if (left == NULL || right == NULL) {
        return 0;
    }

    while (left[index] != '\0' && right[index] != '\0') {
        left_ch = (unsigned char)left[index];
        right_ch = (unsigned char)right[index];
        if (left_ch >= (unsigned char)'A' &&
            left_ch <= (unsigned char)'Z') {
            left_ch = (unsigned char)(left_ch - (unsigned char)'A' +
                                      (unsigned char)'a');
        }
        if (right_ch >= (unsigned char)'A' &&
            right_ch <= (unsigned char)'Z') {
            right_ch = (unsigned char)(right_ch - (unsigned char)'A' +
                                       (unsigned char)'a');
        }
        if (left_ch != right_ch) {
            return 0;
        }
        ++index;
    }

    return left[index] == '\0' && right[index] == '\0';
}

static int
sixel_builtin_hdr_ascii_has_prefix(char const *text, char const *prefix)
{
    size_t index;
    unsigned char text_ch;
    unsigned char prefix_ch;

    index = 0u;
    if (text == NULL || prefix == NULL) {
        return 0;
    }

    while (prefix[index] != '\0') {
        text_ch = (unsigned char)text[index];
        prefix_ch = (unsigned char)prefix[index];
        if (text_ch == '\0') {
            return 0;
        }
        if (tolower(text_ch) != tolower(prefix_ch)) {
            return 0;
        }
        ++index;
    }

    return 1;
}

static int
sixel_builtin_hdr_parse_double_list(char const *text,
                                    double *values,
                                    size_t count)
{
    char *endptr;
    char const *cursor;
    size_t index;

    if (text == NULL || values == NULL || count == 0u) {
        return 0;
    }

    cursor = text;
    for (index = 0u; index < count; ++index) {
        while (*cursor != '\0' && isspace((unsigned char)*cursor)) {
            ++cursor;
        }
        if (*cursor == '\0') {
            return 0;
        }

        values[index] = strtod(cursor, &endptr);
        if (endptr == cursor || !isfinite(values[index])) {
            return 0;
        }
        cursor = endptr;
    }

    while (*cursor != '\0' && isspace((unsigned char)*cursor)) {
        ++cursor;
    }

    return *cursor == '\0';
}

static int
sixel_builtin_hdr_parse_gamma_line(char const *line, double *gamma)
{
    double values[1];

    values[0] = 0.0;
    if (line == NULL || gamma == NULL) {
        return 0;
    }
    if (!sixel_builtin_hdr_parse_double_list(line, values, 1u)) {
        return 0;
    }
    if (!(values[0] > 0.0)) {
        return 0;
    }

    *gamma = values[0];
    return 1;
}

static int
sixel_builtin_hdr_parse_exposure_line(char const *line, double *exposure)
{
    double values[1];

    values[0] = 0.0;
    if (line == NULL || exposure == NULL) {
        return 0;
    }
    if (!sixel_builtin_hdr_parse_double_list(line, values, 1u)) {
        return 0;
    }
    if (!(values[0] > 0.0)) {
        return 0;
    }

    *exposure = values[0];
    return 1;
}

static int
sixel_builtin_hdr_parse_colorcorr_line(char const *line,
                                       double *red,
                                       double *green,
                                       double *blue)
{
    double values[3];

    values[0] = 0.0;
    values[1] = 0.0;
    values[2] = 0.0;
    if (line == NULL || red == NULL || green == NULL || blue == NULL) {
        return 0;
    }
    if (!sixel_builtin_hdr_parse_double_list(line, values, 3u)) {
        return 0;
    }
    if (!(values[0] > 0.0) ||
        !(values[1] > 0.0) ||
        !(values[2] > 0.0)) {
        return 0;
    }

    *red = values[0];
    *green = values[1];
    *blue = values[2];
    return 1;
}

static int
sixel_builtin_hdr_parse_pixaspect_line(char const *line, double *pixaspect)
{
    double values[1];

    values[0] = 0.0;
    if (line == NULL || pixaspect == NULL) {
        return 0;
    }
    if (!sixel_builtin_hdr_parse_double_list(line, values, 1u)) {
        return 0;
    }
    if (!(values[0] > 0.0)) {
        return 0;
    }

    *pixaspect = values[0];
    return 1;
}

static void
sixel_builtin_hdr_mark_malformed(int *field_malformed,
                                 sixel_builtin_hdr_profile_hint_t *hint)
{
    if (field_malformed != NULL) {
        *field_malformed = 1;
    }
    if (hint != NULL) {
        hint->malformed = 1;
    }
}

static int
sixel_builtin_hdr_multiply_positive(double base,
                                    double factor,
                                    double *out_value)
{
    double product;

    product = 0.0;
    if (out_value == NULL) {
        return 0;
    }
    if (!isfinite(base) || !isfinite(factor)) {
        return 0;
    }
    if (base <= 0.0 || factor <= 0.0) {
        return 0;
    }
    if (base > DBL_MAX / factor) {
        return 0;
    }

    product = base * factor;
    if (!isfinite(product) || product <= 0.0) {
        return 0;
    }

    *out_value = product;
    return 1;
}

static int
sixel_builtin_hdr_parse_header_metadata_line(
    char const *line,
    sixel_builtin_hdr_profile_hint_t *hint)
{
    double gamma_value;
    double exposure_value;
    double exposure_scale;
    double base_scale;
    double colorcorr_r;
    double colorcorr_g;
    double colorcorr_b;
    double pixaspect;
    int format_kind;

    gamma_value = 0.0;
    exposure_value = 0.0;
    exposure_scale = 1.0;
    base_scale = 1.0;
    colorcorr_r = 0.0;
    colorcorr_g = 0.0;
    colorcorr_b = 0.0;
    pixaspect = 0.0;
    format_kind = SIXEL_BUILTIN_HDR_FORMAT_UNKNOWN;

    if (line == NULL || hint == NULL) {
        return 0;
    }

    if (sixel_builtin_hdr_ascii_has_prefix(line, "GAMMA=")) {
        if (sixel_builtin_hdr_parse_gamma_line(line + 6, &gamma_value)) {
            hint->gamma = gamma_value;
            hint->has_gamma = 1;
        } else {
            sixel_builtin_hdr_mark_malformed(&hint->gamma_malformed, hint);
        }
        return 1;
    }

    if (sixel_builtin_hdr_ascii_has_prefix(line, "PRIMARIES=")) {
        if (!sixel_builtin_hdr_parse_primaries_line(line + 10, hint)) {
            sixel_builtin_hdr_mark_malformed(&hint->primaries_malformed,
                                             hint);
        }
        return 1;
    }

    if (sixel_builtin_hdr_ascii_has_prefix(line, "EXPOSURE=")) {
        if (!sixel_builtin_hdr_parse_exposure_line(line + 9,
                                                   &exposure_value)) {
            sixel_builtin_hdr_mark_malformed(&hint->exposure_malformed, hint);
            return 1;
        }
        exposure_scale = hint->has_exposure ? hint->exposure_scale : 1.0;
        if (!sixel_builtin_hdr_multiply_positive(exposure_scale,
                                                 exposure_value,
                                                 &exposure_scale)) {
            sixel_builtin_hdr_mark_malformed(&hint->exposure_malformed, hint);
            return 1;
        }
        hint->exposure_scale = exposure_scale;
        hint->has_exposure = 1;
        return 1;
    }

    if (sixel_builtin_hdr_ascii_has_prefix(line, "COLORCORR=")) {
        if (!sixel_builtin_hdr_parse_colorcorr_line(line + 10,
                                                    &colorcorr_r,
                                                    &colorcorr_g,
                                                    &colorcorr_b)) {
            sixel_builtin_hdr_mark_malformed(&hint->colorcorr_malformed, hint);
            return 1;
        }

        base_scale = hint->has_colorcorr ? hint->colorcorr_r : 1.0;
        if (!sixel_builtin_hdr_multiply_positive(base_scale,
                                                 colorcorr_r,
                                                 &colorcorr_r)) {
            sixel_builtin_hdr_mark_malformed(&hint->colorcorr_malformed, hint);
            return 1;
        }
        base_scale = hint->has_colorcorr ? hint->colorcorr_g : 1.0;
        if (!sixel_builtin_hdr_multiply_positive(base_scale,
                                                 colorcorr_g,
                                                 &colorcorr_g)) {
            sixel_builtin_hdr_mark_malformed(&hint->colorcorr_malformed, hint);
            return 1;
        }
        base_scale = hint->has_colorcorr ? hint->colorcorr_b : 1.0;
        if (!sixel_builtin_hdr_multiply_positive(base_scale,
                                                 colorcorr_b,
                                                 &colorcorr_b)) {
            sixel_builtin_hdr_mark_malformed(&hint->colorcorr_malformed, hint);
            return 1;
        }

        hint->has_colorcorr = 1;
        hint->colorcorr_r = colorcorr_r;
        hint->colorcorr_g = colorcorr_g;
        hint->colorcorr_b = colorcorr_b;
        return 1;
    }

    if (sixel_builtin_hdr_ascii_has_prefix(line, "PIXASPECT=")) {
        if (!sixel_builtin_hdr_parse_pixaspect_line(line + 10,
                                                    &pixaspect)) {
            sixel_builtin_hdr_mark_malformed(&hint->pixaspect_malformed, hint);
            return 1;
        }
        base_scale = hint->has_pixaspect ? hint->pixaspect : 1.0;
        if (!sixel_builtin_hdr_multiply_positive(base_scale,
                                                 pixaspect,
                                                 &pixaspect)) {
            sixel_builtin_hdr_mark_malformed(&hint->pixaspect_malformed, hint);
            return 1;
        }
        hint->has_pixaspect = 1;
        hint->pixaspect = pixaspect;
        return 1;
    }

    if (sixel_builtin_hdr_ascii_has_prefix(line, "VIEW=")) {
        if (line[5] == '\0') {
            sixel_builtin_hdr_mark_malformed(&hint->view_malformed, hint);
            return 1;
        }
        hint->has_view = 1;
        return 1;
    }

    if (sixel_builtin_hdr_ascii_has_prefix(line, "FORMAT=")) {
        if (!sixel_builtin_hdr_parse_format_line(line + 7, &format_kind)) {
            sixel_builtin_hdr_mark_malformed(&hint->format_malformed, hint);
            return 1;
        }
        hint->has_format = 1;
        hint->format_kind = format_kind;
        return 1;
    }

    return 0;
}

static void
sixel_builtin_hdr_mark_resolution_malformed(
    sixel_builtin_hdr_profile_hint_t *hint)
{
    sixel_builtin_hdr_mark_malformed(NULL, hint);
    if (hint != NULL) {
        hint->resolution_malformed = 1;
    }
}

static int
sixel_builtin_hdr_validate_xy(double x, double y)
{
    if (!isfinite(x) || !isfinite(y)) {
        return 0;
    }
    if (!(x > 0.0 && y > 0.0)) {
        return 0;
    }
    if (x >= 1.0 || y >= 1.0) {
        return 0;
    }
    if ((x + y) >= 1.0) {
        return 0;
    }

    return 1;
}

static int
sixel_builtin_hdr_parse_primaries_line(char const *line,
                                       sixel_builtin_hdr_profile_hint_t *hint)
{
    double values[8];

    if (line == NULL || hint == NULL) {
        return 0;
    }
    if (!sixel_builtin_hdr_parse_double_list(line, values, 8u)) {
        return 0;
    }

    if (!sixel_builtin_hdr_validate_xy(values[0], values[1]) ||
        !sixel_builtin_hdr_validate_xy(values[2], values[3]) ||
        !sixel_builtin_hdr_validate_xy(values[4], values[5]) ||
        !sixel_builtin_hdr_validate_xy(values[6], values[7])) {
        return 0;
    }

    hint->red_x = values[0];
    hint->red_y = values[1];
    hint->green_x = values[2];
    hint->green_y = values[3];
    hint->blue_x = values[4];
    hint->blue_y = values[5];
    hint->white_x = values[6];
    hint->white_y = values[7];
    hint->has_primaries = 1;

    return 1;
}

static int
sixel_builtin_hdr_parse_format_line(char const *line, int *format_kind)
{
    if (line == NULL || format_kind == NULL) {
        return 0;
    }

    if (sixel_builtin_hdr_ascii_case_equal(line, "32-bit_rle_rgbe")) {
        *format_kind = SIXEL_BUILTIN_HDR_FORMAT_RGBE;
        return 1;
    }
    if (sixel_builtin_hdr_ascii_case_equal(line, "32-bit_rle_xyze")) {
        *format_kind = SIXEL_BUILTIN_HDR_FORMAT_XYZE;
        return 1;
    }

    return 0;
}

static char const *
sixel_builtin_hdr_skip_ascii_space(char const *cursor)
{
    if (cursor == NULL) {
        return NULL;
    }

    while (*cursor != '\0' &&
           isspace((unsigned char)*cursor)) {
        ++cursor;
    }

    return cursor;
}

static int
sixel_builtin_hdr_parse_positive_decimal(char const **pcursor, int *value)
{
    char const *cursor;
    int digit;
    int parsed;

    cursor = NULL;
    digit = 0;
    parsed = 0;

    if (pcursor == NULL || *pcursor == NULL || value == NULL) {
        return 0;
    }

    cursor = *pcursor;
    while (*cursor != '\0' &&
           isdigit((unsigned char)*cursor)) {
        digit = *cursor - '0';
        if (parsed > (INT_MAX - digit) / 10) {
            return 0;
        }
        parsed = parsed * 10 + digit;
        ++cursor;
    }
    if (cursor == *pcursor || parsed <= 0) {
        return 0;
    }

    *value = parsed;
    *pcursor = cursor;
    return 1;
}

static int
sixel_builtin_hdr_parse_resolution_line(char const *line,
                                        sixel_builtin_hdr_profile_hint_t *hint)
{
    char const *cursor;
    char axis1_token_axis;
    char axis1_token_sign;
    char axis2_token_axis;
    char axis2_token_sign;
    int axis1_len;
    int axis2_len;
    int axis1;
    int axis2;
    int axis1_sign;
    int axis2_sign;
    int width;
    int height;

    cursor = NULL;
    axis1_token_axis = '\0';
    axis1_token_sign = '\0';
    axis2_token_axis = '\0';
    axis2_token_sign = '\0';
    axis1_len = 0;
    axis2_len = 0;
    axis1 = 0;
    axis2 = 0;
    axis1_sign = 0;
    axis2_sign = 0;
    width = 0;
    height = 0;

    if (line == NULL || hint == NULL) {
        return 0;
    }

    /*
     * Parse the orientation line manually to avoid scanf_s vararg contract
     * differences on MSVC. The expected form is:
     *   [+|-][XY] <len> [+|-][XY] <len>
     */
    cursor = sixel_builtin_hdr_skip_ascii_space(line);
    if (cursor == NULL || *cursor == '\0') {
        return 0;
    }

    axis1_token_sign = *cursor;
    if (axis1_token_sign != '+' && axis1_token_sign != '-') {
        return 0;
    }
    ++cursor;
    if (*cursor == '\0') {
        return 0;
    }
    axis1_token_axis = *cursor;
    ++cursor;

    cursor = sixel_builtin_hdr_skip_ascii_space(cursor);
    if (!sixel_builtin_hdr_parse_positive_decimal(&cursor, &axis1_len)) {
        return 0;
    }

    cursor = sixel_builtin_hdr_skip_ascii_space(cursor);
    if (cursor == NULL || *cursor == '\0') {
        return 0;
    }
    axis2_token_sign = *cursor;
    if (axis2_token_sign != '+' && axis2_token_sign != '-') {
        return 0;
    }
    ++cursor;
    if (*cursor == '\0') {
        return 0;
    }
    axis2_token_axis = *cursor;
    ++cursor;

    cursor = sixel_builtin_hdr_skip_ascii_space(cursor);
    if (!sixel_builtin_hdr_parse_positive_decimal(&cursor, &axis2_len)) {
        return 0;
    }

    cursor = sixel_builtin_hdr_skip_ascii_space(cursor);
    if (cursor == NULL || *cursor != '\0') {
        return 0;
    }

    if (axis1_token_sign == '+') {
        axis1_sign = 1;
    } else if (axis1_token_sign == '-') {
        axis1_sign = -1;
    } else {
        return 0;
    }

    if (axis2_token_sign == '+') {
        axis2_sign = 1;
    } else if (axis2_token_sign == '-') {
        axis2_sign = -1;
    } else {
        return 0;
    }

    if (axis1_token_axis == 'X' || axis1_token_axis == 'x') {
        axis1 = SIXEL_BUILTIN_HDR_AXIS_X;
    } else if (axis1_token_axis == 'Y' || axis1_token_axis == 'y') {
        axis1 = SIXEL_BUILTIN_HDR_AXIS_Y;
    } else {
        return 0;
    }

    if (axis2_token_axis == 'X' || axis2_token_axis == 'x') {
        axis2 = SIXEL_BUILTIN_HDR_AXIS_X;
    } else if (axis2_token_axis == 'Y' || axis2_token_axis == 'y') {
        axis2 = SIXEL_BUILTIN_HDR_AXIS_Y;
    } else {
        return 0;
    }

    if (axis1 == axis2) {
        return 0;
    }

    width = axis1 == SIXEL_BUILTIN_HDR_AXIS_X ? axis1_len : axis2_len;
    height = axis1 == SIXEL_BUILTIN_HDR_AXIS_Y ? axis1_len : axis2_len;
    if (width <= 0 || height <= 0) {
        return 0;
    }

    hint->has_resolution = 1;
    hint->orientation_axis1 = axis1;
    hint->orientation_axis1_sign = axis1_sign;
    hint->orientation_axis1_length = axis1_len;
    hint->orientation_axis2 = axis2;
    hint->orientation_axis2_sign = axis2_sign;
    hint->orientation_axis2_length = axis2_len;
    hint->width = width;
    hint->height = height;
    return 1;
}

static void
sixel_builtin_hdr_init_profile_hint(sixel_builtin_hdr_profile_hint_t *out_hint)
{
    if (out_hint == NULL) {
        return;
    }

    memset(out_hint, 0, sizeof(*out_hint));
    out_hint->format_kind = SIXEL_BUILTIN_HDR_FORMAT_UNKNOWN;
    out_hint->gamma = 1.0;
    out_hint->exposure_scale = 1.0;
    out_hint->colorcorr_r = 1.0;
    out_hint->colorcorr_g = 1.0;
    out_hint->colorcorr_b = 1.0;
    out_hint->pixaspect = 1.0;
}

#define SIXEL_HDR_SRGB_WHITE_X 0.3127
#define SIXEL_HDR_SRGB_WHITE_Y 0.3290
#define SIXEL_HDR_SRGB_RED_X   0.6400
#define SIXEL_HDR_SRGB_RED_Y   0.3300
#define SIXEL_HDR_SRGB_GREEN_X 0.3000
#define SIXEL_HDR_SRGB_GREEN_Y 0.6000
#define SIXEL_HDR_SRGB_BLUE_X  0.1500
#define SIXEL_HDR_SRGB_BLUE_Y  0.0600

typedef enum sixel_builtin_hdr_fallback_profile {
    SIXEL_BUILTIN_HDR_FALLBACK_LINEAR_SRGB = 0,
    SIXEL_BUILTIN_HDR_FALLBACK_SRGB
} sixel_builtin_hdr_fallback_profile_t;

typedef enum sixel_builtin_hdr_tonemap_mode {
    SIXEL_BUILTIN_HDR_TONEMAP_NONE = 0,
    SIXEL_BUILTIN_HDR_TONEMAP_REINHARD
} sixel_builtin_hdr_tonemap_mode_t;

typedef struct sixel_builtin_hdr_profile_trace {
    double effective_gamma;
    int gamma_from_header;
    int primaries_from_header;
    int header_profile_used;
    int fallback_profile_used;
    sixel_builtin_hdr_fallback_profile_t fallback_profile;
    int profile_apply_failed;
} sixel_builtin_hdr_profile_trace_t;

static int
sixel_builtin_hdr_parse_fallback_profile(
    char const *text,
    sixel_builtin_hdr_fallback_profile_t *out_profile)
{
    if (out_profile == NULL) {
        return 0;
    }
    if (text == NULL || text[0] == '\0') {
        *out_profile = SIXEL_BUILTIN_HDR_FALLBACK_LINEAR_SRGB;
        return 1;
    }

    if (sixel_builtin_hdr_ascii_case_equal(text, "linear-srgb") ||
        sixel_builtin_hdr_ascii_case_equal(text, "linear_srgb") ||
        sixel_builtin_hdr_ascii_case_equal(text, "linearsrgb") ||
        sixel_builtin_hdr_ascii_case_equal(text, "linear")) {
        *out_profile = SIXEL_BUILTIN_HDR_FALLBACK_LINEAR_SRGB;
        return 1;
    }

    if (sixel_builtin_hdr_ascii_case_equal(text, "srgb") ||
        sixel_builtin_hdr_ascii_case_equal(text, "gamma-srgb") ||
        sixel_builtin_hdr_ascii_case_equal(text, "gamma_srgb") ||
        sixel_builtin_hdr_ascii_case_equal(text, "gammasrgb") ||
        sixel_builtin_hdr_ascii_case_equal(text, "gamma")) {
        *out_profile = SIXEL_BUILTIN_HDR_FALLBACK_SRGB;
        return 1;
    }

    return 0;
}

static int
sixel_builtin_hdr_parse_tonemap_mode(
    char const *text,
    sixel_builtin_hdr_tonemap_mode_t *out_mode)
{
    if (out_mode == NULL) {
        return 0;
    }
    if (text == NULL || text[0] == '\0' ||
        sixel_builtin_hdr_ascii_case_equal(text, "none") ||
        sixel_builtin_hdr_ascii_case_equal(text, "off") ||
        sixel_builtin_hdr_ascii_case_equal(text, "disabled") ||
        sixel_builtin_hdr_ascii_case_equal(text, "0")) {
        *out_mode = SIXEL_BUILTIN_HDR_TONEMAP_NONE;
        return 1;
    }
    if (sixel_builtin_hdr_ascii_case_equal(text, "reinhard") ||
        sixel_builtin_hdr_ascii_case_equal(text, "on") ||
        sixel_builtin_hdr_ascii_case_equal(text, "1")) {
        *out_mode = SIXEL_BUILTIN_HDR_TONEMAP_REINHARD;
        return 1;
    }

    return 0;
}

static char const *
sixel_builtin_hdr_tonemap_mode_name(sixel_builtin_hdr_tonemap_mode_t mode)
{
    if (mode == SIXEL_BUILTIN_HDR_TONEMAP_REINHARD) {
        return "reinhard";
    }
    return "none";
}

static char const *
sixel_builtin_hdr_fallback_profile_name(
    sixel_builtin_hdr_fallback_profile_t profile)
{
    if (profile == SIXEL_BUILTIN_HDR_FALLBACK_SRGB) {
        return "srgb";
    }
    return "linear-srgb";
}

static char const *
sixel_builtin_hdr_format_name(int format_kind)
{
    if (format_kind == SIXEL_BUILTIN_HDR_FORMAT_XYZE) {
        return "xyze";
    }
    if (format_kind == SIXEL_BUILTIN_HDR_FORMAT_RGBE) {
        return "rgbe";
    }
    return "unknown";
}

static char
sixel_builtin_hdr_axis_letter(int axis)
{
    if (axis == SIXEL_BUILTIN_HDR_AXIS_X) {
        return 'X';
    }
    return 'Y';
}

static char const *
sixel_builtin_hdr_orientation_name(
    sixel_builtin_hdr_profile_hint_t const *hint,
    char *buffer,
    size_t buffer_size)
{
    int nwrite;

    nwrite = 0;
    if (buffer == NULL || buffer_size == 0u) {
        return "unknown";
    }
    buffer[0] = '\0';
    if (hint == NULL || !hint->has_resolution) {
        return "unknown";
    }

    nwrite = snprintf(buffer,
                      buffer_size,
                      "%c%c %d %c%c %d",
                      hint->orientation_axis1_sign > 0 ? '+' : '-',
                      sixel_builtin_hdr_axis_letter(hint->orientation_axis1),
                      hint->orientation_axis1_length,
                      hint->orientation_axis2_sign > 0 ? '+' : '-',
                      sixel_builtin_hdr_axis_letter(hint->orientation_axis2),
                      hint->orientation_axis2_length);
    if (nwrite <= 0 || (size_t)nwrite >= buffer_size) {
        return "unknown";
    }
    return buffer;
}

static void
sixel_builtin_hdr_init_profile_trace(
    sixel_builtin_hdr_profile_trace_t *trace)
{
    if (trace == NULL) {
        return;
    }
    trace->effective_gamma = 1.0;
    trace->gamma_from_header = 0;
    trace->primaries_from_header = 0;
    trace->header_profile_used = 0;
    trace->fallback_profile_used = 0;
    trace->fallback_profile = SIXEL_BUILTIN_HDR_FALLBACK_LINEAR_SRGB;
    trace->profile_apply_failed = 0;
}

static int
sixel_builtin_hdr_parse_use_header_exposure(
    char const *text,
    int *out_enabled)
{
    if (out_enabled == NULL) {
        return 0;
    }
    if (text == NULL || text[0] == '\0' ||
        sixel_builtin_hdr_ascii_case_equal(text, "1") ||
        sixel_builtin_hdr_ascii_case_equal(text, "on") ||
        sixel_builtin_hdr_ascii_case_equal(text, "true") ||
        sixel_builtin_hdr_ascii_case_equal(text, "yes")) {
        *out_enabled = 1;
        return 1;
    }
    if (sixel_builtin_hdr_ascii_case_equal(text, "0") ||
        sixel_builtin_hdr_ascii_case_equal(text, "off") ||
        sixel_builtin_hdr_ascii_case_equal(text, "false") ||
        sixel_builtin_hdr_ascii_case_equal(text, "no")) {
        *out_enabled = 0;
        return 1;
    }
    return 0;
}

static int
sixel_builtin_hdr_parse_double_env(
    char const *env_name,
    double *out_value)
{
    char const *env_text;
    char *endptr;
    double value;

    env_text = NULL;
    endptr = NULL;
    value = 0.0;
    if (env_name == NULL || out_value == NULL) {
        return 0;
    }

    env_text = sixel_compat_getenv(env_name);
    if (env_text == NULL || env_text[0] == '\0') {
        *out_value = 0.0;
        return 1;
    }

    value = strtod(env_text, &endptr);
    if (endptr == env_text || endptr == NULL || endptr[0] != '\0' ||
        !isfinite(value)) {
        return 0;
    }

    *out_value = value;
    return 1;
}

static double
sixel_builtin_abs_double(double value)
{
    return (value < 0.0) ? -value : value;
}

static int
sixel_builtin_hdr_is_linear_srgb(double gamma,
                                 double white_x,
                                 double white_y,
                                 double red_x,
                                 double red_y,
                                 double green_x,
                                 double green_y,
                                 double blue_x,
                                 double blue_y)
{
    double const gamma_epsilon = 0.000001;
    double const chroma_epsilon = 0.0001;

    if (sixel_builtin_abs_double(gamma - 1.0) > gamma_epsilon) {
        return 0;
    }
    if (sixel_builtin_abs_double(white_x - SIXEL_HDR_SRGB_WHITE_X) >
        chroma_epsilon) {
        return 0;
    }
    if (sixel_builtin_abs_double(white_y - SIXEL_HDR_SRGB_WHITE_Y) >
        chroma_epsilon) {
        return 0;
    }
    if (sixel_builtin_abs_double(red_x - SIXEL_HDR_SRGB_RED_X) >
        chroma_epsilon) {
        return 0;
    }
    if (sixel_builtin_abs_double(red_y - SIXEL_HDR_SRGB_RED_Y) >
        chroma_epsilon) {
        return 0;
    }
    if (sixel_builtin_abs_double(green_x - SIXEL_HDR_SRGB_GREEN_X) >
        chroma_epsilon) {
        return 0;
    }
    if (sixel_builtin_abs_double(green_y - SIXEL_HDR_SRGB_GREEN_Y) >
        chroma_epsilon) {
        return 0;
    }
    if (sixel_builtin_abs_double(blue_x - SIXEL_HDR_SRGB_BLUE_X) >
        chroma_epsilon) {
        return 0;
    }
    if (sixel_builtin_abs_double(blue_y - SIXEL_HDR_SRGB_BLUE_Y) >
        chroma_epsilon) {
        return 0;
    }

    return 1;
}

static void
sixel_builtin_hdr_apply_source_profile(unsigned char *pixels,
                                       int width,
                                       int height,
                                       int pixelformat,
                                       sixel_builtin_hdr_profile_hint_t const
                                           *hint,
                                       SIXELSTATUS hint_status,
                                       sixel_builtin_hdr_profile_trace_t
                                           *profile_trace)
{
    sixel_builtin_hdr_fallback_profile_t fallback_profile;
    sixel_cms_profile_t *src_profile;
    char const *fallback_profile_text;
    double effective_gamma;
    double effective_white_x;
    double effective_white_y;
    double effective_red_x;
    double effective_red_y;
    double effective_green_x;
    double effective_green_y;
    double effective_blue_x;
    double effective_blue_y;
    int use_header_profile_hint;
    int header_linear_override;
    int converted;
    int src_profile_is_header;
    int profile_applied;

    fallback_profile = SIXEL_BUILTIN_HDR_FALLBACK_LINEAR_SRGB;
    src_profile = NULL;
    fallback_profile_text = NULL;
    effective_gamma = 1.0;
    effective_white_x = SIXEL_HDR_SRGB_WHITE_X;
    effective_white_y = SIXEL_HDR_SRGB_WHITE_Y;
    effective_red_x = SIXEL_HDR_SRGB_RED_X;
    effective_red_y = SIXEL_HDR_SRGB_RED_Y;
    effective_green_x = SIXEL_HDR_SRGB_GREEN_X;
    effective_green_y = SIXEL_HDR_SRGB_GREEN_Y;
    effective_blue_x = SIXEL_HDR_SRGB_BLUE_X;
    effective_blue_y = SIXEL_HDR_SRGB_BLUE_Y;
    use_header_profile_hint = 0;
    header_linear_override = 0;
    converted = 0;
    src_profile_is_header = 0;
    profile_applied = 0;
    sixel_builtin_hdr_init_profile_trace(profile_trace);

    if (pixels == NULL || width <= 0 || height <= 0) {
        return;
    }

    if (SIXEL_SUCCEEDED(hint_status) &&
        hint != NULL &&
        hint->gamma_malformed) {
        loader_trace_message(
            "builtin HDR: malformed GAMMA metadata; "
            "using fallback gamma");
    }
    if (SIXEL_SUCCEEDED(hint_status) &&
        hint != NULL &&
        hint->primaries_malformed) {
        loader_trace_message(
            "builtin HDR: malformed PRIMARIES metadata; "
            "using fallback primaries");
    }
    if (SIXEL_SUCCEEDED(hint_status) &&
        hint != NULL &&
        (hint->has_gamma || hint->has_primaries)) {
        use_header_profile_hint = 1;
    }

    if (use_header_profile_hint) {
        if (hint->has_gamma) {
            effective_gamma = hint->gamma;
        }
        if (hint->has_primaries) {
            effective_white_x = hint->white_x;
            effective_white_y = hint->white_y;
            effective_red_x = hint->red_x;
            effective_red_y = hint->red_y;
            effective_green_x = hint->green_x;
            effective_green_y = hint->green_y;
            effective_blue_x = hint->blue_x;
            effective_blue_y = hint->blue_y;
        }
        if (sixel_builtin_hdr_is_linear_srgb(effective_gamma,
                                             effective_white_x,
                                             effective_white_y,
                                             effective_red_x,
                                             effective_red_y,
                                             effective_green_x,
                                             effective_green_y,
                                             effective_blue_x,
                                             effective_blue_y)) {
            header_linear_override = 1;
            profile_applied = 1;
        } else {
            src_profile = sixel_cms_create_rgb_profile_from_gamma_chrm(
                effective_gamma,
                effective_white_x,
                effective_white_y,
                effective_red_x,
                effective_red_y,
                effective_green_x,
                effective_green_y,
                effective_blue_x,
                effective_blue_y);
            if (src_profile == NULL) {
                loader_trace_message(
                    "builtin HDR: header-derived source profile is "
                    "unavailable on this CMS backend; falling back");
            } else {
                src_profile_is_header = 1;
            }
        }
    }

    if (src_profile == NULL && !header_linear_override) {
        if (profile_trace != NULL) {
            profile_trace->fallback_profile_used = 1;
        }
        fallback_profile_text = sixel_compat_getenv(
            "SIXEL_LOADER_HDR_FALLBACK_PROFILE");
        if (!sixel_builtin_hdr_parse_fallback_profile(fallback_profile_text,
                                                      &fallback_profile)) {
            loader_trace_message(
                "builtin HDR: unknown SIXEL_LOADER_HDR_FALLBACK_PROFILE='%s'; "
                "using linear-sRGB fallback",
                fallback_profile_text);
            fallback_profile = SIXEL_BUILTIN_HDR_FALLBACK_LINEAR_SRGB;
        }
        if (profile_trace != NULL) {
            profile_trace->fallback_profile = fallback_profile;
        }

        if (fallback_profile == SIXEL_BUILTIN_HDR_FALLBACK_SRGB) {
            src_profile = sixel_cms_create_srgb_profile();
            if (src_profile == NULL) {
                loader_trace_message(
                    "builtin HDR: fallback profile 'srgb' is unavailable on "
                    "this CMS backend; using decode-linear fallback");
                if (profile_trace != NULL) {
                    profile_trace->profile_apply_failed = 1;
                }
            }
        } else {
            profile_applied = 1;
        }
    }

    if (src_profile != NULL) {
        converted = sixel_cms_convert_profile_to_linearrgb(pixels,
                                                            width,
                                                            height,
                                                            pixelformat,
                                                            src_profile);
        sixel_cms_close_profile(src_profile);
        if (!converted) {
            loader_trace_message(
                "builtin HDR: source profile conversion failed; "
                "using decode-linear fallback");
            if (profile_trace != NULL) {
                profile_trace->profile_apply_failed = 1;
                profile_trace->fallback_profile_used = 1;
                profile_trace->fallback_profile =
                    SIXEL_BUILTIN_HDR_FALLBACK_LINEAR_SRGB;
                profile_trace->header_profile_used = 0;
                profile_trace->gamma_from_header = 0;
                profile_trace->primaries_from_header = 0;
                profile_trace->effective_gamma = 1.0;
            }
            return;
        }
        profile_applied = 1;
    }

    if (profile_trace == NULL || !profile_applied) {
        return;
    }
    if (header_linear_override || src_profile_is_header) {
        profile_trace->header_profile_used = 1;
        profile_trace->gamma_from_header = hint->has_gamma ? 1 : 0;
        profile_trace->primaries_from_header = hint->has_primaries ? 1 : 0;
        profile_trace->effective_gamma = effective_gamma;
        profile_trace->fallback_profile_used = 0;
        profile_trace->fallback_profile =
            SIXEL_BUILTIN_HDR_FALLBACK_LINEAR_SRGB;
        return;
    }
    profile_trace->header_profile_used = 0;
    profile_trace->gamma_from_header = 0;
    profile_trace->primaries_from_header = 0;
    profile_trace->effective_gamma = 1.0;
}

static void
sixel_builtin_hdr_apply_dynamic_range(unsigned char *pixels,
                                      int width,
                                      int height,
                                      int pixelformat,
                                      sixel_builtin_hdr_profile_hint_t const
                                          *hint,
                                      SIXELSTATUS hint_status,
                                      int enable_cms,
                                      sixel_builtin_hdr_profile_trace_t const
                                          *profile_trace)
{
    char const *tonemap_text;
    char const *use_header_exposure_text;
    char const *fallback_label;
    char const *gamma_label;
    char const *primaries_label;
    char const *format_label;
    char const *orientation_label;
    char const *view_label;
    char const *exposure_mode;
    char orientation_buffer[32];
    sixel_builtin_hdr_tonemap_mode_t tonemap_mode;
    float *float_pixels;
    size_t pixel_count;
    size_t sample_count;
    size_t index;
    double effective_gamma;
    double header_exposure_scale;
    double header_exposure_inverse;
    double exposure_ev;
    double env_exposure_scale;
    double exposure_scale;
    double exposure_scale_rgb[3];
    double colorcorr_r;
    double colorcorr_g;
    double colorcorr_b;
    double colorcorr_inverse_r;
    double colorcorr_inverse_g;
    double colorcorr_inverse_b;
    double value;
    double channel_scale;
    int use_header_exposure;
    int channel;

    tonemap_text = NULL;
    use_header_exposure_text = NULL;
    fallback_label = NULL;
    gamma_label = "srgb";
    primaries_label = "srgb";
    format_label = "unknown";
    orientation_label = "unknown";
    view_label = "none";
    exposure_mode = "disabled";
    orientation_buffer[0] = '\0';
    tonemap_mode = SIXEL_BUILTIN_HDR_TONEMAP_NONE;
    float_pixels = NULL;
    pixel_count = 0u;
    sample_count = 0u;
    index = 0u;
    effective_gamma = 1.0;
    header_exposure_scale = 1.0;
    header_exposure_inverse = 1.0;
    exposure_ev = 0.0;
    env_exposure_scale = 1.0;
    exposure_scale = 1.0;
    exposure_scale_rgb[0] = 1.0;
    exposure_scale_rgb[1] = 1.0;
    exposure_scale_rgb[2] = 1.0;
    colorcorr_r = 1.0;
    colorcorr_g = 1.0;
    colorcorr_b = 1.0;
    colorcorr_inverse_r = 1.0;
    colorcorr_inverse_g = 1.0;
    colorcorr_inverse_b = 1.0;
    value = 0.0;
    channel_scale = 1.0;
    use_header_exposure = 1;
    channel = 0;

    if (pixels == NULL || width <= 0 || height <= 0) {
        return;
    }
    if (pixelformat != SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 &&
        pixelformat != SIXEL_PIXELFORMAT_RGBFLOAT32) {
        return;
    }
    if ((size_t)width > SIZE_MAX / (size_t)height) {
        return;
    }

    if (SIXEL_SUCCEEDED(hint_status) &&
        hint != NULL &&
        hint->has_exposure) {
        header_exposure_scale = hint->exposure_scale;
    }
    if (SIXEL_SUCCEEDED(hint_status) &&
        hint != NULL &&
        hint->exposure_malformed) {
        loader_trace_message(
            "builtin HDR: malformed EXPOSURE metadata; "
            "ignoring invalid values");
    }
    if (SIXEL_SUCCEEDED(hint_status) &&
        hint != NULL &&
        hint->colorcorr_malformed) {
        loader_trace_message(
            "builtin HDR: malformed COLORCORR metadata; "
            "ignoring invalid values");
    }
    if (SIXEL_SUCCEEDED(hint_status) &&
        hint != NULL &&
        hint->pixaspect_malformed) {
        loader_trace_message(
            "builtin HDR: malformed PIXASPECT metadata; "
            "ignoring invalid values");
    }
    if (SIXEL_SUCCEEDED(hint_status) &&
        hint != NULL &&
        hint->view_malformed) {
        loader_trace_message(
            "builtin HDR: malformed VIEW metadata; ignoring invalid values");
    }
    if (SIXEL_SUCCEEDED(hint_status) &&
        hint != NULL &&
        hint->format_malformed) {
        loader_trace_message(
            "builtin HDR: malformed FORMAT metadata; "
            "falling back to detected decode path");
    }
    if (SIXEL_SUCCEEDED(hint_status) &&
        hint != NULL &&
        hint->resolution_malformed) {
        loader_trace_message(
            "builtin HDR: malformed resolution metadata; "
            "falling back to detected decode path");
    }
    if (SIXEL_SUCCEEDED(hint_status) &&
        hint != NULL &&
        hint->has_colorcorr) {
        colorcorr_r = hint->colorcorr_r;
        colorcorr_g = hint->colorcorr_g;
        colorcorr_b = hint->colorcorr_b;
    }

    tonemap_text = sixel_compat_getenv("SIXEL_LOADER_HDR_TONEMAP");
    if (!sixel_builtin_hdr_parse_tonemap_mode(tonemap_text, &tonemap_mode)) {
        loader_trace_message(
            "builtin HDR: unknown SIXEL_LOADER_HDR_TONEMAP='%s'; "
            "using none",
            tonemap_text);
        tonemap_mode = SIXEL_BUILTIN_HDR_TONEMAP_NONE;
    }

    if (!sixel_builtin_hdr_parse_double_env("SIXEL_LOADER_HDR_EXPOSURE_EV",
                                            &exposure_ev)) {
        loader_trace_message(
            "builtin HDR: invalid SIXEL_LOADER_HDR_EXPOSURE_EV; using 0");
        exposure_ev = 0.0;
    }

    env_exposure_scale = pow(2.0, exposure_ev);
    if (!isfinite(env_exposure_scale) || env_exposure_scale <= 0.0) {
        loader_trace_message(
            "builtin HDR: exposure scale overflow (ev=%f); using env=1.0",
            exposure_ev);
        env_exposure_scale = 1.0;
    }
    exposure_scale = env_exposure_scale;

    use_header_exposure_text = sixel_compat_getenv(
        "SIXEL_LOADER_HDR_USE_HEADER_EXPOSURE");
    if (!sixel_builtin_hdr_parse_use_header_exposure(use_header_exposure_text,
                                                     &use_header_exposure)) {
        loader_trace_message(
            "builtin HDR: unknown SIXEL_LOADER_HDR_USE_HEADER_EXPOSURE='%s'; "
            "using on",
            use_header_exposure_text);
        use_header_exposure = 1;
    }
    exposure_mode = use_header_exposure ? "inverse" : "disabled";

    if (use_header_exposure &&
        SIXEL_SUCCEEDED(hint_status) &&
        hint != NULL &&
        hint->has_exposure) {
        if (header_exposure_scale > 0.0 &&
            isfinite(header_exposure_scale)) {
            header_exposure_inverse = 1.0 / header_exposure_scale;
            if (!isfinite(header_exposure_inverse) ||
                header_exposure_inverse <= 0.0) {
                loader_trace_message(
                    "builtin HDR: header exposure inverse overflow (%f); "
                    "ignoring header exposure",
                    header_exposure_scale);
                header_exposure_inverse = 1.0;
            }
        } else {
            loader_trace_message(
                "builtin HDR: invalid header exposure (%f); "
                "ignoring header exposure",
                header_exposure_scale);
            header_exposure_inverse = 1.0;
        }
    }

    if (colorcorr_r > 0.0 && isfinite(colorcorr_r)) {
        colorcorr_inverse_r = 1.0 / colorcorr_r;
    } else {
        loader_trace_message(
            "builtin HDR: invalid COLORCORR red channel (%f); "
            "using 1.0",
            colorcorr_r);
        colorcorr_r = 1.0;
        colorcorr_inverse_r = 1.0;
    }
    if (colorcorr_g > 0.0 && isfinite(colorcorr_g)) {
        colorcorr_inverse_g = 1.0 / colorcorr_g;
    } else {
        loader_trace_message(
            "builtin HDR: invalid COLORCORR green channel (%f); "
            "using 1.0",
            colorcorr_g);
        colorcorr_g = 1.0;
        colorcorr_inverse_g = 1.0;
    }
    if (colorcorr_b > 0.0 && isfinite(colorcorr_b)) {
        colorcorr_inverse_b = 1.0 / colorcorr_b;
    } else {
        loader_trace_message(
            "builtin HDR: invalid COLORCORR blue channel (%f); "
            "using 1.0",
            colorcorr_b);
        colorcorr_b = 1.0;
        colorcorr_inverse_b = 1.0;
    }

    if (header_exposure_inverse > 0.0 &&
        isfinite(header_exposure_inverse) &&
        exposure_scale <= DBL_MAX / header_exposure_inverse) {
        exposure_scale *= header_exposure_inverse;
    } else {
        loader_trace_message(
            "builtin HDR: exposure scale overflow after header inverse (%f); "
            "using env-only scale",
            header_exposure_inverse);
        header_exposure_inverse = 1.0;
        exposure_scale = env_exposure_scale;
    }

    if (colorcorr_inverse_r > 0.0 &&
        isfinite(colorcorr_inverse_r) &&
        exposure_scale <= DBL_MAX / colorcorr_inverse_r) {
        exposure_scale_rgb[0] = exposure_scale * colorcorr_inverse_r;
    } else {
        exposure_scale_rgb[0] = exposure_scale;
    }
    if (colorcorr_inverse_g > 0.0 &&
        isfinite(colorcorr_inverse_g) &&
        exposure_scale <= DBL_MAX / colorcorr_inverse_g) {
        exposure_scale_rgb[1] = exposure_scale * colorcorr_inverse_g;
    } else {
        exposure_scale_rgb[1] = exposure_scale;
    }
    if (colorcorr_inverse_b > 0.0 &&
        isfinite(colorcorr_inverse_b) &&
        exposure_scale <= DBL_MAX / colorcorr_inverse_b) {
        exposure_scale_rgb[2] = exposure_scale * colorcorr_inverse_b;
    } else {
        exposure_scale_rgb[2] = exposure_scale;
    }

    if (!isfinite(exposure_scale_rgb[0]) || exposure_scale_rgb[0] <= 0.0) {
        exposure_scale_rgb[0] = exposure_scale;
    }
    if (!isfinite(exposure_scale_rgb[1]) || exposure_scale_rgb[1] <= 0.0) {
        exposure_scale_rgb[1] = exposure_scale;
    }
    if (!isfinite(exposure_scale_rgb[2]) || exposure_scale_rgb[2] <= 0.0) {
        exposure_scale_rgb[2] = exposure_scale;
    }

    if (SIXEL_SUCCEEDED(hint_status) &&
        hint != NULL &&
        hint->has_format) {
        format_label = sixel_builtin_hdr_format_name(hint->format_kind);
    }
    if (SIXEL_SUCCEEDED(hint_status) &&
        hint != NULL) {
        orientation_label = sixel_builtin_hdr_orientation_name(
            hint,
            orientation_buffer,
            sizeof(orientation_buffer));
    }
    if (SIXEL_SUCCEEDED(hint_status) &&
        hint != NULL &&
        hint->has_view) {
        view_label = "present";
    }

    if (!enable_cms) {
        gamma_label = "disabled";
        primaries_label = "disabled";
        fallback_label = "disabled";
    } else if (profile_trace == NULL) {
        fallback_label = "decode-linear";
    } else if (profile_trace->profile_apply_failed) {
        gamma_label = "decode-linear";
        primaries_label = "decode-linear";
        fallback_label = "decode-linear";
    } else if (profile_trace->header_profile_used) {
        gamma_label = profile_trace->gamma_from_header ? "header" : "srgb";
        primaries_label =
            profile_trace->primaries_from_header ? "header" : "srgb";
        effective_gamma = profile_trace->effective_gamma;
        fallback_label = "header";
    } else if (profile_trace->fallback_profile_used) {
        if (profile_trace->fallback_profile ==
            SIXEL_BUILTIN_HDR_FALLBACK_SRGB) {
            gamma_label = "srgb";
            primaries_label = "srgb";
        } else {
            gamma_label = "linear-srgb";
            primaries_label = "linear-srgb";
        }
        fallback_label = sixel_builtin_hdr_fallback_profile_name(
            profile_trace->fallback_profile);
    } else {
        fallback_label = "decode-linear";
    }

    loader_trace_message(
        "builtin HDR: final controls format=%s orientation=%s gamma=%s(%f) "
        "primaries=%s exposure=%f colorcorr=%f/%f/%f pixaspect=%f view=%s "
        "(header=%f header_inv=%f env_ev=%f use_header=%s mode=%s) "
        "tonemap=%s fallback=%s",
        format_label,
        orientation_label,
        gamma_label,
        effective_gamma,
        primaries_label,
        exposure_scale,
        colorcorr_r,
        colorcorr_g,
        colorcorr_b,
        (SIXEL_SUCCEEDED(hint_status) &&
         hint != NULL &&
         hint->has_pixaspect) ? hint->pixaspect : 1.0,
        view_label,
        header_exposure_scale,
        header_exposure_inverse,
        exposure_ev,
        use_header_exposure ? "on" : "off",
        exposure_mode,
        sixel_builtin_hdr_tonemap_mode_name(tonemap_mode),
        fallback_label);

    if (tonemap_mode == SIXEL_BUILTIN_HDR_TONEMAP_NONE &&
        sixel_builtin_abs_double(exposure_scale_rgb[0] - 1.0) <= 0.0000001 &&
        sixel_builtin_abs_double(exposure_scale_rgb[1] - 1.0) <= 0.0000001 &&
        sixel_builtin_abs_double(exposure_scale_rgb[2] - 1.0) <= 0.0000001) {
        return;
    }

    pixel_count = (size_t)width * (size_t)height;
    if (pixel_count > SIZE_MAX / 3u) {
        return;
    }
    sample_count = pixel_count * 3u;
    float_pixels = (float *)pixels;

    for (index = 0u; index < sample_count; ++index) {
        channel = (int)(index % 3u);
        channel_scale = exposure_scale_rgb[channel];
        value = (double)float_pixels[index];
        if (!isfinite(value) || value < 0.0) {
            value = 0.0;
        }

        value *= channel_scale;
        if (!isfinite(value)) {
            if (tonemap_mode == SIXEL_BUILTIN_HDR_TONEMAP_REINHARD) {
                value = DBL_MAX;
            } else {
                value = (double)FLT_MAX;
            }
        }
        if (value < 0.0) {
            value = 0.0;
        } else if (tonemap_mode == SIXEL_BUILTIN_HDR_TONEMAP_NONE &&
                   value > (double)FLT_MAX) {
            value = (double)FLT_MAX;
        }

        if (tonemap_mode == SIXEL_BUILTIN_HDR_TONEMAP_REINHARD) {
            value = value / (1.0 + value);
            if (!isfinite(value) || value < 0.0) {
                value = 0.0;
            } else if (value > 1.0) {
                value = 1.0;
            }
        }

        float_pixels[index] = (float)value;
    }
}

static void
sixel_builtin_hdr_apply_postprocess(
    unsigned char *pixels,
    int width,
    int height,
    int pixelformat,
    sixel_builtin_hdr_profile_hint_t const *hint,
    SIXELSTATUS hint_status,
    int enable_cms)
{
    sixel_builtin_hdr_profile_trace_t profile_trace;

    sixel_builtin_hdr_init_profile_trace(&profile_trace);
    if (enable_cms) {
        sixel_builtin_hdr_apply_source_profile(pixels,
                                               width,
                                               height,
                                               pixelformat,
                                               hint,
                                               hint_status,
                                               &profile_trace);
    }
    sixel_builtin_hdr_apply_dynamic_range(pixels,
                                          width,
                                          height,
                                          pixelformat,
                                          hint,
                                          hint_status,
                                          enable_cms,
                                          &profile_trace);
}

static SIXELSTATUS
sixel_builtin_hdr_assign_decoded_frame(
    sixel_chunk_t const *chunk,
    sixel_frame_t *frame,
    unsigned char *pixels,
    int width,
    int height,
    int hdr_pixelformat,
    int hdr_colorspace)
{
    SIXELSTATUS status;

    status = SIXEL_FALSE;
    if (chunk == NULL || frame == NULL || pixels == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    sixel_frame_set_loop_count(frame, 1);
    status = sixel_frame_as_interface(frame)->vtbl->init_pixels(
        sixel_frame_as_interface(frame),
        &(sixel_frame_pixels_request_t){
            pixels,
            NULL,
            width,
            height,
            hdr_pixelformat,
            hdr_colorspace,
            -1,
            SIXEL_PIXELFORMAT_IS_FLOAT32(hdr_pixelformat)
            ? SIXEL_FRAME_PIXELS_FLOAT32
            : SIXEL_FRAME_PIXELS_U8
        });
    if (SIXEL_FAILED(status)) {
        sixel_allocator_free(chunk->allocator, pixels);
        return status;
    }

    return SIXEL_OK;
}

SIXELSTATUS
sixel_builtin_load_hdr_frame(
    sixel_chunk_t const *chunk,
    sixel_frame_t *frame,
    int enable_cms)
{
    /*
     * Keep HDR-specific decode/profile/postprocess logic inside fromhdr
     * so loader-builtin only dispatches by format.
     */
    SIXELSTATUS status;
    unsigned char *pixels;
    SIXELSTATUS hint_status;
    sixel_builtin_hdr_profile_hint_t hint;
    int width;
    int height;
    int hdr_pixelformat;
    int hdr_colorspace;
    int target_pixelformat;

    status = SIXEL_FALSE;
    pixels = NULL;
    hint_status = SIXEL_FALSE;
    sixel_builtin_hdr_init_profile_hint(&hint);
    width = 0;
    height = 0;
    hdr_pixelformat = SIXEL_PIXELFORMAT_RGB888;
    hdr_colorspace = SIXEL_COLORSPACE_GAMMA;
    target_pixelformat = SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;

    if (chunk == NULL || frame == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = sixel_builtin_decode_hdr_float32_with_hint(chunk,
                                                        &pixels,
                                                        &width,
                                                        &height,
                                                        &hdr_pixelformat,
                                                        &hdr_colorspace,
                                                        &hint,
                                                        &hint_status);
    if (status != SIXEL_OK) {
        return status;
    }

    status = sixel_builtin_hdr_assign_decoded_frame(chunk,
                                                    frame,
                                                    pixels,
                                                    width,
                                                    height,
                                                    hdr_pixelformat,
                                                    hdr_colorspace);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    sixel_builtin_hdr_apply_postprocess(pixels,
                                        width,
                                        height,
                                        hdr_pixelformat,
                                        &hint,
                                        hint_status,
                                        enable_cms);

    if (enable_cms) {
        target_pixelformat = loader_cms_target_pixelformat();
        status = sixel_frame_set_pixelformat(frame, target_pixelformat);
    }

    return status;
}

static int
sixel_builtin_hdr_read_byte(unsigned char const *buffer,
                            size_t size,
                            size_t *cursor,
                            unsigned char *out_value)
{
    if (buffer == NULL || cursor == NULL || out_value == NULL) {
        return 0;
    }
    if (*cursor >= size) {
        return 0;
    }

    *out_value = buffer[*cursor];
    *cursor += 1u;
    return 1;
}

static int
sixel_builtin_hdr_read_pixel(unsigned char const *buffer,
                             size_t size,
                             size_t *cursor,
                             unsigned char out_pixel[4])
{
    size_t index;

    index = 0u;
    if (buffer == NULL || cursor == NULL || out_pixel == NULL) {
        return 0;
    }

    for (index = 0u; index < 4u; ++index) {
        if (!sixel_builtin_hdr_read_byte(buffer,
                                         size,
                                         cursor,
                                         &out_pixel[index])) {
            return 0;
        }
    }
    return 1;
}

static int
sixel_builtin_hdr_decode_new_rle_scanline(unsigned char const *buffer,
                                          size_t size,
                                          size_t *cursor,
                                          size_t scanline_length,
                                          unsigned char *scanline_rgbe)
{
    int component;
    size_t pos;
    unsigned char run_length;
    unsigned char value;
    size_t count;
    size_t offset;

    component = 0;
    pos = 0u;
    run_length = 0u;
    value = 0u;
    count = 0u;
    offset = 0u;
    if (buffer == NULL || cursor == NULL || scanline_rgbe == NULL) {
        return 0;
    }

    for (component = 0; component < 4; ++component) {
        pos = 0u;
        while (pos < scanline_length) {
            if (!sixel_builtin_hdr_read_byte(buffer,
                                             size,
                                             cursor,
                                             &run_length)) {
                return 0;
            }
            if (run_length == 0u) {
                return 0;
            }

            if (run_length > 128u) {
                count = (size_t)(run_length - 128u);
                if (pos + count > scanline_length) {
                    return 0;
                }
                if (!sixel_builtin_hdr_read_byte(buffer,
                                                 size,
                                                 cursor,
                                                 &value)) {
                    return 0;
                }
                for (offset = 0u; offset < count; ++offset) {
                    scanline_rgbe[(pos + offset) * 4u + (size_t)component] =
                        value;
                }
            } else {
                count = (size_t)run_length;
                if (pos + count > scanline_length) {
                    return 0;
                }
                for (offset = 0u; offset < count; ++offset) {
                    if (!sixel_builtin_hdr_read_byte(buffer,
                                                     size,
                                                     cursor,
                                                     &value)) {
                        return 0;
                    }
                    scanline_rgbe[(pos + offset) * 4u + (size_t)component] =
                        value;
                }
            }
            pos += count;
        }
    }

    return 1;
}

static int
sixel_builtin_hdr_decode_old_scanline(unsigned char const *buffer,
                                      size_t size,
                                      size_t *cursor,
                                      size_t scanline_length,
                                      unsigned char const first_pixel[4],
                                      unsigned char *scanline_rgbe)
{
    size_t x_offset;
    size_t run_length_multiplier;
    size_t run_length;
    size_t index;
    unsigned char pixel[4];

    x_offset = 0u;
    run_length_multiplier = 1u;
    run_length = 0u;
    index = 0u;
    pixel[0] = 0u;
    pixel[1] = 0u;
    pixel[2] = 0u;
    pixel[3] = 0u;
    if (buffer == NULL ||
        cursor == NULL ||
        first_pixel == NULL ||
        scanline_rgbe == NULL ||
        scanline_length == 0u) {
        return 0;
    }

    if (first_pixel[0] == 1u &&
        first_pixel[1] == 1u &&
        first_pixel[2] == 1u) {
        return 0;
    }

    memcpy(scanline_rgbe, first_pixel, 4u);
    x_offset = 1u;

    while (x_offset < scanline_length) {
        if (!sixel_builtin_hdr_read_pixel(buffer, size, cursor, pixel)) {
            return 0;
        }

        if (pixel[0] == 1u && pixel[1] == 1u && pixel[2] == 1u) {
            run_length = (size_t)pixel[3] * run_length_multiplier;
            if (run_length == 0u || x_offset + run_length > scanline_length) {
                return 0;
            }
            for (index = 0u; index < run_length; ++index) {
                memcpy(scanline_rgbe + (x_offset + index) * 4u,
                       scanline_rgbe + (x_offset - 1u) * 4u,
                       4u);
            }
            x_offset += run_length;
            if (run_length_multiplier > SIZE_MAX / 256u) {
                return 0;
            }
            run_length_multiplier *= 256u;
            continue;
        }

        memcpy(scanline_rgbe + x_offset * 4u, pixel, 4u);
        run_length_multiplier = 1u;
        x_offset += 1u;
    }

    return 1;
}

static void
sixel_builtin_hdr_rgbe_to_float(unsigned char const pixel[4],
                                double out_triplet[3])
{
    double scale;

    scale = 0.0;
    if (pixel == NULL || out_triplet == NULL) {
        return;
    }

    if (pixel[3] == 0u) {
        out_triplet[0] = 0.0;
        out_triplet[1] = 0.0;
        out_triplet[2] = 0.0;
        return;
    }

    scale = ldexp(1.0, (int)pixel[3] - (128 + 8));
    out_triplet[0] = (double)pixel[0] * scale;
    out_triplet[1] = (double)pixel[1] * scale;
    out_triplet[2] = (double)pixel[2] * scale;
}

static void
sixel_builtin_hdr_xyz_to_linearrgb(double xyz[3])
{
    double x;
    double y;
    double z;
    double r;
    double g;
    double b;

    x = 0.0;
    y = 0.0;
    z = 0.0;
    r = 0.0;
    g = 0.0;
    b = 0.0;
    if (xyz == NULL) {
        return;
    }

    x = xyz[0];
    y = xyz[1];
    z = xyz[2];
    r = 3.240969941904521 * x
      + -1.537383177570093 * y
      + -0.498610760293003 * z;
    g = -0.969243636280880 * x
      + 1.875967501507721 * y
      + 0.041555057407176 * z;
    b = 0.055630079696993 * x
      + -0.203976958888977 * y
      + 1.056971514242878 * z;

    if (!isfinite(r) || r < 0.0) {
        r = 0.0;
    }
    if (!isfinite(g) || g < 0.0) {
        g = 0.0;
    }
    if (!isfinite(b) || b < 0.0) {
        b = 0.0;
    }

    xyz[0] = r;
    xyz[1] = g;
    xyz[2] = b;
}

static int
sixel_builtin_hdr_map_scan_position(
    sixel_builtin_hdr_profile_hint_t const *hint,
                                    int scanline_index,
                                    int sample_index,
                                    int *out_x,
                                    int *out_y)
{
    int x;
    int y;

    x = 0;
    y = 0;
    if (hint == NULL ||
        !hint->has_resolution ||
        out_x == NULL ||
        out_y == NULL) {
        return 0;
    }

    if (hint->orientation_axis1 == SIXEL_BUILTIN_HDR_AXIS_X) {
        x = hint->orientation_axis1_sign > 0
            ? scanline_index
            : (hint->orientation_axis1_length - 1 - scanline_index);
    } else {
        y = hint->orientation_axis1_sign > 0
            ? scanline_index
            : (hint->orientation_axis1_length - 1 - scanline_index);
    }

    if (hint->orientation_axis2 == SIXEL_BUILTIN_HDR_AXIS_X) {
        x = hint->orientation_axis2_sign > 0
            ? sample_index
            : (hint->orientation_axis2_length - 1 - sample_index);
    } else {
        y = hint->orientation_axis2_sign > 0
            ? sample_index
            : (hint->orientation_axis2_length - 1 - sample_index);
    }

    if (x < 0 || x >= hint->width || y < 0 || y >= hint->height) {
        return 0;
    }

    *out_x = x;
    *out_y = (hint->height - 1) - y;
    return 1;
}

static SIXELSTATUS
sixel_builtin_hdr_custom_decode_fail(
    sixel_chunk_t const *chunk,
    unsigned char *pixels,
    unsigned char *scanline_rgbe,
    SIXELSTATUS status,
    char const *message)
{
    if (chunk != NULL) {
        if (scanline_rgbe != NULL) {
            sixel_allocator_free(chunk->allocator, scanline_rgbe);
        }
        if (pixels != NULL) {
            sixel_allocator_free(chunk->allocator, pixels);
        }
    }
    if (message != NULL) {
        sixel_helper_set_additional_message(message);
    }

    return status;
}

static int
sixel_builtin_hdr_scanline_uses_new_rle(
    size_t scanline_length,
    unsigned char const first_pixel[4])
{
    int encoded_scanline_length;

    encoded_scanline_length = 0;
    if (first_pixel == NULL) {
        return 0;
    }

    encoded_scanline_length = ((int)first_pixel[2] << 8)
                            | (int)first_pixel[3];
    return scanline_length >= 8u &&
           scanline_length < 32768u &&
           first_pixel[0] == 2u &&
           first_pixel[1] == 2u &&
           first_pixel[2] < 128u &&
           encoded_scanline_length == (int)scanline_length;
}

static SIXELSTATUS
sixel_builtin_hdr_decode_scanline_rgbe(
    sixel_chunk_t const *chunk,
    size_t *cursor,
    size_t scanline_length,
    unsigned char *scanline_rgbe)
{
    unsigned char first_pixel[4];
    int use_new_rle;

    first_pixel[0] = 0u;
    first_pixel[1] = 0u;
    first_pixel[2] = 0u;
    first_pixel[3] = 0u;
    use_new_rle = 0;

    if (chunk == NULL ||
        chunk->buffer == NULL ||
        cursor == NULL ||
        scanline_rgbe == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (!sixel_builtin_hdr_read_pixel(chunk->buffer,
                                      chunk->size,
                                      cursor,
                                      first_pixel)) {
        sixel_helper_set_additional_message(
            "builtin HDR: truncated pixel stream.");
        return SIXEL_STBI_ERROR;
    }

    use_new_rle = sixel_builtin_hdr_scanline_uses_new_rle(scanline_length,
                                                          first_pixel);
    if (use_new_rle) {
        if (!sixel_builtin_hdr_decode_new_rle_scanline(chunk->buffer,
                                                       chunk->size,
                                                       cursor,
                                                       scanline_length,
                                                       scanline_rgbe)) {
            sixel_helper_set_additional_message(
                "builtin HDR: invalid run-length stream.");
            return SIXEL_STBI_ERROR;
        }
        return SIXEL_OK;
    }

    if (!sixel_builtin_hdr_decode_old_scanline(chunk->buffer,
                                               chunk->size,
                                               cursor,
                                               scanline_length,
                                               first_pixel,
                                               scanline_rgbe)) {
        sixel_helper_set_additional_message(
            "builtin HDR: invalid legacy run-length stream.");
        return SIXEL_STBI_ERROR;
    }

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_builtin_hdr_write_scanline_samples(
    sixel_builtin_hdr_profile_hint_t const *hint,
    unsigned char const *scanline_rgbe,
    int scanline_index,
    size_t scanline_length,
    unsigned char *pixels)
{
    int sample_index;
    int x;
    int y;
    size_t pixel_offset;
    double sample_rgb[3];

    sample_index = 0;
    x = 0;
    y = 0;
    pixel_offset = 0u;
    sample_rgb[0] = 0.0;
    sample_rgb[1] = 0.0;
    sample_rgb[2] = 0.0;

    if (hint == NULL || scanline_rgbe == NULL || pixels == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    for (sample_index = 0; sample_index < (int)scanline_length;
         ++sample_index) {
        if (!sixel_builtin_hdr_map_scan_position(hint,
                                                 scanline_index,
                                                 sample_index,
                                                 &x,
                                                 &y)) {
            sixel_helper_set_additional_message(
                "builtin HDR: invalid orientation mapping.");
            return SIXEL_STBI_ERROR;
        }

        sixel_builtin_hdr_rgbe_to_float(
            scanline_rgbe + (size_t)sample_index * 4u,
            sample_rgb);
        if (hint->format_kind == SIXEL_BUILTIN_HDR_FORMAT_XYZE) {
            sixel_builtin_hdr_xyz_to_linearrgb(sample_rgb);
        }

        pixel_offset = ((size_t)y * (size_t)hint->width + (size_t)x) * 3u;
        ((float *)pixels)[pixel_offset + 0u] = (float)sample_rgb[0];
        ((float *)pixels)[pixel_offset + 1u] = (float)sample_rgb[1];
        ((float *)pixels)[pixel_offset + 2u] = (float)sample_rgb[2];
    }

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_builtin_hdr_write_legacy_stream_sample(
    sixel_builtin_hdr_profile_hint_t const *hint,
    size_t scanline_length,
    size_t pixel_index,
    unsigned char const rgbe[4],
    unsigned char *pixels)
{
    int scanline_index;
    int sample_index;
    int x;
    int y;
    size_t pixel_offset;
    double sample_rgb[3];

    scanline_index = 0;
    sample_index = 0;
    x = 0;
    y = 0;
    pixel_offset = 0u;
    sample_rgb[0] = 0.0;
    sample_rgb[1] = 0.0;
    sample_rgb[2] = 0.0;
    if (hint == NULL ||
        rgbe == NULL ||
        pixels == NULL ||
        scanline_length == 0u) {
        return SIXEL_BAD_ARGUMENT;
    }

    scanline_index = (int)(pixel_index / scanline_length);
    sample_index = (int)(pixel_index % scanline_length);
    if (!sixel_builtin_hdr_map_scan_position(hint,
                                             scanline_index,
                                             sample_index,
                                             &x,
                                             &y)) {
        sixel_helper_set_additional_message(
            "builtin HDR: invalid orientation mapping.");
        return SIXEL_STBI_ERROR;
    }

    sixel_builtin_hdr_rgbe_to_float(rgbe, sample_rgb);
    if (hint->format_kind == SIXEL_BUILTIN_HDR_FORMAT_XYZE) {
        sixel_builtin_hdr_xyz_to_linearrgb(sample_rgb);
    }

    pixel_offset = ((size_t)y * (size_t)hint->width + (size_t)x) * 3u;
    ((float *)pixels)[pixel_offset + 0u] = (float)sample_rgb[0];
    ((float *)pixels)[pixel_offset + 1u] = (float)sample_rgb[1];
    ((float *)pixels)[pixel_offset + 2u] = (float)sample_rgb[2];
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_builtin_hdr_decode_legacy_stream(
    sixel_chunk_t const *chunk,
    size_t *cursor,
    sixel_builtin_hdr_profile_hint_t const *hint,
    size_t scanline_length,
    size_t scanline_count,
    unsigned char *pixels)
{
    size_t total_pixels;
    size_t pixel_index;
    size_t run_length_multiplier;
    size_t run_length;
    size_t repeat_index;
    unsigned char rgbe[4];
    unsigned char previous_rgbe[4];
    int have_previous;
    SIXELSTATUS status;

    total_pixels = 0u;
    pixel_index = 0u;
    run_length_multiplier = 1u;
    run_length = 0u;
    repeat_index = 0u;
    rgbe[0] = 0u;
    rgbe[1] = 0u;
    rgbe[2] = 0u;
    rgbe[3] = 0u;
    previous_rgbe[0] = 0u;
    previous_rgbe[1] = 0u;
    previous_rgbe[2] = 0u;
    previous_rgbe[3] = 0u;
    have_previous = 0;
    status = SIXEL_FALSE;
    if (chunk == NULL ||
        chunk->buffer == NULL ||
        cursor == NULL ||
        hint == NULL ||
        scanline_length == 0u ||
        pixels == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (scanline_count > SIZE_MAX / scanline_length) {
        sixel_helper_set_additional_message(
            "builtin HDR: invalid image dimensions.");
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    total_pixels = scanline_count * scanline_length;

    while (pixel_index < total_pixels) {
        if (!sixel_builtin_hdr_read_pixel(chunk->buffer,
                                          chunk->size,
                                          cursor,
                                          rgbe)) {
            sixel_helper_set_additional_message(
                "builtin HDR: truncated pixel stream.");
            return SIXEL_STBI_ERROR;
        }

        if (rgbe[0] == 1u && rgbe[1] == 1u && rgbe[2] == 1u) {
            if (!have_previous) {
                sixel_helper_set_additional_message(
                    "builtin HDR: invalid legacy run-length stream.");
                return SIXEL_STBI_ERROR;
            }
            run_length = (size_t)rgbe[3] * run_length_multiplier;
            if (run_length == 0u || run_length > total_pixels - pixel_index) {
                sixel_helper_set_additional_message(
                    "builtin HDR: invalid legacy run-length stream.");
                return SIXEL_STBI_ERROR;
            }
            for (repeat_index = 0u;
                 repeat_index < run_length;
                 ++repeat_index) {
                status = sixel_builtin_hdr_write_legacy_stream_sample(
                    hint,
                    scanline_length,
                    pixel_index,
                    previous_rgbe,
                    pixels);
                if (SIXEL_FAILED(status)) {
                    return status;
                }
                ++pixel_index;
            }
            if (run_length_multiplier > SIZE_MAX / 256u) {
                sixel_helper_set_additional_message(
                    "builtin HDR: invalid legacy run-length stream.");
                return SIXEL_STBI_ERROR;
            }
            run_length_multiplier *= 256u;
            continue;
        }

        previous_rgbe[0] = rgbe[0];
        previous_rgbe[1] = rgbe[1];
        previous_rgbe[2] = rgbe[2];
        previous_rgbe[3] = rgbe[3];
        have_previous = 1;
        run_length_multiplier = 1u;
        status = sixel_builtin_hdr_write_legacy_stream_sample(hint,
                                                              scanline_length,
                                                              pixel_index,
                                                              rgbe,
                                                              pixels);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        ++pixel_index;
    }

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_builtin_decode_hdr_float32_custom(
    sixel_chunk_t const *chunk,
    sixel_builtin_hdr_profile_hint_t const *hint,
    unsigned char **ppixels,
    int *pwidth,
    int *pheight,
    int *ppixelformat,
    int *pcolorspace)
{
    unsigned char *pixels;
    unsigned char *scanline_rgbe;
    size_t pixel_count;
    size_t sample_count;
    size_t pixel_bytes;
    size_t scanline_length;
    size_t scanline_count;
    size_t scanline_bytes;
    size_t cursor;
    int scanline_index;
    unsigned char first_pixel[4];
    int use_new_rle;
    SIXELSTATUS status;

    pixels = NULL;
    scanline_rgbe = NULL;
    pixel_count = 0u;
    sample_count = 0u;
    pixel_bytes = 0u;
    scanline_length = 0u;
    scanline_count = 0u;
    scanline_bytes = 0u;
    cursor = 0u;
    scanline_index = 0;
    first_pixel[0] = 0u;
    first_pixel[1] = 0u;
    first_pixel[2] = 0u;
    first_pixel[3] = 0u;
    use_new_rle = 0;
    status = SIXEL_FALSE;

    if (chunk == NULL ||
        chunk->buffer == NULL ||
        hint == NULL ||
        ppixels == NULL ||
        pwidth == NULL ||
        pheight == NULL ||
        ppixelformat == NULL ||
        pcolorspace == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (!hint->has_resolution || !hint->has_format) {
        return SIXEL_FALSE;
    }
    if (hint->format_kind != SIXEL_BUILTIN_HDR_FORMAT_RGBE &&
        hint->format_kind != SIXEL_BUILTIN_HDR_FORMAT_XYZE) {
        sixel_helper_set_additional_message(
            "builtin HDR: unsupported FORMAT line.");
        return SIXEL_STBI_ERROR;
    }
    if (hint->pixel_data_offset > chunk->size) {
        sixel_helper_set_additional_message(
            "builtin HDR: malformed resolution or payload offset.");
        return SIXEL_STBI_ERROR;
    }
    if (hint->width <= 0 || hint->height <= 0 ||
        hint->orientation_axis1_length <= 0 ||
        hint->orientation_axis2_length <= 0) {
        sixel_helper_set_additional_message(
            "builtin HDR: invalid image dimensions.");
        return SIXEL_STBI_ERROR;
    }

    if ((size_t)hint->width > SIZE_MAX / (size_t)hint->height) {
        sixel_helper_set_additional_message(
            "builtin HDR: invalid image dimensions.");
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    pixel_count = (size_t)hint->width * (size_t)hint->height;
    if (pixel_count > SIZE_MAX / 3u) {
        sixel_helper_set_additional_message(
            "builtin HDR: invalid image dimensions.");
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    sample_count = pixel_count * 3u;
    if (sample_count > SIZE_MAX / sizeof(float)) {
        sixel_helper_set_additional_message(
            "builtin HDR: invalid image dimensions.");
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    pixel_bytes = sample_count * sizeof(float);
    pixels = (unsigned char *)sixel_allocator_malloc(chunk->allocator,
                                                     pixel_bytes);
    if (pixels == NULL) {
        sixel_helper_set_additional_message(
            "builtin HDR: allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }
    memset(pixels, 0, pixel_bytes);

    scanline_length = (size_t)hint->orientation_axis2_length;
    scanline_count = (size_t)hint->orientation_axis1_length;
    if (scanline_length > SIZE_MAX / 4u) {
        return sixel_builtin_hdr_custom_decode_fail(
            chunk,
            pixels,
            NULL,
            SIXEL_BAD_INTEGER_OVERFLOW,
            "builtin HDR: scanline is too large.");
    }

    scanline_bytes = scanline_length * 4u;
    scanline_rgbe = (unsigned char *)sixel_allocator_malloc(chunk->allocator,
                                                            scanline_bytes);
    if (scanline_rgbe == NULL) {
        return sixel_builtin_hdr_custom_decode_fail(
            chunk,
            pixels,
            NULL,
            SIXEL_BAD_ALLOCATION,
            "builtin HDR: allocation failed.");
    }

    cursor = hint->pixel_data_offset;
    if (scanline_count > 0u) {
        if (chunk->size - cursor < 4u) {
            return sixel_builtin_hdr_custom_decode_fail(
                chunk,
                pixels,
                scanline_rgbe,
                SIXEL_STBI_ERROR,
                "builtin HDR: truncated pixel stream.");
        }
        first_pixel[0] = chunk->buffer[cursor + 0u];
        first_pixel[1] = chunk->buffer[cursor + 1u];
        first_pixel[2] = chunk->buffer[cursor + 2u];
        first_pixel[3] = chunk->buffer[cursor + 3u];
        use_new_rle = sixel_builtin_hdr_scanline_uses_new_rle(scanline_length,
                                                              first_pixel);
    }

    /*
     * Legacy RGBE does not provide per-scanline headers. Decode the payload
     * as one stream and derive scanline/sample coordinates from pixel index.
     */
    if (!use_new_rle) {
        status = sixel_builtin_hdr_decode_legacy_stream(chunk,
                                                        &cursor,
                                                        hint,
                                                        scanline_length,
                                                        scanline_count,
                                                        pixels);
        if (SIXEL_FAILED(status)) {
            return sixel_builtin_hdr_custom_decode_fail(
                chunk,
                pixels,
                scanline_rgbe,
                status,
                NULL);
        }
        sixel_allocator_free(chunk->allocator, scanline_rgbe);
        *ppixels = pixels;
        *pwidth = hint->width;
        *pheight = hint->height;
        *ppixelformat = SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
        *pcolorspace = SIXEL_COLORSPACE_LINEAR;
        return SIXEL_OK;
    }

    for (scanline_index = 0;
         scanline_index < (int)scanline_count;
         ++scanline_index) {
        status = sixel_builtin_hdr_decode_scanline_rgbe(chunk,
                                                        &cursor,
                                                        scanline_length,
                                                        scanline_rgbe);
        if (SIXEL_FAILED(status)) {
            return sixel_builtin_hdr_custom_decode_fail(
                chunk,
                pixels,
                scanline_rgbe,
                status,
                NULL);
        }

        status = sixel_builtin_hdr_write_scanline_samples(hint,
                                                          scanline_rgbe,
                                                          scanline_index,
                                                          scanline_length,
                                                          pixels);
        if (SIXEL_FAILED(status)) {
            return sixel_builtin_hdr_custom_decode_fail(
                chunk,
                pixels,
                scanline_rgbe,
                status,
                NULL);
        }
    }

    sixel_allocator_free(chunk->allocator, scanline_rgbe);
    *ppixels = pixels;
    *pwidth = hint->width;
    *pheight = hint->height;
    *ppixelformat = SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
    *pcolorspace = SIXEL_COLORSPACE_LINEAR;
    return SIXEL_OK;
}

static int
sixel_builtin_hdr_hint_has_decodable_stream(
    SIXELSTATUS hint_status,
    sixel_builtin_hdr_profile_hint_t const *hint)
{
    if (!SIXEL_SUCCEEDED(hint_status) || hint == NULL) {
        return 0;
    }
    if (!hint->has_format || !hint->has_resolution) {
        return 0;
    }
    if (hint->format_kind != SIXEL_BUILTIN_HDR_FORMAT_RGBE &&
        hint->format_kind != SIXEL_BUILTIN_HDR_FORMAT_XYZE) {
        return 0;
    }

    return 1;
}

static SIXELSTATUS
sixel_builtin_decode_hdr_float32_with_hint(
    sixel_chunk_t const *chunk,
    unsigned char **ppixels,
    int *pwidth,
    int *pheight,
    int *ppixelformat,
    int *pcolorspace,
    sixel_builtin_hdr_profile_hint_t *out_hint,
    SIXELSTATUS *out_hint_status)
{
    SIXELSTATUS hint_status;
    sixel_builtin_hdr_profile_hint_t hint;

    hint_status = SIXEL_FALSE;
    sixel_builtin_hdr_init_profile_hint(&hint);

    if (chunk == NULL ||
        chunk->buffer == NULL ||
        ppixels == NULL ||
        pwidth == NULL ||
        pheight == NULL ||
        ppixelformat == NULL ||
        pcolorspace == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (out_hint != NULL) {
        sixel_builtin_hdr_init_profile_hint(out_hint);
    }
    if (out_hint_status != NULL) {
        *out_hint_status = SIXEL_FALSE;
    }

    *ppixels = NULL;
    *pwidth = 0;
    *pheight = 0;
    *ppixelformat = SIXEL_PIXELFORMAT_RGB888;
    *pcolorspace = SIXEL_COLORSPACE_GAMMA;

    if (chunk->size == 0u) {
        return SIXEL_FALSE;
    }

    hint_status = sixel_builtin_parse_hdr_profile_hint(chunk, &hint);
    if (out_hint != NULL) {
        *out_hint = hint;
    }
    if (out_hint_status != NULL) {
        *out_hint_status = hint_status;
    }

    if (!sixel_builtin_hdr_hint_has_decodable_stream(hint_status, &hint)) {
        return SIXEL_FALSE;
    }
    /*
     * Decode all supported HDR streams through the built-in decoder so
     * behavior stays identical across toolchains and runtimes.
     */
    return sixel_builtin_decode_hdr_float32_custom(chunk,
                                                   &hint,
                                                   ppixels,
                                                   pwidth,
                                                   pheight,
                                                   ppixelformat,
                                                   pcolorspace);
}

typedef struct sixel_builtin_hdr_line_reader {
    unsigned char const *buffer;
    size_t size;
    size_t cursor;
} sixel_builtin_hdr_line_reader_t;

static void
sixel_builtin_hdr_line_reader_init(
    sixel_builtin_hdr_line_reader_t *reader,
    sixel_chunk_t const *chunk)
{
    if (reader == NULL) {
        return;
    }

    reader->buffer = NULL;
    reader->size = 0u;
    reader->cursor = 0u;
    if (chunk == NULL || chunk->buffer == NULL) {
        return;
    }

    reader->buffer = chunk->buffer;
    reader->size = chunk->size;
}

static int
sixel_builtin_hdr_line_reader_next(
    sixel_builtin_hdr_line_reader_t *reader,
    size_t *line_start,
    size_t *line_length)
{
    size_t cursor;

    cursor = 0u;
    if (reader == NULL ||
        reader->buffer == NULL ||
        line_start == NULL ||
        line_length == NULL ||
        reader->cursor >= reader->size) {
        return 0;
    }

    cursor = reader->cursor;
    *line_start = cursor;
    while (cursor < reader->size &&
           reader->buffer[cursor] != '\n' &&
           reader->buffer[cursor] != '\r') {
        ++cursor;
    }
    *line_length = cursor - *line_start;

    if (cursor < reader->size) {
        if (reader->buffer[cursor] == '\r') {
            ++cursor;
            if (cursor < reader->size &&
                reader->buffer[cursor] == '\n') {
                ++cursor;
            }
        } else if (reader->buffer[cursor] == '\n') {
            ++cursor;
            if (cursor < reader->size &&
                reader->buffer[cursor] == '\r') {
                ++cursor;
            }
        }
    }

    reader->cursor = cursor;
    return 1;
}

static int
sixel_builtin_hdr_copy_line_to_buffer(
    char *line_buffer,
    size_t line_buffer_size,
    unsigned char const *source_buffer,
    size_t line_start,
    size_t line_length)
{
    if (line_buffer == NULL ||
        source_buffer == NULL ||
        line_buffer_size == 0u ||
        line_length >= line_buffer_size) {
        return 0;
    }

    memcpy(line_buffer, source_buffer + line_start, line_length);
    line_buffer[line_length] = '\0';
    return 1;
}

static SIXELSTATUS
sixel_builtin_parse_hdr_profile_hint(
    sixel_chunk_t const *chunk,
    sixel_builtin_hdr_profile_hint_t *out_hint)
{
    sixel_builtin_hdr_line_reader_t reader;
    size_t line_start;
    size_t line_length;
    char line[1024];
    int in_header;
    int have_resolution_line;
    int header_line_index;
    int have_signature;

    sixel_builtin_hdr_line_reader_init(&reader, chunk);
    line_start = 0u;
    line_length = 0u;
    in_header = 1;
    have_resolution_line = 0;
    header_line_index = 0;
    have_signature = 0;

    if (chunk == NULL || chunk->buffer == NULL || out_hint == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    sixel_builtin_hdr_init_profile_hint(out_hint);
    if (chunk->size == 0u) {
        return SIXEL_FALSE;
    }

    while (!have_resolution_line &&
           sixel_builtin_hdr_line_reader_next(&reader,
                                              &line_start,
                                              &line_length)) {

        if (in_header) {
            if (line_length == 0u) {
                in_header = 0;
                continue;
            }
            if (!sixel_builtin_hdr_copy_line_to_buffer(line,
                                                       sizeof(line),
                                                       chunk->buffer,
                                                       line_start,
                                                       line_length)) {
                continue;
            }
            if (header_line_index == 0) {
                if (!sixel_builtin_hdr_ascii_case_equal(line,
                                                        "#?RADIANCE") &&
                    !sixel_builtin_hdr_ascii_case_equal(line, "#?RGBE")) {
                    return SIXEL_FALSE;
                }
                have_signature = 1;
                ++header_line_index;
                continue;
            }
            sixel_builtin_hdr_parse_header_metadata_line(line, out_hint);
            ++header_line_index;
            continue;
        }

        if (line_length == 0u) {
            continue;
        }
        if (!sixel_builtin_hdr_copy_line_to_buffer(line,
                                                   sizeof(line),
                                                   chunk->buffer,
                                                   line_start,
                                                   line_length)) {
            sixel_builtin_hdr_mark_resolution_malformed(out_hint);
            break;
        }

        if (!sixel_builtin_hdr_parse_resolution_line(line, out_hint)) {
            sixel_builtin_hdr_mark_resolution_malformed(out_hint);
            break;
        }
        out_hint->pixel_data_offset = reader.cursor;
        have_resolution_line = 1;
    }

    if (!have_signature) {
        return SIXEL_FALSE;
    }
    if (!out_hint->has_resolution) {
        sixel_builtin_hdr_mark_resolution_malformed(out_hint);
    }

    return SIXEL_OK;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 : */
/* EOF */
