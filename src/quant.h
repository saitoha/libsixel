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

#ifdef __cplusplus
extern "C" {
#endif

unsigned char *
sixel_quant_make_palette(
    unsigned const char /* in */ *data,  /* data for sampling */
    int /* in */ length,                 /* data size */
    int /* in */ depth,
    int /* in */ reqcolors,
    int /* in */ *ncolors,
    int /* in */ *origcolors,
    int /* in */ methodForLargest,
    int /* in */ methodForRep,
    int /* in */ qualityMode);

int
sixel_quant_apply_palette(unsigned char *data,
                          int width, int height, int depth,
                          unsigned char *palette, int reqcolor,
                          int const methodForDiffuse,
                          int foptimize,
                          int foptimize_palette,
                          int complexion,
                          unsigned short *cachetable,
                          int *ncolor,
                          unsigned char *result);


void
sixel_quant_free_palette(unsigned char * data);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_QUANT_H */

/* emacs, -*- Mode: C; tab-width: 4; indent-tabs-mode: nil -*- */
/* vim: set expandtab ts=4 : */
/* EOF */
