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

#ifndef LIBSIXEL_SIXEL_H
#define LIBSIXEL_SIXEL_H

#define PALETTE_MAX 256

/* LSImage definition */
typedef struct LSImageStruct {
    /* Palette-based image pixels */
    unsigned char *pixels;
    int sx;
    int sy;
    int depth;
    int ncolors;
    int red[PALETTE_MAX];
    int green[PALETTE_MAX];
    int blue[PALETTE_MAX];
    int keycolor;  /* background color */
} LSImage, *LSImagePtr;

typedef int (* putchar_function)(int ch);
typedef int (* printf_function)(const char *fmt, ...);

typedef struct LSOutputContextStruct {
    putchar_function fn_putchar;
    printf_function fn_printf;
} LSOutputContext, *LSOutputContextPtr;

LSOutputContextPtr LSOutputContext_new();
void LSOutputContext_free(LSOutputContextPtr context);

/* converter API */

#ifdef __cplusplus
extern "C" {
#endif

extern LSImagePtr LibSixel_SixelToLSImage(uint8_t *p, int len);
extern void LibSixel_LSImageToSixel(LSImagePtr im, LSOutputContextPtr context);

#ifdef __cplusplus
}
#endif

/* LSImage manipulation API */

#ifdef __cplusplus
extern "C" {
#endif

extern LSImagePtr
LSImage_create(int sx, int sy, int depth, int ncolors);

extern void
LSImage_destroy(LSImagePtr im);

extern void
LSImage_setpalette(LSImagePtr im, int n, int r, int g, int b);

extern void
LSImage_setpixels(LSImagePtr im, uint8_t *pixels);

extern void
LSImage_setpixel(LSImagePtr im, int x, int y, int color);

extern void
LSImage_copy(LSImagePtr dst, LSImagePtr src, int w, int h);

extern void
LSImage_fill(LSImagePtr im, int color);

extern void
LSImage_fillrectangle(LSImagePtr im, int x1, int y1, int x2, int y2, int color);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_SIXEL_H */

/* emacs, -*- Mode: C; tab-width: 4; indent-tabs-mode: nil -*- */
/* vim: set expandtab ts=4 : */
/* EOF */
