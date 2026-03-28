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
#if HAVE_STRING_H
# include <string.h>
#endif

#include <sixel.h>

#include "fromhdr.h"

int
stbi_is_hdr_from_memory(unsigned char const *buffer, int len);

float *
stbi_loadf_from_memory(unsigned char const *buffer,
                       int len,
                       int *x,
                       int *y,
                       int *channels_in_file,
                       int desired_channels);

char const *
stbi_failure_reason(void);

void
stbi_image_free(void *retval_from_stbi_load);

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

SIXELSTATUS
sixel_builtin_decode_hdr_float32(
    sixel_chunk_t const *chunk,
    unsigned char **ppixels,
    int *pwidth,
    int *pheight,
    int *ppixelformat,
    int *pcolorspace)
{
    float *decoded_pixels;
    int depth;
    char const *reason;

    decoded_pixels = NULL;
    depth = 0;
    reason = NULL;

    if (chunk == NULL ||
        chunk->buffer == NULL ||
        ppixels == NULL ||
        pwidth == NULL ||
        pheight == NULL ||
        ppixelformat == NULL ||
        pcolorspace == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *ppixels = NULL;
    *pwidth = 0;
    *pheight = 0;
    *ppixelformat = SIXEL_PIXELFORMAT_RGB888;
    *pcolorspace = SIXEL_COLORSPACE_GAMMA;

    if (chunk->size == 0u) {
        return SIXEL_FALSE;
    }
    if (chunk->size > (size_t)INT_MAX) {
        sixel_helper_set_additional_message(
            "builtin HDR: input chunk is too large.");
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    if (!stbi_is_hdr_from_memory(chunk->buffer, (int)chunk->size)) {
        return SIXEL_FALSE;
    }

    decoded_pixels = stbi_loadf_from_memory(chunk->buffer,
                                            (int)chunk->size,
                                            pwidth,
                                            pheight,
                                            &depth,
                                            3);
    if (decoded_pixels == NULL) {
        reason = stbi_failure_reason();
        if (reason != NULL) {
            sixel_helper_set_additional_message(reason);
        }
        return SIXEL_STBI_ERROR;
    }
    if (*pwidth <= 0 || *pheight <= 0) {
        stbi_image_free(decoded_pixels);
        sixel_helper_set_additional_message(
            "builtin HDR: invalid image dimensions.");
        return SIXEL_STBI_ERROR;
    }

    *ppixels = (unsigned char *)decoded_pixels;
    *ppixelformat = SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
    *pcolorspace = SIXEL_COLORSPACE_LINEAR;

    return SIXEL_OK;
}

SIXELSTATUS
sixel_builtin_parse_hdr_profile_hint(
    sixel_chunk_t const *chunk,
    sixel_builtin_hdr_profile_hint_t *out_hint)
{
    size_t cursor;
    size_t line_start;
    size_t line_end;
    size_t line_length;
    char line[256];
    int parsing_done;
    double gamma_value;
    double exposure_value;
    double exposure_scale;

    cursor = 0u;
    line_start = 0u;
    line_end = 0u;
    line_length = 0u;
    parsing_done = 0;
    gamma_value = 0.0;
    exposure_value = 0.0;
    exposure_scale = 1.0;

    if (chunk == NULL || chunk->buffer == NULL || out_hint == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    memset(out_hint, 0, sizeof(*out_hint));
    out_hint->gamma = 1.0;
    out_hint->exposure_scale = 1.0;
    if (chunk->size == 0u) {
        return SIXEL_FALSE;
    }

    while (cursor < chunk->size && !parsing_done) {
        line_start = cursor;
        while (cursor < chunk->size &&
               chunk->buffer[cursor] != '\n' &&
               chunk->buffer[cursor] != '\r') {
            ++cursor;
        }
        line_end = cursor;
        line_length = line_end - line_start;

        while (cursor < chunk->size &&
               (chunk->buffer[cursor] == '\n' ||
                chunk->buffer[cursor] == '\r')) {
            ++cursor;
        }

        if (line_length == 0u) {
            parsing_done = 1;
            continue;
        }
        if (line_length >= sizeof(line)) {
            continue;
        }

        memcpy(line, chunk->buffer + line_start, line_length);
        line[line_length] = '\0';

        if (sixel_builtin_hdr_ascii_has_prefix(line, "GAMMA=")) {
            if (sixel_builtin_hdr_parse_gamma_line(line + 6, &gamma_value)) {
                out_hint->gamma = gamma_value;
                out_hint->has_gamma = 1;
            } else {
                out_hint->gamma_malformed = 1;
                out_hint->malformed = 1;
            }
            continue;
        }
        if (sixel_builtin_hdr_ascii_has_prefix(line, "PRIMARIES=")) {
            if (!sixel_builtin_hdr_parse_primaries_line(line + 10, out_hint)) {
                out_hint->primaries_malformed = 1;
                out_hint->malformed = 1;
            }
            continue;
        }
        if (sixel_builtin_hdr_ascii_has_prefix(line, "EXPOSURE=")) {
            if (!sixel_builtin_hdr_parse_exposure_line(line + 9,
                                                       &exposure_value)) {
                out_hint->exposure_malformed = 1;
                out_hint->malformed = 1;
                continue;
            }
            exposure_scale = out_hint->has_exposure ? out_hint->exposure_scale
                                                    : 1.0;
            if (!isfinite(exposure_scale) ||
                exposure_scale <= 0.0 ||
                exposure_scale > DBL_MAX / exposure_value) {
                out_hint->exposure_malformed = 1;
                out_hint->malformed = 1;
                continue;
            }
            exposure_scale *= exposure_value;
            if (!isfinite(exposure_scale) || exposure_scale <= 0.0) {
                out_hint->exposure_malformed = 1;
                out_hint->malformed = 1;
                continue;
            }
            out_hint->exposure_scale = exposure_scale;
            out_hint->has_exposure = 1;
            continue;
        }
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
