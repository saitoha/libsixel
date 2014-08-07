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
 * Hayaki Saito (user@zuse.jp) modified this and re-licensed
 * it under the MIT license.
 *
 */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(HAVE_INTTYPES_H)
# include <inttypes.h>
#endif

#include "sixel.h"

/* exported function */
extern int
LibSixel_LSImageToSixel(LSImagePtr im,
                        LSOutputContextPtr context);

/* implementation */

typedef struct _SixNode {
    struct _SixNode *next;
    int pal;
    int sx;
    int mx;
    unsigned char *map;
} SixNode;

static SixNode *node_top = NULL;
static SixNode *node_free = NULL;

static int save_pix = 0;
static int save_count = 0;
static int act_palet = (-1);

static long use_palet[PALETTE_MAX];
static unsigned char conv_palet[PALETTE_MAX];


static int
PutFlash(LSOutputContextPtr const context)
{
    int n;
    int ret;

#if defined(USE_VT240)        /* VT240 Max 255 ? */
    while (save_count > 255) {
        ret = context->fn_printf("!%d%c", 255, save_pix);
        if (ret <= 0) {
            return (-1);
        }
        save_count -= 255;
    }
#endif  /* defined(USE_VT240) */

    if (save_count > 3) {
        /* DECGRI Graphics Repeat Introducer ! Pn Ch */

        ret = context->fn_printf("!%d%c", save_count, save_pix);
        if (ret <= 0) {
            return (-1);
        }

    } else {
        for (n = 0 ; n < save_count ; n++) {
            ret = context->fn_putchar(save_pix);
            if (ret <= 0) {
                return (-1);
            }
        }
    }

    save_pix = 0;
    save_count = 0;

    return 0;
}


static void
PutPixel(LSOutputContextPtr const context, int pix)
{
    if (pix < 0 || pix > 63) {
        pix = 0;
    }

    pix += '?';

    if (pix == save_pix) {
        save_count++;
    } else {
        PutFlash(context);
        save_pix = pix;
        save_count = 1;
    }
}


static void
PutPalet(LSOutputContextPtr context,
         LSImagePtr im,
         int pal)
{
    /* designate palette index */
    if (act_palet != pal) {
        context->fn_printf("#%d", conv_palet[pal]);
        act_palet = pal;
    }
}


static int
PutCr(LSOutputContextPtr context)
{
    int ret;

    /* DECGCR Graphics Carriage Return */
    /* x = 0; */
    ret = context->fn_putchar('$');
    if (ret <= 0) {
        return (-1);
    }
    return 0;
}


static int
PutLf(LSOutputContextPtr context)
{
    int ret;

    /* DECGNL Graphics Next Line */
    /* x = 0; */
    /* y += 6; */
    ret = context->fn_putchar('-');
    if (ret <= 0) {
        return (-1);
    }
    return 0;
}


static void
NodeFree()
{
    SixNode *np;

    while ((np = node_free) != NULL) {
        node_free = np->next;
        free(np);
    }
}


static void
NodeDel(SixNode *np)
{
    SixNode *tp;

    if ((tp = node_top) == np) {
        node_top = np->next;
    }

    else {
        while (tp->next != NULL) {
            if (tp->next == np) {
                tp->next = np->next;
                break;
            }
            tp = tp->next;
        }
    }

    np->next = node_free;
    node_free = np;
}


static int
NodeAdd(int pal, int sx, int mx, unsigned char *map)
{
    SixNode *np, *tp, top;

    if ((np = node_free) != NULL) {
        node_free = np->next;
    } else if ((np = (SixNode *)malloc(sizeof(SixNode))) == NULL) {
        return (-1);
    }

    np->pal = pal;
    np->sx = sx;
    np->mx = mx;
    np->map = map;

    top.next = node_top;
    tp = &top;

    while (tp->next != NULL) {
        if (np->sx < tp->next->sx) {
            break;
        } else if (np->sx == tp->next->sx && np->mx > tp->next->mx) {
            break;
        }
        tp = tp->next;
    }

    np->next = tp->next;
    tp->next = np;
    node_top = top.next;

    return 0;
}


static int
NodeLine(int pal, int width, unsigned char *map)
{
    int sx, mx, n;
    int ret;

    for (sx = 0 ; sx < width ; sx++) {
        if (map[sx] == 0) {
            continue;
        }

        for (mx = sx + 1 ; mx < width ; mx++) {
            if (map[mx] != 0) {
                continue;
            }

            for (n = 1 ; (mx + n) < width ; n++) {
                if (map[mx + n] != 0) {
                    break;
                }
            }

            if (n >= 10 || (mx + n) >= width) {
                break;
            }
            mx = mx + n - 1;
        }

        ret = NodeAdd(pal, sx, mx, map);
        if (ret != 0) {
            return ret;
        }
        sx = mx - 1;
    }

    return 0;
}


static int
PutNode(LSOutputContextPtr context, LSImagePtr im, int x, SixNode *np)
{
    if (im->ncolors != 2 || im->keycolor == -1) {
        PutPalet(context, im, np->pal);
    }

    for (; x < np->sx ; x++) {
        if (x != im->keycolor) {
            PutPixel(context, 0);
        }
    }

    for (; x < np->mx ; x++) {
        if (x != im->keycolor) {
            PutPixel(context, np->map[x]);
        }
    }

    PutFlash(context);

    return x;
}


int
LibSixel_LSImageToSixel(LSImagePtr im, LSOutputContextPtr context)
{
    int x, y, i, n, c;
    int maxPalet;
    int width, height;
    int len, pix, skip;
    int back = (-1);
    int ret;
    unsigned char *map;
    SixNode *np;
    unsigned char list[PALETTE_MAX];

    width  = im->sx;
    height = im->sy;

    maxPalet = im->ncolors;
    back = im->keycolor;
    len = maxPalet * width;

    if ((map = (unsigned char *)malloc(len)) == NULL) {
        return (-1);
    }

    memset(map, 0, len);

    for (n = 0 ; n < maxPalet ; n++) {
        conv_palet[n] = list[n] = n;
    }

    if (context->has_8bit_control) {
        ret = context->fn_printf("\x90" "0;0;0" "q");
    } else {
        ret = context->fn_printf("\x1bP" "0;0;0" "q");
    }
    if (ret <= 0) {
        return (-1);
    }
    ret = context->fn_printf("\"1;1;%d;%d", width, height);
    if (ret <= 0) {
        return (-1);
    }

    if (im->ncolors != 2 || im->keycolor == -1) {
        for (n = 0 ; n < maxPalet ; n++) {
            /* DECGCI Graphics Color Introducer  # Pc ; Pu; Px; Py; Pz */
            ret = context->fn_printf("#%d;2;%d;%d;%d",
                                     conv_palet[n],
                                     (im->red[n] * 100 + 127) / 255,
                                     (im->green[n] * 100 + 127) / 255,
                                     (im->blue[n] * 100 + 127) / 255);
            if (ret <= 0) {
                return (-1);
            }
        }
    }

    for (y = i = 0 ; y < height ; y++) {
        for (x = 0 ; x < width ; x++) {
            pix = im->pixels[y * width + x];
            if (pix >= 0 && pix < maxPalet && pix != back) {
                map[pix * width + x] |= (1 << i);
            }
        }

        if (++i < 6 && (y + 1) < height) {
            continue;
        }

        for (n = 0 ; n < maxPalet ; n++) {
            ret = NodeLine(n, width, map + n * width);
            if (ret != 0) {
                return ret;
            }
        }

        for (x = 0 ; (np = node_top) != NULL ;) {
            if (x > np->sx) {
                ret = PutCr(context);
                if (ret != 0) {
                    return (-1);
                }
                x = 0;
            }

            x = PutNode(context, im, x, np);
            NodeDel(np);
            np = node_top;

            while (np != NULL) {
                if (np->sx < x) {
                    np = np->next;
                    continue;
                }

                x = PutNode(context, im, x, np);
                NodeDel(np);
                np = node_top;
            }
        }

        ret = PutLf(context);
        if (ret != 0) {
            return (-1);
        }

        i = 0;
        memset(map, 0, len);
    }

    if (context->has_8bit_control) {
        ret = context->fn_printf("\x9c");
    } else {
        ret = context->fn_printf("\x1b\\");
    }
    if (ret <= 0) {
        return (-1);
    }

    NodeFree();
    free(map);

    return 0;
}

/* emacs, -*- Mode: C; tab-width: 4; indent-tabs-mode: nil -*- */
/* vim: set expandtab ts=4 : */
/* EOF */
