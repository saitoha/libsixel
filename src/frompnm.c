/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 * Copyright (c) 2014-2016 Hayaki Saito
 * Copyright (c) 2014 kmiya@culti
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

/* STDC_HEADERS */
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#if HAVE_STRING_H
# include <string.h>
#endif  /* HAVE_STRING_H */
#if HAVE_CTYPE_H
# include <ctype.h>
#endif  /* HAVE_CTYPE_H */
#if HAVE_MATH_H
# include <math.h>
#endif  /* HAVE_MATH_H */

#ifndef SIZE_MAX
# define SIZE_MAX ((size_t)-1)
#endif

#include <sixel.h>

#include "compat_stub.h"
#include "loader-common.h"

#define PNM_MAX_WIDTH   (1 << 16)
#define PNM_MAX_HEIGHT  (1 << 16)
#define PNM_MAX_DEPTH   65535
#define PNM_TOKEN_MAX   64

typedef enum pnm_tuple_type {
    PNM_TUPLE_UNSPECIFIED = 0,
    PNM_TUPLE_BLACKANDWHITE,
    PNM_TUPLE_BLACKANDWHITE_ALPHA,
    PNM_TUPLE_GRAYSCALE,
    PNM_TUPLE_GRAYSCALE_ALPHA,
    PNM_TUPLE_RGB,
    PNM_TUPLE_RGB_ALPHA
} pnm_tuple_type_t;

typedef enum pnm_raster_mode {
    PNM_RASTER_ASCII = 0,
    PNM_RASTER_BINARY,
    PNM_RASTER_BITMAP_BINARY
} pnm_raster_mode_t;

typedef struct pnm_header {
    int magic;
    int width;
    int height;
    int depth;
    int maxval;
    int grayscale;
    int has_alpha;
    int ascii;
    int bitmap;
    unsigned char *raster;
    unsigned char *end;
} pnm_header_t;

typedef struct pnm_reader {
    unsigned char *p;
    unsigned char *end;
    pnm_raster_mode_t mode;
    int bytes_per_sample;
    int bitmap_width;
    int bitmap_x;
    int bitmap_mask;
    unsigned int bitmap_byte;
} pnm_reader_t;

static int
pnm_decimal_append_checked(int *value, int digit)
{
    if (*value > (INT_MAX - digit) / 10) {
        return 0;
    }
    *value = *value * 10 + digit;
    return 1;
}

static int
pnm_parse_decimal_token(char const *token, int *value)
{
    char const *s;
    int parsed;

    s = NULL;
    parsed = 0;
    if (token == NULL || value == NULL || token[0] == '\0') {
        return 0;
    }

    s = token;
    parsed = 0;
    while (*s != '\0') {
        if (*s < '0' || *s > '9') {
            return 0;
        }
        if (!pnm_decimal_append_checked(&parsed, *s - '0')) {
            return 0;
        }
        s++;
    }

    *value = parsed;
    return 1;
}

static void
pnm_skip_spaces_and_comments(unsigned char **pp, unsigned char *end)
{
    unsigned char *p;

    p = NULL;
    if (pp == NULL || *pp == NULL || end == NULL) {
        return;
    }

    p = *pp;
    while (p < end) {
        if (isspace((unsigned char)*p)) {
            p++;
            continue;
        }
        if (*p == '#') {
            while (p < end && *p != '\n' && *p != '\r') {
                p++;
            }
            continue;
        }
        break;
    }

    *pp = p;
}

static int
pnm_read_token(unsigned char **pp,
               unsigned char *end,
               char *token,
               size_t token_size)
{
    unsigned char *p;
    size_t len;

    p = NULL;
    len = 0u;
    if (pp == NULL || *pp == NULL || end == NULL || token == NULL ||
        token_size < 2u) {
        return 0;
    }

    p = *pp;
    pnm_skip_spaces_and_comments(&p, end);
    if (p >= end) {
        return 0;
    }

    len = 0u;
    while (p < end && !isspace((unsigned char)*p) && *p != '#') {
        if (len + 1u >= token_size) {
            sixel_helper_set_additional_message(
                "load_pnm: token is too long.");
            return 0;
        }
        token[len++] = (char)*p;
        p++;
    }
    if (len == 0u) {
        return 0;
    }

    token[len] = '\0';
    *pp = p;
    return 1;
}

static int
pnm_read_integer(unsigned char **pp,
                 unsigned char *end,
                 int *value,
                 char const *field)
{
    char token[PNM_TOKEN_MAX];

    memset(token, 0, sizeof(token));
    if (!pnm_read_token(pp, end, token, sizeof(token))) {
        sixel_helper_set_additional_message(
            "load_pnm: missing integer token.");
        return 0;
    }
    if (!pnm_parse_decimal_token(token, value)) {
        sixel_helper_set_additional_message(
            "load_pnm: invalid integer token.");
        return 0;
    }

    (void)field;
    return 1;
}

/*
 * Consume the mandatory separator that follows a binary PNM/PAM header.
 *
 * We intentionally consume only one whitespace separator plus optional comment
 * lines, so binary payload bytes are not eaten accidentally.
 */
static int
pnm_advance_to_binary_data(unsigned char **pp, unsigned char *end)
{
    unsigned char *p;
    int consumed_separator;

    p = NULL;
    consumed_separator = 0;
    if (pp == NULL || *pp == NULL || end == NULL) {
        return 0;
    }

    p = *pp;
    if (p >= end || !isspace((unsigned char)*p)) {
        sixel_helper_set_additional_message(
            "load_pnm: missing binary raster separator.");
        return 0;
    }

    consumed_separator = 1;
    p++;
    if (p < end && *(p - 1) == '\r' && *p == '\n') {
        p++;
    }

    while (p < end && *p == '#') {
        while (p < end && *p != '\n' && *p != '\r') {
            p++;
        }
        if (p >= end || !isspace((unsigned char)*p)) {
            sixel_helper_set_additional_message(
                "load_pnm: invalid comment terminator before raster.");
            return 0;
        }
        p++;
        if (p < end && *(p - 1) == '\r' && *p == '\n') {
            p++;
        }
    }

    if (!consumed_separator) {
        return 0;
    }
    *pp = p;
    return 1;
}

static int
pnm_parse_tuple_type(char const *token, pnm_tuple_type_t *tuple_type)
{
    if (token == NULL || tuple_type == NULL) {
        return 0;
    }

    if (strcmp(token, "BLACKANDWHITE") == 0) {
        *tuple_type = PNM_TUPLE_BLACKANDWHITE;
        return 1;
    }
    if (strcmp(token, "BLACKANDWHITE_ALPHA") == 0) {
        *tuple_type = PNM_TUPLE_BLACKANDWHITE_ALPHA;
        return 1;
    }
    if (strcmp(token, "GRAYSCALE") == 0) {
        *tuple_type = PNM_TUPLE_GRAYSCALE;
        return 1;
    }
    if (strcmp(token, "GRAYSCALE_ALPHA") == 0) {
        *tuple_type = PNM_TUPLE_GRAYSCALE_ALPHA;
        return 1;
    }
    if (strcmp(token, "RGB") == 0) {
        *tuple_type = PNM_TUPLE_RGB;
        return 1;
    }
    if (strcmp(token, "RGB_ALPHA") == 0) {
        *tuple_type = PNM_TUPLE_RGB_ALPHA;
        return 1;
    }

    return 0;
}

static int
pnm_validate_header_limits(pnm_header_t const *header)
{
    char message[128];

    memset(message, 0, sizeof(message));
    if (header == NULL) {
        return 0;
    }

    if (header->width < 1 || header->height < 1) {
        sixel_helper_set_additional_message(
            "load_pnm: width/height must be positive.");
        return 0;
    }
    if (header->width > PNM_MAX_WIDTH) {
        sixel_compat_snprintf(message,
                              sizeof(message),
                              "load_pnm: image width exceeds limit %d.",
                              PNM_MAX_WIDTH);
        sixel_helper_set_additional_message(message);
        return 0;
    }
    if (header->height > PNM_MAX_HEIGHT) {
        sixel_compat_snprintf(message,
                              sizeof(message),
                              "load_pnm: image height exceeds limit %d.",
                              PNM_MAX_HEIGHT);
        sixel_helper_set_additional_message(message);
        return 0;
    }
    if (header->maxval < 1 || header->maxval > PNM_MAX_DEPTH) {
        sixel_compat_snprintf(message,
                              sizeof(message),
                              "load_pnm: sample depth exceeds limit %d.",
                              PNM_MAX_DEPTH);
        sixel_helper_set_additional_message(message);
        return 0;
    }

    return 1;
}

static int
pnm_parse_header(unsigned char *buffer,
                 int length,
                 pnm_header_t *header)
{
    unsigned char *p;
    unsigned char *end;
    char token[PNM_TOKEN_MAX];
    int width_set;
    int height_set;
    int depth_set;
    int maxval_set;
    int endhdr_seen;
    pnm_tuple_type_t tuple_type;

    p = NULL;
    end = NULL;
    memset(token, 0, sizeof(token));
    width_set = 0;
    height_set = 0;
    depth_set = 0;
    maxval_set = 0;
    endhdr_seen = 0;
    tuple_type = PNM_TUPLE_UNSPECIFIED;

    if (buffer == NULL || header == NULL || length < 3) {
        sixel_helper_set_additional_message(
            "load_pnm: input buffer is too short.");
        return 0;
    }

    if (buffer[0] != 'P') {
        sixel_helper_set_additional_message(
            "load_pnm: first character is not 'P'.");
        return 0;
    }

    memset(header, 0, sizeof(*header));
    header->magic = buffer[1];
    p = buffer + 2;
    end = buffer + length;

    switch (header->magic) {
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
        if (!pnm_read_integer(&p, end, &header->width, "width") ||
            !pnm_read_integer(&p, end, &header->height, "height")) {
            return 0;
        }
        if (header->magic == '1' || header->magic == '4') {
            header->maxval = 1;
        } else if (!pnm_read_integer(&p, end, &header->maxval, "maxval")) {
            return 0;
        }

        header->depth =
            (header->magic == '3' || header->magic == '6') ? 3 : 1;
        header->grayscale = header->depth == 1 ? 1 : 0;
        header->has_alpha = 0;
        header->ascii =
            (header->magic == '1' ||
             header->magic == '2' ||
             header->magic == '3');
        header->bitmap =
            (header->magic == '1' || header->magic == '4') ? 1 : 0;

        if (!header->ascii && !pnm_advance_to_binary_data(&p, end)) {
            return 0;
        }
        break;

    case '7':
        while (pnm_read_token(&p, end, token, sizeof(token))) {
            if (strcmp(token, "ENDHDR") == 0) {
                endhdr_seen = 1;
                break;
            }
            if (strcmp(token, "WIDTH") == 0) {
                if (!pnm_read_integer(&p, end, &header->width, "WIDTH")) {
                    return 0;
                }
                width_set = 1;
                continue;
            }
            if (strcmp(token, "HEIGHT") == 0) {
                if (!pnm_read_integer(&p, end, &header->height, "HEIGHT")) {
                    return 0;
                }
                height_set = 1;
                continue;
            }
            if (strcmp(token, "DEPTH") == 0) {
                if (!pnm_read_integer(&p, end, &header->depth, "DEPTH")) {
                    return 0;
                }
                depth_set = 1;
                continue;
            }
            if (strcmp(token, "MAXVAL") == 0) {
                if (!pnm_read_integer(&p, end, &header->maxval, "MAXVAL")) {
                    return 0;
                }
                maxval_set = 1;
                continue;
            }
            if (strcmp(token, "TUPLTYPE") == 0) {
                if (!pnm_read_token(&p, end, token, sizeof(token))) {
                    sixel_helper_set_additional_message(
                        "load_pnm: missing TUPLTYPE value.");
                    return 0;
                }
                if (!pnm_parse_tuple_type(token, &tuple_type)) {
                    sixel_helper_set_additional_message(
                        "load_pnm: unsupported PAM TUPLTYPE.");
                    return 0;
                }
                continue;
            }

            sixel_helper_set_additional_message(
                "load_pnm: unknown PAM header key.");
            return 0;
        }

        if (!endhdr_seen) {
            sixel_helper_set_additional_message(
                "load_pnm: PAM header is missing ENDHDR.");
            return 0;
        }
        if (!width_set || !height_set || !depth_set || !maxval_set) {
            sixel_helper_set_additional_message(
                "load_pnm: PAM header is missing required fields.");
            return 0;
        }

        header->ascii = 0;
        header->bitmap = 0;
        header->grayscale = 0;
        header->has_alpha = 0;

        if (tuple_type == PNM_TUPLE_UNSPECIFIED) {
            switch (header->depth) {
            case 1:
                header->grayscale = 1;
                header->has_alpha = 0;
                break;
            case 2:
                header->grayscale = 1;
                header->has_alpha = 1;
                break;
            case 3:
                header->grayscale = 0;
                header->has_alpha = 0;
                break;
            case 4:
                header->grayscale = 0;
                header->has_alpha = 1;
                break;
            default:
                sixel_helper_set_additional_message(
                    "load_pnm: unsupported PAM DEPTH value.");
                return 0;
            }
        } else {
            switch (tuple_type) {
            case PNM_TUPLE_BLACKANDWHITE:
            case PNM_TUPLE_GRAYSCALE:
                if (header->depth != 1) {
                    sixel_helper_set_additional_message(
                        "load_pnm: PAM TUPLTYPE/DEPTH mismatch.");
                    return 0;
                }
                header->grayscale = 1;
                header->has_alpha = 0;
                break;
            case PNM_TUPLE_BLACKANDWHITE_ALPHA:
            case PNM_TUPLE_GRAYSCALE_ALPHA:
                if (header->depth != 2) {
                    sixel_helper_set_additional_message(
                        "load_pnm: PAM TUPLTYPE/DEPTH mismatch.");
                    return 0;
                }
                header->grayscale = 1;
                header->has_alpha = 1;
                break;
            case PNM_TUPLE_RGB:
                if (header->depth != 3) {
                    sixel_helper_set_additional_message(
                        "load_pnm: PAM TUPLTYPE/DEPTH mismatch.");
                    return 0;
                }
                header->grayscale = 0;
                header->has_alpha = 0;
                break;
            case PNM_TUPLE_RGB_ALPHA:
                if (header->depth != 4) {
                    sixel_helper_set_additional_message(
                        "load_pnm: PAM TUPLTYPE/DEPTH mismatch.");
                    return 0;
                }
                header->grayscale = 0;
                header->has_alpha = 1;
                break;
            default:
                sixel_helper_set_additional_message(
                    "load_pnm: unsupported PAM TUPLTYPE.");
                return 0;
            }
        }

        if (!pnm_advance_to_binary_data(&p, end)) {
            return 0;
        }
        break;

    default:
        sixel_helper_set_additional_message(
            "load_pnm: unknown PNM format.");
        return 0;
    }

    if (!pnm_validate_header_limits(header)) {
        return 0;
    }

    header->raster = p;
    header->end = end;
    return 1;
}

static void
pnm_reader_init(pnm_reader_t *reader, pnm_header_t const *header)
{
    memset(reader, 0, sizeof(*reader));
    reader->p = header->raster;
    reader->end = header->end;
    reader->mode = PNM_RASTER_BINARY;
    if (header->ascii) {
        reader->mode = PNM_RASTER_ASCII;
    } else if (header->bitmap) {
        reader->mode = PNM_RASTER_BITMAP_BINARY;
    }
    reader->bytes_per_sample = header->maxval > 255 ? 2 : 1;
    reader->bitmap_width = header->width;
}

static int
pnm_reader_read_sample(pnm_reader_t *reader, int *sample)
{
    if (reader == NULL || sample == NULL) {
        return 0;
    }

    switch (reader->mode) {
    case PNM_RASTER_ASCII:
        if (!pnm_read_integer(&reader->p, reader->end, sample, "sample")) {
            return 0;
        }
        return 1;

    case PNM_RASTER_BINARY:
        if (reader->bytes_per_sample == 1) {
            if (reader->p >= reader->end) {
                sixel_helper_set_additional_message(
                    "load_pnm: unexpected end of binary raster.");
                return 0;
            }
            *sample = (int)*reader->p;
            reader->p++;
            return 1;
        }
        if (reader->p + 1 >= reader->end) {
            sixel_helper_set_additional_message(
                "load_pnm: unexpected end of 16-bit binary raster.");
            return 0;
        }
        *sample = ((int)reader->p[0] << 8) | (int)reader->p[1];
        reader->p += 2;
        return 1;

    case PNM_RASTER_BITMAP_BINARY:
        if (reader->bitmap_mask == 0) {
            if (reader->p >= reader->end) {
                sixel_helper_set_additional_message(
                    "load_pnm: unexpected end of bitmap raster.");
                return 0;
            }
            reader->bitmap_byte = *reader->p;
            reader->p++;
            reader->bitmap_mask = 0x80;
        }

        *sample = (reader->bitmap_byte & (unsigned int)reader->bitmap_mask)
                    ? 1 : 0;
        reader->bitmap_mask >>= 1;
        reader->bitmap_x++;
        if (reader->bitmap_x >= reader->bitmap_width) {
            reader->bitmap_x = 0;
            reader->bitmap_mask = 0;
        }
        return 1;

    default:
        break;
    }

    return 0;
}

static int
pnm_validate_sample(int sample, int maxval, int bitmap)
{
    if (bitmap) {
        if (sample == 0 || sample == 1) {
            return 1;
        }
        sixel_helper_set_additional_message(
            "load_pnm: bitmap sample must be 0 or 1.");
        return 0;
    }

    if (sample < 0 || sample > maxval) {
        sixel_helper_set_additional_message(
            "load_pnm: sample value is out of range.");
        return 0;
    }

    return 1;
}

static unsigned char
pnm_scale_sample_to_byte(int sample, int maxval)
{
    if (maxval <= 0) {
        return 0u;
    }
    return (unsigned char)(sample * 255 / maxval);
}

static float
pnm_scale_sample_to_unit(int sample, int maxval)
{
    if (maxval <= 0) {
        return 0.0f;
    }
    return (float)sample / (float)maxval;
}

static unsigned char
pnm_bitmap_sample_to_byte(int sample)
{
    return sample == 0 ? 255u : 0u;
}

static float
pnm_bitmap_sample_to_unit(int sample)
{
    return sample == 0 ? 1.0f : 0.0f;
}

static float
pnm_clamp_unit(float value)
{
    if (value < 0.0f) {
        return 0.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

static float
pnm_decode_srgb_unit(float value)
{
    value = pnm_clamp_unit(value);
    if (value <= 0.04045f) {
        return value / 12.92f;
    }
#if HAVE_MATH_H
    return powf((value + 0.055f) / 1.055f, 2.4f);
#else
    return value;
#endif
}

static float
pnm_decode_srgb_byte(unsigned char value)
{
    static int initialized;
    static float lut[256];
    int i;

    i = 0;
    if (!initialized) {
        for (i = 0; i < 256; ++i) {
            lut[i] = pnm_decode_srgb_unit((float)i / 255.0f);
        }
        initialized = 1;
    }

    return lut[value];
}

static void
pnm_resolve_background_linear(float bg_linear[3], unsigned char *bgcolor)
{
    int background_colorspace;

    bg_linear[0] = 0.0f;
    bg_linear[1] = 0.0f;
    bg_linear[2] = 0.0f;
    if (bgcolor == NULL) {
        return;
    }

    background_colorspace = loader_background_colorspace();
    if (background_colorspace == SIXEL_COLORSPACE_LINEAR) {
        bg_linear[0] = (float)bgcolor[0] / 255.0f;
        bg_linear[1] = (float)bgcolor[1] / 255.0f;
        bg_linear[2] = (float)bgcolor[2] / 255.0f;
        return;
    }

    bg_linear[0] = pnm_decode_srgb_byte(bgcolor[0]);
    bg_linear[1] = pnm_decode_srgb_byte(bgcolor[1]);
    bg_linear[2] = pnm_decode_srgb_byte(bgcolor[2]);
}

static int
pnm_decode_rgb8_noalpha(pnm_header_t const *header, unsigned char *rgb)
{
    pnm_reader_t reader;
    size_t pixel_total;
    size_t index;
    size_t offset;
    int sample0;
    int sample1;
    int sample2;
    unsigned char value;

    memset(&reader, 0, sizeof(reader));
    pixel_total = 0u;
    index = 0u;
    offset = 0u;
    sample0 = 0;
    sample1 = 0;
    sample2 = 0;
    value = 0u;

    pnm_reader_init(&reader, header);
    pixel_total = (size_t)header->width * (size_t)header->height;
    for (index = 0u; index < pixel_total; ++index) {
        offset = index * 3u;
        if (header->grayscale) {
            if (!pnm_reader_read_sample(&reader, &sample0) ||
                !pnm_validate_sample(sample0,
                                     header->maxval,
                                     header->bitmap)) {
                return 0;
            }
            if (header->bitmap) {
                value = pnm_bitmap_sample_to_byte(sample0);
            } else {
                value = pnm_scale_sample_to_byte(sample0, header->maxval);
            }
            rgb[offset + 0u] = value;
            rgb[offset + 1u] = value;
            rgb[offset + 2u] = value;
            continue;
        }

        if (!pnm_reader_read_sample(&reader, &sample0) ||
            !pnm_reader_read_sample(&reader, &sample1) ||
            !pnm_reader_read_sample(&reader, &sample2) ||
            !pnm_validate_sample(sample0, header->maxval, 0) ||
            !pnm_validate_sample(sample1, header->maxval, 0) ||
            !pnm_validate_sample(sample2, header->maxval, 0)) {
            return 0;
        }

        rgb[offset + 0u] = pnm_scale_sample_to_byte(sample0, header->maxval);
        rgb[offset + 1u] = pnm_scale_sample_to_byte(sample1, header->maxval);
        rgb[offset + 2u] = pnm_scale_sample_to_byte(sample2, header->maxval);
    }

    return 1;
}

static int
pnm_decode_rgbfloat_noalpha(pnm_header_t const *header, float *rgb)
{
    pnm_reader_t reader;
    size_t pixel_total;
    size_t index;
    size_t offset;
    int sample0;
    int sample1;
    int sample2;
    float value;

    memset(&reader, 0, sizeof(reader));
    pixel_total = 0u;
    index = 0u;
    offset = 0u;
    sample0 = 0;
    sample1 = 0;
    sample2 = 0;
    value = 0.0f;

    pnm_reader_init(&reader, header);
    pixel_total = (size_t)header->width * (size_t)header->height;
    for (index = 0u; index < pixel_total; ++index) {
        offset = index * 3u;
        if (header->grayscale) {
            if (!pnm_reader_read_sample(&reader, &sample0) ||
                !pnm_validate_sample(sample0,
                                     header->maxval,
                                     header->bitmap)) {
                return 0;
            }
            if (header->bitmap) {
                value = pnm_bitmap_sample_to_unit(sample0);
            } else {
                value = pnm_scale_sample_to_unit(sample0, header->maxval);
            }
            rgb[offset + 0u] = value;
            rgb[offset + 1u] = value;
            rgb[offset + 2u] = value;
            continue;
        }

        if (!pnm_reader_read_sample(&reader, &sample0) ||
            !pnm_reader_read_sample(&reader, &sample1) ||
            !pnm_reader_read_sample(&reader, &sample2) ||
            !pnm_validate_sample(sample0, header->maxval, 0) ||
            !pnm_validate_sample(sample1, header->maxval, 0) ||
            !pnm_validate_sample(sample2, header->maxval, 0)) {
            return 0;
        }

        rgb[offset + 0u] = pnm_scale_sample_to_unit(sample0, header->maxval);
        rgb[offset + 1u] = pnm_scale_sample_to_unit(sample1, header->maxval);
        rgb[offset + 2u] = pnm_scale_sample_to_unit(sample2, header->maxval);
    }

    return 1;
}

static int
pnm_decode_rgba8(pnm_header_t const *header,
                 unsigned char *rgb,
                 unsigned char *alpha,
                 int *has_transparency)
{
    pnm_reader_t reader;
    size_t pixel_total;
    size_t index;
    size_t offset;
    int sample0;
    int sample1;
    int sample2;
    int sample3;
    unsigned char color;
    unsigned char a;

    memset(&reader, 0, sizeof(reader));
    pixel_total = 0u;
    index = 0u;
    offset = 0u;
    sample0 = 0;
    sample1 = 0;
    sample2 = 0;
    sample3 = 0;
    color = 0u;
    a = 0u;
    if (has_transparency != NULL) {
        *has_transparency = 0;
    }

    pnm_reader_init(&reader, header);
    pixel_total = (size_t)header->width * (size_t)header->height;
    for (index = 0u; index < pixel_total; ++index) {
        offset = index * 3u;
        if (header->grayscale) {
            if (!pnm_reader_read_sample(&reader, &sample0) ||
                !pnm_reader_read_sample(&reader, &sample1) ||
                !pnm_validate_sample(sample0, header->maxval, 0) ||
                !pnm_validate_sample(sample1, header->maxval, 0)) {
                return 0;
            }

            color = pnm_scale_sample_to_byte(sample0, header->maxval);
            a = pnm_scale_sample_to_byte(sample1, header->maxval);
            rgb[offset + 0u] = color;
            rgb[offset + 1u] = color;
            rgb[offset + 2u] = color;
            alpha[index] = a;
            if (has_transparency != NULL && a < 255u) {
                *has_transparency = 1;
            }
            continue;
        }

        if (!pnm_reader_read_sample(&reader, &sample0) ||
            !pnm_reader_read_sample(&reader, &sample1) ||
            !pnm_reader_read_sample(&reader, &sample2) ||
            !pnm_reader_read_sample(&reader, &sample3) ||
            !pnm_validate_sample(sample0, header->maxval, 0) ||
            !pnm_validate_sample(sample1, header->maxval, 0) ||
            !pnm_validate_sample(sample2, header->maxval, 0) ||
            !pnm_validate_sample(sample3, header->maxval, 0)) {
            return 0;
        }

        rgb[offset + 0u] = pnm_scale_sample_to_byte(sample0, header->maxval);
        rgb[offset + 1u] = pnm_scale_sample_to_byte(sample1, header->maxval);
        rgb[offset + 2u] = pnm_scale_sample_to_byte(sample2, header->maxval);
        a = pnm_scale_sample_to_byte(sample3, header->maxval);
        alpha[index] = a;
        if (has_transparency != NULL && a < 255u) {
            *has_transparency = 1;
        }
    }

    return 1;
}

static int
pnm_decode_rgbafloat(pnm_header_t const *header,
                     float *rgb,
                     float *alpha,
                     int *has_transparency)
{
    pnm_reader_t reader;
    size_t pixel_total;
    size_t index;
    size_t offset;
    int sample0;
    int sample1;
    int sample2;
    int sample3;
    float color;
    float a;

    memset(&reader, 0, sizeof(reader));
    pixel_total = 0u;
    index = 0u;
    offset = 0u;
    sample0 = 0;
    sample1 = 0;
    sample2 = 0;
    sample3 = 0;
    color = 0.0f;
    a = 0.0f;
    if (has_transparency != NULL) {
        *has_transparency = 0;
    }

    pnm_reader_init(&reader, header);
    pixel_total = (size_t)header->width * (size_t)header->height;
    for (index = 0u; index < pixel_total; ++index) {
        offset = index * 3u;
        if (header->grayscale) {
            if (!pnm_reader_read_sample(&reader, &sample0) ||
                !pnm_reader_read_sample(&reader, &sample1) ||
                !pnm_validate_sample(sample0, header->maxval, 0) ||
                !pnm_validate_sample(sample1, header->maxval, 0)) {
                return 0;
            }

            color = pnm_scale_sample_to_unit(sample0, header->maxval);
            a = pnm_scale_sample_to_unit(sample1, header->maxval);
            rgb[offset + 0u] = color;
            rgb[offset + 1u] = color;
            rgb[offset + 2u] = color;
            alpha[index] = a;
            if (has_transparency != NULL && sample1 < header->maxval) {
                *has_transparency = 1;
            }
            continue;
        }

        if (!pnm_reader_read_sample(&reader, &sample0) ||
            !pnm_reader_read_sample(&reader, &sample1) ||
            !pnm_reader_read_sample(&reader, &sample2) ||
            !pnm_reader_read_sample(&reader, &sample3) ||
            !pnm_validate_sample(sample0, header->maxval, 0) ||
            !pnm_validate_sample(sample1, header->maxval, 0) ||
            !pnm_validate_sample(sample2, header->maxval, 0) ||
            !pnm_validate_sample(sample3, header->maxval, 0)) {
            return 0;
        }

        rgb[offset + 0u] = pnm_scale_sample_to_unit(sample0, header->maxval);
        rgb[offset + 1u] = pnm_scale_sample_to_unit(sample1, header->maxval);
        rgb[offset + 2u] = pnm_scale_sample_to_unit(sample2, header->maxval);
        a = pnm_scale_sample_to_unit(sample3, header->maxval);
        alpha[index] = a;
        if (has_transparency != NULL && sample3 < header->maxval) {
            *has_transparency = 1;
        }
    }

    return 1;
}

static void
pnm_compose_rgba8_to_linearrgbfloat32(float *dst,
                                      unsigned char const *rgb,
                                      unsigned char const *alpha,
                                      size_t pixel_total,
                                      float const bg_linear[3])
{
    size_t index;
    size_t offset;
    float alpha_unit;
    float inv_alpha;

    index = 0u;
    offset = 0u;
    alpha_unit = 0.0f;
    inv_alpha = 0.0f;
    for (index = 0u; index < pixel_total; ++index) {
        offset = index * 3u;
        alpha_unit = (float)alpha[index] / 255.0f;
        inv_alpha = 1.0f - alpha_unit;
        dst[offset + 0u] = pnm_decode_srgb_byte(rgb[offset + 0u]) * alpha_unit
                         + bg_linear[0] * inv_alpha;
        dst[offset + 1u] = pnm_decode_srgb_byte(rgb[offset + 1u]) * alpha_unit
                         + bg_linear[1] * inv_alpha;
        dst[offset + 2u] = pnm_decode_srgb_byte(rgb[offset + 2u]) * alpha_unit
                         + bg_linear[2] * inv_alpha;
    }
}

static void
pnm_compose_rgbafloat_to_linearrgbfloat32(float *dst,
                                          float const *rgb,
                                          float const *alpha,
                                          size_t pixel_total,
                                          float const bg_linear[3])
{
    size_t index;
    size_t offset;
    float alpha_unit;
    float inv_alpha;

    index = 0u;
    offset = 0u;
    alpha_unit = 0.0f;
    inv_alpha = 0.0f;
    for (index = 0u; index < pixel_total; ++index) {
        offset = index * 3u;
        alpha_unit = pnm_clamp_unit(alpha[index]);
        inv_alpha = 1.0f - alpha_unit;
        dst[offset + 0u] = pnm_decode_srgb_unit(rgb[offset + 0u]) * alpha_unit
                         + bg_linear[0] * inv_alpha;
        dst[offset + 1u] = pnm_decode_srgb_unit(rgb[offset + 1u]) * alpha_unit
                         + bg_linear[1] * inv_alpha;
        dst[offset + 2u] = pnm_decode_srgb_unit(rgb[offset + 2u]) * alpha_unit
                         + bg_linear[2] * inv_alpha;
    }
}

SIXELSTATUS
load_pnm(unsigned char      /* in */  *p,
         int                /* in */  length,
         sixel_allocator_t  /* in */  *allocator,
         unsigned char      /* in */  *bgcolor,
         unsigned char      /* out */ **result,
         int                /* out */ *psx,
         int                /* out */ *psy,
         unsigned char      /* out */ **ppalette,
         int                /* out */ *pncolors,
         int                /* out */ *ppixelformat)
{
    SIXELSTATUS status;
    pnm_header_t header;
    size_t pixel_total;
    size_t byte_count;
    size_t alpha_byte_count;
    size_t float_byte_count;
    unsigned char *rgb8;
    unsigned char *alpha8;
    float *rgbf;
    float *alphaf;
    float *composed;
    int has_transparency;
    float bg_linear[3];

    status = SIXEL_FALSE;
    memset(&header, 0, sizeof(header));
    pixel_total = 0u;
    byte_count = 0u;
    alpha_byte_count = 0u;
    float_byte_count = 0u;
    rgb8 = NULL;
    alpha8 = NULL;
    rgbf = NULL;
    alphaf = NULL;
    composed = NULL;
    has_transparency = 0;
    bg_linear[0] = 0.0f;
    bg_linear[1] = 0.0f;
    bg_linear[2] = 0.0f;

    if (result == NULL || psx == NULL || psy == NULL ||
        ppixelformat == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *result = NULL;
    if (ppalette != NULL) {
        *ppalette = NULL;
    }
    if (pncolors != NULL) {
        *pncolors = 0;
    }

    if (!pnm_parse_header(p, length, &header)) {
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }

    if ((size_t)header.width > SIZE_MAX / (size_t)header.height) {
        sixel_helper_set_additional_message(
            "load_pnm: image dimensions overflow.");
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto end;
    }
    pixel_total = (size_t)header.width * (size_t)header.height;

    if (!header.has_alpha) {
        if (header.maxval <= 255) {
            if (pixel_total > SIZE_MAX / 3u) {
                sixel_helper_set_additional_message(
                    "load_pnm: RGB8 buffer size overflow.");
                status = SIXEL_BAD_INTEGER_OVERFLOW;
                goto end;
            }
            byte_count = pixel_total * 3u;
            rgb8 = (unsigned char *)sixel_allocator_malloc(allocator,
                                                           byte_count);
            if (rgb8 == NULL) {
                status = SIXEL_BAD_ALLOCATION;
                goto end;
            }
            if (!pnm_decode_rgb8_noalpha(&header, rgb8)) {
                status = SIXEL_RUNTIME_ERROR;
                goto end;
            }
            *result = rgb8;
            rgb8 = NULL;
            *ppixelformat = SIXEL_PIXELFORMAT_RGB888;
        } else {
            if (pixel_total > SIZE_MAX / (3u * sizeof(float))) {
                sixel_helper_set_additional_message(
                    "load_pnm: RGB float buffer size overflow.");
                status = SIXEL_BAD_INTEGER_OVERFLOW;
                goto end;
            }
            float_byte_count = pixel_total * 3u * sizeof(float);
            rgbf = (float *)sixel_allocator_malloc(allocator,
                                                   float_byte_count);
            if (rgbf == NULL) {
                status = SIXEL_BAD_ALLOCATION;
                goto end;
            }
            if (!pnm_decode_rgbfloat_noalpha(&header, rgbf)) {
                status = SIXEL_RUNTIME_ERROR;
                goto end;
            }
            *result = (unsigned char *)rgbf;
            rgbf = NULL;
            *ppixelformat = SIXEL_PIXELFORMAT_RGBFLOAT32;
        }

        *psx = header.width;
        *psy = header.height;
        status = SIXEL_OK;
        goto end;
    }

    if (header.maxval <= 255) {
        if (pixel_total > SIZE_MAX / 3u) {
            sixel_helper_set_additional_message(
                "load_pnm: RGBA8 temporary RGB buffer overflow.");
            status = SIXEL_BAD_INTEGER_OVERFLOW;
            goto end;
        }
        if (pixel_total > SIZE_MAX) {
            sixel_helper_set_additional_message(
                "load_pnm: RGBA8 temporary alpha buffer overflow.");
            status = SIXEL_BAD_INTEGER_OVERFLOW;
            goto end;
        }
        if (pixel_total > SIZE_MAX / (3u * sizeof(float))) {
            sixel_helper_set_additional_message(
                "load_pnm: composed float buffer overflow.");
            status = SIXEL_BAD_INTEGER_OVERFLOW;
            goto end;
        }

        byte_count = pixel_total * 3u;
        alpha_byte_count = pixel_total;
        float_byte_count = pixel_total * 3u * sizeof(float);

        rgb8 = (unsigned char *)sixel_allocator_malloc(allocator, byte_count);
        alpha8 = (unsigned char *)sixel_allocator_malloc(allocator,
                                                         alpha_byte_count);
        if (rgb8 == NULL || alpha8 == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        if (!pnm_decode_rgba8(&header, rgb8, alpha8, &has_transparency)) {
            status = SIXEL_RUNTIME_ERROR;
            goto end;
        }

        if (!has_transparency) {
            sixel_allocator_free(allocator, alpha8);
            alpha8 = NULL;
            *result = rgb8;
            rgb8 = NULL;
            *ppixelformat = SIXEL_PIXELFORMAT_RGB888;
            *psx = header.width;
            *psy = header.height;
            status = SIXEL_OK;
            goto end;
        }

        composed = (float *)sixel_allocator_malloc(allocator, float_byte_count);
        if (composed == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        pnm_resolve_background_linear(bg_linear, bgcolor);
        pnm_compose_rgba8_to_linearrgbfloat32(composed,
                                              rgb8,
                                              alpha8,
                                              pixel_total,
                                              bg_linear);

        sixel_allocator_free(allocator, rgb8);
        sixel_allocator_free(allocator, alpha8);
        rgb8 = NULL;
        alpha8 = NULL;

        *result = (unsigned char *)composed;
        composed = NULL;
        *ppixelformat = SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
        *psx = header.width;
        *psy = header.height;
        status = SIXEL_OK;
        goto end;
    }

    if (pixel_total > SIZE_MAX / (3u * sizeof(float)) ||
        pixel_total > SIZE_MAX / sizeof(float)) {
        sixel_helper_set_additional_message(
            "load_pnm: RGBA float temporary buffer overflow.");
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto end;
    }

    float_byte_count = pixel_total * 3u * sizeof(float);
    alpha_byte_count = pixel_total * sizeof(float);

    rgbf = (float *)sixel_allocator_malloc(allocator, float_byte_count);
    alphaf = (float *)sixel_allocator_malloc(allocator, alpha_byte_count);
    if (rgbf == NULL || alphaf == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    if (!pnm_decode_rgbafloat(&header, rgbf, alphaf, &has_transparency)) {
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }

    if (!has_transparency) {
        sixel_allocator_free(allocator, alphaf);
        alphaf = NULL;
        *result = (unsigned char *)rgbf;
        rgbf = NULL;
        *ppixelformat = SIXEL_PIXELFORMAT_RGBFLOAT32;
        *psx = header.width;
        *psy = header.height;
        status = SIXEL_OK;
        goto end;
    }

    composed = (float *)sixel_allocator_malloc(allocator, float_byte_count);
    if (composed == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    pnm_resolve_background_linear(bg_linear, bgcolor);
    pnm_compose_rgbafloat_to_linearrgbfloat32(composed,
                                              rgbf,
                                              alphaf,
                                              pixel_total,
                                              bg_linear);

    sixel_allocator_free(allocator, rgbf);
    sixel_allocator_free(allocator, alphaf);
    rgbf = NULL;
    alphaf = NULL;

    *result = (unsigned char *)composed;
    composed = NULL;
    *ppixelformat = SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
    *psx = header.width;
    *psy = header.height;
    status = SIXEL_OK;

end:
    if (rgb8 != NULL) {
        sixel_allocator_free(allocator, rgb8);
    }
    if (alpha8 != NULL) {
        sixel_allocator_free(allocator, alpha8);
    }
    if (rgbf != NULL) {
        sixel_allocator_free(allocator, rgbf);
    }
    if (alphaf != NULL) {
        sixel_allocator_free(allocator, alphaf);
    }
    if (composed != NULL) {
        sixel_allocator_free(allocator, composed);
    }

    return status;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
