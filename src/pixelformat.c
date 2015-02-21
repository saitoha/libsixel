/*
 * Copyright (c) 2014,2015 Hayaki Saito
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

#include "sixel.h"
#include <stdio.h>
#include <memory.h>


static void
get_rgb(unsigned char const *data,
        int const pixelformat,
        int depth,
        unsigned char *r,
        unsigned char *g,
        unsigned char *b)
{
    unsigned int pixels = 0;
    unsigned int low;
    unsigned int high;
    int count = 0;

    while (count < depth) {
        pixels = *(data + count) | (pixels << 8);
        count++;
    }

    /* TODO: we should swap bytes (only necessary on LSByte first hardware?) */
    if (depth == 2) {
        low    = pixels & 0xff;
        high   = (pixels >> 8) & 0xff;
        pixels = (low << 8) | high;
    }

    switch (pixelformat) {
    case PIXELFORMAT_RGB555:
        *r = ((pixels >> 10) & 0x1f) << 3;
        *g = ((pixels >>  5) & 0x1f) << 3;
        *b = ((pixels >>  0) & 0x1f) << 3;
        break;
    case PIXELFORMAT_RGB565:
        *r = ((pixels >> 11) & 0x1f) << 3;
        *g = ((pixels >>  5) & 0x3f) << 2;
        *b = ((pixels >>  0) & 0x1f) << 3;
        break;
    case PIXELFORMAT_RGB888:
        *r = (pixels >>  0) & 0xff;
        *g = (pixels >>  8) & 0xff;
        *b = (pixels >> 16) & 0xff;
        break;
    case PIXELFORMAT_BGR555:
        *r = ((pixels >>  0) & 0x1f) << 3;
        *g = ((pixels >>  5) & 0x1f) << 3;
        *b = ((pixels >> 10) & 0x1f) << 3;
        break;
    case PIXELFORMAT_BGR565:
        *r = ((pixels >>  0) & 0x1f) << 3;
        *g = ((pixels >>  5) & 0x3f) << 2;
        *b = ((pixels >> 11) & 0x1f) << 3;
        break;
    case PIXELFORMAT_BGR888:
        *r = (pixels >> 16) & 0xff;
        *g = (pixels >>  8) & 0xff;
        *b = (pixels >>  0) & 0xff;
        break;
    case PIXELFORMAT_RGBA8888:
        *r = (pixels >> 24) & 0xff;
        *g = (pixels >> 16) & 0xff;
        *b = (pixels >>  8) & 0xff;
        break;
    case PIXELFORMAT_ARGB8888:
        *r = (pixels >> 16) & 0xff;
        *g = (pixels >>  8) & 0xff;
        *b = (pixels >>  0) & 0xff;
        break;
    case PIXELFORMAT_GA88:
        *r = *g = *b = (pixels >> 8) & 0xff;
        break;
    case PIXELFORMAT_G8:
    case PIXELFORMAT_AG88:
        *r = *g = *b = pixels & 0xff;
        break;
    default:
        *r = *g = *b = 0;
        break;
    }
}


int
sixel_helper_compute_depth(int pixelformat)
{
    int depth = (-1);  /* unknown */

    switch (pixelformat) {
    case PIXELFORMAT_ARGB8888:
    case PIXELFORMAT_RGBA8888:
        depth = 4;
        break;
    case PIXELFORMAT_RGB888:
    case PIXELFORMAT_BGR888:
        depth = 3;
        break;
    case PIXELFORMAT_RGB555:
    case PIXELFORMAT_RGB565:
    case PIXELFORMAT_BGR555:
    case PIXELFORMAT_BGR565:
    case PIXELFORMAT_AG88:
    case PIXELFORMAT_GA88:
        depth = 2;
        break;
    case PIXELFORMAT_G8:
    case PIXELFORMAT_PAL1:
    case PIXELFORMAT_PAL2:
    case PIXELFORMAT_PAL4:
    case PIXELFORMAT_PAL8:
        depth = 1;
        break;
    default:
        break;
    }

    return depth;
}


static int
expand_rgb(unsigned char *dst,
           unsigned char const *src,
           int width, int height,
           int pixelformat, int depth)
{
    int x;
    int y;
    int dst_offset;
    int src_offset;
    unsigned char r, g, b;

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            src_offset = depth * (y * width + x);
            dst_offset = 3 * (y * width + x);
            get_rgb(src + src_offset, pixelformat, depth, &r, &g, &b);

            *(dst + dst_offset + 0) = r;
            *(dst + dst_offset + 1) = g;
            *(dst + dst_offset + 2) = b;
        }
    }

    return 0;
}


static int
expand_palette(unsigned char *dst, unsigned char const *src,
               int width, int height, int const pixelformat)
{
    int x;
    int y;
    int i;
    int bpp;  /* bit per plane */

    switch (pixelformat) {
    case PIXELFORMAT_PAL1:
        bpp = 1;
        break;
    case PIXELFORMAT_PAL2:
        bpp = 2;
        break;
    case PIXELFORMAT_PAL4:
        bpp = 4;
        break;
    case PIXELFORMAT_PAL8:
        for (i = 0; i < width * height; ++i, ++src) {
            *dst++ = *src;
        }
        return 0;
    default:
        return (-1);
    }

#if HAVE_DEBUG
    fprintf(stderr, "expanding PAL%d to PAL8...\n", bpp);
#endif

    for (y = 0; y < height; ++y) {
        for (x = 0; x < width * bpp / 8; ++x) {
            for (i = 0; i < 8 / bpp; ++i) {
                *dst++ = *src >> (8 / bpp - 1 - i) * bpp & ((1 << bpp) - 1);
            }
            src++;
        }
        x = width - x * 8 / bpp;
        if (x > 0) {
            for (i = 0; i < x; ++i) {
                *dst++ = *src >> (8 - (i + 1) * bpp) & ((1 << bpp) - 1);
            }
            src++;
        }
    }
    return 0;
}


int
sixel_helper_normalize_pixelformat(
    unsigned char       /* out */ *dst,             /* destination buffer */
    int                 /* out */ *dst_pixelformat, /* converted pixelformat */
    unsigned char const /* in */  *src,             /* source pixels */
    int                 /* in */  src_pixelformat,  /* format of source image */
    int                 /* in */  width,            /* width of source image */
    int                 /* in */  height)           /* height of source image */
{
    switch (src_pixelformat) {
    case PIXELFORMAT_G8:
        (void) expand_rgb(dst, src, width, height, src_pixelformat, 1);
        *dst_pixelformat = PIXELFORMAT_RGB888;
        break;
    case PIXELFORMAT_RGB565:
    case PIXELFORMAT_RGB555:
    case PIXELFORMAT_BGR565:
    case PIXELFORMAT_BGR555:
    case PIXELFORMAT_GA88:
    case PIXELFORMAT_AG88:
        (void) expand_rgb(dst, src, width, height, src_pixelformat, 2);
        *dst_pixelformat = PIXELFORMAT_RGB888;
        break;
    case PIXELFORMAT_RGB888:
    case PIXELFORMAT_BGR888:
        (void) expand_rgb(dst, src, width, height, src_pixelformat, 3);
        *dst_pixelformat = PIXELFORMAT_RGB888;
        break;
    case PIXELFORMAT_RGBA8888:
    case PIXELFORMAT_ARGB8888:
        (void) expand_rgb(dst, src, width, height, src_pixelformat, 4);
        *dst_pixelformat = PIXELFORMAT_RGB888;
        break;
    case PIXELFORMAT_PAL1:
    case PIXELFORMAT_PAL2:
    case PIXELFORMAT_PAL4:
        return expand_palette(dst, src, width, height, src_pixelformat);
    case PIXELFORMAT_PAL8:
        memcpy(dst, src, width * height);
        break;
    default:
        return (-1);
    }

    return 0;
}

/* emacs, -*- Mode: C; tab-width: 4; indent-tabs-mode: nil -*- */
/* vim: set expandtab ts=4 : */
/* EOF */
