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

#if defined(HAVE_INTTYPES_H)
# include <inttypes.h>
#endif

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


LSImagePtr
LibSixel_SixelToLSImage(unsigned char *p, int len)
{
    int n, i, a, b, c;
    int px, py;
    int mx, my;
    int ax, ay;
    int tx, ty;
    int rep, col, bc, id;
    int param[10];
    LSImagePtr im, dm;
    unsigned char *s;
    static char pam[256];
    static char gra[256];
    int sixel_palet[PALETTE_MAX];

    px = py = 0;
    mx = my = 0;
    ax = 2;
    ay = 1;
    tx = ty = 0;
    rep = 1;
    col = 0;
    bc = 0;

    if ((im = LSImage_create(2048, 2048, 3, -1)) == NULL) {
        return NULL;
    }

    for (n = 0; n < 16; n++) {
        sixel_palet[n] = ColTab[n];
    }

    /* colors 16-231 are a 6x6x6 color cube */
    for (a = 0; a < 6; a++) {
        for (b = 0; b < 6; b++) {
            for (c = 0; c < 6; c++) {
                sixel_palet[n++] = RGB(a * 51, b * 51, c * 51);
            }
        }
    }
    /* colors 232-255 are a grayscale ramp, intentionally leaving out */
    for (a = 0; a < 24; a++) {
        sixel_palet[n++] = RGB(a * 11, a * 11, a * 11);
    }

    bc = RGBA(0, 0, 0, 127);

    for (; n < PALETTE_MAX; n++) {
        sixel_palet[n] = RGB(255, 255, 255);
    }

    LSImage_fill(im, bc);

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
                        ay = 2;
                        break;
                    case 2:
                        ay = 5;
                        break;
                    case 3:
                        ay = 4;
                        break;
                    case 4:
                        ay = 4;
                        break;
                    case 5:
                        ay = 3;
                        break;
                    case 6:
                        ay = 3;
                        break;
                    case 7:
                        ay = 2;
                        break;
                    case 8:
                        ay = 2;
                        break;
                    case 9:
                        ay = 1;
                        break;
                    }
                }

                if (n > 2) {        /* Pn3 */
                    if (param[2] == 0) {
                        param[2] = 10;
                    }
                    ax = ax * param[2] / 10;
                    ay = ay * param[2] / 10;
                    if (ax <= 0) ax = 1;
                    if (ay <= 0) ay = 1;
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

            if (n > 0) ay = param[0];
            if (n > 1) ax = param[1];
            if (n > 2 && param[2] > 0) tx = param[2];
            if (n > 3 && param[3] > 0) ty = param[3];

            if (ax <= 0) ax = 1;
            if (ay <= 0) ay = 1;

            if (im->sx < tx || im->sy < ty) {
                dm = LSImage_create(im->sx > tx ? im->sx : tx,
                                    im->sy > ty ? im->sy : ty, 3, -1);
                if (dm == NULL) {
                    return NULL;
                }
                LSImage_fill(dm, bc);
                LSImage_copy(dm, im, im->sx, im->sy);
                LSImage_destroy(im);
                im = dm;
            }

        } else if (*p == '!') {
            /* DECGRI Graphics Repeat Introducer ! Pn Ch */
            p = GetParam(++p, param, &n);

            if (n > 0) {
                rep = param[0];
            }

        } else if (*p == '#') {
            /* DECGCI Graphics Color Introducer # Pc; Pu; Px; Py; Pz */
            p = GetParam(++p, param, &n);

            if (n > 0) {
                if ((col = param[0]) < 0) {
                    col = 0;
                } else if (col >= PALETTE_MAX) {
                    col = PALETTE_MAX - 1;
                }
            }

            if (n > 4) {
                if (param[1] == 1) {            /* HLS */
                    if (param[2] > 360) param[2] = 360;
                    if (param[3] > 100) param[3] = 100;
                    if (param[4] > 100) param[4] = 100;
                    sixel_palet[col] = HLStoRGB(param[2] * 100 / 360, param[3], param[4]);
                } else if (param[1] == 2) {    /* RGB */
                    if (param[2] > 100) param[2] = 100;
                    if (param[3] > 100) param[3] = 100;
                    if (param[4] > 100) param[4] = 100;
                    sixel_palet[col] = XRGB(param[2], param[3], param[4]);
                }
            }

        } else if (*p == '$') {
            /* DECGCR Graphics Carriage Return */
            p++;
            px = 0;
            rep = 1;

        } else if (*p == '-') {
            /* DECGNL Graphics Next Line */
            p++;
            px  = 0;
            py += 6;
            rep = 1;

        } else if (*p >= '?' && *p <= '\177') {
            if (im->sx < (px + rep) || im->sy < (py + 6)) {
                int nx = im->sx * 2;
                int ny = im->sy * 2;

                while (nx < (px + rep) || ny < (py + 6)) {
                    nx *= 2;
                    ny *= 2;
                }

                if ((dm = LSImage_create(nx, ny, 3, -1)) == NULL) {
                    return NULL;
                }
                LSImage_fill(dm, bc);
                LSImage_copy(dm, im, im->sx, im->sy);
                LSImage_destroy(im);
                im = dm;
            }

            if ((b = *(p++) - '?') == 0) {
                px += rep;

            } else {
                a = 0x01;

                if (rep <= 1) {
                    for (i = 0; i < 6; i++) {
                        if ((b & a) != 0) {
                            LSImage_setpixel(im, px, py + i, sixel_palet[col]);
                            if (mx < px) {
                                mx = px;
                            }
                            if (my < (py + i)) {
                                my = py + i;
                            }
                        }
                        a <<= 1;
                    }
                    px += 1;

                } else { /* rep > 1 */
                    for (i = 0; i < 6; i++) {
                        if ((b & a) != 0) {
                            c = a << 1;
                            for (n = 1; (i + n) < 6; n++) {
                                if ((b & c) == 0) {
                                    break;
                                }
                                c <<= 1;
                            }
                            LSImage_fillrectangle(im, px, py + i,
                                                  px + rep - 1,
                                                  py + i + n - 1,
                                                  sixel_palet[col]);

                            if (mx < (px + rep - 1)) {
                                mx = px + rep - 1;
                            }
                            if (my < (py + i + n - 1)) {
                                my = py + i + n - 1;
                            }

                            i += (n - 1);
                            a <<= (n - 1);
                        }
                        a <<= 1;
                    }
                    px += rep;
                }
            }
            rep = 1;

        } else {
            p++;
        }
    }

    if (++mx < tx) {
        mx = tx;
    }
    if (++my < ty) {
        my = ty;
    }

    if (im->sx > mx || im->sy > my) {
        if ((dm = LSImage_create(mx, my, 3, -1)) == NULL) {
            return NULL;
        }
        LSImage_copy(dm, im, dm->sx, dm->sy);
        LSImage_destroy(im);
        im = dm;
    }

    return im;
}

/* emacs, -*- Mode: C; tab-width: 4; indent-tabs-mode: nil -*- */
/* vim: set expandtab ts=4 : */
/* EOF */
