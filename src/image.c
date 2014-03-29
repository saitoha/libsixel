/*
 * Copyright (c) 2014 Hayaki Saito
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
#include "config.h"
#include <stdlib.h>

#if defined(HAVE_INTTYPES_H)
# include <inttypes.h>
#endif

#include "sixel.h"

LSImagePtr
LSImage_create(int sx, int sy, int depth, int ncolors)
{
    LSImagePtr im;
   
    im = (LSImagePtr)malloc(sizeof(LSImage));
    im->pixels = (uint8_t *)malloc(sx * sy * depth);
    im->sx = sx;
    im->sy = sy;
    im->depth = depth;
    im->ncolors = ncolors;
    im->keycolor = -1;
    return im;
}

void
LSImage_setpalette(LSImagePtr im,
                   int n, int r, int g, int b)
{
    im->red[n] = r;
    im->green[n] = g;
    im->blue[n] = b;
}

void
LSImage_setpixels(LSImagePtr im, uint8_t *pixels)
{
    im->pixels = pixels;
}

void
LSImage_copy(LSImagePtr dst,
             LSImagePtr src,
             int w, int h)
{
    int x, y;

    for (y = 0; y < h; y++) {
        for (x = 0; x < w * dst->depth; x++) {
            dst->pixels[dst->sx * dst->depth * y + x] 
                = src->pixels[src->sx * src->depth * y + x];
        }
    }
}

void
LSImage_destroy(LSImagePtr im)
{
    free(im->pixels);
    free(im);
}

void
LSImage_fill(LSImagePtr im, int color)
{
    int x, y;
    for (y = 0; y < im->sy; y++) {
        for (x = 0; x < im->sx; x++) {
            LSImage_setpixel(im, x, y, color);
        }
    }
}

void
LSImage_fillrectangle(LSImagePtr im,
                      int x1, int y1, int x2, int y2,
                      int color)
{
    int x, y;
    for (y = y1; y <= y2; y++) {
        for (x = x1; x <= x2; x++) {
            LSImage_setpixel(im, x, y, color);
        }
    }
}

void
LSImage_setpixel(LSImagePtr im, int x, int y, int color)
{
    im->pixels[im->sx * 3 * y + x * 3 + 0] = (color >> 16) & 0xff;
    im->pixels[im->sx * 3 * y + x * 3 + 1] = (color >> 8) & 0xff;
    im->pixels[im->sx * 3 * y + x * 3 + 2] = color & 0xff;
}

/* emacs, -*- Mode: C; tab-width: 4; indent-tabs-mode: nil -*- */
/* vim: set expandtab ts=4 : */
/* EOF */
