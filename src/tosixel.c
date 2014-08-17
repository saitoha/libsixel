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

#include "output.h"
#include "dither.h"
#include "image.h"
#include "sixel.h"

/* implementation */

static void
advance(sixel_output_t *context, int nwrite)
{
    if ((context->pos += nwrite) >= SIXEL_OUTPUT_PACKET_SIZE) {
        context->fn_write((char *)context->buffer, SIXEL_OUTPUT_PACKET_SIZE, context->priv);
        memcpy(context->buffer,
               context->buffer + SIXEL_OUTPUT_PACKET_SIZE,
               (context->pos -= SIXEL_OUTPUT_PACKET_SIZE));
    }
}


static int
PutFlash(sixel_output_t *const context)
{
    int n;
    int ret;
    char buf[256];
    int nwrite;

#if defined(USE_VT240)        /* VT240 Max 255 ? */
    while (context->save_count > 255) {
        nwrite = spritf((char *)context->buffer + context->pos, "!255%c", context->save_pixel);
        if (nwrite <= 0) {
            return (-1);
        }
        advance(context, nwrite);
        context->save_count -= 255;
    }
#endif  /* defined(USE_VT240) */

    if (context->save_count > 3) {
        /* DECGRI Graphics Repeat Introducer ! Pn Ch */
        nwrite = sprintf((char *)context->buffer + context->pos, "!%d%c", context->save_count, context->save_pixel);
        if (nwrite <= 0) {
            return (-1);
        }
        advance(context, nwrite);
    } else {
        for (n = 0; n < context->save_count; n++) {
            context->buffer[context->pos] = (char)context->save_pixel;
            advance(context, 1);
        }
    }

    context->save_pixel = 0;
    context->save_count = 0;

    return 0;
}


static void
PutPixel(sixel_output_t *const context, int pix)
{
    if (pix < 0 || pix > 63) {
        pix = 0;
    }

    pix += '?';

    if (pix == context->save_pixel) {
        context->save_count++;
    } else {
        PutFlash(context);
        context->save_pixel = pix;
        context->save_count = 1;
    }
}


static void
PutPalet(sixel_output_t *context, sixel_image_t *im, int pal)
{
    int nwrite;

    /* designate palette index */
    if (context->active_palette != pal) {
        nwrite = sprintf((char *)context->buffer + context->pos, "#%d", context->conv_palette[pal]);
        advance(context, nwrite);
        context->active_palette = pal;
    }
}


static void
NodeFree(sixel_output_t *const context)
{
    sixel_node_t *np;

    while ((np = context->node_free) != NULL) {
        context->node_free = np->next;
        free(np);
    }
}


static void
NodeDel(sixel_output_t *const context, sixel_node_t *np)
{
    sixel_node_t *tp;

    if ((tp = context->node_top) == np) {
        context->node_top = np->next;
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

    np->next = context->node_free;
    context->node_free = np;
}


static int
NodeAdd(sixel_output_t *const context, int pal, int sx, int mx, unsigned char *map)
{
    sixel_node_t *np, *tp, top;

    if ((np = context->node_free) != NULL) {
        context->node_free = np->next;
    } else if ((np = (sixel_node_t *)malloc(sizeof(sixel_node_t))) == NULL) {
        return (-1);
    }

    np->pal = pal;
    np->sx = sx;
    np->mx = mx;
    np->map = map;

    top.next = context->node_top;
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
    context->node_top = top.next;

    return 0;
}


static int
NodeLine(sixel_output_t *const context, int pal, int width, unsigned char *map)
{
    int sx, mx, n;
    int ret;

    for (sx = 0; sx < width; sx++) {
        if (map[sx] == 0) {
            continue;
        }

        for (mx = sx + 1; mx < width; mx++) {
            if (map[mx] != 0) {
                continue;
            }

            for (n = 1; (mx + n) < width; n++) {
                if (map[mx + n] != 0) {
                    break;
                }
            }

            if (n >= 10 || (mx + n) >= width) {
                break;
            }
            mx = mx + n - 1;
        }

        ret = NodeAdd(context, pal, sx, mx, map);
        if (ret != 0) {
            return ret;
        }
        sx = mx - 1;
    }

    return 0;
}


static int
PutNode(sixel_output_t *const context, sixel_image_t *im, int x,
        sixel_node_t *np, int ncolors, int keycolor)
{
    if (ncolors != 2 || keycolor == -1) {
        PutPalet(context, im, np->pal);
    }

    for (; x < np->sx; x++) {
        if (x != keycolor) {
            PutPixel(context, 0);
        }
    }

    for (; x < np->mx; x++) {
        if (x != keycolor) {
            PutPixel(context, np->map[x]);
        }
    }

    PutFlash(context);

    return x;
}


int
LibSixel_LSImageToSixel(sixel_image_t *im, sixel_output_t *context)
{
    int x, y, i, n, c;
    int maxPalet;
    int width, height;
    int len, pix, skip;
    int back = (-1);
    int ret;
    unsigned char *map;
    sixel_node_t *np;
    unsigned char list[SIXEL_PALETTE_MAX];
    char buf[256];
    int nwrite;

    width  = im->sx;
    height = im->sy;
    context->pos = 0;

    maxPalet = im->dither->ncolors;
    if (maxPalet < 1) {
        return (-1);
    }
    back = im->dither->keycolor;
    len = maxPalet * width;
    context->active_palette = (-1);

#if HAVE_CALLOC
    if ((map = (unsigned char *)calloc(len, sizeof(unsigned char))) == NULL) {
        return (-1);
    }
#else
    if ((map = (unsigned char *)malloc(len)) == NULL) {
        return (-1);
    }
    memset(map, 0, len);
#endif
    for (n = 0; n < maxPalet; n++) {
        context->conv_palette[n] = list[n] = n;
    }

    if (context->has_8bit_control) {
        nwrite = sprintf((char *)context->buffer, "\x90" "0;0;0" "q");
    } else {
        nwrite = sprintf((char *)context->buffer, "\x1bP" "0;0;0" "q");
    }
    if (nwrite <= 0) {
        return (-1);
    }
    advance(context, nwrite);
    nwrite = sprintf((char *)context->buffer + context->pos, "\"1;1;%d;%d", width, height);
    if (nwrite <= 0) {
        return (-1);
    }
    advance(context, nwrite);

    if (maxPalet != 2 || back == -1) {
        for (n = 0; n < maxPalet; n++) {
            /* DECGCI Graphics Color Introducer  # Pc ; Pu; Px; Py; Pz */
            nwrite = sprintf((char *)context->buffer + context->pos, "#%d;2;%d;%d;%d",
                             context->conv_palette[n],
                             (im->dither->palette[n * 3 + 0] * 100 + 127) / 255,
                             (im->dither->palette[n * 3 + 1] * 100 + 127) / 255,
                             (im->dither->palette[n * 3 + 2] * 100 + 127) / 255);
            if (nwrite <= 0) {
                return (-1);
            }
            advance(context, nwrite);
            if (nwrite <= 0) {
                return (-1);
            }
        }
        context->buffer[context->pos] = '\n';
        advance(context, 1);
    }

    for (y = i = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            pix = im->pixels[y * width + x];
            if (pix >= 0 && pix < maxPalet && pix != back) {
                map[pix * width + x] |= (1 << i);
            }
        }

        if (++i < 6 && (y + 1) < height) {
            continue;
        }

        for (n = 0; n < maxPalet; n++) {
            ret = NodeLine(context, n, width, map + n * width);
            if (ret != 0) {
                return ret;
            }
        }

        for (x = 0; (np = context->node_top) != NULL;) {
            if (x > np->sx) {
                /* DECGCR Graphics Carriage Return */
                context->buffer[context->pos] = '$';
                advance(context, 1);
                x = 0;
            }

            x = PutNode(context, im, x, np, maxPalet, back);
            NodeDel(context, np);
            np = context->node_top;

            while (np != NULL) {
                if (np->sx < x) {
                    np = np->next;
                    continue;
                }

                x = PutNode(context, im, x, np, maxPalet, back);
                NodeDel(context, np);
                np = context->node_top;
            }
        }

        /* DECGNL Graphics Next Line */
        context->buffer[context->pos] = '-';
        advance(context, 1);
        if (nwrite <= 0) {
            return (-1);
        }

        i = 0;
        memset(map, 0, len);
    }

    if (context->has_8bit_control) {
        context->buffer[context->pos] = '\x9c';
        advance(context, 1);
    } else {
        context->buffer[context->pos] = '\x1b';
        context->buffer[context->pos + 1] = '\\';
        advance(context, 2);
    }
    if (nwrite <= 0) {
        return (-1);
    }

    /* flush buffer */
    if (context->pos > 0) {
        context->fn_write((char *)context->buffer, context->pos, context->priv);
    }

    NodeFree(context);
    free(map);

    return 0;
}

int sixel_encode(unsigned char  /* in */ *pixels,   /* pixel bytes */
                 int            /* in */ width,     /* image width */
                 int            /* in */ height,    /* image height */
                 int            /* in */ depth,     /* pixel depth */
                 sixel_dither_t /* in */ *dither,   /* dither context */
                 sixel_output_t /* in */ *context)  /* output context */
{
    sixel_image_t *im;
    int ret;

    /* create intermidiate bitmap image */
    im = sixel_create_image(pixels, width, height, depth, 1, dither);
    if (!im) {
        return (-1);
    }

    /* apply palette */
    ret = sixel_apply_palette(im);
    if (ret != 0) {
        return ret;
    }

    LibSixel_LSImageToSixel(im, context);

    sixel_image_destroy(im);

    return 0;
}


/* emacs, -*- Mode: C; tab-width: 4; indent-tabs-mode: nil -*- */
/* vim: set expandtab ts=4 : */
/* EOF */
