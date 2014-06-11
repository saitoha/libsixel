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

#ifndef LIBSIXEL_SCALE_H
#define LIBSIXEL_SCALE_H

/* method for re-sampling */
enum methodForResampling {
    RES_NEAREST,  /* Use nearest neighbor method */
    RES_GAUSSIAN, /* Use guaussian filter */
    RES_HANNING,  /* Use hanning filter */
    RES_HAMMING,  /* Use hamming filter */
    RES_BILINEAR, /* Use bilinear filter */
    RES_WELSH,    /* Use welsh filter */
    RES_BICUBIC,  /* Use bicubic filter */
    RES_LANCZOS2, /* Use lanczos-2 filter */
    RES_LANCZOS3, /* Use lanczos-3 filter */
    RES_LANCZOS4, /* Use lanczos-4 filter */
};


#ifdef __cplusplus
extern "C" {
#endif

/* exported functions */

/* image scaling api */

unsigned char *
LSS_scale(unsigned char const *pixels,
          int srcx, int srcy, int depth,
          int destx, int desty,
          enum methodForResampling const methodForResampling);

#ifdef __cplusplus
}
#endif


#endif /* LIBSIXEL_SCALE_H */

/* emacs, -*- Mode: C; tab-width: 4; indent-tabs-mode: nil -*- */
/* vim: set expandtab ts=4 : */
/* EOF */
