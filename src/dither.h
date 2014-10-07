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

#ifndef LIBSIXEL_DITHER_H
#define LIBSIXEL_DITHER_H

/* dither context object */
typedef struct sixel_dither {
    unsigned int ref;           /* reference counter */
    unsigned char *palette;     /* palette definition */
    unsigned short *cachetable; /* cache table */
    int reqcolors;              /* requested colors */
    int ncolors;                /* active colors */
    int origcolors;             /* original colors */
    int optimized;              /* pixel is 15bpp compressable */
    int method_for_largest;     /* method for finding the largest dimention 
                                   for splitting */
    int method_for_rep;         /* method for choosing a color from the box */
    int method_for_diffuse;     /* method for diffusing */
    int quality_mode;           /* quality of histgram */
    int keycolor;               /* background color */
} sixel_dither_t;

#ifdef __cplusplus
extern "C" {
#endif

/* apply palette */
unsigned char * sixel_apply_palette(unsigned char *pixels, int width, int height,
                                    sixel_dither_t *dither);

void sixel_normalize_bitfield(unsigned char *dst, unsigned char *src,
                              int width, int height, int const bitfield);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_DITHER_H */

/* emacs, -*- Mode: C; tab-width: 4; indent-tabs-mode: nil -*- */
/* vim: set expandtab ts=4 : */
/* EOF */
