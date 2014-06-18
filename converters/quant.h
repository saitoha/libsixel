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

/* method for finding the largest dimention for splitting,
 * and sorting by that component */
enum methodForLargest {
    LARGE_AUTO,  /* choose automatically the method for finding the largest
                    dimention */
    LARGE_NORM,  /* simply comparing the range in RGB space */
    LARGE_LUM    /* transforming into luminosities before the comparison */
};

/* method for choosing a color from the box */
enum methodForRep {
    REP_AUTO,           /* choose automatically the method for selecting
                           representative color from each box */
    REP_CENTER_BOX,     /* choose the center of the box */
    REP_AVERAGE_COLORS, /* choose the average all the color
                           in the box (specified in Heckbert's paper) */
    REP_AVERAGE_PIXELS  /* choose the averate all the pixels in the box */
};

/* method for dithering */
enum methodForDiffuse {
    DIFFUSE_AUTO,       /* choose diffusion type automatically */
    DIFFUSE_NONE,       /* don't diffuse */
    DIFFUSE_ATKINSON,   /* diffuse with Bill Atkinson's method */
    DIFFUSE_FS,         /* diffuse with Floyd-Steinberg method */
    DIFFUSE_JAJUNI,     /* diffuse with Jarvis, Judice & Ninke method */
    DIFFUSE_STUCKI,     /* diffuse with Stucki's method */
    DIFFUSE_BURKES      /* diffuse with Burkes' method */
};

/* quality modes */
enum qualityMode {
    QUALITY_AUTO,       /* choose quality mode automatically */
    QUALITY_HIGH,       /* high quality */
    QUALITY_LOW,        /* low quality */
};


#ifdef __cplusplus
extern "C" {
#endif

/* exported functions */

/* the palette manipulation API */

unsigned char *
LSQ_MakePalette(unsigned char *data,
                int x,
                int y,
                int depth,
                int reqcolors,
                int *ncolors,
                int *origcolors,
                enum methodForLargest const methodForLargest,
                enum methodForRep const methodForRep,
                enum qualityMode const qualityMode);


unsigned char *
LSQ_ApplyPalette(unsigned char *data,
                 int x,
                 int y,
                 int depth,
                 unsigned char *palette,
                 int ncolors,
                 enum methodForDiffuse const methodForDiffuse,
                 int foptimize);


extern void
LSQ_FreePalette(unsigned char * data);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_QUANT_H */

/* emacs, -*- Mode: C; tab-width: 4; indent-tabs-mode: nil -*- */
/* vim: set expandtab ts=4 : */
/* EOF */
