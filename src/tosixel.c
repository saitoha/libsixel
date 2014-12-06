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
#include "sixel.h"

/* implementation */

static void
penetrate(sixel_output_t *context, int nwrite)
{
    int pos;
    for (pos = 0; pos < nwrite; pos += 508) {
        context->fn_write("\x1bP", 2, context->priv);
        context->fn_write(((char *)context->buffer)+pos,
                nwrite - pos < 508 ? nwrite - pos : 508, context->priv);
        context->fn_write("\x1b\\", 2, context->priv);
    }
}


static void
sixel_advance(sixel_output_t *context, int nwrite)
{
    if ((context->pos += nwrite) >= SIXEL_OUTPUT_PACKET_SIZE) {
        if (context->penetrate_multiplexer) {
            penetrate(context, SIXEL_OUTPUT_PACKET_SIZE);
        } else {
            context->fn_write((char *)context->buffer,
                              SIXEL_OUTPUT_PACKET_SIZE, context->priv);
        }
        memcpy(context->buffer,
               context->buffer + SIXEL_OUTPUT_PACKET_SIZE,
               (context->pos -= SIXEL_OUTPUT_PACKET_SIZE));
    }
}


static int
sixel_put_flash(sixel_output_t *const context)
{
    int n;
    int nwrite;

#if defined(USE_VT240)        /* VT240 Max 255 ? */
    while (context->save_count > 255) {
        nwrite = spritf((char *)context->buffer + context->pos,
                        "!255%c", context->save_pixel);
        if (nwrite <= 0) {
            return (-1);
        }
        sixel_advance(context, nwrite);
        context->save_count -= 255;
    }
#endif  /* defined(USE_VT240) */

    if (context->save_count > 3) {
        /* DECGRI Graphics Repeat Introducer ! Pn Ch */
        nwrite = sprintf((char *)context->buffer + context->pos,
                         "!%d%c", context->save_count, context->save_pixel);
        if (nwrite <= 0) {
            return (-1);
        }
        sixel_advance(context, nwrite);
    } else {
        for (n = 0; n < context->save_count; n++) {
            context->buffer[context->pos] = (char)context->save_pixel;
            sixel_advance(context, 1);
        }
    }

    context->save_pixel = 0;
    context->save_count = 0;

    return 0;
}


static void
sixel_put_pixel(sixel_output_t *const context, int pix)
{
    if (pix < 0 || pix > '?') {
        pix = 0;
    }

    pix += '?';

    if (pix == context->save_pixel) {
        context->save_count++;
    } else {
        sixel_put_flash(context);
        context->save_pixel = pix;
        context->save_count = 1;
    }
}


static void
sixel_node_del(sixel_output_t *const context, sixel_node_t *np)
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
sixel_put_node(sixel_output_t *const context, int x,
        sixel_node_t *np, int ncolors, int keycolor)
{
    int nwrite;

    if (ncolors != 2 || keycolor == -1) {
        /* designate palette index */
        if (context->active_palette != np->pal) {
            nwrite = sprintf((char *)context->buffer + context->pos,
                             "#%d", np->pal);
            sixel_advance(context, nwrite);
            context->active_palette = np->pal;
        }
    }

    for (; x < np->sx; x++) {
        if (x != keycolor) {
            sixel_put_pixel(context, 0);
        }
    }

    for (; x < np->mx; x++) {
        if (x != keycolor) {
            sixel_put_pixel(context, np->map[x]);
        }
    }

    sixel_put_flash(context);

    return x;
}

enum {
    PALETTE_HIT = 1,
    PALETTE_CHANGE = 2,
};


static int
sixel_encode_header(int width, int height, sixel_output_t *context)
{
    int nwrite;
    int p[3] = {0, 0, 0};
    int pcount = 3;
    int use_raster_attributes = 1;

    context->pos = 0;

    if (!context->skip_dcs_envelope) {
        if (context->has_8bit_control) {
            nwrite = sprintf((char *)context->buffer, "\x90");
        } else {
            nwrite = sprintf((char *)context->buffer, "\x1bP");
        }
        if (nwrite <= 0) {
            return (-1);
        }
        sixel_advance(context, nwrite);
    }

    if (p[2] == 0) {
        pcount--;
        if (p[1] == 0) {
            pcount--;
            if (p[0] == 0) {
                pcount--;
            }
        }
    }

    if (pcount > 0) {
        nwrite = sprintf((char *)context->buffer + context->pos, "%d", p[0]);
        if (nwrite <= 0) {
            return (-1);
        }
        sixel_advance(context, nwrite);
        if (pcount > 1) {
            nwrite = sprintf((char *)context->buffer + context->pos, ";%d", p[1]);
            if (nwrite <= 0) {
                return (-1);
            }
            sixel_advance(context, nwrite);
            if (pcount > 2) {
                nwrite = sprintf((char *)context->buffer + context->pos, ";%d", p[2]);
                if (nwrite <= 0) {
                    return (-1);
                }
                sixel_advance(context, nwrite);
            }
        }
    }

    nwrite = sprintf((char *)context->buffer + context->pos, "q");
    if (nwrite <= 0) {
        return (-1);
    }
    sixel_advance(context, nwrite);

    if (use_raster_attributes) {
        nwrite = sprintf((char *)context->buffer + context->pos, "\"1;1;%d;%d", width, height);
        if (nwrite <= 0) {
            return (-1);
        }
        sixel_advance(context, nwrite);
    }

    return 0;
}


static int
sixel_encode_body(unsigned char *pixels, int width, int height,
                  unsigned char *palette, int ncolors, int keycolor, int bodyonly,
                  sixel_output_t *context, unsigned char *palstate)
{
    int x, y, i, n, c;
    int sx, mx;
    int len, pix;
    unsigned char *map;
    sixel_node_t *np, *tp, top;
    int nwrite;

    if (ncolors < 1) {
        return (-1);
    }
    len = ncolors * width;
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

    if (!bodyonly && (ncolors != 2 || keycolor == -1)) {
        if (context->palette_type == PALETTETYPE_HLS) {
            for (n = 0; n < ncolors; n++) {
                int h;
                int l;
                int s;
                int r, g, b, max, min;
                if (palstate && palstate[n] != PALETTE_CHANGE) {
                    continue;
                }
                r = palette[n * 3 + 0];
                g = palette[n * 3 + 1];
                b = palette[n * 3 + 2];
                max = r > g ? (r > b ? r: b): (g > b ? g: b);
                min = r < g ? (r < b ? r: b): (g < b ? g: b);
                l = ((max + min) * 100 + 255) / 510;
                if (max == min) {
                    h = s = 0;
                } else {
                    if (l < 50) {
                        s = ((max - min) * 100 + 127) / (max + min);
                    } else {
                        s = ((max - min) * 100 + 127) / ((255 - max) + (255 - min));
                    }
                    if (r == max) {
                        h = 120 + (g - b) * 60 / (max - min);
                    } else if (g == max) {
                        h = 240 + (b - r) * 60 / (max - min);
                    } else if (r < g) /* if (b == max) */ {
                        h = 360 + (r - g) * 60 / (max - min);
                    } else {
                        h = 0 + (r - g) * 60 / (max - min);
                    }
                }
                /* DECGCI Graphics Color Introducer  # Pc ; Pu; Px; Py; Pz */
                nwrite = sprintf((char *)context->buffer + context->pos, "#%d;1;%d;%d;%d",
                                 n, h, l, s);
                if (nwrite <= 0) {
                    return (-1);
                }
                sixel_advance(context, nwrite);
            }
        } else {
            for (n = 0; n < ncolors; n++) {
                if (palstate && palstate[n] != PALETTE_CHANGE) {
                    continue;
                }
                /* DECGCI Graphics Color Introducer  # Pc ; Pu; Px; Py; Pz */
                nwrite = sprintf((char *)context->buffer + context->pos, "#%d;2;%d;%d;%d",
                                 n,
                                 (palette[n * 3 + 0] * 100 + 127) / 255,
                                 (palette[n * 3 + 1] * 100 + 127) / 255,
                                 (palette[n * 3 + 2] * 100 + 127) / 255);
                if (nwrite <= 0) {
                    return (-1);
                }
                sixel_advance(context, nwrite);
            }
        }
    }

    for (y = i = 0; y < height; y++) {
        int fillable;
        if (context->encode_policy != ENCODEPOLICY_SIZE) {
            fillable = 0;
        }
        else if (palstate) {
            /* high color sixel */
            pix = pixels[(y-i)*width];
            if (pix < 0 || pix >= ncolors || pix == keycolor) {
                fillable = 0;
            } else {
                fillable = 1;
            }
        } else {
            /* normal sixel */
            fillable = 1;
        }
        for (x = 0; x < width; x++) {
            pix = pixels[y * width + x];
            if (pix >= 0 && pix < ncolors && pix != keycolor) {
                map[pix * width + x] |= (1 << i);
            }
            else if (!palstate) {
                fillable = 0;
            }
        }

        if (++i < 6 && (y + 1) < height) {
            continue;
        }

        for (c = 0; c < ncolors; c++) {
            for (sx = 0; sx < width; sx++) {
                if (*(map + c * width + sx) == 0) {
                    continue;
                }

                for (mx = sx + 1; mx < width; mx++) {
                    if (*(map + c * width + mx) != 0) {
                        continue;
                    }

                    for (n = 1; (mx + n) < width; n++) {
                        if (*(map + c * width + mx + n) != 0) {
                            break;
                        }
                    }

                    if (n >= 10 || (mx + n) >= width) {
                        break;
                    }
                    mx = mx + n - 1;
                }

                if ((np = context->node_free) != NULL) {
                    context->node_free = np->next;
                } else if ((np = (sixel_node_t *)malloc(sizeof(sixel_node_t))) == NULL) {
                    return (-1);
                }

                np->pal = c;
                np->sx = sx;
                np->mx = mx;
                np->map = map + c * width;

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

                sx = mx - 1;
            }

        }

        if (y != 5) {
            /* DECGNL Graphics Next Line */
            context->buffer[context->pos] = '-';
            sixel_advance(context, 1);
        }

        for (x = 0; (np = context->node_top) != NULL;) {
            sixel_node_t *next;
            if (x > np->sx) {
                /* DECGCR Graphics Carriage Return */
                context->buffer[context->pos] = '$';
                sixel_advance(context, 1);
                x = 0;
            }

            if (fillable) {
                memset(np->map + np->sx, (1 << i) - 1, np->mx - np->sx);
            }
            x = sixel_put_node(context, x, np, ncolors, keycolor);
            next = np->next;
            sixel_node_del(context, np);
            np = next;

            while (np != NULL) {
                if (np->sx < x) {
                    np = np->next;
                    continue;
                }

                if (fillable) {
                    memset(np->map + np->sx, (1 << i) - 1, np->mx - np->sx);
                }
                x = sixel_put_node(context, x, np, ncolors, keycolor);
                next = np->next;
                sixel_node_del(context, np);
                np = next;
            }

            fillable = 0;
        }

        i = 0;
        memset(map, 0, len);
    }

    if (palstate) {
        context->buffer[context->pos] = '$';
        sixel_advance(context, 1);
    }

    /* free nodes */
    while ((np = context->node_free) != NULL) {
        context->node_free = np->next;
        free(np);
    }

    free(map);

    return 0;
}


static int
sixel_encode_footer(sixel_output_t *context)
{
    int nwrite;

    if (!context->skip_dcs_envelope && !context->penetrate_multiplexer) {
        if (context->has_8bit_control) {
            nwrite = sprintf((char *)context->buffer + context->pos, "\x9c");
        } else {
            nwrite = sprintf((char *)context->buffer + context->pos, "\x1b\\");
        }
        if (nwrite <= 0) {
            return (-1);
        }
        sixel_advance(context, nwrite);
    }

    /* flush buffer */
    if (context->pos > 0) {
        if (context->penetrate_multiplexer) {
            penetrate(context, context->pos);
            context->fn_write("\x1bP\x1b\x1b\\\x1bP\\\x1b\\", 10, context->priv);
        }
        else {
            context->fn_write((char *)context->buffer, context->pos, context->priv);
        }
    }

    return 0;
}


static int
sixel_encode_dither(unsigned char *pixels, int width, int height,
                    sixel_dither_t *dither, sixel_output_t *context)
{
    unsigned char *paletted_pixels = NULL;
    unsigned char *input_pixels;
    int nret = (-1);

    switch (dither->pixelformat) {
    case PIXELFORMAT_PAL8:
    case PIXELFORMAT_G8:
    case PIXELFORMAT_GA88:
    case PIXELFORMAT_AG88:
        input_pixels = pixels;
        break;
    default:
        /* apply palette */
        paletted_pixels = sixel_dither_apply_palette(dither, pixels, width, height);
        if (paletted_pixels == NULL) {
            goto end;
        }
        input_pixels = paletted_pixels;
        break;
    }

    sixel_encode_header(width, height, context);
    sixel_encode_body(input_pixels, width, height,
                      dither->palette, dither->ncolors,
                      dither->keycolor, dither->bodyonly, context, NULL);
    sixel_encode_footer(context);
    nret = 0;

end:
    free(paletted_pixels);

    return nret;
}


static void
dither_func_none(unsigned char *data, int width)
{
    (void) data;  /* unused */
    (void) width; /* unused */
}


static void
dither_func_fs(unsigned char *data, int width)
{
    int r, g, b;
    int error_r = data[0] & 0x7;
    int error_g = data[1] & 0x7;
    int error_b = data[2] & 0x7;

    /* Floyd Steinberg Method
     *          curr    7/16
     *  3/16    5/48    1/16
     */
    r = (data[3 + 0] + (error_r * 5 >> 4));
    g = (data[3 + 1] + (error_g * 5 >> 4));
    b = (data[3 + 2] + (error_b * 5 >> 4));
    data[3 + 0] = r > 0xff ? 0xff: r;
    data[3 + 1] = g > 0xff ? 0xff: g;
    data[3 + 2] = b > 0xff ? 0xff: b;
    r = data[width * 3 - 3 + 0] + (error_r * 3 >> 4);
    g = data[width * 3 - 3 + 1] + (error_g * 3 >> 4);
    b = data[width * 3 - 3 + 2] + (error_b * 3 >> 4);
    data[width * 3 - 3 + 0] = r > 0xff ? 0xff: r;
    data[width * 3 - 3 + 1] = g > 0xff ? 0xff: g;
    data[width * 3 - 3 + 2] = b > 0xff ? 0xff: b;
    r = data[width * 3 + 0] + (error_r * 5 >> 4);
    g = data[width * 3 + 1] + (error_g * 5 >> 4);
    b = data[width * 3 + 2] + (error_b * 5 >> 4);
    data[width * 3 + 0] = r > 0xff ? 0xff: r;
    data[width * 3 + 1] = g > 0xff ? 0xff: g;
    data[width * 3 + 2] = b > 0xff ? 0xff: b;
}


static void
dither_func_atkinson(unsigned char *data, int width)
{
    int r, g, b;
    int error_r = data[0] & 0x7;
    int error_g = data[1] & 0x7;
    int error_b = data[2] & 0x7;

    error_r += 4;
    error_g += 4;
    error_b += 4;

    /* Atkinson's Method
     *          curr    1/8    1/8
     *   1/8     1/8    1/8
     *           1/8
     */
    r = data[(width * 0 + 1) * 3 + 0] + (error_r >> 3);
    g = data[(width * 0 + 1) * 3 + 1] + (error_g >> 3);
    b = data[(width * 0 + 1) * 3 + 2] + (error_b >> 3);
    data[(width * 0 + 1) * 3 + 0] = r > 0xff ? 0xff: r;
    data[(width * 0 + 1) * 3 + 1] = g > 0xff ? 0xff: g;
    data[(width * 0 + 1) * 3 + 2] = b > 0xff ? 0xff: b;
    r = data[(width * 0 + 2) * 3 + 0] + (error_r >> 3);
    g = data[(width * 0 + 2) * 3 + 1] + (error_g >> 3);
    b = data[(width * 0 + 2) * 3 + 2] + (error_b >> 3);
    data[(width * 0 + 2) * 3 + 0] = r > 0xff ? 0xff: r;
    data[(width * 0 + 2) * 3 + 1] = g > 0xff ? 0xff: g;
    data[(width * 0 + 2) * 3 + 2] = b > 0xff ? 0xff: b;
    r = data[(width * 1 - 1) * 3 + 0] + (error_r >> 3);
    g = data[(width * 1 - 1) * 3 + 1] + (error_g >> 3);
    b = data[(width * 1 - 1) * 3 + 2] + (error_b >> 3);
    data[(width * 1 - 1) * 3 + 0] = r > 0xff ? 0xff: r;
    data[(width * 1 - 1) * 3 + 1] = g > 0xff ? 0xff: g;
    data[(width * 1 - 1) * 3 + 2] = b > 0xff ? 0xff: b;
    r = data[(width * 1 + 0) * 3 + 0] + (error_r >> 3);
    g = data[(width * 1 + 0) * 3 + 1] + (error_g >> 3);
    b = data[(width * 1 + 0) * 3 + 2] + (error_b >> 3);
    data[(width * 1 + 0) * 3 + 0] = r > 0xff ? 0xff: r;
    data[(width * 1 + 0) * 3 + 1] = g > 0xff ? 0xff: g;
    data[(width * 1 + 0) * 3 + 2] = b > 0xff ? 0xff: b;
    r = (data[(width * 1 + 1) * 3 + 0] + (error_r >> 3));
    g = (data[(width * 1 + 1) * 3 + 1] + (error_g >> 3));
    b = (data[(width * 1 + 1) * 3 + 2] + (error_b >> 3));
    data[(width * 1 + 1) * 3 + 0] = r > 0xff ? 0xff: r;
    data[(width * 1 + 1) * 3 + 1] = g > 0xff ? 0xff: g;
    data[(width * 1 + 1) * 3 + 2] = b > 0xff ? 0xff: b;
    r = (data[(width * 2 + 0) * 3 + 0] + (error_r >> 3));
    g = (data[(width * 2 + 0) * 3 + 1] + (error_g >> 3));
    b = (data[(width * 2 + 0) * 3 + 2] + (error_b >> 3));
    data[(width * 2 + 0) * 3 + 0] = r > 0xff ? 0xff: r;
    data[(width * 2 + 0) * 3 + 1] = g > 0xff ? 0xff: g;
    data[(width * 2 + 0) * 3 + 2] = b > 0xff ? 0xff: b;
}


static void
dither_func_jajuni(unsigned char *data, int width)
{
    int r, g, b;
    int error_r = data[0] & 0x7;
    int error_g = data[1] & 0x7;
    int error_b = data[2] & 0x7;

    error_r += 4;
    error_g += 4;
    error_b += 4;

    /* Jarvis, Judice & Ninke Method
     *                  curr    7/48    5/48
     *  3/48    5/48    7/48    5/48    3/48
     *  1/48    3/48    5/48    3/48    1/48
     */
    r = data[(width * 0 + 1) * 3 + 0] + (error_r * 7 / 48);
    g = data[(width * 0 + 1) * 3 + 1] + (error_g * 7 / 48);
    b = data[(width * 0 + 1) * 3 + 2] + (error_b * 7 / 48);
    data[(width * 0 + 1) * 3 + 0] = r > 0xff ? 0xff: r;
    data[(width * 0 + 1) * 3 + 1] = g > 0xff ? 0xff: g;
    data[(width * 0 + 1) * 3 + 2] = b > 0xff ? 0xff: b;
    r = data[(width * 0 + 2) * 3 + 0] + (error_r * 5 / 48);
    g = data[(width * 0 + 2) * 3 + 1] + (error_g * 5 / 48);
    b = data[(width * 0 + 2) * 3 + 2] + (error_b * 5 / 48);
    data[(width * 0 + 2) * 3 + 0] = r > 0xff ? 0xff: r;
    data[(width * 0 + 2) * 3 + 1] = g > 0xff ? 0xff: g;
    data[(width * 0 + 2) * 3 + 2] = b > 0xff ? 0xff: b;
    r = data[(width * 1 - 2) * 3 + 0] + (error_r * 3 / 48);
    g = data[(width * 1 - 2) * 3 + 1] + (error_g * 3 / 48);
    b = data[(width * 1 - 2) * 3 + 2] + (error_b * 3 / 48);
    data[(width * 1 - 2) * 3 + 0] = r > 0xff ? 0xff: r;
    data[(width * 1 - 2) * 3 + 1] = g > 0xff ? 0xff: g;
    data[(width * 1 - 2) * 3 + 2] = b > 0xff ? 0xff: b;
    r = data[(width * 1 - 1) * 3 + 0] + (error_r * 5 / 48);
    g = data[(width * 1 - 1) * 3 + 1] + (error_g * 5 / 48);
    b = data[(width * 1 - 1) * 3 + 2] + (error_b * 5 / 48);
    data[(width * 1 - 1) * 3 + 0] = r > 0xff ? 0xff: r;
    data[(width * 1 - 1) * 3 + 1] = g > 0xff ? 0xff: g;
    data[(width * 1 - 1) * 3 + 2] = b > 0xff ? 0xff: b;
    r = data[(width * 1 + 0) * 3 + 0] + (error_r * 7 / 48);
    g = data[(width * 1 + 0) * 3 + 1] + (error_g * 7 / 48);
    b = data[(width * 1 + 0) * 3 + 2] + (error_b * 7 / 48);
    data[(width * 1 + 0) * 3 + 0] = r > 0xff ? 0xff: r;
    data[(width * 1 + 0) * 3 + 1] = g > 0xff ? 0xff: g;
    data[(width * 1 + 0) * 3 + 2] = b > 0xff ? 0xff: b;
    r = data[(width * 1 + 1) * 3 + 0] + (error_r * 5 / 48);
    g = data[(width * 1 + 1) * 3 + 1] + (error_g * 5 / 48);
    b = data[(width * 1 + 1) * 3 + 2] + (error_b * 5 / 48);
    data[(width * 1 + 1) * 3 + 0] = r > 0xff ? 0xff: r;
    data[(width * 1 + 1) * 3 + 1] = g > 0xff ? 0xff: g;
    data[(width * 1 + 1) * 3 + 2] = b > 0xff ? 0xff: b;
    r = data[(width * 1 + 2) * 3 + 0] + (error_r * 3 / 48);
    g = data[(width * 1 + 2) * 3 + 1] + (error_g * 3 / 48);
    b = data[(width * 1 + 2) * 3 + 2] + (error_b * 3 / 48);
    data[(width * 1 + 2) * 3 + 0] = r > 0xff ? 0xff: r;
    data[(width * 1 + 2) * 3 + 1] = g > 0xff ? 0xff: g;
    data[(width * 1 + 2) * 3 + 2] = b > 0xff ? 0xff: b;
    r = data[(width * 2 - 2) * 3 + 0] + (error_r * 1 / 48);
    g = data[(width * 2 - 2) * 3 + 1] + (error_g * 1 / 48);
    b = data[(width * 2 - 2) * 3 + 2] + (error_b * 1 / 48);
    data[(width * 2 - 2) * 3 + 0] = r > 0xff ? 0xff: r;
    data[(width * 2 - 2) * 3 + 1] = g > 0xff ? 0xff: g;
    data[(width * 2 - 2) * 3 + 2] = b > 0xff ? 0xff: b;
    r = data[(width * 2 - 1) * 3 + 0] + (error_r * 3 / 48);
    g = data[(width * 2 - 1) * 3 + 1] + (error_g * 3 / 48);
    b = data[(width * 2 - 1) * 3 + 2] + (error_b * 3 / 48);
    data[(width * 2 - 1) * 3 + 0] = r > 0xff ? 0xff: r;
    data[(width * 2 - 1) * 3 + 1] = g > 0xff ? 0xff: g;
    data[(width * 2 - 1) * 3 + 2] = b > 0xff ? 0xff: b;
    r = data[(width * 2 + 0) * 3 + 0] + (error_r * 5 / 48);
    g = data[(width * 2 + 0) * 3 + 1] + (error_g * 5 / 48);
    b = data[(width * 2 + 0) * 3 + 2] + (error_b * 5 / 48);
    data[(width * 2 + 0) * 3 + 0] = r > 0xff ? 0xff: r;
    data[(width * 2 + 0) * 3 + 1] = g > 0xff ? 0xff: g;
    data[(width * 2 + 0) * 3 + 2] = b > 0xff ? 0xff: b;
    r = data[(width * 2 + 1) * 3 + 0] + (error_r * 3 / 48);
    g = data[(width * 2 + 1) * 3 + 1] + (error_g * 3 / 48);
    b = data[(width * 2 + 1) * 3 + 2] + (error_b * 3 / 48);
    data[(width * 2 + 1) * 3 + 0] = r > 0xff ? 0xff: r;
    data[(width * 2 + 1) * 3 + 1] = g > 0xff ? 0xff: g;
    data[(width * 2 + 1) * 3 + 2] = b > 0xff ? 0xff: b;
    r = data[(width * 2 + 2) * 3 + 0] + (error_r * 1 / 48);
    g = data[(width * 2 + 2) * 3 + 1] + (error_g * 1 / 48);
    b = data[(width * 2 + 2) * 3 + 2] + (error_b * 1 / 48);
    data[(width * 2 + 2) * 3 + 0] = r > 0xff ? 0xff: r;
    data[(width * 2 + 2) * 3 + 1] = g > 0xff ? 0xff: g;
    data[(width * 2 + 2) * 3 + 2] = b > 0xff ? 0xff: b;
}


static void
dither_func_stucki(unsigned char *data, int width)
{
    int r, g, b;
    int error_r = data[0] & 0x7;
    int error_g = data[1] & 0x7;
    int error_b = data[2] & 0x7;

    error_r += 4;
    error_g += 4;
    error_b += 4;

    /* Stucki's Method
     *                  curr    8/48    4/48
     *  2/48    4/48    8/48    4/48    2/48
     *  1/48    2/48    4/48    2/48    1/48
     */
    r = data[(width * 0 + 1) * 3 + 0] + (error_r * 8 / 48);
    g = data[(width * 0 + 1) * 3 + 1] + (error_g * 8 / 48);
    b = data[(width * 0 + 1) * 3 + 2] + (error_b * 8 / 48);
    data[(width * 0 + 1) * 3 + 0] = r > 0xff ? 0xff: r;
    data[(width * 0 + 1) * 3 + 1] = g > 0xff ? 0xff: g;
    data[(width * 0 + 1) * 3 + 2] = b > 0xff ? 0xff: b;
    r = data[(width * 0 + 2) * 3 + 0] + (error_r * 4 / 48);
    g = data[(width * 0 + 2) * 3 + 1] + (error_g * 4 / 48);
    b = data[(width * 0 + 2) * 3 + 2] + (error_b * 4 / 48);
    data[(width * 0 + 2) * 3 + 0] = r > 0xff ? 0xff: r;
    data[(width * 0 + 2) * 3 + 1] = g > 0xff ? 0xff: g;
    data[(width * 0 + 2) * 3 + 2] = b > 0xff ? 0xff: b;
    r = data[(width * 1 - 2) * 3 + 0] + (error_r * 2 / 48);
    g = data[(width * 1 - 2) * 3 + 1] + (error_g * 2 / 48);
    b = data[(width * 1 - 2) * 3 + 2] + (error_b * 2 / 48);
    data[(width * 1 - 2) * 3 + 0] = r > 0xff ? 0xff: r;
    data[(width * 1 - 2) * 3 + 1] = g > 0xff ? 0xff: g;
    data[(width * 1 - 2) * 3 + 2] = b > 0xff ? 0xff: b;
    r = data[(width * 1 - 1) * 3 + 0] + (error_r * 4 / 48);
    g = data[(width * 1 - 1) * 3 + 1] + (error_g * 4 / 48);
    b = data[(width * 1 - 1) * 3 + 2] + (error_b * 4 / 48);
    data[(width * 1 - 1) * 3 + 0] = r > 0xff ? 0xff: r;
    data[(width * 1 - 1) * 3 + 1] = g > 0xff ? 0xff: g;
    data[(width * 1 - 1) * 3 + 2] = b > 0xff ? 0xff: b;
    r = data[(width * 1 + 0) * 3 + 0] + (error_r * 8 / 48);
    g = data[(width * 1 + 0) * 3 + 1] + (error_g * 8 / 48);
    b = data[(width * 1 + 0) * 3 + 2] + (error_b * 8 / 48);
    data[(width * 1 + 0) * 3 + 0] = r > 0xff ? 0xff: r;
    data[(width * 1 + 0) * 3 + 1] = g > 0xff ? 0xff: g;
    data[(width * 1 + 0) * 3 + 2] = b > 0xff ? 0xff: b;
    r = data[(width * 1 + 1) * 3 + 0] + (error_r * 4 / 48);
    g = data[(width * 1 + 1) * 3 + 1] + (error_g * 4 / 48);
    b = data[(width * 1 + 1) * 3 + 2] + (error_b * 4 / 48);
    data[(width * 1 + 1) * 3 + 0] = r > 0xff ? 0xff: r;
    data[(width * 1 + 1) * 3 + 1] = g > 0xff ? 0xff: g;
    data[(width * 1 + 1) * 3 + 2] = b > 0xff ? 0xff: b;
    r = data[(width * 1 + 2) * 3 + 0] + (error_r * 2 / 48);
    g = data[(width * 1 + 2) * 3 + 1] + (error_g * 2 / 48);
    b = data[(width * 1 + 2) * 3 + 2] + (error_b * 2 / 48);
    data[(width * 1 + 2) * 3 + 0] = r > 0xff ? 0xff: r;
    data[(width * 1 + 2) * 3 + 1] = g > 0xff ? 0xff: g;
    data[(width * 1 + 2) * 3 + 2] = b > 0xff ? 0xff: b;
    r = data[(width * 2 - 2) * 3 + 0] + (error_r * 1 / 48);
    g = data[(width * 2 - 2) * 3 + 1] + (error_g * 1 / 48);
    b = data[(width * 2 - 2) * 3 + 2] + (error_b * 1 / 48);
    data[(width * 2 - 2) * 3 + 0] = r > 0xff ? 0xff: r;
    data[(width * 2 - 2) * 3 + 1] = g > 0xff ? 0xff: g;
    data[(width * 2 - 2) * 3 + 2] = b > 0xff ? 0xff: b;
    r = data[(width * 2 - 1) * 3 + 0] + (error_r * 2 / 48);
    g = data[(width * 2 - 1) * 3 + 1] + (error_g * 2 / 48);
    b = data[(width * 2 - 1) * 3 + 2] + (error_b * 2 / 48);
    data[(width * 2 - 1) * 3 + 0] = r > 0xff ? 0xff: r;
    data[(width * 2 - 1) * 3 + 1] = g > 0xff ? 0xff: g;
    data[(width * 2 - 1) * 3 + 2] = b > 0xff ? 0xff: b;
    r = data[(width * 2 + 0) * 3 + 0] + (error_r * 4 / 48);
    g = data[(width * 2 + 0) * 3 + 1] + (error_g * 4 / 48);
    b = data[(width * 2 + 0) * 3 + 2] + (error_b * 4 / 48);
    data[(width * 2 + 0) * 3 + 0] = r > 0xff ? 0xff: r;
    data[(width * 2 + 0) * 3 + 1] = g > 0xff ? 0xff: g;
    data[(width * 2 + 0) * 3 + 2] = b > 0xff ? 0xff: b;
    r = data[(width * 2 + 1) * 3 + 0] + (error_r * 2 / 48);
    g = data[(width * 2 + 1) * 3 + 1] + (error_g * 2 / 48);
    b = data[(width * 2 + 1) * 3 + 2] + (error_b * 2 / 48);
    data[(width * 2 + 1) * 3 + 0] = r > 0xff ? 0xff: r;
    data[(width * 2 + 1) * 3 + 1] = g > 0xff ? 0xff: g;
    data[(width * 2 + 1) * 3 + 2] = b > 0xff ? 0xff: b;
    r = data[(width * 2 + 2) * 3 + 0] + (error_r * 1 / 48);
    g = data[(width * 2 + 2) * 3 + 1] + (error_g * 1 / 48);
    b = data[(width * 2 + 2) * 3 + 2] + (error_b * 1 / 48);
    data[(width * 2 + 2) * 3 + 0] = r > 0xff ? 0xff: r;
    data[(width * 2 + 2) * 3 + 1] = g > 0xff ? 0xff: g;
    data[(width * 2 + 2) * 3 + 2] = b > 0xff ? 0xff: b;
}


static void
dither_func_burkes(unsigned char *data, int width)
{
    int r, g, b;
    int error_r = data[0] & 0x7;
    int error_g = data[1] & 0x7;
    int error_b = data[2] & 0x7;

    error_r += 2;
    error_g += 2;
    error_b += 2;

    /* Burkes' Method
     *                  curr    4/16    2/16
     *  1/16    2/16    4/16    2/16    1/16
     */
    r = data[(width * 0 + 1) * 3 + 0] + (error_r * 4 / 16);
    g = data[(width * 0 + 1) * 3 + 1] + (error_g * 4 / 16);
    b = data[(width * 0 + 1) * 3 + 2] + (error_b * 4 / 16);
    data[(width * 0 + 1) * 3 + 0] = r > 0xff ? 0xff: r;
    data[(width * 0 + 1) * 3 + 1] = g > 0xff ? 0xff: g;
    data[(width * 0 + 1) * 3 + 2] = b > 0xff ? 0xff: b;
    r = data[(width * 0 + 2) * 3 + 0] + (error_r * 2 / 16);
    g = data[(width * 0 + 2) * 3 + 1] + (error_g * 2 / 16);
    b = data[(width * 0 + 2) * 3 + 2] + (error_b * 2 / 16);
    data[(width * 0 + 2) * 3 + 0] = r > 0xff ? 0xff: r;
    data[(width * 0 + 2) * 3 + 1] = g > 0xff ? 0xff: g;
    data[(width * 0 + 2) * 3 + 2] = b > 0xff ? 0xff: b;
    r = data[(width * 1 - 2) * 3 + 0] + (error_r * 1 / 16);
    g = data[(width * 1 - 2) * 3 + 1] + (error_g * 1 / 16);
    b = data[(width * 1 - 2) * 3 + 2] + (error_b * 1 / 16);
    data[(width * 1 - 2) * 3 + 0] = r > 0xff ? 0xff: r;
    data[(width * 1 - 2) * 3 + 1] = g > 0xff ? 0xff: g;
    data[(width * 1 - 2) * 3 + 2] = b > 0xff ? 0xff: b;
    r = data[(width * 1 - 1) * 3 + 0] + (error_r * 2 / 16);
    g = data[(width * 1 - 1) * 3 + 1] + (error_g * 2 / 16);
    b = data[(width * 1 - 1) * 3 + 2] + (error_b * 2 / 16);
    data[(width * 1 - 1) * 3 + 0] = r > 0xff ? 0xff: r;
    data[(width * 1 - 1) * 3 + 1] = g > 0xff ? 0xff: g;
    data[(width * 1 - 1) * 3 + 2] = b > 0xff ? 0xff: b;
    r = data[(width * 1 + 0) * 3 + 0] + (error_r * 4 / 16);
    g = data[(width * 1 + 0) * 3 + 1] + (error_g * 4 / 16);
    b = data[(width * 1 + 0) * 3 + 2] + (error_b * 4 / 16);
    data[(width * 1 + 0) * 3 + 0] = r > 0xff ? 0xff: r;
    data[(width * 1 + 0) * 3 + 1] = g > 0xff ? 0xff: g;
    data[(width * 1 + 0) * 3 + 2] = b > 0xff ? 0xff: b;
    r = data[(width * 1 + 1) * 3 + 0] + (error_r * 2 / 16);
    g = data[(width * 1 + 1) * 3 + 1] + (error_g * 2 / 16);
    b = data[(width * 1 + 1) * 3 + 2] + (error_b * 2 / 16);
    data[(width * 1 + 1) * 3 + 0] = r > 0xff ? 0xff: r;
    data[(width * 1 + 1) * 3 + 1] = g > 0xff ? 0xff: g;
    data[(width * 1 + 1) * 3 + 2] = b > 0xff ? 0xff: b;
    r = data[(width * 1 + 2) * 3 + 0] + (error_r * 1 / 16);
    g = data[(width * 1 + 2) * 3 + 1] + (error_g * 1 / 16);
    b = data[(width * 1 + 2) * 3 + 2] + (error_b * 1 / 16);
    data[(width * 1 + 2) * 3 + 0] = r > 0xff ? 0xff: r;
    data[(width * 1 + 2) * 3 + 1] = g > 0xff ? 0xff: g;
    data[(width * 1 + 2) * 3 + 2] = b > 0xff ? 0xff: b;
}


static void
sixel_apply_15bpp_dither(
    unsigned char *pixels,
    int x, int y, int width, int height,
    int method_for_diffuse)
{
    /* apply floyd steinberg dithering */
    switch (method_for_diffuse) {
    case DIFFUSE_FS:
        if (x < width - 1 && y < height - 1) {
            dither_func_fs(pixels, width);
        }
        break;
    case DIFFUSE_ATKINSON:
        if (x < width - 2 && y < height - 2) {
            dither_func_atkinson(pixels, width);
        }
        break;
    case DIFFUSE_JAJUNI:
        if (x < width - 2 && y < height - 2) {
            dither_func_jajuni(pixels, width);
        }
        break;
    case DIFFUSE_STUCKI:
        if (x < width - 2 && y < height - 2) {
            dither_func_stucki(pixels, width);
        }
        break;
    case DIFFUSE_BURKES:
        if (x < width - 2 && y < height - 1) {
            dither_func_burkes(pixels, width);
        }
        break;
    case DIFFUSE_NONE:
    default:
        dither_func_none(pixels, width);
        break;
    }
}


static int
sixel_encode_fullcolor(unsigned char *pixels, int width, int height,
                       sixel_dither_t *dither, sixel_output_t *context)
{
    unsigned char *paletted_pixels = NULL;
    unsigned char *normalized_pixels = NULL;
    /* Mark sixel line pixels which have been already drawn. */
    unsigned char *marks;
    unsigned char *rgbhit;
    unsigned char *rgb2pal;
    unsigned char palhitcount[256];
    unsigned char palstate[256];
    int output_count;
    int nret = (-1);
    int const maxcolors = 1 << 15;

    if (dither->pixelformat != PIXELFORMAT_RGB888) {
        /* normalize pixelfromat */
        normalized_pixels = malloc(width * height * 3);
        if (normalized_pixels == NULL) {
            goto error;
        }
        sixel_normalize_pixelformat(normalized_pixels, pixels, width, height, dither->pixelformat);
        pixels = normalized_pixels;
    }

    paletted_pixels = (unsigned char*)malloc(width * height + maxcolors * 2 + width * 6);
    if (paletted_pixels == NULL) {
        goto error;
    }
    rgbhit = paletted_pixels + width * height;
    memset(rgbhit, 0, maxcolors * 2 + width * 6);
    rgb2pal = rgbhit + maxcolors;
    marks = rgb2pal + maxcolors;
    output_count = 0;
    while (1) {
        int x, y;
        unsigned char *dst;
        unsigned char *mptr;
        int dirty;
        int mod_y;
        int nextpal;
        int threshold;

        dst = paletted_pixels;
        nextpal = 0;
        threshold = 1;
        dirty = 0;
        mptr = marks;
        memset(palstate, 0, sizeof(palstate));
        y = mod_y = 0;

        while (1) {
            for (x = 0; x < width; x++, mptr++, dst++, pixels += 3) {
                if (*mptr) {
                    *dst = 255;
                }
                else {
                    int pix = ((pixels[0] & 0xf8) << 7) |
                              ((pixels[1] & 0xf8) << 2) |
                              ((pixels[2] >> 3) & 0x1f);
                    sixel_apply_15bpp_dither(pixels,
                                             x, y, width, height,
                                             dither->method_for_diffuse);
                    if (!rgbhit[pix]) {
                        while (1) {
                            if (nextpal >= 255) {
                                if (threshold >= 255) {
                                    break;
                                }
                                else {
                                    threshold = (threshold == 1) ? 9: 255;
                                    nextpal = 0;
                                }
                            }
                            else if (palstate[nextpal] ||
                                     palhitcount[nextpal] > threshold) {
                                nextpal++;
                            }
                            else {
                                break;
                            }
                        }

                        if (nextpal >= 255) {
                            dirty = 1;
                            *dst = 255;
                        }
                        else {
                            unsigned char *pal = dither->palette + (nextpal * 3);

                            rgbhit[pix] = 1;
                            if (output_count > 0) {
                                rgbhit[((pal[0] & 0xf8) << 7) |
                                       ((pal[1] & 0xf8) << 2) |
                                       ((pal[2] >> 3) & 0x1f)] = 0;
                            }
                            *dst = rgb2pal[pix] = nextpal++;
                            *mptr = 1;
                            palstate[*dst] = PALETTE_CHANGE;
                            palhitcount[*dst] = 1;
                            *(pal++) = pixels[0];
                            *(pal++) = pixels[1];
                            *(pal++) = pixels[2];
                        }
                    }
                    else {
                        *dst = rgb2pal[pix];
                        *mptr = 1;
                        if (!palstate[*dst]) {
                            palstate[*dst] = PALETTE_HIT;
                        }
                        if (palhitcount[*dst] < 255) {
                            palhitcount[*dst]++;
                        }
                    }
                }
            }

            if (++y >= height) {
                if (dirty) {
                    mod_y = 5;
                }
                else {
                    goto end;
                }
            }
            if (dirty && mod_y == 5) {
                int orig_height = height;

                if (output_count++ == 0) {
                    sixel_encode_header(width, height, context);
                }
                height = y;
                sixel_encode_body(paletted_pixels, width, height,
                                  dither->palette, 255, 255,
                                  dither->bodyonly, context, palstate);
                pixels -= (6 * width * 3);
                height = orig_height - height + 6;
                break;
            }

            if (++mod_y == 6) {
                mptr = memset(marks, 0, width * 6);
                mod_y = 0;
            }
        }
    }

end:
    if (output_count == 0) {
        sixel_encode_header(width, height, context);
    }
    sixel_encode_body(paletted_pixels, width, height,
                      dither->palette, 255, 255,
                      dither->bodyonly, context, palstate);
    sixel_encode_footer(context);
    nret = 0;

error:
    free(paletted_pixels);
    free(normalized_pixels);

    return nret;
}


int sixel_encode(unsigned char  /* in */ *pixels,     /* pixel bytes */
                 int            /* in */ width,       /* image width */
                 int            /* in */ height,      /* image height */
                 int const      /* in */ depth,       /* color depth */
                 sixel_dither_t /* in */ *dither,     /* dither context */
                 sixel_output_t /* in */ *context)    /* output context */
{
    int nret = (-1);

    /* TODO: reference counting should be thread-safe */
    sixel_dither_ref(dither);
    sixel_output_ref(context);

    (void) depth;

    if (dither->quality_mode == QUALITY_HIGHCOLOR) {
        nret = sixel_encode_fullcolor(pixels, width, height,
                                      dither, context);
    } else {
        nret = sixel_encode_dither(pixels, width, height,
                                   dither, context);
    }

    sixel_output_unref(context);
    sixel_dither_unref(dither);

    return nret;
}


/* emacs, -*- Mode: C; tab-width: 4; indent-tabs-mode: nil -*- */
/* vim: set expandtab ts=4 : */
/* EOF */
