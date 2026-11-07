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
#include "fromsixel-internal.h"
#include "decoder-ormode.h"
#include "decoder-columns.h"
#include "decoder-simd.h"
#include "cpu.h"
#include "decoder.h"
#include "decoder-parallel.h"
#include "sixel_decode_pixels.h"
#include "timeline-logger.h"

#define SIXEL_RGB(r, g, b) (((r) << 16) + ((g) << 8) +  (b))

#define PALVAL(n,a,m) (((n) * (a) + ((m) / 2)) / (m))

#define SIXEL_XRGB(r,g,b) SIXEL_RGB(PALVAL(r, 255, 100), PALVAL(g, 255, 100), PALVAL(b, 255, 100))

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
image_buffer_finalize_normal_columns(image_buffer_t *image,
                                      uint64_t const *source,
                                      int first, int end)
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
    int x0;
#if SIXEL_DECODER_NEON
    uint8x16_t indices;
    uint8x16_t painted;
    uint32_t block[16];
    unsigned char lanes[16];
    int i;
    int simd;
#endif

    if (source == NULL || (image->depth != 1 && image->depth != 2 &&
            image->depth != 4)) {
        return;
    }
#if SIXEL_DECODER_NEON
    simd = sixel_cpu_simd_level() == SIXEL_SIMD_LEVEL_NEON;
#endif
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
        columns = source + (size_t)(band - first) * width;
        rows = image->height - band * 6;
        if (rows > 6) {
            rows = 6;
        }
        for (r = 0; r < rows; ++r) {
            row = ((size_t)band * 6u + (size_t)r) * width;
            shift = r * 8;
            x0 = 0;
#if SIXEL_DECODER_NEON
            if (simd && image->depth != 2) {
                bytes = image->depth == 1 ?
                    image->pixels.in_bytes + row :
                    image->pixels.in_bytes + row * 4u;
                coverage = image->paint_mask == NULL ? NULL :
                    image->paint_mask + row;
                for (; x0 + 16 <= image->width; x0 += 16) {
                    indices = sixel_simd_columns_row16(columns + x0, r,
                                                       &painted);
                    if (image->depth == 1) {
                        vst1q_u8(bytes + x0,
                                 vbslq_u8(painted, indices,
                                          vld1q_u8(bytes + x0)));
                    } else {
                        vst1q_u8(lanes, indices);
                        for (i = 0; i < 16; ++i) {
                            block[i] = lut[lanes[i]];
                        }
                        sixel_simd_blend_rgba16(bytes + (size_t)x0 * 4u,
                                                block, painted);
                    }
                    if (coverage != NULL) {
                        vst1q_u8(coverage + x0,
                                 vorrq_u8(vld1q_u8(coverage + x0), painted));
                    }
                }
            }
#endif
            if (image->depth == 1) {
                bytes = image->pixels.in_bytes + row;
                for (x = x0; x < image->width; ++x) {
                    mask = ((columns[x] >> (48 + r)) & 1u) ? UINT32_MAX : 0u;
                    value = (unsigned char)(columns[x] >> shift);
                    bytes[x] = (unsigned char)((bytes[x] & ~mask) |
                                               (value & mask));
                }
            } else if (image->depth == 2) {
                shorts = image->pixels.in_shorts + row;
                for (x = x0; x < image->width; ++x) {
                    mask = ((columns[x] >> (48 + r)) & 1u) ? UINT32_MAX : 0u;
                    value = (unsigned char)(columns[x] >> shift);
                    shorts[x] = (unsigned short)((shorts[x] & ~mask) |
                                                 (value & mask));
                }
            } else {
                bytes = image->pixels.in_bytes + row * 4u;
                for (x = x0; x < image->width; ++x) {
                    mask = ((columns[x] >> (48 + r)) & 1u) ? UINT32_MAX : 0u;
                    memcpy(&value, bytes + (size_t)x * 4u, sizeof(value));
                    value = (value & ~mask) |
                        (lut[(columns[x] >> shift) & 255u] & mask);
                    memcpy(bytes + (size_t)x * 4u, &value, sizeof(value));
                }
            }
            if (image->paint_mask != NULL) {
                coverage = image->paint_mask + row;
                for (x = x0; x < image->width; ++x) {
                    coverage[x] |= (unsigned char)(
                        ((columns[x] >> (48 + r)) & 1u) ? 255u : 0u);
                }
            }
        }
    }
}

SIXEL_INTERNAL_API void
image_buffer_finalize_normal_bands(image_buffer_t *image, int first, int end)
{
    if (image->normal_columns != NULL) {
        image_buffer_finalize_normal_columns(image, image->normal_columns +
            (size_t)first * (size_t)image->width, first, end);
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
    int x0;
#if SIXEL_DECODER_NEON
    uint8x16_t indices;
    uint8x16_t vector_max;
    unsigned char block[16];
    int i;
    int simd;
#endif

    if (image == NULL || image->ormode_planes == NULL) {
        return;
    }
#if SIXEL_DECODER_NEON
    simd = sixel_cpu_simd_level() == SIXEL_SIMD_LEVEL_NEON;
    vector_max = vdupq_n_u8(0);
#endif
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
                x0 = 0;
#if SIXEL_DECODER_NEON
                if (simd) {
                    for (; x0 + 16 <= image->width; x0 += 16) {
                        indices = sixel_simd_gather16(low + x0, r);
                        vector_max = vmaxq_u8(vector_max, indices);
                        vst1q_u8(bytes + x0, indices);
                    }
                }
#endif
                for (x = x0; x < image->width; ++x) {
                    index = sixel_ormode_gather(low[x], r);
                    max_index = index > max_index ? index : max_index;
                    bytes[x] = (unsigned char)index;
                }
            } else if (high == NULL && image->depth == 4U) {
                bytes = image->pixels.in_bytes + row * 4u;
                x0 = 0;
#if SIXEL_DECODER_NEON
                if (simd) {
                    for (; x0 + 16 <= image->width; x0 += 16) {
                        indices = sixel_simd_gather16(low + x0, r);
                        vector_max = vmaxq_u8(vector_max, indices);
                        vst1q_u8(block, indices);
                        for (i = 0; i < 16; ++i) {
                            memcpy(bytes + (size_t)(x0 + i) * 4u,
                                   lut[block[i]], 4u);
                        }
                    }
                }
#endif
                for (x = x0; x < image->width; ++x) {
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
#if SIXEL_DECODER_NEON
    if (vmaxvq_u8(vector_max) > max_index) {
        max_index = vmaxvq_u8(vector_max);
    }
#endif
    if ((int)max_index + 1 > image->ncolors) {
        image->ncolors = (int)max_index + 1;
    }
    if (image->paint_mask != NULL) {
        memset(image->paint_mask, 0xff,
               (size_t)image->width * image->height);
    }
}



/*
 * Token fast lane shared by the band-word runs.
 *
 * Most of a typical stream is a short literal run followed by a register
 * selection "#Pc" or a repeat "!Pn". Returning to the state switch for each
 * of them costs several state transitions and the general paint checks, so
 * the runs below consume these tokens in place when their effect is fully
 * known: a complete selection of a byte register, a complete repeat that
 * fits before the run width, or a carriage return "$". Anything else --
 * definitions, wide registers, clipping, a token cut by the end of input,
 * or numbers long enough to need the overflow diagnostics -- is left
 * unconsumed for the general parser. Parsing a number has no side effects,
 * so declining after looking ahead is always safe.
 *
 * The runs are entered only after the general parser has handled a data
 * character for the current band, which already settled the parallel-start
 * decision; selections, repeats and "$" cannot change that decision until
 * a raster attribute reaches the general parser again.
 */
#define SIXEL_FAST_DIGITS_MAX 5

/* Read at most SIXEL_FAST_DIGITS_MAX decimal digits. Returns the byte after
 * the number, or NULL when the number is longer or reaches end. */
static unsigned char *
parser_fast_number(unsigned char *p, unsigned char *end, int *value)
{
    int v;
    int n;

    v = 0;
    n = 0;
    while (p < end && *p >= '0' && *p <= '9') {
        if (++n > SIXEL_FAST_DIGITS_MAX) {
            return NULL;
        }
        v = v * 10 + (*p++ - '0');
    }
    if (p >= end) {
        return NULL;
    }
    *value = v;
    return p;
}

/* Fold a run's last painted column and row union into the paint extents. */
static void
parser_context_note_extent(parser_context_t *context, int last, int rows)
{
    int r;

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
}

/* Apply "#Pc" without parameters exactly as the general DECGCI path does
 * for a byte register; the caller declines wide registers. */
static void
parser_context_fast_select(parser_context_t *context, image_buffer_t *image,
                           int index)
{
    context->color_index = index;
    context->params[0] = index;
    context->nparams = 1;
    context->param = 0;
    if (index + 1 > image->ncolors) {
        image->ncolors = index + 1;
    }
}


/* Consume adjacent unrepeated data bytes with one plane-word store each,
 * together with the selections, repeats and carriage returns between them
 * (see the fast lane above). The caller has proved a complete band fits
 * the allocated raster. Stop at the right edge so the ordinary parser
 * still owns growth and clipping. */
static unsigned char *
parser_context_store_ormode_run(parser_context_t *context,
                                image_buffer_t *image,
                                unsigned char *p, unsigned char *end,
                                int width)
{
    uint64_t *low;
    uint64_t *high;
    uint64_t spread;
    uint64_t high_spread;
    unsigned char *q;
    size_t band;
    int x;
    int last;
#if SIXEL_DECODER_NEON
    int block_last;
#endif
    int bits;
    int rows;
    int count;

    /* An incomplete plane allocation must use the checked parser path. */
    if (image->ormode_planes == NULL ||
            (context->ormode_high_spread != 0 &&
             image->ormode_high_planes == NULL)) {
        return p;
    }
    band = (size_t)(context->pos_y / 6) * (size_t)image->width;
    low = image->ormode_planes + band;
    high = image->ormode_high_planes == NULL ? NULL :
        image->ormode_high_planes + band;
    spread = context->ormode_spread;
    high_spread = context->ormode_high_spread;
    x = context->pos_x;
    last = -1;
    rows = 0;
    count = context->repeat_count;
    context->repeat_count = 1;
    for (;;) {
        if (count > 1) {
            /* The repeat is known to fit before width. */
            bits = *p++ - '?';
            sixel_ormode_store(low + x, spread, bits, count);
            if (high_spread != 0) {
                sixel_ormode_store(high + x, high_spread, bits, count);
            }
            x += count;
            if (bits != 0) {
                last = x - 1;
                rows |= bits;
            }
        }
#if SIXEL_DECODER_NEON
        if (context->simd_neon && high_spread == 0) {
            while (end - p >= 16 && width - x >= 16 &&
                    p[15] >= '?' && p[15] <= '~' &&
                    sixel_simd_planes_or16(low + x, spread, p, &rows,
                                           &block_last)) {
                if (block_last >= 0) {
                    last = x + block_last;
                }
                p += 16;
                x += 16;
            }
        }
#endif
        while (p < end && *p >= '?' && *p <= '~' && x < width) {
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
        if (p >= end) {
            break;
        }
        if (*p == '!') {
            q = parser_fast_number(p + 1, end, &count);
            if (q == NULL || *q < '?' || *q > '~') {
                break;
            }
            if (count > 0xffff || count > width - x) {
                break;
            }
            context->param = 0;
            context->nparams = 0;
            p = q;
            if (count <= 1) {
                /* "!0" and "!1" paint one cell: an ordinary literal. */
                count = 0;
            }
            continue;
        }
        if (*p == '#') {
            q = parser_fast_number(p + 1, end, &count);
            /* ESC abandons the selection in the general parser. */
            if (q == NULL || *q == ';' || *q == '\x1b' || count >= 256) {
                break;
            }
            parser_context_fast_select(context, image, count);
            spread = sixel_ormode_spread((unsigned int)count);
            high_spread = 0;
            context->ormode_spread = spread;
            context->ormode_high_spread = 0;
            count = 0;
            p = q;
            continue;
        }
        if (*p == '$') {
            context->pos_x = x;
            parser_context_note_extent(context, last, rows);
            last = -1;
            rows = 0;
            x = 0;
            count = 0;
            ++p;
            continue;
        }
        break;
    }
    context->pos_x = x;
    parser_context_note_extent(context, last, rows);
    return p;
}


/* Consume a contiguous run, and the selections, repeats and carriage
 * returns between runs, without returning to the control-state switch.
 * Full-band bounds have already been checked. Aggregate paint extents once
 * per run; an unset sixel advances x but neither paints nor grows extents. */
static unsigned char *
parser_context_store_normal_run(parser_context_t *context,
                                image_buffer_t *image,
                                unsigned char *p, unsigned char *end,
                                int width)
{
    uint64_t *columns;
    uint64_t color;
    uint64_t mask;
    unsigned char *q;
    int x;
    int last;
    int rows;
    int bits;
    int count;

    columns = image->normal_columns +
        (size_t)(context->pos_y / 6) * (size_t)image->width;
    color = context->normal_color;
    x = context->pos_x;
    last = -1;
    rows = 0;
    if (image->depth != 4 && context->color_index > image->ncolors) {
        image->ncolors = context->color_index;
    }
    count = context->repeat_count;
    context->repeat_count = 1;
    for (;;) {
        if (count > 1) {
            /* The repeat is known to fit before width. */
            bits = *p++ - '?';
            sixel_column_store(columns + x, color, bits, count);
            x += count;
            if (bits != 0) {
                last = x - 1;
                rows |= bits;
            }
        }
        while (p < end && *p >= '?' && *p <= '~' && x < width) {
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
        if (p >= end) {
            break;
        }
        if (*p == '!') {
            q = parser_fast_number(p + 1, end, &count);
            if (q == NULL || *q < '?' || *q > '~') {
                break;
            }
            if (count > 0xffff || count > width - x) {
                break;
            }
            context->param = 0;
            context->nparams = 0;
            p = q;
            if (count <= 1) {
                /* "!0" and "!1" paint one cell: an ordinary literal. */
                count = 0;
            }
            continue;
        }
        if (*p == '#') {
            q = parser_fast_number(p + 1, end, &count);
            /* ESC abandons the selection in the general parser. */
            if (q == NULL || *q == ';' || *q == '\x1b' || count >= 256) {
                break;
            }
            parser_context_fast_select(context, image, count);
            color = sixel_column_color((unsigned int)count);
            context->normal_color = color;
            count = 0;
            p = q;
            continue;
        }
        if (*p == '$') {
            context->pos_x = x;
            parser_context_note_extent(context, last, rows);
            last = -1;
            rows = 0;
            x = 0;
            count = 0;
            ++p;
            continue;
        }
        break;
    }
    context->pos_x = x;
    parser_context_note_extent(context, last, rows);
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

    image->capture = 0;
    image->capture_defined = NULL;
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
image_buffer_expand_rows(unsigned char **pixels, size_t stride,
                         int height, int scale, sixel_allocator_t *allocator)
{
    unsigned char *expanded;
    int y;

    if (*pixels == NULL) {
        return SIXEL_OK;
    }
    if (stride > SIZE_MAX / (size_t)height) {
        return SIXEL_BAD_INPUT;
    }
    expanded = sixel_allocator_malloc(allocator, stride * (size_t)height);
    if (expanded == NULL) {
        sixel_helper_set_additional_message(
            "image_buffer_expand_rows: allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }
    for (y = 0; y < height; ++y) {
        memcpy(expanded + (size_t)y * stride,
               *pixels + (size_t)(y / scale) * stride, stride);
    }
    sixel_allocator_free(allocator, *pixels);
    *pixels = expanded;
    return SIXEL_OK;
}

/* Constant-aspect painters use unscaled rows. Expand their materialized
 * result, coverage and separate fused-undither output exactly once. The
 * changing-aspect painter has already produced device rows. */
static SIXELSTATUS
image_buffer_expand_vertical(image_buffer_t *image,
    parser_context_t const *context, sixel_allocator_t *allocator,
    sixel_decoder_undither_context_t *undither)
{
    SIXELSTATUS status;
    int height;
    int scale;

    scale = context->vertical_scale;
    if (context->changing_aspect || scale == 1) {
        return SIXEL_OK;
    }
    if (context->output_pv > 0 &&
            image->height == context->attributed_pv) {
        /* The last source row can represent a partial output row group.
         * Rounding Pv up internally must not grow the declared image. */
        height = context->output_pv;
    } else {
        if (image->height > SIXEL_HEIGHT_LIMIT / scale) {
            sixel_helper_set_additional_message(
                "image_buffer_expand_vertical: height is too huge.");
            return SIXEL_BAD_INPUT;
        }
        height = image->height * scale;
    }
    status = image_buffer_expand_rows(&image->pixels.in_bytes,
        (size_t)image->width * image->depth, height, scale, allocator);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    status = image_buffer_expand_rows(&image->paint_mask,
        (size_t)image->width, height, scale, allocator);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    if (undither != NULL && undither->pixels != NULL) {
        status = image_buffer_expand_rows(&undither->pixels,
            (size_t)image->width * undither->pixel_size,
            height, scale, allocator);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }
    image->height = height;
    return SIXEL_OK;
}

static SIXELSTATUS
parser_context_init(parser_context_t *context)
{
    SIXELSTATUS status = SIXEL_FALSE;

    context->state = PS_GROUND;
    context->palette_transform = NULL;
#if SIXEL_DECODER_NEON
    context->simd_neon = sixel_cpu_simd_level() == SIXEL_SIMD_LEVEL_NEON;
#else
    context->simd_neon = 0;
#endif
    context->pos_x = 0;
    context->pos_y = 0;
    context->max_x = 0;
    context->max_y = 0;
    context->attributed_pan = 1;
    context->attributed_pad = 1;
    context->attributed_ph = 0;
    context->output_pv = 0;
    context->attributed_pv = 0;
    context->vertical_scale = 1;
    context->changing_aspect = 0;
    context->aspect_resume = NULL;
    context->aspect_ormode = NULL;
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
    context->drawing_control = 0;
    context->palette_redefined_after_paint = 0;
    context->sequence = 0;
    context->capture = 0;
    context->capture_defined = NULL;
    context->capture_definitions = 0;
    context->capture_columns = NULL;
    context->capture_columns_height = 0;
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
     * "DCS P1;P2;P3 q". P1 does not affect decoded pixel geometry.
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
    int macro_pad;

    status = SIXEL_FALSE;
    scaled_pan = 0;
    scaled_pad = 0;
    macro_pad = 1;

    if (context->nparams > 0) {
        /* Pn1 */
        switch (context->params[0]) {
        case 0:
        case 1:
            macro_pad = 2;
            break;
        case 2:
            macro_pad = 5;
            break;
        case 3:
        case 4:
            macro_pad = 4;
            break;
        case 5:
        case 6:
            macro_pad = 3;
            break;
        case 7:
        case 8:
            macro_pad = 2;
            break;
        case 9:
            macro_pad = 1;
            break;
        default:
            macro_pad = 2;
            break;
        }
    }

    if (parser_context_is_ormode_request(context)) {
        context->ormode = 1;
        status = context->changing_aspect
            ? sixel_aspect_enable_ormode(image, context, allocator)
            : image_buffer_enable_ormode(image, allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        context->ormode_spread = sixel_ormode_spread(
            (unsigned int)context->color_index);
        context->ormode_high_spread = sixel_ormode_spread(
            (unsigned int)context->color_index >> 8);
        if (!context->changing_aspect &&
                context->ormode_high_spread != 0) {
            image->ormode_high_planes = image_buffer_allocate_planes(
                image->width, image->height, allocator);
            if (image->ormode_high_planes == NULL) {
                status = SIXEL_BAD_ALLOCATION;
                goto end;
            }
        }
    }

    if (context->nparams > 2) {
        /* Preserve legacy P3 overflow checks without setting the aspect. */
        if (context->params[2] == 0) {
            context->params[2] = 10;
        }

        status = safe_multiply_by_params_div10(2,
                                               context->params[2],
                                               &scaled_pan);
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        status = safe_multiply_by_params_div10(macro_pad,
                                               context->params[2],
                                               &scaled_pad);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    context->nparams = 0;
    context->state = PS_DECSIXEL;
    status = SIXEL_OK;

end:
    return status;
}


/* Private results never cross an API boundary. Strict packets replay the
 * reference validator only for exceptional syntax or paint-time palettes. */
enum { DECODE_RESTART_ASPECT = -1, DECODE_RESTART_EXACT = -2 };

/* A segment owns source rows; only this compositor knows device rows.
 * Raster-size edits are composition checkpoints within a constant-scale
 * segment: publish under the old clip before changing that clip. */
typedef struct aspect_segment {
    image_buffer_t *canvas;
    int out_y;
    int max_x;
    int max_y;
    int changes;
    int bands;
} aspect_segment_t;

/* Internal, process-local test switch. Tests change it only between calls;
 * production callers never write it and ordinary decoding never reads it. */
static int aspect_reference;

SIXEL_INTERNAL_API void
sixel_decode_aspect_reference(int enabled)
{
    aspect_reference = enabled;
}

static SIXELSTATUS
sixel_segment_compose(image_buffer_t *patch, parser_context_t *context,
    aspect_segment_t *segment, int trust, sixel_allocator_t *allocator)
{
    image_buffer_t *canvas;
    SIXELSTATUS status;
    unsigned char const *mask;
    unsigned char const *first;
    unsigned char const *source;
    unsigned char *dest;
    size_t row;
    size_t output;
    int bottom;
    int width;
    int height;
    int y;
    int dy;
    int x;
    int end;
    int stop;
    int scale;
    int painted;

    canvas = segment->canvas;
    scale = context->vertical_scale;
    bottom = context->max_y < 0 ? 0
        : segment->out_y + (context->max_y + 1) * scale;
    if (trust && context->output_pv > 0 && bottom > context->output_pv) {
        bottom = context->output_pv;
    }
    width = canvas->width > patch->width ? canvas->width : patch->width;
    height = canvas->height > bottom ? canvas->height : bottom;
    if (width != canvas->width || height != canvas->height) {
        status = image_buffer_resize(canvas, width, height,
            context->bgindex, allocator);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }
    /* Unused tail bands can be large when a DECGRA splits a declared
     * raster halfway through. Only publish bands that contain paint. */
    image_buffer_finalize_normal_bands(patch, 0,
        (context->max_y + 6) / 6);
    image_buffer_release_columns(patch, allocator);
    painted = 0;
    for (y = 0; y <= context->max_y; ++y) {
        row = (size_t)y * patch->width;
        mask = patch->paint_mask + row;
        first = memchr(mask, 0xff, (size_t)patch->width);
        if (first == NULL) {
            continue;
        }
        painted = 1;
        end = segment->out_y + (y + 1) * scale;
        if (end > bottom) {
            end = bottom;
        }
        /* Copy covered horizontal spans, then multiply them vertically.
         * Dense images take one memcpy per row; holes never erase pixels. */
        while (first != NULL) {
            x = (int)(first - mask);
            first = memchr(mask + x, 0, (size_t)(patch->width - x));
            stop = first == NULL ? patch->width : (int)(first - mask);
            source = patch->pixels.in_bytes + (row + x) * patch->depth;
            for (dy = segment->out_y + y * scale; dy < end; ++dy) {
                output = (size_t)dy * canvas->width + x;
                dest = canvas->pixels.in_bytes + output * canvas->depth;
                memcpy(dest, source, (size_t)(stop - x) * canvas->depth);
                if (canvas->paint_mask != NULL) {
                    memset(canvas->paint_mask + output, 0xff,
                        (size_t)(stop - x));
                }
            }
            first = memchr(mask + stop, 0xff,
                (size_t)(patch->width - stop));
        }
    }
    if (context->max_x > segment->max_x) {
        segment->max_x = context->max_x;
    }
    if (painted && bottom - 1 > segment->max_y) {
        segment->max_y = bottom - 1;
    }
    memcpy(canvas->palette, patch->palette, sizeof(canvas->palette));
    canvas->ncolors = patch->ncolors;
    segment->out_y += context->pos_y * scale;
    context->pos_y = 0;
    context->max_x = -1;
    context->max_y = -1;
    memset(patch->paint_mask, 0, (size_t)patch->width * patch->height);
    return SIXEL_OK;
}

/* Rebase only geometry. Parser state, x, repeat, register selection, palette
 * history and capture validation state survive every composition checkpoint. */
static SIXELSTATUS
sixel_segment_raster(image_buffer_t *patch, parser_context_t *context,
    aspect_segment_t *segment, sixel_allocator_t *allocator)
{
    image_buffer_t *canvas;
    SIXELSTATUS status;
    int width;
    int height;
    int rows;

    canvas = segment->canvas;
    width = canvas->width > context->attributed_ph
        ? canvas->width : context->attributed_ph;
    height = canvas->height > context->output_pv
        ? canvas->height : context->output_pv;
    if (width != canvas->width || height != canvas->height) {
        status = image_buffer_resize(canvas, width, height,
            context->bgindex, allocator);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }
    rows = context->output_pv - segment->out_y;
    context->attributed_pv = rows > 0
        ? 1 + (rows - 1) / context->vertical_scale : 0;
    rows = height - segment->out_y;
    rows = rows > 0 ? 1 + (rows - 1) / context->vertical_scale : 1;
    if (context->max_y >= 0 && rows < patch->height) {
        rows = patch->height;
    }
    if (width < patch->width) {
        width = patch->width;
    }
    if (width == patch->width && rows == patch->height) {
        return SIXEL_OK;
    }
    return image_buffer_resize(patch, width, rows, context->bgindex,
        allocator);
}

static int sixel_sequence_parameters(unsigned char const **cursor,
    unsigned char const *end, int *params, int capacity);

/* Convert sixel data into indexed pixel bytes and palette data. */
/* Constant instantiations keep strict capture validation and reusable
 * packet storage out of the ordinary serial state machine. */
#if defined(_MSC_VER)
static __forceinline int
#elif defined(__GNUC__) || defined(__clang__)
static inline __attribute__((always_inline)) int
#else
static inline int
#endif
sixel_decode_raw_body(
    unsigned char     *p,         /* sixel bytes */
    int                len,       /* size of sixel bytes */
    image_buffer_t    *image,
    parser_context_t  *context,
    sixel_allocator_t *allocator, /* allocator object */
    sixel_timeline_logger_t    *logger,
    int logger_prepared,
    unsigned int decode_flags,
    sixel_decoder_undither_context_t *undither, int capture,
    aspect_segment_t *segment)
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
    int run_width;
    int target_bottom;
    int drawn_y_end;
    int store_bits;
    int advance;
    int scale;
    int rescale_head;
    size_t pos;
    unsigned char *p0 = p;
    unsigned char const *capture_end;
    int capture_count;
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
                if (segment != NULL &&
                        parser_context_is_ormode_request(context)) {
                    status = sixel_segment_compose(image, context, segment,
                        trust_raster_size, allocator);
                    if (SIXEL_FAILED(status)) {
                        goto end;
                    }
                    context->pos_y = segment->out_y;
                    context->max_x = segment->max_x;
                    context->max_y = segment->max_y;
                    context->attributed_pv = context->output_pv;
                    image = segment->canvas;
                    segment = NULL;
                    context->changing_aspect = 1;
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
            if (capture && *p != '"' && *p != '!' && *p != '#' &&
                    *p != '$' && *p != '-' && (*p < '?' || *p > '~')) {
                if (*p == 0x9c || (p0 + len - p >= 2 &&
                        p[0] == 0x1b && p[1] == '\\')) {
                    goto finalize;
                }
                return DECODE_RESTART_EXACT;
            }
            switch (*p) {
            case '\x1b':
                context->state = PS_ESC;
                p++;
                break;
            case '"':
                if (capture) {
                    capture_end = p + 1;
                    capture_count = sixel_sequence_parameters(&capture_end,
                        p0 + len, context->params, 4);
                    if (capture_count != 4 ||
                            context->params[2] != image->width ||
                            context->params[3] != context->output_pv) {
                        return DECODE_RESTART_EXACT;
                    }
                    p = (unsigned char *)(void *)capture_end;
                    context->nparams = 3;
                    context->param = context->params[3];
                    goto sequence_raster_parameters;
                }
                context->param = 0;
                context->nparams = 0;
                context->state = PS_DECGRA;
                p++;
                break;
            case '!':
                if (capture) {
                    capture_end = p + 1;
                    capture_count = sixel_sequence_parameters(&capture_end,
                        p0 + len, context->params, 1);
                    if (capture_count != 1 || capture_end == p0 + len ||
                            *capture_end < '?' || *capture_end > '~' ||
                            context->params[0] > 0xffff) {
                        return DECODE_RESTART_EXACT;
                    }
                    context->drawing_control = 1;
                    context->repeat_count = context->params[0] == 0
                        ? 1 : context->params[0];
                    p = (unsigned char *)(void *)capture_end;
                    break;
                }
                context->drawing_control = 1;
                context->param = 0;
                context->nparams = 0;
                context->state = PS_DECGRI;
                p++;
                break;
            case '#':
                if (capture) {
                    capture_end = p + 1;
                    capture_count = sixel_sequence_parameters(&capture_end,
                        p0 + len, context->params, 5);
                    if ((capture_count != 1 && capture_count != 5) ||
                            context->params[0] >= SIXEL_PALETTE_MAX_DECODER) {
                        return DECODE_RESTART_EXACT;
                    }
                    if (capture_count == 5) {
                        if (context->params[1] != 1 &&
                                context->params[1] != 2) {
                            return DECODE_RESTART_EXACT;
                        }
                        context->capture_definitions = 1;
                        if (context->capture_defined != NULL) {
                            context->capture_defined[context->params[0]] = 1;
                        }
                    }
                    p = (unsigned char *)(void *)capture_end;
                    context->nparams = capture_count - 1;
                    context->param = context->params[capture_count - 1];
                    goto sequence_color_parameters;
                }
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
                context->drawing_control = 1;
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
                context->drawing_control = 1;
                context->pos_x = 0;
                if (segment != NULL) {
                    ++segment->bands;
                    if ((int64_t)segment->out_y +
                            ((int64_t)context->pos_y + 6) *
                            context->vertical_scale > INT_MAX) {
                        status = SIXEL_BAD_INPUT;
                        goto end;
                    }
                }
                advance = context->changing_aspect
                    ? 6 * context->vertical_scale : 6;
                if (capture && !context->changing_aspect &&
                        context->pos_y >
                            INT_MAX / context->vertical_scale - 6) {
                    return DECODE_RESTART_EXACT;
                }
                if (context->pos_y > INT_MAX - advance) {
                    status = SIXEL_BAD_INPUT;
                    goto end;
                }
                context->pos_y += advance;
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
                    if (capture &&
                            (context->attributed_ph == 0 ||
                             (context->capture_defined != NULL &&
                              !context->capture_defined[
                                  context->color_index]))) {
                        return DECODE_RESTART_EXACT;
                    }
                    context->painted = 1;
                    if (segment != NULL) {
                        /* Full runs need no per-pixel device arithmetic.
                         * Check the band once before entering byte lanes. */
                        if ((int64_t)segment->out_y +
                                ((int64_t)context->pos_y + 6) *
                                context->vertical_scale > INT_MAX) {
                            status = SIXEL_BAD_INPUT;
                            goto end;
                        }
                        sy = segment->out_y + context->pos_y *
                            context->vertical_scale;
                        target_bottom = sy + 6 * context->vertical_scale;
                        if (trust_raster_size && context->output_pv > 0 &&
                                target_bottom > context->output_pv) {
                            target_bottom = context->output_pv;
                            for (i = 0; i < 6; ++i) {
                                if (((*p - '?') & (1 << i)) != 0 &&
                                        sy + (i + 1) *
                                        context->vertical_scale >
                                        context->output_pv &&
                                        (!capture || sy + i *
                                         context->vertical_scale >=
                                         context->output_pv)) {
                                    context->painted_outside_raster = 1;
                                }
                            }
                        }
                        if (context->pos_x < 0 || context->pos_x >
                                INT_MAX - context->repeat_count) {
                            status = SIXEL_BAD_INPUT;
                            goto end;
                        }
                        if (!(context->sequence || capture) || *p != '?') {
                            draw_repeat = context->repeat_count;
                            if (trust_raster_size &&
                                    context->attributed_ph > 0 &&
                                    draw_repeat > context->attributed_ph -
                                        context->pos_x) {
                                draw_repeat = context->pos_x <
                                    context->attributed_ph
                                    ? context->attributed_ph - context->pos_x
                                    : 0;
                            }
                            if ((draw_repeat > 0 && context->pos_x >
                                    SIXEL_WIDTH_LIMIT - draw_repeat) ||
                                    target_bottom > SIXEL_HEIGHT_LIMIT) {
                                status = SIXEL_BAD_INPUT;
                                goto end;
                            }
                            sx = segment->canvas->width;
                            sy = segment->canvas->height;
                            while (draw_repeat > 0 &&
                                    sx < context->pos_x + draw_repeat) {
                                sx = sx > SIXEL_WIDTH_LIMIT / 2
                                    ? SIXEL_WIDTH_LIMIT : sx * 2;
                            }
                            while (sy < target_bottom) {
                                sy = sy > SIXEL_HEIGHT_LIMIT / 2
                                    ? SIXEL_HEIGHT_LIMIT : sy * 2;
                            }
                            if (sx != segment->canvas->width ||
                                    sy != segment->canvas->height) {
                                status = image_buffer_resize(segment->canvas,
                                    sx, sy, context->bgindex, allocator);
                                if (SIXEL_FAILED(status)) {
                                    goto end;
                                }
                            }
                        }
                        sy = segment->out_y + context->pos_y *
                            context->vertical_scale;
                        if (trust_raster_size && context->output_pv > 0 &&
                                sy >= context->output_pv) {
                            /* No row is visible. Retain the reference's x
                             * overflow, width growth and skip semantics. */
                            y = context->pos_y;
                            n = context->attributed_pv;
                            context->pos_y = sy;
                            context->attributed_pv = context->output_pv;
                            status = capture
                                ? sixel_aspect_paint_capture(segment->canvas,
                                    context, *p - '?', 1, allocator)
                                : sixel_aspect_paint(segment->canvas,
                                    context, *p - '?', 1, allocator);
                            context->pos_y = y;
                            context->attributed_pv = n;
                            if (segment->canvas->ncolors > image->ncolors) {
                                image->ncolors = segment->canvas->ncolors;
                            }
                            if (SIXEL_FAILED(status)) {
                                goto end;
                            }
                            ++p;
                            break;
                        }
                    }
                    if (context->changing_aspect) {
                        status = capture
                            ? sixel_aspect_paint_capture(image, context,
                                *p - '?', trust_raster_size, allocator)
                            : sixel_aspect_paint(image, context, *p - '?',
                                trust_raster_size, allocator);
                        if (SIXEL_FAILED(status)) {
                            goto end;
                        }
                        ++p;
                        break;
                    }
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
                        if ((!trust_raster_size ||
                                context->attributed_pv <= 0 ||
                                context->pos_y <=
                                    context->attributed_pv - 6) &&
                                context->pos_x >= 0 &&
                                context->pos_x < image->width &&
                                context->pos_y >= 0 &&
                                context->pos_y <= image->height - 6) {
                            /* Runs, including a leading repeat, stop before
                             * the trusted width; clipping stays scalar. */
                            run_width = image->width;
                            if (trust_raster_size &&
                                    context->attributed_ph > 0 &&
                                    run_width > context->attributed_ph) {
                                run_width = context->attributed_ph;
                            }
                            if (context->pos_x < run_width &&
                                    context->repeat_count <=
                                        run_width - context->pos_x) {
                                p = parser_context_store_ormode_run(
                                    context, image, p, p0 + len, run_width);
                                continue;
                            }
                        }
                    } else {
                        /* Sequence packets keep an existing canvas; their
                         * untouched samples must never be reinitialized. */
                        if ((!context->sequence || segment != NULL) &&
                                !context->normal_disabled &&
                                image->normal_columns == NULL) {
                            if (capture && context->capture_columns != NULL &&
                                    *context->capture_columns != NULL &&
                                    context->capture_columns_height ==
                                        image->height) {
                                image->normal_columns =
                                    *context->capture_columns;
                                *context->capture_columns = NULL;
                            } else {
                                image->normal_columns =
                                    image_buffer_allocate_planes(image->width,
                                        image->height, allocator);
                            }
                            if (image->normal_columns == NULL) {
                                status = SIXEL_BAD_ALLOCATION;
                                goto end;
                            }
                        }
                        if (image->normal_columns != NULL &&
                                (segment == NULL || !trust_raster_size ||
                                 context->output_pv <= 0 ||
                                 segment->out_y + (context->pos_y + 6) *
                                     context->vertical_scale <=
                                     context->output_pv) &&
                                (!trust_raster_size ||
                                 context->attributed_pv <= 0 ||
                                 context->pos_y <=
                                     context->attributed_pv - 6) &&
                                context->pos_x >= 0 &&
                                context->pos_x < image->width &&
                                context->pos_y >= 0 &&
                                context->pos_y <= image->height - 6) {
                            /* Runs, including a leading repeat, stop before
                             * the trusted width; clipping stays scalar. */
                            run_width = image->width;
                            if (trust_raster_size &&
                                    context->attributed_ph > 0 &&
                                    run_width > context->attributed_ph) {
                                run_width = context->attributed_ph;
                            }
                            if (context->pos_x < run_width &&
                                    context->repeat_count <=
                                        run_width - context->pos_x) {
                                p = parser_context_store_normal_run(
                                    context, image, p, p0 + len, run_width);
                                continue;
                            }
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
                    if ((context->sequence || capture) && bits == 0) {
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
                        sx = segment != NULL && sx > SIXEL_WIDTH_LIMIT / 2
                            ? SIXEL_WIDTH_LIMIT : sx * 2;
                    }

                    sy = image->height;
                    while (sy < target_bottom) {
                        sy = segment != NULL && sy > SIXEL_HEIGHT_LIMIT / 2
                            ? SIXEL_HEIGHT_LIMIT : sy * 2;
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
                /* Detect only completed commands: ESC and EOF leave a
                 * non-sequence DECGRA unapplied. Blank sixels and cursor or
                 * repeat commands count even when they paint no pixels.
                 * Parallel requests join and discard private worker state
                 * before returning FALSE; the serial cursor then replays
                 * every byte, so this test cannot miss earlier drawing. */
                context->attributed_pan = context->params[0];
                context->attributed_pad = context->nparams > 1
                    ? context->params[1] : 0;
                scale = sixel_vertical_scale(context->attributed_pan,
                                               context->attributed_pad);
                if (segment == NULL && !context->changing_aspect &&
                        !context->sequence &&
                        (context->painted || context->drawing_control) &&
                        scale != context->vertical_scale) {
                    /* Leave the completed command ready to be applied once
                     * by a resumed segment parser, or replayed from scratch. */
                    context->aspect_resume = p;
                    --context->nparams;
                    return DECODE_RESTART_ASPECT;
                }
                if (segment != NULL &&
                        (scale != context->vertical_scale ||
                         (context->nparams > 2 && context->params[2] > 0 &&
                          context->params[2] != context->attributed_ph) ||
                         (context->nparams > 3 && context->params[3] > 0 &&
                          context->params[3] != context->output_pv))) {
                    status = sixel_segment_compose(image, context, segment,
                        trust_raster_size, allocator);
                    if (SIXEL_FAILED(status)) {
                        goto end;
                    }
                    if (scale != context->vertical_scale) {
                        ++segment->changes;
                    }
                    /* Keep small exact fixtures on the segmented route.
                     * Beyond four changes require one band per change. */
                    if (segment->changes > 4 &&
                            segment->changes > segment->bands + 1 &&
                            aspect_reference != 2) {
                        context->pos_y = segment->out_y;
                        context->max_x = segment->max_x;
                        context->max_y = segment->max_y;
                        image = segment->canvas;
                        segment = NULL;
                        context->changing_aspect = 1;
                    }
                }
                rescale_head = segment == NULL && !context->changing_aspect &&
                    !context->sequence && scale != context->vertical_scale;
                context->vertical_scale = scale;
                if (capture) {
                    image->capture = scale;
                }
                if (context->vertical_scale > SIXEL_HEIGHT_LIMIT) {
                    status = SIXEL_BAD_INPUT;
                    sixel_helper_set_additional_message(
                        "sixel decode: vertical aspect is too huge.");
                    goto end;
                }
                if (context->nparams > 2 && context->params[2] > 0) {
                    context->attributed_ph = context->params[2];
                }
                if (context->nparams > 3 && context->params[3] > 0) {
                    context->output_pv = context->params[3];
                    if (context->output_pv > SIXEL_HEIGHT_LIMIT) {
                        status = SIXEL_BAD_INPUT;
                        sixel_helper_set_additional_message(
                            "sixel decode: raster height is too huge.");
                        goto end;
                    }
                }
                if (context->output_pv > 0) {
                    /* A new head scale can retain an earlier declared Pv. */
                    context->attributed_pv = context->changing_aspect
                        ? context->output_pv
                        : 1 + (context->output_pv - 1) /
                            context->vertical_scale;
                }

                if (segment != NULL) {
                    status = sixel_segment_raster(image, context, segment,
                        allocator);
                    if (SIXEL_FAILED(status)) {
                        goto end;
                    }
                    goto raster_done;
                }

                /* Before drawing, a new scale can shrink the source
                 * raster. Workers use allocated dimensions as their
                 * bounds, so discard the obsolete head geometry too. */
                sx = context->attributed_ph;
                if (image->width > sx && (!rescale_head || sx == 0)) {
                    sx = image->width;
                }
                sy = context->attributed_pv;
                if (image->height > sy && (!rescale_head || sy == 0)) {
                    sy = image->height;
                }
                if (image->width != sx || image->height != sy) {
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
                if (segment == NULL && !context->sequence &&
                        !context->changing_aspect && !raster_ready &&
                        (!capture || !context->drawing_control) &&
                        context->attributed_ph > 0 &&
                        context->attributed_pv > 0) {
                    raster_ready = 1;
                }
                parallel_anchor = p;
#endif  /* SIXEL_ENABLE_THREADS */
raster_done:
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
                /* Consume the whole number here instead of one digit per
                 * pass through the state switch; each digit is still
                 * checked for overflow exactly as before. */
                do {
                    status = safe_addition_for_params(context, p);
                    if (SIXEL_FAILED(status)) {
                        goto end;
                    }
                    p++;
                } while (p < p0 + len && *p >= '0' && *p <= '9');
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
                /* Consume the whole number here instead of one digit per
                 * pass through the state switch; each digit is still
                 * checked for overflow exactly as before. */
                do {
                    status = safe_addition_for_params(context, p);
                    if (SIXEL_FAILED(status)) {
                        goto end;
                    }
                    p++;
                } while (p < p0 + len && *p >= '0' && *p <= '9');
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

                if (context->ormode && !context->changing_aspect) {
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
                        if (capture) {
                            return DECODE_RESTART_EXACT;
                        }
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
    if (segment != NULL) {
        status = sixel_segment_compose(image, context, segment,
            trust_raster_size, allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        context->pos_y = segment->out_y;
        context->max_x = segment->max_x;
        context->max_y = segment->max_y;
        context->attributed_pv = context->output_pv;
        context->changing_aspect = 1;
        image = segment->canvas;
    }
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

/* Keep these entry points separate: inlining them into the dispatch can
 * merge common blocks and reintroduce capture branches in ordinary loops. */
#if defined(_MSC_VER)
__declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static int
sixel_decode_raw_ordinary(
    unsigned char *p, int len, image_buffer_t *image,
    parser_context_t *context, sixel_allocator_t *allocator,
    sixel_timeline_logger_t *logger, int logger_prepared,
    unsigned int decode_flags, sixel_decoder_undither_context_t *undither)
{
    return sixel_decode_raw_body(p, len, image, context, allocator,
        logger, logger_prepared, decode_flags, undither, 0, NULL);
}

#if defined(_MSC_VER)
__declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static int
sixel_decode_raw_capture(
    unsigned char *p, int len, image_buffer_t *image,
    parser_context_t *context, sixel_allocator_t *allocator,
    sixel_timeline_logger_t *logger, int logger_prepared,
    unsigned int decode_flags, sixel_decoder_undither_context_t *undither)
{
    return sixel_decode_raw_body(p, len, image, context, allocator,
        logger, logger_prepared, decode_flags, undither, 1, NULL);
}

/* This third parser instantiation leaves ordinary/capture machine code
 * free of segment branches. Workers are deliberately disabled inside it. */
#if defined(_MSC_VER)
__declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static int
sixel_decode_segmented(unsigned char *p, int len, image_buffer_t *canvas,
    parser_context_t *context, sixel_allocator_t *allocator,
    unsigned int decode_flags)
{
    image_buffer_t *patch;
    aspect_segment_t segment;
    SIXELSTATUS status;

    patch = sixel_allocator_calloc(allocator, 1, sizeof(*patch));
    if (patch == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    /* Holes are masked out. Initialize wide indexes too, so blending a
     * partial column never reads indeterminate background samples. */
    status = image_buffer_init(patch, canvas->width, canvas->height,
        0, canvas->depth, 1, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    memcpy(patch->palette, canvas->palette, sizeof(patch->palette));
    patch->ncolors = canvas->ncolors;
    patch->capture_defined = context->capture_defined;
    memset(&segment, 0, sizeof(segment));
    segment.canvas = canvas;
    context->changing_aspect = 0;
    context->max_x = -1;
    context->max_y = -1;
    /* Reusable packet columns belong to a different height and canvas. */
    context->capture_columns = NULL;
    status = sixel_decode_raw_body(p, len, patch, context, allocator,
        NULL, 0, decode_flags, NULL, context->capture, &segment);
    context->changing_aspect = 1;
end:
    sixel_allocator_free(allocator, patch->pixels.p);
    image_buffer_release_paint_mask(patch, allocator);
    image_buffer_release_columns(patch, allocator);
    image_buffer_release_ormode_planes(patch, allocator);
    sixel_allocator_free(allocator, patch);
    return status;
}

/* Reuse the lazy decoder's first segment when all its paint is still in
 * band words at 1:1. A multiplied prefix may contain partial device rows
 * clipped under earlier raster sizes; that history cannot be reconstructed
 * from its source-row mask. Such prefixes, scalar paint and unbounded
 * rasters replay instead. No work is added to ordinary images. */
#if defined(_MSC_VER)
__declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static int
sixel_decode_resume_segments(unsigned char *end, image_buffer_t *patch,
    parser_context_t *context, sixel_allocator_t *allocator,
    unsigned int decode_flags)
{
    image_buffer_t *canvas;
    aspect_segment_t segment;
    SIXELSTATUS status;
    int mask;

    if (aspect_reference == 1 || context->ormode ||
            context->vertical_scale != 1 || context->normal_disabled || patch->normal_columns == NULL ||
            context->attributed_ph <= 0 || context->output_pv <= 0 ||
            patch->width != context->attributed_ph ||
            patch->height != context->attributed_pv) {
        return DECODE_RESTART_ASPECT;
    }
    canvas = sixel_allocator_calloc(allocator, 1, sizeof(*canvas));
    if (canvas == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    mask = patch->paint_mask != NULL;
    status = image_buffer_init(canvas, patch->width, context->output_pv,
        context->bgindex, patch->depth, mask, allocator);
    if (SIXEL_FAILED(status)) {
        sixel_allocator_free(allocator, canvas);
        return status;
    }
    status = image_buffer_enable_paint_mask(patch, allocator);
    if (SIXEL_SUCCEEDED(status)) {
        memset(&segment, 0, sizeof(segment));
        segment.canvas = canvas;
        segment.bands = context->pos_y / 6;
        /* Capture's columns are private to the prefix's source height. */
        context->capture_columns = NULL;
        status = sixel_decode_raw_body(context->aspect_resume,
            (int)(end - context->aspect_resume), patch, context, allocator,
            NULL, 0, decode_flags, NULL, context->capture, &segment);
    }
    context->changing_aspect = 1;
    sixel_allocator_free(allocator, patch->pixels.p);
    image_buffer_release_paint_mask(patch, allocator);
    image_buffer_release_columns(patch, allocator);
    *patch = *canvas;
    sixel_allocator_free(allocator, canvas);
    return status;
}

/* Dispatch once per packet, before either specialized parser loop. */
static int
sixel_decode_raw_impl(
    unsigned char *p, int len, image_buffer_t *image,
    parser_context_t *context, sixel_allocator_t *allocator,
    sixel_timeline_logger_t *logger, int logger_prepared,
    unsigned int decode_flags, sixel_decoder_undither_context_t *undither)
{
    if (context->changing_aspect && aspect_reference != 1 &&
            !context->ormode) {
        return sixel_decode_segmented(p, len, image, context, allocator,
            decode_flags);
    }
    if (context->capture) {
        return sixel_decode_raw_capture(p, len, image, context, allocator,
            logger, logger_prepared, decode_flags, undither);
    }
    return sixel_decode_raw_ordinary(p, len, image, context, allocator,
        logger, logger_prepared, decode_flags, undither);
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
    int changing_aspect;

    image->pixels.p = NULL;
    image->paint_mask = NULL;
    image->ormode_planes = NULL;
    image->ormode_high_planes = NULL;
    image->normal_columns = NULL;

    logger = NULL;
    logger_prepared = 0;
    i = 0U;
    nparams = body_nparams;
    changing_aspect = 0;
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

retry:
    status = parser_context_init(context);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    context->palette_transform = transform;
    context->changing_aspect = changing_aspect;

    /*
     * The serial parser always runs first. When palette and raster
     * attributes become available, the parser may request a parallel worker
     * via sixel_decoder_parallel_request_start(). This guarantees bounds
     * checks and logging are consistent before any background decode starts.
     */

    if (logger_prepared && !changing_aspect) {
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
    if (status == DECODE_RESTART_ASPECT && undither == NULL) {
        status = sixel_decode_resume_segments(p + len, image, context,
            allocator, decode_flags);
    }
    if (status == DECODE_RESTART_ASPECT) {
        /* No workers survive request_start(). Discard all tentative stores
         * without materializing columns or OR planes, then rebuild palette,
         * parser flags, raster bounds and body parameters from their inputs.
         * Device-row composition cannot request another restart or fusion. */
        sixel_allocator_free(allocator, image->pixels.p);
        image->pixels.p = NULL;
        image_buffer_release_paint_mask(image, allocator);
        image_buffer_release_ormode_planes(image, allocator);
        image_buffer_release_columns(image, allocator);
        if (undither != NULL) {
            sixel_allocator_free(undither->allocator, undither->pixels);
            undither->pixels = NULL;
            free(undither->palette);
            undither->palette = NULL;
            undither->ncolors = 0;
            undither->palette_size = 0;
            undither->pixel_size = 0;
        }
        if (logger_prepared) {
            /* Keep one timeline session and one final result. Worker events
             * describe real discarded work; do not log a false completion. */
            sixel_timeline_logger_logf(logger, "decoder", "parser", "restart",
                0, 0, 0, len, 0, len,
                "discarding constant-aspect attempt; restarting device rows");
        }
        changing_aspect = 1;
        goto retry;
    }
    if (context->aspect_ormode != NULL) {
        status = sixel_aspect_finish(image, context, status, allocator);
    }
    if (SIXEL_SUCCEEDED(status)) {
        if (context->ormode) {
            image_buffer_finalize_ormode(image);
        } else {
            image_buffer_materialize_columns(image, allocator);
        }
        /* Constant-aspect fast4 expands after reconstruction, preserving
         * source-row neighbors. Changing aspect already uses device rows
         * and cannot start fused workers; expansion is a no-op there. */
        if (undither == NULL) {
            status = image_buffer_expand_vertical(image, context, allocator,
                                                   NULL);
        }
    }
    if (SIXEL_FAILED(status)) {
        sixel_allocator_free(allocator, image->pixels.p);
        image->pixels.p = NULL;
        image_buffer_release_paint_mask(image, allocator);
        image_buffer_release_ormode_planes(image, allocator);
        image_buffer_release_columns(image, allocator);
        goto end;
    }

    status = SIXEL_OK;

end:
    /* Header errors may occur after creating the serial OR accumulator. */
    if (context->aspect_ormode != NULL) {
        status = sixel_aspect_finish(image, context, status, allocator);
    }
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
        status = image_buffer_expand_vertical(image, &context, allocator,
                                               &undither);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        *pheight = image->height;
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

    if (context.changing_aspect) {
        /* Use the ordinary row filter over the complete device image.
         * It cannot start reconstruction workers, even for large rasters. */
        if ((size_t)image->width > SIZE_MAX / (size_t)image->height / 3U) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        rgb_pixels = sixel_allocator_malloc(allocator,
            (size_t)image->width * image->height * 3U);
        if (rgb_pixels == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        status = sixel_dequantize_k_undither_fast4_rows(
            image->pixels.in_bytes, image->width, image->height,
            palette, image->ncolors, similarity_bias, 0, image->height,
            allocator, rgb_pixels);
    } else {
        status = sixel_dequantize_k_undither_fast4(
            image->pixels.in_bytes,
            image->width,
            image->height,
            palette,
            image->ncolors,
            similarity_bias,
            allocator,
            &rgb_pixels);
    }
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    undither.pixels = rgb_pixels;
    undither.pixel_size = 3;
    rgb_pixels = NULL;
    status = image_buffer_expand_vertical(image, &context, allocator,
                                           &undither);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    rgb_pixels = undither.pixels;
    undither.pixels = NULL;
    *pheight = image->height;

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
 * The capture adapter checks framing/geometry ahead of ordinary packet
 * decoding. The reference validator and exact painter below remain available
 * for exceptional syntax, paint-time palette capture and differential tests.
 * Registers and the RGBA canvas belong to the capture; cursors are per packet.
 * Single-image interfaces retain their permissive parsing contract.
 */
typedef struct sixel_sequence_state {
    int width;
    int height;
    int vertical_scale;
    int changing_aspect;
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
    int scale;
    int drawing;
    unsigned char command;

    p = *cursor;
    raster = 0;
    state->vertical_scale = 1;
    state->changing_aspect = 0;
    drawing = 0;
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
            if (count != 4 || params[2] <= 0 ||
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
            scale = sixel_vertical_scale(params[0], params[1]);
            if (scale > SIXEL_HEIGHT_LIMIT) {
                return SIXEL_BAD_INPUT;
            }
            if ((raster && scale != state->vertical_scale) ||
                    (!raster && drawing && scale != 1)) {
                state->changing_aspect = 1;
            }
            state->vertical_scale = scale;
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
        drawing = 1;
        if (command == '$') {
            x = 0;
            continue;
        }
        if (command == '-') {
            if (y > INT_MAX - 6 * state->vertical_scale) {
                return SIXEL_BAD_INPUT;
            }
            x = 0;
            y += 6 * state->vertical_scale;
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
                        (y >= state->height || row * state->vertical_scale >=
                            state->height - y)) {
                    return SIXEL_BAD_INPUT;
                }
            }
            state->painted = 1;
        }
        x += repeat;
    }
    return SIXEL_BAD_INPUT;
}

/* Only envelope and raster commands are inspected ahead of decoding. The
 * two delimiter searches stop at the first ST or unsupported ESC; neither
 * scans individual sixel bits or walks literal runs in C. Every DECGRA must
 * agree, including declarations after paint. Syntax around it is checked by
 * the ordinary parser (or the reference validator on an exceptional retry).
 *
 * Reference-validator check ownership:
 * - DCS introducer/header arity, integer bounds, P2, required positive raster,
 *   6000-axis limit, identical geometry and scale limit: this function.
 * - ST/completeness/ESC nesting: delimiter search here; C1 nesting, CAN/SUB,
 *   unsupported body controls: strict ordinary/worker parsers.
 * - # arity/range/model, ! arity/character/0xffff limit, parameter overflow:
 *   strict command parsing in raw_impl; workers accept only numeric selects
 *   and repeats and otherwise replay serially. Empty parameters retain the
 *   reference parser's zero semantics.
 * - Paint before raster and undefined registers, even overwritten paint:
 *   serial run entry / conservative worker color-selection checks, followed
 *   by reference validation.
 *   Compose also checks undefined final indexes before touching the canvas.
 * - x/y overflow and painted extents: ordinary trusted-raster stores/workers;
 *   unusual results replay the reference validator to preserve status order.
 * - Blank skips, '$', '-', changing Pan/Pad: ordinary cursor/aspect painter;
 *   scale changes restart privately, retaining the same capture contract.
 * - Definition/paint ordering and cumulative PALETTE_REDEFINED: parser
 *   definition observations plus compose coverage; late definitions replay
 *   the reference validator and exact RGBA painter for paint-time capture.
 * Inter-packet whitespace/save/restore rules remain in sequence_next.
 */
static SIXELSTATUS
sixel_sequence_envelope(unsigned char const **cursor,
    unsigned char const *end, sixel_sequence_state_t *state)
{
    unsigned char const *p;
    unsigned char const *esc;
    unsigned char const *st;
    unsigned char const *limit;
    int params[4];
    int count;
    int raster;
    int scale;

    p = *cursor;
    if (p < end && *p == 0x90) {
        ++p;
    } else if (end - p >= 2 && p[0] == 0x1b && p[1] == 'P') {
        p += 2;
    } else {
        return SIXEL_BAD_INPUT;
    }
    count = sixel_sequence_parameters(&p, end, params, 3);
    if (count < 0 || p == end || *p++ != 'q' ||
            (!state->animation && (count < 2 || params[1] != 1)) ||
            (count >= 2 && params[1] != 0 && params[1] != 1)) {
        return SIXEL_BAD_INPUT;
    }
    state->clear = count < 2 || params[1] == 0;
    esc = memchr(p, 0x1b, (size_t)(end - p));
    st = memchr(p, 0x9c, (size_t)((esc != NULL ? esc : end) - p));
    limit = st;
    if (esc != NULL && (limit == NULL || esc < limit)) {
        if (end - esc < 2 || esc[1] != '\\') {
            return SIXEL_BAD_INPUT;
        }
        limit = esc;
    }
    if (limit == NULL) {
        return SIXEL_BAD_INPUT;
    }
    raster = 0;
    state->changing_aspect = 0;
    while ((p = memchr(p, '"', (size_t)(limit - p))) != NULL) {
        ++p;
        count = sixel_sequence_parameters(&p, limit, params, 4);
        if (count != 4 || params[2] <= 0 || params[3] <= 0 ||
                params[2] > SIXEL_DECODE_PIXELS_MAX_SIZE ||
                params[3] > SIXEL_DECODE_PIXELS_MAX_SIZE ||
                (state->width != 0 && (state->width != params[2] ||
                                      state->height != params[3]))) {
            return SIXEL_BAD_INPUT;
        }
        state->width = params[2];
        state->height = params[3];
        scale = sixel_vertical_scale(params[0], params[1]);
        if (scale > SIXEL_HEIGHT_LIMIT) {
            return SIXEL_BAD_INPUT;
        }
        if (!raster) {
            state->vertical_scale = scale;
        }
        raster = 1;
    }
    if (!raster) {
        return SIXEL_BAD_INPUT;
    }
    *cursor = limit + (*limit == 0x9c ? 1 : 2);
    return SIXEL_OK;
}

/* A scaled packet paints a temporary image, then scatters opaque samples
 * onto the output-sized canvas. Each packet owns its aspect, so changing it
 * cannot rescale earlier paint or discard rows from a previous packet. */
static SIXELSTATUS
sixel_sequence_decode_packet(unsigned char const *packet, int len,
    image_buffer_t *canvas, parser_context_t *context,
    sixel_sequence_state_t const *state, sixel_allocator_t *allocator)
{
    image_buffer_t *image;
    SIXELSTATUS status;
    size_t pos;
    size_t size;

    context->changing_aspect = state->changing_aspect;
    if (state->changing_aspect || state->vertical_scale == 1) {
        return sixel_decode_raw_impl((unsigned char *)(void *)packet,
            len, canvas, context, allocator, NULL, 0,
            SIXEL_DECODE_PIXELS_OPTION_TRUST_RASTER_SIZE, NULL);
    }
    image = sixel_allocator_calloc(allocator, 1U, sizeof(*image));
    if (image == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    status = image_buffer_init(image, state->width,
        1 + (state->height - 1) / state->vertical_scale,
        -1, 4, 0, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    memcpy(image->palette, canvas->palette, sizeof(image->palette));
    image->ncolors = canvas->ncolors;
    status = sixel_decode_raw_impl((unsigned char *)(void *)packet,
        len, image, context, allocator, NULL, 0,
        SIXEL_DECODE_PIXELS_OPTION_TRUST_RASTER_SIZE, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = image_buffer_expand_vertical(image, context, allocator, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    size = (size_t)state->width * state->height * 4U;
    for (pos = 0; pos < size; pos += 4U) {
        if (image->pixels.in_bytes[pos + 3U] != 0) {
            memcpy(canvas->pixels.in_bytes + pos,
                   image->pixels.in_bytes + pos, 4U);
        }
    }
    memcpy(canvas->palette, image->palette, sizeof(canvas->palette));
    canvas->ncolors = image->ncolors;
end:
    sixel_allocator_free(allocator, image->pixels.p);
    image_buffer_release_paint_mask(image, allocator);
    image_buffer_release_ormode_planes(image, allocator);
    image_buffer_release_columns(image, allocator);
    sixel_allocator_free(allocator, image);
    return status;
}

struct sixel_sequence_decoder {
    sixel_allocator_t *allocator;
    image_buffer_t image;
    image_buffer_t patch;
    image_buffer_t exact_patch;
    uint64_t *columns;
    int columns_height;
    sixel_sequence_state_t state;
    sixel_sequence_state_t checkpoint;
    int exact;
    unsigned char const *data;
    unsigned char const *cursor;
    unsigned char const *end;
    sixel_palette_transform_t const *transform;
    size_t passes;
    int anchored;
    int failed;
};

/* Reset only packet storage. Palette and definitions are restored from the
 * last committed canvas/checkpoint on retries; borrowed canvas pixels never
 * expose speculative decoding, clearing, or a partially composed packet. */
static SIXELSTATUS
sixel_sequence_prepare_patch(sixel_sequence_decoder_t *d,
    image_buffer_t *patch, int depth, int height)
{
    SIXELSTATUS status;

    if (patch->pixels.p == NULL) {
        status = image_buffer_init(patch, d->state.width, height,
            0, depth, depth != 4, d->allocator);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    } else if (patch->height != height) {
        status = image_buffer_resize(patch, d->state.width, height,
            0, d->allocator);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }
    /* Indexed holes are ignored through the mask; do not clear their data. */
    if (depth == 4) {
        memset(patch->pixels.p, 0,
            (size_t)patch->width * patch->height * 4U);
    } else {
        memset(patch->paint_mask, 0, (size_t)patch->width * patch->height);
    }
    image_buffer_release_columns(patch, d->allocator);
    memcpy(patch->palette, d->image.palette, sizeof(patch->palette));
    patch->ncolors = d->image.ncolors;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_sequence_fast_packet(sixel_sequence_decoder_t *d,
    unsigned char const *packet, unsigned char const *end)
{
    parser_context_t context;
    image_buffer_t *patch;
    unsigned char const *p;
    unsigned char const *mask;
    unsigned char const *first;
    unsigned char *dest;
    unsigned short const *indices;
    SIXELSTATUS status;
    int changing;
    int scale;
    int y;
    int x;
    int color;
    int painted;
    size_t row;
    size_t pos;
    size_t size;

    changing = 0;
    patch = &d->patch;
retry:
    scale = changing ? 1 : d->state.vertical_scale;
    status = sixel_sequence_prepare_patch(d, patch, 2,
        1 + (d->state.height - 1) / scale);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    memcpy(d->state.defined, d->checkpoint.defined,
        sizeof(d->state.defined));
    parser_context_init(&context);
    if (d->columns_height != patch->height) {
        sixel_allocator_free(d->allocator, d->columns);
        d->columns = NULL;
        d->columns_height = patch->height;
    }
    if (d->columns != NULL) {
        memset(d->columns, 0, (size_t)patch->width *
            ((patch->height + 5) / 6) * sizeof(*d->columns));
    }
    context.capture = 1;
    context.capture_columns = &d->columns;
    context.capture_columns_height = patch->height;
    context.capture_defined = d->state.animation ? NULL : d->state.defined;
    context.color_index = d->state.animation ? 15 : 0;
    context.normal_color = sixel_column_color(context.color_index);
    context.palette_transform = d->transform;
    context.changing_aspect = changing;
    context.output_pv = d->state.height;
    patch->capture = 1;
    patch->capture_defined = context.capture_defined;
    status = sixel_decode_raw_impl((unsigned char *)(void *)packet,
        (int)(end - packet), patch, &context, d->allocator, NULL, 0,
        SIXEL_DECODE_PIXELS_OPTION_TRUST_RASTER_SIZE, NULL);
    if (status == DECODE_RESTART_ASPECT) {
        changing = 1;
        goto retry;
    }
    if (status != SIXEL_OK || context.painted_outside_raster ||
            context.palette_redefined_after_paint) {
        goto exact;
    }
    image_buffer_finalize_normal_bands(patch, 0, (patch->height + 5) / 6);
    if (patch->normal_columns != NULL) {
        sixel_allocator_free(d->allocator, d->columns);
        d->columns = patch->normal_columns;
        patch->normal_columns = NULL;
        d->columns_height = patch->height;
    }
    scale = changing ? 1 : context.vertical_scale;
    /* Verify final register references before any canvas stores. Run/worker
     * checks additionally catch undefined paint later overwritten by a valid
     * index. Fully defined palettes avoid this exceptional extra traversal. */
    if (!d->state.animation && patch->ncolors > 0 &&
            memchr(d->state.defined, 0, (size_t)patch->ncolors) != NULL) {
        size = (size_t)patch->width * patch->height;
        for (pos = 0; pos < size; ++pos) {
            if (patch->paint_mask[pos] &&
                    !d->state.defined[patch->pixels.in_shorts[pos]]) {
                goto exact;
            }
        }
    }
    if (d->state.painted && context.capture_definitions) {
        d->state.flags |= SIXEL_DECODE_PIXELS_RESULT_PALETTE_REDEFINED;
    }
    painted = 0;
    /* Expand constant aspect as part of composition, without another image
     * allocation/copy. memchr skips empty rows and leading unpainted spans. */
    for (y = 0; y < d->state.height; ++y) {
        row = (size_t)(y / scale) * patch->width;
        mask = patch->paint_mask + row;
        first = memchr(mask, 0xff, (size_t)patch->width);
        dest = d->image.pixels.in_bytes +
            (size_t)y * d->state.width * 4U;
        if (d->state.clear) {
            memset(dest, 0, (size_t)d->state.width * 4U);
        }
        if (first == NULL) {
            continue;
        }
        painted = 1;
        indices = patch->pixels.in_shorts + row;
        for (x = (int)(first - mask); x < patch->width; ++x) {
            if (mask[x]) {
                color = patch->palette[indices[x]];
                dest[4 * x] = (unsigned char)(color >> 16);
                dest[4 * x + 1] = (unsigned char)(color >> 8);
                dest[4 * x + 2] = (unsigned char)color;
                dest[4 * x + 3] = 255;
            }
        }
    }
    d->state.painted |= painted;
    goto commit_palette;

exact:
    /* The reference validator runs first even after a parser error: its
     * BAD_INPUT has precedence over ordinary numeric/decode failures. */
    d->state = d->checkpoint;
    p = packet;
    status = sixel_sequence_packet(&p, end, &d->state);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    patch = &d->exact_patch;
    status = sixel_sequence_prepare_patch(d, patch, 4, d->state.height);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    parser_context_init(&context);
    context.sequence = 1;
    context.color_index = d->state.animation ? 15 : 0;
    context.palette_transform = d->transform;
    status = sixel_sequence_decode_packet(packet, (int)(end - packet),
        patch, &context, &d->state, d->allocator);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    size = (size_t)d->state.width * d->state.height * 4U;
    if (d->state.clear) {
        memset(d->image.pixels.p, 0, size);
    }
    for (pos = 0; pos < size; pos += 4U) {
        if (patch->pixels.in_bytes[pos + 3U]) {
            memcpy(d->image.pixels.in_bytes + pos,
                patch->pixels.in_bytes + pos, 4U);
        }
    }
commit_palette:
    memcpy(d->image.palette, patch->palette, sizeof(patch->palette));
    d->image.ncolors = patch->ncolors;
    return SIXEL_OK;
}

/* Per-instance reference switch for the opt-in differential runner. It is
 * internal, does not consult the environment, and changes no public API. */
SIXEL_INTERNAL_API void
sixel_sequence_set_exact(sixel_sequence_decoder_t *d)
{
    d->exact = 1;
}

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
        sixel_allocator_free(allocator, d->columns);
        sixel_allocator_free(allocator, d->patch.pixels.p);
        image_buffer_release_paint_mask(&d->patch, allocator);
        image_buffer_release_columns(&d->patch, allocator);
        sixel_allocator_free(allocator, d->exact_patch.pixels.p);
        image_buffer_release_paint_mask(&d->exact_patch, allocator);
        image_buffer_release_columns(&d->exact_patch, allocator);
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
    d->checkpoint = d->state;
    status = d->exact
        ? sixel_sequence_packet(&p, d->end, &d->state)
        : sixel_sequence_envelope(&p, d->end, &d->state);
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
    /* Success must publish storage before either clearing or decoding. */
    if (d->image.pixels.p == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto failure;
    }
    if (!d->exact) {
        status = sixel_sequence_fast_packet(d, packet, p);
        if (SIXEL_FAILED(status)) {
            goto failure;
        }
        goto publish;
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
    status = sixel_sequence_decode_packet(packet, (int)(p - packet),
        &d->image, &context, &d->state, d->allocator);
    if (SIXEL_FAILED(status)) {
        goto failure;
    }
publish:
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
sixel_decode_sequence_mapped_exact(unsigned char const *data, size_t size,
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
        status = sixel_sequence_decode_packet(packet, (int)(p - packet),
            image, &context, state, allocator);
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
sixel_decode_sequence_mapped(unsigned char const *data, size_t size,
    sixel_palette_transform_t const *transform, unsigned char **pixels,
    int *width, int *height, unsigned int *flags, size_t *passes,
    sixel_allocator_t *allocator)
{
    sixel_sequence_decoder_t *d;
    unsigned char const *rgba;
    size_t byte_end;
    int w;
    int h;
    SIXELSTATUS status;

    *pixels = NULL;
    *width = 0;
    *height = 0;
    *flags = 0;
    *passes = 0;
    status = sixel_sequence_open(&d, data, size, 0, allocator);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    d->transform = transform;
    do {
        status = sixel_sequence_next(d, &rgba, &w, &h, &byte_end);
    } while (status == SIXEL_OK);
    if (status == SIXEL_FALSE && d->passes != 0) {
        *pixels = d->image.pixels.in_bytes;
        d->image.pixels.p = NULL;
        *width = d->state.width;
        *height = d->state.height;
        *flags = d->state.flags;
        *passes = d->passes;
        status = SIXEL_OK;
    } else {
        sixel_helper_set_additional_message(
            "multi-pass input requires complete fixed-raster P2=1 SIXEL "
            "packets with defined colors and valid anchor framing.");
    }
    sixel_sequence_close(d);
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
