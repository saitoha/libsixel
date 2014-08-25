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

#define RGBA(r, g, b, a) (((a) << 24) + ((r) << 16) + ((g) << 8) +  (b))

#define PALVAL(n,a,m) (((n) * (a) + ((m) / 2)) / (m))

#define XRGB(r,g,b) RGB(PALVAL(r, 255, 100), PALVAL(g, 255, 100), PALVAL(b, 255, 100))

static int const ColTab[] = {
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


static int
HueToRGB(int n1, int n2, int hue)
{
    const int HLSMAX = 100;

    if (hue < 0) {
        hue += HLSMAX;
    }

    if (hue > HLSMAX) {
        hue -= HLSMAX;
    }

    if (hue < (HLSMAX / 6)) {
        return (n1 + (((n2 - n1) * hue + (HLSMAX / 12)) / (HLSMAX / 6)));
    }
    if (hue < (HLSMAX / 2)) {
        return (n2);
    }
    if (hue < ((HLSMAX * 2) / 3)) {
        return (n1 + (((n2 - n1) * (((HLSMAX * 2) / 3) - hue) + (HLSMAX / 12))/(HLSMAX / 6)));
    }
    return (n1);
}


static int
HLStoRGB(int hue, int lum, int sat)
{
    int R, G, B;
    int Magic1, Magic2;
    const int RGBMAX = 255;
    const int HLSMAX = 100;

    if (sat == 0) {
        R = G = B = (lum * RGBMAX) / HLSMAX;
    } else {
        if (lum <= (HLSMAX / 2)) {
            Magic2 = (lum * (HLSMAX + sat) + (HLSMAX / 2)) / HLSMAX;
        } else {
            Magic2 = lum + sat - ((lum * sat) + (HLSMAX / 2)) / HLSMAX;
        }
        Magic1 = 2 * lum - Magic2;

        R = (HueToRGB(Magic1, Magic2, hue + (HLSMAX / 3)) * RGBMAX + (HLSMAX / 2)) / HLSMAX;
        G = (HueToRGB(Magic1, Magic2, hue) * RGBMAX + (HLSMAX / 2)) / HLSMAX;
        B = (HueToRGB(Magic1, Magic2, hue - (HLSMAX / 3)) * RGBMAX + (HLSMAX/2)) / HLSMAX;
    }
    return RGB(R, G, B);
}


static unsigned char *
GetParam(unsigned char *p, int *param, int *len)
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
    int repeat_count, color_index, max_color_index = 2, background_color_index;
    int param[10];
    unsigned char *s;
    static char pam[256];
    static char gra[256];
    int sixel_palet[SIXEL_PALETTE_MAX];
    unsigned char *imbuf, *dmbuf;
    int imsx, imsy;
    int dmsx, dmsy;
    int y;

    posision_x = posision_y = 0;
    max_x = max_y = 0;
    attributed_pan = 2;
    attributed_pad = 1;
    attributed_ph = attributed_pv = 0;
    repeat_count = 1;
    color_index = 0;
    background_color_index = 0;

    imsx = 2048;
    imsy = 2048;
    imbuf = allocator(imsx * imsy);

    if (imbuf == NULL) {
        return (-1);
    }

    for (n = 0; n < 16; n++) {
        sixel_palet[n] = ColTab[n];
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

    pam[0] = gra[0] = '\0';

    while (*p != '\0') {
        if ((p[0] == '\033' && p[1] == 'P') || *p == 0x90) {
            if (*p == '\033') {
                p++;
            }

            s = ++p;
            p = GetParam(p, param, &n);
            if (s < p) {
                for (i = 0; i < 255 && s < p;) {
                    pam[i++] = *(s++);
                }
                pam[i] = '\0';
            }

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
                    if (attributed_pan <= 0) attributed_pan = 1;
                    if (attributed_pad <= 0) attributed_pad = 1;
                }
            }

        } else if ((p[0] == '\033' && p[1] == '\\') || *p == 0x9C) {
            break;
        } else if (*p == '"') {
            /* DECGRA Set Raster Attributes " Pan; Pad; Ph; Pv */
            s = p++;
            p = GetParam(p, param, &n);
            if (s < p) {
                for (i = 0; i < 255 && s < p;) {
                    gra[i++] = *(s++);
                }
                gra[i] = '\0';
            }

            if (n > 0) attributed_pad = param[0];
            if (n > 1) attributed_pan = param[1];
            if (n > 2 && param[2] > 0) attributed_ph = param[2];
            if (n > 3 && param[3] > 0) attributed_pv = param[3];

            if (attributed_pan <= 0) attributed_pan = 1;
            if (attributed_pad <= 0) attributed_pad = 1;

            if (imsx < attributed_ph || imsy < attributed_pv) {
                dmsx = imsx > attributed_ph ? imsx : attributed_ph;
                dmsy = imsy > attributed_pv ? imsy : attributed_pv;
                dmbuf = allocator(dmsx * dmsy);
                if (dmbuf == NULL) {
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
            p = GetParam(++p, param, &n);

            if (n > 0) {
                repeat_count = param[0];
            }

        } else if (*p == '#') {
            /* DECGCI Graphics Color Introducer # Pc; Pu; Px; Py; Pz */
            p = GetParam(++p, param, &n);

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
                    sixel_palet[color_index] = HLStoRGB(param[2] * 100 / 360, param[3], param[4]);
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
            posision_x  = 0;
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
                if ((dmbuf = allocator(dmsx * dmsy)) == NULL) {
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
