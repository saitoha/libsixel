/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 * Copyright (c) 2014-2016 Hayaki Saito
 * Copyright (c) 2014 kmiya@culti
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
 * This file is derived from "sixel" original version (2014-3-2)
 * http://nanno.dip.jp/softlib/man/rlogin/sixel.tar.gz
 *
 */

/*
 * Initial developer of this file is kmiya@culti.
 *
 * He distributes it under very permissive license which permits
 * using, copying, modification, redistribution, and all other
 * public activities without any restrictions.
 *
 * He declares this is compatible with MIT/BSD/GPL.
 *
 * Hayaki Saito <saitoha@me.com> modified this and re-licensed
 * it to the MIT license.
 */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

/* STDC_HEADERS */
#include <stdlib.h>
#include <stdio.h>

#if HAVE_CTYPE_H
# include <ctype.h>   /* isdigit */
#endif  /* HAVE_CTYPE_H */
#if HAVE_STRING_H
# include <string.h>  /* memcpy */
#endif  /* HAVE_STRING_H */
#if HAVE_LIMITS_H
# include <limits.h>
#endif  /* HAVE_LIMITS_H */
#if HAVE_INTTYPES_H
# include <inttypes.h>
#endif  /* HAVE_INTTYPES_H */
#if HAVE_ASSERT_H
# include <assert.h>
#endif  /* HAVE_ASSERT_H */

#include <sixel.h>
#include "output.h"
#include "colorspace.h"
#include "decoder-image.h"
#include "decoder-ormode.h"
#include "decoder-columns.h"
#include "decoder.h"
#include "decoder-parallel.h"
#include "sixel_decode_pixels.h"
#include "timeline-logger.h"

#define SIXEL_RGB(r, g, b) (((r) << 16) + ((g) << 8) +  (b))

#define PALVAL(n,a,m) (((n) * (a) + ((m) / 2)) / (m))

#define SIXEL_XRGB(r,g,b) SIXEL_RGB(PALVAL(r, 255, 100), PALVAL(g, 255, 100), PALVAL(b, 255, 100))

#define DECSIXEL_PARAMS_MAX 16
#define SIXEL_PALETTE_MAX_DECODER 65536
typedef unsigned char sixel_decoder_index_t;

static int const sixel_default_color_table[] = {
    SIXEL_XRGB(0,  0,  0),   /*  0 Black    */
    SIXEL_XRGB(20, 20, 80),  /*  1 Blue     */
    SIXEL_XRGB(80, 13, 13),  /*  2 Red      */
    SIXEL_XRGB(20, 80, 20),  /*  3 Green    */
    SIXEL_XRGB(80, 20, 80),  /*  4 Magenta  */
    SIXEL_XRGB(20, 80, 80),  /*  5 Cyan     */
    SIXEL_XRGB(80, 80, 20),  /*  6 Yellow   */
    SIXEL_XRGB(53, 53, 53),  /*  7 Gray 50% */
    SIXEL_XRGB(26, 26, 26),  /*  8 Gray 25% */
    SIXEL_XRGB(33, 33, 60),  /*  9 Blue*    */
    SIXEL_XRGB(60, 26, 26),  /* 10 Red*     */
    SIXEL_XRGB(33, 60, 33),  /* 11 Green*   */
    SIXEL_XRGB(60, 33, 60),  /* 12 Magenta* */
    SIXEL_XRGB(33, 60, 60),  /* 13 Cyan*    */
    SIXEL_XRGB(60, 60, 33),  /* 14 Yellow*  */
    SIXEL_XRGB(80, 80, 80),  /* 15 Gray 75% */
};

/*
 * Map source coordinates when a palette value is created. Keeping this at
 * the definition boundary preserves paint-time colors and lets parallel
 * workers and OR-mode consume the same already-mapped palette.
 */
static int sixel_palette_map_rgb(int color,
                                 sixel_palette_transform_t const *transform,
                                 int is_default)
{
    unsigned char rgb[3];

    if (transform == NULL) {
        return color;
    }
    rgb[0] = (unsigned char)((color >> 16) & 0xff);
    rgb[1] = (unsigned char)((color >> 8) & 0xff);
    rgb[2] = (unsigned char)(color & 0xff);
    transform->map_rgb(transform->context, rgb, is_default);
    return SIXEL_RGB(rgb[0], rgb[1], rgb[2]);
}

/*
 * Store a single pixel in the image buffer. When the decoder is in direct
 * color mode the palette index is translated into an RGBA quadruplet with an
 * opaque alpha channel. Indexed mode keeps the original palette entry so the
 * caller can compose a palette later.
 */
static void
image_buffer_store_pixel(image_buffer_t *image, size_t pos, int color_index)
{
    unsigned char *bytes;
    int color;
    int depth;

    depth = image->depth;
    if (color_index < 0 || color_index >= SIXEL_PALETTE_MAX_DECODER) {
        return;
    }

    if (depth == 1U) {
        image->pixels.in_bytes[pos] = (unsigned char)color_index;
    } else if (depth == 2U) {
        image->pixels.in_shorts[pos] = (unsigned short)color_index;
    } else {  /* rgba */
        color = image->palette[color_index];
        bytes = image->pixels.in_bytes + pos * 4U;
        bytes[0] = (unsigned char)((color >> 16) & 0xff);
        bytes[1] = (unsigned char)((color >> 8) & 0xff);
        bytes[2] = (unsigned char)(color & 0xff);
        bytes[3] = 255u;
    }
}

static void
image_buffer_mark_painted_span(image_buffer_t *image,
                               size_t pos,
                               int repeat)
{
    if (image->paint_mask == NULL || repeat <= 0) {
        return;
    }

    memset(image->paint_mask + pos, 0xff, (size_t)repeat);
}

/*
 * Fill a horizontal run starting at (x, y) with the requested palette index.
 * Direct color output expands the span into RGBA bytes to keep the alpha
 * channel and the color components consistent with single pixel writes.
 */
static void
image_buffer_fill_span(image_buffer_t *image,
                       int y,
                       int x,
                       int repeat,
                       int color_index)
{
    size_t pos;
    int n;

    pos = (size_t)image->width * (size_t)y + (size_t)x;

    if (image->depth == 1U) {
        memset(image->pixels.in_bytes + pos, color_index, (size_t)repeat);
    } else {
        for (n = 0; n < repeat; ++n) {
            image_buffer_store_pixel(image, pos + (size_t)n, color_index);
        }
    }
    image_buffer_mark_painted_span(image, pos, repeat);
}

static void
image_buffer_release_paint_mask(image_buffer_t *image,
                                sixel_allocator_t *allocator)
{
    if (image == NULL || image->paint_mask == NULL) {
        return;
    }

    if (allocator != NULL) {
        sixel_allocator_free(allocator, image->paint_mask);
    } else {
        free(image->paint_mask);
    }
    image->paint_mask = NULL;
}

static void
image_buffer_release_ormode_planes(image_buffer_t *image,
                                   sixel_allocator_t *allocator)
{
    if (image == NULL) {
        return;
    }
    if (allocator != NULL) {
        sixel_allocator_free(allocator, image->ormode_planes);
        sixel_allocator_free(allocator, image->ormode_high_planes);
    } else {
        free(image->ormode_planes);
        free(image->ormode_high_planes);
    }
    image->ormode_planes = NULL;
    image->ormode_high_planes = NULL;
}

static void
image_buffer_release_columns(image_buffer_t *image,
                              sixel_allocator_t *allocator)
{
    if (image == NULL) {
        return;
    }
    if (allocator != NULL) {
        sixel_allocator_free(allocator, image->normal_columns);
    } else {
        free(image->normal_columns);
    }
    image->normal_columns = NULL;
}

/* Select depth and mask handling outside the pixel loops. Blending with the
 * initialized destination preserves its exact unpainted sentinel/background,
 * including the distinct byte and wide-index representations. RGBA lookup
 * words are assembled with memcpy so their byte order is host independent. */
SIXEL_INTERNAL_API void
image_buffer_finalize_normal_bands(image_buffer_t *image, int first, int end)
{
    uint64_t const *columns;
    uint32_t lut[256];
    uint32_t value;
    uint32_t mask;
    unsigned char rgba[4];
    unsigned char *bytes;
    unsigned char *coverage;
    unsigned short *shorts;
    size_t width;
    size_t row;
    int band;
    int rows;
    int r;
    int shift;
    int color;
    int x;

    if (image->normal_columns == NULL) {
        return;
    }
    width = (size_t)image->width;
    if (image->depth == 4) {
        for (x = 0; x < 256; ++x) {
            color = image->palette[x];
            rgba[0] = (unsigned char)(color >> 16);
            rgba[1] = (unsigned char)(color >> 8);
            rgba[2] = (unsigned char)color;
            rgba[3] = 255u;
            memcpy(&lut[x], rgba, sizeof(rgba));
        }
    }
    for (band = first; band < end; ++band) {
        columns = image->normal_columns + (size_t)band * width;
        rows = image->height - band * 6;
        if (rows > 6) {
            rows = 6;
        }
        for (r = 0; r < rows; ++r) {
            row = ((size_t)band * 6u + (size_t)r) * width;
            shift = r * 8;
            if (image->depth == 1) {
                bytes = image->pixels.in_bytes + row;
                for (x = 0; x < image->width; ++x) {
                    mask = 0u - (uint32_t)((columns[x] >> (48 + r)) & 1u);
                    value = (unsigned char)(columns[x] >> shift);
                    bytes[x] = (unsigned char)((bytes[x] & ~mask) |
                                               (value & mask));
                }
            } else if (image->depth == 2) {
                shorts = image->pixels.in_shorts + row;
                for (x = 0; x < image->width; ++x) {
                    mask = 0u - (uint32_t)((columns[x] >> (48 + r)) & 1u);
                    value = (unsigned char)(columns[x] >> shift);
                    shorts[x] = (unsigned short)((shorts[x] & ~mask) |
                                                 (value & mask));
                }
            } else {
                bytes = image->pixels.in_bytes + row * 4u;
                for (x = 0; x < image->width; ++x) {
                    mask = 0u - (uint32_t)((columns[x] >> (48 + r)) & 1u);
                    memcpy(&value, bytes + (size_t)x * 4u, sizeof(value));
                    value = (value & ~mask) |
                        (lut[(columns[x] >> shift) & 255u] & mask);
                    memcpy(bytes + (size_t)x * 4u, &value, sizeof(value));
                }
            }
            if (image->paint_mask != NULL) {
                coverage = image->paint_mask + row;
                for (x = 0; x < image->width; ++x) {
                    coverage[x] |= (unsigned char)(0u -
                        (unsigned int)((columns[x] >> (48 + r)) & 1u));
                }
            }
        }
    }
}

/* Materialization is one-way: later paint must retain palette history and
 * any pixels written by the ordinary or parallel painter. */
static void
image_buffer_materialize_columns(image_buffer_t *image,
                                  sixel_allocator_t *allocator)
{
    if (image->normal_columns != NULL) {
        image_buffer_finalize_normal_bands(image, 0, (image->height + 5) / 6);
        image_buffer_release_columns(image, allocator);
    }
}

static SIXELSTATUS
image_buffer_enable_paint_mask(image_buffer_t *image,
                               sixel_allocator_t *allocator);

/* The existing dimension limits also bound the rounded band count. Check
 * the byte product independently for 32-bit hosts before allocating. */
static uint64_t *
image_buffer_allocate_planes(int width, int height,
                             sixel_allocator_t *allocator)
{
    size_t bands;
    size_t count;
    uint64_t *planes;

    bands = ((size_t)height + 5u) / 6u;
    if ((size_t)width > (size_t)-1 / sizeof(*planes) / bands) {
        return NULL;
    }
    count = (size_t)width * bands;
    planes = (uint64_t *)sixel_allocator_calloc(allocator, count,
                                               sizeof(*planes));
    return planes;
}

SIXEL_INTERNAL_API SIXELSTATUS
image_buffer_enable_ormode(image_buffer_t *image,
                           sixel_allocator_t *allocator)
{
    if (image == NULL || image->pixels.p == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    image_buffer_release_ormode_planes(image, allocator);
    image_buffer_release_columns(image, allocator);
    image->ormode_planes = image_buffer_allocate_planes(
        image->width, image->height, allocator);
    if (image->ormode_planes == NULL) {
        sixel_helper_set_additional_message(
            "image_buffer_enable_ormode: allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }
    return SIXEL_OK;
}

/* Resolve only after the final palette definition and final raster resize.
 * Index zero is opaque, including untouched cells. No paint-mask writes or
 * per-pixel maxima are needed while accumulating the received planes.
 *
 * Rows are written front to back so each output row is one sequential
 * stream.  The common shapes (byte indices or RGBA from the low eight planes)
 * get their own loops; the per-pixel depth and upper-plane tests stay in the
 * general loop for 16-bit output and wide registers. */
SIXEL_INTERNAL_API void
image_buffer_finalize_ormode(image_buffer_t *image)
{
    uint64_t const *low;
    uint64_t const *high;
    unsigned char lut[256][4];
    unsigned char *bytes;
    unsigned char *dst;
    unsigned int index;
    unsigned int max_index;
    size_t width;
    size_t row;
    int band;
    int bands;
    int rows;
    int color;
    int r;
    int x;

    if (image == NULL || image->ormode_planes == NULL) {
        return;
    }
    width = (size_t)image->width;
    bands = (image->height + 5) / 6;
    max_index = 0u;
    if (image->depth == 4U && image->ormode_high_planes == NULL) {
        for (x = 0; x < 256; ++x) {
            color = image->palette[x];
            lut[x][0] = (unsigned char)(color >> 16);
            lut[x][1] = (unsigned char)(color >> 8);
            lut[x][2] = (unsigned char)color;
            lut[x][3] = 255u;
        }
    }
    for (band = 0; band < bands; ++band) {
        low = image->ormode_planes + (size_t)band * width;
        high = image->ormode_high_planes == NULL ? NULL :
            image->ormode_high_planes + (size_t)band * width;
        rows = image->height - band * 6;
        if (rows > 6) {
            rows = 6;
        }
        for (r = 0; r < rows; ++r) {
            row = ((size_t)band * 6u + (size_t)r) * width;
            if (high == NULL && image->depth == 1U) {
                bytes = image->pixels.in_bytes + row;
                for (x = 0; x < image->width; ++x) {
                    index = sixel_ormode_gather(low[x], r);
                    max_index = index > max_index ? index : max_index;
                    bytes[x] = (unsigned char)index;
                }
            } else if (high == NULL && image->depth == 4U) {
                bytes = image->pixels.in_bytes + row * 4u;
                for (x = 0; x < image->width; ++x) {
                    index = sixel_ormode_gather(low[x], r);
                    max_index = index > max_index ? index : max_index;
                    memcpy(bytes + (size_t)x * 4u, lut[index], 4u);
                }
            } else {
                for (x = 0; x < image->width; ++x) {
                    index = sixel_ormode_gather(low[x], r);
                    if (high != NULL) {
                        index |= sixel_ormode_gather(high[x], r) << 8;
                    }
                    max_index = index > max_index ? index : max_index;
                    if (image->depth == 1U) {
                        image->pixels.in_bytes[row + (size_t)x] =
                            (unsigned char)index;
                    } else if (image->depth == 2U) {
                        image->pixels.in_shorts[row + (size_t)x] =
                            (unsigned short)index;
                    } else {
                        color = image->palette[index];
                        dst = image->pixels.in_bytes +
                            (row + (size_t)x) * 4u;
                        dst[0] = (unsigned char)(color >> 16);
                        dst[1] = (unsigned char)(color >> 8);
                        dst[2] = (unsigned char)color;
                        dst[3] = 255u;
                    }
                }
            }
        }
    }
    /* Retain the existing palette-definition/default floor as well as every
     * composed index. A register need not be selected itself to be produced. */
    if ((int)max_index + 1 > image->ncolors) {
        image->ncolors = (int)max_index + 1;
    }
    if (image->paint_mask != NULL) {
        memset(image->paint_mask, 0xff,
               (size_t)image->width * image->height);
    }
}

typedef enum parse_state {
    PS_GROUND     = 0,
    PS_ESC        = 1,  /* ESC */
    PS_DCS        = 2,  /* DCS Device Control String Introducer \033P P...P I...I F */
    PS_DECSIXEL   = 3,  /* DECSIXEL body part ", $, -, ? ... ~ */
    PS_DECGRA     = 4,  /* DECGRA Set Raster Attributes " Pan; Pad; Ph; Pv */
    PS_DECGRI     = 5,  /* DECGRI Graphics Repeat Introducer ! Pn Ch */
    PS_DECGCI     = 6   /* DECGCI Graphics Color Introducer # Pc; Pu; Px; Py; Pz */
} parse_state_t;

typedef struct parser_context {
    parse_state_t state;
    sixel_palette_transform_t const *palette_transform;
    int pos_x;
    int pos_y;
    int max_x;
    int max_y;
    int attributed_pan;
    int attributed_pad;
    int attributed_ph;
    int attributed_pv;
    int repeat_count;
    int color_index;
    int bgindex;
    int ormode;
    uint64_t ormode_spread;
    uint64_t ormode_high_spread;
    uint64_t normal_color;
    int normal_disabled;
    int painted_outside_raster;
    int painted;
    int palette_redefined_after_paint;
    /* Sequence composition keeps one canvas; parallel workers own a packet. */
    int sequence;
    int param;
    int nparams;
    int params[DECSIXEL_PARAMS_MAX];
} parser_context_t;


/* Consume adjacent unrepeated data bytes with one plane-word store each.
 * The caller has proved a complete band fits the allocated raster. Stop at
 * the right edge so the ordinary parser still owns growth and clipping. */
static unsigned char *
parser_context_store_ormode_run(parser_context_t *context,
                                image_buffer_t *image,
                                unsigned char *p, unsigned char *end)
{
    uint64_t *low;
    uint64_t *high;
    uint64_t spread;
    uint64_t high_spread;
    size_t band;
    int x;
    int last;
    int bits;
    int rows;
    int r;

    band = (size_t)(context->pos_y / 6) * (size_t)image->width;
    low = image->ormode_planes + band;
    high = image->ormode_high_planes == NULL ? NULL :
        image->ormode_high_planes + band;
    spread = context->ormode_spread;
    high_spread = context->ormode_high_spread;
    x = context->pos_x;
    last = -1;
    rows = 0;
    while (p < end && *p >= '?' && *p <= '~' && x < image->width) {
        bits = *p++ - '?';
        low[x] |= (uint64_t)bits * spread;
        if (high_spread != 0) {
            high[x] |= (uint64_t)bits * high_spread;
        }
        if (bits != 0) {
            last = x;
            rows |= bits;
        }
        ++x;
    }
    context->pos_x = x;
    if (last > context->max_x) {
        context->max_x = last;
    }
    for (r = 5; r >= 0; --r) {
        if ((rows & (1 << r)) != 0) {
            if (context->pos_y + r > context->max_y) {
                context->max_y = context->pos_y + r;
            }
            break;
        }
    }
    return p;
}


/* Consume a contiguous run without returning to the control-state switch.
 * Full-band bounds have already been checked. Aggregate paint extents once
 * per run; an unset sixel advances x but neither paints nor grows extents. */
static unsigned char *
parser_context_store_normal_run(parser_context_t *context,
                                image_buffer_t *image,
                                unsigned char *p, unsigned char *end)
{
    uint64_t *columns;
    uint64_t color;
    uint64_t mask;
    int x;
    int last;
    int rows;
    int bits;
    int r;

    columns = image->normal_columns +
        (size_t)(context->pos_y / 6) * (size_t)image->width;
    color = context->normal_color;
    x = context->pos_x;
    last = -1;
    rows = 0;
    if (image->depth != 4 && context->color_index > image->ncolors) {
        image->ncolors = context->color_index;
    }
    while (p < end && *p >= '?' && *p <= '~' && x < image->width) {
        bits = *p++ - '?';
        mask = sixel_column_row_masks[bits];
        columns[x] = (columns[x] & ~mask) | (color & mask) |
            ((uint64_t)bits << 48);
        if (bits != 0) {
            last = x;
            rows |= bits;
        }
        ++x;
    }
    context->pos_x = x;
    if (last > context->max_x) {
        context->max_x = last;
    }
    for (r = 5; r >= 0; --r) {
        if ((rows & (1 << r)) != 0) {
            if (context->pos_y + r > context->max_y) {
                context->max_y = context->pos_y + r;
            }
            break;
        }
    }
    return p;
}

/*
 * Primary color hues:
 *  blue:    0 degrees
 *  red:   120 degrees
 *  green: 240 degrees
 */
static int
hls_to_rgb(int hue, int lum, int sat)
{
    int hls[3];
    unsigned char rgb[3];

    hls[0] = hue;
    hls[1] = lum;
    hls[2] = sat;
    sixel_color_hls_to_rgb101(rgb, hls);
    return SIXEL_XRGB(rgb[0], rgb[1], rgb[2]);
}


SIXEL_INTERNAL_API SIXELSTATUS
image_buffer_init(
    image_buffer_t        *image,
    int                    width,
    int                    height,
    int                    bgindex,
    int                    depth,
    int                    keep_paint_mask,
    sixel_allocator_t     *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    size_t size;
    size_t stride;
    int i;
    int n;
    int r;
    int g;
    int b;

    /* check parameters */
    if (width <= 0) {
        sixel_helper_set_additional_message(
            "image_buffer_init: an invalid width parameter detected.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    if (height <= 0) {
        sixel_helper_set_additional_message(
            "image_buffer_init: an invalid width parameter detected.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    if (width > SIXEL_WIDTH_LIMIT) {
        sixel_helper_set_additional_message(
            "image_buffer_init: given width parameter is too huge.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    if (height > SIXEL_HEIGHT_LIMIT) {
        sixel_helper_set_additional_message(
            "image_buffer_init: given height parameter is too huge.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    image->depth = depth;
    stride = (size_t)width * depth;
    size = stride * (size_t)height;
    image->width = width;
    image->height = height;
    image->paint_mask = NULL;
    image->ormode_planes = NULL;
    image->ormode_high_planes = NULL;
    image->normal_columns = NULL;
    image->pixels.p = (unsigned char *)sixel_allocator_malloc(allocator, size);
    if (depth == 4U) {
        image->ncolors = (-1);
    } else {
        image->ncolors = 2;
    }

    if (image->pixels.p == NULL) {
        sixel_helper_set_additional_message(
            "sixel_deocde_raw: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    if (depth == 4U) {
        memset(image->pixels.p, 0U, size);
    } else {
        if (depth == 1U) {
            memset(image->pixels.p, bgindex, size);
        } else {  /* 2U */
            for (n = 0; n < width * height; ++n) {
                image_buffer_store_pixel(image, n, bgindex);
            }
        }
    }

    /* Every output depth uses the same defined initial palette. */
    for (n = 0; n < 16; n++) {
        image->palette[n] = sixel_default_color_table[n];
    }

    /* colors 16-231 are a 6x6x6 color cube */
    for (r = 0; r < 6; r++) {
        for (g = 0; g < 6; g++) {
            for (b = 0; b < 6; b++) {
                image->palette[n++] = SIXEL_RGB(r * 51, g * 51, b * 51);
            }
        }
    }

    /* colors 232-255 are a grayscale ramp, intentionally leaving out */
    for (i = 0; i < 24; i++) {
        image->palette[n++] = SIXEL_RGB(i * 11, i * 11, i * 11);
    }

#if HAVE_ASSERT
    assert(n == 256);
#endif  /* HAVE_ASSERT */

    for (n = 256; n < SIXEL_PALETTE_MAX_DECODER; n++) {
        image->palette[n] = SIXEL_RGB(255, 255, 255);
    }
    if (keep_paint_mask) {
        status = image_buffer_enable_paint_mask(image, allocator);
        if (SIXEL_FAILED(status)) {
            sixel_allocator_free(allocator, image->pixels.p);
            image->pixels.p = NULL;
            goto end;
        }
    }
    status = SIXEL_OK;

end:
    return status;
}

static SIXELSTATUS
image_buffer_enable_paint_mask(image_buffer_t *image,
                               sixel_allocator_t *allocator)
{
    size_t pixels;

    if (image == NULL || image->width <= 0 || image->height <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    if ((size_t)image->width > ((size_t)-1 / (size_t)image->height)) {
        return SIXEL_BAD_ALLOCATION;
    }
    if (image->paint_mask != NULL) {
        return SIXEL_OK;
    }

    pixels = (size_t)image->width * (size_t)image->height;
    image->paint_mask = (unsigned char *)sixel_allocator_malloc(allocator,
                                                                pixels);
    if (image->paint_mask == NULL) {
        sixel_helper_set_additional_message(
            "image_buffer_enable_paint_mask: "
            "sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }
    memset(image->paint_mask, 0, pixels);

    return SIXEL_OK;
}

SIXELSTATUS
image_buffer_resize(
    image_buffer_t        *image,
    int                    width,
    int                    height,
    int                    bgindex,
    sixel_allocator_t     *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    size_t size;
    unsigned char *alt_buffer;
    unsigned char *alt_paint_mask;
    uint64_t *alt_planes;
    uint64_t *alt_high_planes;
    uint64_t *alt_columns;
    size_t bands;
    size_t index_size;
    int n;
    int min_height;
    int copy_width;
    size_t stride;
    size_t old_stride;
    size_t copy_stride;
    size_t depth = image->depth;

    alt_paint_mask = NULL;

    /* check parameters */
    if (width <= 0) {
        sixel_helper_set_additional_message(
            "image_buffer_init: an invalid width parameter detected.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    if (height <= 0) {
        sixel_helper_set_additional_message(
            "image_buffer_init: an invalid width parameter detected.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    if (height > SIXEL_HEIGHT_LIMIT) {
        sixel_helper_set_additional_message(
            "image_buffer_init: given height parameter is too huge.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    if (width > SIXEL_WIDTH_LIMIT) {
        sixel_helper_set_additional_message(
            "image_buffer_init: given width parameter is too huge.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    if (height > SIXEL_HEIGHT_LIMIT) {
        sixel_helper_set_additional_message(
            "image_buffer_init: given height parameter is too huge.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    stride = (size_t)width * depth;
    size = stride * (size_t)height;
    alt_planes = NULL;
    alt_high_planes = NULL;
    alt_columns = NULL;
    alt_buffer = (sixel_decoder_index_t *)
        sixel_allocator_malloc(allocator, size);
    if (alt_buffer == NULL || size == 0) {
        /* free source image */
        sixel_allocator_free(allocator, image->pixels.p);
        image->pixels.p = NULL;
        sixel_helper_set_additional_message(
            "image_buffer_resize: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    if (image->ormode_planes != NULL) {
        alt_planes = image_buffer_allocate_planes(width, height, allocator);
        if (image->ormode_high_planes != NULL) {
            alt_high_planes = image_buffer_allocate_planes(
                width, height, allocator);
        }
        if (alt_planes == NULL || (image->ormode_high_planes != NULL &&
                alt_high_planes == NULL)) {
            sixel_allocator_free(allocator, alt_buffer);
            sixel_allocator_free(allocator, alt_planes);
            sixel_allocator_free(allocator, alt_high_planes);
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
    }
    if (image->normal_columns != NULL) {
        alt_columns = image_buffer_allocate_planes(width, height, allocator);
        if (alt_columns == NULL) {
            sixel_allocator_free(allocator, alt_buffer);
            sixel_allocator_free(allocator, alt_planes);
            sixel_allocator_free(allocator, alt_high_planes);
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
    }
    if (image->paint_mask != NULL) {
        index_size = (size_t)width * (size_t)height;
        alt_paint_mask = (unsigned char *)
            sixel_allocator_malloc(allocator, index_size);
        if (alt_paint_mask == NULL) {
            sixel_allocator_free(allocator, alt_buffer);
            sixel_allocator_free(allocator, alt_planes);
            sixel_allocator_free(allocator, alt_high_planes);
            sixel_allocator_free(allocator, alt_columns);
            sixel_helper_set_additional_message(
                "image_buffer_resize: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        memset(alt_paint_mask, 0, index_size);
    }

    min_height = height > image->height ? image->height: height;
    old_stride = (size_t)image->width * depth;
    copy_width = width > image->width ? image->width: width;
    if (alt_columns != NULL) {
        bands = ((size_t)min_height + 5u) / 6u;
        for (n = 0; (size_t)n < bands; ++n) {
            memcpy(alt_columns + (size_t)width * (size_t)n,
                   image->normal_columns +
                   (size_t)image->width * (size_t)n,
                   (size_t)copy_width * sizeof(*alt_columns));
        }
    }
    if (alt_planes != NULL) {
        /* Pixels are materialized only after parsing. Copy complete band
         * words on growth or cropping, including a partial final band. */
        bands = ((size_t)min_height + 5u) / 6u;
        for (n = 0; (size_t)n < bands; ++n) {
            memcpy(alt_planes + (size_t)width * (size_t)n,
                   image->ormode_planes + (size_t)image->width * (size_t)n,
                   (size_t)copy_width * sizeof(*alt_planes));
            if (alt_high_planes != NULL) {
                memcpy(alt_high_planes + (size_t)width * (size_t)n,
                       image->ormode_high_planes +
                       (size_t)image->width * (size_t)n,
                       (size_t)copy_width * sizeof(*alt_high_planes));
            }
        }
    } else {
        if (width > image->width) {
            copy_stride = old_stride;
            for (n = 0; n < min_height; ++n) {
                memcpy(alt_buffer + stride * (size_t)n,
                       image->pixels.in_bytes + old_stride * (size_t)n,
                       copy_stride);
                if (stride > copy_stride) {
                    if (depth == 4U) {  /* rgba */
                        memset(alt_buffer + stride * (size_t)n + copy_stride,
                               0,
                               stride - copy_stride);
                    } else {
                        memset(alt_buffer + stride * (size_t)n + copy_stride,
                               bgindex,
                               stride - copy_stride);
                    }
                }
            }
        } else {
            copy_stride = stride;
            for (n = 0; n < min_height; ++n) {
                memcpy(alt_buffer + stride * (size_t)n,
                       image->pixels.in_bytes + old_stride * (size_t)n,
                       copy_stride);
            }
        }
    }
    if (alt_paint_mask != NULL) {
        for (n = 0; n < min_height; ++n) {
            memcpy(alt_paint_mask + (size_t)width * (size_t)n,
                   image->paint_mask +
                   (size_t)image->width * (size_t)n,
                   (size_t)copy_width);
        }
    }

    if (alt_planes == NULL && height > image->height) {
        if (depth == 4u) {  /* rgba */
            memset(alt_buffer + stride * (size_t)image->height,
                   0,
                   stride * (size_t)(height - image->height));
        } else {
            memset(alt_buffer + stride * (size_t)image->height,
                   bgindex,
                   stride * (size_t)(height - image->height));
        }
    }

    /* free source image */
    sixel_allocator_free(allocator, image->pixels.p);
    image_buffer_release_paint_mask(image, allocator);
    image_buffer_release_ormode_planes(image, allocator);
    image_buffer_release_columns(image, allocator);

    image->pixels.in_bytes = alt_buffer;
    image->paint_mask = alt_paint_mask;
    image->ormode_planes = alt_planes;
    image->ormode_high_planes = alt_high_planes;
    image->normal_columns = alt_columns;
    image->width = width;
    image->height = height;

    status = SIXEL_OK;

end:
    return status;
}


static SIXELSTATUS
parser_context_init(parser_context_t *context)
{
    SIXELSTATUS status = SIXEL_FALSE;

    context->state = PS_GROUND;
    context->palette_transform = NULL;
    context->pos_x = 0;
    context->pos_y = 0;
    context->max_x = 0;
    context->max_y = 0;
    context->attributed_pan = 2;
    context->attributed_pad = 1;
    context->attributed_ph = 0;
    context->attributed_pv = 0;
    context->repeat_count = 1;
    context->color_index = 15;
    context->bgindex = (-1);
    context->ormode = 0;
    context->ormode_spread = sixel_ormode_spread(15u);
    context->ormode_high_spread = 0;
    context->normal_color = sixel_column_color(15u);
    context->normal_disabled = 0;
    context->painted_outside_raster = 0;
    context->painted = 0;
    context->palette_redefined_after_paint = 0;
    context->sequence = 0;
    context->nparams = 0;
    context->param = 0;

    status = SIXEL_OK;

    return status;
}

static int
parser_context_bgindex(parser_context_t const *context)
{
    return context->ormode ? 0 : context->bgindex;
}

static int
parser_context_is_ormode_request(parser_context_t const *context)
{
    /*
     * OR mode is signaled by the non-standard P2=5 extension in
     * "DCS P1;P2;P3 q". P1 remains the sixel aspect ratio selector.
     */
    return context->nparams > 1 && context->params[1] == 5;
}


static SIXELSTATUS
safe_addition_for_params(parser_context_t *context, unsigned char *p)
{
    SIXELSTATUS status = SIXEL_FALSE;
    int x;

    x = *p - '0'; /* 0 <= x <= 9 */
    if ((context->param > INT_MAX / 10) || (x > INT_MAX - context->param * 10)) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        sixel_helper_set_additional_message(
            "safe_addition_for_params: ingeger overflow detected.");
        goto end;
    }
    context->param = context->param * 10 + x;
    status = SIXEL_OK;

end:
    return status;
}


static SIXELSTATUS
safe_multiply_by_params_div10(int lhs, int rhs, int *result)
{
    SIXELSTATUS status = SIXEL_FALSE;

    if (lhs > 0 && rhs > INT_MAX / lhs) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        sixel_helper_set_additional_message(
            "safe_multiply_by_params_div10: integer overflow detected.");
        goto end;
    }

    *result = lhs * rhs / 10;
    status = SIXEL_OK;

end:
    return status;
}

static SIXELSTATUS
parser_context_enter_decsixel(parser_context_t *context,
                              image_buffer_t *image,
                              sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    int scaled_pan;
    int scaled_pad;

    status = SIXEL_FALSE;
    scaled_pan = 0;
    scaled_pad = 0;

    if (context->nparams > 0) {
        /* Pn1 */
        switch (context->params[0]) {
        case 0:
        case 1:
            context->attributed_pad = 2;
            break;
        case 2:
            context->attributed_pad = 5;
            break;
        case 3:
        case 4:
            context->attributed_pad = 4;
            break;
        case 5:
        case 6:
            context->attributed_pad = 3;
            break;
        case 7:
        case 8:
            context->attributed_pad = 2;
            break;
        case 9:
            context->attributed_pad = 1;
            break;
        default:
            context->attributed_pad = 2;
            break;
        }
    }

    if (parser_context_is_ormode_request(context)) {
        context->ormode = 1;
        status = image_buffer_enable_ormode(image, allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        context->ormode_spread = sixel_ormode_spread(
            (unsigned int)context->color_index);
        context->ormode_high_spread = sixel_ormode_spread(
            (unsigned int)context->color_index >> 8);
        if (context->ormode_high_spread != 0) {
            image->ormode_high_planes = image_buffer_allocate_planes(
                image->width, image->height, allocator);
            if (image->ormode_high_planes == NULL) {
                status = SIXEL_BAD_ALLOCATION;
                goto end;
            }
        }
    }

    if (context->nparams > 2) {
        /* Pn3 */
        if (context->params[2] == 0) {
            context->params[2] = 10;
        }

        status = safe_multiply_by_params_div10(context->attributed_pan,
                                               context->params[2],
                                               &scaled_pan);
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        status = safe_multiply_by_params_div10(context->attributed_pad,
                                               context->params[2],
                                               &scaled_pad);
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        context->attributed_pan = scaled_pan;
        context->attributed_pad = scaled_pad;
        if (context->attributed_pan <= 0) {
            context->attributed_pan = 1;
        }
        if (context->attributed_pad <= 0) {
            context->attributed_pad = 1;
        }
    }

    context->nparams = 0;
    context->state = PS_DECSIXEL;
    status = SIXEL_OK;

end:
    return status;
}


/* convert sixel data into indexed pixel bytes and palette data */
SIXELAPI SIXELSTATUS
sixel_decode_raw_impl(
    unsigned char     *p,         /* sixel bytes */
    int                len,       /* size of sixel bytes */
    image_buffer_t    *image,
    parser_context_t  *context,
    sixel_allocator_t *allocator, /* allocator object */
    sixel_timeline_logger_t    *logger,
    int logger_prepared,
    unsigned int decode_flags,
    sixel_decoder_undither_context_t *undither)
{
    SIXELSTATUS status = SIXEL_FALSE;
    int n;
    int i;
    int y;
    int bits;
    int sixel_vertical_mask;
    int sx;
    int sy;
    int c;
    int trust_raster_size;
    int draw_repeat;
    int target_bottom;
    int drawn_y_end;
    int store_bits;
    size_t pos;
    unsigned char *p0 = p;
#if SIXEL_ENABLE_THREADS
    int parallel_started = 0;
    int raster_ready = 0;
    int palette_ready = 0;
    unsigned char *parallel_anchor = p;
#else
    (void) logger;
    (void) logger_prepared;
    (void) undither;
#endif  /* SIXEL_ENABLE_THREADS */

    trust_raster_size = (decode_flags &
        SIXEL_DECODE_PIXELS_OPTION_TRUST_RASTER_SIZE) != 0U;

    while (p < p0 + len) {
        switch (context->state) {
        case PS_GROUND:
            switch (*p) {
            case 0x1b:
                context->state = PS_ESC;
                p++;
                break;
            case 0x90:
                context->state = PS_DCS;
                p++;
                break;
            case 0x9c:
                p++;
                goto finalize;
            default:
                p++;
                break;
            }
            break;

        case PS_ESC:
            switch (*p) {
            case '\\':
            case 0x9c:
                p++;
                goto finalize;
            case 'P':
                context->param = -1;
                context->state = PS_DCS;
                p++;
                break;
            default:
                p++;
                break;
            }
            break;

        case PS_DCS:
            switch (*p) {
            case 0x1b:
                context->state = PS_ESC;
                p++;
                break;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                if (context->param < 0) {
                    context->param = 0;
                }
                status = safe_addition_for_params(context, p);
                if (SIXEL_FAILED(status)) {
                    goto end;
                }
                p++;
                break;
            case ';':
                if (context->param < 0) {
                    context->param = 0;
                }
                if (context->nparams < DECSIXEL_PARAMS_MAX) {
                    context->params[context->nparams++] = context->param;
                }
                context->param = 0;
                p++;
                break;
            case 'q':
                if (context->param >= 0 && context->nparams < DECSIXEL_PARAMS_MAX) {
                    context->params[context->nparams++] = context->param;
                }
                status = parser_context_enter_decsixel(context,
                                                       image,
                                                       allocator);
                if (SIXEL_FAILED(status)) {
                    goto end;
                }
                p++;
                break;
            default:
                p++;
                break;
            }
            break;

        case PS_DECSIXEL:
            switch (*p) {
            case '\x1b':
                context->state = PS_ESC;
                p++;
                break;
            case '"':
                context->param = 0;
                context->nparams = 0;
                context->state = PS_DECGRA;
                p++;
                break;
            case '!':
                context->param = 0;
                context->nparams = 0;
                context->state = PS_DECGRI;
                p++;
                break;
            case '#':
                context->param = 0;
                context->nparams = 0;
#if SIXEL_ENABLE_THREADS
                if (!palette_ready) {
                    palette_ready = 1;
                }
#endif  /* SIXEL_ENABLE_THREADS */
                context->state = PS_DECGCI;
                p++;
                break;
            case '$':
                /* DECGCR Graphics Carriage Return */
                context->pos_x = 0;
#if SIXEL_ENABLE_THREADS
                if (!palette_ready) {
                    palette_ready = 1;
                }
                if (!parallel_started && raster_ready && palette_ready) {
                    if (image->normal_columns != NULL) {
                        /* A late raster can enable parallel work after
                         * serial paint. Publish its pixels first. */
                        image_buffer_materialize_columns(image, allocator);
                        context->normal_disabled = 1;
                    }
                    status = sixel_decoder_parallel_request_start(
                        image->depth == 4U ? 1: 0,
                        context->ormode,
                        p0,
                        len,
                        parallel_anchor,
                        image,
                        context->color_index,
                        image->palette,
                        logger_prepared ? logger : NULL,
                        decode_flags,
                        &context->painted_outside_raster,
                        undither);
                    parallel_started = 1;
                    if (status == SIXEL_FALSE) {
                        /* Parallel decode aborted; continue serially. */
                    } else {
                        goto end;
                    }
                }
#endif  /* SIXEL_ENABLE_THREADS */
                p++;
                break;
            case '-':
                /* DECGNL Graphics Next Line */
                context->pos_x = 0;
                if (context->pos_y > INT_MAX - 6) {
                    status = SIXEL_BAD_INPUT;
                    goto end;
                }
                context->pos_y += 6;
#if SIXEL_ENABLE_THREADS
                if (!palette_ready) {
                    palette_ready = 1;
                }
                if (!parallel_started && raster_ready && palette_ready) {
                    if (image->normal_columns != NULL) {
                        /* A late raster can enable parallel work after
                         * serial paint. Publish its pixels first. */
                        image_buffer_materialize_columns(image, allocator);
                        context->normal_disabled = 1;
                    }
                    status = sixel_decoder_parallel_request_start(
                        image->depth == 4U ? 1: 0,
                        context->ormode,
                        p0,
                        len,
                        parallel_anchor,
                        image,
                        context->color_index,
                        image->palette,
                        logger_prepared ? logger : NULL,
                        decode_flags,
                        &context->painted_outside_raster,
                        undither);
                    parallel_started = 1;
                    if (status == SIXEL_FALSE) {
                        /* Parallel decode aborted; continue serially. */
                    } else {
                        goto end;
                    }
                }
#endif  /* SIXEL_ENABLE_THREADS */
                p++;
                break;
            default:
                if (*p >= '?' && *p <= '~') {  /* sixel characters */
                    context->painted = 1;
#if SIXEL_ENABLE_THREADS
                    if (!palette_ready) {
                        palette_ready = 1;
                    }
                    if (!parallel_started && raster_ready &&
                            palette_ready) {
                        if (image->normal_columns != NULL) {
                            /* Publish earlier paint before a late raster
                             * enables the parallel destination writer. */
                            image_buffer_materialize_columns(image, allocator);
                            context->normal_disabled = 1;
                        }
                        status = sixel_decoder_parallel_request_start(
                            image->depth == 4U ? 1: 0,
                            context->ormode,
                            p0,
                            len,
                            parallel_anchor,
                            image,
                            context->color_index,
                            image->palette,
                            logger_prepared ? logger : NULL,
                            decode_flags,
                            &context->painted_outside_raster,
                            undither);
                        parallel_started = 1;
                        if (status == SIXEL_FALSE) {
                            /* Parallel decode aborted; continue serially. */
                        } else {
                            goto end;
                        }
                    }
#endif  /* SIXEL_ENABLE_THREADS */

                    /* Keep OR runs outside normal-column setup checks. */
                    if (context->ormode) {
                        if (!trust_raster_size &&
                                context->repeat_count == 1 &&
                                context->pos_x >= 0 &&
                                context->pos_x < image->width &&
                                context->pos_y >= 0 &&
                                context->pos_y <= image->height - 6) {
                            p = parser_context_store_ormode_run(
                                context, image, p, p0 + len);
                            continue;
                        }
                    } else {
                        /* Sequence packets keep an existing canvas; their
                         * untouched samples must never be reinitialized. */
                        if (!context->sequence &&
                                !context->normal_disabled &&
                                image->normal_columns == NULL) {
                            image->normal_columns =
                                image_buffer_allocate_planes(image->width,
                                    image->height, allocator);
                            if (image->normal_columns == NULL) {
                                status = SIXEL_BAD_ALLOCATION;
                                goto end;
                            }
                        }
                        if (image->normal_columns != NULL &&
                                !trust_raster_size &&
                                context->repeat_count == 1 &&
                                context->pos_x >= 0 &&
                                context->pos_x < image->width &&
                                context->pos_y >= 0 &&
                                context->pos_y <= image->height - 6) {
                            p = parser_context_store_normal_run(
                                context, image, p, p0 + len);
                            continue;
                        }
                    }

                    if (context->pos_x < 0 || context->pos_y < 0 ||
                            context->pos_x > INT_MAX -
                                context->repeat_count ||
                            context->pos_y > INT_MAX - 6) {
                        status = SIXEL_BAD_INPUT;
                        goto end;
                    }
                    bits = *p - '?';
                    /* A validated sequence skip never grows the canvas. */
                    if (context->sequence && bits == 0) {
                        context->pos_x += context->repeat_count;
                        context->repeat_count = 1;
                        p++;
                        break;
                    }
                    draw_repeat = context->repeat_count;
                    if (trust_raster_size && context->attributed_ph > 0) {
                        if (context->pos_x >= context->attributed_ph) {
                            if (bits != 0) {
                                context->painted_outside_raster = 1;
                            }
                            draw_repeat = 0;
                        } else if (context->pos_x + draw_repeat >
                                context->attributed_ph) {
                            if (bits != 0) {
                                context->painted_outside_raster = 1;
                            }
                            draw_repeat =
                                context->attributed_ph - context->pos_x;
                        }
                    }

                    target_bottom = context->pos_y + 6;
                    if (trust_raster_size && context->attributed_pv > 0 &&
                            target_bottom > context->attributed_pv) {
                        target_bottom = context->attributed_pv;
                    }

                    /* Reject before doubling dimensions, including streams
                     * that advance far outside a declared raster. */
                    if ((draw_repeat > 0 && context->pos_x >
                            SIXEL_WIDTH_LIMIT - draw_repeat) ||
                            target_bottom > SIXEL_HEIGHT_LIMIT) {
                        status = SIXEL_BAD_INPUT;
                        goto end;
                    }
                    sx = image->width;
                    while (draw_repeat > 0 &&
                            sx < context->pos_x + draw_repeat) {
                        sx *= 2;
                    }

                    sy = image->height;
                    while (sy < target_bottom) {
                        sy *= 2;
                    }

                    if (sx > image->width || sy > image->height) {
                        status = image_buffer_resize(
                            image,
                            sx,
                            sy,
                            parser_context_bgindex(context),
                            allocator);
                        if (SIXEL_FAILED(status)) {
                            goto end;
                        }
                    }

                    if (image->depth != 4U &&
                            context->color_index > image->ncolors) {
                        image->ncolors = context->color_index;
                    }

                    if (context->ormode || image->normal_columns != NULL) {
                        store_bits = bits;
                        if (trust_raster_size &&
                                context->attributed_pv > 0) {
                            for (i = 0; i < 6; ++i) {
                                if ((bits & (1 << i)) != 0 &&
                                        context->pos_y + i >=
                                        context->attributed_pv) {
                                    context->painted_outside_raster = 1;
                                    store_bits &= ~(1 << i);
                                }
                            }
                        }
                        if (store_bits != 0 && draw_repeat > 0) {
                            pos = (size_t)(context->pos_y / 6) *
                                image->width + (size_t)context->pos_x;
                            if (image->normal_columns != NULL) {
                                sixel_column_store(image->normal_columns + pos,
                                    context->normal_color, store_bits,
                                    draw_repeat);
                            } else {
                                sixel_ormode_store(image->ormode_planes + pos,
                                    context->ormode_spread, store_bits,
                                    draw_repeat);
                            }
                            if (context->ormode_high_spread != 0) {
                                sixel_ormode_store(
                                    image->ormode_high_planes + pos,
                                    context->ormode_high_spread, store_bits,
                                    draw_repeat);
                            }
                            if (context->max_x < context->pos_x +
                                    draw_repeat - 1) {
                                context->max_x = context->pos_x +
                                    draw_repeat - 1;
                            }
                            for (i = 5; i >= 0; --i) {
                                if ((store_bits & (1 << i)) != 0) {
                                    if (context->max_y <
                                            context->pos_y + i) {
                                        context->max_y =
                                            context->pos_y + i;
                                    }
                                    break;
                                }
                            }
                        }
                        context->pos_x += context->repeat_count;
                    } else if (bits == 0) {
                        context->pos_x += context->repeat_count;
                    } else {
                        sixel_vertical_mask = 0x01;
                        if (context->repeat_count <= 1) {
                            for (i = 0; i < 6; i++) {
                                if ((bits & sixel_vertical_mask) != 0) {
                                    if (trust_raster_size &&
                                            context->attributed_pv > 0 &&
                                            context->pos_y + i >=
                                            context->attributed_pv) {
                                        context->painted_outside_raster = 1;
                                        sixel_vertical_mask <<= 1;
                                        continue;
                                    }
                                    if (draw_repeat <= 0) {
                                        sixel_vertical_mask <<= 1;
                                        continue;
                                    }
                                    pos =
                                        (size_t)image->width *
                                        (size_t)(context->pos_y + i) +
                                        (size_t)context->pos_x;
                                    image_buffer_store_pixel(
                                        image,
                                        pos,
                                        context->color_index);
                                    image_buffer_mark_painted_span(
                                        image,
                                        pos,
                                        1);
                                    if (context->max_x < context->pos_x) {
                                        context->max_x = context->pos_x;
                                    }
                                    if (context->max_y <
                                            context->pos_y + i) {
                                        context->max_y = context->pos_y + i;
                                    }
                                }
                                sixel_vertical_mask <<= 1;
                            }
                            context->pos_x += 1;
                        } else {
                            /* context->repeat_count > 1 */
                            for (i = 0; i < 6; i++) {
                                if ((bits & sixel_vertical_mask) != 0) {
                                    c = sixel_vertical_mask << 1;
                                    for (n = 1; (i + n) < 6; n++) {
                                        if ((bits & c) == 0) {
                                            break;
                                        }
                                        c <<= 1;
                                    }
                                    drawn_y_end = -1;
                                    for (y = context->pos_y + i;
                                            y < context->pos_y + i + n;
                                            ++y) {
                                        if (trust_raster_size &&
                                                context->attributed_pv > 0 &&
                                                y >= context->attributed_pv) {
                                            context->painted_outside_raster =
                                                1;
                                            continue;
                                        }
                                        if (draw_repeat <= 0) {
                                            continue;
                                        }
                                        image_buffer_fill_span(
                                            image,
                                            y,
                                            context->pos_x,
                                            draw_repeat,
                                            context->color_index);
                                        drawn_y_end = y;
                                    }
                                    if (drawn_y_end >= 0 &&
                                            context->max_x <
                                            context->pos_x +
                                            draw_repeat - 1) {
                                        context->max_x =
                                            context->pos_x + draw_repeat - 1;
                                    }
                                    if (context->max_y < drawn_y_end) {
                                        context->max_y = drawn_y_end;
                                    }
                                    i += (n - 1);
                                    sixel_vertical_mask <<= (n - 1);
                                }
                                sixel_vertical_mask <<= 1;
                            }
                            context->pos_x += context->repeat_count;
                        }
                    }
                    context->repeat_count = 1;
                }
                p++;
                break;
            }
            break;

        case PS_DECGRA:
            /* DECGRA Set Raster Attributes " Pan; Pad; Ph; Pv */
            switch (*p) {
            case '\x1b':
                if (context->sequence) {
                    goto sequence_raster_parameters;
                }
                context->state = PS_ESC;
                p++;
                break;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                status = safe_addition_for_params(context, p);
                if (SIXEL_FAILED(status)) {
                    goto end;
                }
                p++;
                break;
            case ';':
                if (context->nparams < DECSIXEL_PARAMS_MAX) {
                    context->params[context->nparams++] = context->param;
                }
                context->param = 0;
                p++;
                break;
            default:
sequence_raster_parameters:
                if (context->nparams < DECSIXEL_PARAMS_MAX) {
                    context->params[context->nparams++] = context->param;
                }
                if (context->nparams > 0) {
                    context->attributed_pad = context->params[0];
                }
                if (context->nparams > 1) {
                    context->attributed_pan = context->params[1];
                }
                if (context->nparams > 2 && context->params[2] > 0) {
                    context->attributed_ph = context->params[2];
                }
                if (context->nparams > 3 && context->params[3] > 0) {
                    context->attributed_pv = context->params[3];
                }

                if (context->attributed_pan <= 0) {
                    context->attributed_pan = 1;
                }
                if (context->attributed_pad <= 0) {
                    context->attributed_pad = 1;
                }

                if (image->width < context->attributed_ph ||
                        image->height < context->attributed_pv) {
                    sx = context->attributed_ph;
                    if (image->width > context->attributed_ph) {
                        sx = image->width;
                    }

                    sy = context->attributed_pv;
                    if (image->height > context->attributed_pv) {
                        sy = image->height;
                    }

                    status = image_buffer_resize(
                        image,
                        sx,
                        sy,
                        parser_context_bgindex(context),
                        allocator);
                    if (SIXEL_FAILED(status)) {
                        goto end;
                    }
                }
#if SIXEL_ENABLE_THREADS
                if (!context->sequence && !raster_ready &&
                        context->attributed_ph > 0 &&
                        context->attributed_pv > 0) {
                    raster_ready = 1;
                }
                parallel_anchor = p;
#endif  /* SIXEL_ENABLE_THREADS */
                context->state = PS_DECSIXEL;
                context->param = 0;
                context->nparams = 0;
            }
            break;

        case PS_DECGRI:
            /* DECGRI Graphics Repeat Introducer ! Pn Ch */
            switch (*p) {
            case '\x1b':
                context->state = PS_ESC;
                p++;
                break;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                status = safe_addition_for_params(context, p);
                if (SIXEL_FAILED(status)) {
                    goto end;
                }
                p++;
                break;
            default:
                context->repeat_count = context->param;
                if (context->repeat_count == 0) {
                    context->repeat_count = 1;
                }
                if (context->repeat_count > 0xffff) {  /* check too huge number */
                    status = SIXEL_BAD_INPUT;
                    sixel_helper_set_additional_message(
                        "sixel_decode_raw_impl: detected too huge repeat parameter.");
                    goto end;
                }
                context->state = PS_DECSIXEL;
                context->param = 0;
                context->nparams = 0;
                break;
            }
            break;

        case PS_DECGCI:
            /* DECGCI Graphics Color Introducer # Pc; Pu; Px; Py; Pz */
            switch (*p) {
            case '\x1b':
                if (context->sequence) {
                    goto sequence_color_parameters;
                }
                context->state = PS_ESC;
                p++;
                break;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                status = safe_addition_for_params(context, p);
                if (SIXEL_FAILED(status)) {
                    goto end;
                }
                p++;
                break;
            case ';':
                if (context->nparams < DECSIXEL_PARAMS_MAX) {
                    context->params[context->nparams++] = context->param;
                }
                context->param = 0;
                p++;
                break;
            default:
sequence_color_parameters:
                context->state = PS_DECSIXEL;
                if (context->nparams < DECSIXEL_PARAMS_MAX) {
                    context->params[context->nparams++] = context->param;
                }
                context->param = 0;

                if (context->nparams > 0) {
                    context->color_index = context->params[0];
                    if (context->color_index < 0) {
                        context->color_index = 0;
                    } else if (context->color_index >= SIXEL_PALETTE_MAX_DECODER) {
                        context->color_index = SIXEL_PALETTE_MAX_DECODER - 1;
                    }
                }

                if (context->ormode) {
                    context->ormode_spread = sixel_ormode_spread(
                        (unsigned int)context->color_index);
                    context->ormode_high_spread = sixel_ormode_spread(
                        (unsigned int)context->color_index >> 8);
                    /* Wide registers use the same six-row representation.
                     * Allocate the second eight planes only when needed. */
                    if (context->ormode_high_spread != 0 &&
                            image->ormode_high_planes == NULL) {
                        image->ormode_high_planes =
                            image_buffer_allocate_planes(image->width,
                                image->height, allocator);
                        if (image->ormode_high_planes == NULL) {
                            status = SIXEL_BAD_ALLOCATION;
                            goto end;
                        }
                    }
                } else {
                    if (context->color_index >= 256 ||
                            (image->depth == 4 && context->painted &&
                             context->nparams > 4)) {
                        /* Byte lanes cannot hold wide registers. Direct
                         * output also owns paint-time palette values; raw
                         * indexes intentionally keep the final palette. */
                        image_buffer_materialize_columns(image, allocator);
                        context->normal_disabled = 1;
                    } else {
                        context->normal_color = sixel_column_color(
                            (unsigned int)context->color_index);
                    }
                }

                if (context->color_index + 1 > image->ncolors) {
                    image->ncolors = context->color_index + 1;
                    if (image->ncolors > SIXEL_PALETTE_MAX_DECODER) {
                        image->ncolors = SIXEL_PALETTE_MAX_DECODER;
                    }
                }

                if (context->nparams > 4) {
                    /*
                     * A definition that lands after painting has begun makes
                     * the index plane and the palette disagree over time.
                     * High color output relies on exactly that, so record it
                     * for callers that consume raw indexes.
                     */
                    if (context->painted) {
                        context->palette_redefined_after_paint = 1;
                    }
                    if (context->params[1] == 1) {
                        /* HLS */
                        if (context->params[2] > 360) {
                            context->params[2] = 360;
                        }
                        if (context->params[3] > 100) {
                            context->params[3] = 100;
                        }
                        if (context->params[4] > 100) {
                            context->params[4] = 100;
                        }
                        image->palette[context->color_index]
                            = hls_to_rgb(context->params[2], context->params[3], context->params[4]);
                    } else if (context->params[1] == 2) {
                        /* RGB */
                        if (context->params[2] > 100) {
                            context->params[2] = 100;
                        }
                        if (context->params[3] > 100) {
                            context->params[3] = 100;
                        }
                        if (context->params[4] > 100) {
                            context->params[4] = 100;
                        }
                        image->palette[context->color_index]
                            = SIXEL_XRGB(context->params[2], context->params[3], context->params[4]);
                    }
                    if (context->params[1] == 1 || context->params[1] == 2) {
                        image->palette[context->color_index] =
                            sixel_palette_map_rgb(
                                image->palette[context->color_index],
                                context->palette_transform, 0);
                    }
#if SIXEL_ENABLE_THREADS
                    parallel_anchor = p;
#endif  /* SIXEL_ENABLE_THREADS */
                }
                break;
            }
            break;
        default:
            break;
        }
    }

finalize:
    if (++context->max_x < context->attributed_ph) {
        context->max_x = context->attributed_ph;
    }

    if (++context->max_y < context->attributed_pv) {
        context->max_y = context->attributed_pv;
    }

    if (context->attributed_ph > 0 &&
            context->max_x > context->attributed_ph) {
        context->painted_outside_raster = 1;
    }
    if (context->attributed_pv > 0 &&
            context->max_y > context->attributed_pv) {
        context->painted_outside_raster = 1;
    }

    if (image->width > context->max_x || image->height > context->max_y) {
        status = image_buffer_resize(image,
                                     context->max_x,
                                     context->max_y,
                                     parser_context_bgindex(context),
                                     allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    status = SIXEL_OK;

end:
    return status;
}

static SIXELSTATUS
sixel_decode_image(unsigned char *p, int len, int initial_width,
                   int initial_height, int depth, int keep_paint_mask,
                   image_buffer_t *image, parser_context_t *context,
                   sixel_allocator_t *allocator, unsigned int decode_flags,
                   sixel_decoder_undither_context_t *undither, int body_only,
                   int const *body_params, size_t body_nparams,
                   sixel_palette_transform_t const *transform)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_timeline_logger_t *logger;
    int logger_prepared;
    size_t i;
    size_t nparams;
    int white;

    image->pixels.p = NULL;
    image->paint_mask = NULL;
    image->ormode_planes = NULL;
    image->ormode_high_planes = NULL;
    image->normal_columns = NULL;

    logger = NULL;
    logger_prepared = 0;
    i = 0U;
    nparams = body_nparams;
    (void)sixel_timeline_logger_prepare_env(allocator, &logger);
    logger_prepared = logger != NULL;
    if (logger_prepared) {
        /*
         * File I/O window for timeline visualization. The buffer is already
         * populated, but logging the bounds keeps decode timing aligned with
         * encoder logs.
         */
        sixel_timeline_logger_logf(logger,
                          "decoder",
                          "io",
                          "start",
                          0,
                          0,
                          0,
                          len,
                          0,
                          len,
                          "reading sixel payload");
    }

    status = parser_context_init(context);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    context->palette_transform = transform;

    /*
     * The serial parser always runs first. When palette and raster
     * attributes become available, the parser may request a parallel worker
     * via sixel_decoder_parallel_request_start(). This guarantees bounds
     * checks and logging are consistent before any background decode starts.
     */

    if (logger_prepared) {
        /* Mark when the serial parser begins scanning tokens. */
        sixel_timeline_logger_logf(logger,
                          "decoder",
                          "controller",
                          "start",
                          0,
                          0,
                          0,
                          len,
                          0,
                          len,
                          "serial parser begin depth=%u",
                          depth);
    }

    status = image_buffer_init(image,
                               initial_width,
                               initial_height,
                               context->bgindex,
                               depth,
                               keep_paint_mask,
                               allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    if (transform != NULL) {
        /* Defaults above 255 are all white. A pure transform needs only one
         * evaluation for that repeated source value, not 65,280 callbacks. */
        for (i = 0U; i < 256U; i++) {
            image->palette[i] =
                sixel_palette_map_rgb(image->palette[i], transform, 1);
        }
        white = sixel_palette_map_rgb(SIXEL_RGB(255, 255, 255), transform, 1);
        for (i = 256U; i < SIXEL_PALETTE_MAX_DECODER; i++) {
            image->palette[i] = white;
        }
    }

    if (body_only) {
        if (nparams > 0U && body_params == NULL) {
            sixel_helper_set_additional_message(
                "sixel_decode_image: DCS q params are null.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        if (nparams > DECSIXEL_PARAMS_MAX) {
            nparams = DECSIXEL_PARAMS_MAX;
        }
        for (i = 0U; i < nparams; i++) {
            if (body_params[i] < 0) {
                sixel_helper_set_additional_message(
                    "sixel_decode_image: DCS q parameter is negative.");
                status = SIXEL_BAD_ARGUMENT;
                goto end;
            }
            context->params[i] = body_params[i];
        }
        context->nparams = (int)nparams;
        context->param = -1;
        status = parser_context_enter_decsixel(context, image, allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    status = sixel_decode_raw_impl(p,
                                   len,
                                   image,
                                   context,
                                   allocator,
                                   logger,
                                   logger_prepared,
                                   decode_flags,
                                   undither);
    if (SIXEL_FAILED(status)) {
        sixel_allocator_free(allocator, image->pixels.p);
        image->pixels.p = NULL;
        image_buffer_release_paint_mask(image, allocator);
        image_buffer_release_ormode_planes(image, allocator);
        image_buffer_release_columns(image, allocator);
        goto end;
    }

    if (context->ormode) {
        image_buffer_finalize_ormode(image);
    } else {
        image_buffer_materialize_columns(image, allocator);
    }

    status = SIXEL_OK;

end:
    if (logger_prepared) {
        sixel_timeline_logger_logf(logger,
                          "decoder",
                          "parser",
                          "finish",
                          0,
                          0,
                          0,
                          len,
                          0,
                          len,
                          "parser status=%d",
                          status);
        sixel_timeline_logger_logf(logger,
                          "decoder",
                          "io",
                          "finish",
                          0,
                          0,
                          0,
                          len,
                          0,
                          len,
                          "input processed status=%d",
                          status);
        sixel_timeline_logger_unref(logger);
    }
    return status;
}

static SIXELSTATUS sixel_decode_raw_with_options_internal(
    unsigned char *p, int len, unsigned int decode_flags,
    unsigned char **pixels, unsigned char **paint_mask, int *pwidth,
    int *pheight, unsigned char **palette, int *ncolors,
    unsigned int *result_flags, sixel_allocator_t *allocator,
    sixel_palette_transform_t const *transform)
{
    SIXELSTATUS status = SIXEL_FALSE;
    parser_context_t context;
    image_buffer_t *image = NULL;
    int n;
    int alloc_size;

    if (result_flags != NULL) {
        *result_flags = 0U;
    }
    if (paint_mask != NULL) {
        *paint_mask = NULL;
    }
    if (pixels != NULL) {
        *pixels = NULL;
    }
    if (palette != NULL) {
        *palette = NULL;
    }
    if (ncolors != NULL) {
        *ncolors = 0;
    }
    if (pixels == NULL || pwidth == NULL || pheight == NULL ||
            palette == NULL || ncolors == NULL) {
        sixel_helper_set_additional_message(
            "sixel_decode_raw: output argument is null.");
        return SIXEL_BAD_ARGUMENT;
    }

    if (allocator) {
        sixel_allocator_ref(allocator);
    } else {
        status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
        if (SIXEL_FAILED(status)) {
            allocator = NULL;
            goto error;
        }
    }

    image = (image_buffer_t *)malloc(sizeof(*image));
    if (image == NULL) {
        sixel_helper_set_additional_message(
            "sixel_decode_raw: malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto error;
    }
    image->pixels.p = NULL;
    image->paint_mask = NULL;

    status = sixel_decode_image(p, len, 1, /* initial_width */
                                1,         /* initial_height */
                                1,         /* depth */
                                paint_mask != NULL, image, &context, allocator,
                                decode_flags, NULL, 0, NULL, 0U, transform);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    *ncolors = alloc_size = image->ncolors;
    if (alloc_size < SIXEL_PALETTE_MAX_DECODER) {
        /*
         * Keep the returned palette backed by the decoder's full default
         * table for internal writers that still inspect byte-valued raw
         * pixels. The public color range remains [0, *ncolors); larger pixel
         * values are transparent/unpainted sentinels, not palette entries.
         */
        alloc_size = SIXEL_PALETTE_MAX_DECODER;
    }
    *palette = (unsigned char *)sixel_allocator_malloc(
        allocator,
        (size_t)(alloc_size * 3));
    if (*palette == NULL) {
        sixel_allocator_free(allocator, image->pixels.p);
        image->pixels.p = NULL;
        sixel_helper_set_additional_message(
            "sixel_decode_raw: palette allocation failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto error;
    }
    /*
     * Copy the full palette table so default entries remain initialized.
     * This keeps unused slots valid when the image uses fewer colors.
     */
    for (n = 0; n < alloc_size; ++n) {
        (*palette)[n * 3 + 0] = image->palette[n] >> 16 & 0xff;
        (*palette)[n * 3 + 1] = image->palette[n] >> 8 & 0xff;
        (*palette)[n * 3 + 2] = image->palette[n] & 0xff;
    }

    *pwidth = image->width;
    *pheight = image->height;
    *pixels = image->pixels.p;
    image->pixels.p = NULL;
    if (paint_mask != NULL) {
        *paint_mask = image->paint_mask;
        image->paint_mask = NULL;
    }
    if (result_flags != NULL && context.painted_outside_raster) {
        *result_flags |= SIXEL_DECODE_PIXELS_RESULT_PAINT_OUTSIDE_RASTER;
        if ((decode_flags &
                SIXEL_DECODE_PIXELS_OPTION_TRUST_RASTER_SIZE) != 0U) {
            *result_flags |= SIXEL_DECODE_PIXELS_RESULT_CLIPPED_TO_RASTER;
        }
    }
    if (result_flags != NULL && context.palette_redefined_after_paint) {
        *result_flags |= SIXEL_DECODE_PIXELS_RESULT_PALETTE_REDEFINED;
    }

    status = SIXEL_OK;
    goto end;

error:
    if (image != NULL && image->pixels.p != NULL) {
        if (allocator != NULL) {
            sixel_allocator_free(allocator, image->pixels.p);
        } else {
            free(image->pixels.p);
        }
        image->pixels.p = NULL;
    }

end:
    image_buffer_release_paint_mask(image, allocator);
    image_buffer_release_ormode_planes(image, allocator);
    image_buffer_release_columns(image, allocator);
    free(image);
    sixel_allocator_unref(allocator);
    return status;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_decode_raw_mapped(
    unsigned char *p, int len, unsigned int decode_flags,
    sixel_palette_transform_t const *transform, unsigned char **pixels,
    unsigned char **paint_mask, int *pwidth, int *pheight,
    unsigned char **palette, int *ncolors, unsigned int *result_flags,
    sixel_allocator_t *allocator)
{
    return sixel_decode_raw_with_options_internal(
        p, len, decode_flags, pixels, paint_mask, pwidth, pheight, palette,
        ncolors, result_flags, allocator, transform);
}

/* convert sixel data into indexed pixel bytes and palette data */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_decode_raw_with_options(
    unsigned char     *p,
    int                len,
    unsigned int       decode_flags,
    unsigned char    **pixels,
    int               *pwidth,
    int               *pheight,
    unsigned char    **palette,
    int               *ncolors,
    unsigned int      *result_flags,
    sixel_allocator_t *allocator)
{
    return sixel_decode_raw_with_options_internal(
        p, len, decode_flags, pixels, NULL, pwidth, pheight, palette, ncolors,
        result_flags, allocator, NULL);
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_decode_raw_with_options_mask(
    unsigned char     *p,
    int                len,
    unsigned int       decode_flags,
    unsigned char    **pixels,
    unsigned char    **paint_mask,
    int               *pwidth,
    int               *pheight,
    unsigned char    **palette,
    int               *ncolors,
    unsigned int      *result_flags,
    sixel_allocator_t *allocator)
{
    return sixel_decode_raw_with_options_internal(
        p, len, decode_flags, pixels, paint_mask, pwidth, pheight, palette,
        ncolors, result_flags, allocator, NULL);
}

SIXELAPI SIXELSTATUS
sixel_decode_raw(
    unsigned char       /* in */  *p,           /* sixel bytes */
    int                 /* in */  len,          /* size of sixel bytes */
    unsigned char       /* out */ **pixels,     /* decoded pixels */
    int                 /* out */ *pwidth,      /* image width */
    int                 /* out */ *pheight,     /* image height */
    unsigned char       /* out */ **palette,    /* RGB palette */
    int                 /* out */ *ncolors,     /* palette size (<= 256) */
    sixel_allocator_t   /* in */  *allocator)   /* allocator object or null */
{
    return sixel_decode_raw_with_options(p,
                                         len,
                                         0U,
                                         pixels,
                                         pwidth,
                                         pheight,
                                         palette,
                                         ncolors,
                                         NULL,
                                         allocator);
}

/*
 * Small embedded decoders may link fromsixel.c without decoder.c.  Keep the
 * decoder-backed fast4 helpers out of those archives.
 */
#if !defined(SIXEL_DECODE_PIXELS_NO_FAST4)
static SIXELSTATUS
sixel_decode_fast4_promote_rgba(unsigned char **out_pixels,
                                unsigned char const *rgb_pixels,
                                int width,
                                int height,
                                sixel_allocator_t *allocator)
{
    unsigned char *rgba_pixels;
    size_t pixels;
    size_t pixel_index;
    size_t rgb_index;
    size_t rgba_index;

    if (out_pixels == NULL || rgb_pixels == NULL ||
            width <= 0 || height <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    if ((size_t)width > ((size_t)-1 / (size_t)height)) {
        return SIXEL_BAD_ALLOCATION;
    }
    pixels = (size_t)width * (size_t)height;
    if (pixels > ((size_t)-1 / 4u)) {
        return SIXEL_BAD_ALLOCATION;
    }

    rgba_pixels = (unsigned char *)sixel_allocator_malloc(
        allocator,
        pixels * 4u);
    if (rgba_pixels == NULL) {
        sixel_helper_set_additional_message(
            "sixel_decode_kundither_fast4: rgba allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    for (pixel_index = 0u; pixel_index < pixels; ++pixel_index) {
        rgb_index = pixel_index * 3u;
        rgba_index = pixel_index * 4u;
        rgba_pixels[rgba_index + 0u] = rgb_pixels[rgb_index + 0u];
        rgba_pixels[rgba_index + 1u] = rgb_pixels[rgb_index + 1u];
        rgba_pixels[rgba_index + 2u] = rgb_pixels[rgb_index + 2u];
        rgba_pixels[rgba_index + 3u] = 255u;
    }

    *out_pixels = rgba_pixels;
    return SIXEL_OK;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_decode_kundither_fast4_with_options(unsigned char *p,
                                          int len,
                                          int direct_output,
                                          int similarity_bias,
                                          unsigned int decode_flags,
                                          unsigned int *result_flags,
                                          unsigned char **pixels,
                                          int *pwidth,
                                          int *pheight,
                                          sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    parser_context_t context;
    image_buffer_t *image;
    sixel_decoder_undither_context_t undither;
    unsigned char *palette;
    unsigned char *rgb_pixels;
    unsigned char *rgba_pixels;
    int alloc_size;
    int n;

    status = SIXEL_FALSE;
    image = NULL;
    palette = NULL;
    rgb_pixels = NULL;
    rgba_pixels = NULL;
    memset(&undither, 0, sizeof(undither));
    if (result_flags != NULL) {
        *result_flags = 0U;
    }

    if (p == NULL || len <= 0 || pixels == NULL ||
            pwidth == NULL || pheight == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *pixels = NULL;
    *pwidth = 0;
    *pheight = 0;

    if (allocator) {
        sixel_allocator_ref(allocator);
    } else {
        status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
        if (SIXEL_FAILED(status)) {
            allocator = NULL;
            goto end;
        }
    }

    image = (image_buffer_t *)malloc(sizeof(*image));
    if (image == NULL) {
        sixel_helper_set_additional_message(
            "sixel_decode_kundither_fast4: malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    image->pixels.p = NULL;
    image->ormode_planes = NULL;
    image->ormode_high_planes = NULL;
    image->normal_columns = NULL;

    undither.enabled = 1;
    undither.direct_output = direct_output != 0;
    undither.similarity_bias = similarity_bias;
    undither.allocator = allocator;

    status = sixel_decode_image(p, len, 1, 1, 1, 0, image, &context, allocator,
                                decode_flags, &undither, 0, NULL, 0U, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    *pwidth = image->width;
    *pheight = image->height;
    if (result_flags != NULL && context.painted_outside_raster) {
        *result_flags |= SIXEL_DECODE_PIXELS_RESULT_PAINT_OUTSIDE_RASTER;
        if ((decode_flags &
                SIXEL_DECODE_PIXELS_OPTION_TRUST_RASTER_SIZE) != 0U) {
            *result_flags |= SIXEL_DECODE_PIXELS_RESULT_CLIPPED_TO_RASTER;
        }
    }
    if (result_flags != NULL && context.palette_redefined_after_paint) {
        *result_flags |= SIXEL_DECODE_PIXELS_RESULT_PALETTE_REDEFINED;
    }
    if (undither.pixels != NULL) {
        *pixels = undither.pixels;
        undither.pixels = NULL;
        status = SIXEL_OK;
        goto end;
    }

    /*
     * The fused path requires a raster-sized parallel decode.  Inputs without
     * that shape still use the same fast4 reconstruction after normal raw
     * decoding so the command-line mode has one stable visual definition.
     */
    alloc_size = image->ncolors;
    if (alloc_size < SIXEL_PALETTE_MAX_DECODER) {
        alloc_size = SIXEL_PALETTE_MAX_DECODER;
    }
    palette = (unsigned char *)sixel_allocator_malloc(
        allocator,
        (size_t)alloc_size * 3u);
    if (palette == NULL) {
        sixel_helper_set_additional_message(
            "sixel_decode_kundither_fast4: palette allocation failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    for (n = 0; n < alloc_size; ++n) {
        palette[n * 3 + 0] = image->palette[n] >> 16 & 0xff;
        palette[n * 3 + 1] = image->palette[n] >> 8 & 0xff;
        palette[n * 3 + 2] = image->palette[n] & 0xff;
    }

    status = sixel_dequantize_k_undither_fast4(
        image->pixels.in_bytes,
        image->width,
        image->height,
        palette,
        image->ncolors,
        similarity_bias,
        allocator,
        &rgb_pixels);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    if (direct_output) {
        status = sixel_decode_fast4_promote_rgba(
            &rgba_pixels,
            rgb_pixels,
            image->width,
            image->height,
            allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        *pixels = rgba_pixels;
        rgba_pixels = NULL;
    } else {
        *pixels = rgb_pixels;
        rgb_pixels = NULL;
    }
    status = SIXEL_OK;

end:
    if (image != NULL) {
        image_buffer_release_ormode_planes(image, allocator);
        image_buffer_release_columns(image, allocator);
        sixel_allocator_free(allocator, image->pixels.p);
        free(image);
    }
    sixel_allocator_free(allocator, palette);
    sixel_allocator_free(allocator, rgb_pixels);
    sixel_allocator_free(allocator, rgba_pixels);
    sixel_allocator_free(allocator, undither.pixels);
    sixel_allocator_unref(allocator);
    return status;
}
#endif


/* convert sixel data into wide-indexed(16bit) pixels and palette data */
SIXELAPI SIXELSTATUS
sixel_decode_wide(
    unsigned char       /* in */  *p,           /* sixel bytes */
    int                 /* in */  len,          /* size of sixel bytes */
    unsigned short      /* out */ **pixels,     /* decoded wide indexes */
    int                 /* out */ *pwidth,      /* image width */
    int                 /* out */ *pheight,     /* image height */
    unsigned char       /* out */ **palette,    /* RGB palette */
    int                 /* out */ *ncolors,     /* palette size (<= 256) */
    sixel_allocator_t   /* in */  *allocator)   /* allocator object or null */
{
    SIXELSTATUS status = SIXEL_FALSE;
    parser_context_t context;
    image_buffer_t *image = NULL;
    int n;
    int alloc_size;

    if (allocator) {
        sixel_allocator_ref(allocator);
    } else {
        status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
        if (SIXEL_FAILED(status)) {
            allocator = NULL;
            goto error;
        }
    }

    image = (image_buffer_t *)malloc(sizeof(*image));
    if (image == NULL) {
        sixel_helper_set_additional_message(
            "sixel_decode_wide: malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto error;
    }
    image->pixels.p = NULL;

    status = sixel_decode_image(p, len, 1, /* initial width */
                                1,         /* initial height */
                                2,         /* depth */
                                0, image, &context, allocator, 0U, NULL, 0,
                                NULL, 0U, NULL);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    *ncolors = alloc_size = image->ncolors;
    if (alloc_size < SIXEL_PALETTE_MAX_DECODER) {
        /* memory access range should be 0 <= 255 */
        alloc_size = SIXEL_PALETTE_MAX_DECODER;
    }
    *palette = (unsigned char *)sixel_allocator_malloc(
        allocator,
        (size_t)(alloc_size * 3));
    if (palette == NULL) {
        sixel_allocator_free(allocator, image->pixels.p);
        image->pixels.p = NULL;
        sixel_helper_set_additional_message(
            "sixel_deocde_raw: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto error;
    }
    /*
     * Copy the full palette table so default entries remain initialized.
     * This keeps unused slots valid when the image uses fewer colors.
     */
    for (n = 0; n < alloc_size; ++n) {
        (*palette)[n * 3 + 0] = image->palette[n] >> 16 & 0xff;
        (*palette)[n * 3 + 1] = image->palette[n] >> 8 & 0xff;
        (*palette)[n * 3 + 2] = image->palette[n] & 0xff;
    }

    *pwidth = image->width;
    *pheight = image->height;
    *pixels = image->pixels.in_shorts;

    status = SIXEL_OK;
    goto end;

error:
    if (image != NULL && image->pixels.p != NULL) {
        if (allocator != NULL) {
            sixel_allocator_free(allocator, image->pixels.p);
        } else {
            free(image->pixels.p);
        }
        image->pixels.p = NULL;
    }

end:
    image_buffer_release_ormode_planes(image, allocator);
    image_buffer_release_columns(image, allocator);
    free(image);
    sixel_allocator_unref(allocator);
    return status;
}

/*
 * The strict capture adapter validates framing and paint bounds before handing
 * each packet to the existing direct painter. Register definitions and the
 * RGBA canvas belong to the whole capture; cursor/parser state is per packet.
 * This is deliberately separate from the permissive single-image interface.
 */
typedef struct sixel_sequence_state {
    int width;
    int height;
    int painted;
    unsigned int flags;
    int animation;
    int clear;
    unsigned char defined[SIXEL_PALETTE_MAX_DECODER];
} sixel_sequence_state_t;

static int
sixel_sequence_parameters(unsigned char const **cursor,
                           unsigned char const *end, int *params,
                           int capacity)
{
    unsigned char const *p;
    int count;
    int value;
    int digit;

    p = *cursor;
    count = 0;
    if (p == end || ((*p < '0' || *p > '9') && *p != ';')) {
        return 0;
    }
    for (;;) {
        if (count == capacity) {
            return -1;
        }
        value = 0;
        while (p < end && *p >= '0' && *p <= '9') {
            digit = *p++ - '0';
            if (value > (INT_MAX - digit) / 10) {
                return -1;
            }
            value = value * 10 + digit;
        }
        params[count++] = value;
        if (p == end || *p != ';') {
            break;
        }
        ++p;
    }
    *cursor = p;
    return count;
}

/* Return the first byte after ST only after validating the entire packet. */
static SIXELSTATUS
sixel_sequence_packet(unsigned char const **cursor,
                       unsigned char const *end,
                       sixel_sequence_state_t *state)
{
    unsigned char const *p;
    int params[5];
    int count;
    int raster;
    int x;
    int y;
    int color;
    int repeat;
    int bits;
    int row;
    unsigned char command;

    p = *cursor;
    raster = 0;
    x = 0;
    y = 0;
    color = state->animation ? 15 : 0;
    if (p < end && *p == 0x90) {
        ++p;
    } else if (end - p >= 2 && p[0] == 0x1b && p[1] == 'P') {
        p += 2;
    } else {
        return SIXEL_BAD_INPUT;
    }
    count = sixel_sequence_parameters(&p, end, params, 3);
    if (count < 0 || p == end || *p++ != 'q') {
        return SIXEL_BAD_INPUT;
    }
    state->clear = count < 2 || params[1] == 0;
    if ((!state->animation && (count < 2 || params[1] != 1))
            || (count >= 2 && params[1] != 0 && params[1] != 1)) {
        return SIXEL_BAD_INPUT;
    }
    while (p < end) {
        if (*p == 0x9c ||
                (end - p >= 2 && p[0] == 0x1b && p[1] == '\\')) {
            p += *p == 0x9c ? 1 : 2;
            if (!raster) {
                return SIXEL_BAD_INPUT;
            }
            *cursor = p;
            return SIXEL_OK;
        }
        command = *p++;
        if (command == '"') {
            count = sixel_sequence_parameters(&p, end, params, 4);
            if (count != 4 || params[0] <= 0 ||
                    params[0] != params[1] || params[2] <= 0 ||
                    params[3] <= 0 ||
                    params[2] > SIXEL_DECODE_PIXELS_MAX_SIZE ||
                    params[3] > SIXEL_DECODE_PIXELS_MAX_SIZE) {
                return SIXEL_BAD_INPUT;
            }
            if (state->width != 0 && (state->width != params[2] ||
                    state->height != params[3])) {
                return SIXEL_BAD_INPUT;
            }
            state->width = params[2];
            state->height = params[3];
            raster = 1;
            continue;
        }
        if (command == '#') {
            count = sixel_sequence_parameters(&p, end, params, 5);
            if ((count != 1 && count != 5) ||
                    params[0] >= SIXEL_PALETTE_MAX_DECODER) {
                return SIXEL_BAD_INPUT;
            }
            color = params[0];
            if (count == 5) {
                if (params[1] != 1 && params[1] != 2) {
                    return SIXEL_BAD_INPUT;
                }
                state->defined[color] = 1;
                if (state->painted) {
                    state->flags |=
                        SIXEL_DECODE_PIXELS_RESULT_PALETTE_REDEFINED;
                }
            }
            continue;
        }
        if (command == '$') {
            x = 0;
            continue;
        }
        if (command == '-') {
            if (y > INT_MAX - 6) {
                return SIXEL_BAD_INPUT;
            }
            x = 0;
            y += 6;
            continue;
        }
        repeat = 1;
        if (command == '!') {
            count = sixel_sequence_parameters(&p, end, params, 1);
            if (count != 1 || params[0] > 0xffff || p == end) {
                return SIXEL_BAD_INPUT;
            }
            repeat = params[0] == 0 ? 1 : params[0];
            command = *p++;
        }
        if (command < '?' || command > '~' || !raster ||
                x > INT_MAX - repeat) {
            return SIXEL_BAD_INPUT;
        }
        bits = command - '?';
        if (bits != 0) {
            if ((!state->animation && !state->defined[color])
                    || x + repeat > state->width) {
                return SIXEL_BAD_INPUT;
            }
            for (row = 0; row < 6; ++row) {
                if ((bits & (1 << row)) != 0 &&
                        (y >= state->height || row >= state->height - y)) {
                    return SIXEL_BAD_INPUT;
                }
            }
            state->painted = 1;
        }
        x += repeat;
    }
    return SIXEL_BAD_INPUT;
}

struct sixel_sequence_decoder {
    sixel_allocator_t *allocator;
    image_buffer_t image;
    sixel_sequence_state_t state;
    unsigned char const *data;
    unsigned char const *cursor;
    unsigned char const *end;
    sixel_palette_transform_t const *transform;
    size_t passes;
    int anchored;
    int failed;
};

SIXEL_INTERNAL_API SIXELSTATUS
sixel_sequence_open(sixel_sequence_decoder_t **decoder,
    unsigned char const *data, size_t size, int animation,
    sixel_allocator_t *allocator)
{
    sixel_sequence_decoder_t *d;
    SIXELSTATUS status;

    if (decoder == NULL) { return SIXEL_BAD_ARGUMENT; }
    *decoder = NULL;
    if (data == NULL || size == 0U || size > INT_MAX) {
        return SIXEL_BAD_INPUT;
    }
    if (allocator == NULL) {
        status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
        if (SIXEL_FAILED(status)) { return status; }
    } else {
        sixel_allocator_ref(allocator);
    }
    d = sixel_allocator_calloc(allocator, 1U, sizeof(*d));
    if (d == NULL) {
        sixel_allocator_unref(allocator);
        return SIXEL_BAD_ALLOCATION;
    }
    d->allocator = allocator;
    d->data = data;
    d->cursor = data;
    d->end = data + size;
    d->anchored = -1;
    d->state.animation = animation;
    *decoder = d;
    return SIXEL_OK;
}

SIXEL_INTERNAL_API void
sixel_sequence_close(sixel_sequence_decoder_t *d)
{
    sixel_allocator_t *allocator;

    if (d != NULL) {
        allocator = d->allocator;
        sixel_allocator_free(d->allocator, d->image.pixels.p);
        image_buffer_release_paint_mask(&d->image, d->allocator);
        image_buffer_release_ormode_planes(&d->image, d->allocator);
        image_buffer_release_columns(&d->image, d->allocator);
        sixel_allocator_free(d->allocator, d);
        sixel_allocator_unref(allocator);
    }
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_sequence_next(sixel_sequence_decoder_t *d,
    unsigned char const **rgba, int *width, int *height, size_t *byte_end)
{
    unsigned char const *p;
    unsigned char const *packet;
    parser_context_t context;
    SIXELSTATUS status;

    if (d == NULL || rgba == NULL || width == NULL
            || height == NULL || byte_end == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *rgba = NULL;
    if (d->failed) {
        return SIXEL_BAD_INPUT;
    }
    p = d->cursor;
    while (p < d->end && (*p == ' ' || *p == '\t'
            || *p == '\r' || *p == '\n')) {
        ++p;
    }
    if (p == d->end && d->passes != 0U) {
        d->cursor = p;
        *byte_end = (size_t)(p - d->data);
        return SIXEL_FALSE;
    }
    status = SIXEL_BAD_INPUT;
    if (d->anchored < 0) {
        d->anchored = d->end - p >= 2 && p[0] == 0x1b && p[1] == '7';
        /* img2sixel's non-terminal animation output homes before each
         * frame. Accept that exact fixed-origin profile for video only. */
        if (d->state.animation && d->end - p >= 3
                && p[0] == 0x1b && p[1] == '[' && p[2] == 'H') {
            d->anchored = 2;
        }
    }
    if (d->anchored == 2) {
        if (d->end - p < 3 || p[0] != 0x1b || p[1] != '[' || p[2] != 'H') {
            goto failure;
        }
        p += 3;
    } else if (d->anchored) {
        if (d->end - p < 2 || p[0] != 0x1b
                || p[1] != (d->passes == 0U ? '7' : '8')) {
            goto failure;
        }
        p += 2;
    }
    packet = p;
    status = sixel_sequence_packet(&p, d->end, &d->state);
    if (SIXEL_FAILED(status)) {
        goto failure;
    }
    if (d->image.pixels.p == NULL) {
        if ((size_t)d->state.width >
                SIZE_MAX / (size_t)d->state.height / 4U) {
            status = SIXEL_BAD_INPUT;
            goto failure;
        }
        status = image_buffer_init(&d->image, d->state.width,
            d->state.height, -1, 4, 0, d->allocator);
        if (SIXEL_FAILED(status)) {
            goto failure;
        }
    }
    /* Clear only display samples, not color registers. Transparent samples
     * are flattened against the assessment background after composition. */
    if (d->state.clear) {
        memset(d->image.pixels.p, 0,
            (size_t)d->state.width * (size_t)d->state.height * 4U);
    }
    parser_context_init(&context);
    context.sequence = 1;
    context.color_index = d->state.animation ? 15 : 0;
    context.palette_transform = d->transform;
    status = sixel_decode_raw_impl((unsigned char *)(void *)packet,
        (int)(p - packet), &d->image, &context, d->allocator, NULL, 0,
        SIXEL_DECODE_PIXELS_OPTION_TRUST_RASTER_SIZE, NULL);
    if (SIXEL_FAILED(status)) {
        goto failure;
    }
    d->cursor = p;
    ++d->passes;
    *rgba = d->image.pixels.in_bytes;
    *width = d->state.width;
    *height = d->state.height;
    *byte_end = (size_t)(p - d->data);
    return SIXEL_OK;
failure:
    d->failed = 1;
    sixel_helper_set_additional_message(
        "capture requires complete fixed-raster SIXEL packets, defined "
        "colors, P2=0/1 and valid fixed-anchor framing");
    return status;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_decode_sequence_mapped(unsigned char const *data, size_t size,
    sixel_palette_transform_t const *transform, unsigned char **pixels,
    int *width, int *height, unsigned int *flags, size_t *passes,
    sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    image_buffer_t *image;
    sixel_sequence_state_t *state;
    parser_context_t context;
    unsigned char const *p;
    unsigned char const *end;
    unsigned char const *packet;
    int anchored;

    *pixels = NULL;
    *width = 0;
    *height = 0;
    *flags = 0U;
    *passes = 0U;
    if (data == NULL || size == 0U || size > INT_MAX) {
        return SIXEL_BAD_INPUT;
    }
    image = sixel_allocator_calloc(allocator, 1U, sizeof(*image));
    state = sixel_allocator_calloc(allocator, 1U, sizeof(*state));
    status = SIXEL_BAD_ALLOCATION;
    if (image == NULL || state == NULL) {
        goto end;
    }
    p = data;
    end = data + size;
    anchored = -1;
    while (p < end) {
        while (p < end && (*p == ' ' || *p == '\t' ||
                *p == '\r' || *p == '\n')) {
            ++p;
        }
        if (p == end) {
            break;
        }
        if (anchored < 0) {
            anchored = end - p >= 2 && p[0] == 0x1b && p[1] == '7';
        }
        status = SIXEL_BAD_INPUT;
        if (anchored) {
            if (end - p < 2 || p[0] != 0x1b ||
                    p[1] != (*passes == 0U ? '7' : '8')) {
                goto end;
            }
            p += 2;
        }
        packet = p;
        status = sixel_sequence_packet(&p, end, state);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        if (image->pixels.p == NULL) {
            if ((size_t)state->width >
                    SIZE_MAX / (size_t)state->height / 4U) {
                status = SIXEL_BAD_INPUT;
                goto end;
            }
            status = image_buffer_init(image, state->width, state->height,
                                        -1, 4, 0, allocator);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
        }
        parser_context_init(&context);
        context.sequence = 1;
        context.color_index = 0;
        context.palette_transform = transform;
        status = sixel_decode_raw_impl((unsigned char *)(void *)packet,
            (int)(p - packet), image, &context, allocator, NULL, 0,
            SIXEL_DECODE_PIXELS_OPTION_TRUST_RASTER_SIZE, NULL);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        ++*passes;
    }
    status = SIXEL_BAD_INPUT;
    if (*passes == 0U) {
        goto end;
    }
    *width = state->width;
    *height = state->height;
    *flags = state->flags;
    *pixels = image->pixels.in_bytes;
    image->pixels.p = NULL;
    status = SIXEL_OK;
end:
    if (image != NULL) {
        sixel_allocator_free(allocator, image->pixels.p);
        image_buffer_release_paint_mask(image, allocator);
        image_buffer_release_ormode_planes(image, allocator);
        image_buffer_release_columns(image, allocator);
    }
    sixel_allocator_free(allocator, image);
    sixel_allocator_free(allocator, state);
    if (SIXEL_FAILED(status)) {
        *passes = 0U;
        sixel_helper_set_additional_message(
            "multi-pass input requires complete fixed-raster P2=1 SIXEL "
            "packets with defined colors and valid anchor framing.");
    }
    return status;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_decode_direct_mapped(
    unsigned char *p, int len, unsigned int decode_flags,
    sixel_palette_transform_t const *transform, int body_only,
    int const *body_params, size_t body_nparams, unsigned char **pixels,
    int *pwidth, int *pheight, unsigned char **palette, int *ncolors,
    unsigned int *result_flags, sixel_allocator_t *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    parser_context_t context;
    image_buffer_t *image = NULL;
    int alloc_size;
    int n;

    if (result_flags != NULL) {
        *result_flags = 0U;
    }
    if (pixels != NULL) {
        *pixels = NULL;
    }
    if (palette != NULL) {
        *palette = NULL;
    }
    if (ncolors != NULL) {
        *ncolors = 0;
    }
    if (pixels == NULL || pwidth == NULL || pheight == NULL) {
        sixel_helper_set_additional_message(
            "sixel_decode_direct: output argument is null.");
        return SIXEL_BAD_ARGUMENT;
    }

    if (allocator) {
        sixel_allocator_ref(allocator);
    } else {
        status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
        if (SIXEL_FAILED(status)) {
            allocator = NULL;
            goto error;
        }
    }

    image = (image_buffer_t *)malloc(sizeof(*image));
    if (image == NULL) {
        sixel_helper_set_additional_message(
            "sixel_decode_direct: malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto error;
    }
    image->pixels.p = NULL;
    image->paint_mask = NULL;

    status = sixel_decode_image(p, len, 1, 1, 4U, 0, image, &context, allocator,
                                decode_flags, NULL, body_only, body_params,
                                body_nparams, transform);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    *pwidth = image->width;
    *pheight = image->height;
    if (result_flags != NULL && context.painted_outside_raster) {
        *result_flags |= SIXEL_DECODE_PIXELS_RESULT_PAINT_OUTSIDE_RASTER;
        if ((decode_flags &
                SIXEL_DECODE_PIXELS_OPTION_TRUST_RASTER_SIZE) != 0U) {
            *result_flags |= SIXEL_DECODE_PIXELS_RESULT_CLIPPED_TO_RASTER;
        }
    }
    if (result_flags != NULL && context.palette_redefined_after_paint) {
        *result_flags |= SIXEL_DECODE_PIXELS_RESULT_PALETTE_REDEFINED;
    }
    if (palette != NULL && ncolors != NULL) {
        *ncolors = alloc_size = image->ncolors;
        if (alloc_size < SIXEL_PALETTE_MAX_DECODER) {
            alloc_size = SIXEL_PALETTE_MAX_DECODER;
        }
        *palette = (unsigned char *)sixel_allocator_malloc(
            allocator,
            (size_t)alloc_size * 3u);
        if (*palette == NULL) {
            sixel_helper_set_additional_message(
                "sixel_decode_direct: palette allocation failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto error;
        }
        for (n = 0; n < alloc_size; ++n) {
            (*palette)[n * 3 + 0] = image->palette[n] >> 16 & 0xff;
            (*palette)[n * 3 + 1] = image->palette[n] >> 8 & 0xff;
            (*palette)[n * 3 + 2] = image->palette[n] & 0xff;
        }
    }
    *pixels = image->pixels.in_bytes;
    image->pixels.p = NULL;

    status = SIXEL_OK;
    goto end;

error:
    if (image != NULL && image->pixels.p != NULL) {
        if (allocator != NULL) {
            sixel_allocator_free(allocator, image->pixels.p);
        } else {
            free(image->pixels.p);
        }
        image->pixels.p = NULL;
    }

end:
    image_buffer_release_paint_mask(image, allocator);
    image_buffer_release_ormode_planes(image, allocator);
    image_buffer_release_columns(image, allocator);
    free(image);
    sixel_allocator_unref(allocator);
    return status;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_decode_direct_with_options(
    unsigned char       *p,
    int                  len,
    unsigned int         decode_flags,
    unsigned char      **pixels,
    int                 *pwidth,
    int                 *pheight,
    unsigned char      **palette,
    int                 *ncolors,
    unsigned int        *result_flags,
    sixel_allocator_t   *allocator)
{
    return sixel_decode_direct_mapped(p, len, decode_flags, NULL, 0, NULL, 0U,
                                      pixels, pwidth, pheight, palette, ncolors,
                                      result_flags, allocator);
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_decode_direct_body_with_options(
    unsigned char       *p,
    int                  len,
    unsigned int         decode_flags,
    int const           *params,
    size_t               nparams,
    unsigned char      **pixels,
    int                 *pwidth,
    int                 *pheight,
    unsigned char      **palette,
    int                 *ncolors,
    unsigned int        *result_flags,
    sixel_allocator_t   *allocator)
{
    return sixel_decode_direct_mapped(p, len, decode_flags, NULL, 1, params,
                                      nparams, pixels, pwidth, pheight, palette,
                                      ncolors, result_flags, allocator);
}


SIXELAPI SIXELSTATUS
sixel_decode_direct(
    unsigned char       *p,
    int                  len,
    unsigned char      **pixels,
    int                 *pwidth,
    int                 *pheight,
    sixel_allocator_t   *allocator)
{
    return sixel_decode_direct_with_options(p,
                                            len,
                                            0U,
                                            pixels,
                                            pwidth,
                                            pheight,
                                            NULL,
                                            NULL,
                                            NULL,
                                            allocator);
}


/* deprecated */
SIXELAPI SIXELSTATUS
sixel_decode(unsigned char              /* in */   *p,
             int                        /* in */    len,
             unsigned char              /* out */ **pixels,
             int                        /* out */  *pwidth,
             int                        /* out */  *pheight,
             unsigned char              /* out */ **palette,
             int                        /* out */  *ncolors,
             sixel_allocator_function   /* in */    fn_malloc)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_allocator_t *allocator;
    unsigned char *raw_pixels;
    unsigned char *raw_palette;
    unsigned char *legacy_palette;
    size_t palette_bytes;

    allocator = NULL;
    raw_pixels = NULL;
    raw_palette = NULL;
    legacy_palette = NULL;
    palette_bytes = 0U;

    if (p == NULL || pixels == NULL || pwidth == NULL ||
            pheight == NULL || palette == NULL || ncolors == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *pixels = NULL;
    *palette = NULL;
    *pwidth = 0;
    *pheight = 0;
    *ncolors = 0;

    status = sixel_allocator_new(&allocator, fn_malloc, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        allocator = NULL;
        goto end;
    }

    /*
     * Keep the deprecated API on top of sixel_decode_raw() so every decoder
     * entry point observes the same OR-mode bit-plane and background rules.
     * The old private path used depth 0, which bypassed those invariants and
     * could leave palette-index 0 ambiguous for callers that still use this
     * compatibility function.
     *
     * This wrapper returns a compact palette with only the public color range.
     * Pixel values outside [0, *ncolors) keep the raw transparent/unpainted
     * sentinel meaning and must not be used to index that palette.
     */
    status = sixel_decode_raw(p,
                              len,
                              &raw_pixels,
                              pwidth,
                              pheight,
                              &raw_palette,
                              ncolors,
                              allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (*ncolors <= 0 || *ncolors > SIXEL_PALETTE_MAX_DECODER) {
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    palette_bytes = (size_t)*ncolors * 3U;
    if (palette_bytes / 3U != (size_t)*ncolors) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto end;
    }
    legacy_palette = (unsigned char *)sixel_allocator_malloc(allocator,
                                                             palette_bytes);
    if (legacy_palette == NULL) {
        sixel_helper_set_additional_message(
            "sixel_decode: palette allocation failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    memcpy(legacy_palette, raw_palette, palette_bytes);

    *pixels = raw_pixels;
    *palette = legacy_palette;
    raw_pixels = NULL;
    legacy_palette = NULL;
    status = SIXEL_OK;

end:
    if (raw_palette != NULL && allocator != NULL) {
        sixel_allocator_free(allocator, raw_palette);
    }
    if (legacy_palette != NULL && allocator != NULL) {
        sixel_allocator_free(allocator, legacy_palette);
    }
    if (raw_pixels != NULL && allocator != NULL) {
        sixel_allocator_free(allocator, raw_pixels);
    }
    sixel_allocator_unref(allocator);
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
