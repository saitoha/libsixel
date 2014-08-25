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

#include "dither.h"
#include "sixel.h"

sixel_image_t *
sixel_create_image(unsigned char *pixels, int sx, int sy, int depth,
                   int borrowed, sixel_dither_t *dither)
{
    sixel_image_t *im;

    im = (sixel_image_t *)malloc(sizeof(sixel_image_t));
    im->pixels = pixels;
    im->sx = sx;
    im->sy = sy;
    im->depth = depth;
    im->borrowed = borrowed;
    im->dither = dither;
    sixel_dither_ref(dither);
    return im;
}


void
sixel_image_destroy(sixel_image_t *im)
{
    if (im->dither) {
        sixel_dither_unref(im->dither);
    }
    if (im->pixels && !im->borrowed) {
        free(im->pixels);
    }
    free(im);
}

/* emacs, -*- Mode: C; tab-width: 4; indent-tabs-mode: nil -*- */
/* vim: set expandtab ts=4 : */
/* EOF */
