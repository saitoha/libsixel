/*
 * SPDX-License-Identifier: MIT
 *
 * This file is derived from "stb_image.h" that is in public domain.
 * https://github.com/nothings/stb
 *
 * Hayaki Saito <saitoha@me.com> modified this and re-licensed
 * it under the MIT license.
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

/* STDC_HEADERS */
#include <stdio.h>
#include <stdlib.h>

#if HAVE_STRING_H
# include <string.h>
#endif  /* HAVE_STRING_H */
#if HAVE_CTYPE_H
# include <ctype.h>
#endif  /* HAVE_CTYPE_H */
#if HAVE_ASSERT_H
# include <assert.h>
#endif  /* HAVE_ASSERT_H */
#if HAVE_LIMITS_H
# include <limits.h>
#endif  /* HAVE_LIMITS_H */
#if HAVE_STDINT_H
# include <stdint.h>
#endif  /* HAVE_STDINT_H */

#include "fromgif.h"
#include "compat_stub.h"
#include "frame.h"
#include "loader-common.h"

/*
 * gif_context_t struct and start_xxx functions
 *
 * gif_context_t structure is our basic context used by all images, so it
 * contains all the IO context, plus some basic image information
 */
typedef struct
{
   unsigned int img_x, img_y;
   int img_n, img_out_n;

   int buflen;
   unsigned char buffer_start[128];

   unsigned char *img_buffer, *img_buffer_end;
   unsigned char *img_buffer_original;
} gif_context_t;

typedef struct
{
   signed short prefix;
   unsigned char first;
   unsigned char suffix;
} gif_lzw;

#define GIF_LZW_MAX_CODE_SIZE 12
#define GIF_LZW_MAX 0xFFF  /* 0b1111_1111_1111 */
#define GIF_RGB_STRIDE 3
#define GIF_RGBA_STRIDE 4
#define GIF_HASH_EMPTY_KEY 0xffffffffU

static size_t const gif_color_table_bytes[8] = {
    6u, 12u, 24u, 48u, 96u, 192u, 384u, 768u
};

typedef struct
{
   int has_transparency;
   int image_count;
} gif_stream_info_t;

typedef struct
{
   int w, h;
   /*
    * Keep the compositing canvas in packed RGB instead of palette
    * indices.
    * GIF frames can switch local color tables per frame, so carrying only
    * indices across frames corrupts colors when the active palette changes.
    */
   unsigned char *out;       /* composited frame buffer (RGB888) */
   unsigned char *prev_out;  /* canvas backup for disposal method 3 */
   unsigned char *alpha_out; /* alpha mask (optional, 0/255) */
   unsigned char *prev_alpha;/* alpha backup for disposal method 3 */
   unsigned char *history;   /* pixels modified in the previous frame */
   int flags, bgindex, ratio, transparent, eflags;
   unsigned char pal[256][3];
   unsigned char lpal[256][3];
   gif_lzw codes[1 << GIF_LZW_MAX_CODE_SIZE];
   unsigned char *color_table;
   int parse, step;
   int lflags;
   int start_x, start_y;
   int max_x, max_y;
   int cur_x, cur_y;
   int actual_width, actual_height;
   int line_size;
   int loop_count;
   int delay;
   int is_multiframe;
   int is_terminated;
   int preserve_transparency;
   int stream_is_multiframe;
   int global_color_table_entries;
   int color_table_entries;
} gif_t;

#if defined(_MSC_VER)
#pragma warning(push)
/*
 * MSVC warns about constant conditions and potentially uninitialized locals
 * when walking the GIF raster/state machine. The logic is deliberate, so
 * silence those diagnostics locally while keeping the control flow intact.
 */
#pragma warning(disable : 4701 4702)
#endif  /* _MSC_VER */


/* initialize a memory-decode context */
static unsigned char
gif_get8(gif_context_t *s)
{
    unsigned char value;

    value = 0u;
    if (s == NULL || s->img_buffer == NULL || s->img_buffer_end == NULL) {
        return 0u;
    }
    if (s->img_buffer >= s->img_buffer_end) {
        return 0u;
    }
    value = *s->img_buffer;
    s->img_buffer++;
    return value;
}


static int
gif_get16le(gif_context_t *s)
{
    int z = gif_get8(s);
    return z + (gif_get8(s) << 8);
}


static int
gif_read_u8(
    gif_context_t /* in */ *s,
    unsigned char /* out */ *value)
{
    if (s == NULL || value == NULL ||
        s->img_buffer == NULL || s->img_buffer_end == NULL ||
        s->img_buffer >= s->img_buffer_end) {
        return 0;
    }
    *value = *s->img_buffer++;
    return 1;
}


static int
gif_read_u16le(
    gif_context_t /* in */ *s,
    int           /* out */ *value)
{
    unsigned char lo;
    unsigned char hi;

    lo = 0u;
    hi = 0u;
    if (value == NULL) {
        return 0;
    }
    if (!gif_read_u8(s, &lo) || !gif_read_u8(s, &hi)) {
        return 0;
    }
    *value = (int)lo + ((int)hi << 8);
    return 1;
}


static int
gif_skip_bytes(
    gif_context_t /* in */ *s,
    size_t        /* in */ count)
{
    if (s == NULL || s->img_buffer == NULL || s->img_buffer_end == NULL) {
        return 0;
    }
    if ((size_t)(s->img_buffer_end - s->img_buffer) < count) {
        return 0;
    }
    s->img_buffer += count;
    return 1;
}


static SIXELSTATUS
gif_skip_subblocks(
    gif_context_t /* in */ *s,
    int           /* in */ is_extension_block)
{
    unsigned char block_size;

    block_size = 0u;
    for (;;) {
        if (!gif_read_u8(s, &block_size)) {
            sixel_helper_set_additional_message(
                is_extension_block != 0
                ? "corrupt GIF (reason: truncated extension block)."
                : "corrupt GIF (reason: truncated data block).");
            return SIXEL_RUNTIME_ERROR;
        }
        if (block_size == 0u) {
            break;
        }
        if (!gif_skip_bytes(s, (size_t)block_size)) {
            sixel_helper_set_additional_message(
                is_extension_block != 0
                ? "corrupt GIF (reason: truncated extension block)."
                : "corrupt GIF (reason: truncated data block).");
            return SIXEL_RUNTIME_ERROR;
        }
    }
    return SIXEL_OK;
}


static void
gif_parse_colortable(
    gif_context_t /* in */ *s,
    unsigned char       /* in */ pal[256][3],
    int           /* in */ num_entries)
{
    int i;

    for (i = 0; i < num_entries; ++i) {
        pal[i][2] = gif_get8(s);
        pal[i][1] = gif_get8(s);
        pal[i][0] = gif_get8(s);
    }
}


static SIXELSTATUS
gif_parse_colortable_checked(
    gif_context_t /* in */ *s,
    unsigned char       /* in */ pal[256][3],
    int           /* in */ num_entries)
{
    if (s == NULL || s->img_buffer == NULL || s->img_buffer_end == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (num_entries < 0 || num_entries > 256) {
        return SIXEL_BAD_INPUT;
    }
    if ((size_t)(s->img_buffer_end - s->img_buffer) <
        (size_t)num_entries * (size_t)GIF_RGB_STRIDE) {
        sixel_helper_set_additional_message(
            "corrupt GIF (reason: truncated data block).");
        return SIXEL_RUNTIME_ERROR;
    }
    gif_parse_colortable(s, pal, num_entries);
    return SIXEL_OK;
}


static SIXELSTATUS
gif_load_header(
    gif_context_t /* in */ *s,
    gif_t         /* in */ *g)
{
    SIXELSTATUS status = SIXEL_FALSE;
    unsigned char version;
    if (gif_get8(s) != 'G') {
        goto end;
    }
    if (gif_get8(s) != 'I') {
        goto end;
    }
    if (gif_get8(s) != 'F') {
        goto end;
    }
    if (gif_get8(s) != '8') {
        goto end;
    }

    version = gif_get8(s);

    if (version != '7' && version != '9') {
        goto end;
    }
    if (gif_get8(s) != 'a') {
        goto end;
    }

    g->w = gif_get16le(s);
    g->h = gif_get16le(s);
    g->flags = gif_get8(s);
    g->bgindex = gif_get8(s);
    g->ratio = gif_get8(s);
    g->transparent = (-1);
    g->loop_count = (-1);

    if (g->flags & 0x80) {
        g->global_color_table_entries = 2 << (g->flags & 7);
        status = gif_parse_colortable_checked(s,
                                              g->pal,
                                              g->global_color_table_entries);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        g->color_table_entries = g->global_color_table_entries;
    } else {
        g->global_color_table_entries = 0;
        g->color_table_entries = 0;
    }

    status = SIXEL_OK;

end:
    return status;
}


static void
gif_resolve_background_color(
    gif_t const      /* in */ *g,
    unsigned char    /* out */ *bg_r,
    unsigned char    /* out */ *bg_g,
    unsigned char    /* out */ *bg_b)
{
    if (bg_r != NULL) {
        *bg_r = 0u;
    }
    if (bg_g != NULL) {
        *bg_g = 0u;
    }
    if (bg_b != NULL) {
        *bg_b = 0u;
    }
    if (g == NULL || g->global_color_table_entries <= 0 ||
        g->bgindex < 0 || g->bgindex >= g->global_color_table_entries) {
        return;
    }
    if (bg_r != NULL) {
        *bg_r = g->pal[g->bgindex][2];
    }
    if (bg_g != NULL) {
        *bg_g = g->pal[g->bgindex][1];
    }
    if (bg_b != NULL) {
        *bg_b = g->pal[g->bgindex][0];
    }
}


static SIXELSTATUS
gif_scan_stream_info(
    unsigned char const *buffer,
    int size,
    gif_stream_info_t *info)
{
    SIXELSTATUS status = SIXEL_OK;
    unsigned char const *p;
    unsigned char const *end;
    size_t gct_size;
    size_t lct_size;
    unsigned char marker;
    unsigned char packed;
    unsigned char extension_label;
    unsigned char block_size;

    p = NULL;
    end = NULL;
    gct_size = 0u;
    lct_size = 0u;
    marker = 0u;
    packed = 0u;
    extension_label = 0u;
    block_size = 0u;

    if (buffer == NULL || info == NULL || size <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    info->has_transparency = 0;
    info->image_count = 0;
    if ((size_t)size < 13u) {
        return SIXEL_BAD_INPUT;
    }

    p = buffer;
    end = buffer + (size_t)size;
    if (memcmp(p, "GIF", 3u) != 0) {
        return SIXEL_BAD_INPUT;
    }

    packed = p[10u];
    p += 13u;
    if ((packed & 0x80u) != 0u) {
        gct_size = gif_color_table_bytes[(size_t)packed & 0x07u];
        if ((size_t)(end - p) < gct_size) {
            return SIXEL_BAD_INPUT;
        }
        p += gct_size;
    }

    while (p < end) {
        marker = *p++;
        if (marker == 0x3bu) {
            status = SIXEL_OK;
            goto end;
        }

        if (marker == 0x2cu) {
            ++info->image_count;
            if ((size_t)(end - p) < 9u) {
                status = SIXEL_BAD_INPUT;
                goto end;
            }

            packed = p[8u];
            p += 9u;
            if ((packed & 0x80u) != 0u) {
                lct_size = gif_color_table_bytes[(size_t)packed & 0x07u];
                if ((size_t)(end - p) < lct_size) {
                    status = SIXEL_BAD_INPUT;
                    goto end;
                }
                p += lct_size;
            }
            if (p >= end) {
                status = SIXEL_BAD_INPUT;
                goto end;
            }
            ++p;  /* LZW minimum code size */

            for (;;) {
                if (p >= end) {
                    status = SIXEL_BAD_INPUT;
                    goto end;
                }
                block_size = *p++;
                if (block_size == 0u) {
                    break;
                }
                if ((size_t)(end - p) < (size_t)block_size) {
                    status = SIXEL_BAD_INPUT;
                    goto end;
                }
                p += block_size;
            }
            continue;
        }

        if (marker != 0x21u) {
            status = SIXEL_BAD_INPUT;
            goto end;
        }
        if (p >= end) {
            status = SIXEL_BAD_INPUT;
            goto end;
        }

        extension_label = *p++;
        if (extension_label == 0xf9u) {
            if (p >= end) {
                status = SIXEL_BAD_INPUT;
                goto end;
            }
            block_size = *p++;
            if (block_size != 4u) {
                if ((size_t)(end - p) < (size_t)block_size) {
                    status = SIXEL_BAD_INPUT;
                    goto end;
                }
                p += block_size;
            } else {
                if ((size_t)(end - p) < 4u) {
                    status = SIXEL_BAD_INPUT;
                    goto end;
                }
                packed = p[0u];
                p += 4u;
                if ((packed & 0x01u) != 0u) {
                    info->has_transparency = 1;
                }
            }
        }

        for (;;) {
            if (p >= end) {
                status = SIXEL_BAD_INPUT;
                goto end;
            }
            block_size = *p++;
            if (block_size == 0u) {
                break;
            }
            if ((size_t)(end - p) < (size_t)block_size) {
                status = SIXEL_BAD_INPUT;
                goto end;
            }
            p += block_size;
        }
    }

    status = SIXEL_BAD_INPUT;

end:
    return status;
}


static SIXELSTATUS
gif_export_pal8_frame(
    sixel_frame_t /* in */ *frame,
    gif_t         /* in */ *pg,
    int           /* in */ reqcolors)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    size_t pixel_total;
    size_t table_size;
    size_t table_mask;
    size_t pixel_index;
    size_t offset;
    size_t step;
    size_t slot;
    int maxcolors;
    int max_opaque_colors;
    int ncolors;
    int has_transparent_pixels;
    int keycolor_index;
    int found;
    int lookup_index;
    unsigned int key;
    unsigned int mixed;
    unsigned int probe;
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char *indices;
    unsigned char *palette;
    unsigned int *keys;
    unsigned char *values;

    status = SIXEL_FALSE;
    allocator = NULL;
    pixel_total = 0u;
    table_size = 0u;
    table_mask = 0u;
    pixel_index = 0u;
    offset = 0u;
    step = 0u;
    slot = 0u;
    maxcolors = 0;
    max_opaque_colors = 0;
    ncolors = 0;
    has_transparent_pixels = 0;
    keycolor_index = -1;
    found = 0;
    lookup_index = 0;
    key = 0u;
    mixed = 0u;
    probe = 0u;
    r = 0u;
    g = 0u;
    b = 0u;
    indices = NULL;
    palette = NULL;
    keys = NULL;
    values = NULL;

    if (frame == NULL || pg == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    allocator = sixel_frame_get_allocator(frame);
    if (allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (pg->w <= 0 || pg->h <= 0) {
        return SIXEL_BAD_INPUT;
    }
    if ((size_t)pg->w > SIZE_MAX / (size_t)pg->h) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    pixel_total = (size_t)pg->w * (size_t)pg->h;
    if (pixel_total > SIXEL_ALLOCATE_BYTES_MAX) {
        return SIXEL_BAD_ALLOCATION;
    }

    maxcolors = reqcolors;
    if (maxcolors <= 0 || maxcolors > SIXEL_PALETTE_MAX) {
        maxcolors = SIXEL_PALETTE_MAX;
    }

    if (pg->preserve_transparency != 0 && pg->alpha_out != NULL) {
        for (pixel_index = 0u; pixel_index < pixel_total; ++pixel_index) {
            if (pg->alpha_out[pixel_index] == 0u) {
                has_transparent_pixels = 1;
                break;
            }
        }
    }

    max_opaque_colors = maxcolors - (has_transparent_pixels != 0 ? 1 : 0);
    if (max_opaque_colors < 0) {
        loader_trace_message(
            "fromgif: PAL8 fallback reason=color_overflow reqcolors=%d "
            "max_opaque=%d has_transparent=%d",
            maxcolors,
            max_opaque_colors,
            has_transparent_pixels);
        status = SIXEL_FALSE;
        goto end;
    }

    indices = (unsigned char *)sixel_allocator_malloc(allocator, pixel_total);
    if (indices == NULL) {
        sixel_helper_set_additional_message(
            "gif_export_pal8_frame: indices allocation failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    palette = (unsigned char *)sixel_allocator_malloc(
        allocator,
        (size_t)maxcolors * (size_t)GIF_RGB_STRIDE);
    if (palette == NULL) {
        sixel_helper_set_additional_message(
            "gif_export_pal8_frame: palette allocation failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    if (max_opaque_colors > 0) {
        table_size = 2048u;
        while (table_size < (size_t)max_opaque_colors * 4u) {
            table_size <<= 1u;
        }
        table_mask = table_size - 1u;

        keys = (unsigned int *)sixel_allocator_malloc(
            allocator,
            sizeof(unsigned int) * table_size);
        values = (unsigned char *)sixel_allocator_malloc(allocator, table_size);
        if (keys == NULL || values == NULL) {
            sixel_helper_set_additional_message(
                "gif_export_pal8_frame: hash allocation failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        for (pixel_index = 0u; pixel_index < table_size; ++pixel_index) {
            keys[pixel_index] = GIF_HASH_EMPTY_KEY;
            values[pixel_index] = 0u;
        }
    }

    for (pixel_index = 0u; pixel_index < pixel_total; ++pixel_index) {
        if (has_transparent_pixels && pg->alpha_out[pixel_index] == 0u) {
            indices[pixel_index] = 0u;
            continue;
        }
        if (max_opaque_colors == 0) {
            status = SIXEL_FALSE;
            goto end;
        }

        offset = pixel_index * (size_t)GIF_RGB_STRIDE;
        r = pg->out[offset + 0u];
        g = pg->out[offset + 1u];
        b = pg->out[offset + 2u];
        key = ((unsigned int)r << 16)
            | ((unsigned int)g << 8)
            | (unsigned int)b;
        mixed = key;
        mixed ^= mixed >> 13;
        mixed *= 0x9e3779b1u;
        mixed ^= mixed >> 16;
        step = 0u;
        found = 0;
        lookup_index = 0;
        slot = 0u;

        if (keys != NULL && values != NULL) {
            for (;;) {
                slot = (mixed + step) & table_mask;
                probe = keys[slot];
                if (probe == GIF_HASH_EMPTY_KEY) {
                    break;
                }
                if (probe == key) {
                    lookup_index = (int)values[slot];
                    if (lookup_index < ncolors &&
                        palette[(size_t)lookup_index * 3u + 0u] == r &&
                        palette[(size_t)lookup_index * 3u + 1u] == g &&
                        palette[(size_t)lookup_index * 3u + 2u] == b) {
                        found = 1;
                        break;
                    }
                }
                ++step;
                if (step > table_mask) {
                    break;
                }
            }
        }

        if (!found && (keys == NULL || step > table_mask)) {
            for (lookup_index = 0; lookup_index < ncolors; ++lookup_index) {
                if (palette[(size_t)lookup_index * 3u + 0u] == r &&
                    palette[(size_t)lookup_index * 3u + 1u] == g &&
                    palette[(size_t)lookup_index * 3u + 2u] == b) {
                    found = 1;
                    break;
                }
            }
        }

        if (!found) {
            if (ncolors >= max_opaque_colors) {
                loader_trace_message(
                    "fromgif: PAL8 fallback reason=color_overflow "
                    "reqcolors=%d max_opaque=%d has_transparent=%d",
                    maxcolors,
                    max_opaque_colors,
                    has_transparent_pixels);
                status = SIXEL_FALSE;
                goto end;
            }
            lookup_index = ncolors;
            palette[(size_t)lookup_index * 3u + 0u] = r;
            palette[(size_t)lookup_index * 3u + 1u] = g;
            palette[(size_t)lookup_index * 3u + 2u] = b;
            ++ncolors;

            if (keys != NULL && values != NULL && step <= table_mask) {
                keys[slot] = key;
                values[slot] = (unsigned char)lookup_index;
            }
        }

        indices[pixel_index] = (unsigned char)lookup_index;
    }

    if (has_transparent_pixels) {
        keycolor_index = ncolors;
        if (keycolor_index >= maxcolors) {
            loader_trace_message(
                "fromgif: PAL8 fallback reason=keycolor_overflow "
                "reqcolors=%d max_opaque=%d has_transparent=%d",
                maxcolors,
                max_opaque_colors,
                has_transparent_pixels);
            status = SIXEL_FALSE;
            goto end;
        }
        palette[(size_t)keycolor_index * 3u + 0u] = 0u;
        palette[(size_t)keycolor_index * 3u + 1u] = 0u;
        palette[(size_t)keycolor_index * 3u + 2u] = 0u;
        for (pixel_index = 0u; pixel_index < pixel_total; ++pixel_index) {
            if (pg->alpha_out[pixel_index] == 0u) {
                indices[pixel_index] = (unsigned char)keycolor_index;
            }
        }
        ++ncolors;
    }

    if (ncolors <= 0 || ncolors > maxcolors) {
        status = SIXEL_FALSE;
        goto end;
    }

    sixel_frame_set_pixels(frame, indices);
    sixel_frame_set_palette(frame, palette);
    sixel_frame_set_ncolors(frame, ncolors);
    frame->pixelformat = SIXEL_PIXELFORMAT_PAL8;
    frame->colorspace = SIXEL_COLORSPACE_GAMMA;
    frame->alpha_zero_is_transparent = 0;
    sixel_frame_set_transparent(frame, keycolor_index);

    indices = NULL;
    palette = NULL;
    status = SIXEL_OK;

end:
    sixel_allocator_free(allocator, values);
    sixel_allocator_free(allocator, keys);
    sixel_allocator_free(allocator, palette);
    sixel_allocator_free(allocator, indices);
    return status;
}


static SIXELSTATUS
gif_export_nonpal_frame(
    sixel_frame_t /* in */ *frame,
    gif_t         /* in */ *pg)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    size_t frame_pixels;
    size_t frame_size;
    size_t pixel_index;
    size_t src_offset;
    size_t dst_offset;
    int has_alpha;
    int output_stride;
    unsigned char *pixels;

    status = SIXEL_FALSE;
    allocator = NULL;
    frame_pixels = 0u;
    frame_size = 0u;
    pixel_index = 0u;
    src_offset = 0u;
    dst_offset = 0u;
    has_alpha = 0;
    output_stride = GIF_RGB_STRIDE;
    pixels = NULL;

    if (frame == NULL || pg == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    allocator = sixel_frame_get_allocator(frame);
    if (allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (pg->w <= 0 || pg->h <= 0) {
        return SIXEL_BAD_INPUT;
    }
    if ((size_t)pg->w > SIZE_MAX / (size_t)pg->h) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    frame_pixels = (size_t)pg->w * (size_t)pg->h;
    has_alpha = (pg->preserve_transparency != 0 && pg->alpha_out != NULL)
        ? 1 : 0;
    output_stride = has_alpha ? GIF_RGBA_STRIDE : GIF_RGB_STRIDE;
    if (frame_pixels > SIZE_MAX / (size_t)output_stride) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    frame_size = frame_pixels * (size_t)output_stride;

    pixels = (unsigned char *)sixel_allocator_malloc(allocator, frame_size);
    if (pixels == NULL) {
        sixel_helper_set_additional_message(
            "gif_export_nonpal_frame: pixel allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    if (has_alpha) {
        for (pixel_index = 0u; pixel_index < frame_pixels; ++pixel_index) {
            src_offset = pixel_index * (size_t)GIF_RGB_STRIDE;
            dst_offset = pixel_index * (size_t)GIF_RGBA_STRIDE;
            pixels[dst_offset + 0u] = pg->out[src_offset + 0u];
            pixels[dst_offset + 1u] = pg->out[src_offset + 1u];
            pixels[dst_offset + 2u] = pg->out[src_offset + 2u];
            pixels[dst_offset + 3u] = pg->alpha_out[pixel_index];
        }
        frame->pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
    } else {
        memcpy(pixels, pg->out, frame_size);
        frame->pixelformat = SIXEL_PIXELFORMAT_RGB888;
    }

    sixel_frame_set_pixels(frame, pixels);
    frame->colorspace = SIXEL_COLORSPACE_GAMMA;
    sixel_frame_set_palette(frame, NULL);
    sixel_frame_set_ncolors(frame, -1);
    sixel_frame_set_transparent(frame, -1);
    frame->alpha_zero_is_transparent = has_alpha;

    status = SIXEL_OK;
    return status;
}


static SIXELSTATUS
gif_init_frame(
    sixel_frame_t /* in */ *frame,
    gif_t         /* in */ *pg,
    unsigned char /* in */ *bgcolor,
    int           /* in */ reqcolors,
    int           /* in */ fuse_palette)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    unsigned char *old_pixels;
    unsigned char *old_palette;

    (void)bgcolor;

    status = SIXEL_FALSE;
    allocator = NULL;
    old_pixels = NULL;
    old_palette = NULL;
    if (frame == NULL || pg == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    allocator = sixel_frame_get_allocator(frame);
    if (allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    old_pixels = sixel_frame_get_pixels(frame);
    old_palette = sixel_frame_get_palette(frame);
    sixel_allocator_free(allocator, old_pixels);
    sixel_allocator_free(allocator, old_palette);
    sixel_frame_set_pixels(frame, NULL);
    sixel_frame_set_palette(frame, NULL);
    sixel_frame_set_ncolors(frame, -1);
    frame->alpha_zero_is_transparent = 0;
    sixel_frame_set_transparent(frame, -1);

    sixel_frame_set_delay(frame, pg->delay);
    if (fuse_palette != 0) {
        status = gif_export_pal8_frame(frame, pg, reqcolors);
        if (status == SIXEL_OK) {
            goto finalize;
        }
        if (status != SIXEL_FALSE) {
            goto end;
        }
    }

    status = gif_export_nonpal_frame(frame, pg);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

finalize:
    sixel_frame_set_multiframe(frame, pg->stream_is_multiframe != 0);
    status = SIXEL_OK;

end:
    return status;
}


static SIXELSTATUS
gif_out_code(
    gif_t           /* in */ *g,
    unsigned short  /* in */ code
)
{
    size_t idx;
    size_t pixel_offset;
    size_t palette_offset;
    unsigned char suffix;
    SIXELSTATUS status;

    if (g == NULL || g->out == NULL || g->color_table == NULL) {
        sixel_helper_set_additional_message(
            "corrupt GIF (reason: decoder buffer unavailable).");
        return SIXEL_RUNTIME_ERROR;
    }

    if (code > GIF_LZW_MAX) {
        sixel_helper_set_additional_message("gif_out_code() failed; GIF file corrupt");
        return SIXEL_RUNTIME_ERROR;
    }

    /* recurse to decode the prefixes, since the linked-list is backwards,
       and working backwards through an interleaved image would be nasty */
    if (g->codes[code].prefix >= 0) {
        status = gif_out_code(g, (unsigned short)g->codes[code].prefix);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }

    if (g->cur_y >= g->max_y) {
        return SIXEL_OK;
    }

    idx = (size_t)g->cur_x + (size_t)g->cur_y * g->w;
    suffix = g->codes[code].suffix;
    if ((int)suffix >= g->color_table_entries) {
        sixel_helper_set_additional_message(
            "corrupt GIF (reason: color index out of range).");
        return SIXEL_RUNTIME_ERROR;
    }
    /*
     * Track every decoded pixel position, even when the source index is the
     * transparent index. GIF disposal methods 2/3 are defined on the whole
     * image rectangle of the previous frame, not only on non-transparent
     * writes. If we only mark opaque writes here, the next disposal step keeps
     * stale pixels and visible noise appears in animated transparent GIFs.
     */
    if (g->history) {
        g->history[idx] = 1;
    }
    if (!(g->transparent >= 0 && suffix == g->transparent)) {
        pixel_offset = idx * (size_t)GIF_RGB_STRIDE;
        palette_offset = (size_t)suffix * 3;
        g->out[pixel_offset + 0] = g->color_table[palette_offset + 2];
        g->out[pixel_offset + 1] = g->color_table[palette_offset + 1];
        g->out[pixel_offset + 2] = g->color_table[palette_offset + 0];
        if (g->alpha_out != NULL) {
            g->alpha_out[idx] = 0xffu;
        }
    }
    if (g->cur_x >= g->actual_width) {
        g->actual_width = g->cur_x + 1;
    }
    if (g->cur_y >= g->actual_height) {
        g->actual_height = g->cur_y + 1;
    }

    g->cur_x++;

    if (g->cur_x >= g->max_x) {
        g->cur_x = g->start_x;
        g->cur_y += g->step;

        while (g->cur_y >= g->max_y && g->parse > 0) {
            g->step = 1 << g->parse;
            g->cur_y = g->start_y + (g->step >> 1);
            --g->parse;
        }
    }

    return SIXEL_OK;
}


static SIXELSTATUS
gif_process_raster(
    gif_context_t /* in */ *s,
    gif_t         /* in */ *g
)
{
    SIXELSTATUS status = SIXEL_FALSE;
    unsigned char lzw_cs;
    unsigned char block_size;
    unsigned char next_byte;
    signed int len, code;
    signed int codesize, codemask, avail, oldcode, bits, valid_bits, clear;
    gif_lzw *p;

    block_size = 0u;
    next_byte = 0u;

    /* LZW Minimum Code Size */
    if (!gif_read_u8(s, &lzw_cs)) {
        sixel_helper_set_additional_message(
            "corrupt GIF (reason: truncated data block).");
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }
    if (lzw_cs > GIF_LZW_MAX_CODE_SIZE) {
        sixel_helper_set_additional_message(
            "Unsupported GIF (LZW code size)");
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }

    clear = 1 << lzw_cs;
    codesize = lzw_cs + 1;
    codemask = (1 << codesize) - 1;
    bits = 0;
    valid_bits = 0;
    for (code = 0; code < clear; code++) {
        g->codes[code].prefix = (-1);
        g->codes[code].first = (unsigned char) code;
        g->codes[code].suffix = (unsigned char) code;
    }

    /* support no starting clear code */
    avail = clear + 2;
    oldcode = (-1);

    len = 0;
    for(;;) {
        if (valid_bits < codesize) {
            if (len == 0) {
                if (!gif_read_u8(s, &block_size)) {
                    sixel_helper_set_additional_message(
                        "corrupt GIF (reason: truncated data block).");
                    status = SIXEL_RUNTIME_ERROR;
                    goto end;
                }
                len = (signed int)block_size; /* start new block */
                if (len == 0) {
                    status = SIXEL_OK;
                    goto end;
                }
            }
            --len;
            if (!gif_read_u8(s, &next_byte)) {
                sixel_helper_set_additional_message(
                    "corrupt GIF (reason: truncated data block).");
                status = SIXEL_RUNTIME_ERROR;
                goto end;
            }
            bits |= (signed int)next_byte << valid_bits;
            valid_bits += 8;
        } else {
            code = bits & codemask;
            bits >>= codesize;
            valid_bits -= codesize;
            /* @OPTIMIZE: is there some way we can accelerate the non-clear path? */
            if (code == clear) {  /* clear code */
                codesize = lzw_cs + 1;
                codemask = (1 << codesize) - 1;
                avail = clear + 2;
                oldcode = (-1);
            } else if (code == clear + 1) { /* end of stream code */
                if (len > 0 &&
                    !gif_skip_bytes(s, (size_t)len)) {
                    sixel_helper_set_additional_message(
                        "corrupt GIF (reason: truncated data block).");
                    status = SIXEL_RUNTIME_ERROR;
                    goto end;
                }
                len = 0;
                status = gif_skip_subblocks(s, 0);
                goto end;
            } else if (code <= avail) {
                if (oldcode >= 0) {
                    if (avail < (1 << GIF_LZW_MAX_CODE_SIZE)) {
                        p = &g->codes[avail++];
                        p->prefix = (signed short) oldcode;
                        p->first = g->codes[oldcode].first;
                        p->suffix = (code == avail) ? p->first : g->codes[code].first;
                    }
                } else if (code == avail) {
                    sixel_helper_set_additional_message(
                        "corrupt GIF (reason: illegal code in raster).");
                    status = SIXEL_RUNTIME_ERROR;
                    goto end;
                }

                status = gif_out_code(g, (unsigned short) code);
                if (status != SIXEL_OK) {
                    goto end;
                }

                if ((avail & codemask) == 0 && avail <= 0x0FFF) {
                    codesize++;
                    codemask = (1 << codesize) - 1;
                }

                oldcode = code;
            } else {
                sixel_helper_set_additional_message(
                    "corrupt GIF (reason: illegal code in raster).");
                status = SIXEL_RUNTIME_ERROR;
                goto end;
            }
        }
    }

    status = SIXEL_OK;

end:
    return status;
}


/* this function is ported from stb_image.h */
static SIXELSTATUS
gif_load_next(
    gif_context_t /* in */ *s,
    gif_t         /* in */ *g,
    unsigned char /* in */ *bgcolor
)
{
    SIXELSTATUS status = SIXEL_FALSE;
    unsigned char buffer[256];
    unsigned char c;
    unsigned char block_size;
    unsigned char extension_label;
    unsigned char extension_block[4];
    unsigned char loop_subtype;
    int x;
    int y;
    int w;
    int h;
    int table_entries;
    int loop_count_raw;
    int dispose;
    size_t pcount;
    size_t bcount;
    size_t i;
    size_t pixel_offset;
    unsigned char bg_r;
    unsigned char bg_g;
    unsigned char bg_b;
    unsigned char bg_a;

    block_size = 0u;
    extension_label = 0u;
    extension_block[0] = 0u;
    extension_block[1] = 0u;
    extension_block[2] = 0u;
    extension_block[3] = 0u;
    loop_subtype = 0u;
    table_entries = 0;
    loop_count_raw = 0;

    /* apply disposal of previous frame and prepare buffers */
    if (g->out) {
        if (g->w <= 0 || g->h <= 0 ||
            (size_t)g->w > SIZE_MAX / (size_t)g->h) {
            sixel_helper_set_additional_message(
                "corrupt GIF (reason: invalid image size).");
            status = SIXEL_BAD_INPUT;
            goto end;
        }
        if (g->w > SIXEL_WIDTH_LIMIT || g->h > SIXEL_HEIGHT_LIMIT) {
            sixel_helper_set_additional_message(
                "corrupt GIF (reason: image dimensions exceed limit).");
            status = SIXEL_BAD_INPUT;
            goto end;
        }
        pcount = (size_t)g->w * (size_t)g->h;
        if (pcount > SIXEL_ALLOCATE_BYTES_MAX) {
            sixel_helper_set_additional_message(
                "corrupt GIF (reason: image data exceeds limit).");
            status = SIXEL_BAD_INPUT;
            goto end;
        }
        if (pcount > SIZE_MAX / (size_t)GIF_RGB_STRIDE) {
            sixel_helper_set_additional_message(
                "corrupt GIF (reason: image data exceeds limit).");
            status = SIXEL_BAD_INPUT;
            goto end;
        }
        bcount = pcount * (size_t)GIF_RGB_STRIDE;

        gif_resolve_background_color(g, &bg_r, &bg_g, &bg_b);
        bg_a = g->preserve_transparency != 0 ? 0u : 0xffu;

        if (g->is_multiframe && g->history != NULL) {
            dispose = (g->eflags & 0x1C) >> 2;
            if (dispose == 3) {
                if (g->prev_out != NULL) {
                    for (i = 0; i < pcount; ++i) {
                        if (g->history[i]) {
                            pixel_offset = i * (size_t)GIF_RGB_STRIDE;
                            memcpy(g->out + pixel_offset,
                                   g->prev_out + pixel_offset,
                                   (size_t)GIF_RGB_STRIDE);
                            if (g->alpha_out != NULL && g->prev_alpha != NULL) {
                                g->alpha_out[i] = g->prev_alpha[i];
                            }
                        }
                    }
                } else {
                    for (i = 0; i < pcount; ++i) {
                        if (g->history[i]) {
                            pixel_offset = i * (size_t)GIF_RGB_STRIDE;
                            g->out[pixel_offset + 0] = bg_r;
                            g->out[pixel_offset + 1] = bg_g;
                            g->out[pixel_offset + 2] = bg_b;
                            if (g->alpha_out != NULL) {
                                g->alpha_out[i] = bg_a;
                            }
                        }
                    }
                }
            } else if (dispose == 2) {
                for (i = 0; i < pcount; ++i) {
                    if (g->history[i]) {
                        pixel_offset = i * (size_t)GIF_RGB_STRIDE;
                        g->out[pixel_offset + 0] = bg_r;
                        g->out[pixel_offset + 1] = bg_g;
                        g->out[pixel_offset + 2] = bg_b;
                        if (g->alpha_out != NULL) {
                            g->alpha_out[i] = bg_a;
                        }
                    }
                }
            }
        }

        if (g->prev_out) {
            memcpy(g->prev_out, g->out, bcount);
        }
        if (g->alpha_out != NULL && g->prev_alpha != NULL) {
            memcpy(g->prev_alpha, g->alpha_out, pcount);
        }
        if (g->history) {
            memset(g->history, 0, pcount);
        }

        g->eflags = 0;
        g->transparent = (-1);
        g->delay = SIXEL_DEFALUT_GIF_DELAY;
        g->is_multiframe = 1;
    }

    for (;;) {
        if (!gif_read_u8(s, &c)) {
            sixel_helper_set_additional_message(
                "corrupt GIF (reason: truncated data block).");
            status = SIXEL_RUNTIME_ERROR;
            goto end;
        }
        switch (c) {
        case 0x2C:  /* Image Separator (1 byte) */
            if (!gif_read_u16le(s, &x) || !gif_read_u16le(s, &y) ||
                !gif_read_u16le(s, &w) || !gif_read_u16le(s, &h) ||
                !gif_read_u8(s, &block_size)) {
                sixel_helper_set_additional_message(
                    "corrupt GIF (reason: truncated data block).");
                status = SIXEL_RUNTIME_ERROR;
                goto end;
            }
            if (x >= g->w || y >= g->h || x + w > g->w || y + h > g->h) {
                sixel_helper_set_additional_message(
                    "corrupt GIF (reason: bad Image Separator).");
                status = SIXEL_RUNTIME_ERROR;
                goto end;
            }

            g->line_size = g->w;
            g->start_x = x;
            g->start_y = y;
            g->max_x   = g->start_x + w;
            g->max_y   = g->start_y + h;
            g->cur_x   = g->start_x;
            g->cur_y   = g->start_y;
            g->actual_width   = g->start_x;
            g->actual_height   = g->start_y;

            /* Packed Fields (1 byte)
             * +-+-+-+--+---+
             * | | | |  |   |
             * +-+-+-+--+---+
             *  | | |  |  |
             *  | | |  |  +- Size of Local Color Table (3 bits)
             *  | | |  +- Reserved (2 bits)
             *  | | +- Sort Flag (1 bit)
             *  | +- Interlace Flag (1 bit)
             *  +- Local Color Table Flag (1 bit)
             */
            g->lflags = (int)block_size;

            /* Interlace Flag */
            if (g->lflags & 0x40) {
                g->step = 8; /* first interlaced spacing */
                g->parse = 3;
            } else {
                g->step = 1;
                g->parse = 0;
            }

            /* Local Color Table Flag */
            if (g->lflags & 0x80) {
                table_entries = 2 << (g->lflags & 7);
                status = gif_parse_colortable_checked(s, g->lpal, table_entries);
                if (SIXEL_FAILED(status)) {
                    goto end;
                }
                g->color_table_entries = table_entries;
                g->color_table = (unsigned char *) g->lpal;
            } else if (g->flags & 0x80) {
                if (!g->preserve_transparency &&
                    g->transparent >= 0 &&
                    (g->eflags & 0x01)) {
                   if (bgcolor) {
                       g->pal[g->transparent][0] = bgcolor[2];
                       g->pal[g->transparent][1] = bgcolor[1];
                       g->pal[g->transparent][2] = bgcolor[0];
                   }
                }
                g->color_table_entries = g->global_color_table_entries;
                g->color_table = (unsigned char *)g->pal;
            } else {
                sixel_helper_set_additional_message(
                    "corrupt GIF (reason: missing color table).");
                status = SIXEL_RUNTIME_ERROR;
                goto end;
            }

            status = gif_process_raster(s, g);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
            goto end;

        case 0x21:  /* Comment Extension. */
            if (!gif_read_u8(s, &extension_label)) {
                sixel_helper_set_additional_message(
                    "corrupt GIF (reason: truncated extension block).");
                status = SIXEL_RUNTIME_ERROR;
                goto end;
            }
            switch (extension_label) {
            case 0x01:  /* Plain Text Extension */
                status = gif_skip_subblocks(s, 1);
                if (SIXEL_FAILED(status)) {
                    goto end;
                }
                break;
            case 0xFE:  /* Comment Extension */
                status = gif_skip_subblocks(s, 1);
                if (SIXEL_FAILED(status)) {
                    goto end;
                }
                break;
            case 0xF9:  /* Graphic Control Extension */
                if (!gif_read_u8(s, &block_size)) {
                    sixel_helper_set_additional_message(
                        "corrupt GIF (reason: truncated extension block).");
                    status = SIXEL_RUNTIME_ERROR;
                    goto end;
                }
                if (block_size == 4u) {
                    if (!gif_read_u8(s, &extension_block[0]) ||
                        !gif_read_u8(s, &extension_block[1]) ||
                        !gif_read_u8(s, &extension_block[2]) ||
                        !gif_read_u8(s, &extension_block[3])) {
                        sixel_helper_set_additional_message(
                            "corrupt GIF (reason: truncated extension block).");
                        status = SIXEL_RUNTIME_ERROR;
                        goto end;
                    }
                    g->eflags = (int)extension_block[0];
                    g->delay = (int)extension_block[1]
                        | ((int)extension_block[2] << 8);
                    g->transparent = (int)extension_block[3];
                    if ((g->eflags & 0x01) == 0) {
                        g->transparent = (-1);
                    }
                } else if (!gif_skip_bytes(s, (size_t)block_size)) {
                    sixel_helper_set_additional_message(
                        "corrupt GIF (reason: truncated extension block).");
                    status = SIXEL_RUNTIME_ERROR;
                    goto end;
                }
                status = gif_skip_subblocks(s, 1);
                if (SIXEL_FAILED(status)) {
                    goto end;
                }
                break;
            case 0xFF:  /* Application Extension */
                if (!gif_read_u8(s, &block_size)) {
                    sixel_helper_set_additional_message(
                        "corrupt GIF (reason: truncated extension block).");
                    status = SIXEL_RUNTIME_ERROR;
                    goto end;
                }
                if ((size_t)(s->img_buffer_end - s->img_buffer) < (size_t)block_size) {
                    sixel_helper_set_additional_message(
                        "corrupt GIF (reason: truncated extension block).");
                    status = SIXEL_RUNTIME_ERROR;
                    goto end;
                }
                if (block_size > 0u) {
                    memcpy(buffer, s->img_buffer, (size_t)block_size);
                }
                buffer[block_size] = 0u;
                if (block_size == 11u &&
                    memcmp(buffer, "NETSCAPE2.0", 11u) == 0) {
                    if (!gif_skip_bytes(s, (size_t)block_size)) {
                        sixel_helper_set_additional_message(
                            "corrupt GIF (reason: truncated extension block).");
                        status = SIXEL_RUNTIME_ERROR;
                        goto end;
                    }
                    if (!gif_read_u8(s, &block_size)) {
                        sixel_helper_set_additional_message(
                            "corrupt GIF (reason: truncated extension block).");
                        status = SIXEL_RUNTIME_ERROR;
                        goto end;
                    }
                    if (block_size == 3u) {
                        if (!gif_read_u8(s, &loop_subtype) ||
                            !gif_read_u16le(s, &loop_count_raw)) {
                            sixel_helper_set_additional_message(
                                "corrupt GIF (reason: truncated extension block).");
                            status = SIXEL_RUNTIME_ERROR;
                            goto end;
                        }
                        if (loop_subtype == 0x01u) {
                            g->loop_count = loop_count_raw;
                        }
                    } else if (!gif_skip_bytes(s, (size_t)block_size)) {
                        sixel_helper_set_additional_message(
                            "corrupt GIF (reason: truncated extension block).");
                        status = SIXEL_RUNTIME_ERROR;
                        goto end;
                    }
                } else if (!gif_skip_bytes(s, (size_t)block_size)) {
                    sixel_helper_set_additional_message(
                        "corrupt GIF (reason: truncated extension block).");
                    status = SIXEL_RUNTIME_ERROR;
                    goto end;
                }
                status = gif_skip_subblocks(s, 1);
                if (SIXEL_FAILED(status)) {
                    goto end;
                }
                break;
            default:
                status = gif_skip_subblocks(s, 1);
                if (SIXEL_FAILED(status)) {
                    goto end;
                }
                break;
            }
            break;

        case 0x3B:  /* gif stream termination code */
            g->is_terminated = 1;
            status = SIXEL_OK;
            goto end;

        default:
            sixel_compat_snprintf(
                (char *)buffer,
                sizeof(buffer),
                "corrupt GIF (reason: unknown code %02x).",
                c);
            sixel_helper_set_additional_message((char *)buffer);
            status = SIXEL_RUNTIME_ERROR;
            goto end;
        }
    }

    status = SIXEL_OK;

end:
    return status;
}

/*
 * Local function pointer helper to avoid clashing with loader-builtin when
 * amalgamated into a single translation unit.
 */
typedef union sixel_fromgif_fn_pointer {
    sixel_load_image_function fn;
    void *                    p;
} sixel_fromgif_fn_pointer_t;

SIXELSTATUS
load_gif(
    unsigned char       /* in */ *buffer,
    int                 /* in */ size,
    unsigned char       /* in */ *bgcolor,
    int                 /* in */ reqcolors,
    int                 /* in */ fuse_palette,
    int                 /* in */ fstatic,
    int                 /* in */ loop_control,
    int                 /* in */ start_frame_no,
    void                /* in */ *fn_load,     /* callback */
    void                /* in */ *context,     /* private data for callback */
    sixel_allocator_t   /* in */ *allocator)   /* allocator object */
{
    gif_context_t s;
    gif_t *g;
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_frame_t *frame;
    sixel_fromgif_fn_pointer_t fnp;
    char message[256];
    size_t pcount;
    size_t bcount;
    size_t i;
    unsigned char bg_r;
    unsigned char bg_g;
    unsigned char bg_b;
    unsigned char bg_a;
    int emit_frame;
    int decoded_frame_no;
    int emitted_frame_no;
    gif_stream_info_t stream_info;
    int preserve_transparency;
    int stream_scan_failed;
    int need_multiframe_buffers;

    frame = NULL;
    g = NULL;
    fnp.p = fn_load;
    bg_a = 0u;
    stream_info.has_transparency = 0;
    stream_info.image_count = 0;
    preserve_transparency = 0;
    stream_scan_failed = 0;
    need_multiframe_buffers = 0;

    if (buffer == NULL || size <= 0 || fn_load == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    status = sixel_frame_new(&frame, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    g = (gif_t *)sixel_allocator_malloc(allocator, sizeof(*g));
    if (g == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    s.img_buffer = s.img_buffer_original = (unsigned char *)buffer;
    s.img_buffer_end = (unsigned char *)buffer + size;
    memset(g, 0, sizeof(*g));
    status = gif_scan_stream_info((unsigned char const *)buffer,
                                  size,
                                  &stream_info);
    if (SIXEL_FAILED(status)) {
        /*
         * Transparency pre-scan is advisory. Decode continues on the legacy
         * RGB path when the stream cannot be scanned safely.
         */
        loader_trace_message(
            "fromgif: stream pre-scan failed; using safe multiframe buffers.");
        stream_scan_failed = 1;
        stream_info.has_transparency = 0;
        stream_info.image_count = 1;
        status = SIXEL_OK;
    }
    preserve_transparency =
        bgcolor == NULL && stream_info.has_transparency != 0 ? 1 : 0;
    if (stream_info.image_count <= 0) {
        stream_info.image_count = 1;
    }
    g->stream_is_multiframe = stream_info.image_count > 1 ? 1 : 0;
    need_multiframe_buffers =
        (g->stream_is_multiframe != 0 || stream_scan_failed != 0) ? 1 : 0;
    g->preserve_transparency = preserve_transparency;
    g->delay = SIXEL_DEFALUT_GIF_DELAY;
    g->loop_count = -1;
    status = gif_load_header(&s, g);
    if (status != SIXEL_OK) {
        goto end;
    }
    if (g->w <= 0 || g->h <= 0 ||
        (size_t)g->w > SIZE_MAX / (size_t)g->h) {
        sixel_helper_set_additional_message(
            "corrupt GIF (reason: invalid image size).");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    if (g->w > SIXEL_WIDTH_LIMIT || g->h > SIXEL_HEIGHT_LIMIT) {
        sixel_helper_set_additional_message(
            "corrupt GIF (reason: image dimensions exceed limit).");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    pcount = (size_t)g->w * (size_t)g->h;
    if (pcount > SIXEL_ALLOCATE_BYTES_MAX) {
        sixel_helper_set_additional_message(
            "corrupt GIF (reason: image data exceeds limit).");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    if (pcount > SIZE_MAX / (size_t)GIF_RGB_STRIDE) {
        sixel_helper_set_additional_message(
            "corrupt GIF (reason: image data exceeds limit).");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    bcount = pcount * (size_t)GIF_RGB_STRIDE;

    g->out = (unsigned char *)sixel_allocator_malloc(allocator, bcount);
    g->prev_out = NULL;
    g->alpha_out = NULL;
    g->prev_alpha = NULL;
    g->history = NULL;
    if (need_multiframe_buffers != 0) {
        g->prev_out = (unsigned char *)sixel_allocator_malloc(allocator, bcount);
        g->history = (unsigned char *)sixel_allocator_malloc(allocator, pcount);
    }
    if (g->preserve_transparency != 0) {
        g->alpha_out = (unsigned char *)sixel_allocator_malloc(allocator, pcount);
        if (need_multiframe_buffers != 0) {
            g->prev_alpha = (unsigned char *)sixel_allocator_malloc(
                allocator,
                pcount);
        }
    }
    if (g->out == NULL ||
        (need_multiframe_buffers != 0 &&
         (g->prev_out == NULL || g->history == NULL)) ||
        (g->preserve_transparency != 0 && g->alpha_out == NULL) ||
        (g->preserve_transparency != 0 &&
         need_multiframe_buffers != 0 &&
         g->prev_alpha == NULL)) {
        sixel_compat_snprintf(
            message,
            sizeof(message),
            "load_gif: sixel_allocator_malloc() failed. size=%zu.",
            pcount);
        sixel_helper_set_additional_message(message);
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    gif_resolve_background_color(g, &bg_r, &bg_g, &bg_b);
    bg_a = g->preserve_transparency != 0 ? 0u : 0xffu;
    for (i = 0; i < pcount; ++i) {
        size_t pixel_offset;

        pixel_offset = i * (size_t)GIF_RGB_STRIDE;
        g->out[pixel_offset + 0] = bg_r;
        g->out[pixel_offset + 1] = bg_g;
        g->out[pixel_offset + 2] = bg_b;
        if (g->prev_out != NULL) {
            g->prev_out[pixel_offset + 0] = bg_r;
            g->prev_out[pixel_offset + 1] = bg_g;
            g->prev_out[pixel_offset + 2] = bg_b;
        }
        if (g->alpha_out != NULL) {
            g->alpha_out[i] = bg_a;
            if (g->prev_alpha != NULL) {
                g->prev_alpha[i] = bg_a;
            }
        }
    }
    if (g->history != NULL) {
        memset(g->history, 0, pcount);
    }

    sixel_frame_set_loop_count(frame, 0);

    for (;;) { /* per loop */

        sixel_frame_set_frame_no(frame, 0);
        decoded_frame_no = 0;
        emitted_frame_no = 0;

        s.img_buffer = s.img_buffer_original;
        g->loop_count = -1;
        status = gif_load_header(&s, g);
        if (status != SIXEL_OK) {
            goto end;
        }
        if (g->w <= 0 || g->h <= 0 ||
            (size_t)g->w > SIZE_MAX / (size_t)g->h) {
            sixel_helper_set_additional_message(
                "corrupt GIF (reason: invalid image size).");
            status = SIXEL_BAD_INPUT;
            goto end;
        }
        if (g->w > SIXEL_WIDTH_LIMIT || g->h > SIXEL_HEIGHT_LIMIT) {
            sixel_helper_set_additional_message(
                "corrupt GIF (reason: image dimensions exceed limit).");
            status = SIXEL_BAD_INPUT;
            goto end;
        }
        pcount = (size_t)g->w * (size_t)g->h;
        if (pcount > SIXEL_ALLOCATE_BYTES_MAX) {
            sixel_helper_set_additional_message(
                "corrupt GIF (reason: image data exceeds limit).");
            status = SIXEL_BAD_INPUT;
            goto end;
        }
        if (pcount > SIZE_MAX / (size_t)GIF_RGB_STRIDE) {
            sixel_helper_set_additional_message(
                "corrupt GIF (reason: image data exceeds limit).");
            status = SIXEL_BAD_INPUT;
            goto end;
        }

        /* reset canvas for new loop */
        gif_resolve_background_color(g, &bg_r, &bg_g, &bg_b);
        bg_a = g->preserve_transparency != 0 ? 0u : 0xffu;
        for (i = 0; i < pcount; ++i) {
            size_t pixel_offset;

            pixel_offset = i * (size_t)GIF_RGB_STRIDE;
            g->out[pixel_offset + 0] = bg_r;
            g->out[pixel_offset + 1] = bg_g;
            g->out[pixel_offset + 2] = bg_b;
            if (g->prev_out != NULL) {
                g->prev_out[pixel_offset + 0] = bg_r;
                g->prev_out[pixel_offset + 1] = bg_g;
                g->prev_out[pixel_offset + 2] = bg_b;
            }
            if (g->alpha_out != NULL) {
                g->alpha_out[i] = bg_a;
                if (g->prev_alpha != NULL) {
                    g->prev_alpha[i] = bg_a;
                }
            }
        }
        if (g->history != NULL) {
            memset(g->history, 0, pcount);
        }
        g->is_multiframe = 0;

        g->is_terminated = 0;

        for (;;) { /* per frame */
            status = gif_load_next(&s, g, bgcolor);
            if (status != SIXEL_OK) {
                goto end;
            }
            if (g->is_terminated) {
                break;
            }

            /*
             * GIF image descriptors may encode only a dirty rectangle with
             * non-zero offsets. The decoder composites each frame onto the
             * full logical screen in g.out, so exported frame dimensions must
             * remain the logical screen size. Using the descriptor bounds here
             * would make the packed RGB buffer width/height inconsistent and
             * produce corrupted output on partial-update frames.
             */
            sixel_frame_set_width(frame, g->w);
            sixel_frame_set_height(frame, g->h);
            status = gif_init_frame(frame, g, bgcolor, reqcolors, fuse_palette);
            if (status != SIXEL_OK) {
                goto end;
            }

            emit_frame = 1;
            /*
             * Start-frame override applies only to the first decoded loop.
             * Later loops always emit from frame 0 to preserve GIF looping.
             */
            if (start_frame_no >= 0 &&
                sixel_frame_get_loop_no(frame) == 0 &&
                decoded_frame_no < start_frame_no) {
                emit_frame = 0;
            }

            if (emit_frame) {
                /*
                 * Report frame numbers relative to the emitted stream.
                 * This keeps the first emitted frame at index 0 even when
                 * start-frame skipping suppresses earlier decoded frames.
                 */
                sixel_frame_set_frame_no(frame, emitted_frame_no);
                status = fnp.fn(frame, context);
                if (status != SIXEL_OK) {
                    goto end;
                }

                if (fstatic) {
                    goto end;
                }
                ++emitted_frame_no;
            }

            ++decoded_frame_no;
            sixel_frame_increment_frame_no(frame);
        }

        sixel_frame_increment_loop_count(frame);

        if (g->loop_count < 0) {
            break;
        }
        if (loop_control == SIXEL_LOOP_DISABLE ||
            sixel_frame_get_frame_no(frame) == 1) {
            break;
        }
        if (loop_control == SIXEL_LOOP_AUTO) {
            if (sixel_frame_get_loop_no(frame) == g->loop_count) {
                break;
            }
        }
    }

end:
    if (g != NULL) {
        sixel_allocator_free(allocator, g->out);
        sixel_allocator_free(allocator, g->prev_out);
        sixel_allocator_free(allocator, g->alpha_out);
        sixel_allocator_free(allocator, g->prev_alpha);
        sixel_allocator_free(allocator, g->history);
        sixel_allocator_free(allocator, g);
    }
    if (frame != NULL) {
        sixel_frame_unref(frame);
    }

    return status;
}


#if defined(_MSC_VER)
#pragma warning(pop)
#endif  /* _MSC_VER */


/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
