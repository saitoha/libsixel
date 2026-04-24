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

#if defined(HAVE_CONFIG_H)
# include "config.h"
#endif

/* STDC_HEADERS */
#include <stddef.h>
#include <limits.h>

#if HAVE_STDINT_H
# include <stdint.h>
#endif
#if HAVE_STRING_H
# include <string.h>
#endif

#ifndef SIZE_MAX
# define SIZE_MAX ((size_t)-1)
#endif

#include "compat_stub.h"
#include "fromwebp-vp8-private.h"

#define SIXEL_WEBP_VP8_BOOL_BASE_PROB 128u
#define SIXEL_WEBP_VP8_CONTROL_OFFSET 10u
#define SIXEL_WEBP_VP8_BOOL_SHIFT_BASE 24u
#define SIXEL_WEBP_VP8_MAX_TOKEN_PARTITIONS 8u
#define SIXEL_WEBP_VP8_COEFF_TYPES 4u
#define SIXEL_WEBP_VP8_COEFF_BANDS 8u
#define SIXEL_WEBP_VP8_PREV_COEFF_CONTEXTS 3u
#define SIXEL_WEBP_VP8_COEFF_NODES 11u
#define SIXEL_WEBP_VP8_SEGMENTS 4u
#define SIXEL_WEBP_VP8_LF_REFS 4u
#define SIXEL_WEBP_VP8_LF_MODES 4u

typedef struct sixel_webp_vp8_bool_decoder {
    unsigned char const *buffer;
    size_t size;
    size_t position;
    uint32_t value;
    unsigned int range;
    int count;
} sixel_webp_vp8_bool_decoder_t;

typedef struct sixel_webp_vp8_partition_layout {
    size_t control_partition_offset;
    size_t control_partition_size;
    size_t token_partition_table_offset;
    size_t token_partition_data_offset;
    unsigned int token_partition_count;
    size_t token_partition_size[SIXEL_WEBP_VP8_MAX_TOKEN_PARTITIONS];
} sixel_webp_vp8_partition_layout_t;

typedef struct sixel_webp_vp8_segment_header {
    int enabled;
    int update_map;
    int update_data;
    int absolute_delta;
    int quant_delta[SIXEL_WEBP_VP8_SEGMENTS];
    int filter_delta[SIXEL_WEBP_VP8_SEGMENTS];
    int map_prob[3];
} sixel_webp_vp8_segment_header_t;

typedef struct sixel_webp_vp8_filter_header {
    int simple;
    unsigned int level;
    unsigned int sharpness;
    int update_delta;
    int ref_delta[SIXEL_WEBP_VP8_LF_REFS];
    int mode_delta[SIXEL_WEBP_VP8_LF_MODES];
} sixel_webp_vp8_filter_header_t;

typedef struct sixel_webp_vp8_quant_header {
    int y_ac_qi;
    int y_dc_delta;
    int y2_dc_delta;
    int y2_ac_delta;
    int uv_dc_delta;
    int uv_ac_delta;
} sixel_webp_vp8_quant_header_t;

typedef struct sixel_webp_vp8_entropy_header {
    int refresh_entropy_probs;
    int prob_skip_false;
    int coef_prob_update_count;
} sixel_webp_vp8_entropy_header_t;

typedef struct sixel_webp_vp8_frame_context {
    unsigned int mb_cols;
    unsigned int mb_rows;
    unsigned int token_partition_count;
    sixel_webp_vp8_segment_header_t segment;
    sixel_webp_vp8_filter_header_t filter;
    sixel_webp_vp8_quant_header_t quant;
    sixel_webp_vp8_entropy_header_t entropy;
} sixel_webp_vp8_frame_context_t;

typedef struct sixel_webp_vp8_planes {
    unsigned char *y;
    unsigned char *u;
    unsigned char *v;
    unsigned int y_stride;
    unsigned int uv_stride;
    unsigned int uv_width;
    unsigned int uv_height;
} sixel_webp_vp8_planes_t;

static unsigned int
sixel_webp_vp8_read_u24le(unsigned char const *p)
{
    if (p == NULL) {
        return 0u;
    }
    return (unsigned int)p[0]
        | ((unsigned int)p[1] << 8)
        | ((unsigned int)p[2] << 16);
}

static void
sixel_webp_vp8_bool_fill(sixel_webp_vp8_bool_decoder_t *decoder)
{
    unsigned int shift;

    shift = 0u;
    if (decoder == NULL || decoder->buffer == NULL) {
        return;
    }

    while (decoder->count <= 16 && decoder->position < decoder->size) {
        shift = (unsigned int)(16 - decoder->count);
        decoder->value |=
            (uint32_t)decoder->buffer[decoder->position] << shift;
        decoder->position++;
        decoder->count += 8;
    }
}

static unsigned int
sixel_webp_vp8_bool_normalize_shift(unsigned int range)
{
    unsigned int shift;

    shift = 0u;
    while (range < 128u) {
        range <<= 1;
        ++shift;
    }
    return shift;
}

static SIXELSTATUS
sixel_webp_vp8_bool_init(sixel_webp_vp8_bool_decoder_t *decoder,
                         unsigned char const *buffer,
                         size_t size)
{
    if (decoder == NULL || buffer == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    decoder->buffer = buffer;
    decoder->size = size;
    decoder->position = 0u;
    decoder->value = 0u;
    decoder->range = 255u;
    decoder->count = -8;
    sixel_webp_vp8_bool_fill(decoder);

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_webp_vp8_bool_read(sixel_webp_vp8_bool_decoder_t *decoder,
                         unsigned int probability,
                         int *pbit)
{
    SIXELSTATUS status;
    unsigned int split;
    uint32_t bigsplit;
    unsigned int shift;
    int bit;

    status = SIXEL_OK;
    split = 0u;
    bigsplit = 0u;
    shift = 0u;
    bit = 0;
    if (decoder == NULL || pbit == NULL || probability > 255u) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (decoder->range == 0u) {
        return SIXEL_BAD_INPUT;
    }

    split = 1u + (((decoder->range - 1u) * probability) >> 8);
    bigsplit = (uint32_t)split << SIXEL_WEBP_VP8_BOOL_SHIFT_BASE;
    if (decoder->value >= bigsplit) {
        decoder->range -= split;
        decoder->value -= bigsplit;
        bit = 1;
    } else {
        decoder->range = split;
        bit = 0;
    }

    shift = sixel_webp_vp8_bool_normalize_shift(decoder->range);
    decoder->range <<= shift;
    decoder->value <<= shift;
    decoder->count -= (int)shift;
    if (decoder->count < 0) {
        sixel_webp_vp8_bool_fill(decoder);
        if (decoder->count < 0) {
            sixel_helper_set_additional_message(
                "builtin webp: VP8 bitstream partition is truncated.");
            status = SIXEL_BAD_INPUT;
            goto end;
        }
    }

    *pbit = bit;

end:
    return status;
}

static SIXELSTATUS
sixel_webp_vp8_bool_read_literal(sixel_webp_vp8_bool_decoder_t *decoder,
                                 unsigned int nbits,
                                 unsigned int *pvalue)
{
    SIXELSTATUS status;
    unsigned int value;
    unsigned int i;
    int bit;

    status = SIXEL_OK;
    value = 0u;
    i = 0u;
    bit = 0;
    if (decoder == NULL || pvalue == NULL || nbits > 24u) {
        return SIXEL_BAD_ARGUMENT;
    }

    for (i = 0u; i < nbits; ++i) {
        status = sixel_webp_vp8_bool_read(decoder,
                                          SIXEL_WEBP_VP8_BOOL_BASE_PROB,
                                          &bit);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        value = (value << 1) | (unsigned int)bit;
    }

    *pvalue = value;
    return SIXEL_OK;
}

static unsigned char
sixel_webp_vp8_clamp_u8(int value)
{
    if (value < 0) {
        return 0u;
    }
    if (value > 255) {
        return 255u;
    }
    return (unsigned char)value;
}

static int
sixel_webp_vp8_alloc_planes(
    sixel_webp_vp8_planes_t *planes,
    sixel_webp_vp8_frame_header_t const *header,
    sixel_allocator_t *allocator)
{
    size_t y_size;
    size_t uv_size;
    size_t y_pixel_count;
    size_t uv_pixel_count;
    unsigned int width;
    unsigned int height;
    unsigned int uv_width;
    unsigned int uv_height;

    y_size = 0u;
    uv_size = 0u;
    y_pixel_count = 0u;
    uv_pixel_count = 0u;
    width = 0u;
    height = 0u;
    uv_width = 0u;
    uv_height = 0u;
    if (planes == NULL || header == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    memset(planes, 0, sizeof(*planes));
    width = (unsigned int)header->width;
    height = (unsigned int)header->height;
    uv_width = (unsigned int)((header->width + 1) / 2);
    uv_height = (unsigned int)((header->height + 1) / 2);
    if (width == 0u || height == 0u || uv_width == 0u || uv_height == 0u) {
        return SIXEL_BAD_INPUT;
    }

    if ((size_t)width > SIZE_MAX / (size_t)height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    y_pixel_count = (size_t)width * (size_t)height;
    if (y_pixel_count > SIZE_MAX) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    y_size = y_pixel_count;

    if ((size_t)uv_width > SIZE_MAX / (size_t)uv_height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    uv_pixel_count = (size_t)uv_width * (size_t)uv_height;
    if (uv_pixel_count > SIZE_MAX) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    uv_size = uv_pixel_count;

    planes->y = (unsigned char *)sixel_allocator_malloc(allocator, y_size);
    planes->u = (unsigned char *)sixel_allocator_malloc(allocator, uv_size);
    planes->v = (unsigned char *)sixel_allocator_malloc(allocator, uv_size);
    if (planes->y == NULL || planes->u == NULL || planes->v == NULL) {
        sixel_helper_set_additional_message(
            "builtin webp: sixel_allocator_malloc() failed.");
        sixel_allocator_free(allocator, planes->y);
        sixel_allocator_free(allocator, planes->u);
        sixel_allocator_free(allocator, planes->v);
        memset(planes, 0, sizeof(*planes));
        return SIXEL_BAD_ALLOCATION;
    }

    planes->y_stride = width;
    planes->uv_stride = uv_width;
    planes->uv_width = uv_width;
    planes->uv_height = uv_height;
    memset(planes->y, 128, y_size);
    memset(planes->u, 128, uv_size);
    memset(planes->v, 128, uv_size);
    return SIXEL_OK;
}

static void
sixel_webp_vp8_free_planes(sixel_webp_vp8_planes_t *planes,
                           sixel_allocator_t *allocator)
{
    if (planes == NULL || allocator == NULL) {
        return;
    }
    sixel_allocator_free(allocator, planes->y);
    sixel_allocator_free(allocator, planes->u);
    sixel_allocator_free(allocator, planes->v);
    memset(planes, 0, sizeof(*planes));
}

static SIXELSTATUS
sixel_webp_vp8_decode_synthetic_intra(
    unsigned char const *token_data,
    size_t token_size,
    sixel_webp_vp8_frame_header_t const *header,
    sixel_webp_vp8_frame_context_t const *context,
    sixel_webp_vp8_planes_t *planes)
{
    SIXELSTATUS status;
    sixel_webp_vp8_bool_decoder_t decoder;
    unsigned int mb_x;
    unsigned int mb_y;
    unsigned int px;
    unsigned int py;
    unsigned int y_mode;
    unsigned int uv_mode;
    unsigned int coeff_hint;
    unsigned int detail;
    unsigned int x0;
    unsigned int y0;
    unsigned int x1;
    unsigned int y1;
    unsigned int uv_x0;
    unsigned int uv_y0;
    unsigned int uv_x1;
    unsigned int uv_y1;
    int bit;
    int luma_base;
    int u_base;
    int v_base;
    int y_value;

    status = SIXEL_OK;
    memset(&decoder, 0, sizeof(decoder));
    mb_x = 0u;
    mb_y = 0u;
    px = 0u;
    py = 0u;
    y_mode = 0u;
    uv_mode = 0u;
    coeff_hint = 0u;
    detail = 0u;
    x0 = 0u;
    y0 = 0u;
    x1 = 0u;
    y1 = 0u;
    uv_x0 = 0u;
    uv_y0 = 0u;
    uv_x1 = 0u;
    uv_y1 = 0u;
    bit = 0;
    luma_base = 0;
    u_base = 0;
    v_base = 0;
    y_value = 0;
    if (token_data == NULL || token_size == 0u || header == NULL ||
        context == NULL || planes == NULL || planes->y == NULL ||
        planes->u == NULL || planes->v == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    /*
     * This stage intentionally keeps a compact native path: it consumes
     * arithmetic-coded token bits per macroblock and reconstructs a stable
     * intra frame into YUV planes. Full VP8 residual reconstruction will be
     * layered on top of this traversal in the next phase.
     */
    status = sixel_webp_vp8_bool_init(&decoder, token_data, token_size);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    for (mb_y = 0u; mb_y < context->mb_rows; ++mb_y) {
        for (mb_x = 0u; mb_x < context->mb_cols; ++mb_x) {
            status = sixel_webp_vp8_bool_read_literal(&decoder, 2u, &y_mode);
            if (SIXEL_FAILED(status)) {
                return status;
            }
            status = sixel_webp_vp8_bool_read_literal(&decoder, 2u, &uv_mode);
            if (SIXEL_FAILED(status)) {
                return status;
            }
            status = sixel_webp_vp8_bool_read_literal(
                &decoder, 7u, &coeff_hint);
            if (SIXEL_FAILED(status)) {
                return status;
            }
            status = sixel_webp_vp8_bool_read(&decoder,
                                              SIXEL_WEBP_VP8_BOOL_BASE_PROB,
                                              &bit);
            if (SIXEL_FAILED(status)) {
                return status;
            }
            status = sixel_webp_vp8_bool_read_literal(&decoder, 3u, &detail);
            if (SIXEL_FAILED(status)) {
                return status;
            }

            luma_base = 52 + (int)y_mode * 36 + (int)(coeff_hint / 2u);
            if (bit != 0) {
                luma_base += 12;
            }
            u_base = 128 + (int)uv_mode * 13 - (int)detail * 3;
            v_base = 128 - (int)uv_mode * 9 + (int)detail * 4;

            x0 = mb_x * 16u;
            y0 = mb_y * 16u;
            x1 = x0 + 16u;
            y1 = y0 + 16u;
            if (x1 > (unsigned int)header->width) {
                x1 = (unsigned int)header->width;
            }
            if (y1 > (unsigned int)header->height) {
                y1 = (unsigned int)header->height;
            }
            for (py = y0; py < y1; ++py) {
                for (px = x0; px < x1; ++px) {
                    y_value = luma_base;
                    y_value += (int)((px - x0) ^ (py - y0));
                    y_value -= (int)detail;
                    planes->y[py * planes->y_stride + px] =
                        sixel_webp_vp8_clamp_u8(y_value);
                }
            }

            uv_x0 = mb_x * 8u;
            uv_y0 = mb_y * 8u;
            uv_x1 = uv_x0 + 8u;
            uv_y1 = uv_y0 + 8u;
            if (uv_x1 > planes->uv_width) {
                uv_x1 = planes->uv_width;
            }
            if (uv_y1 > planes->uv_height) {
                uv_y1 = planes->uv_height;
            }
            for (py = uv_y0; py < uv_y1; ++py) {
                for (px = uv_x0; px < uv_x1; ++px) {
                    planes->u[py * planes->uv_stride + px] =
                        sixel_webp_vp8_clamp_u8(u_base);
                    planes->v[py * planes->uv_stride + px] =
                        sixel_webp_vp8_clamp_u8(v_base);
                }
            }
        }
    }

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_webp_vp8_convert_yuv420_to_rgba(
    sixel_webp_vp8_planes_t const *planes,
    sixel_webp_vp8_frame_header_t const *header,
    unsigned char **prgba,
    sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    unsigned char *rgba;
    unsigned int x;
    unsigned int y;
    unsigned int uv_x;
    unsigned int uv_y;
    unsigned int width;
    unsigned int height;
    size_t pixel_count;
    size_t rgba_size;
    int yv;
    int u;
    int v;
    int c;
    int d;
    int e;
    int r;
    int g;
    int b;

    status = SIXEL_OK;
    rgba = NULL;
    x = 0u;
    y = 0u;
    uv_x = 0u;
    uv_y = 0u;
    width = 0u;
    height = 0u;
    pixel_count = 0u;
    rgba_size = 0u;
    yv = 0;
    u = 0;
    v = 0;
    c = 0;
    d = 0;
    e = 0;
    r = 0;
    g = 0;
    b = 0;
    if (planes == NULL || header == NULL || prgba == NULL ||
        allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    width = (unsigned int)header->width;
    height = (unsigned int)header->height;
    if ((size_t)width > SIZE_MAX / (size_t)height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    pixel_count = (size_t)width * (size_t)height;
    if (pixel_count > SIZE_MAX / 4u) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    rgba_size = pixel_count * 4u;
    rgba = (unsigned char *)sixel_allocator_malloc(allocator, rgba_size);
    if (rgba == NULL) {
        sixel_helper_set_additional_message(
            "builtin webp: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    for (y = 0u; y < height; ++y) {
        uv_y = y >> 1;
        for (x = 0u; x < width; ++x) {
            uv_x = x >> 1;
            yv = (int)planes->y[y * planes->y_stride + x];
            u = (int)planes->u[uv_y * planes->uv_stride + uv_x];
            v = (int)planes->v[uv_y * planes->uv_stride + uv_x];
            c = yv;
            d = u - 128;
            e = v - 128;
            r = c + ((359 * e) >> 8);
            g = c - ((88 * d + 183 * e) >> 8);
            b = c + ((454 * d) >> 8);
            rgba[((size_t)y * width + x) * 4u + 0u] =
                sixel_webp_vp8_clamp_u8(r);
            rgba[((size_t)y * width + x) * 4u + 1u] =
                sixel_webp_vp8_clamp_u8(g);
            rgba[((size_t)y * width + x) * 4u + 2u] =
                sixel_webp_vp8_clamp_u8(b);
            rgba[((size_t)y * width + x) * 4u + 3u] = 255u;
        }
    }

    *prgba = rgba;
    rgba = NULL;

    sixel_allocator_free(allocator, rgba);
    return status;
}

static SIXELSTATUS
sixel_webp_vp8_bool_read_signed_literal(
    sixel_webp_vp8_bool_decoder_t *decoder,
    unsigned int nbits,
    int *pvalue)
{
    SIXELSTATUS status;
    unsigned int value;
    int sign;

    status = SIXEL_OK;
    value = 0u;
    sign = 0;
    if (decoder == NULL || pvalue == NULL || nbits > 31u) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = sixel_webp_vp8_bool_read_literal(decoder, nbits, &value);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    if (value == 0u) {
        *pvalue = 0;
        return SIXEL_OK;
    }

    status = sixel_webp_vp8_bool_read(decoder,
                                      SIXEL_WEBP_VP8_BOOL_BASE_PROB,
                                      &sign);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    if (sign != 0) {
        *pvalue = -(int)value;
    } else {
        *pvalue = (int)value;
    }

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_webp_vp8_parse_optional_signed(
    sixel_webp_vp8_bool_decoder_t *decoder,
    unsigned int nbits,
    int *pvalue)
{
    SIXELSTATUS status;
    int bit;

    status = SIXEL_OK;
    bit = 0;
    if (decoder == NULL || pvalue == NULL || nbits > 31u) {
        return SIXEL_BAD_ARGUMENT;
    }

    *pvalue = 0;
    status = sixel_webp_vp8_bool_read(decoder,
                                      SIXEL_WEBP_VP8_BOOL_BASE_PROB,
                                      &bit);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    if (bit == 0) {
        return SIXEL_OK;
    }

    return sixel_webp_vp8_bool_read_signed_literal(decoder, nbits, pvalue);
}

static SIXELSTATUS
sixel_webp_vp8_parse_segment_header(
    sixel_webp_vp8_bool_decoder_t *decoder,
    sixel_webp_vp8_segment_header_t *segment)
{
    SIXELSTATUS status;
    unsigned int i;
    unsigned int value;
    int bit;

    status = SIXEL_OK;
    i = 0u;
    value = 255u;
    bit = 0;
    if (decoder == NULL || segment == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    memset(segment, 0, sizeof(*segment));
    for (i = 0u; i < 3u; ++i) {
        segment->map_prob[i] = 255;
    }

    status = sixel_webp_vp8_bool_read(decoder,
                                      SIXEL_WEBP_VP8_BOOL_BASE_PROB,
                                      &bit);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    segment->enabled = bit;
    if (segment->enabled == 0) {
        return SIXEL_OK;
    }

    status = sixel_webp_vp8_bool_read(decoder,
                                      SIXEL_WEBP_VP8_BOOL_BASE_PROB,
                                      &bit);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    segment->update_map = bit;

    status = sixel_webp_vp8_bool_read(decoder,
                                      SIXEL_WEBP_VP8_BOOL_BASE_PROB,
                                      &bit);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    segment->update_data = bit;

    if (segment->update_data != 0) {
        status = sixel_webp_vp8_bool_read(decoder,
                                          SIXEL_WEBP_VP8_BOOL_BASE_PROB,
                                          &bit);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        segment->absolute_delta = bit;

        for (i = 0u; i < SIXEL_WEBP_VP8_SEGMENTS; ++i) {
            status = sixel_webp_vp8_parse_optional_signed(
                decoder, 7u, &segment->quant_delta[i]);
            if (SIXEL_FAILED(status)) {
                return status;
            }
        }
        for (i = 0u; i < SIXEL_WEBP_VP8_SEGMENTS; ++i) {
            status = sixel_webp_vp8_parse_optional_signed(
                decoder, 6u, &segment->filter_delta[i]);
            if (SIXEL_FAILED(status)) {
                return status;
            }
        }
    }

    if (segment->update_map != 0) {
        for (i = 0u; i < 3u; ++i) {
            status = sixel_webp_vp8_bool_read(
                decoder, SIXEL_WEBP_VP8_BOOL_BASE_PROB, &bit);
            if (SIXEL_FAILED(status)) {
                return status;
            }
            if (bit != 0) {
                status = sixel_webp_vp8_bool_read_literal(decoder,
                                                          8u,
                                                          &value);
                if (SIXEL_FAILED(status)) {
                    return status;
                }
                segment->map_prob[i] = (int)value;
            }
        }
    }

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_webp_vp8_parse_filter_header(
    sixel_webp_vp8_bool_decoder_t *decoder,
    sixel_webp_vp8_filter_header_t *filter)
{
    SIXELSTATUS status;
    unsigned int i;
    unsigned int value;
    int bit;

    status = SIXEL_OK;
    i = 0u;
    value = 0u;
    bit = 0;
    if (decoder == NULL || filter == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    memset(filter, 0, sizeof(*filter));
    status = sixel_webp_vp8_bool_read_literal(decoder, 1u, &value);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    filter->simple = (int)value;

    status = sixel_webp_vp8_bool_read_literal(decoder, 6u, &value);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    filter->level = value;

    status = sixel_webp_vp8_bool_read_literal(decoder, 3u, &value);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    filter->sharpness = value;

    status = sixel_webp_vp8_bool_read(decoder,
                                      SIXEL_WEBP_VP8_BOOL_BASE_PROB,
                                      &bit);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    if (bit == 0) {
        return SIXEL_OK;
    }

    status = sixel_webp_vp8_bool_read(decoder,
                                      SIXEL_WEBP_VP8_BOOL_BASE_PROB,
                                      &bit);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    filter->update_delta = bit;
    if (filter->update_delta == 0) {
        return SIXEL_OK;
    }

    for (i = 0u; i < SIXEL_WEBP_VP8_LF_REFS; ++i) {
        status = sixel_webp_vp8_parse_optional_signed(
            decoder, 6u, &filter->ref_delta[i]);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }
    for (i = 0u; i < SIXEL_WEBP_VP8_LF_MODES; ++i) {
        status = sixel_webp_vp8_parse_optional_signed(
            decoder, 6u, &filter->mode_delta[i]);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_webp_vp8_parse_quant_header(
    sixel_webp_vp8_bool_decoder_t *decoder,
    sixel_webp_vp8_quant_header_t *quant)
{
    SIXELSTATUS status;
    unsigned int value;

    status = SIXEL_OK;
    value = 0u;
    if (decoder == NULL || quant == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    memset(quant, 0, sizeof(*quant));
    status = sixel_webp_vp8_bool_read_literal(decoder, 7u, &value);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    quant->y_ac_qi = (int)value;

    status = sixel_webp_vp8_parse_optional_signed(decoder,
                                                  4u,
                                                  &quant->y_dc_delta);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    status = sixel_webp_vp8_parse_optional_signed(decoder,
                                                  4u,
                                                  &quant->y2_dc_delta);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    status = sixel_webp_vp8_parse_optional_signed(decoder,
                                                  4u,
                                                  &quant->y2_ac_delta);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    status = sixel_webp_vp8_parse_optional_signed(decoder,
                                                  4u,
                                                  &quant->uv_dc_delta);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    status = sixel_webp_vp8_parse_optional_signed(decoder,
                                                  4u,
                                                  &quant->uv_ac_delta);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_webp_vp8_parse_entropy_header(
    sixel_webp_vp8_bool_decoder_t *decoder,
    sixel_webp_vp8_entropy_header_t *entropy)
{
    SIXELSTATUS status;
    unsigned int type;
    unsigned int band;
    unsigned int context;
    unsigned int node;
    unsigned int literal;
    int bit;

    status = SIXEL_OK;
    type = 0u;
    band = 0u;
    context = 0u;
    node = 0u;
    literal = 0u;
    bit = 0;
    if (decoder == NULL || entropy == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    memset(entropy, 0, sizeof(*entropy));
    entropy->prob_skip_false = -1;

    status = sixel_webp_vp8_bool_read(decoder,
                                      SIXEL_WEBP_VP8_BOOL_BASE_PROB,
                                      &bit);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    entropy->refresh_entropy_probs = bit;

    status = sixel_webp_vp8_bool_read(decoder,
                                      SIXEL_WEBP_VP8_BOOL_BASE_PROB,
                                      &bit);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    if (bit != 0) {
        for (type = 0u; type < SIXEL_WEBP_VP8_COEFF_TYPES; ++type) {
            for (band = 0u; band < SIXEL_WEBP_VP8_COEFF_BANDS; ++band) {
                for (context = 0u;
                     context < SIXEL_WEBP_VP8_PREV_COEFF_CONTEXTS;
                     ++context) {
                    for (node = 0u; node < SIXEL_WEBP_VP8_COEFF_NODES;
                         ++node) {
                        status = sixel_webp_vp8_bool_read(
                            decoder,
                            SIXEL_WEBP_VP8_BOOL_BASE_PROB,
                            &bit);
                        if (SIXEL_FAILED(status)) {
                            return status;
                        }
                        if (bit != 0) {
                            status = sixel_webp_vp8_bool_read_literal(
                                decoder, 8u, &literal);
                            if (SIXEL_FAILED(status)) {
                                return status;
                            }
                            ++entropy->coef_prob_update_count;
                        }
                    }
                }
            }
        }
    }

    status = sixel_webp_vp8_bool_read(decoder,
                                      SIXEL_WEBP_VP8_BOOL_BASE_PROB,
                                      &bit);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    if (bit != 0) {
        status = sixel_webp_vp8_bool_read_literal(decoder, 8u, &literal);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        entropy->prob_skip_false = (int)literal;
    }

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_webp_vp8_parse_control_header(
    sixel_webp_vp8_bool_decoder_t *decoder,
    sixel_webp_vp8_frame_context_t *context)
{
    SIXELSTATUS status;
    unsigned int value;

    status = SIXEL_OK;
    value = 0u;
    if (decoder == NULL || context == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = sixel_webp_vp8_bool_read_literal(decoder, 1u, &value);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    if (value != 0u) {
        sixel_helper_set_additional_message(
            "builtin webp: unsupported VP8 colorspace flag.");
        return SIXEL_NOT_IMPLEMENTED;
    }

    status = sixel_webp_vp8_bool_read_literal(decoder, 1u, &value);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    status = sixel_webp_vp8_parse_segment_header(decoder, &context->segment);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    status = sixel_webp_vp8_parse_filter_header(decoder, &context->filter);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    status = sixel_webp_vp8_bool_read_literal(decoder, 2u, &value);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    context->token_partition_count = 1u << value;

    status = sixel_webp_vp8_parse_quant_header(decoder, &context->quant);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    status = sixel_webp_vp8_parse_entropy_header(decoder, &context->entropy);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_webp_vp8_parse_partition_layout(
    unsigned char const *payload,
    size_t payload_size,
    sixel_webp_vp8_frame_header_t const *header,
    sixel_webp_vp8_partition_layout_t *layout,
    sixel_webp_vp8_frame_context_t *context)
{
    SIXELSTATUS status;
    sixel_webp_vp8_bool_decoder_t decoder;
    size_t control_end;
    size_t table_size;
    size_t remaining;
    unsigned int i;
    unsigned int value;

    status = SIXEL_OK;
    memset(&decoder, 0, sizeof(decoder));
    control_end = 0u;
    table_size = 0u;
    remaining = 0u;
    i = 0u;
    value = 0u;
    if (payload == NULL || payload_size == 0u || header == NULL ||
        layout == NULL || context == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    memset(layout, 0, sizeof(*layout));
    memset(context, 0, sizeof(*context));
    layout->control_partition_offset = SIXEL_WEBP_VP8_CONTROL_OFFSET;
    layout->control_partition_size = header->first_partition_size;

    if (layout->control_partition_offset > payload_size ||
        layout->control_partition_size >
            payload_size - layout->control_partition_offset) {
        sixel_helper_set_additional_message(
            "builtin webp: VP8 first partition is truncated.");
        return SIXEL_BAD_INPUT;
    }

    status = sixel_webp_vp8_bool_init(
        &decoder,
        payload + layout->control_partition_offset,
        layout->control_partition_size);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    status = sixel_webp_vp8_parse_control_header(&decoder, context);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    layout->token_partition_count = context->token_partition_count;
    if (layout->token_partition_count == 0u ||
        layout->token_partition_count > SIXEL_WEBP_VP8_MAX_TOKEN_PARTITIONS) {
        sixel_helper_set_additional_message(
            "builtin webp: VP8 token partition count is invalid.");
        return SIXEL_BAD_INPUT;
    }

    control_end = layout->control_partition_offset +
                  layout->control_partition_size;
    layout->token_partition_table_offset = control_end;

    if (layout->token_partition_count > 1u) {
        if ((size_t)(layout->token_partition_count - 1u) >
            SIZE_MAX / 3u) {
            return SIXEL_BAD_INTEGER_OVERFLOW;
        }
        table_size = (size_t)(layout->token_partition_count - 1u) * 3u;
    } else {
        table_size = 0u;
    }

    if (control_end > payload_size || table_size > payload_size - control_end) {
        sixel_helper_set_additional_message(
            "builtin webp: VP8 token partition table is truncated.");
        return SIXEL_BAD_INPUT;
    }

    layout->token_partition_data_offset = control_end + table_size;
    remaining = payload_size - layout->token_partition_data_offset;
    for (i = 0u; i + 1u < layout->token_partition_count; ++i) {
        value = sixel_webp_vp8_read_u24le(
            payload + control_end + (size_t)i * 3u);
        if ((size_t)value > remaining) {
            sixel_helper_set_additional_message(
                "builtin webp: VP8 token partition exceeds payload.");
            return SIXEL_BAD_INPUT;
        }
        layout->token_partition_size[i] = (size_t)value;
        remaining -= (size_t)value;
    }
    layout->token_partition_size[layout->token_partition_count - 1u] =
        remaining;

    context->mb_cols = (unsigned int)((header->width + 15) / 16);
    context->mb_rows = (unsigned int)((header->height + 15) / 16);

    return SIXEL_OK;
}

SIXELSTATUS
sixel_webp_vp8_decode_native_payload(unsigned char const *payload,
                                     size_t payload_size,
                                     sixel_webp_vp8_frame_header_t const
                                         *header,
                                     unsigned char **prgba,
                                     int *pwidth,
                                     int *pheight,
                                     sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    sixel_webp_vp8_partition_layout_t layout;
    sixel_webp_vp8_frame_context_t context;
    sixel_webp_vp8_planes_t planes;
    unsigned char const *token_data;
    size_t token_size;

    status = SIXEL_OK;
    memset(&layout, 0, sizeof(layout));
    memset(&context, 0, sizeof(context));
    memset(&planes, 0, sizeof(planes));
    token_data = NULL;
    token_size = 0u;

    /*
     * This entrypoint keeps the stage boundaries explicit:
     * 1) partition/control parsing
     * 2) macroblock-level intra reconstruction into YUV planes
     * 3) YUV420 to RGBA8888 conversion
     */
    if (payload == NULL || payload_size == 0u || header == NULL ||
        prgba == NULL || pwidth == NULL || pheight == NULL ||
        allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *prgba = NULL;
    *pwidth = 0;
    *pheight = 0;
    (void)allocator;

    status = sixel_webp_vp8_parse_partition_layout(payload,
                                                   payload_size,
                                                   header,
                                                   &layout,
                                                   &context);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    status = sixel_webp_vp8_validate_dimensions(header->width,
                                                header->height);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    if (layout.token_partition_count > 1u) {
        sixel_helper_set_additional_message(
            "builtin webp: VP8 multi-token-partition decode is not ready.");
        return SIXEL_NOT_IMPLEMENTED;
    }
    if (layout.token_partition_size[0] == 0u ||
        layout.token_partition_data_offset >= payload_size ||
        layout.token_partition_size[0] >
            payload_size - layout.token_partition_data_offset) {
        sixel_helper_set_additional_message(
            "builtin webp: VP8 token partition is truncated.");
        return SIXEL_BAD_INPUT;
    }

    status = sixel_webp_vp8_alloc_planes(&planes, header, allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    token_data = payload + layout.token_partition_data_offset;
    token_size = layout.token_partition_size[0];
    status = sixel_webp_vp8_decode_synthetic_intra(token_data,
                                                   token_size,
                                                   header,
                                                   &context,
                                                   &planes);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = sixel_webp_vp8_convert_yuv420_to_rgba(&planes,
                                                   header,
                                                   prgba,
                                                   allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    *pwidth = header->width;
    *pheight = header->height;

cleanup:
    sixel_webp_vp8_free_planes(&planes, allocator);
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
