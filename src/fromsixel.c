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
#include "decoder-image.h"
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

static int
sixel_clamp_hls_component(double value)
{
    int channel;

    channel = (int)(value + 0.5);
    if (channel < 0) {
        channel = 0;
    } else if (channel > 100) {
        channel = 100;
    }

    return channel;
}

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
}

static void
image_buffer_release_ormode_indexes(image_buffer_t *image,
                                    sixel_allocator_t *allocator)
{
    if (image == NULL || image->ormode_indexes == NULL) {
        return;
    }

    if (allocator != NULL) {
        sixel_allocator_free(allocator, image->ormode_indexes);
    } else {
        free(image->ormode_indexes);
    }
    image->ormode_indexes = NULL;
}

static SIXELSTATUS
image_buffer_enable_ormode(image_buffer_t *image,
                           sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    size_t pixels;
    size_t bytes;
    int n;

    status = SIXEL_BAD_ARGUMENT;
    if (image == NULL || image->pixels.p == NULL) {
        goto end;
    }

    pixels = (size_t)image->width * (size_t)image->height;
    if (image->depth == 1U) {
        memset(image->pixels.in_bytes, 0, pixels);
    } else if (image->depth == 2U) {
        for (n = 0; n < image->width * image->height; ++n) {
            image->pixels.in_shorts[n] = 0;
        }
    } else if (image->depth == 4U) {
        bytes = pixels * sizeof(*image->ormode_indexes);
        if (image->ormode_indexes == NULL) {
            image->ormode_indexes = (unsigned short *)
                sixel_allocator_malloc(allocator, bytes);
            if (image->ormode_indexes == NULL) {
                sixel_helper_set_additional_message(
                    "image_buffer_enable_ormode: "
                    "sixel_allocator_malloc() failed.");
                status = SIXEL_BAD_ALLOCATION;
                goto end;
            }
        }
        memset(image->ormode_indexes, 0, bytes);
        memset(image->pixels.in_bytes, 0, pixels * 4U);
    }

    status = SIXEL_OK;

end:
    return status;
}

static int
image_buffer_ormode_depth_is_supported(image_buffer_t const *image)
{
    return image->depth == 1U || image->depth == 2U ||
        (image->depth == 4U && image->ormode_indexes != NULL);
}

/*
 * Direct-color OR mode keeps a side-car index buffer while parsing bit
 * planes.  Finalizing after the parser has seen every palette definition
 * lets palette index 0 become an opaque color instead of transparent black.
 */
static void
image_buffer_finalize_ormode_direct(image_buffer_t *image)
{
    size_t pixels;
    size_t n;
    unsigned char *bytes;
    unsigned short palette_index;
    int color;

    if (image == NULL || image->depth != 4U ||
            image->pixels.p == NULL || image->ormode_indexes == NULL) {
        return;
    }

    pixels = (size_t)image->width * (size_t)image->height;
    for (n = 0; n < pixels; ++n) {
        palette_index = image->ormode_indexes[n];
        color = image->palette[palette_index];
        bytes = image->pixels.in_bytes + n * 4U;
        bytes[0] = (unsigned char)((color >> 16) & 0xff);
        bytes[1] = (unsigned char)((color >> 8) & 0xff);
        bytes[2] = (unsigned char)(color & 0xff);
        bytes[3] = 255u;
    }
}

/*
 * OR mode is a bit-plane dialect.  Keep this path separate from normal
 * repaint semantics so the common decoder path keeps its old overwrite
 * behavior.  The direct-color path only composes indexes here; a final linear
 * pass expands them to RGBA after palette definitions are complete.
 */
static void
image_buffer_ormode_store_sixel(image_buffer_t *image,
                                int pos_y,
                                int pos_x,
                                int repeat,
                                int bits,
                                int color_index)
{
    int i;
    int n;
    int max_index;
    int composed_index;
    size_t pos;
    unsigned char *bytes;
    unsigned short *shorts;
    unsigned short *indexes;

    if (!image_buffer_ormode_depth_is_supported(image)) {
        return;
    }

    max_index = -1;
    if (image->depth == 1U) {
        for (i = 0; i < 6; ++i) {
            if ((bits & (1 << i)) == 0) {
                continue;
            }
            pos = (size_t)image->width * (size_t)(pos_y + i) +
                (size_t)pos_x;
            bytes = image->pixels.in_bytes + pos;
            for (n = 0; n < repeat; ++n) {
                composed_index = bytes[n] | color_index;
                bytes[n] = (unsigned char)composed_index;
                if (max_index < composed_index) {
                    max_index = composed_index;
                }
            }
        }
    } else if (image->depth == 2U) {
        for (i = 0; i < 6; ++i) {
            if ((bits & (1 << i)) == 0) {
                continue;
            }
            pos = (size_t)image->width * (size_t)(pos_y + i) +
                (size_t)pos_x;
            shorts = image->pixels.in_shorts + pos;
            for (n = 0; n < repeat; ++n) {
                composed_index = shorts[n] | color_index;
                shorts[n] = (unsigned short)composed_index;
                if (max_index < composed_index) {
                    max_index = composed_index;
                }
            }
        }
    } else {
        for (i = 0; i < 6; ++i) {
            if ((bits & (1 << i)) == 0) {
                continue;
            }
            pos = (size_t)image->width * (size_t)(pos_y + i) +
                (size_t)pos_x;
            indexes = image->ormode_indexes + pos;
            for (n = 0; n < repeat; ++n) {
                composed_index = indexes[n] | color_index;
                indexes[n] = (unsigned short)composed_index;
            }
        }
    }

    if (max_index >= 0 && max_index + 1 > image->ncolors) {
        image->ncolors = max_index + 1;
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
    int painted_outside_raster;
    int param;
    int nparams;
    int params[DECSIXEL_PARAMS_MAX];
} parser_context_t;


/*
 * Primary color hues:
 *  blue:    0 degrees
 *  red:   120 degrees
 *  green: 240 degrees
 */
static int
hls_to_rgb(int hue, int lum, int sat)
{
    double min, max;
    int r, g, b;

    r = 0;
    g = 0;
    b = 0;

    if (sat == 0) {
        r = g = b = lum;
    }

    /* https://wikimedia.org/api/rest_v1/media/math/render/svg/17e876f7e3260ea7fed73f69e19c71eb715dd09d */
    max = lum + sat * (1.0 - (lum > 50 ? (2 * (lum / 100.0) - 1.0): - (2 * (lum / 100.0) - 1.0))) / 2.0;

    /* https://wikimedia.org/api/rest_v1/media/math/render/svg/f6721b57985ad83db3d5b800dc38c9980eedde1d */
    min = lum - sat * (1.0 - (lum > 50 ? (2 * (lum / 100.0) - 1.0): - (2 * (lum / 100.0) - 1.0))) / 2.0;

    /* sixel hue color ring is roteted -120 degree from nowadays general one. */
    hue = (hue + 240) % 360;

    /* https://wikimedia.org/api/rest_v1/media/math/render/svg/937e8abdab308a22ff99de24d645ec9e70f1e384 */
    switch (hue / 60) {
    case 0:  /* 0 <= hue < 60 */
        r = sixel_clamp_hls_component(max);
        g = sixel_clamp_hls_component(min + (max - min) * (hue / 60.0));
        b = sixel_clamp_hls_component(min);
        break;
    case 1:  /* 60 <= hue < 120 */
        r = sixel_clamp_hls_component(
                min + (max - min) * ((120 - hue) / 60.0));
        g = sixel_clamp_hls_component(max);
        b = sixel_clamp_hls_component(min);
        break;
    case 2:  /* 120 <= hue < 180 */
        r = sixel_clamp_hls_component(min);
        g = sixel_clamp_hls_component(max);
        b = sixel_clamp_hls_component(
                min + (max - min) * ((hue - 120) / 60.0));
        break;
    case 3:  /* 180 <= hue < 240 */
        r = sixel_clamp_hls_component(min);
        g = sixel_clamp_hls_component(
                min + (max - min) * ((240 - hue) / 60.0));
        b = sixel_clamp_hls_component(max);
        break;
    case 4:  /* 240 <= hue < 300 */
        r = sixel_clamp_hls_component(
                min + (max - min) * ((hue - 240) / 60.0));
        g = sixel_clamp_hls_component(min);
        b = sixel_clamp_hls_component(max);
        break;
    case 5:  /* 300 <= hue < 360 */
        r = sixel_clamp_hls_component(max);
        g = sixel_clamp_hls_component(min);
        b = sixel_clamp_hls_component(
                min + (max - min) * ((360 - hue) / 60.0));
        break;
    default:
#if HAVE___BUILTIN_UNREACHABLE
        __builtin_unreachable();
#endif
        break;
    }

    return SIXEL_XRGB(r, g, b);
}


SIXEL_INTERNAL_API SIXELSTATUS
image_buffer_init(
    image_buffer_t        *image,
    int                    width,
    int                    height,
    int                    bgindex,
    int                    depth,
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
    image->ormode_indexes = NULL;
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

        /* palette initialization */
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
    }
    status = SIXEL_OK;

end:
    return status;
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
    unsigned short *alt_ormode_indexes;
    size_t index_size;
    int n;
    int min_height;
    int copy_width;
    size_t stride;
    size_t old_stride;
    size_t copy_stride;
    size_t depth = image->depth;

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
    alt_ormode_indexes = NULL;
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
    if (image->ormode_indexes != NULL) {
        index_size = (size_t)width * (size_t)height *
            sizeof(*alt_ormode_indexes);
        alt_ormode_indexes = (unsigned short *)
            sixel_allocator_malloc(allocator, index_size);
        if (alt_ormode_indexes == NULL) {
            sixel_allocator_free(allocator, alt_buffer);
            sixel_helper_set_additional_message(
                "image_buffer_resize: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        memset(alt_ormode_indexes, 0, index_size);
    }

    min_height = height > image->height ? image->height: height;
    old_stride = (size_t)image->width * depth;
    copy_width = width > image->width ? image->width: width;
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
    if (alt_ormode_indexes != NULL) {
        for (n = 0; n < min_height; ++n) {
            memcpy(alt_ormode_indexes + (size_t)width * (size_t)n,
                   image->ormode_indexes +
                   (size_t)image->width * (size_t)n,
                   (size_t)copy_width * sizeof(*alt_ormode_indexes));
        }
    }

    if (height > image->height) {
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
    image_buffer_release_ormode_indexes(image, allocator);

    image->pixels.in_bytes = alt_buffer;
    image->ormode_indexes = alt_ormode_indexes;
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
    context->painted_outside_raster = 0;
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
                }

                if (context->nparams > 2) {
                    /* Pn3 */
                    int scaled_pan;
                    int scaled_pad;

                    if (context->params[2] == 0) {
                        context->params[2] = 10;
                    }

                    status = safe_multiply_by_params_div10(
                        context->attributed_pan,
                        context->params[2],
                        &scaled_pan);
                    if (SIXEL_FAILED(status)) {
                        goto end;
                    }

                    status = safe_multiply_by_params_div10(
                        context->attributed_pad,
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
                context->pos_y += 6;
#if SIXEL_ENABLE_THREADS
                if (!palette_ready) {
                    palette_ready = 1;
                }
                if (!parallel_started && raster_ready && palette_ready) {
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
#if SIXEL_ENABLE_THREADS
                    if (!palette_ready) {
                        palette_ready = 1;
                    }
                    if (!parallel_started && raster_ready &&
                            palette_ready) {
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

                    if (context->pos_x < 0 || context->pos_y < 0) {
                        status = SIXEL_BAD_INPUT;
                        goto end;
                    }
                    bits = *p - '?';
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

                    sx = image->width;
                    while (sx < context->pos_x + draw_repeat) {
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

                    if (context->ormode) {
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
                            image_buffer_ormode_store_sixel(
                                image,
                                context->pos_y,
                                context->pos_x,
                                draw_repeat,
                                store_bits,
                                context->color_index);
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
                if (!raster_ready && context->attributed_ph > 0 &&
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

                if (context->color_index + 1 > image->ncolors) {
                    image->ncolors = context->color_index + 1;
                    if (image->ncolors > SIXEL_PALETTE_MAX_DECODER) {
                        image->ncolors = SIXEL_PALETTE_MAX_DECODER;
                    }
                }

                if (context->nparams > 4) {
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
sixel_decode_image(
    unsigned char     *p,
    int                len,
    int                initial_width,
    int                initial_height,
    int                depth,
    image_buffer_t    *image,
    parser_context_t  *context,
    sixel_allocator_t *allocator,
    unsigned int       decode_flags,
    sixel_decoder_undither_context_t *undither)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_timeline_logger_t *logger;
    int logger_prepared;

    image->pixels.p = NULL;
    image->ormode_indexes = NULL;

    logger = NULL;
    logger_prepared = 0;
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
                               allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
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
        image_buffer_release_ormode_indexes(image, allocator);
        goto end;
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


/* convert sixel data into indexed pixel bytes and palette data */
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
            "sixel_decode_raw: malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto error;
    }
    image->pixels.p = NULL;

    status = sixel_decode_image(p,
                                len,
                                1,  /* initial_width */
                                1,  /* initial_height */
                                1,  /* depth */
                                image,
                                &context,
                                allocator,
                                0U,
                                NULL);
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
    *pixels = image->pixels.p;
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
    image_buffer_release_ormode_indexes(image, allocator);
    free(image);
    sixel_allocator_unref(allocator);
    return status;
}

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
    image->ormode_indexes = NULL;

    undither.enabled = 1;
    undither.direct_output = direct_output != 0;
    undither.similarity_bias = similarity_bias;
    undither.allocator = allocator;

    status = sixel_decode_image(p,
                                len,
                                1,
                                1,
                                1,
                                image,
                                &context,
                                allocator,
                                0U,
                                &undither);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    *pwidth = image->width;
    *pheight = image->height;
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
        image_buffer_release_ormode_indexes(image, allocator);
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

    status = sixel_decode_image(p,
                                len,
                                1,  /* initial width */
                                1,  /* initial height */
                                2,  /* depth */
                                image,
                                &context,
                                allocator,
                                0U,
                                NULL);
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
    image_buffer_release_ormode_indexes(image, allocator);
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
    unsigned int        *result_flags,
    sixel_allocator_t   *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    parser_context_t context;
    image_buffer_t *image = NULL;

    if (result_flags != NULL) {
        *result_flags = 0U;
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

    status = sixel_decode_image(p,
                                len,
                                1,
                                1,
                                4U,
                                image,
                                &context,
                                allocator,
                                decode_flags,
                                NULL);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    if (context.ormode) {
        image_buffer_finalize_ormode_direct(image);
    }

    *pwidth = image->width;
    *pheight = image->height;
    *pixels = image->pixels.in_bytes;
    if (result_flags != NULL && context.painted_outside_raster) {
        *result_flags |= SIXEL_DECODE_PIXELS_RESULT_PAINT_OUTSIDE_RASTER;
        if ((decode_flags &
                SIXEL_DECODE_PIXELS_OPTION_TRUST_RASTER_SIZE) != 0U) {
            *result_flags |= SIXEL_DECODE_PIXELS_RESULT_CLIPPED_TO_RASTER;
        }
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
    image_buffer_release_ormode_indexes(image, allocator);
    free(image);
    sixel_allocator_unref(allocator);
    return status;
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
