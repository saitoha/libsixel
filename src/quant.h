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

#ifndef LIBSIXEL_QUANT_H
#define LIBSIXEL_QUANT_H

/* method for finding the largest dimension for splitting,
 * and sorting by that component */
enum methodForLargest {
    LARGE_AUTO = 0,  /* choose automatically the method for finding the largest
                        dimension */
    LARGE_NORM = 1,  /* simply comparing the range in RGB space */
    LARGE_LUM  = 2   /* transforming into luminosities before the comparison */
};

/* method for choosing a color from the box */
enum methodForRep {
    REP_AUTO           = 0, /* choose automatically the method for selecting
                               representative color from each box */
    REP_CENTER_BOX     = 1, /* choose the center of the box */
    REP_AVERAGE_COLORS = 2, /* choose the average all the color
                               in the box (specified in Heckbert's paper) */
    REP_AVERAGE_PIXELS = 3  /* choose the average all the pixels in the box */
};

/* method for diffusing */
enum methodForDiffuse {
    DIFFUSE_AUTO     = 0, /* choose diffusion type automatically */
    DIFFUSE_NONE     = 1, /* don't diffuse */
    DIFFUSE_ATKINSON = 2, /* diffuse with Bill Atkinson's method */
    DIFFUSE_FS       = 3, /* diffuse with Floyd-Steinberg method */
    DIFFUSE_JAJUNI   = 4, /* diffuse with Jarvis, Judice & Ninke method */
    DIFFUSE_STUCKI   = 5, /* diffuse with Stucki's method */
    DIFFUSE_BURKES   = 6  /* diffuse with Burkes' method */
};

/* quality modes */
enum qualityMode {
    QUALITY_AUTO = 0, /* choose quality mode automatically */
    QUALITY_HIGH = 1, /* high quality */
    QUALITY_LOW  = 2, /* low quality */
};

/* built-in dither */
enum builtinDither {
    BUILTIN_MONO_DARK  = 0, /* monochrome terminal with dark background */
    BUILTIN_MONO_LIGHT = 1, /* monochrome terminal with dark background */
    BUILTIN_XTERM16    = 2, /* xterm 16color */
    BUILTIN_XTERM256   = 3, /* xterm 256color */
};

#ifdef __cplusplus
extern "C" {
#endif

unsigned char *
LSQ_MakePalette(unsigned char *data, int x, int y, int depth,
                int reqcolors, int *ncolors, int *origcolors,
                int const methodForLargest,
                int const methodForRep,
                int const qualityMode);

int
LSQ_ApplyPalette(unsigned char *data, int width, int height, int depth,
                 unsigned char *palette, int ncolor,
                 int const methodForDiffuse,
                 int foptimize,
                 int complexion,
                 unsigned short *cachetable,
                 unsigned char *result);


extern void
LSQ_FreePalette(unsigned char * data);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_QUANT_H */

/* emacs, -*- Mode: C; tab-width: 4; indent-tabs-mode: nil -*- */
/* vim: set expandtab ts=4 : */
/* EOF */
