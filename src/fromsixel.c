/*
 * this file is derived from "sixel" original version (2014-3-2)
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
 * Hayaki Saito <user@zuse.jp> modified this and re-licensed
 * it under the MIT license.
 *
 * The helper function hls2rgb is imported from Xterm pl#310.
 * This is originally written by Ross Combs.
 * Hayaki Saito <user@zuse.jp> slightly modified this.
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

#include "output.h"
#include "sixel.h"

#define RGB(r, g, b) (((r) << 16) + ((g) << 8) +  (b))

#define PALVAL(n,a,m) (((n) * (a) + ((m) / 2)) / (m))

#define XRGB(r,g,b) RGB(PALVAL(r, 255, 100), PALVAL(g, 255, 100), PALVAL(b, 255, 100))

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
sixel_getparams(unsigned char *p, int *param, int *len)
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
            if (*len < 10) {
                param[(*len)++] = n;
            }
            while (*p == ' ' || *p == '\t') {
                p++;
            }
            if (*p == ';') {
                p++;
            }
        } else if (*p == ';') {
            if (*len < 10) {
                param[(*len)++] = 0;
            }
            p++;
        } else
            break;
    }
    return p;
}


/* convert sixel data into indexed pixel bytes and palette data */
/* TODO: make "free" function as an argument */
int
sixel_decode(unsigned char              /* in */  *p,         /* sixel bytes */
             int                        /* in */  len,        /* size of sixel bytes */
             unsigned char              /* out */ **pixels,   /* decoded pixels */
             int                        /* out */ *pwidth,    /* image width */
             int                        /* out */ *pheight,   /* image height */
             unsigned char              /* out */ **palette,  /* ARGB palette */
             int                        /* out */ *ncolors,   /* palette size (<= 256) */
             sixel_allocator_function   /* out */ allocator)  /* malloc function */
{
    int n, i, r, g, b, sixel_vertical_mask, c;
    int posision_x, posision_y;
    int max_x, max_y;
    int attributed_pan, attributed_pad;
    int attributed_ph, attributed_pv;
    int repeat_count, color_index;
    int max_color_index;
    int background_color_index;
    int param[10];
    int sixel_palet[SIXEL_PALETTE_MAX];
    unsigned char *imbuf, *dmbuf;
    int imsx, imsy;
    int dmsx, dmsy;
    int y;

    (void) len;

    posision_x = posision_y = 0;
    max_x = max_y = 0;
    attributed_pan = 2;
    attributed_pad = 1;
    attributed_ph = attributed_pv = 0;
    repeat_count = 1;
    color_index = 15;
    max_color_index = 2;
    background_color_index = 0;

    imsx = 2048;
    imsy = 2048;
    imbuf = allocator(imsx * imsy);

    if (imbuf == NULL) {
        return (-1);
    }

    for (n = 0; n < 16; n++) {
        sixel_palet[n] = color_table[n];
    }

    /* colors 16-231 are a 6x6x6 color cube */
    for (r = 0; r < 6; r++) {
        for (g = 0; g < 6; g++) {
            for (b = 0; b < 6; b++) {
                sixel_palet[n++] = RGB(r * 51, g * 51, b * 51);
            }
        }
    }
    /* colors 232-255 are a grayscale ramp, intentionally leaving out */
    for (i = 0; i < 24; i++) {
        sixel_palet[n++] = RGB(i * 11, i * 11, i * 11);
    }

    for (; n < SIXEL_PALETTE_MAX; n++) {
        sixel_palet[n] = RGB(255, 255, 255);
    }

    memset(imbuf, background_color_index, imsx * imsy);

    while (*p != '\0') {
        if ((p[0] == '\033' && p[1] == 'P') || *p == 0x90) {
            if (*p == '\033') {
                p++;
            }

            p = sixel_getparams(++p, param, &n);
            if (*p == 'q') {
                p++;
                if (n > 0) {        /* Pn1 */
                    switch(param[0]) {
                    case 0:
                    case 1:
                        attributed_pad = 2;
                        break;
                    case 2:
                        attributed_pad = 5;
                        break;
                    case 3:
                        attributed_pad = 4;
                        break;
                    case 4:
                        attributed_pad = 4;
                        break;
                    case 5:
                        attributed_pad = 3;
                        break;
                    case 6:
                        attributed_pad = 3;
                        break;
                    case 7:
                        attributed_pad = 2;
                        break;
                    case 8:
                        attributed_pad = 2;
                        break;
                    case 9:
                        attributed_pad = 1;
                        break;
                    }
                }

                if (n > 2) {        /* Pn3 */
                    if (param[2] == 0) {
                        param[2] = 10;
                    }
                    attributed_pan = attributed_pan * param[2] / 10;
                    attributed_pad = attributed_pad * param[2] / 10;
                    if (attributed_pan <= 0) {
                        attributed_pan = 1;
                    }
                    if (attributed_pad <= 0) {
                        attributed_pad = 1;
                    }
                }
            }

        } else if ((p[0] == '\033' && p[1] == '\\') || *p == 0x9C) {
            break;
        } else if (*p == '"') {
            /* DECGRA Set Raster Attributes " Pan; Pad; Ph; Pv */
            p = sixel_getparams(p + 1, param, &n);

            if (n > 0) {
                attributed_pad = param[0];
            }
            if (n > 1) {
                attributed_pan = param[1];
            }
            if (n > 2 && param[2] > 0) {
                attributed_ph = param[2];
            }
            if (n > 3 && param[3] > 0) {
                attributed_pv = param[3];
            }

            if (attributed_pan <= 0) {
                attributed_pan = 1;
            }
            if (attributed_pad <= 0) {
                attributed_pad = 1;
            }

            if (imsx < attributed_ph || imsy < attributed_pv) {
                dmsx = imsx > attributed_ph ? imsx : attributed_ph;
                dmsy = imsy > attributed_pv ? imsy : attributed_pv;
                dmbuf = allocator(dmsx * dmsy);
                if (dmbuf == NULL) {
                    free(imbuf);
                    return (-1);
                }
                memset(dmbuf, background_color_index, dmsx * dmsy);
                for (y = 0; y < imsy; ++y) {
                    memcpy(dmbuf + dmsx * y, imbuf + imsx * y, imsx);
                }
                free(imbuf);
                imsx = dmsx;
                imsy = dmsy;
                imbuf = dmbuf;
            }

        } else if (*p == '!') {
            /* DECGRI Graphics Repeat Introducer ! Pn Ch */
            p = sixel_getparams(p + 1, param, &n);

            if (n > 0) {
                repeat_count = param[0];
            }

        } else if (*p == '#') {
            /* DECGCI Graphics Color Introducer # Pc; Pu; Px; Py; Pz */
            p = sixel_getparams(++p, param, &n);

            if (n > 0) {
                if ((color_index = param[0]) < 0) {
                    color_index = 0;
                } else if (color_index >= SIXEL_PALETTE_MAX) {
                    color_index = SIXEL_PALETTE_MAX - 1;
                }
            }

            if (n > 4) {
                if (param[1] == 1) {            /* HLS */
                    if (param[2] > 360) param[2] = 360;
                    if (param[3] > 100) param[3] = 100;
                    if (param[4] > 100) param[4] = 100;
                    sixel_palet[color_index] = hls2rgb(param[2], param[3], param[4]);
                } else if (param[1] == 2) {    /* RGB */
                    if (param[2] > 100) param[2] = 100;
                    if (param[3] > 100) param[3] = 100;
                    if (param[4] > 100) param[4] = 100;
                    sixel_palet[color_index] = XRGB(param[2], param[3], param[4]);
                }
            }

        } else if (*p == '$') {
            /* DECGCR Graphics Carriage Return */
            p++;
            posision_x = 0;
            repeat_count = 1;

        } else if (*p == '-') {
            /* DECGNL Graphics Next Line */
            p++;
            posision_x = 0;
            posision_y += 6;
            repeat_count = 1;

        } else if (*p >= '?' && *p <= '\177') {
            if (imsx < (posision_x + repeat_count) || imsy < (posision_y + 6)) {
                int nx = imsx * 2;
                int ny = imsy * 2;

                while (nx < (posision_x + repeat_count) || ny < (posision_y + 6)) {
                    nx *= 2;
                    ny *= 2;
                }

                dmsx = nx;
                dmsy = ny;
                dmbuf = allocator(dmsx * dmsy);
                if (dmbuf == NULL) {
                    free(imbuf);
                    return (-1);
                }
                memset(dmbuf, background_color_index, dmsx * dmsy);
                for (y = 0; y < imsy; ++y) {
                    memcpy(dmbuf + dmsx * y, imbuf + imsx * y, imsx);
                }
                free(imbuf);
                imsx = dmsx;
                imsy = dmsy;
                imbuf = dmbuf;
            }

            if (color_index > max_color_index) {
                max_color_index = color_index;
            }
            if ((b = *(p++) - '?') == 0) {
                posision_x += repeat_count;

            } else {
                sixel_vertical_mask = 0x01;

                if (repeat_count <= 1) {
                    for (i = 0; i < 6; i++) {
                        if ((b & sixel_vertical_mask) != 0) {
                            imbuf[imsx * (posision_y + i) + posision_x] = color_index;
                            if (max_x < posision_x) {
                                max_x = posision_x;
                            }
                            if (max_y < (posision_y + i)) {
                                max_y = posision_y + i;
                            }
                        }
                        sixel_vertical_mask <<= 1;
                    }
                    posision_x += 1;

                } else { /* repeat_count > 1 */
                    for (i = 0; i < 6; i++) {
                        if ((b & sixel_vertical_mask) != 0) {
                            c = sixel_vertical_mask << 1;
                            for (n = 1; (i + n) < 6; n++) {
                                if ((b & c) == 0) {
                                    break;
                                }
                                c <<= 1;
                            }
                            for (y = posision_y + i; y < posision_y + i + n; ++y) {
                                memset(imbuf + imsx * y + posision_x, color_index, repeat_count);
                            }
                            if (max_x < (posision_x + repeat_count - 1)) {
                                max_x = posision_x + repeat_count - 1;
                            }
                            if (max_y < (posision_y + i + n - 1)) {
                                max_y = posision_y + i + n - 1;
                            }

                            i += (n - 1);
                            sixel_vertical_mask <<= (n - 1);
                        }
                        sixel_vertical_mask <<= 1;
                    }
                    posision_x += repeat_count;
                }
            }
            repeat_count = 1;
        } else {
            p++;
        }
    }

    if (++max_x < attributed_ph) {
        max_x = attributed_ph;
    }
    if (++max_y < attributed_pv) {
        max_y = attributed_pv;
    }

    if (imsx > max_x || imsy > max_y) {
        dmsx = max_x;
        dmsy = max_y;
        if ((dmbuf = allocator(dmsx * dmsy)) == NULL) {
            free(imbuf);
            return (-1);
        }
        for (y = 0; y < dmsy; ++y) {
            memcpy(dmbuf + dmsx * y, imbuf + imsx * y, dmsx);
        }
        free(imbuf);
        imsx = dmsx;
        imsy = dmsy;
        imbuf = dmbuf;
    }

    *pixels = imbuf;
    *pwidth = imsx;
    *pheight = imsy;
    *ncolors = max_color_index + 1;
    *palette = allocator(*ncolors * 4);
    for (n = 0; n < *ncolors; ++n) {
        (*palette)[n * 4 + 0] = sixel_palet[n] >> 16 & 0xff;
        (*palette)[n * 4 + 1] = sixel_palet[n] >> 8 & 0xff;
        (*palette)[n * 4 + 2] = sixel_palet[n] & 0xff;
        (*palette)[n * 4 + 3] = 0xff;
    }
    return 0;
}

/* emacs, -*- Mode: C; tab-width: 4; indent-tabs-mode: nil -*- */
/* vim: set expandtab ts=4 : */
/* EOF */
