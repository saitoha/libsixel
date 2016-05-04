/*
 * This file is derived from "sixel" original version (2014-3-2)
 * http://nanno.dip.jp/softlib/man/rlogin/sixel.tar.gz
 *
 * Initial developer of this file is kmiya@culti.
 *
 * He distributes it under very permissive license which permits
 * useing, copying, modification, redistribution, and all other
 * public activities without any restrictions.
 *
 * He declares this is compatible with MIT/BSD/GPL.
 *
 * Hayaki Saito <saitoha@me.com> modified this and re-licensed
 * it under the MIT license.
 *
 * The helper function hls2rgb is imported from Xterm pl#310.
 * This is originally written by Ross Combs.
 * Hayaki Saito <saitoha@me.com> slightly modified this.
 *
 * -------------------------------------------------------------------------
 * Copyright 2013,2014 by Ross Combs
 *
 *                         All Rights Reserved
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name(s) of the above copyright
 * holders shall not be used in advertising or otherwise to promote the
 * sale, use or other dealings in this Software without prior written
 * authorization.
 * -------------------------------------------------------------------------
 */
#include "config.h"
#include <stdlib.h>  /* NULL */
#include <ctype.h>   /* isdigit */
#include <string.h>  /* memcpy */

#if defined(HAVE_INTTYPES_H)
# include <inttypes.h>
#endif

#include <sixel.h>
#include "output.h"

#define RGB(r, g, b) (((r) << 16) + ((g) << 8) +  (b))

#define PALVAL(n,a,m) (((n) * (a) + ((m) / 2)) / (m))

#define XRGB(r,g,b) RGB(PALVAL(r, 255, 100), PALVAL(g, 255, 100), PALVAL(b, 255, 100))

#define DECSIXEL_PARAMS_MAX 16

static int const color_table[] = {
    XRGB(0,  0,  0),   /*  0 Black    */
    XRGB(20, 20, 80),  /*  1 Blue     */
    XRGB(80, 13, 13),  /*  2 Red      */
    XRGB(20, 80, 20),  /*  3 Green    */
    XRGB(80, 20, 80),  /*  4 Magenta  */
    XRGB(20, 80, 80),  /*  5 Cyan     */
    XRGB(80, 80, 20),  /*  6 Yellow   */
    XRGB(53, 53, 53),  /*  7 Gray 50% */
    XRGB(26, 26, 26),  /*  8 Gray 25% */
    XRGB(33, 33, 60),  /*  9 Blue*    */
    XRGB(60, 26, 26),  /* 10 Red*     */
    XRGB(33, 60, 33),  /* 11 Green*   */
    XRGB(60, 33, 60),  /* 12 Magenta* */
    XRGB(33, 60, 60),  /* 13 Cyan*    */
    XRGB(60, 60, 33),  /* 14 Yellow*  */
    XRGB(80, 80, 80),  /* 15 Gray 75% */
};


typedef struct image_buffer {
    unsigned char *data;
    int width;
    int height;
} image_buffer_t;


typedef struct parser_context {
    int posision_x;
    int posision_y;
    int max_x;
    int max_y;
    int attributed_pan;
    int attributed_pad;
    int attributed_ph;
    int attributed_pv;
    int repeat_count;
    int color_index;
    int max_color_index;
    int bgindex;
    int params[DECSIXEL_PARAMS_MAX];
    int palette[SIXEL_PALETTE_MAX];
} parser_context_t;


/*
 * Primary color hues:
 *  blue:    0 degrees
 *  red:   120 degrees
 *  green: 240 degrees
 */
static int
hls2rgb(int hue, int lum, int sat)
{
    double hs = (hue + 240) % 360;
    double hv = hs / 360.0;
    double lv = lum / 100.0;
    double sv = sat / 100.0;
    double c, x, m, c2;
    double r1, g1, b1;
    int r, g, b;
    int hpi;

    if (sat == 0) {
        r = g = b = lum * 255 / 100;
        return RGB(r, g, b);
    }

    if ((c2 = ((2.0 * lv) - 1.0)) < 0.0)
        c2 = -c2;
    c = (1.0 - c2) * sv;
    hpi = (int) (hv * 6.0);
    x = (hpi & 1) ? c : 0.0;
    m = lv - 0.5 * c;

    switch (hpi) {
    case 0:
        r1 = c;
        g1 = x;
        b1 = 0.0;
        break;
    case 1:
        r1 = x;
        g1 = c;
        b1 = 0.0;
        break;
    case 2:
        r1 = 0.0;
        g1 = c;
        b1 = x;
        break;
    case 3:
        r1 = 0.0;
        g1 = x;
        b1 = c;
        break;
    case 4:
        r1 = x;
        g1 = 0.0;
        b1 = c;
        break;
    case 5:
        r1 = c;
        g1 = 0.0;
        b1 = x;
        break;
    default:
        return RGB(255, 255, 255);
    }

    r = (short) ((r1 + m) * 100.0 + 0.5);
    g = (short) ((g1 + m) * 100.0 + 0.5);
    b = (short) ((b1 + m) * 100.0 + 0.5);

    if (r < 0)
        r = 0;
    else if (r > 100)
        r = 100;
    if (g < 0)
        g = 0;
    else if (g > 100)
        g = 100;
    if (b < 0)
        b = 0;
    else if (b > 100)
        b = 100;
    return RGB(r * 255 / 100, g * 255 / 100, b * 255 / 100);
}


static unsigned char *
sixel_getparams(unsigned char *p, int *params, int *len)
{
    int n;

    *len = 0;
    while (*p != '\0') {
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        if (isdigit(*p)) {
            for (n = 0; isdigit(*p); p++) {
                n = n * 10 + (*p - '0');
            }
            if (*len < DECSIXEL_PARAMS_MAX) {
                params[(*len)++] = n;
            }
            while (*p == ' ' || *p == '\t') {
                p++;
            }
            if (*p == ';') {
                p++;
            }
        } else if (*p == ';') {
            if (*len < DECSIXEL_PARAMS_MAX) {
                params[(*len)++] = 0;
            }
            p++;
        } else
            break;
    }

    return p;
}



static SIXELSTATUS
image_buffer_init(
    image_buffer_t     *buffer,
    int                 width,
    int                 height,
    int                 bgindex,
    sixel_allocator_t  *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    size_t size;

    size = (size_t)(width * height);
    buffer->width = width;
    buffer->height = height;
    buffer->data = (unsigned char *)sixel_allocator_malloc(allocator, size);

    if (buffer->data == NULL) {
        sixel_helper_set_additional_message(
            "sixel_deocde_raw: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    memset(buffer->data, bgindex, size);

    status = SIXEL_OK;

end:
    return status;
}


static SIXELSTATUS
image_buffer_resize(
    image_buffer_t     *buffer,
    int                 width,
    int                 height,
    int                 bgindex,
    sixel_allocator_t  *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    size_t size;
    unsigned char *alt_buffer;
    int n;
    int min_height;

    size = (size_t)(width * height);
    alt_buffer = (unsigned char *)sixel_allocator_malloc(allocator, size);
    if (alt_buffer == NULL) {
        /* free source buffer */
        sixel_allocator_free(allocator, buffer->data);
        buffer->data = NULL;
        sixel_helper_set_additional_message(
            "image_buffer_resize: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    min_height = height > buffer->height ? buffer->height: height;
    if (width > buffer->width) {  /* if width is extended */
        for (n = 0; n < min_height; ++n) {
            /* copy from source buffer */
            memcpy(alt_buffer + width * n,
                   buffer->data + buffer->width * n,
                   (size_t)buffer->width);
            /* fill extended area with background color */
            memset(alt_buffer + width * n + buffer->width,
                   bgindex,
                   (size_t)(width - buffer->width));
        }
    } else {
        for (n = 0; n < min_height; ++n) {
            /* copy from source buffer */
            memcpy(alt_buffer + width * n,
                   buffer->data + buffer->width * n,
                   (size_t)width);
        }
    }

    if (height > buffer->height) {  /* if height is extended */
        /* fill extended area with background color */
        memset(alt_buffer + width * buffer->height,
               bgindex,
               (size_t)(width * (height - buffer->height)));
    }

    /* free source buffer */
    sixel_allocator_free(allocator, buffer->data);

    buffer->data = alt_buffer;
    buffer->width = width;
    buffer->height = height;

    status = SIXEL_OK;
end:
    return status;
}


static SIXELSTATUS
parser_context_init(parser_context_t *state)
{
    SIXELSTATUS status = SIXEL_FALSE;
    int i;
    int n;
    int r;
    int g;
    int b;

    state->posision_x = 0;
    state->posision_y = 0;
    state->max_x = 0;
    state->max_y = 0;
    state->attributed_pan = 2;
    state->attributed_pad = 1;
    state->attributed_ph = 0;
    state->attributed_pv = 0;
    state->repeat_count = 1;
    state->color_index = 15;
    state->max_color_index = 2;
    state->bgindex = 0;

    /* palette initialization */
    for (n = 0; n < 16; n++) {
        state->palette[n] = color_table[n];
    }

    /* colors 16-231 are a 6x6x6 color cube */
    for (r = 0; r < 6; r++) {
        for (g = 0; g < 6; g++) {
            for (b = 0; b < 6; b++) {
                state->palette[n++] = RGB(r * 51, g * 51, b * 51);
            }
        }
    }
    /* colors 232-255 are a grayscale ramp, intentionally leaving out */
    for (i = 0; i < 24; i++) {
        state->palette[n++] = RGB(i * 11, i * 11, i * 11);
    }

    for (; n < SIXEL_PALETTE_MAX; n++) {
        state->palette[n] = RGB(255, 255, 255);
    }

    status = SIXEL_OK;

    return status;
}


/* convert sixel data into indexed pixel bytes and palette data */
SIXELAPI SIXELSTATUS
sixel_decode_raw(
    unsigned char       /* in */  *p,         /* sixel bytes */
    int                 /* in */  len,        /* size of sixel bytes */
    unsigned char       /* out */ **pixels,   /* decoded pixels */
    int                 /* out */ *pwidth,    /* image width */
    int                 /* out */ *pheight,   /* image height */
    unsigned char       /* out */ **palette,  /* ARGB palette */
    int                 /* out */ *ncolors,   /* palette size (<= 256) */
    sixel_allocator_t   /* in */  *allocator) /* allocator object */
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

    image_buffer_t pbuffer;
    parser_context_t state;

    (void) len;

    if (pixels == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        sixel_helper_set_additional_message(
            "sixel_deocde_raw: given argument pixels is null.");
        goto end;
    }

    if (pwidth == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        sixel_helper_set_additional_message(
            "sixel_deocde_raw: given argument pwidth is null.");
        goto end;
    }

    if (pheight == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        sixel_helper_set_additional_message(
            "sixel_deocde_raw: given argument pheight is null.");
        goto end;
    }

    if (ncolors == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        sixel_helper_set_additional_message(
            "sixel_deocde_raw: given argument ncolors is null.");
        goto end;
    }

    if (palette == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        sixel_helper_set_additional_message(
            "sixel_deocde_raw: given argument palette is null.");
        goto end;
    }

    /* initialize return values */
    *pixels = NULL;
    *pwidth = (-1);
    *pheight = (-1);
    *ncolors = 0;
    *palette = NULL;

    /* initialize allocator */
    if (allocator == NULL) {  /* create an allocator */
        status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    } else {
        sixel_allocator_ref(allocator);
    }

    /* parser context initialization */
    status = parser_context_init(&state);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    /* buffer initialization */
    status = image_buffer_init(&pbuffer, 2048, 2048, state.bgindex, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    while (*p != '\0') {
        if ((p[0] == '\033' && p[1] == 'P') || *p == 0x90) {
            if (*p == '\033') {
                p++;
            }

            p = sixel_getparams(++p, state.params, &n);
            if (*p == 'q') {
                p++;
                if (n > 0) {        /* Pn1 */
                    switch(state.params[0]) {
                    case 0:
                    case 1:
                        state.attributed_pad = 2;
                        break;
                    case 2:
                        state.attributed_pad = 5;
                        break;
                    case 3:
                        state.attributed_pad = 4;
                        break;
                    case 4:
                        state.attributed_pad = 4;
                        break;
                    case 5:
                        state.attributed_pad = 3;
                        break;
                    case 6:
                        state.attributed_pad = 3;
                        break;
                    case 7:
                        state.attributed_pad = 2;
                        break;
                    case 8:
                        state.attributed_pad = 2;
                        break;
                    case 9:
                        state.attributed_pad = 1;
                        break;
                    default:
                        state.attributed_pad = 2;
                        break;
                    }
                }

                if (n > 2) {        /* Pn3 */
                    if (state.params[2] == 0) {
                        state.params[2] = 10;
                    }
                    state.attributed_pan = state.attributed_pan * state.params[2] / 10;
                    state.attributed_pad = state.attributed_pad * state.params[2] / 10;
                    if (state.attributed_pan <= 0) {
                        state.attributed_pan = 1;
                    }
                    if (state.attributed_pad <= 0) {
                        state.attributed_pad = 1;
                    }
                }
            }

        } else if ((p[0] == '\033' && p[1] == '\\') || *p == 0x9C) {
            break;
        } else if (*p == '"') {
            /* DECGRA Set Raster Attributes " Pan; Pad; Ph; Pv */
            p = sixel_getparams(p + 1, state.params, &n);

            if (n > 0) {
                state.attributed_pad = state.params[0];
            }
            if (n > 1) {
                state.attributed_pan = state.params[1];
            }
            if (n > 2 && state.params[2] > 0) {
                state.attributed_ph = state.params[2];
            }
            if (n > 3 && state.params[3] > 0) {
                state.attributed_pv = state.params[3];
            }

            if (state.attributed_pan <= 0) {
                state.attributed_pan = 1;
            }
            if (state.attributed_pad <= 0) {
                state.attributed_pad = 1;
            }

            if (pbuffer.width < state.attributed_ph || pbuffer.height < state.attributed_pv) {
                sx = pbuffer.width > state.attributed_ph ? pbuffer.width : state.attributed_ph;
                sy = pbuffer.height > state.attributed_pv ? pbuffer.height : state.attributed_pv;
                status = image_buffer_resize(&pbuffer, sx, sy, state.bgindex, allocator);
                if (SIXEL_FAILED(status)) {
                    goto end;
                }
            }

        } else if (*p == '!') {
            /* DECGRI Graphics Repeat Introducer ! Pn Ch */
            p = sixel_getparams(p + 1, state.params, &n);

            if (n > 0) {
                state.repeat_count = state.params[0];
            }

        } else if (*p == '#') {
            /* DECGCI Graphics Color Introducer # Pc; Pu; Px; Py; Pz */
            p = sixel_getparams(++p, state.params, &n);

            if (n > 0) {
                if ((state.color_index = state.params[0]) < 0) {
                    state.color_index = 0;
                } else if (state.color_index >= SIXEL_PALETTE_MAX) {
                    state.color_index = SIXEL_PALETTE_MAX - 1;
                }
            }

            if (n > 4) {
                if (state.params[1] == 1) {            /* HLS */
                    if (state.params[2] > 360) {
                        state.params[2] = 360;
                    }
                    if (state.params[3] > 100) {
                        state.params[3] = 100;
                    }
                    if (state.params[4] > 100) {
                        state.params[4] = 100;
                    }
                    state.palette[state.color_index]
                        = hls2rgb(state.params[2], state.params[3], state.params[4]);
                } else if (state.params[1] == 2) {    /* RGB */
                    if (state.params[2] > 100) {
                        state.params[2] = 100;
                    }
                    if (state.params[3] > 100) {
                        state.params[3] = 100;
                    }
                    if (state.params[4] > 100) {
                        state.params[4] = 100;
                    }
                    state.palette[state.color_index]
                        = XRGB(state.params[2], state.params[3], state.params[4]);
                }
            }

        } else if (*p == '$') {
            /* DECGCR Graphics Carriage Return */
            p++;
            state.posision_x = 0;
            state.repeat_count = 1;

        } else if (*p == '-') {
            /* DECGNL Graphics Next Line */
            p++;
            state.posision_x = 0;
            state.posision_y += 6;
            state.repeat_count = 1;

        } else if (*p >= '?' && *p <= '\177') {
            if (pbuffer.width < (state.posision_x + state.repeat_count) ||
                pbuffer.height < (state.posision_y + 6)) {
                sx = pbuffer.width * 2;
                sy = pbuffer.height * 2;
                while (sx < (state.posision_x + state.repeat_count) ||
                       sy < (state.posision_y + 6)) {
                    sx *= 2;
                    sy *= 2;
                }
                status = image_buffer_resize(&pbuffer, sx, sy, state.bgindex, allocator);
                if (SIXEL_FAILED(status)) {
                    goto end;
                }
            }

            if (state.color_index > state.max_color_index) {
                state.max_color_index = state.color_index;
            }
            if ((bits = *(p++) - '?') == 0) {
                state.posision_x += state.repeat_count;

            } else {
                sixel_vertical_mask = 0x01;

                if (state.repeat_count <= 1) {
                    for (i = 0; i < 6; i++) {
                        if ((bits & sixel_vertical_mask) != 0) {
                            pbuffer.data[pbuffer.width * (state.posision_y + i) + state.posision_x] = state.color_index;
                            if (state.max_x < state.posision_x) {
                                state.max_x = state.posision_x;
                            }
                            if (state.max_y < (state.posision_y + i)) {
                                state.max_y = state.posision_y + i;
                            }
                        }
                        sixel_vertical_mask <<= 1;
                    }
                    state.posision_x += 1;

                } else { /* state.repeat_count > 1 */
                    for (i = 0; i < 6; i++) {
                        if ((bits & sixel_vertical_mask) != 0) {
                            c = sixel_vertical_mask << 1;
                            for (n = 1; (i + n) < 6; n++) {
                                if ((bits & c) == 0) {
                                    break;
                                }
                                c <<= 1;
                            }
                            for (y = state.posision_y + i; y < state.posision_y + i + n; ++y) {
                                memset(pbuffer.data + pbuffer.width * y + state.posision_x, state.color_index, (size_t)state.repeat_count);
                            }
                            if (state.max_x < (state.posision_x + state.repeat_count - 1)) {
                                state.max_x = state.posision_x + state.repeat_count - 1;
                            }
                            if (state.max_y < (state.posision_y + i + n - 1)) {
                                state.max_y = state.posision_y + i + n - 1;
                            }

                            i += (n - 1);
                            sixel_vertical_mask <<= (n - 1);
                        }
                        sixel_vertical_mask <<= 1;
                    }
                    state.posision_x += state.repeat_count;
                }
            }
            state.repeat_count = 1;
        } else {
            p++;
        }
    }

    if (++state.max_x < state.attributed_ph) {
        state.max_x = state.attributed_ph;
    }
    if (++state.max_y < state.attributed_pv) {
        state.max_y = state.attributed_pv;
    }

    if (pbuffer.width > state.max_x || pbuffer.height > state.max_y) {
        status = image_buffer_resize(&pbuffer, state.max_x, state.max_y, state.bgindex, allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    *ncolors = state.max_color_index + 1;
    *palette = (unsigned char *)sixel_allocator_malloc(allocator, (size_t)(*ncolors * 3));
    if (palette == NULL) {
        sixel_allocator_free(allocator, pbuffer.data);
        sixel_helper_set_additional_message(
            "sixel_deocde_raw: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    for (n = 0; n < *ncolors; ++n) {
        (*palette)[n * 3 + 0] = state.palette[n] >> 16 & 0xff;
        (*palette)[n * 3 + 1] = state.palette[n] >> 8 & 0xff;
        (*palette)[n * 3 + 2] = state.palette[n] & 0xff;
    }

    *pwidth = pbuffer.width;
    *pheight = pbuffer.height;
    *pixels = pbuffer.data;

    status = SIXEL_OK;
end:
    sixel_allocator_ref(allocator);
    return status;
}


SIXELAPI SIXELSTATUS
sixel_decode(unsigned char              /* in */  *p,         /* sixel bytes */
             int                        /* in */  len,        /* size of sixel bytes */
             unsigned char              /* out */ **pixels,   /* decoded pixels */
             int                        /* out */ *pwidth,    /* image width */
             int                        /* out */ *pheight,   /* image height */
             unsigned char              /* out */ **palette,  /* ARGB palette */
             int                        /* out */ *ncolors,   /* palette size (<= 256) */
             sixel_allocator_function   /* in */  fn_malloc)  /* malloc function */
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_allocator_t *allocator = NULL;

    status = sixel_allocator_new(&allocator, fn_malloc, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_decode_raw(p, len, pixels, pwidth, pheight, palette, ncolors, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

end:
    sixel_allocator_unref(allocator);
    return status;
}

/* emacs, -*- Mode: C; tab-width: 4; indent-tabs-mode: nil -*- */
/* vim: set expandtab ts=4 : */
/* EOF */
